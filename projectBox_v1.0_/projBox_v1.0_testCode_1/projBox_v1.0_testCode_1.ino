/*------------------------------------------------------------------------------
  PROJECT:      3YP Motorcycle Feedback System Mobile Device App
  FILE:         projBox_v1.0_testCode_1.ino
  AUTHOR:       SSQ16SHU / 100166648

  DESCRIPTION:  Testing of projectBox v1.0 modules & components
                (Ardunino Uno clone, tm1638, hc05, 10k(?) potentiometer)
                Arduino inputs simulate motorcycle 'status' (eg speed, indicator
                on/off etc), sending values to be interpreted by android app.
  ------------------------------------------------------------------------------
  HISTORY:      v1.0    200120  Initial implementation from existing code
                v1.1    200120  adapted to display speed integer.
                v1.2    200120  implemented sending bike status vector (naive)
                v1.2.1  200122  tidied code
                v2.0    200123  added bitshift button trigger (test).
                v2.1    200319  changed indicators to boolean, added toggling
                                and display to buttons. (revised for clarity).
  ------------------------------------------------------------------------------
  NOTES:      + Bluetooth Rx & Tx are on digital pins to avoid disconnection on
                every upload of new/edited sketch(es).
              + Button trigger has both 7seg reset and delay repeated in each
                check to prevent triggering every loop.
              + While methods exist to clear display, it is often more efficiant
                to specify empty space when displaying text to module (8 char).
              + Potentiometer reading fluctuates very slightly (+/- 1mph) due to
                unstable USB powersupply voltage. This is acceptable as this
                reflects the ever-present slight changes made by manual control.
  ------------------------------------------------------------------------------
  PLANNED IMPROVEMENT:
              + assembling variables into single vector BEFORE sending.
              + possibly change speed increments (ie top out at 75mph: no value
                past this point is legal in UK) : hopefully improve accuracy?
              + try to decrease loop time to improve catch rate of buttons.
  ------------------------------------------------------------------------------
  TODO:       - test transmission (loop) delay
              - Implement non-delay code for input(ie tm1638 buttons)
              - add button input for vector status changes
              - change speed variable to int (app doesnt really need fractions)
              - change to array for transmission (int)
              - investigate other functions (eg peek) for better serial reading
              - tidy code incl header
  ----------------------------------------------------------------------------*/

//Header files------------------------------------------------------------------
#include <SoftwareSerial.h>                     //digital pin use for serial communication(HC05)
#include <TM1638plus.h>                         //tm1638 module library (plus version)


//Definitions-------------------------------------------------------------------
//tm1638 module digital pins
#define  STROBE_TM 5
#define  CLOCK_TM 6
#define  DIO_TM 7

//button listener masks
#define BUTTON_MASK_0 (0x1 << 0)                //used for indicator left
#define BUTTON_MASK_1 (0x1 << 1)                //used for indicator right
#define BUTTON_MASK_2 (0x1 << 2)                //used for low beams
#define BUTTON_MASK_3 (0x1 << 3)                //used for high beams
#define BUTTON_MASK_4 (0x1 << 4)
#define BUTTON_MASK_5 (0x1 << 5)
#define BUTTON_MASK_6 (0x1 << 6)
#define BUTTON_MASK_7 (0x1 << 7)                //used for display testing


//Constants---------------------------------------------------------------------
const int PIN_Speed = A0;                       //analogue pin for potentiometer
const int LED13 = 13;                           //utility LED


//Variables---------------------------------------------------------------------
int sequenceCounter = 1;                        //testing for lost messages (increment logic 0+1)
char inbyte = 0;                                //Char used for reading in Serial characters
float pot = 0;                                  //value of potentiometer input

//bike status variables
bool indicatorL;                                //1/0 representation of (left) indicator status
bool indicatorR;                                //1/0 representation of (right) indicator status
bool lowbeam;                                   //1/0 representation of headlight status  (currently unused by app: potential ambient light sensor prompt?)
bool highbeam;                                  //1/0 representation of highbeam status   (currently unused by app)
int revCounter = 0;                             //simulation of rev count                 (currently unused by app: potential redline wraning /shift prompt)
float currSpeed = 0;                            //current speed (val 0-99)

