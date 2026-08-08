/**
 * ============================================================
 *  Irrigatore Automatico ESP32 by Furios121
 *  Firmware v1.0.3
 * ============================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// BLE
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include "config.h"
#include "web_ui.h"

// ============================================================
//  COSTANTI BLE
// ============================================================
#define BLE_SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define BLE_CHAR_SSID_UUID      "12345678-1234-1234-1234-123456789ab1"
#define BLE_CHAR_PASS_UUID      "12345678-1234-1234-1234-123456789ab2"
#define BLE_CHAR_STATUS_UUID    "12345678-1234-1234-1234-123456789ab3"

// ============================================================
//  ENUM STATI
// ============================================================
enum DeviceState {
  STATE_BLE_WAIT,       // Attesa credenziali BLE
  STATE_WIFI_CONNECT,   // Connessione WiFi in corso
  STATE_RUNNING,        // Operativo
  STATE_OTA_UPDATE,     // Aggiornamento OTA in corso
  STATE_OTA_AVAILABLE,  // Aggiornamento disponibile (non usato in OTA manuale)
  STATE_OTA_FAILED,     // Aggiornamento fallito
  STATE_ERROR           // Errore generico
};

// ============================================================
//  VARIABILI GLOBALI - NON TOCCARE, SONO GESITI DAL FIRMWARE
// ============================================================
DeviceState deviceState = STATE_BLE_WAIT;
Preferences prefs;
WebServer   server(WEB_SERVER_PORT);

// BLE
BLEServer*         bleServer    = nullptr;
BLECharacteristic* charSSID     = nullptr;
BLECharacteristic* charPass     = nullptr;
BLECharacteristic* charStatus   = nullptr;
bool               bleConnected = false;
String             pendingSSID  = "";
String             pendingPass  = "";
bool               credReceived = false;

// Pompa
bool     pumpRunning     = false;
uint32_t pumpStopTime    = 0;

// LED
uint32_t lastBlinkTime   = 0;
uint8_t  blinkStep       = 0;     // step per sequenze multi-led
bool     ledState        = false;

// NTP / Orario
bool     ntpSynced       = false;

// Irrigazione programmata
bool     irrigEnabled    = false;
uint8_t  irrigHour       = 8;
uint8_t  irrigMinute     = 0;
uint16_t irrigDuration   = PUMP_DEFAULT_DURATION;
bool     irrigDoneToday  = false;
uint8_t  lastCheckedMin  = 255;

// Controllo aggiornamenti
String   updateVersion   = "";
String   updateNotes     = "";
String   updateBinUrl    = "";      // URL diretto al .bin su GitHub Releases
bool     updateAvailable = false;
uint32_t lastUpdateCheck = 0;

// ============================================================
//  FUNZIONI LED
// ============================================================

void allLedsOff() {
  digitalWrite(LED_RED,   LOW);
  digitalWrite(LED_Y1,    LOW);
  digitalWrite(LED_Y2,    LOW);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_BLUE,  LOW);
}

/** Chiama in loop() per gestire i LED in base allo stato */
void updateLeds() {
  uint32_t now = millis();

  switch (deviceState) {

    // --- WiFi non connesso: rosso e blu lampeggiano 3 volte di fila ---
    case STATE_BLE_WAIT: {
      // Sequenza: 3 lampeggi rosso+blu, pausa lunga, ripeti
      // Step 0-5: lampeggi (on/off x3), step 6: pausa
      static uint8_t seq = 0;
      static uint32_t last = 0;
      uint32_t interval = (seq < 6) ? BLINK_SLOW : 1200;
      if (now - last >= interval) {
        last = now;
        if (seq < 6) {
          bool on = (seq % 2 == 0);
          digitalWrite(LED_RED,  on ? HIGH : LOW);
          digitalWrite(LED_BLUE, on ? HIGH : LOW);
        } else {
          digitalWrite(LED_RED,  LOW);
          digitalWrite(LED_BLUE, LOW);
        }
        seq = (seq + 1) % 8; // 6 step blink + 2 step pausa
      }
      break;
    }

    // --- Configurazione WiFi attiva: lampeggio blu veloce ---
    case STATE_WIFI_CONNECT: {
      if (now - lastBlinkTime >= BLINK_FAST) {
        lastBlinkTime = now;
        ledState = !ledState;
        digitalWrite(LED_BLUE, ledState ? HIGH : LOW);
        digitalWrite(LED_RED,  LOW);
      }
      break;
    }

    // --- Operativo: verde fisso ---
    case STATE_RUNNING: {
      allLedsOff();
      digitalWrite(LED_GREEN, HIGH);
      break;
    }

    // --- Aggiornamento in corso
    case STATE_OTA_UPDATE: {
      const uint8_t leds[] = { LED_RED, LED_Y1, LED_Y2, LED_GREEN, LED_BLUE };
      if (now - lastBlinkTime >= BLINK_SEQ) {
        lastBlinkTime = now;
        allLedsOff();
        blinkStep = (blinkStep + 1) % 5;
        digitalWrite(leds[blinkStep], HIGH);
      }
      break;
    }

    // --- Aggiornamento disponibile
    case STATE_OTA_AVAILABLE: {
      if (now - lastBlinkTime >= BLINK_FAST) {
        lastBlinkTime = now;
        ledState = !ledState;
        digitalWrite(LED_Y1, ledState ? HIGH : LOW);
        digitalWrite(LED_Y2, ledState ? HIGH : LOW);
      }
      break;
    }

    // --- Aggiornamento fallito
    case STATE_OTA_FAILED: {
      if (now - lastBlinkTime >= BLINK_FAST) {
        lastBlinkTime = now;
        ledState = !ledState;
        digitalWrite(LED_RED, ledState ? HIGH : LOW);
      }
      break;
    }

    // --- Errore generic
    case STATE_ERROR: {
      if (now - lastBlinkTime >= BLINK_SLOW) {
        lastBlinkTime = now;
        ledState = !ledState;
        digitalWrite(LED_RED, ledState ? HIGH : LOW);
        digitalWrite(LED_Y1, !ledState ? HIGH : LOW);
      }
      break;
    }
  }
}

