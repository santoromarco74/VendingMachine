# 🔌 Schema Elettrico & Cablaggio - Vending Machine IoT

Questo documento descrive in dettaglio come collegare i componenti hardware alla scheda **STM32 Nucleo F401RE** con shield **X-NUCLEO-IDB05A2**.

## ⚡ Alimentazione
Il sistema è progettato per essere alimentato tramite la porta USB della Nucleo.
* **5V Line:** Alimenta Servo, LCD e Ultrasuoni.
* **3.3V Line:** Alimenta la logica della Nucleo e sensori a basso voltaggio (DHT11, LDR).
* **GND:** Tutti i ground devono essere in comune.

---

## 🗺️ Layout Grafico (Breadboard View)

```text
                        NUCLEO F401RE
                  +-----------------------+
                  | [USB]                 |
                  |                       |
      (LCD SDA) --| D14               A1  |-- (HC-SR04 Trig)
      (LCD SCL) --| D15               A2  |-- (LDR Signal)
                  | ...               A3  |-- (LED Blue)
   (Sonar Echo) --| D9                ... |
    (LED Green) --| D8                ... |
                  | GND               3.3V|-- [Alim. Sensori 3.3V]
      (LED Red) --| D6                5V  |-- [Alim. Servo/LCD 5V]
     (Servo Sig)--| D5                GND |-- [COMUNE A TUTTI]
     (DHT11 Sig)--| D4                ... |
                  | ...               ... |
   (Buzzer Sig) --| D2                ... |
                  +-----------------------+


| Pin LCD | Pin Nucleo | Descrizione                  |
|---------|------------|------------------------------|
| GND     | GND        | Comune                       |
| VCC     | 5V         | Necessario per contrasto ottimale |
| SDA     | D14 (PB9)  | I2C Data                     |
| SCL     | D15 (PB8)  | I2C Clock                    |

Pin Sensore,Pin Nucleo,Descrizione
VCC,5V,Alimentazione
Trig,A1 (PA1),Output Trigger (10us pulse)
Echo,D9 (PC7),Input Echo (Misura tempo)
GND,GND,Comune

Colore Filo,Pin Nucleo,Descrizione
Marrone,GND,Comune
Rosso,5V,Potenza Motore
Arancio,D5 (PB4),Segnale PWM

---

## 🔋 **Alimentazione Esterna (Alimentatore 5V)**

### ⚠️ **PROBLEMA COMUNE: "Non funziona con alimentatore esterno"**

Se alimenti il circuito con alimentatore esterno e la Nucleo con USB PC, il sistema **non funziona** perché le **masse (GND) non sono comuni**!

### ✅ **SOLUZIONE: Collegare le Masse (GND)**

**REGOLA FONDAMENTALE**: Quando usi due alimentatori separati, devi **SEMPRE** collegare le masse insieme.

```
┌──────────────┐              ┌──────────────────┐
│ PC USB       │              │ ALIMENTATORE 5V  │
│              │              │    ESTERNO       │
│         GND ●┼──────────────┼● GND             │
│              │              │                  │
│              │              │         +5V ●────┼──→ VCC Circuito
│              │      ✗       │                  │     (LCD, Servo,
│         +5V  │   NON COLL.  │                  │      Ultrasuoni)
└──────┬───────┘              └──────────────────┘
       │
       ↓ USB