//(unused) values for addition or consideration:
int engineTemp = 0;                             //possibly duplicate for C & F option setting?
int fuelLevel = 0;                              //possibly represent as percentage of tank
int checkEngine = 0;                            //1/0 rep
int currentGear = 0;                            //translate 0 as 1 and 1 as N, 2 as 3 etc app-side
int absActive = 0;                              //abs system on: useful?
int tractionControl = 0;                        //1/0 active if present
int parkingLights = 0;                          //1/0 for parking light warning
//data available after engine shut off?)
float odoTotal = 0;                             //Odometer value
float odoTripA = 0;                             //first trip record
float odoTribB = 0;                             //multiple trip (majority of vehicles have 1+)
float batteryVoltage = 0;                       //current voltage level


//Object Constructors-----------------------------------------------------------
SoftwareSerial EEBlue(10, 11);                  //Bluetooth serial I/O
TM1638plus tm(STROBE_TM, CLOCK_TM , DIO_TM);    //tm1638 module on digital pins


//Program-----------------------------------------------------------------------

void setup() {
  //initialise pins---------------------
  pinMode(PIN_Speed, INPUT);                    //initialize potentiometer on A0
  pinMode(LED13, OUTPUT);                       //initialize LED on digital pin13 as output
  digitalWrite(LED13, LOW);                     //begin with utility LED off

  //initialise serial and bluetooth-----
  Serial.begin(9600);
  EEBlue.begin(9600);
  while (!Serial) {
    ; //wait for serial port to connect
  }
  tm.displayText("TRANSMIT");                   //notify ready (via display)
  delay(250);                                   //readable delay
  tm.displayText("BEGIN   ");
  delay(250);
  Serial.println("Bluetooth setup complete.\n Connect to HC-05 (key: 1234)");

  //tm1638 setup------------------------
  tm.reset();                                   //clear tm1638 module
  tm.brightness(0);                             //set low brightness for tm1638 module

  //tm1638 testing sequence-------------
  //tmTesting();                                //(testing) TM1638 module testing (assigned to button 7)
  Serial.println("Testing complete");
}


void loop() {
  //get input-------------------------
  uint8_t buttons = tm.readButtons();           //get current status of buttons as byte
  getButtons(buttons);                          //trigger methods from bitwise shift of byte
  //doLEDs(buttons);                              //(testing: visual demo) trigger button-associated LED

  //speed from potentiometer----------
  getSpeed();                                   //set speed value from potentiometer resistance
  displaySpeed();                               //display current speed value to tm1638 (7seg)

  //send bike status to app-----------
  //Serial.println("begin sending");              //(testing: visual demo) print to serial monitor
  sendVector();                                 //send current bike status array values to android via hc05

  //loop stability delay--------------
  delay(250);                                   //modifiable delay to control loop iteration speed
}


//Functions---------------------------------------------------------------------

//--------------------------------------
//sends the values over Bluetooth serial to app. (naive version: for testing)
//--------------------------------------
void sendVector() {
  EEBlue.print('#');                            //begin char (signal start of array index to app)
  EEBlue.print(sequenceCounter);
  EEBlue.print('+');                            //'next value' char (split received string)
  EEBlue.print(currSpeed);
  EEBlue.print('+');
  EEBlue.print(indicatorL);
  EEBlue.print('+');
  EEBlue.print(indicatorR);
  EEBlue.print('+');
  EEBlue.print(lowbeam);
  EEBlue.print('+');
  EEBlue.print(highbeam);
  EEBlue.print('+');
  EEBlue.print(revCounter);
  EEBlue.print('+');
  EEBlue.print('~');                            //end char (signal end of index to app)
  Serial.println();                             //push serial monitor to next line (testing)

  sequenceCounter++;                            //increment sequential count
}



//--------------------------------------
//converts float value of speed to int for tm1638 7seg display
//--------------------------------------
void displaySpeed() {
  tm.displayText("SPEED");                      //display context to 7seg
  int tens = currSpeed / 10;                    //get individual int of speed value
  int ones = currSpeed - (10 * tens);

  tm.displayHex(6, tens);                       //display to last two 7segments (6,7)
  tm.displayHex(7, ones);
}