// ============================================================
//  BLE CALLBACKS
// ============================================================

class BLEConnectCB : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    bleConnected = true;
    Serial.println("[BLE] Client connesso");
  }
  void onDisconnect(BLEServer* s) override {
    bleConnected = false;
    Serial.println("[BLE] Client disconnesso");
    if (!credReceived) {
      BLEDevice::startAdvertising();
    }
  }
};

class SSIDCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    pendingSSID = String(c->getValue().c_str());
    Serial.printf("[BLE] SSID ricevuto: %s\n", pendingSSID.c_str());
  }
};

class PassCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    pendingPass = String(c->getValue().c_str());
    Serial.println("[BLE] Password ricevuta");
    if (pendingSSID.length() > 0) {
      credReceived = true;
      charStatus->setValue("CONNECTING");
      charStatus->notify();
    }
  }
};

void startBLE() {
  Serial.println("[BLE] Avvio BLE...");

  // Reset completo dello stato BLE prima di reinizializzare
  bleServer  = nullptr;
  charSSID   = nullptr;
  charPass   = nullptr;
  charStatus = nullptr;
  bleConnected = false;
  credReceived = false;

  delay(200);  // Lascia tempo al sistema di stabilizzarsi

  BLEDevice::init(BLE_SERVICE_NAME);
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new BLEConnectCB());

  BLEService* svc = bleServer->createService(BLE_SERVICE_UUID);

  charSSID = svc->createCharacteristic(BLE_CHAR_SSID_UUID,
    BLECharacteristic::PROPERTY_WRITE);
  charSSID->setCallbacks(new SSIDCallback());

  charPass = svc->createCharacteristic(BLE_CHAR_PASS_UUID,
    BLECharacteristic::PROPERTY_WRITE);
  charPass->setCallbacks(new PassCallback());

  charStatus = svc->createCharacteristic(BLE_CHAR_STATUS_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  charStatus->addDescriptor(new BLE2902());
  charStatus->setValue("WAITING");

  svc->start();

  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(BLE_SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("[BLE] Advertising avviato: " BLE_SERVICE_NAME);
}

void stopBLE() {
  Serial.println("[BLE] Arresto BLE...");
  BLEDevice::stopAdvertising();
  BLEDevice::deinit(true);
  delay(100);
}

// ============================================================
//  WIFI
// ============================================================

bool connectWiFi(const String& ssid, const String& pass, uint32_t timeoutMs = 15000) {
  Serial.printf("[WiFi] Connessione a: %s\n", ssid.c_str());
  deviceState = STATE_WIFI_CONNECT;
  allLedsOff();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) {
      Serial.println("[WiFi] Timeout connessione!");
      return false;
    }
    updateLeds();
    delay(10);
  }
  Serial.printf("[WiFi] Connesso! IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}

// ============================================================
//  NTP
// ============================================================

void syncNTP() {
  configTime(3600, 3600, "pool.ntp.org", "time.nist.gov"); // UTC+1, ora legale +1
  Serial.print("[NTP] Sincronizzazione...");
  uint32_t start = millis();
  struct tm ti;
  while (!getLocalTime(&ti, 100)) {
    if (millis() - start > 10000) {
      Serial.println(" FALLITA");
      return;
    }
    delay(200);
  }
  ntpSynced = true;
  Serial.printf(" OK - %02d:%02d:%02d\n", ti.tm_hour, ti.tm_min, ti.tm_sec);
}

// ============================================================
//  POMPA
// ============================================================

void startPump(uint16_t durationSec) {
  Serial.printf("[POMPA] Avvio per %d secondi\n", durationSec);
  digitalWrite(PUMP_PIN, HIGH);
  pumpRunning  = true;
  pumpStopTime = millis() + (uint32_t)durationSec * 1000;
}

void stopPump() {
  Serial.println("[POMPA] Stop");
  digitalWrite(PUMP_PIN, LOW);
  pumpRunning = false;
}

void handlePump() {
  if (pumpRunning && millis() >= pumpStopTime) {
    stopPump();
  }
}

// ============================================================
//  IRRIGAZIONE AUTOMATICA
// ============================================================

void checkScheduledIrrigation() {
  if (!irrigEnabled || !ntpSynced) return;

  struct tm ti;
  if (!getLocalTime(&ti, 50)) return;

  // Reset flag giornaliero a mezzanotte
  if (ti.tm_hour == 0 && ti.tm_min == 0) {
    irrigDoneToday = false;
  }

  // Controlla orario (esegue solo una volta al minuto corretto)
  if (ti.tm_hour == irrigHour && ti.tm_min == irrigMinute) {
    if (!irrigDoneToday && lastCheckedMin != (uint8_t)ti.tm_min) {
      lastCheckedMin = ti.tm_min;
      irrigDoneToday = true;
      Serial.printf("[IRRIGAZIONE] Avvio programmato %02d:%02d\n", irrigHour, irrigMinute);
      startPump(irrigDuration);
    }
  } else {
    lastCheckedMin = 255;
  }
}

// ============================================================
//  CONTROLLO AGGIORNAMENTI
// ============================================================

bool isNewerVersion(const String& current, const String& remote) {
  int ca[3] = {0}, ra[3] = {0};
  sscanf(current.c_str(), "%d.%d.%d", &ca[0], &ca[1], &ca[2]);
  sscanf(remote.c_str(),  "%d.%d.%d", &ra[0], &ra[1], &ra[2]);
  for (int i = 0; i < 3; i++) {
    if (ra[i] > ca[i]) return true;
    if (ra[i] < ca[i]) return false;
  }
  return false; 
}

void checkForUpdate() {
  String url = OTA_CHECK_URL;
  if (url.length() == 0) return;

  Serial.println("[OTA-CHECK] Controllo aggiornamenti...");
  Serial.printf("[OTA-CHECK] URL: %s\n", url.c_str());

  HTTPClient http;

  // Supporto sia HTTP che HTTPS
  // Per HTTPS usiamo setInsecure() — accetta qualsiasi certificato.
  // Sicuro per questo uso (solo lettura di un JSON pubblico).
  if (url.startsWith("https://")) {
    WiFiClientSecure* client = new WiFiClientSecure();
    client->setInsecure();
    http.begin(*client, url);
  } else {
    http.begin(url);
  }

  http.setTimeout(10000);
  http.addHeader("User-Agent", "ESP32-Irrigatore/" FIRMWARE_VERSION);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int code = http.GET();
  Serial.printf("[OTA-CHECK] HTTP response: %d\n", code);

  if (code != 200) {
    Serial.printf("[OTA-CHECK] Errore HTTP: %d\n", code);
    http.end();
    lastUpdateCheck = millis();
    return;
  }

  String payload = http.getString();
  http.end();

  Serial.printf("[OTA-CHECK] Payload: %s\n", payload.c_str());

  // Parsing JSON (ArduinoJson v6)
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[OTA-CHECK] JSON non valido: %s\n", err.c_str());
    lastUpdateCheck = millis();
    return;
  }

  const char* remoteVer = doc["version"] | "";
  const char* notes     = doc["notes"]   | "";
  const char* binUrl    = doc["bin_url"] | "";

  Serial.printf("[OTA-CHECK] Versione locale: %s  |  Remota: %s\n",
                FIRMWARE_VERSION, remoteVer);

  if (strlen(remoteVer) > 0 && isNewerVersion(FIRMWARE_VERSION, String(remoteVer))) {
    updateVersion   = String(remoteVer);
    updateNotes     = String(notes);
    updateBinUrl    = String(binUrl);
    updateAvailable = true;
    deviceState     = STATE_OTA_AVAILABLE;  
    Serial.printf("[OTA-CHECK] Aggiornamento disponibile: v%s\n", remoteVer);
    Serial.printf("[OTA-CHECK] BIN URL: %s\n", binUrl);
  } else {
    updateAvailable = false;
    updateBinUrl    = "";
    Serial.println("[OTA-CHECK] Firmware aggiornato.");
    if (deviceState == STATE_OTA_AVAILABLE) deviceState = STATE_RUNNING;
  }

  lastUpdateCheck = millis();
}

