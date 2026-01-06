# 🔧 Bug Fixes & Improvements

**Ultimo aggiornamento**: 2026-01-06
**Versione App**: v2.0
**Versione Firmware**: v8.13

---

## 🎓 **FIRMWARE v8.13 - Sonar Stable Restore** (2026-01-06)

### ✅ **LESSON LEARNED: "Bug" era Problema Fisico**

**Problema Riportato**:
> "il sonar misura sempre 6 cm"

**Causa REALE (non bug!)**:
🔴 **Sensore HC-SR04 posizionato al centro del tavolo** → riflesso superficie a 6 cm

**Analisi**:
Il sensore funzionava correttamente fin dall'inizio! I "6 cm costanti" erano la distanza reale dal tavolo.

**Prova**:
Spostando il sensore lontano dal tavolo (verso aria libera):
- ✅ Distanza passa a 135-137 cm (corretto!)
- ✅ Avvicinando mano: 31 cm → 15 cm (corretto!)
- ✅ FSM RIPOSO ↔ ATTESA_MONETA funziona (corretto!)

**Fix v8.13**:
```cpp
// Versione STABILE ripristinata (v8.11 timing)
for(int i=0; i<5; i++) {
    echoDuration = 0;  // ✅ UNICO fix necessario (previene valori obsoleti)

    trig = 0; wait_us(2);
    trig = 1; wait_us(10);
    trig = 0;
    wait_us(15000);  // Timing originale funzionante

    if (echoDuration > 0 && echoDuration < 30000) {
        int distanza = (int)(echoDuration * 0.0343f / 2.0f);
        if (distanza >= 2 && distanza <= 400) {
            somma += distanza;
            validi++;
        }
    }
    // Nessuna pausa tra misure (funzionava già)
}
```

**Rollback da v8.12**:
- ❌ Rimosso `thread_sleep_for(60)` → causava timeout ISR random
- ❌ Rimosso timeout 25ms → 15ms sufficiente e più stabile
- ❌ Rimosso debug logging verboso → output pulito

**Lesson Learned**:
> "Measure twice, check hardware once before fixing software!"
> Il problema era il **montaggio fisico**, non il codice.

**Commit**: `fix: Ripristinata versione sonar stabile + echoDuration=0`

---

## 🎯 **FIRMWARE v8.12 - Sonar HC-SR04 (DEPRECATO)** (2026-01-06)

**⚠️ ATTENZIONE**: Questa versione è deprecata. Usare v8.13.

**Causa Root Errata**:
1. ❌ **echoDuration non azzerata**: La variabile `volatile uint64_t echoDuration` NON veniva azzerata prima di ogni nuova misura
2. ❌ **Valori obsoleti**: Se il sensore non rispondeva (ISR non chiamata), echoDuration manteneva il valore della misura precedente (~350μs)
3. ❌ **Timing insufficiente**: Solo 15ms di pausa tra misure, ma HC-SR04 richiede 60ms di "quiet time"
4. ❌ **Timeout corto**: 15ms non sufficiente per range completo 400cm (servono 23ms)

**Analisi Dettagliata**:
```cpp
// PRIMA (BUGGY) - firmware/main.cpp:566-590
for(int i=0; i<5; i++) {
    // ❌ echoDuration NON azzerata -> mantiene valore precedente!
    trig = 0;
    wait_us(5);
    trig = 1;
    wait_us(15);  // Impulso trigger
    trig = 0;

    wait_us(15000);  // ❌ Solo 15ms timeout (insufficiente per 400cm)

    if (echoDuration > 0 && echoDuration < 50000) {
        int distanza = (int)(echoDuration * 0.0343f / 2.0f);
        if (distanza >= 2 && distanza <= 400) {
            somma += distanza;
            validi++;
        }
    }
    // ❌ Nessuna pausa tra misure -> viola spec HC-SR04!
}
```

**Scenario Bug**:
1. Prima misura: echoDuration = 350μs → 6 cm ✓ (valore corretto)
2. Seconda misura: sensore non risponde → echoDuration rimane 350μs
3. Terza misura: sensore non risponde → echoDuration rimane 350μs
4. Risultato: media sempre ~6 cm anche se distanza reale è diversa!

