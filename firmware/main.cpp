/*
 * ======================================================================================
 * PROGETTO: Vending Machine IoT (BLE + RTOS + Kotlin Interface)
 * TARGET: ST Nucleo F401RE + Shield BLE IDB05A2
 * VERSIONE: v8.10.1 UX IMPROVEMENTS (LCD Notifications + Product Confirmation)
 * ======================================================================================
 *
 * CHANGELOG v8.10.1 (2025-01-06):
 * - [FIX] Risolti residui LCD con padding esplicito a 16 caratteri su tutte le stringhe
 * - [LCD] Tutte le stringhe ora terminano con spazi fino a 16 char + '\0' troncato
 * - [CLEANUP] Rimossi artefatti visivi su transizioni messaggi LCD
 *
 * CHANGELOG v8.10 (2025-01-06):
 * - [UX] Notifiche connessione/disconnessione BLE visualizzate su LCD (1.5s)
 * - [UX] Messaggio conferma ora mostra prodotto selezionato: "Conf. x ACQUA!"
 * - [FEEDBACK] LCD mostra "BLE CONNESSO!" quando app si collega
 * - [FEEDBACK] LCD mostra "BLE DISCONNESSO" quando app si scollega
 * - [CLARITY] Utente vede chiaramente quale prodotto sta per acquistare prima di confermare
 *
 * CHANGELOG v8.9 (2025-01-06):
 * - [FIX CRITICAL] Algoritmo LDR spike detection adattivo (risolve problema luce ambiente)
 * - [ALGORITHM] Baseline mobile (EMA) si adatta automaticamente alla luce ambiente
 * - [ALGORITHM] Rileva monete come variazione improvvisa (+20%) invece di soglia assoluta
 * - [ROBUSTNESS] Funziona correttamente con luce accesa/spenta senza calibrazione
 * - [DEBUG] Log LDR esteso: mostra valore, baseline e delta (Δ) per diagnostica
 * - [FIX] Risolto: credito non scattava con luce ambiente accesa (baseline 47% > soglia 25%)
 *
 * CHANGELOG v8.8 (2025-01-04):
 * - [FEATURE] Notifica connessione/disconnessione BLE sul log seriale
 * - [FEATURE] Flag bleConnesso per monitorare stato connessione
 * - [FEATURE] Feedback visivo LED blu su connessione BLE
 * - [UX] Log STATUS ora mostra stato BLE: "BLE:ON" o "BLE:OFF"
 *
 * CHANGELOG v8.7 (2025-01-03):
 * - [FIX] LDR debouncing ridotto: 5→3 campioni, 300ms→200ms (risolve mancato rilevamento)
 * - [FIX] LED RGB configurabile: common cathode/anode via LED_RGB_INVERTED
 * - [FIX CRITICAL] FSM bloccato in RIPOSO: sonar ora campiona ogni 500ms in RIPOSO (era 5s)
 * - [PERFORMANCE] Sonar adattivo: 500ms in RIPOSO (reattivo), 5s in altri stati (efficiente)
 * - [FIX] LCD residui countdown: padding 16 caratteri con spazi su tutte le stringhe
 * - [CLEANUP] Rimossi log debug LDR verbosi - output pulito
 * - [FIX CRITICAL] Sonar bloccato su distanze vicine: filtro anti-spike ora asimmetrico
 * - [ALGORITHM] Permette allontanamenti rapidi, blocca solo avvicinamenti > 150cm
 *
 * CHANGELOG v8.6:
 * - [PERFORMANCE] Sonar campionato ogni 5s invece che ogni 100ms (50x riduzione overhead)
 * - [CLEANUP] Eliminati log debug sonar (timeout, spike) - output pulito
 * - [UX] Log compatto su singola riga invece di box ASCII (12 righe → 1 riga)
 * - [DEBUG] Monitor ogni 2s: stato, credito, prodotto, LDR, dist, temp, hum, scorte
 * - [STABILITY] Ritorno a configurazione pin stabile pre-v8.8 (ultimo funzionante)
 * - [PIN] Configurazione: LDR=A2, ECHO=D9, TRIG=A1, DHT=D4, SERVO=D5, BUZZER=D2
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

// ======================================================================================
// CONFIGURAZIONE PIN HARDWARE
// ======================================================================================
// Mappa tutti i pin utilizzati sul microcontrollore STM32 Nucleo F401RE
// Questa configurazione è ottimizzata per raggruppare i dispositivi vicini fisicamente

// --- Sensore Ultrasuoni HC-SR04 (rilevamento presenza utente) ---
#define PIN_TRIG    A1          // Trigger: invia impulso ultrasuoni
#define PIN_ECHO    D9          // Echo: riceve risposta ultrasuoni (InterruptIn per precisione timing)

// --- Sensore Fotoresistenza LDR (rilevamento monete) ---
#define PIN_LDR     A2          // Ingresso analogico: valore aumenta quando moneta blocca luce

// --- Sensore Temperatura/Umidità DHT11 ---
#define PIN_DHT     D4          // Pin bidirezionale: lettura temp/hum tramite protocollo proprietario DHT

// --- Attuatori ---
#define PIN_SERVO   D5          // Servomotore SG90: dispensa prodotti (PWM 1-2ms)
#define PIN_BUZZER  D2          // Buzzer piezoelettrico: feedback sonoro

// --- Display LCD 16x2 I2C (interfaccia utente) ---
#define PIN_LCD_SDA D14         // I2C Data (pin hardware fisso)
#define PIN_LCD_SCL D15         // I2C Clock (pin hardware fisso)
                                // Indirizzo I2C: 0x4E (adapter PCF8574)

// ======================================================================================
// PARAMETRI DI CONFIGURAZIONE SISTEMA
// ======================================================================================
// Tutti i parametri critici del sistema: soglie sensori, timeout, prezzi prodotti

// --- Soglie Sensore LDR (rilevamento monete) ---
// ALGORITMO SPIKE DETECTION: rileva variazioni improvvise rispetto al baseline
#define SOGLIA_LDR_DELTA_SCATTO 20  // Delta % sopra baseline per rilevare moneta (spike +20%)
#define SOGLIA_LDR_DELTA_RESET   5  // Delta % sotto baseline per resettare (spike < +5%)
#define LDR_BASELINE_ALPHA      10  // Coefficiente media mobile (1-10, più alto = più reattivo)

// --- Soglie Sensore Ultrasuoni (rilevamento presenza utente) ---
#define DISTANZA_ATTIVA   40    // Distanza in cm sotto la quale utente è considerato presente
                                // Trigger transizione RIPOSO → ATTESA_MONETA

// --- Soglie Temperatura (protezione sistema) ---
#define SOGLIA_TEMP       28    // Temperatura in °C sopra la quale va in stato ERRORE
                                // Previene surriscaldamento componenti

// --- Timeout Sistema ---
#define TIMEOUT_RESTO_AUTO 30000000  // 30 secondi in microsecondi
                                     // Tempo massimo attesa utente prima di resto automatico
// TIMEOUT_EROGAZIONE_AUTO rimosso in v8.4:
// Erogazione ora richiede SEMPRE conferma esplicita tramite comando BLE 10

// --- Debouncing LDR (anti-rimbalzo lettura monete) ---
#define LDR_DEBOUNCE_SAMPLES 3      // Campioni consecutivi richiesti (ridotto da 5 a 3)
#define LDR_DEBOUNCE_TIME_US 200000 // Tempo minimo 200ms (ridotto da 300ms)
                                    // Ottimizzato per compensare oscillazioni valore LDR

// --- Prezzi Prodotti (in EUR) ---
#define PREZZO_ACQUA      1     // Bottiglia acqua: 1 EUR
#define PREZZO_SNACK      2     // Snack confezionato: 2 EUR
#define PREZZO_CAFFE      1     // Caffè: 1 EUR
#define PREZZO_THE        2     // The: 2 EUR
int prezzoSelezionato = PREZZO_ACQUA;  // Prodotto selezionato correntemente
int idProdotto = 1;                    // ID prodotto: 1=ACQUA, 2=SNACK, 3=CAFFE, 4=THE

// --- Filtri FSM (stabilità transizioni stati) ---
#define FILTRO_INGRESSO   5     // Cicli consecutivi < 40cm richiesti per RIPOSO → ATTESA_MONETA
#define FILTRO_USCITA     20    // Cicli consecutivi > 60cm richiesti per ATTESA_MONETA → RIPOSO

// ======================================================================================
// CONFIGURAZIONE BLUETOOTH LOW ENERGY (BLE)
// ======================================================================================
// UUID identificativi univoci per servizio e caratteristiche GATT
// Permette comunicazione wireless con app Android/iOS

const UUID VENDING_SERVICE_UUID((uint16_t)0xA000);  // Servizio principale distributore
const UUID TEMP_CHAR_UUID((uint16_t)0xA001);        // Caratteristica temperatura (notify)
const UUID STATUS_CHAR_UUID((uint16_t)0xA002);      // Caratteristica stato (6 byte): [credito, stato, scorte[4]]
const UUID HUM_CHAR_UUID((uint16_t)0xA003);         // Caratteristica umidità (notify)
const UUID CMD_CHAR_UUID((uint16_t)0xA004);         // Caratteristica comandi (write):
                                                    // 1-4=selezione prodotto, 9=annulla, 10=conferma, 11=rifornimento

// ======================================================================================
// MACCHINA A STATI FINITI (FSM - Finite State Machine)
// ======================================================================================
// Gestisce il flusso operativo del distributore automatico
// Transizioni: RIPOSO ↔ ATTESA_MONETA → EROGAZIONE → RESTO → RIPOSO
//              └─────────────────────→ ERRORE (temperatura alta)

enum Stato {
    RIPOSO,         // Utente lontano, distributore in idle (verde)
    ATTESA_MONETA,  // Utente vicino, attende inserimento monete (ciano/magenta/giallo)
    EROGAZIONE,     // Dispensing prodotto in corso (servo attivo)
    RESTO,          // Restituzione resto/credito residuo
    ERRORE          // Errore sistema (temperatura > soglia, blocco operazioni)
};
Stato statoCorrente = RIPOSO;       // Stato attuale FSM
Stato statoPrecedente = ERRORE;     // Stato precedente (per rilevare cambi stato)

// ======================================================================================
// OGGETTI DRIVER HARDWARE (Mbed OS)
// ======================================================================================
// Istanze delle classi Mbed per interfacciamento con periferiche hardware

TextLCD lcd(PIN_LCD_SDA, PIN_LCD_SCL, 0x4E);  // Display LCD 16x2 I2C (addr 0x4E = 0x27 << 1)
DigitalOut trig(PIN_TRIG);                    // HC-SR04: trigger ultrasuoni (impulso 10μs)
InterruptIn echo(PIN_ECHO);                   // HC-SR04: echo risposta (interrupt driven per timing preciso)
DigitalInOut dht(PIN_DHT);                    // DHT11: pin bidirezionale (input/output switching)
PwmOut servo(PIN_SERVO);                      // SG90: servomotore PWM 50Hz (duty 1-2ms)
AnalogIn ldr(PIN_LDR);                        // LDR: fotoresistenza (ADC 0-3.3V → 0-100%)
DigitalOut buzzer(PIN_BUZZER);                // Buzzer: feedback sonoro (HIGH=suona)
DigitalIn tastoAnnulla(PC_13);                // Pulsante onboard Nucleo (pull-up interno)

// ======================================================================================
// LED RGB (feedback visivo stato sistema)
// ======================================================================================
// Indica stato FSM tramite colori: Verde=RIPOSO, Ciano=ACQUA, Magenta=SNACK, Giallo=CAFFE

#define LED_RGB_INVERTED 0  // Tipo LED RGB:
                            // 0 = Common Cathode (catodo comune a GND, anodi ai pin)
                            // 1 = Common Anode (anodo comune a VCC, catodi ai pin - logica invertita)

DigitalOut ledR(D6);  // LED rosso
DigitalOut ledG(D8);  // LED verde
DigitalOut ledB(A3);  // LED blu

/**
 * @brief Imposta colore LED RGB con supporto common cathode/anode
 * @param r Rosso: 1=acceso, 0=spento
 * @param g Verde: 1=acceso, 0=spento
 * @param b Blu: 1=acceso, 0=spento
 *
 * Esempi: setRGB(0,1,0)=verde, setRGB(1,0,1)=magenta, setRGB(1,1,0)=giallo
 */