// ============================================================
//  WEB SERVER - HANDLERS
// ============================================================

void buildHTML(String& out) {
  out = FPSTR(HTML_PAGE);

  char timeStr[6];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", irrigHour, irrigMinute);

  out.replace("%VERSION%",    FIRMWARE_VERSION);
  out.replace("%IRRIG_TIME%", timeStr);
  out.replace("%IRRIG_DUR%",  String(irrigDuration));
  out.replace("%IRRIG_EN%",   irrigEnabled ? "checked" : "");
}

void handleRoot() {
  String html;
  buildHTML(html);
  server.send(200, "text/html; charset=utf-8", html);
}

void handleSaveIrrig() {
  if (!server.hasArg("time") || !server.hasArg("dur") || !server.hasArg("en")) {
    server.send(400, "text/plain", "Parametri mancanti");
    return;
  }

  String t   = server.arg("time");   // formato "HH:MM"
  uint16_t d = server.arg("dur").toInt();
  bool en    = server.arg("en").toInt() == 1;

  if (t.length() < 5 || d < 1 || d > 3600) {
    server.send(400, "text/plain", "Valori non validi");
    return;
  }

  irrigHour     = t.substring(0, 2).toInt();
  irrigMinute   = t.substring(3, 5).toInt();
  irrigDuration = d;
  irrigEnabled  = en;
  irrigDoneToday = false;

  prefs.begin(PREF_NAMESPACE, false);
  prefs.putUChar(PREF_IRRIG_H,   irrigHour);
  prefs.putUChar(PREF_IRRIG_M,   irrigMinute);
  prefs.putUShort(PREF_IRRIG_DUR, irrigDuration);
  prefs.putBool(PREF_IRRIG_EN,   irrigEnabled);
  prefs.end();

  Serial.printf("[WEB] Irrigazione salvata: %02d:%02d per %ds, abilitata=%d\n",
    irrigHour, irrigMinute, irrigDuration, irrigEnabled);

  server.send(200, "text/plain", "OK");
}

