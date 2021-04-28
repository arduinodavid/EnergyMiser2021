/*
	Name:       EnergyMiser.ino
	Created:  08/12/2020 11:09:11
	Author:     DAVID-HP\David
*/

/*Versions
    100 back to basics, all output on web page
    AH101 removed clearDigoleScreen();  setFont(fonts[4]);
    102 - tweak to delayed message.
    103 - adds settings page
    104 - adds charts
    105 - multiline chart
    106 - load offset
    107 - adds log file
    108 - firmware updater built in
    109 - prints a file
    110 - lists log files
    111 - beeps at 1 to go
    112 - adds email
*/

int version = 112;

//#define david

#include <Metro.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "Update.h"
#include <SPIFFS.h>
#include "FS.h"
#include <EEPROM.h>
#include <Button.h>
#include <SD.h>
#include <SPI.h>
#include <ESP_Mail_Client.h>

#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "energymiser112@googlemail.com"
#define AUTHOR_PASSWORD "EnergyMiser112!"
SMTPSession smtp;
void smtpCallback(SMTP_Status status);

#define SAMPLE_MS 5

Metro secTick = Metro(1000);
Metro tenthTick = Metro(100);
Metro sampleTick = Metro(5);
Metro delayedMessage = Metro(1000);

#define EEPROM_SIZE 100
#define EE_MINS_TO_OFF 1 // 1

#define EE_THRESHOLD 2
#define EE_STARTDELAY 3
#define EE_ENDDELAY 4
#define EE_HYSTERESIS 5
#define EE_A2DRES 6
#define EE_LOADOFFSET 7
#define EE_LOG_ID 8
#define EE_EMAIL 10

// for the display
#include "SoftwareSerial.h"

SoftwareSerial swSer1;
#define digoleSerial swSer1

#define TX (22) // 21

#ifdef david
const char* ssid = "WiFi_I_Am";  // Enter SSID David
const char* password = "malts-tap-CLAD";  //Enter Password David
#else
const char* ssid = "WiFi-is-Great";  // Enter SSID Andrew
const char* password = "milana99";  // Enter Password Andrew
#endif

String labels[] = { "OFF", "ON" };

AsyncWebServer server(80);
AsyncEventSource events("/events");
File fsUploadFile;

// colors
#define RED 0xe0
#define ORANGE 0xf0
#define GREEN 0x10
#define YELLOW 0xfc
#define WHITE 0xff
#define BLACK 0x00
#define BLUE 0x06
#define GREY 0x92
#define TURQUOISE 0x1f

#define line1 50
#define line2 100
#define line3 150
#define line4 200

const unsigned char fonts[] = { 6, 10, 18, 51, 120, 123 };

#define pinLED_Buzzer 26 // 12
#define pinButtonA 21 // 19
#define pinCurrent 35 // 36
#define pinRelay 18 // 33

Button btnA = Button(pinButtonA, true, true, 100);

boolean loadIsOn = false;
String loadState;
String webpage = "", fileLine, fileHeader;

double threshold_default = 14, threshold, threshold_saved;
int startdelay_default = 5, enddelay_default = 1, startdelay, enddelay;

int secs, mins, minsToOff, secsToOff;

#define NO_OF_SAMPLES 900

char strMsg[300], strVal[50], delayedMsg[50];
String ptr;

// for beeping
int onTime, onCount, offCount, offTime, beepCount;

byte systemState = 0;
#define STATE_STARTING 0;
#define STATE_WAITING_FOR_LOAD 1
#define STATE_ABOVE_THRESHOLD 2
#define STATE_WAITING_OFF 3
#define STATE_MONITORING_LOAD 4
#define STATE_FINISHED 5

// sampling
int sampleIndex, samples[NO_OF_SAMPLES];
int noLoadValue = 0;
float mVperAmp = 185.0;
ulong secCount;

// loadCount averaging DC14
#define LOAD_SAMPLES 30
int loadSamples[LOAD_SAMPLES], loadSampleIndex, sampleValNow, onSecs = 10; // LOAD_SAMPLES;

float agg, aggCount, initialsamples;
int loadNow, aveCount;
boolean gotNoLoadCount = false; // DC14

int minsToStart, secsToStart, ss, mm, hh;

int thresholdWattsOnWebPage, thresholdOnWebPage; // DC04

char machineState[100];

uint8_t a2dres = 9, hysteresis = 1;

boolean delayedMessageToSend;

int loadOffset;

char logFileName[20]; // 107
uint8_t fileNo;;

String strLog;
boolean printingFile = false;

char emailTo[50];

