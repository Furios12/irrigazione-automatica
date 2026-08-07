# 🌿 Irrigazione Automatica

Firmware per ESP32 che gestisce un sistema di irrigazione automatica con controllo via web, aggiornamenti OTA da GitHub e configurazione WiFi via Bluetooth.

---

## Indice

- [Componenti](#componenti)
- [Pinout](#pinout)
- [Come iniziare](#come-iniziare)
- [Interfaccia Web](#interfaccia-web)
- [LED di stato](#led-di-stato)
- [Aggiornamenti OTA](#aggiornamenti-ota)
- [Struttura del progetto](#struttura-del-progetto)

---

## Componenti

- ESP32 Mini (o qualsiasi ESP32)
- 5 LED (rosso, giallo x2, verde, blu)
- Pompa acqua (controllata via relè su PIN 33)
- Alimentazione 5V / 12V per la pompa

---

## Pinout

| Pin | Colore | Funzione |
|-----|--------|----------|
| 25  | 🔴 Rosso  | LED stato errore / WiFi |
| 26  | 🟡 Giallo 1 | LED aggiornamento |
| 27  | 🟡 Giallo 2 | LED aggiornamento |
| 14  | 🟢 Verde  | LED operativo |
| 12  | 🔵 Blu    | LED connessione WiFi |
| 33  | —       | Pompa (segnale relè) |

---

## Come iniziare

### 1. Primo avvio — Configurazione WiFi via Bluetooth

Al primo avvio, l'ESP32 non conosce la rete WiFi. Entra automaticamente in modalità Bluetooth e aspetta le credenziali.

I **LED rosso e blu lampeggiano 3 volte** per indicare che si è in modalità di configurazione.

**Come connettersi:**

1. Scarica l'app **nRF Connect** (Android / iOS)
2. Scansiona i dispositivi Bluetooth e cerca `IrrigaESP32`
3. Connettiti al dispositivo
4. Scrivi il nome della tua rete WiFi nella caratteristica UUID `...ab1`
5. Scrivi la password nella caratteristica UUID `...ab2`

L'ESP32 si connette alla rete, salva le credenziali nella memoria flash e non userà più il Bluetooth per i prossimi avvii.

---

### 2. Avvii successivi

L'ESP32 si connette automaticamente al WiFi salvato, sincronizza l'orario via NTP e avvia il web server.

Dal **Serial Monitor** (115200 baud) vedrai l'indirizzo IP assegnato:
```
[WEB] Server avviato su http://192.168.x.x
[mDNS] Avviato: http://irriga.local
```

Apri il browser e vai su:
```
http://irriga.local
```
oppure direttamente all'IP dell'ESP32.

> **Nota:** `irriga.local` funziona su Android, iOS, macOS e Windows 10/11. Su Windows è necessario che il servizio Bonjour sia attivo (di solito già presente se hai iTunes o Zoom installati).

---

## Interfaccia Web

L'interfaccia è raggiungibile da qualsiasi dispositivo sulla stessa rete WiFi.

### Programmazione automatica

Imposta l'orario giornaliero di irrigazione e la durata in secondi. Usa il toggle per abilitare o disabilitare la programmazione senza perdere le impostazioni.

Le impostazioni vengono salvate nella memoria flash e sopravvivono ai riavvii.

### Irrigazione manuale

Avvia la pompa immediatamente per una durata a scelta. Il tasto **Stop** ferma la pompa in qualsiasi momento.

### Aggiornamento Firmware (OTA)

Due modalità disponibili:

- **Automatico da GitHub** — se è disponibile un aggiornamento, appare un banner arancione con un tasto per scaricare e installare direttamente dal repository GitHub
- **Manuale** — carica un file `.bin` dal tuo computer

---

## LED di stato

| Situazione | LED |
|---|---|
| Attesa credenziali WiFi (BLE attivo) | 🔴 Rosso + 🔵 Blu: 3 lampeggi, pausa, ripeti |
| Connessione WiFi in corso | 🔵 Blu lampeggio veloce |
| Operativo normale | 🟢 Verde fisso |
| Aggiornamento in corso | Sequenza scorrevole: 🔴→🟡→🟡→🟢→🔵 |
| Aggiornamento disponibile | 🟡🟡 Gialli lampeggio veloce |
| Aggiornamento fallito | 🔴 Rosso lampeggio veloce (10 sec) |
| Errore generico | 🔴 Rosso e 🟡 Giallo1 alternati |

---

## Aggiornamenti OTA

Il firmware controlla automaticamente gli aggiornamenti leggendo il file `update.json` ospitato su GitHub.

### Come funziona

1. All'avvio, l'ESP32 scarica `update.json` e confronta la versione remota con quella installata
2. Se la versione remota è più recente, i **LED gialli iniziano a lampeggiare** e nella UI appare il banner di aggiornamento
3. Clicca **"Scarica e installa aggiornamento"** — l'ESP32 scarica il `.bin` da GitHub Releases e si aggiorna da solo

### Struttura di update.json

```json
{
  "version": "1.0.4",
  "notes": "Descrizione delle novità",
  "bin_url": "https://github.com/Furios12/irrigazione-automatica/releases/download/v1.0.4/firmware.bin",
  "mandatory": false,
  "released": "2026-08-07"
}
```

### Come rilasciare una nuova versione

1. Aggiorna `FIRMWARE_VERSION` in `config.h`
2. Compila il firmware in Arduino IDE: `Sketch → Export Compiled Binary`
3. Crea una nuova release su GitHub con tag `v1.x.x` e allega il file `firmware.bin`
4. Aggiorna `update.json` nel repo con la nuova versione e il nuovo `bin_url`

L'ESP lo rileverà entro il prossimo controllo (configurabile con `OTA_CHECK_HOURS` in `config.h`).

---

## Struttura del progetto

```
irrigazione-automatica/
├── firmware/
│   ├── firmware.ino   — codice principale
│   ├── config.h       — pin, costanti, URL aggiornamenti
│   └── web_ui.h       — interfaccia web (HTML/CSS/JS)
├── update.json        — manifest aggiornamenti OTA
└── README.md          — questo file
```

---

## Librerie necessarie

Installabili da Arduino IDE → `Tools → Manage Libraries`:

| Libreria | Autore |
|---|---|
| `ArduinoJson` | Benoit Blanchon (v6.x) |
| `ESP32 BLE Arduino` | inclusa con il pacchetto ESP32 |

Il pacchetto ESP32 si installa aggiungendo questo URL in `File → Preferences → Additional boards manager URLs`:
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

### Partition Scheme

Per compilare correttamente (il firmware con BLE è ~1.57 MB) seleziona:

`Tools → Partition Scheme → Minimal SPIFFS (1.9MB APP with OTA /190KB SPIFFS)`

---

*Developed by Furios121*
