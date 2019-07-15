#include <Wire.h>
#include <TimeLib.h>
#include <DS1307RTC.h>
#include <Adafruit_MotorShield.h>
#include <SparkFunColorLCDShield.h>
#include <SPI.h>

// Set open and close times
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
int timeoutCounter = 3000;
bool timerOn = 0;

int ledPin = 12; // choose the pin for the LED

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

// Create an instance of the TFT library
LCDShield lcd;  // Creates an LCDShield, named lcd

// Initialize status messages
char doorPrintout[6];
char enginePrintout[6];
char systemPrintout[6];
char openTimePrintout[6];
char closeTimePrintout[6];

// 24 hour loop variable
time_t previousWeekday;
void setup() {
  Serial.begin(9600);           // Set up Serial library at 9600 bps
  while (!Serial) ; // Wait for serial
  delay(200);
  Serial.println("Chickenz running!");
  Serial.println("-------------------");

  AFMS.begin();  // Create with the default frequency 1.6KHz
  
  // Set the speed to start, from 0 (off) to 255 (max speed)
  myMotor->setSpeed(255);
  myMotor->run(FORWARD);
  // Turn on motor
  myMotor->run(RELEASE);

  pinMode(ledPin, OUTPUT);  // Declare LED as output
  
  pinMode(inButtonUp, INPUT);    // Declare pushbutton as input
  pinMode(inButtonDown, INPUT);    // Declare pushbutton as input

  pinMode(inTouchUp, INPUT);    // Declare push sensor as input
  pinMode(inTouchDown, INPUT);    // Declare push sensor as input


  // Initialize the LCD screen
  lcd.init(PHILIPS);  // Initializes lcd, using an PHILIPSdriver
  lcd.contrast(-61);  // -51's usually a good contrast value
  lcd.clear(WHITE);  // clear the screen
  
  // Print static text
  lcd.setStr("< Chickenz >", 2, 15, BLACK, WHITE);
  lcd.setLine(23, 5, 23, 125, BLACK);
  lcd.setStr("Door:", 25, 5, BLACK, WHITE);
  lcd.setStr("Engine:", 45, 5, BLACK, WHITE);
  lcd.setStr("System:", 65, 5, BLACK, WHITE);
  lcd.setStr("Open:", 85, 5, BLACK, WHITE);
  lcd.setStr("Close:", 105, 5, BLACK, WHITE);

  // Print default status messages
  printStatus(doorPrintout, "Shut", 25, V5);
  printStatus(enginePrintout, "Off", 45, V6);
  printStatus(systemPrintout, "Auto", 65, V7);
  printStatus(openTimePrintout, getOpenTime(), 85, V8);
  printStatus(closeTimePrintout, getCloseTime(), 105, V9);
  // Init 24 hour loop
  previousWeekday = now();
}