char strTime[10];

void setup() {
 
  EEPROM.begin(EEPROM_SIZE);

  swSer1.begin(9600, SWSERIAL_8N1, TX, TX, false, 256);
  swSer1.enableTx(true);

  Serial.begin(115200);
  while (!Serial && millis() < 5000);

  a2dres = EEPROM.read(EE_A2DRES);

  if (a2dres < 9 || a2dres > 12) {
      EEPROM.write(EE_A2DRES, 9); EEPROM.commit();
      a2dres = 9;
  }

  sprintf(strMsg, "A2D resolution = %d", a2dres);  Serial.println(strMsg);
  analogReadResolution(a2dres);

  hysteresis = EEPROM.read(EE_HYSTERESIS);

  if (hysteresis > 5) {
      EEPROM.write(EE_HYSTERESIS, 1); EEPROM.commit();
      hysteresis = 1;
  }
  sprintf(strMsg, "hysteresis = %d", hysteresis);  Serial.println(strMsg);

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS error");
    return; // all bets off
  }
   
  makeHTML();
  makeHeader();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println("Connecting to "); Serial.print(ssid);

  //Wait for WiFi to connect
  int tries = 5;
  while (WiFi.waitForConnectResult() != WL_CONNECTED && tries) {
    Serial.print(".");
    tries -= 1;
    delay(1000);
  }

  if (WiFi.waitForConnectResult() != WL_CONNECTED) esp_restart();

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected to "); Serial.println(ssid);
    Serial.print("IP address: ");  Serial.println(WiFi.localIP());  //IP address assigned

    setUpRoutes();
  }
  else {
    Serial.println("Not connected to WiFi");
  }

  minsToOff = EEPROM.read(EE_MINS_TO_OFF);
  if (minsToOff > 240 || minsToOff == 0) {
    minsToOff = 10;
    EEPROM.write(EE_MINS_TO_OFF, minsToOff); EEPROM.commit();
  }

  pinMode(pinLED_Buzzer, OUTPUT);
  digitalWrite(pinLED_Buzzer, LOW);

  pinMode(pinRelay, OUTPUT); // **
  digitalWrite(pinRelay, LOW);

  pinMode(pinButtonA, INPUT_PULLUP);
  pinMode(pinCurrent, INPUT);

  threshold = EEPROM.read(EE_THRESHOLD);
  if (threshold == 0 || threshold > 240) {
    threshold = threshold_default;
    EEPROM.write(EE_THRESHOLD, (byte)threshold); EEPROM.commit();
  }

  startdelay = EEPROM.read(EE_STARTDELAY);
  if (startdelay == 0 || startdelay > 240) {
    startdelay = startdelay_default;
    EEPROM.write(EE_STARTDELAY, (byte)startdelay); EEPROM.commit();
  }

  enddelay = EEPROM.read(EE_ENDDELAY);
  if (enddelay == 0 || enddelay > 240) {
    enddelay = enddelay_default;
    EEPROM.write(EE_ENDDELAY, (byte)enddelay); EEPROM.commit();
  }

  loadOffset = EEPROM.read(EE_LOADOFFSET);
  if (loadOffset > 20) {
      loadOffset = 0;
      EEPROM.write(EE_LOADOFFSET, 0); EEPROM.commit();
  }

  fileNo = EEPROM.read(EE_LOG_ID);
  //Serial.println(fileNo);
  if (fileNo >= 10) fileNo = 0;
  EEPROM.write(EE_LOG_ID, fileNo + 1); EEPROM.commit();

  sprintf(logFileName, "/log-%d.txt", fileNo); // 107

  if (SPIFFS.exists(String(logFileName))) SPIFFS.remove(String(logFileName));

#ifdef david
  startdelay = 1;
  enddelay = 1;
#endif

  int eeLoc = EE_EMAIL;
  for (int i = 0; i < 50; i++) {
      emailTo[i] =  EEPROM.read(eeLoc);
      eeLoc += 1;
  }

  emailTo[0] = 255;

  for (int i = 0; i < 50; i++) {
      if (emailTo[i] < 48 || emailTo[i] > 127) {
          eeLoc = EE_EMAIL;

          for (int j = 0; j < 50; j++) {
              emailTo[j] = 0;
              EEPROM.write(eeLoc, 0);
              eeLoc += 1;
          }
          EEPROM.commit();

          break;
      }
  }

  beep(1, 1, 1);

  sprintf(strMsg, "Started v%d", version); Serial.println(strMsg);

}

