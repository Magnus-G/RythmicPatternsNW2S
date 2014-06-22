
#include <EventManager.h>
#include <Entropy.h>
#include <Key.h>
#include <Trigger.h>
#include <Clock.h>
#include <Oscillator.h>
#include <Sequence.h>
#include <IO.h>
#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <ShiftRegister.h>
#include <aJSON.h>
#include <SDFirmware.h>


// create a static declaration at the top of the page
// AnalogOut* out1;

using namespace nw2s;

////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////

// drums [z][y][x]
int const noOfDrumPrograms = 2;
int const noOfDrumSteps = 6;
int const noOfDrumOutputs = 6;
int drums[noOfDrumPrograms][noOfDrumOutputs][16] = {

  {
    // 1  2  3  4  5  6  7  8   1  2  3  4  5  6  7  8
    {1, 0, 0, 0, 1, 0, 0, 0,  1, 0, 0, 0, 1, 0, 0, 0},
    {0, 0, 1, 0, 0, 0, 1, 0,  0, 0, 1, 0, 0, 0, 1, 0},
    {0, 1, 0, 1, 0, 1, 0, 0,  0, 1, 0, 1, 0, 1, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 1, 0, 1, 0},
    {1, 0, 0, 1, 0, 0, 1, 0,  0, 1, 0, 0, 1, 0, 0, 1},
  },
  
  {
    // 1  2  3  4  5  6  7  8   1  2  3  4  5  6  7  8
    {1, 1, 0, 0, 1, 1, 0, 0,  1, 1, 0, 0, 1, 0, 0, 0},
    {0, 0, 1, 0, 0, 0, 1, 0,  0, 0, 1, 0, 0, 0, 1, 0},
    {0, 1, 0, 1, 0, 1, 0, 0,  0, 1, 0, 1, 0, 1, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 1, 0, 1, 0},
    {1, 0, 0, 1, 0, 0, 1, 0,  0, 1, 0, 0, 1, 0, 0, 1},
  }
};

/* Ardcore sketch used integers, so we'll use an array instead */
PinDigitalOut outputs[noOfDrumOutputs] = { 
  DUE_OUT_D00, 
  DUE_OUT_D01, 
  DUE_OUT_D02, 
  DUE_OUT_D03, 
  DUE_OUT_D04, 
  DUE_OUT_D05 
};

/* Use an array of 'gates' cause the off-on-off is too fast to see */
/* Gates are devices that will turn off after a duration and are managed by the EventManager */
/* We'll set up a really short gate to make it a trigger */
Gate* gates[noOfDrumOutputs];

/* randomization/probability based on 0-5V measurements in mV */
int randValueSubtract = 5000;
int randValueAdd = 5000;

int isThisATrigger[noOfDrumSteps] = { 1, 1, 1, 1, 1, 1 };
int anslagEveryOther[noOfDrumSteps] = { 1, 1, 1, 1, 1, 1 };

/* Manage state for the transitions */
int nexttime = 0; 
int column = 0;

////////////////////////////////////////////////////////////////////////

/* Lets define our inputs up here since we use them in various places below */
const PinAnalogIn INPUT_TEMPO = DUE_IN_A00;
const PinAnalogIn INPUT_PROGRAM = DUE_IN_A01;
const PinAnalogIn INPUT_SUBTRACT = DUE_IN_A02;
const PinAnalogIn INPUT_ADD = DUE_IN_A03;

const PinAnalogIn INPUT_SETTING = DUE_IN_A05;

////////////////////////////////////////////////////////////////////////

/* Keep a static reference to the sequencer so we can modify it outside of setup() */
NoteSequencer* sequencer1;
NoteSequencer* sequencer2;
NoteSequencer* sequencer3;

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////


