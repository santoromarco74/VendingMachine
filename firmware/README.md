# 🔌 STM32 Firmware - Vending Machine IoT

Firmware per **ST Nucleo F401RE** con shield BLE **X-NUCLEO-IDB05A2**.

---

## 📦 **File Disponibili**

| File | Versione | Descrizione | Stato |
|------|----------|-------------|-------|
| `main.cpp` | **v7.2** | **VERSIONE CORRETTA** con tutti i fix applicati | ✅ **Raccomandato** |
| `main_v7.1_original.cpp` | v7.1 | Versione originale (solo per riferimento storico) | ⚠️ Deprecato |

---

## 🔧 **Differenze tra le Versioni**

### **v7.2 (CORRENTE - RACCOMANDATO)**

✅ **Bug Fixes Critici:**
- Debouncing LDR robusto (5 campioni + 300ms) per evitare conteggi multipli
- DHT11 spostato in thread separato (non blocca più main loop)
- Watchdog timer per recovery automatico da hang
- Validazione comandi BLE (solo 1-4 e 9 accettati)
- Fix buffer overflow LCD con `snprintf()`
- Fix timeout underflow con controllo esplicito

**Changelog completo**: Vedi `../BUGFIXES.md`

### **v7.1 (ORIGINALE - DEPRECATO)**

⚠️ **Problemi Noti:**
- LDR può contare una moneta 2-3 volte
- `thread_sleep_for(18ms)` blocca main loop durante lettura DHT11
- `__disable_irq()` disabilita anche stack BLE
- Nessun watchdog (freeze irrecuperabili)
- Accetta comandi BLE arbitrari

**Usa solo per confronto o debug.**

---

## 🚀 **Come Compilare**

### **Requisiti**

- **IDE**: Keil Studio Cloud oppure Mbed Studio
- **Toolchain**: ARM GCC 10.x o superiore
- **Mbed OS**: 6.x (testato con 6.15.0)
- **Librerie**:
  - `mbed-os`
  - `TextLCD` (per display I2C)
  - `X_NUCLEO_IDB05A1` (stack BLE)

### **Opzione 1: Keil Studio Cloud (Online)**

1. Vai su https://studio.keil.arm.com/
2. Crea nuovo progetto: **"Mbed OS 6 Blinky"**
3. Sostituisci `main.cpp` con il contenuto di questo file
4. Aggiungi libreria `TextLCD`:
   ```
   Pannello "Libraries" → Add Library → cerca "TextLCD"
   ```
5. Aggiungi libreria BLE:
   ```
   Pannello "Libraries" → Add Library → cerca "X_NUCLEO_IDB05A1"
   ```
6. **Target**: Seleziona `NUCLEO_F401RE`
7. Click su **"Build"** (icona martello)
8. Scarica il `.bin` generato

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

### **Problema: "Error: Could not compile"**

**Causa**: Librerie mancanti o versione Mbed OS errata

**Soluzione**:
```bash
mbed add https://github.com/mbedmicro/TextLCD
mbed add https://github.com/ARMmbed/mbed-os-ble-utils
```

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

### **Problema: "Monete contate multiple volte"**

**Causa**: Stai usando v7.1 invece di v7.2
**Soluzione**: Usa `main.cpp` (v7.2) che ha il debouncing robusto

### **Problema: "Sistema si blocca dopo alcuni minuti"**

**Causa**: Watchdog assente (v7.1) o main loop bloccato
**Soluzione**:
1. Usa v7.2 che ha il watchdog
2. Verifica log seriale (9600 baud su USBTX/USBRX)

---

## 📊 **Monitoraggio Seriale**

Per vedere i log di debug:

```bash
# Linux/Mac:
screen /dev/ttyACM0 9600

# Windows (PuTTY):
# COM port: quello della Nucleo
# Baud: 9600
```

Output esempio:
```
BOOT v7.2 FIXED
[LDR] Moneta rilevata! Credito=1
[DHT] Temp=25C Hum=60%
[SECURITY] Comando BLE invalido ricevuto: 0xFF
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
https://github.com/santoromarco74/VendingMonitor/issues

---

**Versione Firmware Corrente**: v7.2 (Golden Master Fixed)
**Data Ultimo Aggiornamento**: 2025-12-22
**Autore**: Marco Santoro
