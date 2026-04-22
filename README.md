# sENTINEL Node V3.5

**ATTENZIONE: LEGGERE PRIMA DI COMPILARE**

Questo repository contiene il firmware per Sentinel Node. Non è un giocattolo plug-and-play. Se non configuri l'ambiente come descritto di seguito, la compilazione fallirà.

## 1. Configurazione Credenziali (Obbligatorio)
Il sistema utilizza un generatore automatico di header per proteggere le tue credenziali. Git non traccerà mai i tuoi segreti.

1. Localizza il file `env_example` nella root.
2. Copialo creando un nuovo file chiamato `.env`:
   ```bash
      cp env_example .env

e quindi procedi alle modifiche personalizzando .env con il tuo setup personale.      
