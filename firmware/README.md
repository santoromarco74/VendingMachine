# 🔌 STM32 Firmware - Vending Machine IoT

Firmware per **ST Nucleo F401RE** con shield BLE **X-NUCLEO-IDB05A2**.

---

## 📦 **File Disponibili**

| File | Versione | Descrizione | Stato |
|------|----------|-------------|-------|
| `main.cpp` | **v8.14** | **VERSIONE FINALE** con tutti i fix e ottimizzazioni | ✅ **Raccomandato** |
| `main_v7.1_original.cpp` | v7.1 | Versione originale legacy (solo per riferimento storico) | ⚠️ Deprecato |

---

## 🔧 **Versione v8.14 (FINALE - RACCOMANDATO)**

### ✨ **Funzionalità Principali**

✅ **LCD Feedback Completo:**
- BLE connessione/disconnessione (v8.10)
- Conferma prodotto con nome dinamico: "Conf. x ACQUA!" (v8.10)
- Rifornimento scorte: "RIFORNIMENTO... → OK! / Scorte: 5/5/5/5" (v8.14)

✅ **LDR Spike Detection Adattivo (v8.9):**
- Baseline mobile EMA (Exponential Moving Average)
- Soglie relative (+20% scatto, +5% reset)
- Funziona con qualsiasi illuminazione ambiente

✅ **Sonar HC-SR04 Stabile (v8.13):**
- echoDuration reset prima di ogni misura
- Timing ottimale: trig 10μs, timeout 15ms
- Nessuna pausa tra misure (evita timeout ISR)

✅ **Auto-Refund BLE (v8.11):**
- Resto immediato alla disconnessione BLE (3s vs 30s timeout)
- Previene perdita accidentale credito

✅ **Sistema Robusto:**
- Watchdog timer 10s
- Thread separato DHT11 (non blocca main loop)
- Validazione comandi BLE
- Gestione scorte con indicatori

**Changelog completo**: Vedi [`../BUGFIXES.md`](../BUGFIXES.md)

### 🆚 **Evoluzione da v7.1**

| Feature | v7.1 (Legacy) | v8.14 (Finale) |
|---------|---------------|----------------|
| **LDR Detection** | Soglia assoluta 25% | Spike detection EMA +20% |
| **Sonar** | Timing base | echoDuration reset + timing ottimale |
| **LCD Feedback** | Base | Completo (BLE, prodotto, rifornimento) |
| **BLE Refund** | Timeout 30s | Auto-refund immediato |
| **Debouncing LDR** | 5 campioni @ 300ms | 3 campioni @ 200ms |
| **Log Output** | 12 righe box ASCII | 1 riga compatta |
| **Scorte** | No gestione | Tracking + rifornimento BLE |

---

## ⚠️ **REQUISITI HARDWARE OBBLIGATORI**

Prima di compilare, verifica di avere **fisicamente**:

| Componente | Modello | Obbligatorio | Note |
|------------|---------|--------------|------|
| **Scheda Nucleo** | F401RE | ✅ SÌ | MCU STM32F401RE |
| **Shield BLE** | X-NUCLEO-IDB05A2 (o IDB05A1) | ✅ **SÌ** | La Nucleo **NON ha BLE integrato**! |
| **Display LCD** | 16x2 I2C | ✅ SÌ | Indirizzo 0x4E o 0x27 |
| **Sensori** | DHT11, HC-SR04, LDR | ✅ SÌ | Vedi WIRING.md |
| **Attuatori** | Servo SG90, Buzzer, LED RGB | ✅ SÌ | Vedi WIRING.md |

**🔴 ATTENZIONE**: Senza lo shield X-NUCLEO-IDB05A2 montato, il firmware darà errore:
```
Assertion failed: _hci_driver != nullptr
```

---

## 🚀 **Come Compilare**

### **Requisiti Software**

- **IDE**: Keil Studio Cloud oppure Mbed Studio
- **Toolchain**: ARM GCC 10.x o superiore
- **Mbed OS**: 6.x (testato con 6.15.0)
- **Librerie**:
  - `mbed-os`
  - `TextLCD` (per display I2C)
  - `X_NUCLEO_IDB0XA1` (driver shield BLE)

### **Opzione 1: Keil Studio Cloud (Online)** ⭐ **RACCOMANDATO**

1. Vai su https://studio.keil.arm.com/
2. **File → New Project → Empty Mbed OS 6 Project**
3. Nome progetto: `VendingMachine`
4. **Target**: Seleziona `NUCLEO_F401RE`
5. **Carica tutti i file della cartella firmware:**
   - `main.cpp` (v8.14)
   - `mbed_app.json` ⚠️ **IMPORTANTE!**
   - `mbed-os.lib`
   - `TextLCD.lib`
   - `X_NUCLEO_IDB0XA1.lib` ⚠️ **DRIVER BLE SHIELD!**
