/*
	Name:       EnergyMiser.ino
	Created:  08/12/2020 11:09:11
	Author:     DAVID-HP\David
*/

/*Versions
  01 - As supplied by David
  02 - General mods too many to list
  03 -
  DC04 - enter watts from web pge
  DC05 - shows time to go on web pge
  DC06 - puts system into pre-load state or finished when button pressed
  AH07 - 900 samples
  DC08 - display fixes
  AH09 - removed testing startdelay=1, changed some beeps for testing and removed old change remarks
  DC10 - reset off timer every time it goes above threshold
  DC11 - rename aveDelta to loadNow and show on web page
  AH12 - changed values is getWatts to imporve conversions
  DC13 - amended BELOW threshold message
  DC14 - adds average of averages
  DC15 - more monitoring and on after 2 secs
  AH16 - Removed lots of test beeps, added beep when STATE_WAITING_OFF & changed threshold back to * 1.1

*/

#define david
//#define noBeep
//#define serialDebug
#define PCB_VERSION

#include <Metro.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include "FS.h"
#include <EEPROM.h>
#include <Button.h>

#define SAMPLE_MS 5

Metro secTick = Metro(1000);
Metro tenthTick = Metro(100);
Metro sampleTick = Metro(5);

#define EEPROM_SIZE 20
#define EE_MINS_TO_OFF 1 // 1

#define EE_THRESHOLD 2
#define EE_STARTDELAY 3
#define EE_ENDDELAY 4

// for the display
#include "SoftwareSerial.h"

SoftwareSerial swSer1;
#define digoleSerial swSer1

#ifdef PCB_VERSION
#define TX (22) // 21
#else
#define TX (21) // 21
#endif

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

#ifdef PCB_VERSION
#define pinLED_Buzzer 26 // 12
#define pinButtonA 21 // 19
#define pinCurrent 35 // 36
#define pinRelay 18 // 33
#else
#define pinLED_Buzzer 12
#define pinButtonA 19
#define pinCurrent 36
#define pinRelay 33
#endif

Button btnA = Button(pinButtonA, true, true, 100);

boolean loadIsOn = false;
String loadState;
String webpage = "";

double threshold_default = 14, threshold, threshold_saved;
int startdelay_default = 5, enddelay_default = 1, startdelay, enddelay;

int secs, mins, minsToOff, secsToOff;

#define NO_OF_SAMPLES 900

char strMsg[300], strVal[50];
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
int loadSamples[LOAD_SAMPLES], loadSampleIndex, sampleValNow, onSecs = 1 + (SAMPLE_MS * NO_OF_SAMPLES/1000); // LOAD_SAMPLES;

float agg, aggCount;
int loadNow, aveCount;
boolean gotNoLoadCount = false; // DC14

int minsToStart, secsToStart;

int thresholdWattsOnWebPage; // DC04

char timeToGo[100];

void setup() {
  analogReadResolution(9);

  EEPROM.begin(EEPROM_SIZE);

  swSer1.begin(9600, SWSERIAL_8N1, TX, TX, false, 256);
  swSer1.enableTx(true);

  Serial.begin(115200);
  while (!Serial && millis() < 5000);

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS error");
    return; // all bets off
  }

  clearDigoleScreen();  setFont(fonts[4]);

  makeHTML();

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

    server.begin();
  }
  else {
    Serial.println("Not connected to WiFi");
  }

  minsToOff = EEPROM.read(EE_MINS_TO_OFF);
  if (minsToOff > 240 || minsToOff == 0) {
    minsToOff = 10;
    EEPROM.write(EE_MINS_TO_OFF, minsToOff); EEPROM.commit();
  }

  // APH Added
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

#ifdef david
  startdelay = enddelay = 1;
#endif

  setTextPosAbs(0, 20);
  gPrint("Monitoring no load val");
  // AH09 removed startdelay = 1; // was 2; for testing

  //for (int i = 0; i <= 5000; i += 50) {
  //    sprintf(strMsg, "watts = %d, count = %d", i, (int)getThreshold(i)); Serial.println(strMsg);
  //}
  beep(1, 1, 1);
}

