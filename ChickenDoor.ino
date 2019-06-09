#include <Wire.h>
#include <TimeLib.h>
#include <DS1307RTC.h>
#include <Adafruit_MotorShield.h>
#include <TFT.h>  
#include <SPI.h>

byte startHour=7;
byte startMinute=00;
byte endHour= 20;
byte endMinute= 30;

tmElements_t tm, tm_hour_start, tm_hour_end;
time_t now_, t_hour_start, t_hour_end;
bool update_tm = 1;

// Initialize status variables
bool doorIsOpen = 0;
bool engineRunning = 0;
bool manualOverride = 0;

int ledPin = 13; // choose the pin for the LED

// Initialize buttons
int inButtonUp = 3;   // choose the input pin (for a pushbutton)
int inButtonDown = 4;   // choose the input pin (for a pushbutton)
int valButtonUp = 0;     // variable for reading the pin status
int valButtonDown = 0;     // variable for reading the pin status

// Initialize door sensors
int inTouchUp = 6;   // choose the input pin (for a pushbutton)
int inTouchDown = 7;   // choose the input pin (for a pushbutton)
int valTouchUp = 0;     // variable for reading the pin status
int valTouchDown = 0;     // variable for reading the pin statu

// Create the motor shield object with the default I2C address
Adafruit_MotorShield AFMS = Adafruit_MotorShield(); 

// Select which 'port' M1, M2, M3 or M4. In this case, M1
Adafruit_DCMotor *myMotor = AFMS.getMotor(1);

// TFT pin definition for Arduino UNO
#define cs   10
#define dc   9
#define rst  8

// Create an instance of the TFT library
TFT TFTscreen = TFT(cs, dc, rst);

// Initialise status messages
char doorPrintout[6];
char enginePrintout[6];
char systemPrintout[6];
char openTimePrintout[6];
char closeTimePrintout[6];

void setup() {
  Serial.begin(9600);           // set up Serial library at 9600 bps
  while (!Serial) ; // wait for serial
  delay(200);
  Serial.println("Chickenz running!");
  Serial.println("-------------------");

  AFMS.begin();  // create with the default frequency 1.6KHz
  //AFMS.begin(1000);  // OR with a different frequency, say 1KHz
  
  // Set the speed to start, from 0 (off) to 255 (max speed)
  myMotor->setSpeed(255);
  myMotor->run(FORWARD);
  // turn on motor
  myMotor->run(RELEASE);

  pinMode(ledPin, OUTPUT);  // declare LED as output
  
  pinMode(inButtonUp, INPUT);    // declare pushbutton as input
  pinMode(inButtonDown, INPUT);    // declare pushbutton as input

  pinMode(inTouchUp, INPUT);    // declare push sensor as input
  pinMode(inTouchDown, INPUT);    // declare push sensor as input


  // Initialize the library
  TFTscreen.begin();
  // Clear the screen with a black background
  TFTscreen.background(0, 0, 0);
  // Set font color
  TFTscreen.stroke(50, 50, 50);
  //Set the text size
  TFTscreen.setTextSize(2);

  // Print static text
  TFTscreen.text("< Chickenz >", 5, 2);
  TFTscreen.text("------------", 5, 12);
  TFTscreen.text("Door:", 5, 25);
  TFTscreen.text("Engine:", 5, 45);
  TFTscreen.text("System:", 5, 65);
  TFTscreen.text("Open:", 5, 85);
  TFTscreen.text("Close:", 5, 105);

  // Print default status messages
  printStatus(doorPrintout, "Shut", 25);
  printStatus(enginePrintout, "Off", 45);
  printStatus(systemPrintout, "Auto", 65);
  printStatus(openTimePrintout, getOpenTime(), 85);
  printStatus(closeTimePrintout, getCloseTime(), 105);
}

