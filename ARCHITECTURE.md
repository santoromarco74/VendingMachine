# 📐 Architettura e Diagrammi VendingMonitor v8.7 Final

Questa documentazione illustra graficamente il funzionamento del sistema VendingMonitor attraverso diagrammi esplicativi.

---

## 🔄 Diagramma FSM (Finite State Machine)

La macchina a stati finiti è il cuore del sistema. Gestisce tutti i flussi operativi del distributore.

```mermaid
stateDiagram-v2
    [*] --> RIPOSO: Boot Sistema

    RIPOSO --> ATTESA_MONETA: Utente si avvicina\n(distanza < 40cm)
    ATTESA_MONETA --> RIPOSO: Utente si allontana\n(distanza > 60cm, credito = 0)

    ATTESA_MONETA --> ATTESA_MONETA: Inserimento monete\n(LDR rileva moneta)

    ATTESA_MONETA --> RESTO: Timeout 30s\n(credito > 0, no conferma)
    ATTESA_MONETA --> RESTO: Annulla manuale\n(pulsante o BLE cmd 9)

    ATTESA_MONETA --> EROGAZIONE: Conferma acquisto\n(credito ≥ prezzo, BLE cmd 10)

    EROGAZIONE --> ATTESA_MONETA: Prodotto erogato\n(credito residuo > 0)
    EROGAZIONE --> RIPOSO: Prodotto erogato\n(credito = 0)

    RESTO --> RIPOSO: Resto completato\n(credito = 0)

    RIPOSO --> ERRORE: Temperatura alta\n(temp ≥ 28°C)
    ATTESA_MONETA --> ERRORE: Temperatura alta\n(temp ≥ 28°C)

    ERRORE --> RIPOSO: Temperatura OK\n(temp < 28°C)

    note right of RIPOSO
        LED: Verde
        LCD: "VENDING IoT"
        Sonar: campiona ogni 500ms
    end note

    note right of ATTESA_MONETA
        LED: Colore prodotto
        - Ciano: Acqua
        - Magenta: Snack
        - Giallo: Caffè
        - Verde: The
        Sonar: campiona ogni 5s
    end note

    note right of EROGAZIONE
        Servo: Attivo (90°→0°→90°)
        Buzzer: Suona
        Scorte: Decrementa
        Credito: Scala prezzo
    end note

    note right of RESTO
        Buzzer: Suona 2 volte
        Credito: Reset a 0
        LCD: "Resto: X EUR"
    end note

    note right of ERRORE
        LED: Rosso
        LCD: "ERRORE TEMP!"
        Sistema: Bloccato
    end note
```

---

## 🏗️ Architettura Sistema

```mermaid
graph TB
    subgraph "📱 App Android (Kotlin)"
        A[Activity MainActivity] --> B[BLE Manager]
        B --> C[GATT Client]
        C --> D[UI Compose]
    end

    subgraph "📡 Comunicazione BLE"
        E[Service 0xA000]
        E --> F1[Temp 0xA001 NOTIFY]
        E --> F2[Status 0xA002 NOTIFY]
        E --> F3[Hum 0xA003 NOTIFY]
        E --> F4[Cmd 0xA004 WRITE]
    end

    subgraph "🖥️ STM32 Nucleo F401RE (C++ Mbed OS)"
        G[Main Loop 100ms] --> H[FSM updateMachine]
        H --> I[Sensor Manager]
        I --> I1[HC-SR04 Thread]
        I --> I2[DHT11 Thread]
        I --> I3[LDR Polling]

        H --> J[Actuator Manager]
        J --> J1[Servo PWM]
        J --> J2[LCD I2C]
        J --> J3[LED RGB]
        J --> J4[Buzzer]

        H --> K[BLE Service]
        K --> L[GATT Server]
    end

    C -.BLE.-> L
    L -.NOTIFY.-> C

    style A fill:#4CAF50
    style G fill:#2196F3
    style E fill:#FF9800
```

---

## 📊 Flusso Operativo Utente

```mermaid
sequenceDiagram
    actor U as 👤 Utente
    participant S as 🔊 HC-SR04
    participant F as 🧠 FSM
    participant L as 🖥️ LCD
    participant LDR as 💡 LDR
    participant BLE as 📡 BLE
    participant APP as 📱 App
    participant SERVO as ⚙️ Servo

    Note over F: STATO: RIPOSO
    U->>S: Si avvicina (< 40cm)
    S->>F: distanza = 25cm
    F->>F: RIPOSO → ATTESA_MONETA
    F->>L: "Ins.Mon x ACQUA"
    Note over F: LED: Ciano

    U->>APP: Seleziona "SNACK"
    APP->>BLE: Cmd 0x02
    BLE->>F: idProdotto = 2
    F->>L: "Ins.Mon x SNACK"
    Note over F: LED: Magenta

    U->>LDR: Inserisce moneta 1
    LDR->>F: LDR > 25% (3 campioni)
    F->>F: credito = 1 EUR
    F->>BLE: STATUS [1, 1, 5, 5, 5, 5]
    BLE->>APP: Aggiorna credito

    U->>LDR: Inserisce moneta 2
    LDR->>F: LDR > 25%
    F->>F: credito = 2 EUR
    F->>L: "Premi CONFERMA!"
    F->>BLE: STATUS [2, 1, 5, 5, 5, 5]
    BLE->>APP: Aggiorna credito

    U->>APP: Preme "CONFERMA"
    APP->>BLE: Cmd 0x0A
    BLE->>F: Conferma acquisto
    F->>F: ATTESA_MONETA → EROGAZIONE
    F->>SERVO: Attiva (90° → 0° → 90°)
    Note over SERVO: Buzzer suona
    F->>F: scorte[2]--
    F->>F: credito = 0
    F->>L: "SNACK erogato!"

    F->>F: EROGAZIONE → RIPOSO
    F->>L: "VENDING IoT"
    Note over F: LED: Verde
```