void setRGB(int r, int g, int b) {
#if LED_RGB_INVERTED
    ledR = !r; ledG = !g; ledB = !b;  // Inverti logica per common anode
#else
    ledR = r; ledG = g; ledB = b;     // Logica diretta per common cathode
#endif
}

// ======================================================================================
// SERIALE USB (debug e logging)
// ======================================================================================
BufferedSerial pc(USBTX, USBRX, 9600);  // Comunicazione seriale USB @ 9600 baud
FileHandle *mbed::mbed_override_console(int fd) { return &pc; }  // Redirige printf() su USB

// ======================================================================================
// VARIABILI GLOBALI DI STATO
// ======================================================================================
// Mantengono lo stato corrente del sistema, sensori, timer

// --- Timer (misurazione tempi) ---
Timer sonarTimer;           // Misura durata impulso echo HC-SR04 (interrupt driven)
Timer timerUltimaMoneta;    // Tempo trascorso da ultima moneta inserita (timeout resto)
Timer timerStato;           // Durata permanenza nello stato corrente
Timer ldrDebounceTimer;     // Timer debouncing LDR (anti-rimbalzo)

// --- Sensore Ultrasuoni HC-SR04 ---
volatile uint64_t echoDuration = 0;  // Durata impulso echo in microsecondi (volatile: modificato da ISR)
int ultimaDistanzaValida = 100;      // Cache ultima distanza valida (filtro anti-spike)
                                     // Inizializzato a 100cm (distanza media ragionevole)

