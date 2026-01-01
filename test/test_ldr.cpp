/*
 * TEST LDR - Diagnostica valori in tempo reale
 *
 * Questo sketch legge continuamente l'LDR e stampa i valori
 * per aiutarti a calibrare le soglie SOGLIA_LDR_SCATTO e SOGLIA_LDR_RESET
 */

#include "mbed.h"

// Pin LDR (v8.9)
#define PIN_LDR A0

AnalogIn ldr(PIN_LDR);
BufferedSerial pc(USBTX, USBRX, 9600);
FileHandle *mbed::mbed_override_console(int fd) { return &pc; }

int main() {
    printf("\n=== TEST LDR - DIAGNOSTICA ===\n");
    printf("Pin LDR: A0\n");
    printf("Soglie attuali nel firmware:\n");
    printf("  - SOGLIA_LDR_SCATTO = 25\n");
    printf("  - SOGLIA_LDR_RESET  = 15\n\n");

    printf("ISTRUZIONI:\n");
    printf("1. Copri l'LDR con la mano (simula moneta che passa)\n");
    printf("2. Osserva il valore che legge\n");
    printf("3. Togli la mano (simula nessuna moneta)\n");
    printf("4. Osserva il valore che legge\n\n");

    printf("Avvio letture ogni 100ms...\n\n");

    int counter = 0;

    while(true) {
        // Leggi valore LDR (0.0 - 1.0)
        float ldr_float = ldr.read();

        // Converti in percentuale (0-100)
        int ldr_val = (int)(ldr_float * 100);

        // Stampa ogni 100ms
        printf("[%04d] LDR: %3d%%  |  Float: %.3f  |  ",
               counter++, ldr_val, ldr_float);

        // Indica se supera le soglie attuali
        if (ldr_val > 25) {
            printf(">>> MONETA RILEVATA (>25)\n");
        } else if (ldr_val < 15) {
            printf("    Nessuna moneta (<15)\n");
        } else {
            printf("    Zona intermedia (15-25)\n");
        }

        ThisThread::sleep_for(100ms);
    }
}