void handlePumpStart() {
  uint16_t dur = PUMP_DEFAULT_DURATION;
  if (server.hasArg("dur")) {
    int d = server.arg("dur").toInt();
    if (d >= 1 && d <= 3600) dur = d;
  }
  startPump(dur);
  server.send(200, "text/plain", "OK");
}

void handlePumpStop() {
  stopPump();
  server.send(200, "text/plain", "OK");
}

void handleOTAUpdate() {
  // Risposta finale dopo il flashing
  server.sendHeader("Connection", "close");
  bool success = !Update.hasError();
  server.send(success ? 200 : 500, "text/plain",
              success ? "OK" : Update.errorString());
  if (success) {
    Serial.println("[OTA] Aggiornamento completato, riavvio...");
    delay(500);
    ESP.restart();
  } else {
    deviceState = STATE_OTA_FAILED;
    Serial.println("[OTA] Aggiornamento FALLITO");
  }
}

void handleOTAUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("[OTA] Inizio upload: %s (%u bytes)\n",
                  upload.filename.c_str(), upload.totalSize);
    deviceState = STATE_OTA_UPDATE;
    allLedsOff();

    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Serial.printf("[OTA] Errore begin: %s\n", Update.errorString());
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Serial.printf("[OTA] Errore write: %s\n", Update.errorString());
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("[OTA] Upload completato: %u bytes\n", upload.totalSize);
    } else {
      Serial.printf("[OTA] Errore end: %s\n", Update.errorString());
      deviceState = STATE_OTA_FAILED;
    }
  }
}