// --- Sensore LDR (rilevamento monete) ---
bool monetaInLettura = false;   // TRUE se moneta attualmente presente davanti a LDR
int ldrSampleCount = 0;         // Contatore campioni consecutivi sopra soglia (debouncing)
int ldrBaseline = 50;           // Baseline mobile LDR (inizializzato a 50%, si adatta automaticamente)
bool ldrBaselineInit = false;   // TRUE dopo prima inizializzazione baseline

// --- Sistema Credito e Pagamento ---
int credito = 0;                // Credito accumulato in EUR (somma monete inserite)
bool creditoResiduo = false;    // TRUE se c'è credito residuo da restituire

// --- Sensore DHT11 (temperatura/umidità) ---
int temp_int = 0;       // Temperatura in gradi Celsius (int)
int hum_int = 0;        // Umidità relativa percentuale (int)
bool dht_valid = false; // TRUE se ultima lettura DHT11 è valida (checksum OK)
Mutex dhtMutex;         // Mutex protezione accesso concorrente (thread DHT vs loop principale)

// --- Filtri FSM (stabilizzazione transizioni) ---
int contatorePresenza = 0;  // Contatore cicli consecutivi con utente presente (dist < 40cm)
int contatoreAssenza = 0;   // Contatore cicli consecutivi con utente assente (dist > 60cm)