void loop() {  
  // read input value from buttons
  valButtonUp = digitalRead(inButtonUp);
  valButtonDown = digitalRead(inButtonDown);

  // read door sensors  
  valTouchUp = digitalRead(inTouchUp);
  valTouchDown = digitalRead(inTouchDown); 
  
  if (valTouchUp == LOW && !doorIsOpen && engineRunning) {
    digitalWrite(ledPin, HIGH);
    Serial.print("Touching up\n");
    doorIsUp();
  } else if (valTouchDown == LOW && doorIsOpen && engineRunning) {
    digitalWrite(ledPin, HIGH);
    Serial.print("Touching down\n");
    doorIsDown();
  } else {
    digitalWrite(ledPin, LOW);
  }    
  
  //get current timestamp
  now_ = RTC.get();
  
  // make current date and time structure
  breakTime(now_, tm);
  
  // make auxiliary structures to be more human editable and friendly
  if(update_tm){
    memcpy(&tm_hour_start, &tm, sizeof(tm));
    memcpy(&tm_hour_end, &tm, sizeof(tm));
    
    // change auxiliary structures to meet your start and end schedule 
    tm_hour_start.Hour = startHour;
    tm_hour_start.Minute = startMinute;
    tm_hour_start.Second = 0;
    tm_hour_end.Hour = endHour;
    tm_hour_end.Minute = endMinute;
    tm_hour_end.Second = 0;
    
    // reverse process to get timestamps
    t_hour_start = makeTime(tm_hour_start);
    t_hour_end = makeTime(tm_hour_end);

    // check if end time is past midnight and correct if needed
    if (startHour > endHour) //past midnight correction
      t_hour_end = t_hour_end + SECS_PER_DAY;
  }
  //final part   

  if (!manualOverride) {
    if ((t_hour_start <= now_) && (now_ <= t_hour_end)){
      /* if we got a valid schedule, don't change the tm_hour structures and the 
      respective t_hour_start and t_hour_end timestamps. They should be updated 
      after exiting the valid schedule */
      if(update_tm)  
        update_tm = 0;
  
      //do something if soo...
      Serial.print("Inside interval\n");
      openDoor();
      
    }else{
      if(update_tm == 0)  
        update_tm = 1;
      // do something if not...
      Serial.print("Outside interval\n");
      closeDoor();
    }
  }

  if (valButtonUp == HIGH && manualOverride && doorIsOpen) {
    Serial.print("Re-activating automatic schedule");
    manualOverride = false;
    printStatus(systemPrintout, "Auto", 65);
    delay(2000);
  } else {
    if (valButtonUp == HIGH) {         // check if the input is HIGH (button released)
      digitalWrite(ledPin, HIGH);  // turn LED ON
      Serial.print("Manually opening door\n");
      manualOverride = true;
      printStatus(systemPrintout, "Man", 65);
      openDoor();
    } else if (valButtonDown == HIGH) {         // check if the input is HIGH (button released)
      digitalWrite(ledPin, HIGH);  // turn LED ON
      Serial.print("Manually closing door\n");
      manualOverride = true;
      printStatus(systemPrintout, "Man", 65);
      closeDoor();
    } else {
      digitalWrite(ledPin, LOW);  // turn LED OFF
    }
  }
}

void doorMove(uint8_t direction) {
  uint8_t i;
  
  myMotor->run(direction);
  for (i=0; i<255; i++) {
    myMotor->setSpeed(i);  
    delay(5);
  }
  engineRunning = 1;
  printStatus(enginePrintout, "On", 45);
}

void stopDoor() {
  myMotor->run(RELEASE);
  engineRunning = 0;
  delay(500); //Wait for engine to stop
  printStatus(enginePrintout, "Off", 45);
}

void doorIsDown() {
    stopDoor();
    doorIsOpen = 0;
    Serial.print("Door closed\n");
    printStatus(doorPrintout, "Shut", 25);
}

void doorIsUp() {
    stopDoor();
    doorIsOpen = 1;
    Serial.print("Door open\n");
    printStatus(doorPrintout, "Open", 25);
}

void closeDoor() {  
  if(doorIsOpen && !engineRunning) {
    doorMove(FORWARD);
  }
}

void openDoor() {
  if(!doorIsOpen && !engineRunning) {
    doorMove(BACKWARD);
  }
}

void printStatus(char variable[6], String status, int y) {
  int x = 95;
  // remove current status
  TFTscreen.stroke(0, 0, 0);
  TFTscreen.text(variable, x, y);

  // print new status
  TFTscreen.stroke(50, 50, 50);
  status.toCharArray(variable, 6);
  TFTscreen.text(variable, x, y);
}

String getOpenTime() {
  String openTime = print2digits(startHour) + ":" + print2digits(startMinute);
  Serial.print("Open time is: ");
  Serial.println(openTime);
  return openTime;
}

String getCloseTime() {
  String closeTime = print2digits(endHour) + ":" + print2digits(endMinute);
  Serial.print("Close time is: ");
  Serial.println(closeTime);
  return closeTime;
}

String print2digits(int number) {
  if (number >= 0 && number < 10) {
    return "0" + String(number);
  }
  return String(number);
}
