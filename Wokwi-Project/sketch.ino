// Log OBD information to an SD card

// Output file format:

// The events file is appended to any time the RPM is measured over a certain threshold
// A blank line is added between sessions
// events.txt
  // Timestamp           | RPM
  // 2023-11-28 19:10:24 | 9999.48
  // 2023-11-28 19:10:24 | 9999.48
  // 2023-11-28 19:10:24 | 9999.48
  //
  // 2023-11-28 19:10:24 | 9999.48

// Every time we start a new session, a new log file is created,
//   numbered using the current month, day, and time
// MMDDhhmm.txt
// 11300131.txt
  // 2023-11-28 19:10:24
  // 99999 mins since OBD cleared
  //
  // Timestamp||RPM
  // 19:10:24.0 943.45
  // 19:10:24.5 949.74
  // 19:10:25.0 340.22
  // 19:10:25.5 9999.48 <---
  // 19:10:26.0 642.43


////////    Config    ////////

//// Constants/Settings ////
#define DEBUG  // Print debug lines to serial, comment out to disable

#define RPM_THRESHOLD 5000.0f  // RPM over which to log as an event

#define EVENT_FILE_NAME "/events.txt"  // Follow 8.3 file naming scheme
////////

//// Pins ////
// These pins can be changed if needed, but it's recommended to use them as is

// RTC Clock
//      RTA_SDA_PIN 21
//      RTA_SCL_PIN 22

// SD Card
#define CS_PIN  5
//      SCK_PIN 18
//      MISO_DO_PIN  19
//      MOSI_DI_PIN  23
////////

////////    End Config    ////////


// TODO: test for data loss/file size on power loss
// TODO: probably flush the event file on each event
// TODO: possibly flush the log file occasionally


// #include <BluetoothSerial.h>
// #include <ELMduino.h>
#include <RTClib.h>
#include <SD.h>


// Constants / Globals
#ifdef DEBUG
  #define DEBUG_BEGIN(x)   Serial.begin (x)
  #define DEBUG_PRINT(x)   Serial.print (x)
  #define DEBUG_PRINTLN(x) Serial.println (x)
  #define ELM_DEBUG true
#else
  #define DEBUG_BEGIN(x)
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define ELM_DEBUG false
#endif


RTC_DS1307 rtc;
DateTime setupTime;
struct nowStruct {
  DateTime dateTime;
  uint8_t deciseconds;
} now;

File eventFile;
File logFile;

// BluetoothSerial SerialBT;
// #define ELM_PORT   SerialBT
// ELM327 myELM327;

// uint32_t rpm = 0;



void blink_led_loop(int interval) {
  while (true) {
#ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, HIGH);
    delay(interval);
    digitalWrite(LED_BUILTIN, LOW);
    delay(interval);
#endif
  }
}


void updateTime() {
  long internalNow = millis();
  now.dateTime = setupTime + TimeSpan(internalNow/1000);
  now.deciseconds = (internalNow % 1000) / 100;
}

void logDataLine(float currRPM) {
  logFile.print(now.dateTime.timestamp(DateTime::TIMESTAMP_TIME));
  logFile.print(".");
  logFile.print(now.deciseconds);
  logFile.print(" ");
  logFile.print(currRPM);
  if (currRPM > RPM_THRESHOLD) {
    // Mark the line if it's over the threshold to make it clearly visible
    logFile.println(" <---");
  } else {
    logFile.println();
  }
}

void logEvent(float currRPM) {
  DEBUG_PRINTLN("RPM exceeded threshold, logging event:");

  char dateTimeBuffer[20] = "YYYY-MM-DD hh:mm:ss";
  now.dateTime.toString(dateTimeBuffer);

  DEBUG_PRINT(dateTimeBuffer);
  DEBUG_PRINT(" | ");
  DEBUG_PRINTLN(currRPM);

  eventFile.print(dateTimeBuffer);
  eventFile.print(" | ");
  eventFile.println(currRPM);
}


