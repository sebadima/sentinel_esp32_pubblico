# SENTINEL Node V3.5

**ATTENZIONE: LEGGERE PRIMA DI COMPILARE**

Questo repository contiene il firmware per Sentinel Node. Non è un giocattolo plug-and-play. Se non configuri l'ambiente come descritto di seguito, la compilazione fallirà.

## 1. Configurazione Credenziali (Obbligatorio)
Il sistema utilizza un generatore automatico di header per proteggere le tue credenziali. Git non traccerà mai i tuoi segreti.

1. Localizza il file `env_example` nella root.
2. Copialo creando un nuovo file chiamato `.env`:
   ```bash
      cp env_example .env

e quindi procedi alle modifiche personalizzando .env con il tuo setup personale.      


Sentinel Node: IoT eterodosso per monitoraggio sensori e ambienti HACCP in autonomia. Basato su ESP32 e Ubuntu LTS, elimina il Cloud e la burocrazia cartacea. Firma SHA-256 locale, storage su FRAM contro l'usura delle SD e isolamento totale della rete. Lo strumento per difendere la sovranità delle piccole aziende alimentari. Hardware e non fumo.

Sentinel Node: Il Manifesto del Monitoraggio Eterodosso
Il mercato IoT oggi è un cimitero di dispositivi fragili che chiedono il permesso a un server in California per accendere un LED. Sentinel Node è l'antitesi di questa decadenza. È un'arma tecnologica progettata per la sovranità delle piccole aziende alimentari, dove l'HACCP non è una serie di crocette su un foglio di carta unto, ma un'esigenza di verità statistica.

La Filosofia del Ferro
Sentinel non "si connette". Sentinel presidia. Costruito su architettura ESP32 e integrato con Ubuntu LTS, il sistema è progettato per operare in isolamento totale. Il Cloud non è un'opportunità, è un punto di fallimento (SPOF) che abbiamo rimosso alla radice. Se internet cade, Sentinel non batte ciglio: continua a firmare e archiviare.

Architettura Anti-Fragile
Mentre gli hobbisti usano le schede SD che si corrompono al primo sbalzo di tensione, Sentinel implementa lo storage su FRAM (Ferroelectric RAM). Questa scelta garantisce:

Cicli di scrittura pressoché infiniti: Milioni di volte superiori alle memorie Flash.

Velocità pura: Nessun ritardo di buffering.

Integrità totale: Il dato viene scritto istantaneamente, proteggendo i log anche in caso di blackout improvviso.

Validazione della Verità: Firma SHA-256
In un regime di controllo HACCP, la validità del dato è tutto. Ogni record generato dai sensori (temperatura, umidità, varchi) viene processato localmente attraverso un algoritmo di firma SHA-256 che incrocia il dato con l'Unique Device ID della MCU.
Questo crea una catena di prove digitale non ripudiabile: il dato che leggi sul monitor è esattamente quello che il sensore ha rilevato, immutato, senza passaggi intermedi in database di terze parti.

De-Burocratizzazione Radicale
Sentinel trasforma la burocrazia cartacea in codice eseguibile. Automatizzando il monitoraggio ambientale, elimini l'errore umano e il rischio di sanzioni. Ma non lo fai affidandoti a una dashboard "as a Service" con canone mensile. Lo fai con il tuo hardware, gestito dal tuo server locale, mantenendo il controllo totale del perimetro di rete.

Specifiche per Tecnici
Kernel: Integrazione nativa con workflow Bash/Python su Ubuntu LTS.

Protocollo: MQTT locale per l'isolamento della rete sensori.

Integrità: Firma crittografica locale per record a prova di perizia.

Resilienza: Supporto mTLS per comunicazioni sicure tra nodi e centrale.

Sentinel Node non è un prodotto. È una dichiarazione di indipendenza tecnologica. Se vuoi la comodità di un giocattolo che smette di funzionare quando l'azienda produttrice fallisce, guarda altrove. Se vuoi il ferro che difende la tua azienda, hai trovato lo strumento.

Hardware puro. Zero marketing. Solo sovranità.