//--------------------------------------
//interprets passed button (0-7) status byte and calls assigned functions
//(exclusive operation: else if: disallows combination of buttons)
//(indicator and headlights also affect their counterparts to mimic real life)
//--------------------------------------
void getButtons(uint8_t value) {

  if (value & BUTTON_MASK_0) {
    Serial.println("Button 0: toggle left indicator");
    //indicator left: toggle on/off
    indicatorL = !indicatorL;
    indicatorR = false;                         //turn off opposite indicator if already on (exclusive on)
    if (indicatorL) {
      tm.displayText("IND L1R0");
    } else {
      tm.displayText("IND L0R0");
    }

    Serial.print("indicator L updated: ");      //(testing) serial monitor value output
    Serial.println(indicatorL);

    delay(500);                                 //half-second display read-time (longer causes latency)
    tm.reset();
  }

  else if (value & BUTTON_MASK_1) {
    Serial.println("Button 1: toggle right indicator");
    //indicator right toggle on/off
    indicatorR = !indicatorR;
    indicatorL = false;                         //turn off opposite indicator if already on (exclusive on)
    if (indicatorR) {
      tm.displayText("IND L0R1");
    } else {
      tm.displayText("IND L0R0");
    }
    Serial.print("indicator R updated: ");      //testing output
    Serial.println(indicatorR);
    delay(500);
    tm.reset();
  }

  else if (value & BUTTON_MASK_2) {
    Serial.println("Button 2: toggle lowbeam");
    highbeam = false;                           //turn off highbeam (as either turning off lights or they ARE off & only lowbeams on)
    lowbeam = !lowbeam;
    if (lowbeam) {
      tm.displayText("LUX L1H0");
    } else {
      tm.displayText("LUX L0H0");
    }
    Serial.print("Lowbeam staus updated: ");    //testing output
    Serial.println(lowbeam);
    Serial.print("highbeams: ");
    Serial.println(highbeam);
    delay(500);
    tm.reset();
  }

  else if (value & BUTTON_MASK_3) {
    Serial.println("Button 3: toggle highbeams");
    highbeam = !highbeam;
    if (highbeam) {
      if (!lowbeam) {
        lowbeam = true;                         ////turn on lowbeam (as ON if highbeams are on)
      }
      tm.displayText("LUX L1H1");
    } else {
      if (lowbeam) {
        tm.displayText("LUX L1H0");
      } else {
        tm.displayText("LUX L0H0");
      }
    }
    Serial.print("Highbeam staus updated: ");   //testing output
    Serial.println(highbeam);
    Serial.print("lowbeams: ");
    Serial.println(lowbeam);
    delay(500);
    tm.reset();
  }

  else if (value & BUTTON_MASK_4) {
    //placeholder visible activity
    tm.displayText("BUTTON 4");
    Serial.println("Button 4");
    delay(1000);
    tm.reset();
  }
  else if (value & BUTTON_MASK_5) {
    //placeholder visible activity
    tm.displayText("BUTTON 5");
    Serial.println("Button 5");
    delay(1000);
    tm.reset();
  }
  else if (value & BUTTON_MASK_6) {
    //placeholder visible activity
    tm.displayText("BUTTON 6");
    Serial.println("Button 6");
    delay(1000);
    tm.reset();
  }
  else if (value & BUTTON_MASK_7) {
    //display testing function
    tm.reset();
    tmTesting();
  }

}


//--------------------------------------
//read analogue value from potentiometer and convert voltage to speed value
//--------------------------------------
void getSpeed() {
  float volt = analogRead(pot);                 //get potentiometer value (of 2^10 values)
  currSpeed = volt / 10.24;                     //divide 2^10 value to get 0-100mph
}


//Test Functions----------------------------------------------------------------
//--------------------------------------
//momentary trigger corresponding LED by button
//--------------------------------------
void doLEDs(uint8_t value) {
  for (uint8_t position = 0; position < 8; position++) {
    tm.setLED(position, value & 1);
    value = value >> 1;
  }
}


//--------------------------------------
//testSequence of sub-test functions of tm1638 module
//--------------------------------------
void tmTesting() {
  tm.displayText("testing");
  delay(500);
  test_led_seq();                               //sequential LED
  test_led_flash();                             //strobe all LED
  test_7seg_brightness();                       //7seg light levels
  test_7seg_hex();                              //iterate all 7seg hex numbers
  test_7seg_bytes();                            //demo segment letters
  test_7seg_segments();                         //sequential segment loops
  test_7seg_dots();                             //sequential momentary dots
  test_7seg_flash();                            //strobe 7seg
  tm.displayText("FIN TEST");
  delay(500);
  tm.reset();
}


//sub-test functions (tm1638)-------------------------------
//strobe leds-------
void test_led_flash() {
  for (int i = 0; i < 8; i++) {
    tm.setLED(i, 1);
  }
  delay(200);
  for (int i = 0; i < 8; i++) {
    tm.setLED(i, 0);
  }
}


//sequential leds---
void test_led_seq() {
  for (int i = 0; i < 8; i++) {
    tm.setLED(i, 1);
    delay(50);
    tm.setLED(i, 0);
  }
}