void loop() {
  btnA.read();

  if (btnA.wasPressed()) {
    beep(1, 5, 1);
    loadIsOn = !loadIsOn;
    updateLoad();

    //DC06
    if (loadIsOn) {
      Serial.println("power on");

      minsToStart = startdelay; // re-load delay
      secsToStart = 0;
      systemState = STATE_WAITING_FOR_LOAD;
      sprintf(timeToGo, "Monitoring OFF in %02d:%02d     ", minsToStart, secsToStart);
    }
    else {
        Serial.println("power off"); // DC15

      systemState = STATE_FINISHED;
      // do something with the screen
    }

  }

  if (secTick.check()) {
    secCount += 1;

    loadSamples[loadSampleIndex] = sampleValNow;

    loadSampleIndex += 1;
    if (loadSampleIndex == LOAD_SAMPLES) {
        loadSampleIndex = 0;
    }

    float sum = 0.0;
    for (int i = 0; i < LOAD_SAMPLES; i++) {
        sum += (float)loadSamples[i];
        loadNow = (int)(sum / LOAD_SAMPLES);
    }

    if (onSecs) { // DC14
        onSecs -= 1;
        setTextPosAbs(0, 50); delay(20);
        gPrint(onSecs); delay(20); gPrint("  "); delay(20);
    }

    if (onSecs == 0 && !gotNoLoadCount) { // DC14, DC15
        gotNoLoadCount = true;

        clearDigoleScreen(); setFont(fonts[4]); delay(20);
        noLoadValue = aveCount; // DC15

        systemState = STATE_WAITING_FOR_LOAD;

        sprintf(strMsg, "No load value = %d", noLoadValue); Serial.println(strMsg); // DC14

        loadIsOn = true;
        updateLoad();

        setTextPosAbs(0, line3); delay(20);
        setColor(YELLOW); delay(20);
        gPrint("Waiting for load"); delay(20);

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
            clearDigoleScreen();  delay(20);  setFont(fonts[4]); delay(20);
            updateLoad();
            setTextPosAbs(0, line3); delay(20);
            setColor(YELLOW); delay(20);
            sprintf(timeToGo, "Monitoring load"); gPrint(timeToGo); Serial.println(timeToGo); delay(20); // DC05
          }
        }

        if (systemState == STATE_WAITING_FOR_LOAD) { // still waiting
          setTextPosAbs(0, line3); delay(20);
          setColor(WHITE); delay(20);
          sprintf(timeToGo, "Monitoring OFF in %02d:%02d     ", minsToStart, secsToStart); gPrint(timeToGo); delay(20); //Serial.println(strMsg); // DC05
        }
      }
      else if (systemState == STATE_MONITORING_LOAD) {
        if (loadNow < (int)threshold + 5) {
          // beep(2, 3, 1); AH15
          systemState = STATE_WAITING_OFF;
          minsToOff = enddelay;
          secsToOff = 0;

          clearDigoleScreen(); delay(20); setFont(fonts[4]); delay(20);
          updateLoad();

          setTextPosAbs(0, line4); delay(20);
          setColor(RED); delay(20);
          sprintf(strMsg, "BELOW Threshold, off in %d mins", minsToOff); gPrint(strMsg); Serial.println(strMsg); // DC13
        }
      }
      else if (systemState == STATE_WAITING_OFF) {
        secsToOff -= 1;

        if (secsToOff < 0) { // AH15
          secsToOff = 59;
          minsToOff -= 1;

          if (minsToOff == 1 && secsToOff == 0) { // Beep ONLY when 1 minute to go & shutdown AH15
              beep(4, 5, 1);
          }

          if (minsToOff < 0) {
            loadIsOn = false;
          
            systemState = STATE_FINISHED;

            updateLoad();

            clearDigoleScreen(); delay(20); setFont(fonts[4]); delay(20);
            setTextPosAbs(0, line3);
            setColor(ORANGE);
            gPrint("Finished!"); 
            Serial.println("Finished!");
          }
        }

        if (systemState == STATE_WAITING_OFF) {
          setTextPosAbs(0, line3); delay(20);
          setColor(WHITE); delay(20);
          sprintf(timeToGo, "Power Off in %02d:%02d     ", minsToOff, secsToOff); gPrint(timeToGo); delay(20);// DC05
        }

        if (loadNow > (int)threshold + 5) { // ABOVE Threshold ?
            //   beep(1, 5, 1); AH09 Changed

              systemState = STATE_MONITORING_LOAD;
              minsToOff = enddelay; // DC10
              secsToOff = 0;

              clearDigoleScreen();  delay(20);  setFont(fonts[4]); delay(20);
              updateLoad();

              setTextPosAbs(0, line3); delay(20);
              setColor(YELLOW); delay(20);
              sprintf(strMsg, "Monitoring load"); gPrint(strMsg); delay(20); Serial.println(strMsg);
              setTextPosAbs(0, line4); delay(20);
              setColor(GREEN); delay(20);
              sprintf(strMsg, "ABOVE Threshold"); gPrint(strMsg); delay(20); Serial.println(strMsg);
            }
      }
    }

    setTextPosAbs(0, line1); delay(20);
    setColor(WHITE); delay(20); // DC08

    if (loadIsOn) {
      sprintf(strMsg, "load = %d,(%4.0fW)        ", loadNow, getWatts(loadNow)); gPrint(strMsg); delay(20); // DC08
      sprintf(strMsg, "[%d] load = %d, watts = %4.0f, aveCount = %d", secCount, loadNow, getWatts(loadNow), aveCount); 

#ifdef serialDebug
      Serial.println(strMsg); // for testing DC15
#endif
    }
  }

  if (sampleTick.check()) {

    int valNow = analogRead(pinCurrent);

    aggCount += (float)valNow;

    int delta = valNow - noLoadValue;
    if (delta < 0) delta *= -1;

    samples[sampleIndex] = delta;

    sampleIndex += 1;

    if (sampleIndex == NO_OF_SAMPLES) { // count used for noLoadValue
      sampleIndex = 0;
      aveCount = aggCount / NO_OF_SAMPLES;
      aggCount = 0;
    }

    agg = 0;
    for (int i = 0; i < NO_OF_SAMPLES; i++) agg += (float)samples[i];
    sampleValNow = (int)agg / NO_OF_SAMPLES; // DC14
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

}

void updateLoad() {
  setTextPosAbs(0, line2);
  delay(20);

  if (loadIsOn) {
    digitalWrite(pinRelay, HIGH);
    // beep(4, 5, 1); // AH15 removed as its annoying
    setColor(GREEN); gPrint("On ");
  }
  else {
    digitalWrite(pinRelay, LOW);
  //beep(6, 5, 1); AH15 removed as its annoying
    setColor(RED);  gPrint("Off");
  }
  delay(20);
} // end of loop()

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
