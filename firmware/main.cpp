/*
 * ======================================================================================
 * PROGETTO: Vending Machine IoT (BLE + RTOS + Kotlin Interface)
 * TARGET: ST Nucleo F401RE + Shield BLE IDB05A2
 * VERSIONE: v8.7 CLEAN (Stock Management + LCD + Keypad + Sonar Optimized)
 * ======================================================================================
 *
 * CHANGELOG v8.7:
 * - [PERFORMANCE] Ridotta frequenza campionamento HC-SR04: 100ms → 500ms (5x meno frequente)
 * - [PERFORMANCE] Ridotti campioni per lettura: 5 → 3 (overhead: 75ms → 45ms)
 * - [PERFORMANCE] Cache distanza tra letture (riduce CPU usage)
 * - [FIX] Drastica riduzione timeout sonar e log spam
 * - [STABILITY] Sensore distanza più stabile con overhead minimo
 *
 * CHANGELOG v8.6:
 * - [FEATURE] Tastiera a membrana 4x3: alternativa fisica all'app smartphone
 * - [FEATURE] Tasti 1-4: selezione prodotti (ACQUA, SNACK, CAFFE, THE)
 * - [FEATURE] Tasto '#': CONFERMA acquisto (equivalente comando BLE 10)
 * - [FEATURE] Tasto '*': ANNULLA e resto (equivalente comando BLE 9)
 * - [DEBOUNCING] 300ms per tastiera (stesso timing LDR per consistenza)
 * - [HARDWARE] Pin: righe D10-D13, colonne D3/D7/A0, pull-up su colonne
 * - [UX] Operazione completamente autonoma senza necessità smartphone
 *
 * CHANGELOG v8.5:
 * - [FIX] Filtro anti-spike per sensore HC-SR04 (elimina letture spurie 999cm)
 * - [FIX] Aumentato timeout echo: 30ms → 50ms (più tollerante a interferenze)
 * - [FIX] Campionamento: 3 → 5 letture per maggiore affidabilità
 * - [FIX] Filtro range valido: 2-400cm (ignora valori fuori range)
 * - [FIX] Anti-spike filter: ignora salti > 100cm (interferenze servo/buzzer)
 * - [FIX] Memoria ultima distanza valida: se timeout usa valore precedente
 * - [STABILITY] Echo timing migliorato: trig 15μs, wait 15ms tra campioni
 *
 * CHANGELOG v8.4:
 * - [UX MAJOR] Rimossa erogazione automatica: richiede SEMPRE conferma esplicita
 * - [UX] Utente può inserire monete e cambiare prodotto liberamente
 * - [UX] LCD mostra "Premi CONFERMA!" quando credito è sufficiente
 * - [UX] Timeout unico 30s per resto (sia credito parziale che completo)
 * - [LOGIC] Erogazione solo tramite comando BLE 10 (pulsante CONFERMA ACQUISTO)
 * - [BEHAVIOR] Logica standard distributori automatici: inserisci → selezioni → confermi
 *
 * CHANGELOG v8.3:
 * - [FIX] Aggiunti delay 500us tra setCursor/printf per prevenire corruzione LCD
 * - [FIX] Tutte operazioni LCD ora hanno timing corretto per stabilità display
 * - [STABILITY] Risolti problemi visualizzazione LCD con operazioni consecutive
 * - [HARDWARE] Confermato: LED RGB GND mancante causava ground bounce → LCD corruption
 *
 * CHANGELOG v8.2:
 * - [CRITICAL FIX] Validazione scorte PRIMA di erogazione (prevenzione dispensing con stock=0)
 * - [FIX] Se scorte esaurite durante EROGAZIONE, mostra "PRODOTTO ESAURITO!" e restituisce credito
 * - [SECURITY] Impedisce movimento servo quando stock non disponibile
 *
 * CHANGELOG v8.1:
 * - [UX] LCD ottimizzato per info prodotto: nome, prezzo, scorte
 * - [UX] Rimosso temperatura e distanza da LCD (disponibili su seriale e app)
 * - [UX] RIPOSO: mostra prodotto selezionato e scorte
 * - [UX] ATTESA_MONETA: mostra nome prodotto, prezzo e scorte rimanenti
 * - [UX] EROGAZIONE: mostra "Erogando NOME_PRODOTTO" durante dispensa
 * - [UX] Post-erogazione: mostra "PRODOTTO erogato! Rim:X" con scorte aggiornate
 *
 * CHANGELOG v8.0:
 * - [CLEANUP] Display singolo LCD 16x2 per interfaccia utente
 * - [FEATURE] Sistema gestione scorte virtuale con array
 * - [BLE] STATUS characteristic 6-byte: [credito, stato, scorte_acqua, scorte_snack, scorte_caffe, scorte_the]
 * - [FIX] Delay 20ms dopo lcd.clear() per prevenire corruzione caratteri
 * - [FIX] Rimosso tutte operazioni LCD da gestori BLE per eliminare race conditions
 * - [DEBUG] Logging seriale completo per sensori, FSM, scorte, eventi
 * - [FEATURE] Comando BLE 11 per rifornimento scorte
 */

