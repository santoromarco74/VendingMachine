# VendingMonitor - Mappa Pin Ottimizzata

**Target:** STM32 Nucleo F401RE
**Versione Firmware:** v8.9+
**Data:** 2025-01-01

---

## 📌 Schema Raggruppamento Pin

I pin sono organizzati per **device** per semplificare il cablaggio e ridurre errori.

### **GRUPPO 1: LCD I2C (Pin Hardware Fissi)**
| Funzione | Pin Nucleo | Note |
|----------|-----------|------|
| SDA | **D14** | Pin I2C hardware (non modificabile) |
| SCL | **D15** | Pin I2C hardware (non modificabile) |

**Indirizzo I2C:** 0x4E (0x27 << 1)
**Dispositivo:** LCD 16x2 con modulo I2C PCF8574

---

### **GRUPPO 2: Tastiera a Membrana 4x4 (8 pin ben raggruppati)**

#### Righe (4 pin digitali consecutivi)
| Riga | Pin Nucleo | Configurazione |
|------|-----------|----------------|
| ROW 1 | **D10** | DigitalOut |
| ROW 2 | **D11** | DigitalOut |
| ROW 3 | **D12** | DigitalOut |
| ROW 4 | **D13** | DigitalOut |

#### Colonne (4 pin digitali consecutivi)
| Colonna | Pin Nucleo | Configurazione |
|---------|-----------|----------------|
| COL 1 | **D2** | DigitalIn con PullUp |
| COL 2 | **D3** | DigitalIn con PullUp |
| COL 3 | **D4** | DigitalIn con PullUp |
| COL 4 | **D5** | DigitalIn con PullUp |

**Layout Tastiera 4x4 Standard:**
```
┌─────┬─────┬─────┬─────┐
│  1  │  2  │  3  │  A  │  ← Selezione prodotti + ANNULLA
├─────┼─────┼─────┼─────┤
│  4  │  5  │  6  │  B  │  ← THE + riservati
├─────┼─────┼─────┼─────┤
│  7  │  8  │  9  │  C  │  ← Riservati
├─────┼─────┼─────┼─────┤
│  *  │  0  │  #  │  D  │  ← Riservati + CONFERMA
└─────┴─────┴─────┴─────┘
```

**Mappatura Tasti:**
- **1-4**: Selezione prodotti (ACQUA, SNACK, CAFFE, THE)
- **A**: ANNULLA e resto
- **D**: CONFERMA acquisto
- **5-9, B, C, *, 0, #**: Riservati per funzioni future

**Stato:** `KEYPAD_ENABLED 0` (disabilitato fino al collegamento hardware)

---

### **GRUPPO 3: LED RGB (3 pin digitali consecutivi)**
| Canale | Pin Nucleo | Colore | GND |
|--------|-----------|--------|-----|
| R (Red) | **D6** | Rosso | ⚠️ **IMPORTANTE: Collegare GND!** |
| G (Green) | **D7** | Verde | ⚠️ **IMPORTANTE: Collegare GND!** |
| B (Blue) | **D8** | Blu | ⚠️ **IMPORTANTE: Collegare GND!** |

**Nota Critica:** LED RGB **deve** essere collegato a GND stabile. Se manca GND causa ground bounce → corruzione LCD!

---

### **GRUPPO 4: HC-SR04 Sensore Ultrasuoni (2 pin consecutivi)**
| Funzione | Pin Nucleo | Tipo |
|----------|-----------|------|
| TRIG | **A1** | DigitalOut |
| ECHO | **A2** | InterruptIn |

**Range:** 2-400 cm
**Campionamento:** Ogni 2 secondi (ridotto overhead)
**Filtri attivi:** Anti-spike, range validation, memoria ultima distanza valida

---

### **GRUPPO 5: Sensori Base (3 pin)**
| Sensore | Pin Nucleo | Tipo | Note |
|---------|-----------|------|------|
| LDR | **A0** | AnalogIn | Rilevamento monete (DEVE essere su pin Ax) |
| DHT11 | **A3** | DigitalInOut | Temp + Umidità |
| BUZZER | **A4** | DigitalOut | Feedback audio |

**DHT11:** Thread separato, lettura ogni 2s
**LDR:** Debouncing 300ms, 5 campioni consecutivi
**BUZZER:** Attivo durante erogazione e resto

---

### **GRUPPO 6: Servo (1 pin PWM isolato)**
| Funzione | Pin Nucleo | Tipo |
|----------|-----------|------|
| SERVO | **D9** | PwmOut |

**Periodo PWM:** 20ms
**Posizioni:** 0.05 (riposo), 0.10 (erogazione)
**Utilizzo:** Dispensa prodotti durante stato EROGAZIONE

---

### **FISSO: Pulsante Annulla**
| Funzione | Pin Nucleo | Tipo |
|----------|-----------|------|
| ANNULLA | **PC_13** | DigitalIn |

**Nota:** Pulsante USER integrato sulla Nucleo (non modificabile)

---

## 🔄 Modifiche da v8.8 → v8.9

### Cambiamenti Principali:
- **Tastiera**: Corretta da 4x3 (7 pin) a 4x4 (8 pin)
- **Colonne tastiera**: Spostate da pin analogici (A0-A2) a pin digitali (D2-D5)
- **Sensori base**: Riorganizzati per liberare pin digitali per tastiera

