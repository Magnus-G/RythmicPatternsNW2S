
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
const PinAnalogIn INPUT_PROGRAM = DUE_IN_A02;
const PinAnalogIn INPUT_SUBTRACT = DUE_IN_A04;
const PinAnalogIn INPUT_ADD = DUE_IN_A06;
 
const PinAnalogIn INPUT_CVSeq_1 = DUE_IN_A01;
const PinAnalogIn INPUT_CVSeq_2 = DUE_IN_A03;
const PinAnalogIn INPUT_CVSeq_3 = DUE_IN_A05;

const PinAnalogIn INPUT_BASENOTE = DUE_IN_A07;

const PinAnalogIn INPUT_SETTING = DUE_IN_A11;

////////////////////////////////////////////////////////////////////////

/* Keep a static reference to the sequencer so we can modify it outside of setup() */
NoteSequencer* sequencer1;
NoteSequencer* sequencer2;
NoteSequencer* sequencer3;

CVNoteSequencer* CVsequencer1;
CVNoteSequencer* CVsequencer2;
CVNoteSequencer* CVsequencer3;

// int const noOfBaseNotes = 3;
// int baseNotePlaceInArray = 0;
// NoteName baseNotes[noOfBaseNotes] = {C, D, E};
// NoteName baseNotes[noOfBaseNotes] = {C, D, E_FLAT, F, G, A_FLAT, B_FLAT};

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
  SequenceNote notelist1[8] = { {1,1}, {1,2}, {1,3}, {1,4}, {1,5}, {1,6}, {1,7}, {1,8} };
  SequenceNote notelist2[8] = { {1,5}, {1,5}, {1,6}, {1,7}, {1,8}, {1,9}, {1,10}, {1,11} };
  SequenceNote notelist3[8] = { {1,8}, {1,9}, {1,10}, {1,11}, {2,1}, {2,2}, {2,3}, {2,4} };
                
  /* Add the raw array to a vector for easier transport */
  NoteSequenceData* notes1 = new NoteSequenceData(notelist1, notelist1 + 8);
  NoteSequenceData* notes2 = new NoteSequenceData(notelist2, notelist2 + 8);
  NoteSequenceData* notes3 = new NoteSequenceData(notelist3, notelist3 + 8);

  /* Build our note-based seuqnce */
  sequencer1 = TriggerNoteSequencer::create(notes1, C, Key::SCALE_CHROMATIC, DUE_IN_D5, DUE_SPI_4822_15);
  sequencer2 = TriggerNoteSequencer::create(notes2, C, Key::SCALE_CHROMATIC, DUE_IN_D6, DUE_SPI_4822_14);
  sequencer3 = TriggerNoteSequencer::create(notes3, C, Key::SCALE_CHROMATIC, DUE_IN_D7, DUE_SPI_4822_13, false);

  sequencer1->setgate(Gate::create(DUE_OUT_D00, 75));

  EventManager::registerDevice(sequencer1);
  EventManager::registerDevice(sequencer2);
  EventManager::registerDevice(sequencer3);

  ////////////////////////////////////////////////////////////////////////

  // Clock* democlock = VariableClock::create(25, 525, DUE_IN_A11, 16);

  SequenceNote notelist4[8] = { {1,1}, {1,2}, {1,3},  {1,4},  {1,5}, {1,6}, {1,7},  {1,8} };
  SequenceNote notelist5[8] = { {1,4}, {1,5}, {1,6},  {1,7},  {1,8}, {1,9}, {1,10}, {1,11} };
  SequenceNote notelist6[8] = { {1,8}, {1,9}, {1,10}, {1,11}, {2,1}, {2,2}, {2,3},  {2,4} };

  NoteSequenceData* notes4 = new NoteSequenceData(notelist4, notelist4 + 8);
  NoteSequenceData* notes5 = new NoteSequenceData(notelist5, notelist5 + 8);
  NoteSequenceData* notes6 = new NoteSequenceData(notelist6, notelist6 + 8);

  // CVNoteSequencer(                    notes, key, scale,           output,          input,    randomize);
  CVsequencer1 = CVNoteSequencer::create(notes4, C, Key::SCALE_CHROMATIC, DUE_SPI_4822_00, INPUT_CVSeq_1);
  CVsequencer2 = CVNoteSequencer::create(notes5, C, Key::SCALE_CHROMATIC, DUE_SPI_4822_01, INPUT_CVSeq_2);
  CVsequencer3 = CVNoteSequencer::create(notes6, C, Key::SCALE_CHROMATIC, DUE_SPI_4822_02, INPUT_CVSeq_3);

  // CVsequencer1->setgate(Gate::create(DUE_OUT_D15, 75));
  // CVsequencer2->setgate(Gate::create(DUE_OUT_D15, 75));
  // CVsequencer3->setgate(Gate::create(DUE_OUT_D15, 75));
  
  // democlock->registerDevice(CVsequencer1);
  
  // EventManager::registerDevice(democlock);  

  EventManager::registerDevice(CVsequencer1);
  EventManager::registerDevice(CVsequencer2);
  EventManager::registerDevice(CVsequencer3);

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////


} // setup

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

