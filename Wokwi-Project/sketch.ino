// Log OBD information to an SD card

// Output file format:

// events.txt
  // Timestamp         | RPM
  // 2023-11-28 19:10:24 9999.48

// log01.txt
  //
  // 2023-11-28 19:10:24
  // 1701169824 sec Unix Time
  // 99999 mins since OBD cleared
  //
  // ms offset from unix time | RPM
  // 1234 943.45
  // 1284 949.74
  // 1324 340.22
  // 1374 9999.48 <--
  // 1434 642.43
  //
  // 2023-11-28 19:10:24
  // 1701169824 sec Unix Time
  // 99999 mins since OBD cleared
  //
  // ms offset from unix time | RPM
  // 1234 943.45
  // 1284 949.74
  // 1324 340.22
  // 1374 9999.48 <--
  // 1434 642.43

// log02.txt
  //
  // 2023-11-28 19:10:24
  // 99999 mins since OBD cleared
  //
  // Timestamp | RPM
  // 19:10:24.00 943.45
  // 19:10:24.50 949.74
  // 19:10:25.00 340.22
  // 19:10:25.50 9999.48 <--
  // 19:10:26.00 642.43
  //
  // 2023-11-29 14:10:24
  // 99999 mins since OBD cleared
  //
  // Timestamp | RPM
  // 14:10:24.00 943.45
  // 14:10:24.50 949.74
  // 14:10:25.00 340.22
  // 14:10:25.50 9999.48 <--
  // 14:10:26.00 642.43



////////    Config    ////////

//// Constants/Settings ////
#define DEBUG  // Print debug lines to serial, comment out to disable

#define EVENT_FILE_NAME "event.txt"  // Follow 8.3 file naming scheme
#define LOG_FILE_NAME "log.txt"      // Follow 8.3 file naming scheme
#define LOG_FILES_BASE_NAME "log"    // Max 6 characters
////////

//// Pins ////
// These pins can be changed if needed, but it's recommended to use them

// SD Card
#define CS_PIN  5
//      SCK_PIN 18
//      MISO_DO_PIN  19
//      MOSI_DI_PIN  23
////////

////////    End Config    ////////




#include <BluetoothSerial.h>
#include <ELMduino.h>
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

File logFile;

BluetoothSerial SerialBT;
#define ELM_PORT   SerialBT
ELM327 myELM327;

uint32_t rpm = 0;



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

bool openLogFile() {
  logFile = SD.open(LOG_FILE_NAME, FILE_WRITE);

  if (!logFile) {
    Serial.println("Error opening the log file");
    return false;
  }
  return true;
}

void flushLogFile() {
  logFile.close();
  openLogFile();
}


void setup() {
  DEBUG_BEGIN(115200);

#ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
#endif

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

  DEBUG_PRINT("Initializing SD card...");
  if (!SD.begin(CS_PIN)) {
    DEBUG_PRINTLN(" initialization failed");
    blink_led_loop(1000);
  }
  DEBUG_PRINTLN(" initialization successful");

  if (!openLogFile()) {
    blink_led_loop(2000);
  }
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
  for (int analogPin = 0; analogPin < 3; analogPin++) {
    int sensor = analogRead(analogPin);
    logFile.println(sensor);
  }

}