---

## 🔌 Architettura Hardware Pins

```mermaid
graph LR
    subgraph "STM32 Nucleo F401RE"
        subgraph "Analog Pins"
            A1[A1 TRIG]
            A2[A2 LDR]
            A3[A3 LED_B]
        end

        subgraph "Digital Pins"
            D2[D2 BUZZER]
            D4[D4 DHT11]
            D5[D5 SERVO]
            D6[D6 LED_R]
            D8[D8 LED_G]
            D9[D9 ECHO]
            D14[D14 SDA]
            D15[D15 SCL]
        end

        subgraph "Onboard"
            PC13[PC_13 Button]
        end
    end

    A1 --> HC1[HC-SR04 Trigger]
    D9 --> HC2[HC-SR04 Echo]
    HC1 -.Ultrasuoni.-> HC2

    A2 --> LDR1[📡 LDR + 10kΩ]

    D4 <--> DHT[🌡️ DHT11]

    D5 --> SRV[⚙️ Servo SG90]

    D2 --> BUZ[🔊 Buzzer]

    D6 --> LEDR[🔴 LED R]
    D8 --> LEDG[🟢 LED G]
    A3 --> LEDB[🔵 LED B]

    D14 --> LCD1[🖥️ LCD I2C SDA]
    D15 --> LCD2[🖥️ LCD I2C SCL]

    PC13 --> BTN[🔘 Annulla]

    style HC1 fill:#FF5722
    style DHT fill:#4CAF50
    style SRV fill:#2196F3
    style LDR1 fill:#FFC107
```

---

## ⚡ Timing e Performance

```mermaid
gantt
    title Ciclo Operativo Sistema (1 secondo)
    dateFormat X
    axisFormat %L ms

    section Main Loop
    Kick Watchdog           :0, 1
    Leggi LDR              :1, 5

    section Sonar (RIPOSO)
    Campiona Distanza      :10, 80
    Filtro Anti-Spike      :90, 10

    section Sonar (ATTESA_MONETA)
    Idle (ogni 5s)         :0, 100

    section DHT11 Thread
    Lettura Temp/Hum       :500, 200

    section Display
    Update LCD             :100, 20
    Log Seriale (ogni 2s)  :200, 5

    section FSM
    updateMachine (100ms)  :0, 5
    updateMachine (100ms)  :100, 5
    updateMachine (100ms)  :200, 5
    updateMachine (100ms)  :300, 5
    updateMachine (100ms)  :400, 5
    updateMachine (100ms)  :500, 5
    updateMachine (100ms)  :600, 5
    updateMachine (100ms)  :700, 5
    updateMachine (100ms)  :800, 5
    updateMachine (100ms)  :900, 5
```

---

## 🧮 Algoritmo Filtro Anti-Spike Asimmetrico

```mermaid
flowchart TD
    A[Inizio leggiDistanza] --> B[Campiona 5 volte]
    B --> C{Campioni validi > 0?}

    C -->|NO| D[Return ultimaDistanzaValida]
    C -->|SI| E[Calcola media campioni]

    E --> F{media < ultimaDist - 150cm?}

    F -->|SI| G[🚫 SPIKE NEGATIVO<br/>Return ultimaDistanzaValida]
    F -->|NO| H[✅ VALIDO<br/>Aggiorna ultimaDistanzaValida]

    H --> I[Return media]

    G --> J[Fine]
    D --> J
    I --> J

    style G fill:#f44336,color:#fff
    style H fill:#4CAF50,color:#fff
    style D fill:#FF9800,color:#fff

    note1[Esempio ACCETTATO:<br/>17cm → 150cm<br/>media=150 > 17-150=-133]
    note2[Esempio BLOCCATO:<br/>200cm → 10cm<br/>media=10 < 200-150=50]

    F -.-> note1
    F -.-> note2
```

---

## 📦 Gestione Scorte