**Fix Applicato**:
```cpp
// DOPO (FIXED) - firmware/main.cpp:566-612
for(int i=0; i<5; i++) {
    // ✅ FIX 1: Azzera echoDuration prima di ogni misura
    echoDuration = 0;

    trig = 0;
    wait_us(5);
    trig = 1;
    wait_us(15);
    trig = 0;

    // ✅ FIX 2: Timeout aumentato 15ms → 25ms (copre 400cm)
    wait_us(25000);

    if (echoDuration > 0 && echoDuration < 50000) {
        int distanza = (int)(echoDuration * 0.0343f / 2.0f);
        if (distanza >= 2 && distanza <= 400) {
            somma += distanza;
            validi++;
        }
    }

    // ✅ FIX 3: Pausa 60ms tra misure (spec HC-SR04)
    if (i < 4) {
        thread_sleep_for(60);
    }
}
```

**Modifiche Tecniche**:
| Fix | Parametro | Prima | Dopo | Motivo |
|-----|-----------|-------|------|--------|
| 1 | echoDuration reset | ❌ No | ✅ Sì (=0) | Previene valori obsoleti |
| 2 | Timeout echo | 15 ms | 25 ms | Range 400cm = 23ms teorici |
| 3 | Pausa tra misure | 0 ms | 60 ms | Spec HC-SR04 quiet time |
| 4 | Debug logging | ❌ No | ✅ Sì | Diagnostica hardware |

**Debug Logging Aggiunto**:
```cpp
// Identifica timeout (echoDuration = 0)
printf("[SONAR] Campione %d: TIMEOUT - echoDuration=0 (ISR non chiamata? Verificare ECHO collegato)\n", i+1);

// Mostra letture parziali
printf("[SONAR DEBUG] Solo %d/5 letture valide | Media: %dcm | echoDuration: %llu μs\n", validi, media, echoDuration);

// Allarme tutte misure fallite
printf("[SONAR DEBUG] Tutte le 5 letture fallite! echoDuration rimasta a 0 - Possibile problema hardware\n");
```

**Test Consigliati**:
1. ✅ Verifica cablaggio: TRIG=A1, ECHO=D9, VCC=5V, GND=GND
2. ✅ Compila e carica firmware v8.12
3. ✅ Osserva serial log: deve mostrare distanze variabili (non fisso 6cm)
4. ✅ Testa range: avvicina mano da 5cm a 100cm, distanza deve cambiare
5. ⚠️ Se echoDuration sempre 0: problema hardware (ECHO disconnesso o ISR non configurati)
6. ⚠️ Se echoDuration costante: possibile short circuit su pin ECHO

**Impatto**: 🔴 CRITICAL - Rilevamento presenza utente non funzionante → FSM bloccato in RIPOSO

**Commit**: `fix: Risolto sensore HC-SR04 bloccato a 6cm con timing corretto`

---

## 📱 Android App (MainActivity.kt)

### 🔴 **CRITICAL FIXES**

#### 1. **Bug Conversione Byte Signed → Unsigned** (MainActivity.kt:386-390)

**Problema**:
```kotlin
// PRIMA (BUGGY)
creditState = data[0].toInt()  // ❌ Byte signed: -128 a 127
machineState = data[1].toInt()
```

Se `data[0] = 0x96` (150 in decimale), veniva interpretato come `-106` perché `Byte` in Kotlin è signed.

**Fix Applicato**:
```kotlin
// DOPO (FIXED)
creditState = data[0].toInt() and 0xFF  // ✅ Unsigned: 0 a 255
machineState = data[1].toInt() and 0xFF
```

**Impatto**: CRITICO - Il credito mostrato poteva essere negativo o errato

---

#### 2. **Validazione Size Dati BLE** (MainActivity.kt:377-385)

**Problema**:
```kotlin
// PRIMA (BUGGY)
if (uuid == CHAR_TEMP_UUID) {
    val temp = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN).int
    // ❌ Nessun controllo su data.size -> crash se < 4 byte
}
```

**Fix Applicato**:
```kotlin
// DOPO (FIXED)
if (uuid == CHAR_TEMP_UUID && data.size >= 4) {
    val temp = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN).int
    // ✅ Controllo dimensione prima del parsing
}
```

**Impatto**: ALTO - Crash app se firmware invia dati corrotti

---

