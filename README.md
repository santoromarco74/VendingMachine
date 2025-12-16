# 🥤 Vending Machine IoT Monitor

![Project Status](https://img.shields.io/badge/Status-Completed-success)
![Platform](https://img.shields.io/badge/Platform-STM32%20Nucleo%20%7C%20Android-blue)
![Connectivity](https://img.shields.io/badge/Connectivity-Bluetooth%20Low%20Energy-orange)

Un sistema IoT completo per la gestione e il monitoraggio di un prototipo di Vending Machine.
Il progetto integra un firmware C++ basato su **Mbed OS** (eseguito su STM32 Nucleo) e una **Companion App Android** scritta in Kotlin/Jetpack Compose, che comunicano in tempo reale via **Bluetooth Low Energy (BLE)**.

## ✨ Funzionalità

### 🤖 Hardware (Firmware)
* **Macchina a Stati Finiti (FSM):** Gestisce logica complessa (Riposo, Attesa, Erogazione, Resto, Errore).
* **Sensori Intelligenti:**
    * Rilevamento monete tramite sensore di luce (LDR).
    * Rilevamento presenza utente tramite ultrasuoni (HC-SR04).
    * Monitoraggio ambientale (Temperatura/Umidità) con DHT11.
* **Attuatori:**
    * Servo motore per l'erogazione del prodotto.
    * Display LCD I2C per messaggi utente.
    * Feedback sonoro (Buzzer) e visivo (LED RGB).
* **Smart Features:**
    * Sistema di **Resto Automatico** (Timeout 30s).
    * Tasto di **Annullamento Manuale** (Pulsante Blu).
    * Blocco di sicurezza per sovra-temperatura.

### 📱 Software (Android App)
* **Interfaccia Moderna:** UI "Dark Tech" realizzata con **Jetpack Compose**.
* **Real-time Monitoring:** Visualizzazione live di Temperatura, Credito inserito e Stato della macchina.
* **BLE Management:**
    * Scansione automatica e filtrata dei dispositivi.
    * Gestione permessi Android 12+ (Scan/Connect) e Legacy (Location).
    * Logica di sottoscrizione notifiche sequenziale (per evitare congestione GATT).

---

## 🛠️ Architettura del Sistema

### Hardware Wiring (Pinout Nucleo F401RE)

| Componente | Pin Nucleo | Note |
| :--- | :--- | :--- |
| **BLE Shield** | Morpho | X-NUCLEO-IDB05A2 |
| **LCD I2C** | D14 (SDA), D15 (SCL) | Indirizzo 0x4E o 0x27 |
| **Trigger Ultrasuoni** | A1 | |
| **Echo Ultrasuoni** | D9 | Interrupt Safe |
| **Sensore LDR** | A2 | Analog In |
| **DHT11** | D4 | Pull-up interno |
| **Servo Motore** | D5 | PWM |
| **Buzzer** | D2 | Attivo Alto |
| **LED RGB** | D6 (R), D8 (G), A3 (B) | |
| **Tasto Annulla** | PC_13 | Tasto Blu utente |

### Protocollo BLE (GATT)

Il sistema espone un **Custom Service** con UUID `0xA000`.

| Caratteristica | UUID | Tipo | Descrizione |
| :--- | :--- | :--- | :--- |
| **Temperatura** | `0xA001` | Notify (Int32) | Temperatura in °C (Little Endian). |
| **Stato/Credito** | `0xA002` | Notify (Byte Array) | **Byte 0:** Credito attuale (€)<br>**Byte 1:** ID Stato (0=Eco, 1=Attesa, 2=Erog, 3=Resto, 4=Err) |

---

## 🚀 Installazione e Setup

### 1. Firmware (STM32)
1.  Aprire il progetto in **Keil Studio Cloud** o **Mbed Studio**.
2.  Importare le librerie necessarie:
    * `mbed-os`
    * `TextLCD`
    * `X_NUCLEO_IDB05A1` (Driver BLE)
3.  Compilare e flashare il file `main.cpp` sulla scheda Nucleo F401RE.

### 2. App Android
1.  Aprire la cartella `android-app` in **Android Studio** (Koala o superiore).
2.  Assicurarsi che il file `AndroidManifest.xml` abbia i permessi corretti.
3.  Collegare un dispositivo Android (con Debug USB attivo) e premere **Run**.
4.  Concedere i permessi di **Posizione** e **Dispositivi Vicini** al primo avvio.

---

## 📸 Screenshots

| Dashboard (Disconnesso) | Scansione & Connessione | Monitoraggio Live |
| :---: | :---: | :---: |
| ![Screen1](path/to/image1.png) | ![Screen2](path/to/image2.png) | ![Screen3](path/to/image3.png) |

*(Nota: Sostituisci i path qui sopra con gli screenshot reali della tua app)*

---

## 🧠 Logica Operativa (FSM)

Il cuore del sistema è una Macchina a Stati:

1.  **RIPOSO (Eco Mode):** LED Verde. Attende che qualcuno si avvicini (Ultrasuoni < 40cm).
2.  **ATTESA MONETA:** LED Blu. L'utente ha 30 secondi per inserire monete.
    * *Input:* Moneta (LDR) -> Incrementa credito.
    * *Input:* Tasto Blu -> Va a RESTO.
    * *Timeout:* 30s senza azioni -> Va a RESTO.
3.  **EROGAZIONE:** LED Giallo. Se Prezzo Raggiunto -> Muove Servo -> Scala Credito.
4.  **RESTO:** LED Viola. Restituisce il credito residuo o annullato.
5.  **ERRORE:** LED Rosso lampeggiante. Attivo se Temp > 28°C.

---

## 📄 Licenza

Progetto sviluppato a scopo didattico/prototipale.
Sentiti libero di usare il codice per i tuoi progetti IoT!

---
*Developed by Marco Santoro* 🚀
