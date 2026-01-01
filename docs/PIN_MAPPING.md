# VendingMonitor - Mappa Pin Ottimizzata

**Target:** STM32 Nucleo F401RE
**Versione Firmware:** v8.8+
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

### **GRUPPO 2: Tastiera a Membrana 4x3 (7 pin raggruppati)**

#### Righe (4 pin digitali consecutivi)
| Riga | Pin Nucleo | Configurazione |
|------|-----------|----------------|
| ROW 1 | **D10** | DigitalOut |
| ROW 2 | **D11** | DigitalOut |
| ROW 3 | **D12** | DigitalOut |
| ROW 4 | **D13** | DigitalOut |

#### Colonne (3 pin analogici consecutivi)
| Colonna | Pin Nucleo | Configurazione |
|---------|-----------|----------------|
| COL 1 | **A0** | DigitalIn con PullUp |
| COL 2 | **A1** | DigitalIn con PullUp |
| COL 3 | **A2** | DigitalIn con PullUp |

**Layout Tastiera:**
```
┌─────┬─────┬─────┐
│  1  │  2  │  3  │  ← Selezione prodotti
├─────┼─────┼─────┤
│  4  │  5  │  6  │  ← THE + futuri
├─────┼─────┼─────┤
│  7  │  8  │  9  │  ← Riservati
├─────┼─────┼─────┤
│  *  │  0  │  #  │  ← ANNULLA | riservato | CONFERMA
└─────┴─────┴─────┘
```

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

### **GRUPPO 4: HC-SR04 Sensore Ultrasuoni (2 pin analogici consecutivi)**
| Funzione | Pin Nucleo | Tipo |
|----------|-----------|------|
| TRIG | **A4** | DigitalOut |
| ECHO | **A5** | InterruptIn |

**Range:** 2-400 cm
**Campionamento:** Ogni 2 secondi (ridotto overhead)
**Filtri attivi:** Anti-spike, range validation, memoria ultima distanza valida

---

### **GRUPPO 5: Sensori Base (3 pin vicini)**
| Sensore | Pin Nucleo | Tipo | Note |
|---------|-----------|------|------|
| DHT11 | **D2** | DigitalInOut | Temp + Umidità |
| BUZZER | **D3** | DigitalOut | Feedback audio |
| LDR | **A3** | AnalogIn | Rilevamento monete |

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

## 🔄 Modifiche da v8.7 → v8.8

### Pin Spostati:
| Device | Vecchio | Nuovo | Motivo |
|--------|---------|-------|--------|
| Tastiera COL2 | D7 | **A1** | Colonne consecutive A0-A2 |
| Tastiera COL3 | A0 | **A2** | Colonne consecutive A0-A2 |
| LED Green | D8 | **D7** | LED RGB consecutivi D6-D8 |
| LED Blue | A3 | **D8** | LED RGB consecutivi D6-D8 |
| Sonar TRIG | A1 | **A4** | Sonar consecutivo A4-A5 |
| Sonar ECHO | D9 | **A5** | Sonar consecutivo A4-A5 |
| DHT11 | D4 | **D2** | Sensori base vicini |
| BUZZER | D2 | **D3** | Sensori base vicini |
| LDR | A2 | **A3** | Sensori base vicini |
| SERVO | D5 | **D9** | Pin PWM disponibile |

### Pin Invariati:
- **D14, D15** (LCD I2C - pin hardware fissi)
- **D10, D11, D12, D13** (Tastiera righe)
- **PC_13** (Pulsante USER)

---

## 📊 Riepilogo Utilizzo Pin

### Pin Digitali:
- **D2:** DHT11
- **D3:** BUZZER
- **D6-D8:** LED RGB (R, G, B)
- **D9:** SERVO
- **D10-D13:** Tastiera righe (4 pin)
- **D14-D15:** LCD I2C (SDA, SCL)

### Pin Analogici:
- **A0-A2:** Tastiera colonne (3 pin)
- **A3:** LDR
- **A4-A5:** HC-SR04 (TRIG, ECHO)

### Pin Speciali:
- **PC_13:** Pulsante USER (ANNULLA)

---

## ⚡ Vantaggi Ottimizzazione

### ✅ Cablaggio Semplificato
- **Tastiera:** 7 pin quasi tutti contigui (D10-13 + A0-2)
- **LED RGB:** 3 pin consecutivi (D6-7-8)
- **Sonar:** 2 pin consecutivi (A4-5)
- **Sensori:** 3 pin vicini (D2-3, A3)

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

- [ ] Verificato tutti i 7 pin della tastiera (righe + colonne)
- [ ] Collegato GND del LED RGB (⚠️ CRITICO)
- [ ] Verificato alimentazione 5V per HC-SR04
- [ ] Verificato partitore resistivo LDR (10kΩ consigliato)
- [ ] Collegato alimentazione separata SERVO (5V esterno se >4 prodotti)
- [ ] Verificato indirizzo I2C LCD (0x4E o 0x27)
- [ ] Impostato `KEYPAD_ENABLED 1` se tastiera collegata

---

**Documento generato automaticamente da v8.8**
**Ultimo aggiornamento:** 2025-01-01
