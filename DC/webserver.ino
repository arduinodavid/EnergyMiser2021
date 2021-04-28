void setUpRoutes() {

  /*
     sprintf(strMsg, "Data %d", (byte)Data);   Serial.println(strMsg); // Print message to LCD
  */

  // Handle Web Server Events DC17
    events.onConnect([](AsyncEventSourceClient* client) {
        if (client->lastId()) {
            Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
        }
        // send event with message "hello!", id current millis
        // and set reconnect delay to 1 second
        client->send("hello!", NULL, millis(), 10000);
        });
    server.addHandler(&events);

  // core page requests
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/EMindex.html", String(), false, getVariable);
  });

  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });

  // set
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest * request) {
    String inputMessage;
    for (int i = 0; i < 50; i++) strVal[i] = 0;

    if (request->hasParam("startdelay")) {
      inputMessage = request->getParam("startdelay")->value();
      strcpy(strVal, inputMessage.c_str());
      startdelay = atoi(strVal);

      EEPROM.write(EE_STARTDELAY, startdelay); EEPROM.commit();
      sprintf(strMsg, "Start delay time (mins) %d", startdelay); Serial.println(strMsg);

      request->redirect("/");
    }
    if (request->hasParam("enddelay")) {
      inputMessage = request->getParam("enddelay")->value();
      strcpy(strVal, inputMessage.c_str());
      enddelay = atoi(strVal);


      EEPROM.write(EE_ENDDELAY, enddelay); EEPROM.commit();
      sprintf(strMsg, "End delay time (mins) %d", enddelay); Serial.println(strMsg);

      request->redirect("/");
    }

    if (request->hasParam("threshold")) {
      inputMessage = request->getParam("threshold")->value(); // DC04
      strcpy(strVal, inputMessage.c_str());
      int thresholdFromWebPage = atoi(strVal);
      sprintf(strMsg, "sent = %d, received = %d", thresholdWattsOnWebPage, thresholdFromWebPage); Serial.println(strMsg);

      if (loadIsOn) {
        if (thresholdFromWebPage == thresholdWattsOnWebPage) { // Auto threshold wanted because value is unchanged // DC04
          threshold = (double)loadNow * 1.1; // Increase load by 10% was 20% // AH15
          sprintf(strMsg, "AUTO threshold set to %d", (byte)threshold); Serial.println(strMsg);
        }
        else {
          threshold = getThreshold(thresholdFromWebPage);
          sprintf(strMsg, "Value from web page = %d", (byte)threshold); Serial.println(strMsg);
        }

        EEPROM.write(EE_THRESHOLD, (byte)threshold); EEPROM.commit();
      }
      else {
        beep(1, 15, 1);
        sprintf(strMsg, "Threshold not set, load off", (byte)threshold);   Serial.println(strMsg);
      }

      request->redirect("/");
    }
  });


  // handle on off buttons
  server.on("/poweron", HTTP_GET, [](AsyncWebServerRequest * request) {
    loadIsOn = true;
    updateLoad();
    Serial.println("power on");

    minsToStart = startdelay; // re-load delay
    secsToStart = 0;
    systemState = STATE_WAITING_FOR_LOAD; //DC06
    strcpy(msg1, "Monitoring OFF in"); // DC20
    sprintf(timeToGo, " %02d:%02d", minsToStart, secsToStart); // DC20
    strcpy(msg2, ""); // DC20
    //sprintf(timeToGo, "Monitoring OFF in %02d:%02d     ", minsToStart, secsToStart);

    request->redirect("/");
  });

  server.on("/poweroff", HTTP_GET, [](AsyncWebServerRequest * request) {
    loadIsOn = false;
    updateLoad();

    Serial.println("power off");

    systemState = STATE_FINISHED; // DC06
    // do something with the screen

    request->redirect("/");
  });


  server.on("/setdefaults", HTTP_GET, [](AsyncWebServerRequest * request) {

    threshold = threshold_default;
    sprintf(strMsg, "threshold is %d", (byte)getWatts(threshold));   Serial.println(strMsg);

    EEPROM.write(EE_THRESHOLD, (byte)threshold); EEPROM.commit();
    startdelay = startdelay_default;

    EEPROM.write(EE_STARTDELAY, (byte)startdelay); EEPROM.commit();
    enddelay = enddelay_default;

    EEPROM.write(EE_ENDDELAY, (byte)enddelay); EEPROM.commit();
    request->redirect("/");
  });

  // send all values
  server.on("/getvalues", HTTP_GET, [](AsyncWebServerRequest * request) {

      if (loadNow > (int)threshold) { // DC14
          // on web page load, state, currentStatus,msg1,timeleft,msg2,loadNow
          sprintf(strMsg, "%d watts,%s,Load above Threshold, , , ,%d", (int)getWatts(loadNow), labels[loadIsOn].c_str(), loadNow); // DC11
      }
      else {
          sprintf(strMsg, "%d watts,%s,,%s,%s,%s,%d", (int)getWatts(loadNow), labels[loadIsOn].c_str(), msg1, timeToGo, msg2, loadNow); // DC05, DC11
      }

    //if (loadIsOn) {

    //  if (loadNow > (int)threshold) {
    //    sprintf(strMsg, "%d watts,%s,Load above Threshold, ,%d", (int)getWatts(loadNow), labels[loadIsOn].c_str(), loadNow); // DC11
    //  }
    //  else {
    //    sprintf(strMsg, "%d watts,%s, ,%s,%d", (int)getWatts(loadNow), labels[loadIsOn].c_str(), timeToGo, loadNow); // DC05, DC11
    //  }
    //}
    //else {
    //  sprintf(strMsg, "Load is Off,%s, ,", labels[loadIsOn].c_str());
    //}
    request->send(200, "text/plain", strMsg);
  });

  // special upload page
  server.on("/upload", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/html", ptr);
  });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest * request) {
    request->send(200);
  },
  [](AsyncWebServerRequest * request, const String & filename, size_t index, uint8_t* data,
     size_t len, bool final) {
    handleUpload(request, filename, index, data, len, final);
  });
}

