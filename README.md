# SENTINEL Node V3.5: Documentazione Tecnica e Specifiche

Sentinel Node è un'unità di monitoraggio IoT industriale basata su architettura ESP32 e Ubuntu LTS, ottimizzata per la gestione autonoma di ambienti HACCP. Il sistema è progettato per operare in edge computing, garantendo l'integrità dei dati senza dipendenze da infrastrutture Cloud esterne.

---

### Architettura e Storage
Il sistema adotta una strategia di storage ibrida per massimizzare la resilienza:
* FRAM (Ferroelectric RAM): Utilizzata per il log dei dati in tempo reale. Offre cicli di scrittura quasi infiniti, eliminando il degrado tipico delle memorie Flash/SD e garantendo la persistenza dei dati anche in caso di blackout improvviso.
* Local SQLite: Database primario su host per l'archiviazione e l'analisi storica dei record.

### Validazione e Sicurezza del Dato
Per garantire la conformità agli standard di audit e la non ripudiabilità:
* Firma SHA-256: Ogni record viene firmato localmente incrociando i dati dei sensori con l'Unique Device ID della MCU.
* Isolamento di Rete: Comunicazione basata su MQTT locale e supporto mTLS. Il perimetro di rete è chiuso, riducendo la superficie di attacco e garantendo il funzionamento offline.

### Specifiche Tecniche
- MCU          : ESP32 (Firmware C++ / Toolchain RAW)
- OS Centrale  : Ubuntu LTS (Gestione tramite Vim/Bash/Python)
- Storage      : FRAM (Real-time) + SQLite (Storage)
- Crittografia : SHA-256 Hardware-backed
- Protocolli   : MQTT, mTLS, HTTP Ingest

---

### Guida alla Configurazione Rapida
Per compilare il firmware e configurare il nodo, eseguire i seguenti comandi atomici:

1. Inizializzazione ambiente:
   cp env_example .env

2. Configurazione parametri di rete (Modificare l'indirizzo IP per puntare al Raspberry Pi / Server locale):
   ```
   WIFI_SSID=cambia_questo
   WIFI_PASSWORD=cambia_questo
   SERVER_URL=http://[IP_ADDRESS]:5040/ingest
   UPDATE_URL=http://[IP_ADDRESS]:5040/update/check
   INGEST_URL=http://[IP_ADDRESS]:5040/ingest
   NTP_SERVER=pool.ntp.org

   ```

3. make 