6. Click su **"Build"** (icona martello)
7. Scarica il `.bin` generato

**⚠️ IMPORTANTE**:
- Tutti i file `.lib` e `mbed_app.json` devono essere nella root del progetto
- Il target deve essere `NUCLEO_F401RE`
- Mbed OS versione 6.15.0 o superiore

### **Opzione 2: Mbed Studio (Desktop)**

```bash
# Clona questo repository
git clone https://github.com/santoromarco74/VendingMonitor.git
cd VendingMonitor/firmware

# Importa in Mbed Studio:
# File → Import Program → Select Folder → Seleziona questa cartella

# Compila:
# Build → NUCLEO_F401RE → Build

# Il file .bin sarà in: BUILD/NUCLEO_F401RE/GCC_ARM/
```

### **Opzione 3: Mbed CLI (Terminale)**

```bash
mbed config root .
mbed target NUCLEO_F401RE
mbed toolchain GCC_ARM
mbed deploy  # Scarica dipendenze
mbed compile

# Flash automatico se scheda connessa:
mbed compile -f
```

---

## 📥 **Flash sulla Scheda**

### **Metodo 1: Drag & Drop (Più Semplice)**

1. Connetti la Nucleo via USB
2. Apparirà come unità di massa `NODE_F401RE`
3. Trascina il file `.bin` sull'unità
4. Attendi il LED LD1 che lampeggia (programmazione)
5. Reset automatico al termine

### **Metodo 2: STM32CubeProgrammer**

1. Scarica da: https://www.st.com/en/development-tools/stm32cubeprog.html
2. Connetti Nucleo
3. Apri STM32CubeProgrammer
4. Seleziona "USB" o "ST-LINK"
5. Click "Connect"
6. Click "Open file" → Seleziona `.bin`
7. Click "Download"

---

## 🔌 **Configurazione Hardware**

### **Pinout (Nucleo F401RE + Shield BLE)**

| Componente | Pin Nucleo | Note |
|------------|------------|------|
| **BLE Shield** | Morpho | X-NUCLEO-IDB05A2 (SPI) |
| **LCD I2C** | D14 (SDA), D15 (SCL) | Indirizzo 0x4E o 0x27 |
| **Ultrasuoni HC-SR04** | A1 (Trig), D9 (Echo) | Interrupt safe |
| **LDR (Monete)** | A2 (Analog) | Fotoresistore con divisore |
| **DHT11** | D4 | Temp & Umidità |
| **Servo SG90** | D5 (PWM) | Erogazione prodotto |
| **Buzzer** | D2 | Feedback acustico |
| **LED RGB** | D6 (R), D8 (G), A3 (B) | LED comune catodo |
| **Pulsante Annulla** | PC_13 | Tasto BLU integrato Nucleo |

**Dettagli completi**: Vedi `../WIRING.md`

---

## 🐛 **Troubleshooting**

### **Problema: "Assertion failed: _hci_driver != nullptr"** 🔴 **CRITICO**

**Errore Completo**:
```
MbedOS Error Info
Error Status: 0x80FF0144 Code: 324 Module: 255
Error Message: Assertion failed: _hci_driver != nullptr
Please provide an implementation for the HCI driver
```

**Causa**: Manca il driver dello shield BLE X-NUCLEO-IDB05A2

**Soluzione**:
1. **Verifica hardware**: Lo shield BLE **deve essere fisicamente montato** sulla Nucleo
2. **Aggiungi la libreria del driver**: Assicurati che il file `X_NUCLEO_IDB0XA1.lib` sia presente
3. **Verifica `mbed_app.json`**: Deve contenere:
   ```json
   {
       "target_overrides": {
           "NUCLEO_F401RE": {
               "target.components_add": ["BlueNRG_MS"],
               "idb0xa1.provide-default": true
           }
       }
   }
   ```
4. **Ricompila completamente** (clean build)

**⚠️ IMPORTANTE**: La Nucleo F401RE **NON ha BLE integrato**. Serve obbligatoriamente lo shield X-NUCLEO-IDB05A2 (o IDB05A1).

---

### **Problema: "'ble/BLE.h' file not found"** ⚠️ **COMUNE**

**Causa**: Manca il file `mbed_app.json` o la configurazione BLE

**Soluzione**:
1. **Verifica che `mbed_app.json` sia presente** nella root del progetto
2. **Verifica che contenga**:
   ```json
   {
       "target_overrides": {
           "*": {
               "target.features_add": ["BLE"]
           }
       }
   }
   ```
3. **Ricompila il progetto**

### **Problema: "Error: Could not compile"**