#include "mbed.h"
#include "ble/BLE.h"
#include "ble/Gap.h"
#include "ble/GattServer.h"
#include "TextLCD.h"

// --- CONFIGURAZIONE PIN ---
#define PIN_TRIG    A1
#define PIN_ECHO    D9
#define PIN_LDR     A2
#define PIN_DHT     D4
#define PIN_SERVO   D5
#define PIN_BUZZER  D2
#define PIN_LCD_SDA D14
#define PIN_LCD_SCL D15

// --- TASTIERA 4x3 (DISABILITATA) ---
// Temporaneamente disabilitata fino al collegamento hardware
#define KEYPAD_ENABLED 0

// --- PIN TASTIERA 4x3 ---
#define PIN_ROW1    D10
#define PIN_ROW2    D11
#define PIN_ROW3    D12
#define PIN_ROW4    D13
#define PIN_COL1    D3
#define PIN_COL2    D7
#define PIN_COL3    A0

// --- PARAMETRI ---
#define SOGLIA_LDR_SCATTO 25
#define SOGLIA_LDR_RESET  15
#define DISTANZA_ATTIVA   40
#define SOGLIA_TEMP       28
#define TIMEOUT_RESTO_AUTO 30000000
// TIMEOUT_EROGAZIONE_AUTO rimosso in v8.4: erogazione solo su conferma esplicita (cmd 10)

// --- DEBOUNCING LDR ---
#define LDR_DEBOUNCE_SAMPLES 5
#define LDR_DEBOUNCE_TIME_US 300000

// --- PREZZI ---
#define PREZZO_ACQUA      1
#define PREZZO_SNACK      2
#define PREZZO_CAFFE      1
#define PREZZO_THE        2
int prezzoSelezionato = PREZZO_ACQUA;
int idProdotto = 1;
#define FILTRO_INGRESSO   5
#define FILTRO_USCITA     20

// --- UUID BLE ---
const UUID VENDING_SERVICE_UUID((uint16_t)0xA000);
const UUID TEMP_CHAR_UUID((uint16_t)0xA001);
const UUID STATUS_CHAR_UUID((uint16_t)0xA002);
const UUID HUM_CHAR_UUID((uint16_t)0xA003);
const UUID CMD_CHAR_UUID((uint16_t)0xA004);

// --- STATI FSM ---
enum Stato { RIPOSO, ATTESA_MONETA, EROGAZIONE, RESTO, ERRORE };
Stato statoCorrente = RIPOSO;
Stato statoPrecedente = ERRORE;

// --- OGGETTI DRIVER ---
TextLCD lcd(PIN_LCD_SDA, PIN_LCD_SCL, 0x4E);
DigitalOut trig(PIN_TRIG);
InterruptIn echo(PIN_ECHO);
DigitalInOut dht(PIN_DHT);
PwmOut servo(PIN_SERVO);
AnalogIn ldr(PIN_LDR);
DigitalOut buzzer(PIN_BUZZER);
DigitalIn tastoAnnulla(PC_13);

#if KEYPAD_ENABLED
// --- TASTIERA 4x3 ---
DigitalOut row1(PIN_ROW1);
DigitalOut row2(PIN_ROW2);
DigitalOut row3(PIN_ROW3);
DigitalOut row4(PIN_ROW4);
DigitalIn col1(PIN_COL1, PullUp);
DigitalIn col2(PIN_COL2, PullUp);
DigitalIn col3(PIN_COL3, PullUp);
#endif

// --- LED RGB ---
DigitalOut ledR(D6);
DigitalOut ledG(D8);
DigitalOut ledB(A3);

void setRGB(int r, int g, int b) {
    ledR = r; ledG = g; ledB = b;
}

BufferedSerial pc(USBTX, USBRX, 9600);
FileHandle *mbed::mbed_override_console(int fd) { return &pc; }

// --- VARIABILI GLOBALI ---
Timer sonarTimer;
Timer timerUltimaMoneta;
Timer timerStato;
Timer ldrDebounceTimer;
volatile uint64_t echoDuration = 0;
bool monetaInLettura = false;
int credito = 0;
int temp_int = 0;
int hum_int = 0;
bool dht_valid = false;
int contatorePresenza = 0;
int contatoreAssenza = 0;
int ldrSampleCount = 0;
bool creditoResiduo = false;

// Filtro distanza per gestire letture spurie 999cm
int ultimaDistanzaValida = 100;  // Valore iniziale default

#if KEYPAD_ENABLED
// --- DEBOUNCING TASTIERA ---
char ultimoTasto = '\0';
bool tastoInLettura = false;
bool keypadTimerStarted = false;
Timer keypadDebounceTimer;
#define KEYPAD_DEBOUNCE_TIME_US 300000  // 300ms debounce
#endif

// --- GESTIONE SCORTE ---
int scorte[5] = {0, 5, 5, 5, 5}; // [0]=dummy, [1]=ACQUA, [2]=SNACK, [3]=CAFFE, [4]=THE
const int SCORTE_MAX = 5;

// Mutex per proteggere accesso DHT
Mutex dhtMutex;

// Watchdog timer
Watchdog &watchdog = Watchdog::get_instance();

