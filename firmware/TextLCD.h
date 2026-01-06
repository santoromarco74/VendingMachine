#ifndef TEXTLCD_H
#define TEXTLCD_H

#include "mbed.h"
#include <cstdarg>

// Comandi standard HD44780
#define LCD_CLEARDISPLAY 0x01
#define LCD_RETURNHOME 0x02
#define LCD_ENTRYMODESET 0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT 0x10
#define LCD_FUNCTIONSET 0x20
#define LCD_SETCGRAMADDR 0x40
#define LCD_SETDDRAMADDR 0x80

// Flag per display entry mode
#define LCD_ENTRYRIGHT 0x00
#define LCD_ENTRYLEFT 0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// Flag per display on/off control
#define LCD_DISPLAYON 0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON 0x02
#define LCD_CURSOROFF 0x00
#define LCD_BLINKON 0x01
#define LCD_BLINKOFF 0x00

// Flag per function set
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE 0x08
#define LCD_1LINE 0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS 0x00

// Flag per backlight (PCF8574 standard)
#define LCD_BACKLIGHT 0x08
#define LCD_NOBACKLIGHT 0x00

class TextLCD {
public:
    // Costruttore: pin SDA, pin SCL, indirizzo I2C (es. 0x27 << 1)
    TextLCD(PinName sda, PinName scl, int i2cAddress = 0x4E);

    void begin();
    void clear();
    void home();
    void setCursor(uint8_t col, uint8_t row);
    void noBacklight();
    void backlight();
    
    // Supporto per printf
    void printf(const char *format, ...);
    void print(const char *str);
    void putc(char c);

private:
    I2C _i2c;
    int _i2cAddress;
    uint8_t _backlightVal;
    uint8_t _displayfunction;
    uint8_t _displaycontrol;
    uint8_t _displaymode;

    void expanderWrite(uint8_t _data);
    void pulseEnable(uint8_t _data);
    void write4bits(uint8_t value);
    void send(uint8_t value, uint8_t mode);
    void command(uint8_t value);
    void write(uint8_t value);
};

#endif