bool openEventFile() {
  bool existingFile = SD.exists(EVENT_FILE_NAME);

  eventFile = SD.open(EVENT_FILE_NAME, FILE_WRITE);

  if (!eventFile) {
    // File didn't open or wasn't created
    return false;
  }

  if (existingFile) {
    // File already exists so we just add a blank line to indicate a new session
    eventFile.println();
    eventFile.println();
  } else {
    // New file, so we add the file header
    eventFile.println("Timestamp           | RPM");
    DEBUG_PRINTLN("Created new event file");
  }

  return true;
}

bool openNewLogFile() {
  char logFileName[14] = "/MMDDhhmm.txt";
  // char logFileName[14] = "/11300225.txt";
  setupTime.toString(logFileName);
  
  DEBUG_PRINT("Writing new log file to:");
  DEBUG_PRINTLN(logFileName);

  logFile = SD.open(logFileName, FILE_WRITE);

  if (!logFile) {
    // File didn't open or wasn't created
    return false;
  }

  // Writes the following to the new file
    // 2023-11-28 19:10:24
    // 99999 mins since OBD cleared
    //
    // Timestamp||RPM
    //

  char dateTimeBuffer[20] = "YYYY-MM-DD hh:mm:ss";
  logFile.println(setupTime.toString(dateTimeBuffer));
  
  logFile.println(); // TODO: OBD time

  logFile.println("\r\nTimestamp||RPM");

  return true;
}

// void flushLogFile() {
//   logFile.close();
//   // openLogFile();
// }



void setup() {
  DEBUG_BEGIN(115200);

#ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
#endif

  // RTC Clock
  if (!rtc.begin()) {
    DEBUG_PRINTLN("Couldn't connect to the RTC clock");
    blink_led_loop(1000);
  }
  
  if (!rtc.isrunning()) {
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    DEBUG_PRINTLN("RTC is NOT running, setting the time now to compile time");
  }

  setupTime = rtc.now();


  // ELM327
  // DEBUG_PRINTLN("Connecting to ELM327 via bluetooth");
  // ELM_PORT.begin("ArduHUD", true);
  
  // if (!ELM_PORT.connect("OBDII")) {
  //   DEBUG_PRINTLN("Couldn't connect to OBD scanner - Phase 1");
  //   blink_led_loop(50);
  // }

  // if (!myELM327.begin(ELM_PORT, ELM_DEBUG, 2000)) {
  //   DEBUG_PRINTLN("Couldn't connect to OBD scanner - Phase 2");
  //   blink_led_loop(200);
  // }
  // DEBUG_PRINTLN("Connected to ELM327");


  // SD Card
  DEBUG_PRINT("Initializing SD card...");
  if (!SD.begin(CS_PIN)) {
    DEBUG_PRINTLN(" initialization failed");
    blink_led_loop(2000);
  }
  DEBUG_PRINTLN(" initialization successful");

  // Files
  if (!openEventFile()) {
    DEBUG_PRINTLN("Error opening the event file");
    blink_led_loop(3000);
  }

  if (!openNewLogFile()) {
    DEBUG_PRINTLN("Error opening the log file");
    blink_led_loop(3000);
  }

  updateTime();
  logDataLine(123.4f);
  // logEvent();
  delay(1240);
 
  updateTime();
  logDataLine(9123.4f);
  logEvent(9123.4f);
  delay(1540);
 
  updateTime();
  logDataLine(1223.4f);
  // logEvent();
  delay(1140);
 
  updateTime();
  logDataLine(6452.1f);
  logEvent(6452.1f);
  delay(1740);
 
  updateTime();
  logDataLine(522.4f);
  // logEvent();
}


void loop() {
  // float tempRPM = myELM327.rpm();

  // if (myELM327.nb_rx_state == ELM_SUCCESS) {
  //   rpm = (uint32_t)tempRPM;
  //   DEBUG_PRINT("RPM: "); DEBUG_PRINTLN(rpm);
  //   logFile.println(rpm);
  // } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
  //   myELM327.printError();
  // }


  // read three 'sensors' and append to the string:
  // for (int analogPin = 0; analogPin < 3; analogPin++) {
  //   int sensor = analogRead(analogPin);
  //   logFile.println(sensor);
  // }

}