**Causa**: Librerie mancanti o versione Mbed OS errata

**Soluzione (Mbed CLI)**:
```bash
mbed deploy  # Scarica tutte le dipendenze
mbed compile --clean
```

**Soluzione (Keil Studio)**:
1. File → Manage Libraries
2. Sync libraries
3. Rebuild progetto

### **Problema: "LCD non funziona"**

**Causa**: Indirizzo I2C errato

**Soluzione**: Modifica riga 69 di `main.cpp`:
```cpp
// Prova prima con 0x4E (default)
TextLCD lcd(PIN_LCD_SDA, PIN_LCD_SCL, 0x4E);

// Se non funziona, prova con 0x27:
TextLCD lcd(PIN_LCD_SDA, PIN_LCD_SCL, 0x27);
```

Per trovare l'indirizzo corretto:
```cpp
// Aggiungi questo nel main per scannerizzare I2C:
I2C i2c(D14, D15);
for(int addr = 0x20; addr < 0x80; addr += 2) {
    if(i2c.write(addr, "", 0) == 0) {
        printf("Device found at 0x%02X\n", addr);
    }
}
```

### **Problema: "BLE non connette"**

**Causa 1**: Shield BLE non montato correttamente
**Soluzione**: Verifica che tutti i pin Morpho siano inseriti

**Causa 2**: Nome BLE non trovato dall'app
**Soluzione**: Il nome BLE è `VendingM` (linea 577). Assicurati che l'app cerchi questo nome.

### **Problema: "LDR non rileva monete con luce accesa"**

**Causa**: Stai usando versione legacy con soglie assolute
**Soluzione**: Usa `main.cpp` (v8.14) con spike detection EMA adattivo

### **Problema: "Sonar mostra sempre 6cm"**

**Causa**: Sensore HC-SR04 troppo vicino al tavolo (riflesso superficie)
**Soluzione**:
1. Sposta sensore lontano da superfici riflettenti
2. Usa v8.14 che ha echoDuration reset (previene valori obsoleti)

### **Problema: "Sistema si blocca dopo alcuni minuti"**

**Causa**: Main loop bloccato o watchdog non attivo
**Soluzione**:
1. Usa v8.14 che ha watchdog 10s configurato
2. Verifica log seriale (115200 baud su USBTX/USBRX)

---

## 📊 **Monitoraggio Seriale**

Per vedere i log di debug:

```bash
# Linux/Mac:
screen /dev/ttyACM0 115200

# Windows (PuTTY):
# COM port: quello della Nucleo
# Baud: 115200
```

Output esempio (v8.14):
```
BOOT v8.14
[BLE] ✓ Dispositivo CONNESSO
[STATUS] BLE:ON | ATTESA_MONETA | €1 | P1@1EUR | LDR:47%(B:45 Δ:+2) | DIST:35cm | T:22°C H:48% | A5 S5 C5 T5
[STOCK] Rifornimento completato: 5 pezzi/prodotto
[BLE] Resto automatico per disconnessione: 1E
```

---

## 🔐 **Note di Sicurezza**

⚠️ **IMPORTANTE**: Il protocollo BLE attuale **non è cifrato**.

- Qualsiasi dispositivo può connettersi e inviare comandi
- I dati sensori sono trasmessi in chiaro
- Adatto solo per **uso didattico/demo**

**Per uso produzione**, implementare:
1. Pairing BLE con PIN code
2. Encryption LE Secure Connections
3. Autenticazione lato server

---

## 📚 **Risorse Utili**

- **Mbed OS Docs**: https://os.mbed.com/docs/mbed-os/
- **Nucleo F401RE**: https://os.mbed.com/platforms/ST-Nucleo-F401RE/
- **BLE Shield**: https://www.st.com/en/ecosystems/x-nucleo-idb05a1.html
- **TextLCD Lib**: https://os.mbed.com/users/simon/code/TextLCD/

---

## 🆘 **Supporto**

Problemi? Apri una issue su GitHub:
https://github.com/santoromarco74/VendingMachine/issues

**Documentazione Completa**:
- 📋 [BUGFIXES.md](../BUGFIXES.md) - Changelog dettagliato
- 🔌 [WIRING.md](../WIRING.md) - Schema collegamenti hardware
- 📱 [ANDROID_APP.md](../ANDROID_APP.md) - Guida app Android
- 📐 [ARCHITECTURE.md](../ARCHITECTURE.md) - Diagrammi e architettura

---

**Versione Firmware Corrente**: v8.14 (LCD Refill Feedback + Sonar Stable) **FINALE**
**Versione App Android**: v2.0 (Font migliorato + Documentazione IT)
**Data Ultimo Aggiornamento**: 2026-01-06
**Autore**: Marco Santoro