void loop() {
    btnA.read();

  if (btnA.wasPressed()) {
    beep(1, 5, 1);

    loadIsOn = !loadIsOn;

    //DC06
    if (loadIsOn) {
          minsToStart = startdelay; // re-load delay
          secsToStart = 0;
          sendMessage("ON from button");
          systemState = STATE_WAITING_FOR_LOAD;
    }
    else {
        sendMessage("OFF from button");
        strcpy(machineState, "manual turn off");
        sendMachineState();
        systemState = STATE_FINISHED;
    }

    updateLoad();

  }

  if (secTick.check()) {
      sendLoad();
      ss += 1;

      if (ss == 60) {
          ss = 0;
          mm += 1;

          if (mm == 60) {
              mm = 0;


              hh += 1;
          }
      }

      sprintf(strVal, "%d,%d,%d,%d", loadNow, sampleValNow, secCount, (int)threshold, hh, mm, ss);
      events.send(strVal, "count", millis());

      //sprintf(strTime, "%02d:%02d:%02d", hh, mm, ss);
      //events.send(strTime, "time", millis());

       secCount += 1;

    if (systemState) {

        //sprintf(strVal, "%d", loadNow);
        //sprintf(strVal, "%d,%d,%d,%d", loadNow, sampleValNow, secCount, (int)threshold);
        //events.send(strVal, "count", millis());

        loadSamples[loadSampleIndex] = sampleValNow;

        loadSampleIndex += 1;
        if (loadSampleIndex == LOAD_SAMPLES) {
            loadSampleIndex = 0;
        }

        float sum = 0.0;
        float scount = 0.0;
        for (int i = 0; i < LOAD_SAMPLES; i++) {
            sum += (float)loadSamples[i];
        }

        loadNow = (int)(sum / LOAD_SAMPLES);
        loadNow -= loadOffset;

        if (loadNow < 0) loadNow = 0;
    }

    if (onSecs) { // DC14
        onSecs -= 1;
    }

    if (onSecs == 0 && !gotNoLoadCount) { // DC14, DC15
        gotNoLoadCount = true;

        noLoadValue = (int) (aggCount / initialsamples);

        systemState = STATE_WAITING_FOR_LOAD;

        sprintf(strMsg, "No load value = %d", noLoadValue); sendMessage(strMsg); // DC14
       
        loadIsOn = true;

        updateLoad();
        //sendMessage("Waiting for load");

        minsToStart = startdelay;
        secsToStart = 0;
    }

    if (loadIsOn) {
      if (systemState == STATE_WAITING_FOR_LOAD) {
        secsToStart -= 1;

        if (secsToStart <= 0) {
          secsToStart = 59;
          minsToStart -= 1;

          if (minsToStart < 0) {
            systemState = STATE_MONITORING_LOAD;
            sendMessage("Monitoring load");
            strcpy(machineState, "monitoring load");
            sendMachineState();
          }
        }

        if (systemState == STATE_WAITING_FOR_LOAD) { // still waiting
          sprintf(machineState, "Starting load monitor in %02d:%02d     ", minsToStart, secsToStart);
          sendMachineState();
        }
      }
      else if (systemState == STATE_MONITORING_LOAD) {
        if (loadNow <= (int)threshold) {

          systemState = STATE_WAITING_OFF;
          minsToOff = enddelay;
          secsToOff = 0;

          sprintf(machineState, "%d is at or below threshold (%d), off in %02d:%02d", loadNow, (int)threshold, minsToOff, secsToOff);

          send_eMail(machineState, "Energy Miser", emailTo); // 112
          sendMessage(String(machineState));
          sendMachineState();
        }
      }
      else if (systemState == STATE_WAITING_OFF) {
        secsToOff -= 1;

        if (secsToOff < 0) { // AH15
          secsToOff = 59;
          minsToOff -= 1;

          if (minsToOff < 0) {
            loadIsOn = false;
          
            systemState = STATE_FINISHED;

            updateLoad();

            strcpy(machineState, "Cycle completed");
            sendMachineState();

            sendMessage("Finished!");
          }
        }

        if (systemState == STATE_WAITING_OFF) {
            if (minsToOff == 1 && secsToOff == 0) beep(4, 5, 1); // 111
            
            sprintf(machineState, "Power Off in %02d:%02d", minsToOff, secsToOff); sendMachineState();// DC05
        }

        if (loadNow >= (int)threshold + hysteresis) {

              systemState = STATE_MONITORING_LOAD;
              sprintf(strMsg, "load now %d (%d + %d) so monitoring again", loadNow, (int)threshold, hysteresis);
              sendMessage(strMsg);

              strcpy(machineState, "monitoring load");
              sendMachineState();

              minsToOff = enddelay;
              secsToOff = 0;
        }
      }
    }
  }

  if (sampleTick.check()) {

    int valNow = analogRead(pinCurrent);

    if (systemState) {
        int delta = valNow - noLoadValue;
        if (delta < 0) delta *= -1;

        samples[sampleIndex] = delta;

        sampleIndex += 1;

        if (sampleIndex == NO_OF_SAMPLES) { // count used for noLoadValue
            sampleIndex = 0;
        }

        agg = 0;
        for (int i = 0; i < NO_OF_SAMPLES; i++) agg += (float)samples[i];
        sampleValNow = (int)(agg / NO_OF_SAMPLES); // DC14
    }
    else {
        sampleValNow = 0;

        aggCount += (float)valNow;
        initialsamples += 1.0;
    }

  }

  if (tenthTick.check()) {
    if (onCount) {
      onCount -= 1;

      if (onCount == 0) {
        digitalWrite(pinLED_Buzzer, LOW);
        offCount = offTime + 1;
      }
    }

    if (offCount) {
      offCount -= 1;

      if (offCount == 0) {
        beepCount -= 1;

        if (beepCount) {
          digitalWrite(pinLED_Buzzer, HIGH);
          onCount = onTime;
        }
      }
    }
  }

  if (delayedMessage.check()) {

      if (printingFile) {
          File flog = SPIFFS.open("/log.txt", FILE_READ);

          char cLog[500]; for (int i = 0; i < 500; i++) cLog[i] = 0;

          if (flog) {
              while (flog.available()) {
                  if (flog.readBytesUntil('\n', cLog, 500)) {
                      events.send(cLog, "line", millis());
                      for (int i = 0; i < 500; i++) cLog[i] = 0;
                      delay(50);
                  };
              }
          }
          printingFile = false;
      }

      if (delayedMessageToSend) {
          delayedMessageToSend = false;
          sendMessage(delayedMsg);
      }

  }
}