// ======================================================================================
// CLASSE BLE SERVICE
// ======================================================================================
class VendingService {
public:
    VendingService(BLE &_ble, int initial_temp, int initial_hum, int initial_credit) :
        ble(_ble),
        tempChar(TEMP_CHAR_UUID, &initial_temp, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY),
        humChar(HUM_CHAR_UUID, &initial_hum, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY),
        cmdChar(CMD_CHAR_UUID, &initial_credit, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESPONSE),
        statusChar(STATUS_CHAR_UUID, statusData, 6, 6, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY)
    {
        statusData[0] = 0;
        statusData[1] = 0;
        statusData[2] = (uint8_t)scorte[1];
        statusData[3] = (uint8_t)scorte[2];
        statusData[4] = (uint8_t)scorte[3];
        statusData[5] = (uint8_t)scorte[4];

        GattCharacteristic *charTable[] = {&tempChar, &humChar, &statusChar, &cmdChar};
        GattService vendingService(VENDING_SERVICE_UUID, charTable, 4);
        ble.gattServer().addService(vendingService);
    }

    void updateTemp(int newTemp) {
        ble.gattServer().write(tempChar.getValueHandle(), (uint8_t *)&newTemp, sizeof(newTemp));
    }

    void updateHum(int newHum) {
        ble.gattServer().write(humChar.getValueHandle(), (uint8_t *)&newHum, sizeof(newHum));
    }

    void updateStatus(int credit, int state) {
        statusData[0] = (uint8_t)credit;
        statusData[1] = (uint8_t)state;
        statusData[2] = (uint8_t)scorte[1];
        statusData[3] = (uint8_t)scorte[2];
        statusData[4] = (uint8_t)scorte[3];
        statusData[5] = (uint8_t)scorte[4];
        ble.gattServer().write(statusChar.getValueHandle(), statusData, 6);
    }

    GattAttribute::Handle_t getCmdHandle() { return cmdChar.getValueHandle(); }

private:
    BLE &ble;
    uint8_t statusData[6];
    ReadOnlyGattCharacteristic<int> tempChar;
    ReadOnlyGattCharacteristic<int> humChar;
    WriteOnlyGattCharacteristic<int> cmdChar;
    GattCharacteristic statusChar;
};

VendingService *vendingServicePtr = nullptr;
static EventQueue event_queue(16 * EVENTS_EVENT_SIZE);

// ======================================================================================
// GESTORE EVENTI GATT SERVER
// ======================================================================================
class VendingServerEventHandler : public ble::GattServer::EventHandler {
    void onDataWritten(const GattWriteCallbackParams &params) override {
        if (vendingServicePtr && params.handle == vendingServicePtr->getCmdHandle()) {
            if (params.len > 0) {
                uint8_t cmd = params.data[0];

                if (cmd < 1 || (cmd > 4 && cmd != 9 && cmd != 10 && cmd != 11)) {
                    printf("[SECURITY] Comando BLE invalido: 0x%02X\n", cmd);
                    return;
                }

                if (cmd == 1) {
                    if (scorte[1] <= 0) {
                        printf("[STOCK] ACQUA esaurita\n");
                        return;
                    }
                    idProdotto = 1; prezzoSelezionato = PREZZO_ACQUA;
                    setRGB(0, 1, 1);
                    timerUltimaMoneta.reset();
                    printf("[BLE] ACQUA selezionata (scorte=%d)\n", scorte[1]);
                }
                else if (cmd == 2) {
                    if (scorte[2] <= 0) {
                        printf("[STOCK] SNACK esaurito\n");
                        return;
                    }
                    idProdotto = 2; prezzoSelezionato = PREZZO_SNACK;
                    setRGB(1, 0, 1);
                    timerUltimaMoneta.reset();
                    printf("[BLE] SNACK selezionato (scorte=%d)\n", scorte[2]);
                }
                else if (cmd == 3) {
                    if (scorte[3] <= 0) {
                        printf("[STOCK] CAFFE esaurito\n");
                        return;
                    }
                    idProdotto = 3; prezzoSelezionato = PREZZO_CAFFE;
                    setRGB(1, 1, 0);
                    timerUltimaMoneta.reset();
                    printf("[BLE] CAFFE selezionato (scorte=%d)\n", scorte[3]);
                }
                else if (cmd == 4) {
                    if (scorte[4] <= 0) {
                        printf("[STOCK] THE esaurito\n");
                        return;
                    }
                    idProdotto = 4; prezzoSelezionato = PREZZO_THE;
                    setRGB(0, 1, 0);
                    timerUltimaMoneta.reset();
                    printf("[BLE] THE selezionato (scorte=%d)\n", scorte[4]);
                }
                else if (cmd == 9) {
                    if (credito > 0) {
                        printf("[ANNULLA] App - Resto: %dE\n", credito);
                        setRGB(1, 0, 1);
                        statoCorrente = RESTO;
                        timerStato.reset();
                        timerStato.start();
                        vendingServicePtr->updateStatus(credito, statoCorrente);
                    }
                }
                else if (cmd == 10) {
                    printf("[BLE] CONFERMA: credito=%d, prezzo=%d, stato=%d\n",
                           credito, prezzoSelezionato, statoCorrente);

                    if (statoCorrente != ATTESA_MONETA) {
                        printf("[BLE] Rifiutata: stato invalido\n");
                    } else if (credito < prezzoSelezionato) {
                        printf("[BLE] Rifiutata: credito insufficiente\n");
                    } else {
                        printf("[BLE] Accettata: avvio erogazione\n");
                        statoCorrente = EROGAZIONE;
                        timerStato.reset();
                        timerStato.start();
                        vendingServicePtr->updateStatus(credito, statoCorrente);
                    }
                }
                else if (cmd == 11) {
                    scorte[1] = SCORTE_MAX;
                    scorte[2] = SCORTE_MAX;
                    scorte[3] = SCORTE_MAX;
                    scorte[4] = SCORTE_MAX;
                    printf("[STOCK] Rifornimento completato: %d pezzi/prodotto\n", SCORTE_MAX);
                    if (vendingServicePtr) vendingServicePtr->updateStatus(credito, statoCorrente);
                }
            }
        }
    }
};

