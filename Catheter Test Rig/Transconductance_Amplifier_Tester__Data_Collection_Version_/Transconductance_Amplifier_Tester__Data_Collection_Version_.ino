
/*
This code is designed to run on an Arduino Nano Every with the transconductance card tester
*/ 
#include <SPI.h>
#define L_DAC A0

//Pin Definitions
const int DAC_CS = 9;
const int H_NEG = 2;
const int H_POS = 3;
const int H_EN = 4;
const int ADC_CS = 8;
const int button1 = 5;
const int button2 = 6;
const int button3 = 7;

//Dac Max Value
const int dacMax14 = 16383; //14 bit maximum value
const int dacMax12 = 4095; //12 bit maximum value
int dacMax = dacMax14;

//Variables
int dacOut = 0;
uint16_t sense;
float tx;
int sceneCounter1 = 0;
int sceneCounter2 = 0;
int sceneCounter3 = 0;
int currentScene = 1;

//Boolean Checks
bool dac14 = true; //used to determine which DAC is present (true = 14 bit, false = 12 bit)
bool sc1Change = false;
bool sc2Change = false;
bool sc3Change = false;
bool firstTime = true;

//Need to configure ADC such that it is slower than the DAC.
//It cannot read very fast.
SPISettings DAC_SETTINGS = SPISettings(1000000, MSBFIRST, SPI_MODE0);
SPISettings ADC_SETTINGS = SPISettings(125000, MSBFIRST, SPI_MODE0);

//Setup----------------------------------------------------------------------------------------------------------------------------------------
void setup() {
  
  //Set up SPI
  Serial.begin(4800);
  SPI.begin(); 

  //Configure DAC_CS pin
  pinMode (DAC_CS, OUTPUT);
  
  //Configure ADC_CS pin
  pinMode (ADC_CS, OUTPUT);
  
  //Configure H Bridge
  pinMode (H_NEG, OUTPUT);
  pinMode (H_POS, OUTPUT);
  pinMode (H_EN, OUTPUT);
  
  //Car no go if this pin isn't held low
  pinMode (L_DAC, OUTPUT);
  
  digitalWrite(H_POS, LOW); //Pull both ends low to start
  digitalWrite(H_NEG, LOW);
  digitalWrite(H_EN, HIGH); //Ensure H-Bridge is always active
  digitalWrite(L_DAC, LOW);

  //Create interrupts for scene selection
  attachInterrupt(digitalPinToInterrupt(button1), incSC1, FALLING);
  attachInterrupt(digitalPinToInterrupt(button2), incSC2, FALLING);
  attachInterrupt(digitalPinToInterrupt(button3), dacSelect, FALLING);
}

//Begin Loop ---------------------------------------------------------------------------------------------------------------------------------
void loop() {
  /*
    setDirection(true);
    //rising
    setDac(dacMax);
    delay(5000);
    //falling
    setDac(0);
    delay(5000);

  //ugly if else that works
    //Triangle functions for Scene 1
    if(sceneCounter1 == 0 && sc1Change == true) {
      fastTriangle();
    }
    if(sceneCounter1 == 1 && sc1Change == true) {
      posTriangle();
    }
    if(sceneCounter1 == 2 && sc1Change == true) {
      negTriangle();
    }
    if(sceneCounter1 == 3 && sc1Change == true) {
      biTriangle();
    }

    //PWM functions for Scene 2
    if(sceneCounter2 == 0 && sc2Change == true) { //patch for output staying maxed out when SC2 = 0
      setDac(0);
    }
    if(sceneCounter2 == 1 && sc2Change == true) {
      
      PWM50();
    }
    if(sceneCounter2 == 2 && sc2Change == true) {
      PWM25();
    }
    if(sceneCounter2 == 3 && sc2Change == true) {
      PWM10();
    }
    */

  //Gonna try switch cases here
  //First switch is used to determine the scene which is currently active
  //This is to differentiate between sets of functions
  switch (currentScene) {
    
    //First case is for Scene 1, featuring triangle outputs
    //This is helpful for checking nonlinearity behavior
    case 1:
      switch (sceneCounter1) {
        case 0:
          fastTriangle();
          break;
        case 1:
          posTriangle();
          break;
        case 2:
          negTriangle();
          break;
        case 3:
          biTriangle();
          break;
        default:
          setDac(0);
          break;
      }
    
    //Second case is for Scene 2, featuring various PWM functions
    //These are helpful for determining rise/fall times as well as time delay between DAC and amp output
    case 2:
      switch (sceneCounter2) {
        case 0:
          setDac(0);
          break;
        case 1:
          PWM50();
          break;
        case 2:
          PWM25();
          break;
        case 3:
          PWM10();
          break;
        default:
          setDac(0);
          break;
      }
  }
  
  //firstTime is used to check if the chosen function is running for the first time since a button has been pressed 
  //Must be reset every main loop so it is only True after interrupt
  firstTime = false; 
}
//End Loop ----------------------------------------------------------------------------------------------------------------------------------