void updateLoad() {

  if (loadIsOn) {
    digitalWrite(pinRelay, HIGH);
    sendState("power is ON");
    sendMessage("power turned ON");
  }
  else {
    digitalWrite(pinRelay, LOW);
    sendState("power is OFF");
    sendMessage("power turned OFF");
  }

} 

void beep(int count, int duration, int spacing) {
#ifdef noBeep
    return;
#endif

  onTime = duration;
  onCount = onTime;
  offTime = spacing;
  beepCount = count;
  tenthTick.reset();
  digitalWrite(pinLED_Buzzer, HIGH);
  tenthTick.reset();
}

float getWatts(int val) { // AH12
       if (!gotNoLoadCount) return 0; // DC14

      if (val < 5) return 2.0; // APH 10 for testing normal 0
      else if (val >= 5 && val < 9) return (float)(val * 1.20); // Measured
      else if (val >= 9 && val < 10) return (float)(val * 1.40); // 17w
      else if (val >= 10 && val < 12) return (float)(val * 1.60); // w
      else if (val >= 12 && val < 14) return (float)(val * 2.00); // w
      else if (val >= 14 && val < 16) return (float)(val * 2.75);
      else if (val >= 16 && val < 18) return (float)(val * 3.25);
      else if (val >= 18 && val < 20) return (float)(val * 4.25);
      else if (val >= 20 && val < 23) return (float)(val * 5.00);
      else if (val >= 23 && val < 26) return (float)(val * 5.25);
      else if (val >= 26 && val < 29) return (float)(val * 5.75);
      else if (val >= 29 && val < 40) return (float)(val * 6.10);
      else if (val >= 40 && val < 50) return (float)(val * 6.50);
      else if (val >= 50 && val < 70) return (float)(val * 7.70);
      else if (val >= 70 && val < 90) return  (float)(val * 8.50); // All below are Guesstimates
      else if (val >= 90 && val < 120) return  (float)(val * 9.00);
      else if (val >= 120 && val < 170) return  (float)(val * 9.70);
      else if (val >= 170 && val < 200) return (float)(val * 11.474); // Measured
      else if (val >= 200) return (float)(val * 12.00); // Guesstimates
}

int getThreshold(int watts) {
  for (int val = 0; val < 201; val++) {
    if (getWatts(val) > watts) return val;
  }
}


void listFilesOnSPIFFS() {
    File root = SPIFFS.open("/");

    File file = root.openNextFile();

    while (file) {
        Serial.println(file.name());
        file = root.openNextFile();
    }
}
