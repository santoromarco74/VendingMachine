#include "TextLCD.h"

TextLCD::TextLCD(PinName sda, PinName scl, int i2cAddress) : _i2c(sda, scl) {
    _i2cAddress = i2cAddress; 
    _backlightVal = LCD_BACKLIGHT;
    _i2c.frequency(100000); // 100 kHz standard
    // begin() viene chiamata dopo nel main o esplicitamente, 
    // ma per sicurezza inizializziamo variabili base
    _displayfunction = LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS;
}

void TextLCD::begin() {
    _displayfunction = LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS;

    // Sequenza di inizializzazione speciale HD44780
    // Attesa iniziale > 40ms dopo accensione
    ThisThread::sleep_for(50ms); 
    
    expanderWrite(_backlightVal);
    ThisThread::sleep_for(1000ms); // Stabilizzazione

    // 1. Primo comando 0x03
    write4bits(0x03 << 4);
    ThisThread::sleep_for(5ms); // > 4.1ms
    
    // 2. Secondo comando 0x03
    write4bits(0x03 << 4);
    ThisThread::sleep_for(5ms); // > 100us
    
    // 3. Terzo comando 0x03
    write4bits(0x03 << 4); 
    // CORREZIONE QUI SOTTO: Usiamo wait_us invece di sleep_for
    wait_us(150); // > 100us, sleep_for non gestisce microsecondi
    
    // 4. Set a 4-bit mode (0x02)
    write4bits(0x02 << 4); 
    ThisThread::sleep_for(1ms);

    // Configurazione finale
    command(LCD_FUNCTIONSET | _displayfunction);
    
    _displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    command(LCD_DISPLAYCONTROL | _displaycontrol);
    
    clear();
    
    _displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    command(LCD_ENTRYMODESET | _displaymode);
    
    home();
}

void TextLCD::clear() {
    command(LCD_CLEARDISPLAY);
    ThisThread::sleep_for(2ms); // Clear richiede tempo
}

void TextLCD::home() {
    command(LCD_RETURNHOME);
    ThisThread::sleep_for(2ms); // Home richiede tempo
}

void TextLCD::setCursor(uint8_t col, uint8_t row) {
    int row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
    if (row > 1) row = 1; 
    command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

void TextLCD::noBacklight() {
    _backlightVal = LCD_NOBACKLIGHT;
    expanderWrite(0);
}

void TextLCD::backlight() {
    _backlightVal = LCD_BACKLIGHT;
    expanderWrite(0);
}

// Implementazione printf personalizzata
void TextLCD::printf(const char *format, ...) {
    char buffer[32]; 
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    print(buffer);
}

void TextLCD::print(const char *str) {
    while (*str) {
        write(*str++);
    }
}

void TextLCD::putc(char c) {
    write(c);
}

// --- Funzioni Low Level ---

void TextLCD::expanderWrite(uint8_t _data) {
    char data_write[1];
    data_write[0] = _data | _backlightVal;
    _i2c.write(_i2cAddress, data_write, 1);
}

void TextLCD::pulseEnable(uint8_t _data) {
    expanderWrite(_data | 0x04); // En high
    wait_us(1); 
    expanderWrite(_data & ~0x04); // En low
    wait_us(50); 
}

void TextLCD::write4bits(uint8_t value) {
    expanderWrite(value);
    pulseEnable(value);
}

void TextLCD::send(uint8_t value, uint8_t mode) {
    uint8_t highnib = value & 0xF0;
    uint8_t lownib = (value << 4) & 0xF0;
    write4bits(highnib | mode);
    write4bits(lownib | mode);
}

void TextLCD::command(uint8_t value) {
    send(value, 0);
}

void TextLCD::write(uint8_t value) {
    send(value, 1); // 1 = Rs high (Data)
}