void handleFactoryReset() {
  Serial.println("[RESET] Factory reset richiesto dalla UI web");
  server.send(200, "text/plain", "OK");
  delay(300);

  // Cancella tutto il namespace preferences
  prefs.begin(PREF_NAMESPACE, false);
  prefs.clear();
  prefs.end();

  Serial.println("[RESET] NVS cancellata. Riavvio in modalità BLE...");
  delay(200);
  ESP.restart();
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// Restituisce JSON con info aggiornamento (usato dalla UI per polling leggero)
void handleUpdateInfo() {
  String json = "{\"available\":";
  json += updateAvailable ? "true" : "false";
  json += ",\"version\":\"" + updateVersion + "\"";
  json += ",\"notes\":\"" + updateNotes + "\"";
  json += ",\"bin_url\":\"" + updateBinUrl + "\"";
  json += ",\"current\":\"" FIRMWARE_VERSION "\"}";
  server.send(200, "application/json", json);
}

// Scarica il .bin da GitHub e lo flasha direttamente (OTA remoto)
void handleOTAGitHub() {
  if (!server.hasArg("url")) {
    server.send(400, "text/plain", "URL mancante");
    return;
  }
  String binUrl = server.arg("url");
  if (binUrl.length() == 0) {
    server.send(400, "text/plain", "URL vuoto");
    return;
  }

  Serial.printf("[OTA-GH] Download da: %s\n", binUrl.c_str());
  deviceState = STATE_OTA_UPDATE;
  allLedsOff();


  server.sendHeader("Connection", "close");

  HTTPClient http;
  WiFiClientSecure* client = new WiFiClientSecure();
  client->setInsecure();
  http.begin(*client, binUrl);
  http.setTimeout(60000);
  http.addHeader("User-Agent", "ESP32-Irrigatore/" FIRMWARE_VERSION);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int code = http.GET();
  Serial.printf("[OTA-GH] HTTP: %d\n", code);

  if (code != 200) {
    http.end();
    delete client;
    deviceState = STATE_OTA_FAILED;
    server.send(500, "text/plain", "Download fallito: HTTP " + String(code));
    return;
  }

  int totalSize = http.getSize();
  Serial.printf("[OTA-GH] Dimensione: %d bytes\n", totalSize);

  if (!Update.begin(totalSize > 0 ? totalSize : UPDATE_SIZE_UNKNOWN)) {
    http.end();
    delete client;
    deviceState = STATE_OTA_FAILED;
    server.send(500, "text/plain", String("Update.begin: ") + Update.errorString());
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[1024];
  int written = 0;
  int read = 0;

  while (http.connected() && (totalSize == -1 || written < totalSize)) {
    size_t available = stream->available();
    if (available) {
      int toRead = min((int)available, (int)sizeof(buf));
      read = stream->readBytes(buf, toRead);
      if (Update.write(buf, read) != (size_t)read) {
        Serial.printf("[OTA-GH] Errore write: %s\n", Update.errorString());
        break;
      }
      written += read;
    } else {
      delay(1);
    }
    updateLeds();
  }

  http.end();
  delete client;

  if (Update.end(true)) {
    Serial.printf("[OTA-GH] Flash completato: %d bytes\n", written);
    server.send(200, "text/plain", "OK");
    delay(500);
    ESP.restart();
  } else {
    Serial.printf("[OTA-GH] Errore end: %s\n", Update.errorString());
    deviceState = STATE_OTA_FAILED;
    server.send(500, "text/plain", String("Flash fallito: ") + Update.errorString());
  }
}

void startWebServer() {
  server.on("/",            HTTP_GET,  handleRoot);
  server.on("/save-irrig",  HTTP_POST, handleSaveIrrig);
  server.on("/pump-start",  HTTP_GET,  handlePumpStart);
  server.on("/pump-stop",   HTTP_GET,  handlePumpStop);
  server.on("/update",      HTTP_POST, handleOTAUpdate, handleOTAUpload);
  server.on("/update-info", HTTP_GET,  handleUpdateInfo);
  server.on("/ota-github",  HTTP_POST, handleOTAGitHub);
  server.on("/factory-reset", HTTP_POST, handleFactoryReset);
  server.onNotFound(handleNotFound);
  server.begin();

  // mDNS: raggiungibile su http://irriga.local
  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", WEB_SERVER_PORT);
    Serial.println("[mDNS] Avviato: http://" MDNS_HOSTNAME ".local");
  } else {
    Serial.println("[mDNS] Errore avvio mDNS");
  }

  Serial.printf("[WEB] Server avviato su http://%s\n", WiFi.localIP().toString().c_str());
}

// ============================================================
//  CARICA/SALVA CONFIG
// ============================================================

void loadConfig(String& ssid, String& pass) {
  prefs.begin(PREF_NAMESPACE, true);
  ssid          = prefs.getString(PREF_SSID, "");
  pass          = prefs.getString(PREF_PASS, "");
  irrigHour     = prefs.getUChar(PREF_IRRIG_H,   8);
  irrigMinute   = prefs.getUChar(PREF_IRRIG_M,   0);
  irrigDuration = prefs.getUShort(PREF_IRRIG_DUR, PUMP_DEFAULT_DURATION);
  irrigEnabled  = prefs.getBool(PREF_IRRIG_EN,   false);
  prefs.end();
}

void saveWiFiConfig(const String& ssid, const String& pass) {
  prefs.begin(PREF_NAMESPACE, false);
  prefs.putString(PREF_SSID, ssid);
  prefs.putString(PREF_PASS, pass);
  prefs.end();
}

// ============================================================
//  SETUP
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[BOOT] Irrigatore ESP32 v" FIRMWARE_VERSION);

  
  pinMode(LED_RED,   OUTPUT);
  pinMode(LED_Y1,    OUTPUT);
  pinMode(LED_Y2,    OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE,  OUTPUT);
  pinMode(PUMP_PIN,  OUTPUT);
  allLedsOff();
  digitalWrite(PUMP_PIN, LOW);


  String savedSSID, savedPass;
  loadConfig(savedSSID, savedPass);
  Serial.printf("[CONFIG] SSID salvato: '%s'\n", savedSSID.c_str());

  if (savedSSID.length() > 0) {
    
    bool ok = connectWiFi(savedSSID, savedPass, 15000);
    if (ok) {
      syncNTP();
      startWebServer();
      checkForUpdate();          // Controlla subito al boot
      // Non sovrascrivere lo stato se checkForUpdate ha trovato un aggiornamento
      if (deviceState != STATE_OTA_AVAILABLE) {
        deviceState = STATE_RUNNING;
      }
      Serial.println("[BOOT] Avvio normale completato.");
      return;
    }
    Serial.println("[BOOT] Connessione fallita, avvio BLE per riconfigurazione...");
  }


  deviceState = STATE_BLE_WAIT;
  startBLE();
  Serial.println("[BOOT] In attesa credenziali WiFi via BLE...");
}