// ======================================================================================
// GESTORE EVENTI GAP
// ======================================================================================
class VendingGapEventHandler : public ble::Gap::EventHandler {
public:
    void onDisconnectionComplete(const ble::DisconnectionCompleteEvent &event) override {
        BLE::Instance().gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);
    }
};

static VendingGapEventHandler gap_handler;
static VendingServerEventHandler server_handler;

#if KEYPAD_ENABLED
// ======================================================================================
// TASTIERA 4x3 - SCAN MATRICE
// ======================================================================================
char scanKeypad() {
    // Layout tastiera:
    //   1   2   3   → Selezione prodotti
    //   4   5   6   → THE + futuri
    //   7   8   9   → Riservati
    //   *   0   #   → ANNULLA, riservato, CONFERMA

    const char keys[4][3] = {
        {'1','2','3'},  // Row 1: ACQUA, SNACK, CAFFE
        {'4','5','6'},  // Row 2: THE, futuro, futuro
        {'7','8','9'},  // Row 3: riservati
        {'*','0','#'}   // Row 4: ANNULLA, riservato, CONFERMA
    };

    DigitalOut* rows[] = {&row1, &row2, &row3, &row4};
    DigitalIn* cols[] = {&col1, &col2, &col3};

    // Inizializza tutte le righe HIGH
    for(int i=0; i<4; i++) {
        *rows[i] = 1;
    }

    // Scan matrice
    for(int r=0; r<4; r++) {
        *rows[r] = 0;  // Attiva riga (LOW)
        wait_us(10);   // Stabilizza

        for(int c=0; c<3; c++) {
            if(*cols[c] == 0) {  // Colonna premuta (pull-up -> LOW quando premuto)
                *rows[r] = 1;  // Disattiva riga
                return keys[r][c];
            }
        }

        *rows[r] = 1;  // Disattiva riga
    }

    return '\0';  // Nessun tasto premuto
}
#endif

// ======================================================================================
// SENSORI
// ======================================================================================
void echoRise() { sonarTimer.reset(); sonarTimer.start(); }
void echoFall() { sonarTimer.stop(); echoDuration = sonarTimer.elapsed_time().count(); }

int leggiDistanza() {
    int somma = 0;
    int validi = 0;

    // Campiona 3 volte (ridotto da 5 per overhead)
    for(int i=0; i<3; i++) {
        trig = 0; wait_us(5);
        trig = 1; wait_us(15);  // Pulse più lungo per affidabilità
        trig = 0;
        wait_us(15000);  // Timeout aumentato a 15ms (circa 250cm max)

        // Timeout aumentato a 50000μs (circa 850cm max, più tollerante)
        if (echoDuration > 0 && echoDuration < 50000) {
            int distanza = (int)(echoDuration * 0.0343f / 2.0f);
            // Filtro range valido: ignora letture < 2cm o > 400cm
            if (distanza >= 2 && distanza <= 400) {
                somma += distanza;
                validi++;
            }
        }
    }

    if (validi == 0) {
        // Nessuna lettura valida: usa ultima distanza valida con decadimento lento
        printf("[SONAR] Timeout completo, uso ultima valida: %dcm\n", ultimaDistanzaValida);
        return ultimaDistanzaValida;
    }

    int media = somma / validi;

    // Filtro anti-spike: se la differenza è > 100cm, probabilmente è rumore
    if (abs(media - ultimaDistanzaValida) > 100 && ultimaDistanzaValida < 400) {
        printf("[SONAR] Spike rilevato %d->%d, mantengo precedente\n", ultimaDistanzaValida, media);
        return ultimaDistanzaValida;
    }

    // Aggiorna ultima distanza valida
    ultimaDistanzaValida = media;
    return media;
}

// ======================================================================================
// DHT11 - THREAD SEPARATO
// ======================================================================================
int pulseIn(int level) {
    int count = 0;
    while (dht == level) {
        if (count++ > 200) return -1;
        wait_us(1);
    }
    return count;
}