### Pin Spostati:
| Device | v8.8 | v8.9 | Motivo |
|--------|------|------|--------|
| Tastiera COL1 | A0 | **D2** | Tastiera 4x4: 4 colonne su pin digitali |
| Tastiera COL2 | A1 | **D3** | Tastiera 4x4: colonne consecutive D2-D5 |
| Tastiera COL3 | A2 | **D4** | Tastiera 4x4: colonne consecutive D2-D5 |
| Tastiera COL4 | - | **D5** | **NUOVA**: quarta colonna per tastiera 4x4 |
| Sonar TRIG | A4 | **A1** | Liberato spazio per tastiera |
| Sonar ECHO | A5 | **A2** | Sonar rimane consecutivo |
| LDR | A3 | **A0** | LDR DEVE essere su pin analogico |
| DHT11 | D2 | **A3** | Liberato D2 per tastiera |
| BUZZER | D3 | **A4** | Liberato D3 per tastiera |

### Tasti Funzionali Cambiati:
| Funzione | v8.8 | v8.9 |
|----------|------|------|
| ANNULLA | * | **A** |
| CONFERMA | # | **D** |

### Pin Invariati:
- **D6, D7, D8** (LED RGB)
- **D9** (SERVO)
- **D10, D11, D12, D13** (Tastiera righe)
- **D14, D15** (LCD I2C - pin hardware fissi)
- **PC_13** (Pulsante USER)

---

## 📊 Riepilogo Utilizzo Pin

### Pin Digitali:
- **D2-D5:** Tastiera colonne (4 pin)
- **D6-D8:** LED RGB (R, G, B)
- **D9:** SERVO
- **D10-D13:** Tastiera righe (4 pin)
- **D14-D15:** LCD I2C (SDA, SCL)

### Pin Analogici:
- **A0:** LDR
- **A1-A2:** HC-SR04 (TRIG, ECHO)
- **A3:** DHT11
- **A4:** BUZZER

### Pin Speciali:
- **PC_13:** Pulsante USER (ANNULLA)

---

## ⚡ Vantaggi Ottimizzazione

### ✅ Cablaggio Semplificato
- **Tastiera 4x4:** 8 pin ben raggruppati (D2-5 + D10-13, solo 2 cavi ribbon)
- **LED RGB:** 3 pin consecutivi (D6-7-8)
- **Sonar:** 2 pin consecutivi (A1-2)
- **Sensori:** Pin analogici disponibili ben distribuiti

### ✅ Riduzione Errori
- Facile identificare gruppi di cavi per device
- Difficile invertire pin all'interno dello stesso gruppo
- Meno crossover di cavi sulla breadboard

### ✅ Debugging Facilitato
- Ogni device ha i suoi pin raggruppati
- Facile tracciare segnali con oscilloscopio
- Troubleshooting più rapido

---

## 🛠️ Note di Implementazione

### Per Abilitare la Tastiera:
```cpp
#define KEYPAD_ENABLED 1  // Cambia da 0 a 1
```

### Verifica Pin PWM:
Il **D9** supporta PWM sulla Nucleo F401RE (Timer TIM1_CH1).
Altri pin PWM disponibili: D3, D5, D6, D10, D11

### Verifica Pull-up:
Le colonne della tastiera usano pull-up **interni** della STM32.
Non servono resistenze esterne.

---

## 📝 Schema di Connessione Breadboard

```
STM32 Nucleo F401RE
┌─────────────────────┐
│                     │
│  D14 ─────────────→ LCD SDA
│  D15 ─────────────→ LCD SCL
│                     │
│  D10 ─┐
│  D11 ─┤
│  D12 ─┼──────────→ Tastiera Righe
│  D13 ─┘
│  A0  ─┐
│  A1  ─┼──────────→ Tastiera Colonne (PullUp)
│  A2  ─┘
│                     │
│  D6  ─┐
│  D7  ─┼──────────→ LED RGB (R, G, B)
│  D8  ─┘
│                     │
│  A4  ─────────────→ HC-SR04 TRIG
│  A5  ─────────────→ HC-SR04 ECHO
│                     │
│  D2  ─────────────→ DHT11 (Data)
│  D3  ─────────────→ BUZZER (+)
│  A3  ─────────────→ LDR (via partitore)
│                     │
│  D9  ─────────────→ SERVO (Signal)
│                     │
│  PC_13 ────────────  Pulsante USER (ANNULLA)
│                     │
└─────────────────────┘
```

---

## 🔧 Checklist Pre-Test

- [ ] Verificato tutti gli **8 pin della tastiera 4x4** (4 righe D10-13 + 4 colonne D2-5)
- [ ] Collegato GND del LED RGB (⚠️ CRITICO - prevenzione ground bounce)
- [ ] Verificato alimentazione 5V per HC-SR04 (TRIG=A1, ECHO=A2)
- [ ] Verificato partitore resistivo LDR su A0 (10kΩ consigliato)
- [ ] Collegato alimentazione separata SERVO (5V esterno se >4 prodotti)
- [ ] Verificato indirizzo I2C LCD (0x4E o 0x27)
- [ ] Verificato DHT11 su A3 (può funzionare 3.3V-5V)
- [ ] Verificato BUZZER su A4
- [ ] Impostato `KEYPAD_ENABLED 1` se tastiera 4x4 collegata
- [ ] Testato tasti A (ANNULLA) e D (CONFERMA)

---

**Documento generato automaticamente da v8.9**
**Ultimo aggiornamento:** 2025-01-01