// callback for web server - returns astring value for a given variable in the html (eg %state%)
String getVariable(const String& var) {

  if (var == "state") {
    if (loadIsOn) loadState = "ON"; //
    else loadState = "OFF";
    return loadState;
  }

  else if (var == "load") {
    return String(getWatts(loadNow)).c_str();
  }

  else if (var == "currentthreshold") {
    thresholdWattsOnWebPage = (int)getWatts(threshold); // DC04
    return String(thresholdWattsOnWebPage).c_str();
  }

  else if (var == "startdelay") {
    return String(startdelay).c_str();
  }

  else if (var == "enddelay") {
    return String(enddelay).c_str();
  }

  else if (var == "timer") { // DC05
    return timeToGo;
  }

  return String();
}


void handleUpload(AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {

  if (!index) {
    //Serial.println((String)"UploadStart: " + filename);

    if (!filename.startsWith("/")) filename = "/" + filename;

    Serial.print("Uploading : "); Serial.println(filename);

    fsUploadFile = SPIFFS.open(filename, "w");
  }

  if (len) {
    if (fsUploadFile) fsUploadFile.write(data, len);
  }

  if (final) {
    if (fsUploadFile) {
      fsUploadFile.close();
      Serial.println((String)"Uploaded: " + filename + ", " + index + ", " + len);
      request->_tempFile.close();
      request->send(200, "text/plain", "File Uploaded !");
    }
    else {
      request->send(200, "text/plain", "500: couldn't create file");
    }
  }
}

void makeHTML() {
  ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>Uploader</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr += ".button {display: block;width: 200px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr += ".button-on {background-color: #3498db;}\n";
  ptr += ".button-on:active {background-color: #2980b9;}\n";
  ptr += ".button-off {background-color: #34495e;}\n";
  ptr += ".button-off:active {background-color: #2c3e50;}\n";
  ptr += "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1>Uploader</h1>\n";

  ptr += "<form method=\"POST\" enctype=\"multipart/form-data\" action  = \"/upload\">";
  ptr += "   <input type=\"file\" name=\"filename\" action=\"/upload\">";
  ptr += "   <input class=\"button\" type=\"submit\" value=\"Upload\">";
  ptr += "</form>";

  ptr += "</body>\n";
  ptr += "</html>\n";
}

void sendMessage(String msg) { // DC15
    events.send(msg.c_str(), "msg", millis());
}
