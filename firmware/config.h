#pragma once

// ================= PIN =================
#define LED_RED    25  // Rosso
#define LED_Y1     26  // Giallo 1
#define LED_Y2     27  // Giallo 2
#define LED_GREEN  14  // Verde
#define LED_BLUE   12  // Blu
#define PUMP_PIN   33  // Pompa

// ================= FIRMWARE =================
#define FIRMWARE_VERSION  "1.0.0"
// URL del file update.json sul tuo server (es. GitHub raw, server personale, ecc.)
// Lascia "" per disabilitare il controllo automatico
#define OTA_CHECK_URL     "https://raw.githubusercontent.com/Furios12/irrigazione-automatica/refs/heads/main/update.json"
// Controlla aggiornamenti ogni X ore (0 = solo all'avvio)
#define OTA_CHECK_HOURS   0

// ================= BLE =================
#define BLE_SERVICE_NAME "IrrigaESP32" // Nome BLE

// ================= WEB SERVER =================
#define WEB_SERVER_PORT  80 // Porta del web server
#define MDNS_HOSTNAME    "irriga"   // raggiungibile su irriga.local

// ================= PREFERENCES =================
#define PREF_NAMESPACE   "irrigatore"
#define PREF_SSID        "wifi_ssid"
#define PREF_PASS        "wifi_pass"
#define PREF_IRRIG_H     "irrig_hour"
#define PREF_IRRIG_M     "irrig_min"
#define PREF_IRRIG_DUR   "irrig_dur"
#define PREF_IRRIG_EN    "irrig_en"

// ================= LED TIMING =================
#define BLINK_FAST       150   // ms
#define BLINK_SLOW       500   // ms
#define BLINK_SEQ        300   // ms sequenza OTA

// ================= IRRIGAZIONE =================
#define PUMP_DEFAULT_DURATION  30  // secondi default
