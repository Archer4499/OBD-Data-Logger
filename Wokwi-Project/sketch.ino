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
#define DEBUG            // Print debug lines to serial, comment out to disable
#define RESTART_ON_FAIL  // Restart if any of the muodules fail to initialize, comment out to disable
#define OLD_LIB          // ELMduino hasn't made a realease of the newest code on Github yet

#define DATA_READ_INTERVAL 500      // Milliseconds between OBD RPM data reads
#define RPM_THRESHOLD      5000.0f  // RPM over which to log as an event

#define EVENT_FILE_NAME "/events.txt"  // Follow 8.3 file naming scheme
////////

//// Pins ////

// RTC Clock
// I think these can be any of 4, 13-14, 16-33
#define DS1302_CLK 4
#define DS1302_DAT 16
#define DS1302_RST 17

// SD Card
// These pins can be changed if needed, but it's recommended to use them as is
#define CS_PIN  5
//      SCK_PIN 18
//      MISO_DO_PIN  19
//      MOSI_DI_PIN  23
////////

////////    End Config    ////////


// TODO: test for data loss/file size on power loss
// TODO: probably flush the event file on each event
// TODO: possibly flush the log file occasionally


#include <BluetoothSerial.h>
#include <ELMduino.h>
#include <RtcDS1302.h>
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


// RTC_DS1307 rtc;
// DateTime setupTime;
// struct nowStruct {
//   DateTime dateTime;
//   uint8_t deciseconds;
// } now;

ThreeWire myWire(DS1302_DAT, DS1302_CLK, DS1302_RST);
RtcDS1302<ThreeWire> rtc(myWire);

RtcDateTime setupTime;
struct NowStruct {
  RtcDateTime dateTime;
  uint8_t deciseconds;
} now;

File eventFile;
File logFile;

BluetoothSerial SerialBT;
#define ELM_PORT   SerialBT
ELM327 myELM327;

long lastDataReadTime = 0;



void blink_led_restart(int interval) {
#ifdef RESTART_ON_FAIL
  for (int i = 0; i < 5000/interval; i++) {
#else
  while (true) {
#endif
#ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, HIGH);
    delay(interval);
    digitalWrite(LED_BUILTIN, LOW);
#endif
    delay(interval);
  }

#ifdef RESTART_ON_FAIL
  ESP.restart();
#endif
}


void updateTime(long currMillis) {
  now.dateTime = setupTime + currMillis/1000;
  now.deciseconds = (currMillis % 1000) / 100;
}

void logDataLine(float currRPM) {
  char timeBuffer[9];
  toTime(timeBuffer, now.dateTime);
  logFile.print(timeBuffer);
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

  logFile.flush();
}

void logEvent(float currRPM) {
  DEBUG_PRINTLN("RPM exceeded threshold, logging event:");

  char dateTimeBuffer[20];
  toDateTime(dateTimeBuffer, now.dateTime);

  DEBUG_PRINT(dateTimeBuffer);
  DEBUG_PRINT(" | ");
  DEBUG_PRINTLN(currRPM);

  eventFile.print(dateTimeBuffer);
  eventFile.print(" | ");
  eventFile.println(currRPM);

  eventFile.flush();
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
  char logFileName[14];
  createLogName(logFileName, setupTime);
  
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

  char dateTimeBuffer[20];
  toDateTime(dateTimeBuffer, setupTime);
  logFile.println(dateTimeBuffer);
  
  logFile.print(getEngineTime());
  logFile.println(" mins since OBD cleared");

  logFile.println("\r\nTimestamp||RPM");

  return true;
}

// void flushLogFile() {
//   logFile.close();
//   // openLogFile();
// }


void toDateTime(char* buffer, const RtcDateTime& dt) {
  snprintf(buffer, 20,
           "%04u-%02u-%02u %02u:%02u:%02u",
           dt.Year(), dt.Month(), dt.Day(),
           dt.Hour(), dt.Minute(), dt.Second() );
}

void toTime(char* buffer, const RtcDateTime& dt) {
  snprintf(buffer, 9, "%02u:%02u:%02u",
           dt.Hour(), dt.Minute(), dt.Second() );
}

void createLogName(char* buffer, const RtcDateTime& dt) {
  snprintf(buffer, 14,
           "/%02u%02u%02u%02u.txt",
           dt.Month(), dt.Day(),
           dt.Hour(), dt.Minute() );
}


bool initELM() {
  myELM327.elm_port = &ELM_PORT;
  myELM327.PAYLOAD_LEN = 40;
  myELM327.debugMode = ELM_DEBUG;
  myELM327.timeout_ms = 1000;

  myELM327.payload = (char *)malloc(myELM327.PAYLOAD_LEN + 1); // allow for terminating '\0'

  myELM327.sendCommand_Blocking(ECHO_OFF);
  delay(100);
  
	myELM327.sendCommand_Blocking(ALLOW_LONG_MESSAGES);
	delay(100);

  // myELM327.sendCommand_Blocking(PRINTING_SPACES_OFF);
  // delay(100);

  if (myELM327.sendCommand_Blocking(PRINTING_SPACES_OFF) == ELM_SUCCESS) {
      if (strstr(myELM327.payload, "OK") != NULL) {
          // Protocol search can take a comparatively long time. Temporarily set
          // the timeout value to 30 seconds, then restore the previous value.
          uint16_t prevTimeout = myELM327.timeout_ms;
          myELM327.timeout_ms = 30000;

          delay(100);
          if (myELM327.sendCommand_Blocking("0100") == ELM_SUCCESS) {
              myELM327.timeout_ms = prevTimeout;
              return true;
          }
      }
  }
  return false;
}

float getRPM() {
  while (true) {
    float tempRPM = myELM327.rpm();

    if (myELM327.nb_rx_state == ELM_SUCCESS) {
      return tempRPM;
    }
    else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
      DEBUG_PRINTLN("Failed getting RPM");
      return 0.0f;
    }
  }
}

