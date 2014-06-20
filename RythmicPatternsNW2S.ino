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
AnalogOut* out1 = AnalogOut::create(DUE_SPI_4822_00);
AnalogOut* out2 = AnalogOut::create(DUE_SPI_4822_01);

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

void loop() 
{



// Regarding the analog outputs:

// Arduino's analogWrite doesn't work on the CV outputs as they are a 
// little more complicated. The DACs are not native devices like the PWM pins, but are peripherals on the SPI bus.

// So it takes a couple of commands to set an output up before you can write values to it:

// create a static declaration at the top of the page
// AnalogOut out1;

// set up the output in setup()
// out1 = AnalogOut::create(DUE_SPI_4822_01);

// in loop() do your writing of values. outputCV uses millivolts
// out1.outputCV(1000);
// out1.outputCV(5000);
// out1.outputCV(-1000);

  out1->outputCV(1000);
  out2->outputCV(2000);


	if (EventManager::getT() >= nexttime) 
  	{
		/* Crude tempo based on an analog input */
		/* 333ms = 60BPM (slowest) to 100ms = 200BPM (fastest) */
    	nexttime += 100 + ((5000 - analogReadmV(INPUT_TEMPO, 0, 5000)) / 20);		
						
		/* Program Selection. Here it's once per beat */					
     	int drumProgram = (noOfDrumPrograms > 1) ? (analogReadmV(INPUT_PROGRAM, 0, 4900) / (5000 / noOfDrumPrograms)) : 0; 


		/* Instead of looping, just keep the current column as a state */
		column = (column + 1) % 16;

		/* temporal. start with first beat point... */
  	  	for (int row = 0; row < noOfDrumSteps; row++) 
  	  	{ 
			/* vertical, outputs. start with output 0... */

    		/* will the program run for this column? */
    		randValueSubtract = Entropy::getValue(0, 5000);

    		if (randValueSubtract > analogReadmV(INPUT_SUBTRACT, 0, 5000)) 
			{            
				/* the hit */
				if (drums[drumProgram][row][column] == 1) 
				{
					digitalWrite(outputs[row], HIGH);
				}

      		  	/* the 1 or 0 from the pattern is added to isThisATrigger */
				/* give isThisATrigger a 1 or 0 depending on hit or not */
      		  	isThisATrigger[row] = drums[drumProgram][row][column]; 

      		  	/* a 1 is added to isThisATrigger anyway... maybe */
      		  	randValueAdd = Entropy::getValue(0, 5000);

      		  	if (randValueAdd < analogReadmV(INPUT_ADD, 0, 5000))
	  		  	{
					digitalWrite(outputs[row], HIGH);
					isThisATrigger[row] = 1;
      		  	}
      
				/* check if gate should be turned off or kept open */
      		  	if (isThisATrigger[row] == 1) 
	  		  	{ 
		  			/* if this is a hit */
          		  	if (anslagEveryOther[row] == 1)
					{ 
						/* if the indicator variable shows 1 */
						gates[row]->reset();
          			  	
						/* indicator value set to 0 to indicate that the last hit turned gate off... 
						   so next one should keep it on and not go through this loop */							
						anslagEveryOther[row] = 0; 
        			}
        			else 
					{
						/* so that next time there will be a turning off */
          				anslagEveryOther[row] = 1;  
        			}
      			}         
		  	}
  		}
	}

	EventManager::loop();
}
