#include <Adafruit_NeoPixel.h>

// Macros
#define SIZE_OF_INT_ARRAY(arrayName) (sizeof(arrayName) / sizeof(int))

// Debugging
#define DEBUG_VERBOSE 0
#define INFO_VERBOSE  0
#define INFO_LESS     1

// Constants
#define SCAN_DELAY        10  //Speed at which to "scan" the columns of each row
#define ROW_SELECT_DELAY  0    //Set to a higher number to witness the rows being selected and read (the LEDS)
#define NEOPIX_PIN        6
#define USE_SHIFT_REG     0

#if USE_SHIFT_REG
  #define SIZE_SR           8   //Number of bit in the shift register(chain)
  #define CHIP_SETTLE_DELAY 1
  
  // Trigger Variables (shift register)
  int serialOut     = 13; //SER or DS (Serial Input/Data Serial)
  int outputEnable  = 12; //OE (Output Enable)
  int storageClock  = 11; //RCLK or STCP (Buffer to registers)
  int serialClock   = 10; //SRCLK or SHCP (Serial Register Clock/Serial Data Clock)
  int serialClear   = 9;  //SRCLR or MR(Shift Register Clear/Memory Reset)
#endif

// Trigger Variables
int rowPins[]		  = {
  2, // Row 1
  3, // Row 2
  4  // Row 3
};

int numRows 		  = SIZE_OF_INT_ARRAY(rowPins); // Rows are A (yellow wires) and B (green wires)


// Sense Variables (Arduino or Parallel In/Serial Out like 74HC165N)
int columnPins[] 	= {
  8,  // Column A
  9,  // Column B
  10, // Column C
  11  // Column D
};

int numCols 		  = SIZE_OF_INT_ARRAY(columnPins);

int totalSwitches = numRows * numCols;

char mapper[12][3]  =
  {
    "A1", "B1", "C1", "D1",
    "A2", "B2", "C2", "D2",
    "A3", "B3", "C3", "D3"
  };

// God I am lazy...
int neoPixMapper[] =
  {
    0,1,2,3,
    7,6,5,4,
    8,9,10,11
  };

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(totalSwitches, NEOPIX_PIN, NEO_GRB + NEO_KHZ800);

 
void setup() {
  Serial.begin(9600);
  
#if USE_SHIFT_REG
  pinMode(serialOut, OUTPUT);
  digitalWrite(serialOut, LOW);
  delay(CHIP_SETTLE_DELAY);
  
  pinMode(outputEnable, OUTPUT);
  shiftRegister_outputEnable(1); // Always enable Output
  
  pinMode(serialClock, OUTPUT);
  digitalWrite(serialClock, LOW);
  
  pinMode(serialClear, OUTPUT);
  digitalWrite(serialClear, HIGH); 	// Default state
  shiftRegister_clear();			// Register reset
#endif
  
  // Set all the outputs as such
  for(int i = 0; i < numRows; ++i) {
  	pinMode(rowPins[i], OUTPUT);
    digitalWrite(rowPins[i], LOW);
    delay(CHIP_SETTLE_DELAY);
  }
  
  // Set all the inputs as such
  for(int j = 0; j < numCols; ++j) {
  	pinMode(columnPins[j], INPUT);
  }

  pixels.begin();
}


void loop() {
  int resultsInBinary = 0;
  
  // Scanning rows
  for(int i = 0; i < numRows; ++i) {
#if USE_SHIFT_REG
    shiftRegister_write(i);
    delay(ROW_SELECT_DELAY);
#endif
    
    digitalWrite(rowPins[i], HIGH);
    delay(CHIP_SETTLE_DELAY);
    
    resultsInBinary |= buttons_read() << i * numCols;
    
    digitalWrite(rowPins[i], LOW);
    delay(CHIP_SETTLE_DELAY);
  }

  if(DEBUG_VERBOSE) {
    Serial.println(resultsInBinary, 2);
  }

  buttons_print(resultsInBinary);
  
  delay(SCAN_DELAY);
}


void buttons_print(int buttonData) {
  // Display buttion status to Serial
  for(int j = 0; j < (numRows * numCols) - 1; ++j) {
    int isSwitched = (buttonData >> j) & 1;
    
    if(isSwitched) {
      pixels.setPixelColor(neoPixMapper[j], pixels.Color(0, 150, 0));
    }
    else {
      pixels.setPixelColor(neoPixMapper[j], pixels.Color(0, 0, 0));
    }

    if(DEBUG_VERBOSE) {
      Serial.println(buttonData, 2);
    }

    if(INFO_VERBOSE || INFO_LESS) {
      // Allocate memory for output string/buffer
      char * textOutput = (char*)malloc(32);
      
      if(INFO_VERBOSE) {
        sprintf(textOutput, "Button %s %s pressed.", mapper[j], isSwitched ? "is" : "is not");
      }
  
      if(INFO_LESS && isSwitched) {
        sprintf(textOutput, "Button %s is pressed.", mapper[j]);
      }

      Serial.println(textOutput);

      // Free memory for buffer
      free(textOutput);
    }
    
    
  }

  pixels.show();
}


int buttons_read() {
  int returnValue = 0;
  
  for(int i = 0; i < numCols; ++i) {
    delay(CHIP_SETTLE_DELAY);
    
    int result = digitalRead(columnPins[i]);

    if(DEBUG_VERBOSE) {
      Serial.print(result);
    }
      
    if(HIGH == result) {
      returnValue |= (1 << i);
    }
  }

  if(DEBUG_VERBOSE) {
    Serial.println();
  }
    
  return returnValue;
}

#if USE_SHIFT_REG
void shiftRegister_write(int lineToOpen) {
  shiftRegister_clear();
  
  //Double check that serial is 0
  digitalWrite(serialOut, LOW);
  delay(CHIP_SETTLE_DELAY);
  
  for(int i = 0; i < SIZE_SR; ++i) {
    if((SIZE_SR - 1) - i == lineToOpen) {
      digitalWrite(serialOut, HIGH);
  	  delay(CHIP_SETTLE_DELAY);
  	  shiftRegister_registerTick();
      digitalWrite(serialOut, LOW);
      delay(CHIP_SETTLE_DELAY);
    }
    else {
  	  shiftRegister_registerTick();
    }
  }
  
  shiftRegister_storageTick();
}


void shiftRegister_registerTick() {
  digitalWrite(serialClock, HIGH); // Clock tick
  delay(CHIP_SETTLE_DELAY);
  digitalWrite(serialClock, LOW);
  delay(CHIP_SETTLE_DELAY);
}


void shiftRegister_storageTick() {
  digitalWrite(storageClock, HIGH); // Clock tick
  delay(CHIP_SETTLE_DELAY);
  digitalWrite(storageClock, LOW);
  delay(CHIP_SETTLE_DELAY);
}


void shiftRegister_outputEnable(int enable) {
  digitalWrite(outputEnable, 1 == enable ? LOW : HIGH); //Yes, these are an alias for 1 and 0, PORTABILITY!
  delay(CHIP_SETTLE_DELAY);
}


void shiftRegister_clear() {
  //Make sure to clear the rgisters
  digitalWrite(serialClear, LOW);
  delay(CHIP_SETTLE_DELAY);
  digitalWrite(serialClear, HIGH);
  delay(CHIP_SETTLE_DELAY);
}
#endif