//Interrupts---------------------------------------------------------------------------------------------------------------------------------
void incSC1() {
  sceneCounter1++;
  currentScene = 1;
  //sc1Change = true;
  //sc2Change = false;
  //sc3Change = false;
  
  firstTime = true;
  if(sceneCounter1 > 3) {
    sceneCounter1 = 0;
  }
}

void incSC2() {
  sceneCounter2++;
  currentScene = 2;
  //sc1Change = false;
  //sc2Change = true;
  //sc3Change = false;
  
  firstTime = true;
  if(sceneCounter2 > 3) {
    sceneCounter2 = 0;
  }
}

void dacSelect() {
  dac14 = !dac14;
  if (dac14 == true) {
    dacMax = dacMax14;
  }
  else {
    dacMax = dacMax12;
  }
}

//DAC write function------------------------------------------------------------------------------------------------------------------------
void setDac(int value) {
  //Convert integer to unsigned 16 bit value (Maximum of 14 bits actually used)
  uint16_t dacPrimaryByte = value;
  
  //if using the 12 bit DAC you must alter the primary byte a bit
  if(dac14 == false) {
    dacPrimaryByte &= 0x0FFF; //limit scope of data to 12 bits
    dacPrimaryByte |= 0x3000; //apply necessary setup bits for the DAC
  }
  
  //Send data to the DAC
  SPI.beginTransaction(DAC_SETTINGS);
  noInterrupts();
  digitalWrite(DAC_CS, LOW);
  SPI.transfer16(dacPrimaryByte);
  digitalWrite(DAC_CS, HIGH);
  interrupts();
  SPI.endTransaction();
}

//ADC read function-------------------------------------------------------------------------------------------------------------------------
//Need to update this to work with 16 bit ADC
float ADC_read() {
  SPI.beginTransaction(ADC_SETTINGS);
  digitalWrite(ADC_CS, LOW);
  uint16_t ret1(SPI.transfer16(0x0000));
  digitalWrite(ADC_CS, HIGH);
  SPI.endTransaction();
  sense = ret1 & 0b0001111111111111;
  tx = float(sense)/5397 * 3.3;
  return tx;
}

//Output Direction Function-----------------------------------------------------------------------------------------------------------------
void setDirection(bool direction) {
  //True = forwards
  //False = backwards
  //Close both sides of H-Bridge
  digitalWrite(H_POS, LOW);
  digitalWrite(H_NEG, LOW);
  //Short pause to eliminate shoot-through
  delay(1);
  //Enable desired direction
  if (direction == true) {
    digitalWrite(H_POS, HIGH);
  }
  if (direction == false) {
    digitalWrite(H_NEG, HIGH);
  }
}

//Triangle Wave Functions------------------------------------------------------------------------------------------------------------------
//Creates a symmetrical Triangle wave with positive current
void posTriangle() {
   setDirection(true);
    //rising
    while(dacOut < dacMax) {
      setDac(dacOut);
      dacOut++;
    }
    //falling
    while(dacOut > 0) {
      setDac(dacOut);
      dacOut--;
    }
    //make sure loop ends at 0
    dacOut = 0;
}

//Creates a symmetrical Triangle wave with negative current
void negTriangle() {
  setDirection(false);
    //rising
    while(dacOut < dacMax) {
      setDac(dacOut);
      dacOut++;
    }
    //falling
    while(dacOut > 0) {
      setDac(dacOut);
      dacOut--;
    }
  //make sure loop ends at 0
  dacOut = 0;
}

//Creates a symmetrical Triangle wave with positive and negative current
//Zero crossing will have some delay to prevent shoot-through
void biTriangle() {
  posTriangle();
  negTriangle();
}

void fastTriangle() {
  if (firstTime == true) {
    setDirection(true);
  }
  //rising
  while(dacOut < dacMax-8) {
    setDac(dacOut);
    dacOut=dacOut+8;
  }
  //falling
  while(dacOut > 8) {
    setDac(dacOut);
    dacOut=dacOut-8;
  }
  //make sure loop ends at 0
  dacOut = 0;
}
//PWM Functions----------------------------------------------------------------------------------------------------------------------------
void PWM50() {
  if (firstTime == true) {
    setDirection(true);
  }
  setDac(0);
  delay(50);
  setDac(4095);
  delay(50);
}

void PWM25() {
  if (firstTime == true) {
    setDirection(true);
  }
  setDac(0);
  delay(75);
  setDac(4095);
  delay(25);
}

void PWM10() {
  if (firstTime == true) {
    setDirection(true);
  }
  setDac(0);
  delay(90);
  setDac(4095);
  delay(10);
}