#### 3. **Thread.sleep() nel Thread GATT** (MainActivity.kt:363-376)

**Problema**:
```kotlin
// PRIMA (BUGGY)
if (charHum != null) {
    Thread.sleep(100)  // ❌ Blocca thread BLE per 100ms
    enableNotification(gatt!!, charHum)
}
```

**Fix Applicato**:
```kotlin
// DOPO (FIXED)
if (charHum != null) {
    mainHandler.postDelayed({
        enableNotification(gatt!!, charHum)
    }, 100)  // ✅ Usa Handler asincrono
}
```

**Impatto**: MEDIO - Poteva causare ANR (Application Not Responding)

---

#### 4. **Reset Dati su Disconnessione** (MainActivity.kt:275-288, 340-343)

**Problema**:
```kotlin
// PRIMA (BUGGY)
override fun onConnectionStateChange(...) {
    if (newState == BluetoothProfile.STATE_DISCONNECTED) {
        connectionStatus = "Disconnesso"
        // ❌ Dati sensori rimanevano visualizzati anche da disconnesso
    }
}
```

**Fix Applicato**:
```kotlin
// DOPO (FIXED)
private fun resetSensorData() {
    tempState = 0; humState = 0
    creditState = 0; machineState = 0
}

override fun onConnectionStateChange(...) {
    if (newState == BluetoothProfile.STATE_DISCONNECTED) {
        runOnUiThread {
            connectionStatus = "Disconnesso"
            resetSensorData()  // ✅ Pulisce dati obsoleti
        }
    }
}
```

**Impatto**: MEDIO - UX migliorata, dati non più misleading

---

## 🔌 Firmware STM32 (main.cpp)

### 🔴 **CRITICAL FIXES**

#### 1. **Debouncing LDR Robusto** (firmware/main.cpp:318-342)

**Problema**:
```cpp
// PRIMA (BUGGY)
if (ldr_val > SOGLIA_LDR_SCATTO && !monetaInLettura) {
    monetaInLettura = true;
    credito++;  // ❌ Incremento immediato senza filtro
    // RISCHIO: 1 moneta contata 2-3 volte per rumori/vibrazioni
}
```

**Fix Applicato**:
```cpp
// DOPO (FIXED)
#define LDR_DEBOUNCE_SAMPLES 5
#define LDR_DEBOUNCE_TIME_US 300000  // 300ms minimo

if (ldr_val > SOGLIA_LDR_SCATTO && !monetaInLettura) {
    if (ldrSampleCount == 0) ldrDebounceTimer.start();
    ldrSampleCount++;

    uint64_t elapsed = ldrDebounceTimer.elapsed_time().count();
    if (ldrSampleCount >= LDR_DEBOUNCE_SAMPLES && elapsed > LDR_DEBOUNCE_TIME_US) {
        monetaInLettura = true;
        credito++;  // ✅ Incremento solo dopo 5 campioni + 300ms
        ldrSampleCount = 0;
    }
}
```

**Impatto**: CRITICO - Evita conteggi multipli errati

---

#### 2. **DHT11 in Thread Separato** (firmware/main.cpp:257-293)

**Problema**:
```cpp
// PRIMA (BUGGY)
void aggiornaDHT() {
    thread_sleep_for(18);  // ❌ BLOCCA MAIN LOOP PER 18ms
    __disable_irq();       // ❌ DISABILITA TUTTI GLI INTERRUPT (anche BLE!)
    // Lettura DHT11...
    __enable_irq();
}

// Chiamato in updateMachine() ogni 100ms
```

**Fix Applicato**:
```cpp
// DOPO (FIXED)
Mutex dhtMutex;  // Protegge accesso a temp_int/hum_int

void dht_reader_thread() {
    while(true) {
        // Lettura DHT11 con IRQ disabled
        __disable_irq();
        // ... lettura ...
        __enable_irq();

        // Aggiorna variabili protette
        dhtMutex.lock();
        temp_int = data[2];
        hum_int = data[0];
        dhtMutex.unlock();

        ThisThread::sleep_for(2000ms);  // ✅ Sleep solo thread DHT
    }
}

// In main():
Thread dhtThread(osPriorityLow);
dhtThread.start(callback(dht_reader_thread));
```

**Impatto**: CRITICO - Evita blocco main loop e stack BLE