void loop() {

  int const noOfBaseNotes = 4;
  int baseNote = analogReadmV(INPUT_BASENOTE, 0, 5000);
  // Serial.println(baseNote);

  if (baseNote > 0 && baseNote < 399) {
    CVsequencer1->setKey(C);
    CVsequencer2->setKey(C);
    CVsequencer3->setKey(C);
    Serial.println("C");
  }
  if (baseNote > 400 && baseNote < 799) {
    CVsequencer1->setKey(C_SHARP);
    CVsequencer2->setKey(C_SHARP);
    CVsequencer3->setKey(C_SHARP);
    Serial.println("C_SHARP");
  }
  // if (baseNote > 2 && baseNote < 2) {
  //   CVsequencer1->setKey(D);
  //   CVsequencer2->setKey(D);
  //   CVsequencer3->setKey(D);
  // }
  // if (baseNote > 3 && baseNote < 3) {
  //   CVsequencer1->setKey(D_SHARP);
  //   CVsequencer2->setKey(D_SHARP);
  //   CVsequencer3->setKey(D_SHARP);
  // }
  // if (baseNote > 0 && baseNote < 0) {
  //   CVsequencer1->setKey(E);
  //   CVsequencer2->setKey(E);
  //   CVsequencer3->setKey(E);
  // }
  // if (baseNote > 1 && baseNote < 1) {
  //   CVsequencer1->setKey(F);
  //   CVsequencer2->setKey(F);
  //   CVsequencer3->setKey(F);
  // }
  // if (baseNote > 2 && baseNote < 2) {
  //   CVsequencer1->setKey(F_SHARP);
  //   CVsequencer2->setKey(F_SHARP);
  //   CVsequencer3->setKey(F_SHARP);
  // }
  // if (baseNote > 3 && baseNote < 3) {
  //   CVsequencer1->setKey(G);
  //   CVsequencer2->setKey(G);
  //   CVsequencer3->setKey(G);
  // }
  // if (baseNote > 0 && baseNote < 0) {
  //   CVsequencer1->setKey(G_SHARP);
  //   CVsequencer2->setKey(G_SHARP);
  //   CVsequencer3->setKey(G_SHARP);
  // }
  // if (baseNote > 1 && baseNote < 1) {
  //   CVsequencer1->setKey(A);
  //   CVsequencer2->setKey(A);
  //   CVsequencer3->setKey(A);
  // }
  // if (baseNote > 2 && baseNote < 2) {
  //   CVsequencer1->setKey(A_SHARP);
  //   CVsequencer2->setKey(A_SHARP);
  //   CVsequencer3->setKey(A_SHARP);
  // }
  // if (baseNote > 3 && baseNote < 3) {
  //   CVsequencer1->setKey(B);
  //   CVsequencer2->setKey(B);
  //   CVsequencer3->setKey(B);
  // }

  // if (EventManager::getT() % 10 == 0) {

    // This in setup: 
    // int const noOfBaseNotes = 3;
    // NoteName baseNotes[noOfBaseNotes] = {C, D, E};

    // int baseNote = analogReadmV(INPUT_BASENOTE);

    // NoteName baseNote = baseNotes[baseNotePlaceInArray];

    // Serial.println(baseNote);

  // }
  
////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