```mermaid
stateDiagram-v2
    [*] --> ScortePiene: Boot/Rifornimento

    ScortePiene --> ScorteDisponibili: Erogazione prodotto
    ScorteDisponibili --> ScortePiene: Comando BLE 11
    ScorteDisponibili --> ScorteDisponibili: Erogazione prodotto
    ScorteDisponibili --> ScorteEsaurite: scorte[i] = 0

    ScorteEsaurite --> ScorteDisponibili: Comando BLE 11

    note right of ScortePiene
        Array: [0, 5, 5, 5, 5]
        Tutti prodotti disponibili
        Selezione abilitata
    end note

    note right of ScorteDisponibili
        Array: [0, 3, 2, 5, 4]
        Alcuni prodotti disponibili
        Selezione abilitata
    end note

    note right of ScorteEsaurite
        Array: [0, 0, 2, 5, 4]
        Prodotto esaurito
        Selezione BLOCCATA
        LCD: "PRODOTTO ESAURITO!"
    end note
```

---

## 🔐 Sicurezza e Validazioni

```mermaid
flowchart TD
    A[Comando BLE ricevuto] --> B{Comando valido?<br/>1-4, 9, 10, 11}

    B -->|NO| C[🚫 IGNORA<br/>Log: SECURITY]
    B -->|SI| D{Tipo comando?}

    D -->|1-4<br/>Selezione| E{Scorte disponibili?}
    D -->|9<br/>Annulla| F{credito > 0?}
    D -->|10<br/>Conferma| G{Validazione tripla}
    D -->|11<br/>Rifornimento| H[✅ Reset scorte MAX]

    E -->|NO| I[🚫 BLOCCA<br/>Log: STOCK esaurito]
    E -->|SI| J[✅ Seleziona prodotto<br/>Cambia LED]

    F -->|NO| K[🚫 IGNORA]
    F -->|SI| L[✅ Vai a RESTO]

    G --> G1{stato = ATTESA_MONETA?}
    G1 -->|NO| M[🚫 RIFIUTA<br/>stato invalido]
    G1 -->|SI| G2{credito ≥ prezzo?}
    G2 -->|NO| N[🚫 RIFIUTA<br/>credito insufficiente]
    G2 -->|SI| G3{"scorte > 0?"}
    G3 -->|NO| O[🚫 BLOCCA<br/>scorte esaurite]
    G3 -->|SI| P[✅ Vai a EROGAZIONE]

    style C fill:#f44336,color:#fff
    style I fill:#f44336,color:#fff
    style K fill:#f44336,color:#fff
    style M fill:#f44336,color:#fff
    style N fill:#f44336,color:#fff
    style O fill:#f44336,color:#fff

    style J fill:#4CAF50,color:#fff
    style L fill:#4CAF50,color:#fff
    style P fill:#4CAF50,color:#fff
    style H fill:#4CAF50,color:#fff
```

---

## 📈 Ottimizzazioni v8.7 Final

### Sonar Campionamento Adattivo

| Stato FSM | Frequenza Campionamento | Motivazione |
|-----------|------------------------|-------------|
| **RIPOSO** | 500ms (2 Hz) | 🎯 Rileva presenza utente rapidamente |
| **ATTESA_MONETA** | 5s (0.2 Hz) | ⚡ Riduce overhead, utente già presente |
| **EROGAZIONE** | 5s (0.2 Hz) | ⚡ Non necessario durante erogazione |
| **RESTO** | 5s (0.2 Hz) | ⚡ Non necessario durante resto |

### LDR Debouncing Ottimizzato

**Prima (v8.5):**
- Campioni richiesti: 5
- Tempo minimo: 300ms
- ❌ Problema: valori oscillanti non rilevati

**Dopo (v8.7):**
- Campioni richiesti: **3** (riduzione 40%)
- Tempo minimo: **200ms** (riduzione 33%)
- ✅ Compensa oscillazioni LDR

### Log Seriale Compatto

**Prima (v8.5):**
```
╔════════════════════════════════════════════════════════════════╗
║ [STATUS] VendingMonitor v8.5 - Monitor Variabili Principali  ║
╠════════════════════════════════════════════════════════════════╣
║ STATO FSM:  ATTESA_MONETA                      ║
║ CREDITO:      2 EUR                                          ║
...
╚════════════════════════════════════════════════════════════════╝
```
**12 righe verticali**

**Dopo (v8.7):**
```
[STATUS] ATTESA_MONETA | €2 | P2@2EUR | LDR:25% | DIST:30cm | T:22°C H:48% | SCORTE: A5 S4 C5 T5
```
**1 riga orizzontale** (riduzione 92% spazio)

---

## 🎓 Conclusioni

Questo sistema dimostra:
- ✅ **FSM robusto** con gestione completa stati ed eventi
- ✅ **Comunicazione BLE** bidirezionale (comandi + notifiche)
- ✅ **Algoritmi ottimizzati** (filtro anti-spike asimmetrico, campionamento adattivo)
- ✅ **Validazioni di sicurezza** su scorte, credito, stati
- ✅ **Performance eccellenti** (loop 100ms, watchdog 10s)

**Versione:** v8.7 Final
**Autore:** Marco Santoro
**Data:** 2025-01-03
