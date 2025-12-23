# 🔧 Bug Fixes & Improvements

**Data Fix**: 2025-12-22
**Versione App**: v1.1
**Versione Firmware**: v7.2

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
| **Firmware STM32** | 6 | 1 | ✅ Alta | ✅ Alta |
| **Totale** | **10** | **3** | **✅** | **✅** |

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

**Note**: Tutti i fix sono backward-compatible e non richiedono modifiche al protocollo BLE.