void dht_reader_thread() {
    while(true) {
        dht.output();
        dht = 0;
        thread_sleep_for(18);
        dht = 1;
        wait_us(30);
        dht.input();

        __disable_irq();
        bool error = false;
        if (pulseIn(1) == -1 || pulseIn(0) == -1 || pulseIn(1) == -1) error = true;

        uint8_t data[5] = {0};
        if (!error) {
            for (int i = 0; i < 40; i++) {
                if (pulseIn(0) == -1) { error = true; break; }
                int width = pulseIn(1);
                if (width == -1) { error = true; break; }
                if (width > 45) data[i/8] |= (1 << (7 - (i%8)));
            }
        }
        __enable_irq();

        if (!error) {
            uint8_t calc = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
            if (data[4] == calc && (data[0] != 0 || data[2] != 0)) {
                dhtMutex.lock();
                hum_int = data[0];
                temp_int = data[2];
                dht_valid = true;
                dhtMutex.unlock();
            }
        }

        ThisThread::sleep_for(2000ms);
    }
}

// ======================================================================================
// LOOP PRINCIPALE
// ======================================================================================
void updateMachine() {
    static int counterTemp = 0;
    static int counterDist = 0;
    static int blinkTimer = 0;
    static int dist = 100;  // Cache distanza

    watchdog.kick();

    int ldr_val = (int)(ldr.read() * 100);

    // Leggi distanza solo ogni 500ms invece di ogni 100ms (riduce overhead)
    if (++counterDist >= 5) {
        counterDist = 0;
        dist = leggiDistanza();
    }

#if KEYPAD_ENABLED
    // TASTIERA 4x3 - Lettura e debouncing
    char tasto = scanKeypad();
    if (tasto != '\0' && !tastoInLettura) {
        if (!keypadTimerStarted) {
            keypadDebounceTimer.reset();
            keypadDebounceTimer.start();
            keypadTimerStarted = true;
            ultimoTasto = tasto;
        }

        if (keypadDebounceTimer.elapsed_time().count() > KEYPAD_DEBOUNCE_TIME_US) {
            tastoInLettura = true;
            printf("[KEYPAD] Tasto premuto: %c\n", tasto);

            // Gestione tasti in base allo stato
            if (statoCorrente == RIPOSO || statoCorrente == ATTESA_MONETA) {
                // Tasti selezione prodotti (1-4)
                if (tasto == '1') {
                    if (scorte[1] > 0) {
                        idProdotto = 1;
                        prezzoSelezionato = PREZZO_ACQUA;
                        setRGB(0, 1, 1);
                        timerUltimaMoneta.reset();
                        printf("[KEYPAD] ACQUA selezionata (scorte=%d)\n", scorte[1]);
                        if (statoCorrente == RIPOSO) statoCorrente = ATTESA_MONETA;
                    } else {
                        printf("[KEYPAD] ACQUA esaurita\n");
                    }
                }
                else if (tasto == '2') {
                    if (scorte[2] > 0) {
                        idProdotto = 2;
                        prezzoSelezionato = PREZZO_SNACK;
                        setRGB(1, 0, 1);
                        timerUltimaMoneta.reset();
                        printf("[KEYPAD] SNACK selezionato (scorte=%d)\n", scorte[2]);
                        if (statoCorrente == RIPOSO) statoCorrente = ATTESA_MONETA;
                    } else {
                        printf("[KEYPAD] SNACK esaurito\n");
                    }
                }
                else if (tasto == '3') {
                    if (scorte[3] > 0) {
                        idProdotto = 3;
                        prezzoSelezionato = PREZZO_CAFFE;
                        setRGB(1, 1, 0);
                        timerUltimaMoneta.reset();
                        printf("[KEYPAD] CAFFE selezionato (scorte=%d)\n", scorte[3]);
                        if (statoCorrente == RIPOSO) statoCorrente = ATTESA_MONETA;
                    } else {
                        printf("[KEYPAD] CAFFE esaurito\n");
                    }
                }
                else if (tasto == '4') {
                    if (scorte[4] > 0) {
                        idProdotto = 4;
                        prezzoSelezionato = PREZZO_THE;
                        setRGB(0, 1, 0);
                        timerUltimaMoneta.reset();
                        printf("[KEYPAD] THE selezionato (scorte=%d)\n", scorte[4]);
                        if (statoCorrente == RIPOSO) statoCorrente = ATTESA_MONETA;
                    } else {
                        printf("[KEYPAD] THE esaurito\n");
                    }
                }
                // Tasto CONFERMA (#)
                else if (tasto == '#' && statoCorrente == ATTESA_MONETA) {
                    printf("[KEYPAD] CONFERMA: credito=%d, prezzo=%d\n", credito, prezzoSelezionato);
                    if (credito >= prezzoSelezionato) {
                        printf("[KEYPAD] Accettata: avvio erogazione\n");
                        statoCorrente = EROGAZIONE;
                        timerStato.reset();
                        timerStato.start();
                        if (vendingServicePtr) vendingServicePtr->updateStatus(credito, statoCorrente);
                    } else {
                        printf("[KEYPAD] Rifiutata: credito insufficiente\n");
                    }
                }
                // Tasto ANNULLA (*)
                else if (tasto == '*' && statoCorrente == ATTESA_MONETA && credito > 0) {
                    printf("[KEYPAD] ANNULLA - Resto: %dE\n", credito);
                    setRGB(1, 0, 1);
                    statoCorrente = RESTO;
                    timerStato.reset();
                    timerStato.start();
                    if (vendingServicePtr) vendingServicePtr->updateStatus(credito, statoCorrente);
                }
            }

            keypadDebounceTimer.stop();
            keypadTimerStarted = false;
        }
    } else if (tasto == '\0') {
        // Nessun tasto premuto: reset debouncing
        tastoInLettura = false;
        if (keypadTimerStarted) {
            keypadDebounceTimer.stop();
            keypadTimerStarted = false;
        }
    }
#endif

    // Aggiorna sensori ogni 2s
    if (++counterTemp > 20) {
        counterTemp = 0;
        if (vendingServicePtr) {
            dhtMutex.lock();
            int temp_copy = temp_int;
            int hum_copy = hum_int;
            bool valid = dht_valid;
            dhtMutex.unlock();

            if (valid) {
                vendingServicePtr->updateTemp(temp_copy);
                vendingServicePtr->updateHum(hum_copy);
                printf("[SENSORI] Temp: %d°C | Hum: %d%% | Dist: %dcm\n", temp_copy, hum_copy, dist);
            }
        }

        dhtMutex.lock();
        int temp_check = temp_int;
        dhtMutex.unlock();

        if (temp_check >= SOGLIA_TEMP && statoCorrente != ERRORE) {
            printf("[ALLARME] Temperatura: %d°C (soglia: %d°C)\n", temp_check, SOGLIA_TEMP);
            statoCorrente = ERRORE;
            lcd.clear();
            wait_us(20000);
        }
    }

    // DEBOUNCING LDR
    if (statoCorrente != ERRORE && statoCorrente != EROGAZIONE && statoCorrente != RESTO) {
        if (ldr_val > SOGLIA_LDR_SCATTO && !monetaInLettura) {
            if (ldrSampleCount == 0) {
                ldrDebounceTimer.start();
            }
            ldrSampleCount++;

            uint64_t elapsed = ldrDebounceTimer.elapsed_time().count();
            if (ldrSampleCount >= LDR_DEBOUNCE_SAMPLES && elapsed > LDR_DEBOUNCE_TIME_US) {
                monetaInLettura = true;
                credito++;
                timerUltimaMoneta.reset();
                creditoResiduo = false;
                ldrSampleCount = 0;
                ldrDebounceTimer.reset();

                if (statoCorrente == RIPOSO) statoCorrente = ATTESA_MONETA;
                if (vendingServicePtr) vendingServicePtr->updateStatus(credito, statoCorrente);

                printf("[LDR] Moneta rilevata! Credito=%d\n", credito);
            }
        } else if (ldr_val < SOGLIA_LDR_RESET) {
            if (monetaInLettura) {
                monetaInLettura = false;
                ldrDebounceTimer.stop();
            }
            ldrSampleCount = 0;
        }
    }

    if (statoCorrente != statoPrecedente) {
        lcd.clear();
        wait_us(20000);
        buzzer = 0;

        const char* nomiStati[] = {"RIPOSO", "ATTESA_MONETA", "EROGAZIONE", "RESTO", "ERRORE"};
        printf("[FSM] %s -> %s | Credito: %dE | Prodotto: %d\n",
               nomiStati[statoPrecedente], nomiStati[statoCorrente], credito, idProdotto);

        statoPrecedente = statoCorrente;
        contatorePresenza = 0;
        contatoreAssenza = 0;
        if (vendingServicePtr) vendingServicePtr->updateStatus(credito, statoCorrente);
    }

    switch (statoCorrente) {
        case RIPOSO:
            setRGB(0, 1, 0);
            buzzer = 0;
            lcd.setCursor(0, 0);
            wait_us(500);
            lcd.printf("  VENDING IoT   ");
            wait_us(500);
            lcd.setCursor(0, 1);
            wait_us(500);

            // Mostra prodotto selezionato e scorte
            char buffer[17];
            if(idProdotto==1)      snprintf(buffer, 17, "ACQUA  Rim:%d/5", scorte[1]);
            else if(idProdotto==2) snprintf(buffer, 17, "SNACK  Rim:%d/5", scorte[2]);
            else if(idProdotto==3) snprintf(buffer, 17, "CAFFE  Rim:%d/5", scorte[3]);
            else                   snprintf(buffer, 17, "THE    Rim:%d/5", scorte[4]);
            lcd.printf("%s", buffer);

            if (dist < DISTANZA_ATTIVA) {
                if (++contatorePresenza > FILTRO_INGRESSO) statoCorrente = ATTESA_MONETA;
            } else contatorePresenza = 0;
            break;

        case ATTESA_MONETA: {
            if (idProdotto == 1) setRGB(0, 1, 1);
            else if (idProdotto == 2) setRGB(1, 0, 1);
            else if (idProdotto == 3) setRGB(1, 1, 0);
            else setRGB(0, 1, 0);

            buzzer = 0;
            lcd.setCursor(0, 0);
            wait_us(500);
            uint64_t tempoPassato = timerUltimaMoneta.elapsed_time().count();

            int secondiMancanti = 0;
            if (tempoPassato < TIMEOUT_RESTO_AUTO) {
                secondiMancanti = (TIMEOUT_RESTO_AUTO - tempoPassato) / 1000000;
            }

            // Mostra stato credito e richiesta conferma
            if (credito >= prezzoSelezionato) {
                // Credito sufficiente: richiede conferma esplicita
                char buf[17];
                snprintf(buf, sizeof(buf), "Premi CONFERMA!");
                lcd.printf("%s", buf);
            } else if (credito > 0 && credito < prezzoSelezionato) {
                // Credito parziale: mostra quanto manca
                char buf[17];
                snprintf(buf, sizeof(buf), "Cr:%dE T:%02ds", credito, secondiMancanti);
                lcd.printf("%s", buf);
            } else {
                // Credito zero: mostra prodotto selezionato
                if(idProdotto==1)      lcd.printf("Ins.Mon x ACQUA ");
                else if(idProdotto==2) lcd.printf("Ins.Mon x SNACK ");
                else if(idProdotto==3) lcd.printf("Ins.Mon x CAFFE ");
                else                   lcd.printf("Ins.Mon x THE   ");
            }

            wait_us(500);
            lcd.setCursor(0, 1);
            wait_us(500);
            char buf2[17];
            if(credito > 0) {
                // Mostra credito e timeout resto
                snprintf(buf2, sizeof(buf2), "Cr:%d/%d T:%02ds", credito, prezzoSelezionato, secondiMancanti);
            } else {
                // Mostra prezzo e scorte prodotto selezionato
                const char* nomi[] = {"", "ACQUA", "SNACK", "CAFFE", "THE"};
                snprintf(buf2, sizeof(buf2), "%s:%dE Rim:%d",
                         nomi[idProdotto], prezzoSelezionato, scorte[idProdotto]);
            }
            lcd.printf("%s", buf2);

            // Gestione eventi
            if (tastoAnnulla == 0 && credito > 0) {
                // Annullamento manuale con pulsante
                lcd.clear();
                wait_us(20000);
                lcd.printf("Annullato Manual");
                printf("[ANNULLA] Pulsante - Resto: %dE\n", credito);
                thread_sleep_for(1000);
                statoCorrente = RESTO;
                timerStato.reset();
                timerStato.start();
                if (vendingServicePtr) vendingServicePtr->updateStatus(credito, statoCorrente);
            }
            else if (credito > 0 && tempoPassato > TIMEOUT_RESTO_AUTO) {
                // Timeout 30s: restituisci qualsiasi credito (parziale o completo)
                lcd.clear();
                wait_us(20000);
                lcd.printf("Tempo Scaduto!");
                printf("[TIMEOUT] Resto automatico - Credito: %dE\n", credito);
                thread_sleep_for(1000);
                statoCorrente = RESTO;
                timerStato.reset();
                timerStato.start();
                if (vendingServicePtr) vendingServicePtr->updateStatus(credito, statoCorrente);
            }
            else if (dist > (DISTANZA_ATTIVA + 20) && credito == 0) {
                // Ritorno a RIPOSO se utente si allontana senza credito
                if (++contatoreAssenza > FILTRO_USCITA) statoCorrente = RIPOSO;
            } else contatoreAssenza = 0;
            break;
        }

        case EROGAZIONE:
            // CRITICAL: Verifica scorte PRIMA di erogare
            if (idProdotto < 1 || idProdotto > 4 || scorte[idProdotto] <= 0) {
                printf("[ERRORE] Tentativo erogazione con scorte=0 (prodotto %d)\n", idProdotto);
                setRGB(1, 0, 0);
                lcd.clear();
                wait_us(20000);
                lcd.setCursor(0, 0);
                lcd.printf("PRODOTTO");
                lcd.setCursor(0, 1);
                lcd.printf("ESAURITO!");
                buzzer = 1;
                thread_sleep_for(2000);
                buzzer = 0;

                // Vai a RESTO per restituire il credito
                statoCorrente = RESTO;
                timerStato.reset();
                timerStato.start();
                if (vendingServicePtr) vendingServicePtr->updateStatus(credito, statoCorrente);
                break;
            }

            // Scorte disponibili: procedi con erogazione
            setRGB(1, 1, 0);
            lcd.setCursor(0, 0);
            wait_us(500);

            // Mostra nome prodotto erogato
            if(idProdotto==1)      lcd.printf("Erogando ACQUA  ");
            else if(idProdotto==2) lcd.printf("Erogando SNACK  ");
            else if(idProdotto==3) lcd.printf("Erogando CAFFE  ");
            else                   lcd.printf("Erogando THE    ");

            wait_us(500);
            lcd.setCursor(0, 1);
            wait_us(500);
            lcd.printf("Attendere       ");
            if (timerStato.elapsed_time().count() < 2000000) {
                buzzer = 1;
                if (timerStato.elapsed_time().count() < 1000000) servo.write(0.10f);
                else servo.write(0.05f);
            } else {
                buzzer = 0;

                // Decrementa scorte dopo erogazione riuscita
                scorte[idProdotto]--;
                printf("[EROGAZIONE] Prodotto %d erogato. Scorte rimanenti: %d\n", idProdotto, scorte[idProdotto]);

                credito -= prezzoSelezionato;

                // Mostra prodotto erogato e scorte aggiornate
                lcd.clear();
                wait_us(20000);
                lcd.setCursor(0, 0);
                const char* nomi[] = {"", "ACQUA", "SNACK", "CAFFE", "THE"};
                char bufErog[17];
                snprintf(bufErog, 17, "%s erogato!", nomi[idProdotto]);
                lcd.printf("%s", bufErog);

                lcd.setCursor(0, 1);
                if (credito > 0) {
                    char bufCred[17];
                    snprintf(bufCred, 17, "Rim:%d Cred:%dE", scorte[idProdotto], credito);
                    lcd.printf("%s", bufCred);
                } else {
                    char bufRim[17];
                    snprintf(bufRim, 17, "Rimanenti: %d", scorte[idProdotto]);
                    lcd.printf("%s", bufRim);
                }
                thread_sleep_for(1500);

                if (credito > 0) {
                    statoCorrente = ATTESA_MONETA;
                    timerUltimaMoneta.reset();
                    timerUltimaMoneta.start();
                    creditoResiduo = true;
                } else {
                    statoCorrente = ATTESA_MONETA;
                    creditoResiduo = false;
                }

                if (vendingServicePtr) vendingServicePtr->updateStatus(credito, statoCorrente);
            }
            break;

        case RESTO:
            setRGB(1, 0, 1);
            lcd.setCursor(0, 0);
            wait_us(500);
            lcd.printf("Ritira Resto    ");
            wait_us(500);
            lcd.setCursor(0, 1);
            wait_us(500);
            char bufResto[17];
            snprintf(bufResto, sizeof(bufResto), "Monete: %d", credito);
            lcd.printf("%s", bufResto);

            if ((timerStato.elapsed_time().count() % 400000) < 200000) buzzer = 1;
            else buzzer = 0;

            if (timerStato.elapsed_time().count() > 3000000) {
                printf("[RESTO] Restituito: %dE\n", credito);
                buzzer = 0;
                credito = 0;
                statoCorrente = ATTESA_MONETA;
            }
            break;

        case ERRORE:
            blinkTimer++;
            if (blinkTimer % 2 == 0) { setRGB(1, 0, 0); buzzer = 1; }
            else { setRGB(0, 0, 0); buzzer = 0; }

            lcd.setCursor(0, 0);
            wait_us(500);
            lcd.printf("! ALLARME TEMP !");
            wait_us(500);
            lcd.setCursor(0, 1);
            wait_us(500);
            char bufErr[17];
            dhtMutex.lock();
            snprintf(bufErr, sizeof(bufErr), "T:%dC > %dC", temp_int, SOGLIA_TEMP);
            dhtMutex.unlock();
            lcd.printf("%s", bufErr);

            dhtMutex.lock();
            int temp_check = temp_int;
            dhtMutex.unlock();
            if (temp_check <= (SOGLIA_TEMP - 2)) statoCorrente = RIPOSO;
            break;
    }
}

