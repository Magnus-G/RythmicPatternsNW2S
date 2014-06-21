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
AnalogOut* out1;
AnalogOut* out2;
AnalogOut* out3;
AnalogOut* out4;

// ^
// this line renders this error:

// Arduino: 1.5.6-r2 (Mac OS X), Board: "Arduino Due (Programming Port)"

// RythmicPatternsNW2S:18: error: no matching function for call to 'nw2s::AnalogOut::AnalogOut()'
// /Users/Datorn/Documents/Arduino/b-master/sketches/libraries/nw2s/IO.h:4253: note: candidates are: nw2s::AnalogOut::AnalogOut(nw2s::PinAnalogOut)
// /Users/Datorn/Documents/Arduino/b-master/sketches/libraries/nw2s/IO.h:4244: note:                 nw2s::AnalogOut::AnalogOut(const nw2s::AnalogOut&)
// RythmicPatternsNW2S.ino: In function 'void setup()':
// RythmicPatternsNW2S:87: error: no match for 'operator=' in 'out1 = nw2s::AnalogOut::create((nw2s::PinAnalogOut)1015)'
// /Users/Datorn/Documents/Arduino/b-master/sketches/libraries/nw2s/IO.h:4244: note: candidates are: nw2s::AnalogOut& nw2s::AnalogOut::operator=(const nw2s::AnalogOut&)

//   This report would have more information with
//   "Show verbose output during compilation"
//   enabled in File > Preferences.


using namespace nw2s;


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

/* Lets define our inputs up here since we use them in various places below */
const PinAnalogIn INPUT_TEMPO = DUE_IN_A00;
const PinAnalogIn INPUT_PROGRAM = DUE_IN_A01;
const PinAnalogIn INPUT_SUBTRACT = DUE_IN_A02;
const PinAnalogIn INPUT_ADD = DUE_IN_A03;
const PinAnalogIn INPUT_NOTE = DUE_IN_A04;

/* Ardcore sketch used integers, so we'll use an array instead */
PinDigitalOut outputs[noOfDrumOutputs] = 
{ 
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

void setup() 
{
	Serial.begin(19200);
  out1 = AnalogOut::create(DUE_SPI_4822_00);
  out2 = AnalogOut::create(DUE_SPI_4822_01);
  out3 = AnalogOut::create(DUE_SPI_4822_02);
  out4 = AnalogOut::create(DUE_SPI_4822_03);
	
	EventManager::initialize();
	
	/* Intialize our six gates */
	for (int i = 0; i < noOfDrumOutputs; i++)
	{
		/* Set up a gate for each output with a duration of 20ms */
		gates[i] = Gate::create(outputs[i], 10);
		EventManager::registerDevice(gates[i]);
	}
}

////////////////////////////////////////////////////////

void loop() {

	if (EventManager::getT() >= nexttime) {
    nexttime += 100 + ((5000 - analogReadmV(INPUT_TEMPO, 0, 5000)) / 20);		
						
    int drumProgram = (noOfDrumPrograms > 1) ? (analogReadmV(INPUT_PROGRAM, 0, 4900) / (5000 / noOfDrumPrograms)) : 0; 
    int noteOut = analogReadmV(INPUT_NOTE);

    out1->outputCV(100 * noteOut);
    out2->outputCV(200 * noteOut);
    out3->outputCV(300 * noteOut);
    out4->outputCV(400 * noteOut);

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
