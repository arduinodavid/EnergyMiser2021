void setUpRoutes() {


  // core page requests
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/diags.html", String(), false, getVariable);
  });

  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, "/style.css", "text/css");
  });

  // settings
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send(SPIFFS, "/settings.html", String(), false, getVariable);
  });

  // chart
  server.on("/chart", HTTP_GET, [](AsyncWebServerRequest* request) {
      request->send(SPIFFS, "/chart.html", String(), false, getVariable);
      });

  // log
  server.on("/log", HTTP_GET, [](AsyncWebServerRequest* request) {
      makeHeader();

      fileHeader += "<h2>Log Files</h2>";
      fileHeader += "<p><table style = 'width:400px'>";
      fileHeader += "<tr><th>Name/Type</th><th>File Size</th></tr>";

      listFiles();
      request->send(200, "text/html", fileLine);
      });

  // download
  server.on("/download", HTTP_GET, [](AsyncWebServerRequest* request) { // 110
      //String inputMessage;
      AsyncWebParameter* p = request->getParam(0);
      request->send(SPIFFS, p->value().c_str(), "text/html", true);
      });

  // set called from diagnostics
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest * request) {
    String inputMessage;
    for (int i = 0; i < 50; i++) strVal[i] = 0;

    if (request->hasParam("startdelay")) {
        inputMessage = request->getParam("startdelay")->value();
        strcpy(strVal, inputMessage.c_str());
        startdelay = atoi(strVal);

        EEPROM.write(EE_STARTDELAY, startdelay); EEPROM.commit();
        sprintf(strMsg, "Start delay time (mins) %d", startdelay);

        request->redirect("/");
        //request->send(SPIFFS, "/settings.html", String(), false, getVariable);
    }

    if (request->hasParam("enddelay")) {
        inputMessage = request->getParam("enddelay")->value();
        strcpy(strVal, inputMessage.c_str());
        enddelay = atoi(strVal);

        EEPROM.write(EE_ENDDELAY, enddelay); EEPROM.commit();
        sprintf(strMsg, "End delay time (mins) %d", enddelay);

        request->redirect("/");
        //request->send(SPIFFS, "/settings.html", String(), false, getVariable);
    }

    if (request->hasParam("a2dresolution")) {
        inputMessage = request->getParam("a2dresolution")->value();
        strcpy(strVal, inputMessage.c_str());
        a2dres = (uint8_t)atoi(strVal);

        EEPROM.write(EE_A2DRES, a2dres); EEPROM.commit();
        sprintf(strMsg, "A2D resolution set to %d", a2dres); Serial.println(strMsg);

        request->send(SPIFFS, "/settings.html", String(), false, getVariable);
    }

    if (request->hasParam("loadoffset")) {
        inputMessage = request->getParam("loadoffset")->value();
        strcpy(strVal, inputMessage.c_str());
        loadOffset = atoi(strVal);

        EEPROM.write(EE_LOADOFFSET, (uint8_t) loadOffset); EEPROM.commit();
        sprintf(strMsg, "load offset set to %d", loadOffset); Serial.println(strMsg);

        request->send(SPIFFS, "/settings.html", String(), false, getVariable);
    }

    if (request->hasParam("hysteresis")) {
        inputMessage = request->getParam("hysteresis")->value();
        strcpy(strVal, inputMessage.c_str());
        hysteresis = (uint8_t)atoi(strVal);

        EEPROM.write(EE_HYSTERESIS, hysteresis); EEPROM.commit();
        sprintf(strMsg, "hysteresis set to %d", hysteresis); Serial.println(strMsg);

        request->send(SPIFFS, "/settings.html", String(), false, getVariable);
    }

    if (request->hasParam("email")) { // 112
        inputMessage = request->getParam("email")->value();
        strcpy(emailTo, inputMessage.c_str());
        a2dres = (uint8_t)atoi(strVal);

        int eeLoc = EE_EMAIL;
        for (int i = 0; i < 50; i++) {
            EEPROM.write(eeLoc, emailTo[i]);
            eeLoc += 1;
        }
        EEPROM.commit();

        // send a test
        send_eMail("Test message", "Energy Miser", String(emailTo));

        request->send(SPIFFS, "/settings.html", String(), false, getVariable);
    }

    if (request->hasParam("threshold")) {
      inputMessage = request->getParam("threshold")->value(); // DC04
      strcpy(strVal, inputMessage.c_str());

      int thresholdFromWebPage = atoi(strVal);

      if (loadIsOn) {
          if (thresholdFromWebPage == thresholdOnWebPage) { // Auto threshold wanted because value is unchanged 
              threshold = (double)loadNow;
              sprintf(strMsg, "AUTO threshold set to %d", (byte)threshold);
          }
          else {
              threshold = thresholdFromWebPage;
              sprintf(strMsg, "Threshold set to %d", (byte)threshold);
          }

        EEPROM.write(EE_THRESHOLD, (byte)threshold); EEPROM.commit();
      }
      else {
        sprintf(strMsg, "Threshold not set, load off", (byte)threshold);
      }

      sendDelayedMessage(strMsg);

      request->redirect("/");
    }
  });


  // handle on off buttons
  server.on("/poweron", HTTP_GET, [](AsyncWebServerRequest * request) {
    loadIsOn = true;
    updateLoad();

    minsToStart = startdelay; // re-load delay
    secsToStart = 0;
    systemState = STATE_WAITING_FOR_LOAD; //DC06

    sendDelayedMessage("ON from web page");

    request->redirect("/");
  });

  server.on("/poweroff", HTTP_GET, [](AsyncWebServerRequest * request) {
    loadIsOn = false;
    updateLoad();

    systemState = STATE_FINISHED; // DC06

    sendDelayedMessage("OFF from web page");

    request->redirect("/");
  });


  server.on("/setdefaults", HTTP_GET, [](AsyncWebServerRequest * request) {

    threshold = threshold_default;
    sprintf(strMsg, "threshold set to %d", (byte)threshold);
    sendDelayedMessage(strMsg);

    EEPROM.write(EE_THRESHOLD, (byte)threshold); EEPROM.commit();
    startdelay = startdelay_default;

    EEPROM.write(EE_STARTDELAY, (byte)startdelay); EEPROM.commit();
    enddelay = enddelay_default;

    EEPROM.write(EE_ENDDELAY, (byte)enddelay); EEPROM.commit();
    request->redirect("/");
  });

  // events
  events.onConnect([](AsyncEventSourceClient* client) {
      if (client->lastId()) {
          Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
      }
      // send event with message "hello!", id current millis
      // and set reconnect delay to 10 seconds
      client->send("hello!", NULL, millis(), 10000);
      });

   // special upload page
  server.on("/upload", HTTP_GET, [](AsyncWebServerRequest * request) {
      makeHeader();
      fileHeader += "<h2>Upload</h2>" + ptr;

    request->send(200, "text/html", fileHeader);
  });

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest * request) {
    request->send(200);
  },  [](AsyncWebServerRequest * request, const String & filename, size_t index, uint8_t* data,
     size_t len, bool final) {
    handleUpload(request, filename, index, data, len, final);
  });

  server.addHandler(&events);

  server.begin();
}

