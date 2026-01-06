/*
 * ======================================================================================
 * PROGETTO: Vending Machine IoT (BLE + RTOS + Kotlin Interface)
 * TARGET: ST Nucleo F401RE + Shield BLE IDB05A2
 * VERSIONE: GOLDEN MASTER v7.1 (Fix LCD e GattServer EventHandler)
 * ======================================================================================
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

// --- PARAMETRI ---
#define SOGLIA_LDR_SCATTO 25
#define SOGLIA_LDR_RESET  15
#define DISTANZA_ATTIVA   40
#define SOGLIA_TEMP       28
#define TIMEOUT_RESTO_AUTO 30000000

// --- NUOVI PREZZI (MENU ESTESO) ---
#define PREZZO_ACQUA      1
#define PREZZO_SNACK      2
#define PREZZO_CAFFE      1 // Nuovo
#define PREZZO_THE        2 // Nuovo
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
volatile uint64_t echoDuration = 0;
bool monetaInLettura = false;
int credito = 0;
int temp_int = 0; int hum_int = 0;
int contatorePresenza = 0;
int contatoreAssenza = 0;

// ======================================================================================
// CLASSE BLE SERVICE
// ======================================================================================
class VendingService {
public:
    VendingService(BLE &_ble, int initial_temp, int initial_hum, int initial_credit) :
        ble(_ble),
        tempChar(TEMP_CHAR_UUID, &initial_temp, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY),
        humChar(HUM_CHAR_UUID, &initial_hum, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY),
        statusChar(STATUS_CHAR_UUID, &initial_credit, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY),
        cmdChar(CMD_CHAR_UUID, &initial_credit, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESPONSE)
    {
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
        uint8_t data[2] = {(uint8_t)credit, (uint8_t)state};
        ble.gattServer().write(statusChar.getValueHandle(), data, 2);
    }

    GattAttribute::Handle_t getCmdHandle() { return cmdChar.getValueHandle(); }

private:
    BLE &ble;
    ReadOnlyGattCharacteristic<int> tempChar;
    ReadOnlyGattCharacteristic<int> humChar;
    ReadOnlyGattCharacteristic<int> statusChar;
    WriteOnlyGattCharacteristic<int> cmdChar;
};

VendingService *vendingServicePtr = nullptr;
static EventQueue event_queue(16 * EVENTS_EVENT_SIZE);

// ======================================================================================
// GESTORE EVENTI GATT SERVER (NUOVO per Mbed OS 6)(4 PRODOTTI)
// ======================================================================================
class VendingServerEventHandler : public ble::GattServer::EventHandler {
    void onDataWritten(const GattWriteCallbackParams &params) override {
        if (vendingServicePtr && params.handle == vendingServicePtr->getCmdHandle()) {
            if (params.len > 0) {
                uint8_t cmd = params.data[0];

                // 1. ACQUA (Ciano)
                if (cmd == 1) {
                    idProdotto = 1; prezzoSelezionato = PREZZO_ACQUA;
                    lcd.clear(); lcd.printf("Sel: ACQUA (1E)");
                    setRGB(0, 1, 1);
                    timerUltimaMoneta.reset();
                }
                // 2. SNACK (Magenta)
                else if (cmd == 2) {
                    idProdotto = 2; prezzoSelezionato = PREZZO_SNACK;
                    lcd.clear(); lcd.printf("Sel: SNACK (2E)");
                    setRGB(1, 0, 1);
                    timerUltimaMoneta.reset();
                }
                // 3. CAFFE (Giallo = R+G)
                else if (cmd == 3) {
                    idProdotto = 3; prezzoSelezionato = PREZZO_CAFFE;
                    lcd.clear(); lcd.printf("Sel: CAFFE (1E)");
                    setRGB(1, 1, 0);
                    timerUltimaMoneta.reset();
                }
                // 4. THE (Verde)
                else if (cmd == 4) {
                    idProdotto = 4; prezzoSelezionato = PREZZO_THE;
                    lcd.clear(); lcd.printf("Sel: THE (2E)");
                    setRGB(0, 1, 0);
                    timerUltimaMoneta.reset();
                }
                // 9. ANNULLA
                else if (cmd == 9) {
                    if (credito > 0) {
                        lcd.clear(); lcd.printf("Annullato da App");
                        setRGB(1, 0, 1); // Viola momentaneo
                        thread_sleep_for(1000);
                        statoCorrente = RESTO; timerStato.reset(); timerStato.start();
                        vendingServicePtr->updateStatus(credito, statoCorrente);
                    }
                }
                // Feedback visivo immediato della selezione (opzionale: attendi 1s)
                if(cmd >= 1 && cmd <= 4) thread_sleep_for(1000);
            }
        }
    }
};
// ======================================================================================
// GESTORE EVENTI GAP (Disconnessione)
// ======================================================================================
class VendingGapEventHandler : public ble::Gap::EventHandler {
public:
    void onDisconnectionComplete(const ble::DisconnectionCompleteEvent &event) override {
        BLE::Instance().gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);
    }
};

static VendingGapEventHandler gap_handler;
static VendingServerEventHandler server_handler; // Istanza del nuovo handler server

// ======================================================================================
// SENSORI
// ======================================================================================
void echoRise() { sonarTimer.reset(); sonarTimer.start(); }
void echoFall() { sonarTimer.stop(); echoDuration = sonarTimer.elapsed_time().count(); }

int leggiDistanza() {
    int somma = 0; int validi = 0;
    for(int i=0; i<3; i++) {
        trig = 0; wait_us(2); trig = 1; wait_us(10); trig = 0; wait_us(10000);
        if (echoDuration > 0 && echoDuration < 30000) {
            somma += (int)(echoDuration * 0.0343f / 2.0f); validi++;
        }
    }
    return (validi == 0) ? 999 : somma / validi;
}

int pulseIn(int level) {
    int count = 0;
    while (dht == level) { if (count++ > 200) return -1; wait_us(1); }
    return count;
}

void aggiornaDHT() {
    dht.output(); dht = 0; thread_sleep_for(18); dht = 1; wait_us(30); dht.input();
    __disable_irq();
    if (pulseIn(1) == -1 || pulseIn(0) == -1 || pulseIn(1) == -1) { __enable_irq(); return; }
    uint8_t data[5] = {0};
    for (int i = 0; i < 40; i++) {
        if (pulseIn(0) == -1) { __enable_irq(); return; }
        int width = pulseIn(1);
        if (width == -1) { __enable_irq(); return; }
        if (width > 45) data[i/8] |= (1 << (7 - (i%8)));
    }
    __enable_irq();
    uint8_t calc = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
    if (data[4] == calc && (data[0] != 0 || data[2] != 0)) {
        hum_int = data[0]; temp_int = data[2];
    }
}

// ======================================================================================
// LOOP PRINCIPALE
// ======================================================================================
void updateMachine() {
    static int counterTemp = 0;
    static int blinkTimer = 0;

    int ldr_val = (int)(ldr.read() * 100);
    int dist = leggiDistanza();

    if (++counterTemp > 20) {
        aggiornaDHT();
        counterTemp = 0;
        if (vendingServicePtr) {
            vendingServicePtr->updateTemp(temp_int);
            vendingServicePtr->updateHum(hum_int);
        }
        if (temp_int >= SOGLIA_TEMP && statoCorrente != ERRORE) {
            statoCorrente = ERRORE; lcd.clear();
        }
    }

    if (statoCorrente != ERRORE && statoCorrente != EROGAZIONE && statoCorrente != RESTO) {
        if (ldr_val > SOGLIA_LDR_SCATTO && !monetaInLettura) {
            monetaInLettura = true; credito++; timerUltimaMoneta.reset();
            if (statoCorrente == RIPOSO) statoCorrente = ATTESA_MONETA;
            if (vendingServicePtr) vendingServicePtr->updateStatus(credito, statoCorrente);
        }
        if (ldr_val < SOGLIA_LDR_RESET && monetaInLettura) monetaInLettura = false;
    }

    if (statoCorrente != statoPrecedente) {
        lcd.clear(); buzzer = 0; statoPrecedente = statoCorrente;
        contatorePresenza = 0; contatoreAssenza = 0;
        if (vendingServicePtr) vendingServicePtr->updateStatus(credito, statoCorrente);
    }

    switch (statoCorrente) {
        case RIPOSO:
            setRGB(0, 1, 0); buzzer = 0;
            lcd.setCursor(0, 0); lcd.printf("ECO MODE BLE OK ");
            lcd.setCursor(0, 1); lcd.printf("L:%02d D:%03d T:%02d ", ldr_val, dist, temp_int);
            if (dist < DISTANZA_ATTIVA) { if (++contatorePresenza > FILTRO_INGRESSO) statoCorrente = ATTESA_MONETA; }
            else contatorePresenza = 0;
            break;

        case ATTESA_MONETA: {
            // Colore LED in base al prodotto
            if (idProdotto == 1) setRGB(0, 1, 1);      // Acqua
            else if (idProdotto == 2) setRGB(1, 0, 1); // Snack
            else if (idProdotto == 3) setRGB(1, 1, 0); // Caffe
            else setRGB(0, 1, 0);                      // The

            buzzer = 0; lcd.setCursor(0, 0);
            uint64_t tempoPassato = timerUltimaMoneta.elapsed_time().count();
            int secondiMancanti = (TIMEOUT_RESTO_AUTO - tempoPassato) / 1000000;
            if (secondiMancanti < 0) secondiMancanti = 0;

            if (credito >= prezzoSelezionato && tempoPassato < 2000000) lcd.printf("Attendi...      ");
            else {
                if (credito > 0) lcd.printf("Timeout in %02ds ", secondiMancanti);
                else {
                    // Nomi corti per stare in 16 caratteri
                    if(idProdotto==1)      lcd.printf("Ins.Mon x ACQUA ");
                    else if(idProdotto==2) lcd.printf("Ins.Mon x SNACK ");
                    else if(idProdotto==3) lcd.printf("Ins.Mon x CAFFE ");
                    else                   lcd.printf("Ins.Mon x THE   ");
                }
            }
            lcd.setCursor(0, 1);
            if(credito > 0) lcd.printf("Cr:%d/%d [Tasto Blu=Esc]", credito, prezzoSelezionato);
            else lcd.printf("Prz:%dE D:%03d T:%02d", prezzoSelezionato, dist, temp_int);

            if (tastoAnnulla == 0 && credito > 0) {
                lcd.clear(); lcd.printf("Annullato Manual"); thread_sleep_for(1000);
                statoCorrente = RESTO; timerStato.reset(); timerStato.start();
                if (vendingServicePtr) vendingServicePtr->updateStatus(credito, statoCorrente);
            }
            else if (credito > 0 && credito < prezzoSelezionato && tempoPassato > TIMEOUT_RESTO_AUTO) {
                lcd.clear(); lcd.printf("Tempo Scaduto!"); thread_sleep_for(1000);
                statoCorrente = RESTO; timerStato.reset(); timerStato.start();
                if (vendingServicePtr) vendingServicePtr->updateStatus(credito, statoCorrente);
            }
            else if (credito >= prezzoSelezionato && tempoPassato > 2000000) {
                statoCorrente = EROGAZIONE; timerStato.reset(); timerStato.start();
            }
            else if (dist > (DISTANZA_ATTIVA + 20) && credito == 0) {
                if (++contatoreAssenza > FILTRO_USCITA) statoCorrente = RIPOSO;
            } else contatoreAssenza = 0;
            break;
        }

        case EROGAZIONE:
            setRGB(1, 1, 0); lcd.setCursor(0, 0); lcd.printf("Erogazione...   "); lcd.setCursor(0, 1); lcd.printf("Attendere       ");
            if (timerStato.elapsed_time().count() < 2000000) {
                buzzer = 1; if (timerStato.elapsed_time().count() < 1000000) servo.write(0.10f); else servo.write(0.05f);
            } else {
                buzzer = 0; credito -= prezzoSelezionato;
                if (credito > 0) { statoCorrente = RESTO; timerStato.reset(); } else { statoCorrente = ATTESA_MONETA; }
            }
            break;

        case RESTO:
            setRGB(1, 0, 1); lcd.setCursor(0, 0); lcd.printf("Ritira Resto    "); lcd.setCursor(0, 1); lcd.printf("Monete: %d       ", credito);
            if ((timerStato.elapsed_time().count() % 400000) < 200000) buzzer = 1; else buzzer = 0;
            if (timerStato.elapsed_time().count() > 3000000) { buzzer = 0; credito = 0; statoCorrente = ATTESA_MONETA; }
            break;

        case ERRORE:
            blinkTimer++; if (blinkTimer % 2 == 0) { setRGB(1, 0, 0); buzzer = 1; } else { setRGB(0, 0, 0); buzzer = 0; }
            lcd.setCursor(0, 0); lcd.printf("! ALLARME TEMP !"); lcd.setCursor(0, 1); lcd.printf("T:%dC > %dC    ", temp_int, SOGLIA_TEMP);
            if (temp_int <= (SOGLIA_TEMP - 2)) statoCorrente = RIPOSO;
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

    // REGISTRAZIONE HANDLER CORRETTA PER MBED OS 6
    ble.gap().setEventHandler(&gap_handler);
    ble.gattServer().setEventHandler(&server_handler); // <--- Nuova registrazione corretta

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
    thread_sleep_for(200); servo.period_ms(20); servo.write(0.05f);
    echo.rise(&echoRise); echo.fall(&echoFall);
    lcd.begin(); lcd.backlight(); lcd.clear(); lcd.setCursor(0,0); lcd.printf("BOOT v7.1 FIXED");
    buzzer = 1; thread_sleep_for(100); buzzer = 0;
    timerUltimaMoneta.start();
    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(scheduleBleEventsProcessing);
    ble.init(bleInitComplete);
    event_queue.dispatch_forever();
}