// ============================================================
//  LOOP
// ============================================================

void loop() {
  
  if (deviceState != STATE_RUNNING && deviceState != STATE_OTA_UPDATE) {
    updateLeds();
  } else if (deviceState == STATE_RUNNING) {
    updateLeds(); 
  } else {
    updateLeds(); 
  }

  // --- Gestione BLE
  if (deviceState == STATE_BLE_WAIT) {
    if (credReceived) {
      credReceived = false;
      Serial.printf("[BLE->WiFi] Tentativo con SSID: %s\n", pendingSSID.c_str());

      stopBLE();
      delay(200);

      bool ok = connectWiFi(pendingSSID, pendingPass, 15000);

      if (ok) {
        // Salva le credenziali
        saveWiFiConfig(pendingSSID, pendingPass);
        syncNTP();
        startWebServer();
        checkForUpdate();        
        if (deviceState != STATE_OTA_AVAILABLE) {
          deviceState = STATE_RUNNING;
        }
        allLedsOff();
        Serial.println("[BOOT] Configurazione completata!");
      } else {
        
        Serial.println("[WiFi] Credenziali errate, riavvio per tornare in modalità BLE...");
        WiFi.disconnect(true);
        delay(500);
        ESP.restart();  // Più sicuro che reinizializzare BLE a caldo
      }
    }
    return; 
  }

  // --- Stato operativo ---
  if (deviceState == STATE_RUNNING || deviceState == STATE_OTA_UPDATE ||
      deviceState == STATE_OTA_AVAILABLE || deviceState == STATE_OTA_FAILED) {

    server.handleClient();
    handlePump();

    // Ripristina stato RUNNING dopo OTA_FAILED con un delay
    static uint32_t otaFailedTime = 0;
    if (deviceState == STATE_OTA_FAILED) {
      if (otaFailedTime == 0) otaFailedTime = millis();
      if (millis() - otaFailedTime > 10000) { // 10 secondi di rosso lampeggiante
        otaFailedTime = 0;
        deviceState   = STATE_RUNNING;
      }
    }

    
    static uint32_t lastIrrigCheck = 0;
    if (millis() - lastIrrigCheck >= 5000) {
      lastIrrigCheck = millis();
      checkScheduledIrrigation();
    }

    // Controllo aggiornamenti periodico
    #if OTA_CHECK_HOURS > 0
    uint32_t otaIntervalMs = (uint32_t)OTA_CHECK_HOURS * 3600000UL;
    if (lastUpdateCheck > 0 && (millis() - lastUpdateCheck >= otaIntervalMs)) {
      checkForUpdate();
    }
    #endif
  }

  
  delay(5);
}
