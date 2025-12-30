/*
 * ======================================================================================
 * PROGETTO: Vending Machine IoT (BLE + RTOS + Kotlin Interface)
 * TARGET: ST Nucleo F401RE + Shield BLE IDB05A2
 * VERSIONE: v8.0 CLEAN (Stock Management + LCD Only)
 * ======================================================================================
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
// DRIVER SSD1306 OLED MINIMALE (128x32 / 128x64)
// ======================================================================================
class SSD1306 {
private:
    I2C *i2c;
    uint8_t addr;
    uint8_t width, height, pages;
    uint8_t cursorX, cursorY;
    bool initialized;

    // Font 5x7 minimale per caratteri ASCII 32-126
    static const uint8_t font5x7[95][5];

    void command(uint8_t cmd) {
        uint8_t data[2] = {0x00, cmd};
        i2c->write(addr, (char*)data, 2);
    }

    void data(uint8_t* buf, int len) {
        uint8_t temp[len + 1];
        temp[0] = 0x40;
        memcpy(temp + 1, buf, len);
        i2c->write(addr, (char*)temp, len + 1);
    }

public:
    SSD1306(PinName sda, PinName scl, uint8_t address = 0x3C, uint8_t h = 32)
        : width(128), height(h), cursorX(0), cursorY(0), initialized(false) {
        i2c = new I2C(sda, scl);
        i2c->frequency(400000);
        addr = address << 1;
        pages = height / 8;
    }

    // Condividi bus I2C esistente (per usare stesso bus di LCD)
    void attachToI2C(I2C* existingI2C, uint8_t address = 0x3C, uint8_t h = 32) {
        i2c = existingI2C;
        addr = address << 1;
        height = h;
        pages = height / 8;
    }

    bool init() {
        // Auto-detect indirizzo: prova 0x3C poi 0x3D
        for (uint8_t testAddr : {0x3C, 0x3D}) {
            addr = testAddr << 1;
            if (i2c->write(addr, NULL, 0) == 0) {
                printf("[OLED] Trovato SSD1306 a 0x%02X, altezza=%d\n", testAddr, height);

                // Sequenza init SSD1306
                command(0xAE); // Display off
                command(0xD5); command(0x80); // Clock div
                command(0xA8); command(height - 1); // Multiplex
                command(0xD3); command(0x00); // Display offset
                command(0x40); // Start line
                command(0x8D); command(0x14); // Charge pump ON
                command(0x20); command(0x00); // Addressing mode horizontal
                command(0xA1); // Segment remap
                command(0xC8); // COM scan direction
                command(0xDA); command(height == 32 ? 0x02 : 0x12); // COM pins
                command(0x81); command(0x8F); // Contrast
                command(0xD9); command(0xF1); // Precharge
                command(0xDB); command(0x40); // VCOM detect
                command(0xA4); // Display RAM
                command(0xA6); // Normal display
                command(0xAF); // Display on

                initialized = true;
                clear();
                return true;
            }
        }
        printf("[OLED] SSD1306 NON trovato (provato 0x3C, 0x3D)\n");
        return false;
    }

    void clear() {
        if (!initialized) return;
        cursorX = 0; cursorY = 0;
        command(0x21); command(0); command(127); // Column range
        command(0x22); command(0); command(pages - 1); // Page range
        uint8_t zero[128] = {0};
        for (int p = 0; p < pages; p++) data(zero, 128);
    }

    void setCursor(uint8_t x, uint8_t y) {
        cursorX = x; cursorY = y;
    }

    void printf(const char* format, ...) {
        if (!initialized) return;
        char buffer[64];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        for (int i = 0; buffer[i] != '\0'; i++) {
            drawChar(cursorX + i * 6, cursorY, buffer[i]);
        }
    }

    void drawChar(uint8_t x, uint8_t page, char c) {
        if (!initialized || c < 32 || c > 126) return;
        uint8_t charData[6];
        memcpy(charData, font5x7[c - 32], 5);
        charData[5] = 0x00; // Spacing

        command(0x21); command(x); command(x + 5);
        command(0x22); command(page); command(page);
        data(charData, 6);
    }
};

// Font 5x7 ridotto (solo caratteri essenziali: 0-9, A-Z, spazio, :, /, %, °, C, E)
const uint8_t SSD1306::font5x7[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // space
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // backslash
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
    {0x0C, 0x52, 0x52, 0x52, 0x3E}, // g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
    {0x7F, 0x10, 0x28, 0x44, 0x00}, // k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44}  // z
};

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
#define TIMEOUT_EROGAZIONE_AUTO 5000000

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
SSD1306 oled(PIN_LCD_SDA, PIN_LCD_SCL, 0x3C, 32);  // Condivide bus I2C, auto-detect 0x3C/0x3D
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

// --- GESTIONE SCORTE ---
int scorte[5] = {0, 5, 5, 5, 5}; // [0]=dummy, [1]=ACQUA, [2]=SNACK, [3]=CAFFE, [4]=THE
const int SCORTE_MAX = 5;

// Mutex per proteggere accesso DHT
Mutex dhtMutex;

// Mutex per proteggere BUS I2C condiviso (LCD 0x4E + OLED 0x3C)
// CRITICO: LCD e OLED condividono SDA/SCL - senza questo mutex si corrompono a vicenda!
Mutex i2cMutex;

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
        printf("[BLE] STATUS inviato: credito=%d, stato=%d, scorte=[%d,%d,%d,%d]\n",
               credit, state, scorte[1], scorte[2], scorte[3], scorte[4]);
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
                    i2cMutex.lock();
                    lcd.clear();
                    lcd.setCursor(0, 0); lcd.printf("ACQUA - 1.00EUR");
                    char buf[17];
                    snprintf(buf, 16, "Rimanenti: %d", scorte[1]);
                    lcd.setCursor(0, 1); lcd.printf("%s", buf);
                    i2cMutex.unlock();
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
                    i2cMutex.lock();
                    lcd.clear();
                    lcd.setCursor(0, 0); lcd.printf("SNACK - 2.00EUR");
                    char buf[17];
                    snprintf(buf, 16, "Rimanenti: %d", scorte[2]);
                    lcd.setCursor(0, 1); lcd.printf("%s", buf);
                    i2cMutex.unlock();
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
                    i2cMutex.lock();
                    lcd.clear();
                    lcd.setCursor(0, 0); lcd.printf("CAFFE - 1.00EUR");
                    char buf[17];
                    snprintf(buf, 16, "Rimanenti: %d", scorte[3]);
                    lcd.setCursor(0, 1); lcd.printf("%s", buf);
                    i2cMutex.unlock();
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
                    i2cMutex.lock();
                    lcd.clear();
                    lcd.setCursor(0, 0); lcd.printf("THE   - 2.00EUR");
                    char buf[17];
                    snprintf(buf, 16, "Rimanenti: %d", scorte[4]);
                    lcd.setCursor(0, 1); lcd.printf("%s", buf);
                    i2cMutex.unlock();
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
    void onConnectionComplete(const ble::ConnectionCompleteEvent &event) override {
        printf("[BLE] Client connesso - scheduling dati iniziali...\n");
        // Schedula invio dati dopo 2.5s per dare tempo all'app di abilitare notifiche
        // Usa event_queue per non bloccare il thread BLE
        event_queue.call_in(std::chrono::milliseconds(2500), []() {
            if (vendingServicePtr) {
                dhtMutex.lock();
                int temp_copy = temp_int;
                int hum_copy = hum_int;
                dhtMutex.unlock();

                vendingServicePtr->updateStatus(credito, statoCorrente);
                vendingServicePtr->updateTemp(temp_copy);
                vendingServicePtr->updateHum(hum_copy);
                printf("[BLE] Dati iniziali inviati: credito=%d, stato=%d, scorte=[%d,%d,%d,%d], T=%d, H=%d\n",
                       credito, statoCorrente, scorte[1], scorte[2], scorte[3], scorte[4], temp_copy, hum_copy);
            }
        });
    }

    void onDisconnectionComplete(const ble::DisconnectionCompleteEvent &event) override {
        BLE::Instance().gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);
        printf("[BLE] Client disconnesso\n");
    }
};

static VendingGapEventHandler gap_handler;
static VendingServerEventHandler server_handler;

// ======================================================================================
// SENSORI
// ======================================================================================
void echoRise() { sonarTimer.reset(); sonarTimer.start(); }
void echoFall() { sonarTimer.stop(); echoDuration = sonarTimer.elapsed_time().count(); }

int leggiDistanza() {
    int somma = 0;
    int validi = 0;
    for(int i=0; i<3; i++) {
        trig = 0; wait_us(2);
        trig = 1; wait_us(10);
        trig = 0; wait_us(10000);
        if (echoDuration > 0 && echoDuration < 30000) {
            somma += (int)(echoDuration * 0.0343f / 2.0f);
            validi++;
        }
    }
    return (validi == 0) ? 999 : somma / validi;
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
    static int blinkTimer = 0;

    watchdog.kick();

    int ldr_val = (int)(ldr.read() * 100);
    int dist = leggiDistanza();

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
            lcd.printf("ECO MODE BLE OK ");
            lcd.setCursor(0, 1);
            char buffer[17];
            dhtMutex.lock();
            snprintf(buffer, sizeof(buffer), "L:%02d D:%03d T:%02d", ldr_val, dist, temp_int);
            dhtMutex.unlock();
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
            uint64_t tempoPassato = timerUltimaMoneta.elapsed_time().count();

            int secondiMancanti = 0;
            if (tempoPassato < TIMEOUT_RESTO_AUTO) {
                secondiMancanti = (TIMEOUT_RESTO_AUTO - tempoPassato) / 1000000;
            }

            // Prepara dati LCD fuori dal lock (veloce)
            char riga1[17], riga2[17];
            if(idProdotto==1)      snprintf(riga1, sizeof(riga1), "ACQUA - 1.00EUR");
            else if(idProdotto==2) snprintf(riga1, sizeof(riga1), "SNACK - 2.00EUR");
            else if(idProdotto==3) snprintf(riga1, sizeof(riga1), "CAFFE - 1.00EUR");
            else                   snprintf(riga1, sizeof(riga1), "THE   - 2.00EUR");

            memset(riga2, ' ', 16);
            riga2[16] = '\0';
            if (credito >= prezzoSelezionato && !creditoResiduo && tempoPassato < TIMEOUT_EROGAZIONE_AUTO) {
                int secondiErogazione = (TIMEOUT_EROGAZIONE_AUTO - tempoPassato) / 1000000;
                snprintf(riga2, 16, "Cr:%dE Erog:%ds", credito, secondiErogazione);
            } else if (credito >= prezzoSelezionato && creditoResiduo) {
                char buf[17];
                snprintf(buf, sizeof(buf), "Cr:%dE T:%02ds", credito, secondiMancanti);
                lcd.printf("%s", buf);
            } else {
                if (credito > 0 && credito < prezzoSelezionato) {
                    char buf[17];
                    snprintf(buf, sizeof(buf), "Cr:%dE T:%02ds", credito, secondiMancanti);
                    lcd.printf("%s", buf);
                } else if (credito == 0) {
                    if(idProdotto==1)      lcd.printf("Ins.Mon x ACQUA ");
                    else if(idProdotto==2) lcd.printf("Ins.Mon x SNACK ");
                    else if(idProdotto==3) lcd.printf("Ins.Mon x CAFFE ");
                    else                   lcd.printf("Ins.Mon x THE   ");
                }
            }
            riga2[16] = '\0';

            // CACHE: Scrivi LCD SOLO se contenuto cambiato (riduce race conditions x10)
            bool needUpdate = (strcmp(riga1, lcdCacheRiga1) != 0 || strcmp(riga2, lcdCacheRiga2) != 0);
            if (needUpdate) {
                i2cMutex.lock();
                lcd.setCursor(0, 0); lcd.printf("%s", riga1);
                lcd.setCursor(0, 1); lcd.printf("%s", riga2);
                i2cMutex.unlock();

                // Aggiorna cache
                strncpy(lcdCacheRiga1, riga1, 17);
                strncpy(lcdCacheRiga2, riga2, 17);
            }

            if (tastoAnnulla == 0 && credito > 0) {
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
            else if (credito > 0 && credito < prezzoSelezionato && tempoPassato > TIMEOUT_RESTO_AUTO) {
                lcd.clear();
                wait_us(20000);
                lcd.printf("Tempo Scaduto!");
                printf("[TIMEOUT] Credito parziale - Resto: %dE\n", credito);
                thread_sleep_for(1000);
                statoCorrente = RESTO;
                timerStato.reset();
                timerStato.start();
                if (vendingServicePtr) vendingServicePtr->updateStatus(credito, statoCorrente);
            }
            else if (credito >= prezzoSelezionato && creditoResiduo && tempoPassato > TIMEOUT_RESTO_AUTO) {
                lcd.clear();
                wait_us(20000);
                lcd.printf("Tempo Scaduto!");
                printf("[TIMEOUT] Credito residuo - Resto: %dE\n", credito);
                thread_sleep_for(1000);
                statoCorrente = RESTO;
                timerStato.reset();
                timerStato.start();
                if (vendingServicePtr) vendingServicePtr->updateStatus(credito, statoCorrente);
            }
            else if (credito >= prezzoSelezionato && !creditoResiduo && tempoPassato > TIMEOUT_EROGAZIONE_AUTO) {
                statoCorrente = EROGAZIONE;
                timerStato.reset();
                timerStato.start();
            }
            else if (dist > (DISTANZA_ATTIVA + 20) && credito == 0) {
                if (++contatoreAssenza > FILTRO_USCITA) statoCorrente = RIPOSO;
            } else contatoreAssenza = 0;

            break;
        }

        case EROGAZIONE:
            // CONTROLLO SCORTE CRITICO: verifica PRIMA di erogare
            if (idProdotto >= 1 && idProdotto <= 4 && scorte[idProdotto] <= 0) {
                i2cMutex.lock();
                lcd.clear();
                lcd.setCursor(0, 0); lcd.printf("PRODOTTO");
                lcd.setCursor(0, 1); lcd.printf("ESAURITO!");
                i2cMutex.unlock();
                setRGB(1, 0, 0); // Rosso
                printf("[SCORTE] ERRORE: Tentativo erogazione con scorte=0 (prodotto %d)\n", idProdotto);
                thread_sleep_for(2000);
                // Vai a RESTO per rimborsare
                statoCorrente = RESTO;
                timerStato.reset();
                timerStato.start();
                if (vendingServicePtr) vendingServicePtr->updateStatus(credito, statoCorrente);
                break;
            }

            setRGB(1, 1, 0);
            lcd.setCursor(0, 0);
            lcd.printf("Erogazione...   ");
            lcd.setCursor(0, 1);
            lcd.printf("Attendere       ");
            if (timerStato.elapsed_time().count() < 2000000) {
                buzzer = 1;
                if (timerStato.elapsed_time().count() < 1000000) servo.write(0.10f);
                else servo.write(0.05f);
            } else {
                buzzer = 0;

                if (idProdotto >= 1 && idProdotto <= 4 && scorte[idProdotto] > 0) {
                    scorte[idProdotto]--;
                    printf("[EROGAZIONE] Prodotto %d erogato. Scorte: %d\n", idProdotto, scorte[idProdotto]);
                } else {
                    printf("[ERRORE] Erogazione con scorte invalide: prodotto=%d\n", idProdotto);
                }

                credito -= prezzoSelezionato;

                if (credito > 0) {
                    statoCorrente = ATTESA_MONETA;
                    timerUltimaMoneta.reset();
                    timerUltimaMoneta.start();
                    creditoResiduo = true;
                    lcd.clear();
                    wait_us(20000);
                    lcd.printf("Credito: %dE", credito);
                    thread_sleep_for(1500);
                } else {
                    statoCorrente = ATTESA_MONETA;
                    creditoResiduo = false;
                }

                if (vendingServicePtr) vendingServicePtr->updateStatus(credito, statoCorrente);
            }
            break;

        case RESTO: {
            setRGB(1, 0, 1);
            lcd.setCursor(0, 0);
            lcd.printf("Ritira Resto    ");
            lcd.setCursor(0, 1);
            char bufResto[17];
            snprintf(bufResto, sizeof(bufResto), "Importo: %d.00E ", credito);

            i2cMutex.lock();
            lcd.setCursor(0, 0); lcd.printf("Ritira Resto    ");
            lcd.setCursor(0, 1); lcd.printf("%s", bufResto);
            i2cMutex.unlock();

            if ((timerStato.elapsed_time().count() % 400000) < 200000) buzzer = 1;
            else buzzer = 0;

            if (timerStato.elapsed_time().count() > 3000000) {
                printf("[RESTO] Restituito: %dE\n", credito);
                buzzer = 0;
                credito = 0;
                statoCorrente = ATTESA_MONETA;
            }
            break;
        }

        case ERRORE: {
            blinkTimer++;
            if (blinkTimer % 2 == 0) { setRGB(1, 0, 0); buzzer = 1; }
            else { setRGB(0, 0, 0); buzzer = 0; }

            lcd.setCursor(0, 0);
            lcd.printf("! ALLARME TEMP !");
            lcd.setCursor(0, 1);
            char bufErr[17];
            dhtMutex.lock();
            snprintf(bufErr, sizeof(bufErr), "T:%dC > %dC", temp_int, SOGLIA_TEMP);
            int temp_check = temp_int;
            dhtMutex.unlock();

            i2cMutex.lock();
            lcd.setCursor(0, 0); lcd.printf("! ALLARME TEMP !");
            lcd.setCursor(0, 1); lcd.printf("%s", bufErr);
            i2cMutex.unlock();

            if (temp_check <= (SOGLIA_TEMP - 2)) statoCorrente = RIPOSO;
            break;
        }
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
    lcd.printf("BOOT v8.0 CLEAN");
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