---

#### 3. **Watchdog Timer** (firmware/main.cpp:115, 303, 596)

**Aggiunta**:
```cpp
Watchdog &watchdog = Watchdog::get_instance();

// In main():
watchdog.start(10000);  // Timeout 10 secondi

// In updateMachine():
watchdog.kick();  // Reset watchdog ogni ciclo
```

**Impatto**: ALTO - Recovery automatico da hang/freeze

---

#### 4. **Validazione Comandi BLE** (firmware/main.cpp:182-187)

**Problema**:
```cpp
// PRIMA (BUGGY)
uint8_t cmd = params.data[0];
if (cmd == 1) { /* ACQUA */ }
// ❌ Nessun controllo: accetta comandi 0x00-0xFF
```

**Fix Applicato**:
```cpp
// DOPO (FIXED)
uint8_t cmd = params.data[0];

// VALIDAZIONE COMANDO (SECURITY FIX)
if (cmd < 1 || (cmd > 4 && cmd != 9)) {
    printf("[SECURITY] Comando invalido: 0x%02X\n", cmd);
    return;  // ✅ Reject invalid commands
}
```

**Impatto**: MEDIO - Sicurezza BLE migliorata

---

#### 5. **Fix Buffer Overflow LCD** (firmware/main.cpp:401, 423, 444, etc.)

**Problema**:
```cpp
// PRIMA (BUGGY)
lcd.printf("Cr:%d/%d [Tasto Blu=Esc]", credito, prezzoSelezionato);
// ❌ Può superare 16 caratteri del display se credito > 9
```

**Fix Applicato**:
```cpp
// DOPO (FIXED)
char buffer[17];  // +1 per null terminator
snprintf(buffer, sizeof(buffer), "Cr:%d/%d [Blu=Esc", credito, prezzoSelezionato);
lcd.printf("%s", buffer);  // ✅ Troncamento sicuro a 16 caratteri
```

**Impatto**: BASSO - Evita display corrotto

---

#### 6. **Fix Timeout Underflow** (firmware/main.cpp:424-428)

**Problema**:
```cpp
// PRIMA (BUGGY)
uint64_t tempoPassato = timerUltimaMoneta.elapsed_time().count();
int secondiMancanti = (TIMEOUT_RESTO_AUTO - tempoPassato) / 1000000;
if (secondiMancanti < 0) secondiMancanti = 0;
// ❌ Sottrazione unsigned può causare underflow prima del check
```

**Fix Applicato**:
```cpp
// DOPO (FIXED)
uint64_t tempoPassato = timerUltimaMoneta.elapsed_time().count();
int secondiMancanti = 0;
if (tempoPassato < TIMEOUT_RESTO_AUTO) {
    secondiMancanti = (TIMEOUT_RESTO_AUTO - tempoPassato) / 1000000;
}  // ✅ Controllo esplicito prima della sottrazione
```

**Impatto**: BASSO - Evita valori negativi spurie

---

## 📊 **Riepilogo Impatto**

| Componente | Fix Applicati | Crash Risolti | Sicurezza | Stabilità |
|------------|---------------|---------------|-----------|-----------|
| **App Android** | 4 | 2 | ⚠️ Medio | ✅ Alta |
| **Firmware STM32** | 7 (v8.9+) | 1 | ✅ Alta | ✅ Alta |
| **Totale** | **11** | **3** | **✅** | **✅** |

**🆕 v8.9**: Aggiunto algoritmo spike detection LDR per rilevamento monete robusto con luce ambiente

---

## 🚀 **Come Applicare i Fix**

### Android App
1. Il codice è già aggiornato in `app/src/main/java/com/example/vendingmonitor/MainActivity.kt`
2. Ricompila l'app: `./gradlew assembleDebug`
3. Reinstalla su dispositivo

### Firmware STM32
1. Il firmware aggiornato è in `firmware/main.cpp`
2. Compila con Keil Studio Cloud o Mbed Studio
3. Flasha sulla Nucleo F401RE

---

## 🧪 **Testing Raccomandato**

### Test App
- ✅ Inserire 150+ monete (testa conversione unsigned)
- ✅ Disconnettere durante erogazione (testa reset dati)
- ✅ Lasciare connesso per 10+ minuti (testa stabilità Handler)