// ======================================================================================
// GESTIONE SCORTE PRODOTTI
// ======================================================================================
// Array scorte: scorte[0]=dummy, scorte[1]=ACQUA, scorte[2]=SNACK, scorte[3]=CAFFE, scorte[4]=THE
int scorte[5] = {0, 5, 5, 5, 5};  // Inizializzazione: 5 pezzi per prodotto (stock pieno)
const int SCORTE_MAX = 5;         // Capacità massima magazzino per prodotto

// ======================================================================================
// WATCHDOG TIMER (sicurezza anti-hang)
// ======================================================================================
// Resetta microcontrollore se loop principale non esegue kick() entro 10 secondi
// Protezione contro blocchi software critici
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
// GESTORE EVENTI GAP (Connessione/Disconnessione BLE)
// ======================================================================================
bool bleConnesso = false;  // Flag stato connessione BLE

class VendingGapEventHandler : public ble::Gap::EventHandler {
public:
    void onConnectionComplete(const ble::ConnectionCompleteEvent &event) override {
        if (event.getStatus() == BLE_ERROR_NONE) {
            bleConnesso = true;
            printf("[BLE] ✓ Dispositivo CONNESSO\n");

            // Feedback visivo: lampeggio LED blu
            setRGB(0, 0, 1);  // Blu

            // Notifica connessione su LCD
            lcd.clear();
            wait_us(20000);
            lcd.setCursor(0, 0);
            lcd.printf("BLE CONNESSO!   ");
            wait_us(500);
            lcd.setCursor(0, 1);
            lcd.printf("App collegata   ");
            thread_sleep_for(1500);  // Mostra messaggio per 1.5 secondi

            setRGB(0, 1, 0);  // Torna verde
            lcd.clear();
            wait_us(20000);
        }
    }