// callback for web server - returns astring value for a given variable in the html (eg %state%)
String getVariable(const String & var) {

  if (var == "onoff") {
    if (loadIsOn) loadState = "ON"; //
    else loadState = "OFF";
    return loadState;
  }

  else if (var == "threshold") {
      thresholdOnWebPage = (int)threshold;
      return String(thresholdOnWebPage).c_str();
  }

  else if (var == "version") {
      return String(version).c_str();
  }

  else if (var == "a2dresolution") {
      return String(a2dres).c_str();
  }

  else if (var == "loadoffset") {
      return String(loadOffset).c_str();
  }


  else if (var == "hysteresis") {
      return String(hysteresis).c_str();
  }

  else if (var == "startdelay") {
      return String(startdelay).c_str();
  }

  else if (var == "enddelay") {
      return String(enddelay).c_str();
  }

  else if (var == "log") {
      return strLog;
  }

  else if (var == "email") { // 112
      return String(emailTo).c_str();
  }

  return String();
}


void handleUpload(AsyncWebServerRequest * request, String filename, size_t index, uint8_t* data, size_t len, bool final) {

  if (!index) {
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
  //ptr = "<!DOCTYPE html> <html>\n";
  //ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  //ptr += "<title>Uploader</title>\n";
  //ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  //ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  //ptr += ".button {display: block;width: 200px;background-color: #3498db;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  //ptr += ".button-on {background-color: #3498db;}\n";
  //ptr += ".button-on:active {background-color: #2980b9;}\n";
  //ptr += ".button-off {background-color: #34495e;}\n";
  //ptr += ".button-off:active {background-color: #2c3e50;}\n";
  //ptr += "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  //ptr += "</style>\n";
  //ptr += "</head>\n";
  //ptr += "<body>\n";
  //ptr += "<h1>Uploader</h1>\n";

  ptr += "<form method=\"POST\" enctype=\"multipart/form-data\" action  = \"/upload\">";
  ptr += "   <input type=\"file\" name=\"filename\" action=\"/upload\">";
  ptr += "   <input class=\"button\" type=\"submit\" value=\"Upload\">";
  ptr += "</form>";

  ptr += "</body>\n";
  ptr += "</html>\n";
}

void sendMessage(String msg) { // 107
    sprintf(strMsg, "[%02d:%02d:%02d] %s", hh, mm, ss, msg.c_str());
    events.send(strMsg, "msg", millis());

    // print it
    Serial.println(msg);

    // log it
    File lfile = SPIFFS.open(logFileName, "a");

    if (lfile) {
        lfile.println(strMsg);
        lfile.close();
    }
}

void sendState(String msg) { // 107
    events.send(msg.c_str(), "state", millis());
    
    // print it
    Serial.println(msg.c_str());

    // log it
    File lfile = SPIFFS.open(logFileName, "a");

    if (lfile) {
        lfile.println(msg.c_str());
        lfile.close();
    }
}

void sendMachineState() { // 107
    events.send(machineState, "machineState", millis());

    // print it
    Serial.println(machineState);

    // log it
    File lfile = SPIFFS.open(logFileName, "a");

    if (lfile) {
        lfile.println(machineState);
        lfile.close();
    }
}

void sendLoad() {
    sprintf(strVal, "%d", loadNow);
    events.send(strVal, "loadNow", millis());
}

void sendDelayedMessage(char * msg) {
    delayedMessageToSend = true;
    delayedMessage.reset();
    strcpy(delayedMsg, msg);
}

void listFiles() {
    File root = SPIFFS.open("/");

    File file = root.openNextFile();

    fileLine = fileHeader;

    while (file) {
        if (strstr(file.name(), "-")) {
            fileLine += "<tr><td><a href='/download?file=" + String(file.name()) + "'>" + String(file.name()) + "</a></td>";
            fileLine += "<td>" + String(file.size()) + "</td></tr>";
        }

        file = root.openNextFile();
    }

    fileLine += "</table></p></body></html>";
}

void makeHeader() {
    fileHeader = "<!DOCTYPE HTML><html><head><title>Energy Miser</title><meta name = 'viewport' content = 'width=device-width, initial-scale=1'>";
    fileHeader += "<link rel = 'stylesheet' type = 'text/css' href = 'style.css'>";
    fileHeader += "</head><body><div class = 'topnav'><h1>Energy Miser</h1></div>";
    fileHeader += "<ul><li><a href = '/'>Home</a></li><li><a href = '/chart'>Chart</a></li><li><a href = '/settings'>Settings</a></li><li><a href = '/log'>Logs</a></li><li><a href = '/upload'>Uploader</a></li></ul>";
}