┌──────────────┐
│ NUCLEO F401  │
│              │
│ GND ●────────┼──→ COMUNE A TUTTI I COMPONENTI
│              │
│ 5V (NC)      │     ← NON usato (già alimentato da circuito)
└──────────────┘
```

**IMPORTANTE:**
- ✅ **Collega GND** della Nucleo con GND dell'alimentatore esterno
- ❌ **NON collegare** +5V della Nucleo con +5V dell'alimentatore (creerebbe conflitto!)
- ✅ Alimenta **SOLO il circuito** (LCD, Servo, Ultrasuoni) dall'alimentatore esterno
- ✅ Alimenta la **Nucleo** solo da USB PC (per programmazione/debug)

### 🔴 **LED LD1 Rosso Lampeggia?**

Se il LED LD1 sulla Nucleo lampeggia rosso quando usi alimentatore esterno, indica:

| Sintomo | Causa | Soluzione |
|---------|-------|-----------|
| **Lampeggio lento** | Comunicazione USB normale | ✅ OK, nessun problema |
| **Lampeggio veloce** | Tensione instabile o overcurrent | ⚠️ Controlla alimentatore |
| **Fisso rosso** | Errore critico alimentazione | ❌ Controlla cablaggi |

**Verifiche da fare:**
1. **Alimentatore adeguato**: Usa alimentatore **5V/2A** (minimo 1.5A)
2. **Tensione stabile**: Misura con multimetro che sia **5.0V ± 0.25V** (4.75-5.25V)
3. **GND comune**: Verifica continuità con tester tra GND Nucleo e GND alimentatore
4. **Corrente disponibile**: Il sistema richiede ~1A (servo + LCD + sensori)

### 📊 **Requisiti Alimentazione**

| Componente | Tensione | Corrente Tipica | Corrente Max |
|------------|----------|-----------------|--------------|
| STM32 Nucleo F401RE | 5V USB | 100 mA | 500 mA |
| LCD 16x2 I2C | 5V | 20 mA | 80 mA |
| Servo SG90 | 5V | 100-300 mA | 650 mA |
| HC-SR04 Ultrasuoni | 5V | 15 mA | 30 mA |
| DHT11 (Temp/Umidità) | 3.3V | 0.5 mA | 2.5 mA |
| LDR + Resistore | 3.3V | 1 mA | 5 mA |
| LED RGB | 3.3V | 30 mA | 60 mA |
| Buzzer | 3.3V | 20 mA | 30 mA |
| **TOTALE SISTEMA** | **5V** | **~500 mA** | **~1.5 A** |

**Raccomandazione**: Usa alimentatore **5V/2A** per margine di sicurezza.

### 🛠️ **Configurazioni Alimentazione**

#### **Configurazione 1: Solo USB PC (Debug/Sviluppo)**
```
PC USB ──→ Nucleo F401RE ──→ Circuito (5V/3.3V)
           ├─ Programmazione ✓
           ├─ Debug seriale ✓
           └─ Alimentazione completa ✓
```
**Vantaggi**: Tutto in uno, semplice
**Limiti**: Max 500mA da USB (potrebbe non bastare per servo in movimento)

#### **Configurazione 2: USB PC + Alimentatore Esterno (SETUP CORRENTE)**
```
PC USB ──→ Nucleo F401RE (solo logica + debug)
                  │
                  │ GND ←─────┐ COLLEGAMENTO CRITICO!
                  │           │
Alimentatore 5V ──┴──→ Circuito (LCD, Servo, Sensori)
```
**Vantaggi**: Debug seriale + alimentazione potente per servo
**Setup**:
1. Collega Nucleo a USB PC
2. Collega GND Nucleo a GND alimentatore esterno
3. Collega +5V alimentatore a VCC circuito (LCD, Servo, Ultrasuoni)
4. **NON** collegare +5V alimentatore a pin 5V della Nucleo

#### **Configurazione 3: Solo Alimentatore Esterno (Produzione)**
```
Alimentatore 5V ──→ Nucleo F401RE (via USB o Vin) ──→ Circuito
                    └─ Operazione standalone ✓
```
**Vantaggi**: Sistema autonomo, niente PC
**Limiti**: Niente debug seriale USB (usa solo LCD)

### 🔍 **Diagnosi Problemi Alimentazione**

| Problema | Sintomo | Causa Probabile | Soluzione |
|----------|---------|-----------------|-----------|
| **Non funziona con alim. esterno** | Sistema "morto" | GND non collegati | Collega GND insieme! |
| **LED rosso lampeggia veloce** | Instabilità | Tensione fluttuante | Alimentatore migliore |
| **Servo non si muove** | Niente movimento | Corrente insufficiente | Alimentatore ≥2A |
| **LCD spento/illeggibile** | Schermo nero | Tensione troppo bassa | Verifica 5V stabile |
| **Reset casuali** | Riavvii improvvisi | Calo tensione | Condensatore 470µF |

### 💡 **Miglioramento: Condensatore di Disaccoppiamento**

Per stabilizzare l'alimentazione (specialmente con servo), aggiungi:
- **Condensatore elettrolitico 470µF/16V** tra +5V e GND (vicino al servo)
- **Condensatore ceramico 100nF** tra +3.3V e GND (vicino alla Nucleo)

Questo elimina picchi di corrente quando il servo si muove.

---