    void onDisconnectionComplete(const ble::DisconnectionCompleteEvent &event) override {
        bleConnesso = false;
        printf("[BLE] ✗ Dispositivo DISCONNESSO\n");

        // Notifica disconnessione su LCD
        lcd.clear();
        wait_us(20000);
        lcd.setCursor(0, 0);
        lcd.printf("BLE DISCONNESSO ");
        wait_us(500);
        lcd.setCursor(0, 1);
        lcd.printf("App scollegata  ");
        thread_sleep_for(1500);  // Mostra messaggio per 1.5 secondi
        lcd.clear();
        wait_us(20000);

        // Riavvia advertising per nuove connessioni
        BLE::Instance().gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);
    }
};

static VendingGapEventHandler gap_handler;
static VendingServerEventHandler server_handler;

// ======================================================================================
// SENSORI - HC-SR04 ULTRASUONI (rilevamento presenza utente)
// ======================================================================================

/**
 * @brief Interrupt Service Routine - fronte di salita echo
 * Chiamata automaticamente quando pin ECHO passa da LOW a HIGH
 * Avvia timer per misurare durata impulso echo
 */
void echoRise() {
    sonarTimer.reset();  // Reset timer
    sonarTimer.start();  // Avvia conteggio microsecondi
}

/**
 * @brief Interrupt Service Routine - fronte di discesa echo
 * Chiamata automaticamente quando pin ECHO passa da HIGH a LOW
 * Stoppa timer e salva durata impulso in variabile volatile
 */
void echoFall() {
    sonarTimer.stop();  // Ferma timer
    echoDuration = sonarTimer.elapsed_time().count();  // Leggi durata in μs (volatile)
}

/**
 * @brief Legge distanza da sensore ultrasuoni HC-SR04 con filtri anti-spike avanzati
 * @return Distanza in centimetri (range 2-400cm)
 *
 * ALGORITMO MULTI-STAGE:
 * 1. Campionamento multiplo: 5 letture consecutive con media
 * 2. Filtro range valido: scarta letture < 2cm o > 400cm (fuori specifiche sensore)
 * 3. Filtro anti-spike ASIMMETRICO:
 *    - Permette allontanamenti rapidi (es: 20cm → 200cm OK)
 *    - Blocca solo avvicinamenti impossibili > 150cm (es: 200cm → 10cm BLOCCATO)
 * 4. Fallback: se nessuna lettura valida, mantiene ultima distanza nota
 *
 * FISICA HC-SR04:
 * - Velocità suono: 343 m/s = 0.0343 cm/μs
 * - Distanza = (tempo_andata_ritorno * velocità) / 2
 * - Timeout 50ms = 850cm max teorico (in pratica max affidabile ~400cm)
 */