void loop() {  
  // Read input value from buttons
  valButtonUp = digitalRead(inButtonUp);
  valButtonDown = digitalRead(inButtonDown);

  // Read door sensors  
  valTouchUp = digitalRead(inTouchUp);
  valTouchDown = digitalRead(inTouchDown); 

  // Tick safety timeout counter
  if(timerOn && timeoutCounter>=0) {
    timeoutCounter = timeoutCounter-1;
    Serial.println(timeoutCounter);
  }

  // Detect if door is open or closed
  if (valTouchUp == LOW && !doorIsOpen && engineRunning) {
    digitalWrite(ledPin, HIGH);
    Serial.print("Touching up\n");
    doorIsUp();
  } else if (valTouchDown == LOW && doorIsOpen && engineRunning) {
    digitalWrite(ledPin, HIGH);
    Serial.print("Touching down\n");
    doorIsDown();
  } else if (timeoutCounter == 0) {
    Serial.println("Safety timeout reached, door stopped");
    doorFailed();
    printStatus(systemPrintout, "Fail", 65, V7);
  } else {
    digitalWrite(ledPin, LOW);
  }
  
  // Get current timestamp
  now_ = RTC.get();
  
  // Make current date and time structure
  breakTime(now_, tm);
  
  // Make auxiliary structures to be more human editable and friendly
  if(update_tm){
    memcpy(&tm_hour_start, &tm, sizeof(tm));
    memcpy(&tm_hour_end, &tm, sizeof(tm));
    
    // Change auxiliary structures to meet your start and end schedule 
    tm_hour_start.Hour = startHour;
    tm_hour_start.Minute = startMinute;
    tm_hour_start.Second = 0;
    tm_hour_end.Hour = endHour;
    tm_hour_end.Minute = endMinute;
    tm_hour_end.Second = 0;
    
    // Reverse process to get timestamps
    t_hour_start = makeTime(tm_hour_start);
    t_hour_end = makeTime(tm_hour_end);

    // Check if end time is past midnight and correct if needed
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
  
      // Do something if current time is inside interval
      Serial.print("Inside interval\n");
      openDoor();
      
    }else{
      if(update_tm == 0)  
        update_tm = 1;
      // Do something else if current time is outside interval
      Serial.print("Outside interval\n");
      closeDoor();
    }
  }

  if (valButtonUp == HIGH && manualOverride && doorIsOpen) {
    Serial.print("Re-activating automatic schedule");
    manualOverride = false;
    printStatus(systemPrintout, "Auto", 65, V7);
    delay(1000);
  } else {
    if (valButtonUp == HIGH) {         // check if the input is HIGH (button released)
      digitalWrite(ledPin, HIGH);  // turn LED ON
      Serial.print("Manually opening door\n");
      manualOverride = true;
      printStatus(systemPrintout, "Man", 65, V7);
      openDoor();
    } else if (valButtonDown == HIGH) {         // check if the input is HIGH (button released)
      digitalWrite(ledPin, HIGH);  // turn LED ON
      Serial.print("Manually closing door\n");
      manualOverride = true;
      printStatus(systemPrintout, "Man", 65, V7);
      closeDoor();
    } else {
      digitalWrite(ledPin, LOW);  // turn LED OFF
    }
  }
}

void doorMove(uint8_t direction) {
  uint8_t i;
  
  printStatus(enginePrintout, "On", 45, V6);
  
  myMotor->run(direction);
  for (i=0; i<255; i++) {
    myMotor->setSpeed(i);  
    delay(5);
  }
  engineRunning = 1;
}

void stopDoor() {
  stopTimer();
  myMotor->run(RELEASE);
  engineRunning = 0;
  delay(500); //Wait for engine to stop
  printStatus(enginePrintout, "Off", 45, V6);
}

void doorIsDown() {
    stopDoor();
    doorIsOpen = 0;
    Serial.print("Door closed\n");
    printStatus(doorPrintout, "Shut", 25, V5);
}

void doorIsUp() {
    stopDoor();
    doorIsOpen = 1;
    Serial.print("Door open\n");
    printStatus(doorPrintout, "Open", 25, V5);
}

void doorFailed() {
    stopDoor();
    doorIsOpen = 1;
    Serial.print("Door failed. Press down to reset\n");
    printStatus(doorPrintout, "?", 25, V5);
}

void closeDoor() {  
  if(doorIsOpen && !engineRunning) {
    printStatus(doorPrintout, "\\/", 25, V5);
    startTimer(1800);
    doorMove(FORWARD);
  }
}

void openDoor() {
  if(!doorIsOpen && !engineRunning) {
    printStatus(doorPrintout, "/\\", 25, V5);
    startTimer(2000);
    doorMove(BACKWARD);
  }
}

void printStatus(char variable[6], String status, int y, int pin) {
  int x = 75;
  // Remove current status
  lcd.setStr(variable, y, x, WHITE, WHITE);

  // Print new status
  status.toCharArray(variable, 6);
  lcd.setStr(variable, y, x, BLACK, WHITE);
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

void startTimer(int time) {
  timeoutCounter = time;
  timerOn = 1;
}

void stopTimer() {
  timerOn = 0;
  timeoutCounter = -1;
}
