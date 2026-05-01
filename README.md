# SENTINEL Node V3.5: Documentazione Tecnica e Specifiche

Sentinel Node è un'unità di monitoraggio IoT basata su architettura ESP32 e Ubuntu LTS, ottimizzata per la gestione autonoma di ambienti multi-sensori. Il sistema è progettato per operare in edge computing, garantendo l'integrità dei dati senza dipendenze da infrastrutture Cloud esterne.

---

### Architettura e Storage
Il sistema adotta una strategia di storage ibrida per massimizzare la resilienza:
* FRAM (Ferroelectric RAM): Utilizzata per il log dei dati in tempo reale. Offre cicli di scrittura quasi infiniti, eliminando il degrado tipico delle memorie Flash/SD e garantendo la persistenza dei dati anche in caso di blackout improvviso.
* Local SQLite: Database primario su host per l'archiviazione e l'analisi storica dei record.

### Validazione e Sicurezza del Dato
Per garantire la conformità agli standard di audit e la non ripudiabilità:
* Firma SHA-256: Ogni record viene firmato localmente incrociando i dati dei sensori con l'Unique Device ID della MCU.
* Isolamento di Rete: Il perimetro di rete è chiuso, non ci sono porte o servizi aperti verso server esterni.

### Specifiche Tecniche
- MCU          : ESP32 (Firmware C++ / Toolchain RAW)
- OS Centrale  : Ubuntu LTS (Gestione tramite Vim/Bash/Python)
- Storage      : FRAM (Real-time) + SQLite (Storage)
- Protocolli   : MQTT, mTLS, HTTP Ingest



## Aggiornamento Over The Air (Pull-OTA
Il Pull-OTA funziona così: l'ESP32 è il cliente e il server Flask è il magazzino.

- Controllo: L'ESP32 contatta il server e legge un piccolo file di testo (JSON) che indica l'ultima versione disponibile.
- Confronto: Se la versione sul server (es. 271) è maggiore di quella che l'ESP32 sta facendo girare (es. 270), parte il download.
- Aggiornamento: L'ESP32 scarica il file binario dal server, lo scrive nella memoria interna e si riavvia da solo con il nuovo codice.

Vantaggio: Non devi collegare cavi USB. Ti basta caricare il file sul server e l'hardware si aggiorna via Wi-Fi quando è pronto.



### Guida alla Configurazione Rapida
Per compilare il firmware puoi eseguire i seguenti comandi:

1. Inizializzazione ambiente:
   ```
   cp env_example .env
   ```

2. Configurazione parametri di rete (Modificare l'indirizzo IP per puntare al Raspberry Pi / Server locale):
   ```
   WIFI_SSID=cambia_questo
   WIFI_PASSWORD=cambia_questo
   SERVER_URL=http://[IP_ADDRESS]:5040/ingest
   UPDATE_URL=http://[IP_ADDRESS]:5040/update/check
   INGEST_URL=http://[IP_ADDRESS]:5040/ingest
   NTP_SERVER=pool.ntp.org

   ```

3. Prima compilazione (per il primo upload via USB):
   ```
   pio run -t upload   
   ```

### Per tutte le successive compilazioni (PULL-OTA)

1. **Aggiornamento Versione**: 
   Incrementa `const int FW_VERSION` nel file `src/main.cpp` (es: da 270 a 271).

2. **Deploy via Server**:
   Lancia il comando:
   ```bash
   make