uint16_t getEngineTime() {
  while (true) {
    uint16_t engineTime = myELM327.timeSinceCodesCleared();

    if (myELM327.nb_rx_state == ELM_SUCCESS) {
      return engineTime;
    }
    else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
      DEBUG_PRINTLN("Failed getting Engine TIme");
      return 0;
    }
  }
}



void setup() {
  DEBUG_BEGIN(115200);

#ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
#endif

  // RTC Clock
  // if (!rtc.begin()) {
  //   DEBUG_PRINTLN("Couldn't connect to the RTC clock");
  //   blink_led_restart(1000);
  // }
  
  // if (!rtc.isrunning()) {
  //   // When time needs to be set on a new device, or after a power loss, the
  //   // following line sets the RTC to the date & time this sketch was compiled
  //   rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  //   DEBUG_PRINTLN("RTC is NOT running, setting the time now to compile time");
  // }

  // setupTime = rtc.now();

  // RTC Clock
  rtc.Begin();
  
  if (!rtc.IsDateTimeValid()) {
      // Common Causes:
      //    1) first time you ran and the device wasn't running yet
      //    2) the battery on the device is low or even missing

      DEBUG_PRINTLN("RTC lost confidence in the DateTime!");
      rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
  }

  if (rtc.GetIsWriteProtected()) {
      DEBUG_PRINTLN("RTC was write protected, enabling writing now");
      rtc.SetIsWriteProtected(false);
  }

  if (!rtc.GetIsRunning()) {
      DEBUG_PRINTLN("RTC was not actively running, starting now");
      rtc.SetIsRunning(true);
  }

  setupTime = rtc.GetDateTime();


  // ELM327
  DEBUG_PRINTLN("Connecting to ELM327 via bluetooth");
  ELM_PORT.begin("ArduHUD", true);
  
  if (!ELM_PORT.connect("OBDII")) {
    DEBUG_PRINTLN("Couldn't connect to OBD scanner - Phase 1");
    blink_led_restart(50);
  }

  if (!initELM()) {
    DEBUG_PRINTLN("Couldn't connect to OBD scanner - Phase 2");
    blink_led_restart(100);
  }
  DEBUG_PRINTLN("Connected to ELM327");


  // SD Card
  DEBUG_PRINT("Initializing SD card...");
  if (!SD.begin(CS_PIN)) {
    DEBUG_PRINTLN(" initialization failed");
    blink_led_restart(1000);
  }
  DEBUG_PRINTLN(" initialization successful");

  // Files
  if (!openEventFile()) {
    DEBUG_PRINTLN("Error opening the event file");
    blink_led_restart(1500);
  }

  if (!openNewLogFile()) {
    DEBUG_PRINTLN("Error opening the log file");
    blink_led_restart(1500);
  }
}


void loop() {
  long currMillis = millis();
  if (currMillis - lastDataReadTime > DATA_READ_INTERVAL) {
    lastDataReadTime = currMillis;

    updateTime(currMillis);

    float rpm = getRPM();

    logDataLine(rpm);
    
    if (rpm > RPM_THRESHOLD) {
      logEvent(rpm);
    }
  } else {
    delay(10);
  }
}