### Test Firmware
- ✅ Inserire monete rapidamente (testa debouncing LDR)
- ✅ Temperatura oscillante 27-29°C (testa thread DHT)
- ✅ Lasciare acceso 1h+ (testa watchdog)

---

## 🆕 **Firmware v8.9 (2025-01-06) - LDR Spike Detection**

### 🔴 **CRITICAL FIX: Sensore LDR Non Funziona con Luce Ambiente**

**Problema**:
```cpp
// PRIMA (BUGGY v8.8)
#define SOGLIA_LDR_SCATTO 25  // Soglia assoluta 25%
#define SOGLIA_LDR_RESET  15

if (ldr_val > SOGLIA_LDR_SCATTO && !monetaInLettura) {
    // ❌ Con luce ambiente baseline è 47% > 25%
    // ❌ Sistema rileva SEMPRE moneta presente (falso positivo)
}
```

**Sintomi**:
- Con luce ambiente **ACCESA**: LDR baseline ~47% > soglia 25% → credito non scatta correttamente
- Con luce ambiente **SPENTA**: LDR baseline ~15% < soglia 25% → funziona
- Toccare LDR aumenta valore a 84-90%, ma se baseline è già alto non viene rilevato

**Root Cause**:
Soglie **assolute** invece di **relative**. Il valore LDR dipende dalla luce ambiente, quindi una soglia fissa (25%) funziona solo con luce spenta.

**Fix Applicato (v8.9)**:
```cpp
// DOPO (FIXED v8.9) - SPIKE DETECTION ADATTIVO
#define SOGLIA_LDR_DELTA_SCATTO 20  // Delta relativo +20% sopra baseline
#define SOGLIA_LDR_DELTA_RESET   5  // Delta relativo +5%
#define LDR_BASELINE_ALPHA      10  // Coefficiente EMA

int ldrBaseline = 50;  // Baseline mobile (EMA)
bool ldrBaselineInit = false;

// Aggiorna baseline mobile (solo quando non c'è moneta)
if (!ldrBaselineInit) {
    ldrBaseline = ldr_val;  // Init
    ldrBaselineInit = true;
} else if (!monetaInLettura) {
    // EMA: si adatta automaticamente alla luce ambiente
    ldrBaseline = ((100 - LDR_BASELINE_ALPHA) * ldrBaseline + LDR_BASELINE_ALPHA * ldr_val) / 100;
}

// Rileva spike come variazione rispetto al baseline
int ldrDelta = ldr_val - ldrBaseline;
if (ldrDelta > SOGLIA_LDR_DELTA_SCATTO && !monetaInLettura) {
    // ✅ Rileva moneta come spike +20% sopra baseline mobile
    // ✅ Funziona con qualsiasi condizione di luce
}
```

**Vantaggi Algoritmo**:
1. **Baseline Adattivo**: EMA (Exponential Moving Average) si adatta automaticamente alla luce ambiente
2. **Soglie Relative**: Rileva variazioni improvvise (+20%), non valori assoluti
3. **Robusto**: Funziona con luce accesa, spenta, o in transizione senza calibrazione
4. **Debug Migliorato**: Log mostra `LDR:val%(B:baseline Δ:+delta)` per diagnostica

**Esempio Comportamento**:
```
Luce ACCESA:
[STATUS] ... | LDR:47%(B:47 Δ:+0)  | ...  → baseline si adatta a 47%
[TOCCO]  ... | LDR:84%(B:47 Δ:+37) | ...  → spike +37% > 20% → MONETA RILEVATA ✅
[STATUS] ... | LDR:48%(B:48 Δ:+0)  | ...  → baseline ritorna a 48%

Luce SPENTA:
[STATUS] ... | LDR:15%(B:15 Δ:+0)  | ...  → baseline si adatta a 15%
[TOCCO]  ... | LDR:43%(B:15 Δ:+28) | ...  → spike +28% > 20% → MONETA RILEVATA ✅
[STATUS] ... | LDR:15%(B:15 Δ:+0)  | ...  → baseline ritorna a 15%
```

**Impatto**: **CRITICO** - Risolve completamente il problema di rilevamento monete con luce ambiente

---

**Note**: Tutti i fix sono backward-compatible e non richiedono modifiche al protocollo BLE.