// ======================================================================================
// BLE INIT
// ======================================================================================
void onBleInitError(BLE &ble, ble_error_t error) { }

void bleInitComplete(BLE::InitializationCompleteCallbackContext *params) {
    BLE& ble = params->ble;
    if (params->error != BLE_ERROR_NONE) return;

    vendingServicePtr = new VendingService(ble, 23, 50, 0);

    ble.gap().setEventHandler(&gap_handler);
    ble.gattServer().setEventHandler(&server_handler);

    static uint8_t _adv_buffer[ble::LEGACY_ADVERTISING_MAX_SIZE];
    ble::AdvertisingDataBuilder _adv_data_builder(_adv_buffer);
    _adv_data_builder.setFlags();
    _adv_data_builder.setName("VendingM");
    ble.gap().setAdvertisingPayload(ble::LEGACY_ADVERTISING_HANDLE, _adv_data_builder.getAdvertisingData());
    ble::AdvertisingParameters adv_parameters(ble::advertising_type_t::CONNECTABLE_UNDIRECTED, ble::adv_interval_t(ble::millisecond_t(1000)));
    ble.gap().setAdvertisingParameters(ble::LEGACY_ADVERTISING_HANDLE, adv_parameters);
    ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);

    event_queue.call_every(100ms, updateMachine);
}

void scheduleBleEventsProcessing(BLE::OnEventsToProcessCallbackContext *context) {
    event_queue.call(Callback<void()>(&context->ble, &BLE::processEvents));
}

int main() {
    thread_sleep_for(200);
    servo.period_ms(20);
    servo.write(0.05f);
    echo.rise(&echoRise);
    echo.fall(&echoFall);
    lcd.begin();
    lcd.backlight();
    lcd.clear();
    wait_us(20000);
    lcd.setCursor(0,0);
    lcd.printf("BOOT v8.7 OPTIM");
    buzzer = 1;
    thread_sleep_for(100);
    buzzer = 0;
    timerUltimaMoneta.start();
    ldrDebounceTimer.reset();

    Thread dhtThread(osPriorityLow);
    dhtThread.start(callback(dht_reader_thread));

    watchdog.start(10000);

    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(scheduleBleEventsProcessing);
    ble.init(bleInitComplete);
    event_queue.dispatch_forever();
}