int leggiDistanza() {
    int somma = 0;    // Somma campioni validi per calcolo media
    int validi = 0;   // Contatore campioni validi

    // FASE 1: CAMPIONAMENTO MULTIPLO (5 letture)
    // Riduce errori casuali, migliora precisione
    for(int i=0; i<5; i++) {
        // Genera impulso trigger 15μs (spec HC-SR04: min 10μs)
        trig = 0;
        wait_us(5);        // Assicura fronte di discesa pulito
        trig = 1;
        wait_us(15);       // Impulso HIGH 15μs
        trig = 0;

        // Attende risposta echo (max 15ms = ~250cm range attesa)
        wait_us(15000);

        // FASE 2: VALIDAZIONE TIMEOUT E RANGE
        // echoDuration scritto da ISR interrupt (echoFall)
        // Timeout 50000μs = 850cm max (oltre è timeout sensore)
        if (echoDuration > 0 && echoDuration < 50000) {
            // Formula fisica: distanza = (tempo_μs * velocità_suono_cm/μs) / 2
            int distanza = (int)(echoDuration * 0.0343f / 2.0f);

            // Filtro range sensore: HC-SR04 affidabile solo 2-400cm
            if (distanza >= 2 && distanza <= 400) {
                somma += distanza;  // Accumula per media
                validi++;
            }
        }
    }

    // FASE 3: GESTIONE NESSUNA LETTURA VALIDA
    // Se tutti e 5 i campioni sono invalidi (timeout/fuori range),
    // mantieni ultima distanza nota per evitare salti a zero
    if (validi == 0) {
        return ultimaDistanzaValida;  // Failsafe: usa cache
    }

    // Calcola media aritmetica campioni validi
    int media = somma / validi;

    // FASE 4: FILTRO ANTI-SPIKE ASIMMETRICO (v8.7 Final)
    // PROBLEMA RISOLTO: filtro simmetrico bloccava allontanamenti rapidi
    // SOLUZIONE: filtro asimmetrico distingue avvicinamenti da allontanamenti
    //
    // FISICA: utente può allontanarsi rapidamente (es: 20cm → 200cm in 500ms)
    //         ma avvicinamenti > 150cm in 500ms sono impossibili (spike sensore)
    //
    // ESEMPI:
    // ✓ 17cm → 150cm: media(150) > ultimaDist(17) - 150 = -133 → ACCETTATO
    // ✓ 20cm → 200cm: media(200) > ultimaDist(20) - 150 = -130 → ACCETTATO
    // ✓ 150cm → 80cm: differenza 70cm < 150cm → ACCETTATO (avvicinamento normale)
    // ✗ 200cm → 10cm: media(10) < ultimaDist(200) - 150 = 50 → BLOCCATO (spike!)
    if (media < ultimaDistanzaValida - 150) {
        // Spike negativo eccessivo: probabile errore sensore
        // Mantieni valore precedente per stabilità
        return ultimaDistanzaValida;
    }

    // FASE 5: AGGIORNA CACHE E RITORNA
    ultimaDistanzaValida = media;  // Salva per prossima chiamata
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
    static int logCounter = 0;
    static int dist = 100;  // Cache distanza

    watchdog.kick();

    int ldr_val = (int)(ldr.read() * 100);

    // Campiona distanza con frequenza variabile in base allo stato
    int sogliaDistanza = (statoCorrente == RIPOSO) ? 5 : 50;  // RIPOSO: ogni 500ms, altri stati: ogni 5s
    if (++counterDist >= sogliaDistanza) {
        counterDist = 0;
        dist = leggiDistanza();
    }

    // LOG COMPATTO: Stampa variabili su singola riga ogni 2 secondi (20 cicli @ 100ms)
    if (++logCounter >= 20) {
        logCounter = 0;
        dhtMutex.lock();
        int temp_copy = temp_int;
        int hum_copy = hum_int;
        dhtMutex.unlock();

        const char* nomiStati[] = {"RIPOSO", "ATTESA_MONETA", "EROGAZIONE", "RESTO", "ERRORE"};
        int ldrDelta = ldr_val - ldrBaseline;
        printf("[STATUS] %s | %-14s | €%-2d | P%d@%dEUR | LDR:%2d%%(B:%2d Δ:%+3d) | DIST:%3dcm | T:%2d°C H:%2d%% | A%d S%d C%d T%d\n",
               bleConnesso ? "BLE:ON " : "BLE:OFF",
               nomiStati[statoCorrente], credito, idProdotto, prezzoSelezionato,
               ldr_val, ldrBaseline, ldrDelta, dist, temp_copy, hum_copy,
               scorte[1], scorte[2], scorte[3], scorte[4]);
    }

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

    // SPIKE DETECTION LDR (algoritmo adattivo anti-luce ambiente)
    if (statoCorrente != ERRORE && statoCorrente != EROGAZIONE && statoCorrente != RESTO) {
        // FASE 1: Inizializza/aggiorna baseline mobile (media esponenziale mobile - EMA)
        // Baseline si adatta automaticamente alla luce ambiente
        if (!ldrBaselineInit) {
            ldrBaseline = ldr_val;  // Prima lettura: imposta baseline immediato
            ldrBaselineInit = true;
        } else if (!monetaInLettura) {
            // Aggiorna baseline SOLO quando non c'è moneta (evita distorsione)
            // EMA: baseline = baseline * (1 - α) + ldr_val * α, con α = LDR_BASELINE_ALPHA/100
            ldrBaseline = ((100 - LDR_BASELINE_ALPHA) * ldrBaseline + LDR_BASELINE_ALPHA * ldr_val) / 100;
        }

        // FASE 2: Calcola spike (differenza rispetto al baseline)
        int ldrDelta = ldr_val - ldrBaseline;

        // FASE 3: Rilevamento spike positivo (moneta blocca luce → LDR aumenta)
        if (ldrDelta > SOGLIA_LDR_DELTA_SCATTO && !monetaInLettura) {
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

                printf("[LDR] Moneta rilevata! Credito=%d EUR (val=%d%%, base=%d%%, Δ=+%d%%)\n",
                       credito, ldr_val, ldrBaseline, ldrDelta);
            }
        }
        // FASE 4: Reset quando spike rientra sotto soglia minima
        else if (ldrDelta < SOGLIA_LDR_DELTA_RESET) {
            if (monetaInLettura) {
                monetaInLettura = false;
                ldrDebounceTimer.stop();
                printf("[LDR] Reset moneta (val=%d%%, base=%d%%, Δ=%+d%%)\n",
                       ldr_val, ldrBaseline, ldrDelta);
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

            // Mostra stato credito e richiesta conferma (padding 16 caratteri)
            char buf[17];  // 16 caratteri + \0
            if (credito >= prezzoSelezionato) {
                // Credito sufficiente: mostra conferma con nome prodotto
                // Padding esplicito con spazi per evitare residui LCD
                const char* nomiProdotti[] = {"", "ACQUA", "SNACK", "CAFFE", "THE"};
                snprintf(buf, sizeof(buf), "Conf. x %s!      ", nomiProdotti[idProdotto]);
                buf[16] = '\0';  // Tronca esattamente a 16 caratteri
            } else if (credito > 0 && credito < prezzoSelezionato) {
                // Credito parziale: mostra quanto manca
                char temp[17];
                snprintf(temp, sizeof(temp), "Cr:%dE T:%02ds", credito, secondiMancanti);
                snprintf(buf, sizeof(buf), "%-16s", temp);
            } else {
                // Credito zero: mostra prodotto selezionato
                // Padding esplicito con spazi per evitare residui LCD
                if(idProdotto==1)      { snprintf(buf, sizeof(buf), "Ins.Mon x ACQUA "); buf[16] = '\0'; }
                else if(idProdotto==2) { snprintf(buf, sizeof(buf), "Ins.Mon x SNACK "); buf[16] = '\0'; }
                else if(idProdotto==3) { snprintf(buf, sizeof(buf), "Ins.Mon x CAFFE "); buf[16] = '\0'; }
                else                   { snprintf(buf, sizeof(buf), "Ins.Mon x THE   "); buf[16] = '\0'; }
            }
            lcd.printf("%s", buf);

            wait_us(500);
            lcd.setCursor(0, 1);
            wait_us(500);

            // Riga 2: credito/timeout o prezzo/scorte (padding 16 caratteri)
            char buf2[17];
            if(credito > 0) {
                // Mostra credito e timeout resto
                char temp[17];
                snprintf(temp, sizeof(temp), "Cr:%d/%d T:%02ds", credito, prezzoSelezionato, secondiMancanti);
                snprintf(buf2, sizeof(buf2), "%-16s", temp);
            } else {
                // Mostra prezzo e scorte prodotto selezionato
                const char* nomi[] = {"", "ACQUA", "SNACK", "CAFFE", "THE"};
                char temp[17];
                snprintf(temp, sizeof(temp), "%s:%dE Rim:%d", nomi[idProdotto], prezzoSelezionato, scorte[idProdotto]);
                snprintf(buf2, sizeof(buf2), "%-16s", temp);
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
    lcd.printf("BOOT v8.10 UX");
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
