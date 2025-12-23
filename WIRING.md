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