//hex no.-----------
void test_7seg_hex() {
  //for each digit (0-9)
  for (int i = 0; i < 10; i++) {
    //for each display (0-7)
    for (int j = 0; j < 8; j++) {
      tm.displayHex(j, i);
      delay(20);
    }
  }
  tm.reset();
}


//individual seg----
void test_7seg_segments() {
  for (int i = 0; i < 8; i++) {
    tm.display7Seg(i, 0b00000001);
    delay(30);
    tm.display7Seg(i, 0b00000011);
    delay(30);
    tm.display7Seg(i, 0b00000111);
    delay(30);
    tm.display7Seg(i, 0b00001111);
    delay(30);
    tm.display7Seg(i, 0b00011111);
    delay(30);
    tm.display7Seg(i, 0b00111111);
    delay(30);
    tm.display7Seg(i, 0b01111111);
    delay(50);
    tm.display7Seg(i, 0b10000000);
    delay(100);
    tm.displayHex(i, 8);
  }
  tm.reset();
}


//byte display------
void test_7seg_bytes() {
  tm.displayText("segments");
  delay(1000);
  tm.displayText("   SEG=");
  tm.display7Seg(1, 0b00000001);
  tm.displayASCII(7, 'A');
  delay(400);
  tm.display7Seg(1, 0b00000010);
  tm.displayASCII(7, 'B');
  delay(400);
  tm.display7Seg(1, 0b00000100);
  tm.displayASCII(7, 'C');
  delay(400);
  tm.display7Seg(1, 0b00001000);
  tm.displayASCII(7, 'D');
  delay(400);
  tm.display7Seg(1, 0b00010000);
  tm.displayASCII(7, 'E');
  delay(400);
  tm.display7Seg(1, 0b00100000);
  tm.displayASCII(7, 'F');
  delay(400);
  tm.display7Seg(1, 0b01000000);
  tm.displayASCII(7, 'G');
  delay(500);
  tm.reset();
}


//brightness (7seg)-
void test_7seg_brightness() {
  for (uint8_t brightness = 0; brightness < 4; brightness++) {
    tm.brightness(brightness);
    tm.displayText("88888888");
    test_led_flash();
    delay(200);
  }
  tm.reset();
  delay(100);

  for (uint8_t brightness = 4; brightness > 1; brightness--) {
    tm.brightness(brightness - 1);              //brightness decrement: avoid =0 loop
    tm.displayText("88888888");
    test_led_flash();
    delay(200);
  }
  tm.reset();
  tm.brightness(0x02);                          //restore default brightness

}

//strobe 7seg-------
void test_7seg_flash() {
  for (int r = 0; r < 3; r++) {
    //all seg ON
    for (int i = 0; i < 8; i++) {
      tm.displayASCIIwDot(i, '8');
    }
    delay(100);
    //all seg OFF
    for (int i = 0; i < 8; i++) {
      tm.displayASCIIwDot(i, ' ');
    }
    delay(100);
  }
}


//7seg dots---------
void test_7seg_dots() {
  for (int i = 0; i < 8; i++) {
    tm.displayASCII(i, '.');
    delay(50);
    tm.displayHex(i, 'A');
  }
}


//clear 7seg--------
void test_7seg_empty() {
  //clear all segments without module reset
  for (int s = 0; s < 8; s++) {
    tm.displayHex(s, 'A');
  }
}


//obsolete test methods---------------------------------------------------------
//--------------------------------------
//separating float to individual int representations for 7seg display
//--------------------------------------
void displaySpeedTest() {
  int big, a, b, c, d;

  //get whole int representation of 2dp float value (multiply by 100)
  big = currSpeed * 100;
  //split into individual int of thousands, hundreds, tens, ones
  a = big / 1000;
  b = (big / 100) - (10 * a);
  c = (big / 10) - (10 * b) - (100 * a);
  d = big - (10 * c) - (100 * b) - (1000 * a);
  //display to 7seg as hex
  tm.displayHex(3, a);
  tm.displayHex(4, b);
  tm.displayASCII(5, '.');
  tm.displayHex(6, c);
  tm.displayHex(7, d);
}


//--------------------------------------
//send analog input from potentiometer to android
//--------------------------------------
void sendSpeed()
{
  getSpeed();                                   //get currSpeed from potentiometer
  EEBlue.print('#');                            //begin transmission character (start index)
  EEBlue.print(currSpeed);                      //send current speed
  EEBlue.print('+');                            //change of value char (debug and split string)
  EEBlue.print('~');                            //end transmission character (signal end index)

  Serial.println();                             //push serial to next line
  Serial.println(currSpeed);                    //(testing) duplicate sent variable to monitor
}