void setup() 
{
  Serial.begin(19200);
  EventManager::initialize();
  
  /* Intialize our six gates */
  for (int i = 0; i < noOfDrumOutputs; i++)
  {
    /* Set up a gate for each output with a duration of 20ms */
    gates[i] = Gate::create(outputs[i], 10);
    EventManager::registerDevice(gates[i]);
  }

////////////////////////////////////////////////////////////////////////

  /* Set up the note data for the sequence */
  SequenceNote notelist1[8] = { {1,1}, {1,4}, {1,1}, {1,5}, {1,1}, {1,4}, {1,1}, {1,5} };
  SequenceNote notelist2[8] = { {1,1}, {1,4}, {1,1}, {1,5}, {1,1}, {1,4}, {1,1}, {1,5} };
  SequenceNote notelist3[8] = { {1,1}, {1,4}, {1,1}, {1,5}, {1,1}, {1,4}, {1,1}, {1,5} };
                
  /* Add the raw array to a vector for easier transport */
  NoteSequenceData* notes1 = new NoteSequenceData(notelist1, notelist1 + 8);
  NoteSequenceData* notes2 = new NoteSequenceData(notelist2, notelist2 + 8);
  NoteSequenceData* notes3 = new NoteSequenceData(notelist3, notelist3 + 8);

  /* Build our note-based seuqnce */
  sequencer1 = TriggerNoteSequencer::create(notes1, C, Key::SCALE_MAJOR, DUE_IN_D0, DUE_SPI_4822_15);
  sequencer2 = TriggerNoteSequencer::create(notes2, C, Key::SCALE_MAJOR, DUE_IN_D0, DUE_SPI_4822_14);
  sequencer3 = TriggerNoteSequencer::create(notes3, C, Key::SCALE_MAJOR, DUE_IN_D0, DUE_SPI_4822_13, false);

  sequencer1->setgate(Gate::create(DUE_OUT_D00, 75));

  EventManager::registerDevice(sequencer1);
  EventManager::registerDevice(sequencer2);
  EventManager::registerDevice(sequencer3);

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////


}

////////////////////////////////////////////////////////////////////////

void loop() {

  if (EventManager::getT() % 10 == 0) {
    /* AnalogReads are relatively expensive and slow, so only do them every 10ms */
    int setting = analogReadmV(INPUT_SETTING);
    
    if (setting > 2000) {
      sequencer1->setKey(E);
      sequencer2->setKey(E);
      sequencer3->setKey(E);
    }
    else {
      sequencer1->setKey(C);
      sequencer2->setKey(C);
      sequencer3->setKey(C);
    }
  }
  
  if (EventManager::getT() >= nexttime) {
    nexttime += 100 + ((5000 - analogReadmV(INPUT_TEMPO, 0, 5000)) / 20);   
            
    int drumProgram = (noOfDrumPrograms > 1) ? (analogReadmV(INPUT_PROGRAM, 0, 4900) / (5000 / noOfDrumPrograms)) : 0;

    column = (column + 1) % 16;

    for (int row = 0; row < noOfDrumSteps; row++) { 

      randValueSubtract = Entropy::getValue(0, 5000);
      if (randValueSubtract > analogReadmV(INPUT_SUBTRACT, 0, 5000)) {

        if (drums[drumProgram][row][column] == 1) {
          digitalWrite(outputs[row], HIGH);
        }

        isThisATrigger[row] = drums[drumProgram][row][column]; 

        randValueAdd = Entropy::getValue(0, 5000);

        if (randValueAdd < analogReadmV(INPUT_ADD, 0, 5000)) {
          digitalWrite(outputs[row], HIGH);
          isThisATrigger[row] = 1;
        }
      
        if (isThisATrigger[row] == 1) { 
          if (anslagEveryOther[row] == 1) { 
            gates[row]->reset();
            anslagEveryOther[row] = 0; 
          }
          else {
            anslagEveryOther[row] = 1;  
          }
        } // gate check
      } // subtract
    } // row
  } // column
  EventManager::loop();
}
