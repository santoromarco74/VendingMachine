/*
 * ======================================================================================
 * APPLICAZIONE ANDROID: VENDING MONITOR
 * ======================================================================================
 * App di monitoraggio e controllo per distributore automatico IoT
 * Comunicazione via Bluetooth Low Energy (BLE) con microcontrollore STM32 Nucleo F401RE
 *
 * FUNZIONALITÀ PRINCIPALI:
 * - Connessione automatica a dispositivo BLE "VendingM"
 * - Selezione prodotti (Acqua, Snack, Caffè, The)
 * - Visualizzazione credito inserito e scorte prodotti
 * - Conferma acquisto e annullamento con resto
 * - Monitoraggio sensori (temperatura, umidità)
 * - Gestione stato macchina (Riposo, Attesa, Erogazione, Resto, Errore)
 * - Rifornimento scorte da remoto
 *
 * ARCHITETTURA:
 * - Jetpack Compose per UI reattiva
 * - Material Design 3 con dark theme
 * - Bluetooth GATT per comunicazione BLE
 * - Handler per operazioni asincrone
 *
 * VERSIONE: v2.0 - Font migliorato + Commenti dettagliati IT
 * ======================================================================================
 */

package com.example.vendingmonitor

// === IMPORT ANDROID FRAMEWORK ===
import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.Toast

// === IMPORT JETPACK COMPOSE ===
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.app.ActivityCompat

// === IMPORT UTILITY ===
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.*

// ======================================================================================
// UUID BLUETOOTH LOW ENERGY (BLE)
// ======================================================================================
// UUID standard per servizio GATT custom del distributore automatico
// Ogni caratteristica BLE ha un UUID univoco per identificare il tipo di dato

/**
 * UUID SERVIZIO PRINCIPALE (0xA000)
 * Raggruppa tutte le caratteristiche BLE del distributore
 */
val SERVICE_UUID: UUID = UUID.fromString("0000A000-0000-1000-8000-00805f9b34fb")

/**
 * UUID CARATTERISTICA TEMPERATURA (0xA001)
 * Invia notifiche con temperatura ambiente in °C (int 32-bit little-endian)
 */
val CHAR_TEMP_UUID: UUID = UUID.fromString("0000A001-0000-1000-8000-00805f9b34fb")

/**
 * UUID CARATTERISTICA STATUS (0xA002)
 * Invia notifiche con 6 byte: [credito, stato, scorte_acqua, scorte_snack, scorte_caffe, scorte_the]
 */
val CHAR_STATUS_UUID: UUID = UUID.fromString("0000A002-0000-1000-8000-00805f9b34fb")

/**
 * UUID CARATTERISTICA UMIDITÀ (0xA003)
 * Invia notifiche con umidità relativa in % (int 32-bit little-endian)
 */
val CHAR_HUM_UUID: UUID = UUID.fromString("0000A003-0000-1000-8000-00805f9b34fb")

/**
 * UUID CARATTERISTICA COMANDI (0xA004)
 * Riceve comandi write (1 byte):
 * - 1 = Seleziona ACQUA
 * - 2 = Seleziona SNACK
 * - 3 = Seleziona CAFFÈ
 * - 4 = Seleziona THE
 * - 9 = Annulla/Resto
 * - 10 = Conferma Acquisto
 * - 11 = Rifornimento Scorte
 */
val CMD_CHAR_UUID: UUID = UUID.fromString("0000A004-0000-1000-8000-00805f9b34fb")

/**
 * UUID CCCD (Client Characteristic Configuration Descriptor)
 * Descrittore standard BLE per abilitare/disabilitare notifiche
 */
val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

// ======================================================================================
// ACTIVITY PRINCIPALE
// ======================================================================================
/**
 * MainActivity
 * Activity principale dell'applicazione Android
 * Gestisce UI Compose, connessione BLE e interazione con distributore
 */
class MainActivity : ComponentActivity() {

    // === VARIABILI DI STATO UI (Composable State) ===
    // Queste variabili sono osservate da Jetpack Compose e causano recomposizione UI quando cambiano

    /**
     * Temperatura ambiente letta dal sensore DHT11 (°C)
     * Range: 0-50°C
     */
    private var tempState by mutableIntStateOf(0)

    /**
     * Umidità relativa letta dal sensore DHT11 (%)
     * Range: 20-90%
     */
    private var humState by mutableIntStateOf(0)

    /**
     * Credito totale inserito dall'utente (EUR)
     * Somma delle monete rilevate dal sensore LDR
     */
    private var creditState by mutableIntStateOf(0)

    /**
     * Stato corrente della macchina a stati finiti (FSM):
     * 0 = RIPOSO (verde - sistema pronto)
     * 1 = ATTESA_MONETA (blu - utente presente)
     * 2 = EROGAZIONE (giallo - dispensa prodotto)
     * 3 = RESTO (magenta - restituzione credito)
     * 4 = ERRORE (rosso - temperatura alta)
     */
    private var machineState by mutableIntStateOf(0)

    /**
     * Stato connessione BLE visualizzato nell'indicatore
     * Valori: "Disconnesso", "Scansione...", "Connessione...", "Connesso"
     */
    private var connectionStatus by mutableStateOf("Disconnesso")

    /**
     * Flag scansione BLE in corso
     * TRUE quando scanner BLE è attivo
     */
    private var isScanning by mutableStateOf(false)

    /**
     * Prodotto attualmente selezionato dall'utente
     * Valori: "ACQUA", "SNACK", "CAFFE", "THE"
     */
    private var selectedProduct by mutableStateOf("ACQUA")

    // === VARIABILI SCORTE PRODOTTI ===
    // Rappresentano il magazzino virtuale del distributore (pezzi disponibili)

    /** Scorte bottiglia acqua (0-5) */
    private var scorteAcqua by mutableIntStateOf(5)

    /** Scorte snack confezionato (0-5) */
    private var scorteSnack by mutableIntStateOf(5)

    /** Scorte caffè (0-5) */
    private var scorteCaffe by mutableIntStateOf(5)

    /** Scorte the (0-5) */
    private var scorteThe by mutableIntStateOf(5)

    // === BLUETOOTH GATT CLIENT ===
    /**
     * Oggetto BluetoothGatt per comunicazione con dispositivo BLE remoto
     * null quando disconnesso, non-null quando connesso
     */
    private var bluetoothGatt: BluetoothGatt? = null

    /**
     * Handler per eseguire operazioni sul main thread (UI thread)
     * Necessario perché callback BLE arrivano su thread GATT
     */
    private val mainHandler = Handler(Looper.getMainLooper())

    // ======================================================================================
    // LIFECYCLE ACTIVITY
    // ======================================================================================

    /**
     * onCreate() - Punto di ingresso dell'Activity
     * Chiamato quando Activity viene creata per la prima volta
     * Configura UI con Jetpack Compose e Material Design 3
     */
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Imposta contenuto UI con Compose
        setContent {
            // Applica tema Material Design 3 dark
            MaterialTheme(colorScheme = darkColorScheme()) {
                // Surface principale con padding per barre di sistema
                // FIX: statusBarsPadding e navigationBarsPadding evitano sovrapposizione con status bar
                Surface(
                    modifier = Modifier
                        .fillMaxSize()
                        .statusBarsPadding()      // Padding superiore per status bar
                        .navigationBarsPadding(), // Padding inferiore per navigation bar
                    color = Color(0xFF121212)     // Sfondo dark theme (#121212)
                ) {
                    VendingDashboard() // Componente root UI
                }
            }
        }
    }

    /**
     * onDestroy() - Pulizia risorse quando Activity viene distrutta
     * CRITICO: Disconnette e rilascia connessione BLE per evitare memory leak
     */
    @SuppressLint("MissingPermission") // Permessi già verificati in checkPermissionsAndScan()
    override fun onDestroy() {
        super.onDestroy()
        // Disconnette GATT e rilascia risorse BLE
        bluetoothGatt?.let {
            try {
                it.disconnect() // Disconnette connessione
                it.close()      // Rilascia risorse native
            } catch (e: Exception) {
                // Ignora eccezioni durante cleanup (dispositivo già disconnesso)
            }
        }
        bluetoothGatt = null
    }

    // ======================================================================================
    // UI COMPOSABLE - LAYOUT PRINCIPALE
    // ======================================================================================

    /**
     * VendingDashboard()
     * Componente Compose principale che rappresenta l'intera schermata
     *
     * LAYOUT:
     * - Header: titolo + indicatore connessione
     * - Zona Cliente: selezione prodotti + credito + pulsanti azione
     * - Zona Diagnostica: sensori temperatura/umidità + stato macchina
     * - Footer: pulsanti rifornimento e connessione
     */
    @Composable
    fun VendingDashboard() {
        // Launcher per richiedere permessi runtime (Android 6+)
        // Callback: startScan() se tutti i permessi concessi, altrimenti mostra Toast
        val permissionsLauncher = rememberLauncherForActivityResult(
            ActivityResultContracts.RequestMultiplePermissions()
        ) { perms ->
            if (perms.values.all { it }) startScan()
            else Toast.makeText(this, "Permessi necessari!", Toast.LENGTH_SHORT).show()
        }

        // Layout verticale principale con padding 16dp
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            // === HEADER ===
            Text(
                "VENDING MONITOR",
                fontSize = 22.sp,           // Font aumentato: 18sp → 22sp
                fontWeight = FontWeight.Black,
                color = Color(0xFFBB86FC), // Viola Material Design 3
                letterSpacing = 2.sp        // Spaziatura caratteri per effetto bold
            )
            Spacer(modifier = Modifier.height(8.dp))
            StatusIndicator() // Indicatore stato connessione (pallino + testo)
            Spacer(modifier = Modifier.height(12.dp))

            // === ZONA ACQUISTO (INTERFACCIA CLIENTE) ===
            Text(
                "SELEZIONA PRODOTTO",
                fontSize = 14.sp,           // Font aumentato: 12sp → 14sp
                color = Color.Gray,
                fontWeight = FontWeight.Bold,
                modifier = Modifier.align(Alignment.Start)
            )
            Spacer(modifier = Modifier.height(8.dp))

            // Card contenitore zona acquisto
            Card(
                colors = CardDefaults.cardColors(containerColor = Color(0xFF1E1E1E)),
                elevation = CardDefaults.cardElevation(4.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                Column(
                    modifier = Modifier.padding(14.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {

                    // === RIGA 1: ACQUA & SNACK ===
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceEvenly
                    ) {
                        ProductButton(
                            name = "ACQUA",
                            price = "1.00 €",
                            color = Color(0xFF00E5FF), // Ciano
                            isSelected = selectedProduct == "ACQUA",
                            stock = scorteAcqua
                        ) {
                            selectedProduct = "ACQUA"
                            writeCommand(1) // Invia comando BLE "seleziona ACQUA"
                        }
                        ProductButton(
                            name = "SNACK",
                            price = "2.00 €",
                            color = Color(0xFFFF4081), // Magenta
                            isSelected = selectedProduct == "SNACK",
                            stock = scorteSnack
                        ) {
                            selectedProduct = "SNACK"
                            writeCommand(2) // Invia comando BLE "seleziona SNACK"
                        }
                    }
                    Spacer(modifier = Modifier.height(12.dp))

                    // === RIGA 2: CAFFÈ & THE ===
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceEvenly
                    ) {
                        ProductButton(
                            name = "CAFFÈ",
                            price = "1.00 €",
                            color = Color(0xFFFFEB3B), // Giallo
                            isSelected = selectedProduct == "CAFFE",
                            stock = scorteCaffe
                        ) {
                            selectedProduct = "CAFFE"
                            writeCommand(3) // Invia comando BLE "seleziona CAFFÈ"
                        }
                        ProductButton(
                            name = "THE",
                            price = "2.00 €",
                            color = Color(0xFF69F0AE), // Verde chiaro
                            isSelected = selectedProduct == "THE",
                            stock = scorteThe
                        ) {
                            selectedProduct = "THE"
                            writeCommand(4) // Invia comando BLE "seleziona THE"
                        }
                    }

                    Spacer(modifier = Modifier.height(12.dp))

                    // === DISPLAY CREDITO ===
                    Text(
                        "CREDITO INSERITO",
                        fontSize = 13.sp,      // Font aumentato: 11sp → 13sp
                        color = Color.Gray
                    )
                    Text(
                        "$creditState,00 €",
                        fontSize = 42.sp,      // Font aumentato: 36sp → 42sp
                        fontWeight = FontWeight.Bold,
                        color = Color.White
                    )

                    // === PULSANTE CONFERMA ACQUISTO ===
                    Spacer(modifier = Modifier.height(8.dp))
                    Button(
                        onClick = {
                            Toast.makeText(
                                this@MainActivity,
                                "Invio conferma acquisto...",
                                Toast.LENGTH_SHORT
                            ).show()
                            writeCommand(10) // Invia comando BLE "conferma acquisto"
                        },
                        colors = ButtonDefaults.buttonColors(
                            containerColor = Color(0xFF4CAF50).copy(alpha = 0.2f)
                        ),
                        border = BorderStroke(1.dp, Color(0xFF4CAF50)),
                        shape = RoundedCornerShape(50),
                        modifier = Modifier.height(40.dp), // Altezza aumentata per font più grande
                        enabled = creditState >= 1 // Abilita solo se c'è credito
                    ) {
                        Text(
                            "CONFERMA ACQUISTO",
                            color = Color(0xFF4CAF50),
                            fontWeight = FontWeight.Bold,
                            fontSize = 13.sp  // Font aumentato: 11sp → 13sp
                        )
                    }

                    // === PULSANTE ANNULLA/RESTO ===
                    Spacer(modifier = Modifier.height(6.dp))
                    Button(
                        onClick = { writeCommand(9) }, // Invia comando BLE "annulla/resto"
                        colors = ButtonDefaults.buttonColors(
                            containerColor = Color(0xFFCF6679).copy(alpha = 0.2f)
                        ),
                        border = BorderStroke(1.dp, Color(0xFFCF6679)),
                        shape = RoundedCornerShape(50),
                        modifier = Modifier.height(40.dp)
                    ) {
                        Text(
                            "ANNULLA / RESTO",
                            color = Color(0xFFCF6679),
                            fontWeight = FontWeight.Bold,
                            fontSize = 13.sp  // Font aumentato: 11sp → 13sp
                        )
                    }
                }
            }

            Spacer(modifier = Modifier.height(12.dp))

            // === ZONA DIAGNOSTICA (MANUTENZIONE) ===
            Text(
                "DIAGNOSTICA",
                fontSize = 13.sp,           // Font aumentato: 11sp → 13sp
                color = Color.Gray,
                fontWeight = FontWeight.Bold,
                modifier = Modifier.align(Alignment.Start)
            )
            Spacer(modifier = Modifier.height(6.dp))

            // === SENSORI: TEMPERATURA & UMIDITÀ ===
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                SensorMiniCard(
                    title = "TEMP",
                    value = "$tempState°C",
                    color = Color(0xFFFF9800), // Arancione
                    modifier = Modifier.weight(1f)
                )
                SensorMiniCard(
                    title = "UMIDITÀ",
                    value = "$humState%",
                    color = Color(0xFF4FC3F7), // Azzurro
                    modifier = Modifier.weight(1f)
                )
            }

            Spacer(modifier = Modifier.height(8.dp))
            MachineStateBar() // Barra stato macchina (RIPOSO, ATTESA, EROGAZIONE, etc.)

            // === PULSANTE RIFORNIMENTO SCORTE ===
            Spacer(modifier = Modifier.height(8.dp))
            Button(
                onClick = {
                    Toast.makeText(
                        this@MainActivity,
                        "Rifornimento in corso...",
                        Toast.LENGTH_SHORT
                    ).show()
                    writeCommand(11) // Invia comando BLE "rifornimento"
                },
                colors = ButtonDefaults.buttonColors(
                    containerColor = Color(0xFF2196F3).copy(alpha = 0.2f)
                ),
                border = BorderStroke(1.dp, Color(0xFF2196F3)),
                shape = RoundedCornerShape(50),
                modifier = Modifier
                    .fillMaxWidth()
                    .height(40.dp)
            ) {
                Text(
                    "RIFORNIMENTO SCORTE",
                    color = Color(0xFF2196F3),
                    fontWeight = FontWeight.Bold,
                    fontSize = 13.sp  // Font aumentato: 11sp → 13sp
                )
            }

            // === PULSANTE CONNETTI/DISCONNETTI (FOOTER) ===
            Spacer(modifier = Modifier.weight(1f)) // Spinge pulsante in fondo
            ActionButton(permissionsLauncher)
        }
    }

    // ======================================================================================
    // UI COMPOSABLE - COMPONENTI CUSTOM
    // ======================================================================================

    /**
     * ProductButton()
     * Pulsante prodotto con nome, prezzo, colore e indicatore scorte
     *
     * COMPORTAMENTO:
     * - Sfondo colorato se selezionato
     * - Grigio se esaurito (stock=0)
     * - Disabilitato se esaurito
     *
     * @param name Nome prodotto (es: "ACQUA")
     * @param price Prezzo formattato (es: "1.00 €")
     * @param color Colore tema prodotto
     * @param isSelected TRUE se prodotto attualmente selezionato
     * @param stock Scorte rimanenti (0-5)
     * @param onClick Callback quando pulsante viene premuto
     */
    @Composable
    fun ProductButton(
        name: String,
        price: String,
        color: Color,
        isSelected: Boolean,
        stock: Int,
        onClick: () -> Unit
    ) {
        val isOutOfStock = stock == 0

        // Determina colore sfondo in base a stato
        val bg = when {
            isOutOfStock -> Color.DarkGray.copy(alpha = 0.3f) // Grigio se esaurito
            isSelected -> color.copy(alpha = 0.2f)            // Colorato se selezionato
            else -> Color.Transparent                          // Trasparente altrimenti
        }

        // Determina colore bordo
        val border = when {
            isOutOfStock -> BorderStroke(1.dp, Color.DarkGray)
            isSelected -> BorderStroke(2.dp, color)            // Bordo spesso se selezionato
            else -> BorderStroke(1.dp, Color.Gray)
        }

        // Determina colore testo
        val textColor = if (isOutOfStock) Color.DarkGray
        else if (isSelected) color
        else Color.White

        Card(
            modifier = Modifier
                .size(width = 140.dp, height = 90.dp)
                .clickable(enabled = !isOutOfStock) { onClick() }, // Disabilita click se esaurito
            colors = CardDefaults.cardColors(containerColor = bg),
            border = border
        ) {
            Column(
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = Arrangement.Center,
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Text(
                    name,
                    fontWeight = FontWeight.Bold,
                    fontSize = 18.sp,      // Font aumentato: 16sp → 18sp
                    color = textColor
                )
                Text(
                    price,
                    fontSize = 15.sp,      // Font aumentato: 13sp → 15sp
                    color = if (isOutOfStock) Color.DarkGray else Color.Gray
                )
                Spacer(modifier = Modifier.height(3.dp))
                Text(
                    text = if (isOutOfStock) "ESAURITO" else "Rim: $stock",
                    fontSize = 11.sp,      // Font aumentato: 9sp → 11sp
                    color = if (isOutOfStock) Color.Red else Color(0xFF4CAF50),
                    fontWeight = FontWeight.Bold
                )
            }
        }
    }

    /**
     * SensorMiniCard()
     * Card compatta per visualizzare sensore (temperatura o umidità)
     *
     * LAYOUT:
     * - Pallino colorato a sinistra
     * - Label sopra (es: "TEMP")
     * - Valore sotto (es: "23°C")
     *
     * @param title Etichetta sensore
     * @param value Valore formattato
     * @param color Colore tema
     * @param modifier Modificatore Compose per layout
     */
    @Composable
    fun SensorMiniCard(
        title: String,
        value: String,
        color: Color,
        modifier: Modifier
    ) {
        Card(
            modifier = modifier,
            colors = CardDefaults.cardColors(containerColor = Color(0xFF2C2C2C))
        ) {
            Row(
                modifier = Modifier.padding(12.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Indicatore colorato (pallino)
                Box(
                    modifier = Modifier
                        .size(8.dp)  // Dimensione aumentata: 7dp → 8dp
                        .background(color, CircleShape)
                )
                Spacer(modifier = Modifier.width(8.dp))
                Column {
                    Text(
                        title,
                        fontSize = 11.sp,  // Font aumentato: 9sp → 11sp
                        color = Color.Gray
                    )
                    Text(
                        value,
                        fontSize = 18.sp,  // Font aumentato: 16sp → 18sp
                        fontWeight = FontWeight.Bold,
                        color = Color.White
                    )
                }
            }
        }
    }

    /**
     * MachineStateBar()
     * Barra visualizzazione stato corrente della macchina a stati finiti (FSM)
     *
     * MAPPING STATI:
     * 0 = RIPOSO (verde)
     * 1 = ATTESA_MONETA (blu)
     * 2 = EROGAZIONE (giallo)
     * 3 = RESTO (magenta)
     * 4 = ERRORE (rosso)
     */
    @Composable
    fun MachineStateBar() {
        val (testo, colore) = when (machineState) {
            0 -> "SISTEMA PRONTO" to Color(0xFF81C784)      // Verde
            1 -> "IN ATTESA..." to Color(0xFF64B5F6)       // Blu
            2 -> "EROGAZIONE..." to Color(0xFFFFD54F)      // Giallo
            3 -> "RESTO / RIMBORSO" to Color(0xFFBA68C8)   // Magenta
            4 -> "ERRORE TEMP" to Color(0xFFE57373)        // Rosso
            else -> "--" to Color.Gray
        }

        Card(
            colors = CardDefaults.cardColors(
                containerColor = colore.copy(alpha = 0.2f)
            ),
            modifier = Modifier.fillMaxWidth()
        ) {
            Row(
                modifier = Modifier.padding(10.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    "STATO:",
                    fontSize = 13.sp,      // Font aumentato: 11sp → 13sp
                    color = Color.White,
                    fontWeight = FontWeight.Bold
                )
                Spacer(modifier = Modifier.width(6.dp))
                Text(
                    testo,
                    fontSize = 15.sp,      // Font aumentato: 13sp → 15sp
                    color = colore,
                    fontWeight = FontWeight.Bold
                )
            }
        }
    }

    /**
     * StatusIndicator()
     * Indicatore stato connessione BLE (pallino + testo)
     *
     * COLORI:
     * - Verde: connesso
     * - Giallo: scansione in corso
     * - Rosso: disconnesso
     */
    @Composable
    fun StatusIndicator() {
        val colorStato = when {
            connectionStatus == "Connesso" -> Color(0xFF00E676)  // Verde
            isScanning -> Color(0xFFFFEB3B)                      // Giallo
            else -> Color(0xFFCF6679)                            // Rosso
        }

        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(
                modifier = Modifier
                    .size(10.dp)
                    .background(colorStato, CircleShape)
            )
            Spacer(modifier = Modifier.width(8.dp))
            Text(
                connectionStatus,
                color = Color.Gray,
                fontSize = 14.sp  // Font aumentato: 12sp → 14sp
            )
        }
    }

    /**
     * ActionButton()
     * Pulsante principale CONNETTI/DISCONNETTI
     *
     * COMPORTAMENTO DINAMICO:
     * - Se disconnesso: "CONNETTI DISPOSITIVO" (viola)
     * - Se connesso: "DISCONNETTI" (rosso)
     *
     * @param launcher Launcher per richiedere permessi runtime
     */
    @Composable
    fun ActionButton(launcher: androidx.activity.result.ActivityResultLauncher<Array<String>>) {
        val buttonColor = if (connectionStatus == "Connesso")
            Color(0xFFCF6679)  // Rosso
        else
            Color(0xFFBB86FC)  // Viola

        val text = if (connectionStatus == "Connesso")
            "DISCONNETTI"
        else
            "CONNETTI DISPOSITIVO"

        Button(
            onClick = {
                if (connectionStatus == "Connesso")
                    disconnectDevice()
                else if (!isScanning)
                    checkPermissionsAndScan(launcher)
                else
                    stopScan()
            },
            colors = ButtonDefaults.buttonColors(containerColor = buttonColor),
            modifier = Modifier
                .fillMaxWidth()
                .height(48.dp), // Altezza aumentata per font più grande
            shape = RoundedCornerShape(8.dp)
        ) {
            Text(
                text,
                color = Color.Black,
                fontWeight = FontWeight.Bold,
                fontSize = 15.sp  // Font aumentato: 13sp → 15sp
            )
        }
    }

    // ======================================================================================
    // LOGICA BLUETOOTH LOW ENERGY (BLE)
    // ======================================================================================

    /**
     * writeCommand()
     * Invia comando write a caratteristica BLE CMD (0xA004)
     *
     * COMANDI DISPONIBILI:
     * - 1 = Seleziona ACQUA
     * - 2 = Seleziona SNACK
     * - 3 = Seleziona CAFFÈ
     * - 4 = Seleziona THE
     * - 9 = Annulla/Resto
     * - 10 = Conferma Acquisto
     * - 11 = Rifornimento Scorte
     *
     * NOTA: Gestisce differenze API Android 13+ (TIRAMISU) vs precedenti
     *
     * @param cmd Codice comando (1 byte)
     */
    @SuppressLint("MissingPermission")
    private fun writeCommand(cmd: Int) {
        android.util.Log.d("VendingMonitor", "writeCommand chiamato con cmd=$cmd")

        // Verifica connessione BLE attiva
        if (bluetoothGatt == null) {
            android.util.Log.e("VendingMonitor", "bluetoothGatt è null!")
            return
        }

        // Recupera servizio GATT principale
        val service = bluetoothGatt?.getService(SERVICE_UUID)
        if (service == null) {
            android.util.Log.e("VendingMonitor", "Servizio BLE non trovato!")
            return
        }

        // Recupera caratteristica comandi
        val charCmd = service.getCharacteristic(CMD_CHAR_UUID)
        if (charCmd == null) {
            android.util.Log.e("VendingMonitor", "Caratteristica CMD non trovata!")
            return
        }

        // Costruisce payload (1 byte: comando)
        val payload = byteArrayOf(cmd.toByte())
        android.util.Log.d("VendingMonitor", "Invio comando $cmd al dispositivo BLE")

        // Scrive caratteristica con API appropriata per versione Android
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            // Android 13+: nuova API con payload inline
            bluetoothGatt?.writeCharacteristic(
                charCmd,
                payload,
                BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
            )
        } else {
            // Android 12-: vecchia API con proprietà value
            @Suppress("DEPRECATION")
            charCmd.value = payload
            @Suppress("DEPRECATION")
            charCmd.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
            @Suppress("DEPRECATION")
            bluetoothGatt?.writeCharacteristic(charCmd)
        }

        android.util.Log.d("VendingMonitor", "Comando $cmd inviato con successo")
    }

    /**
     * resetSensorData()
     * Resetta tutti i dati sensori e stato UI a valori di default
     * Chiamato quando disconnessione BLE o errore comunicazione
     */
    private fun resetSensorData() {
        tempState = 0
        humState = 0
        creditState = 0
        machineState = 0
    }

    /**
     * disconnectDevice()
     * Disconnette dispositivo BLE e rilascia risorse GATT
     * Resetta anche UI e dati sensori
     */
    @SuppressLint("MissingPermission")
    private fun disconnectDevice() {
        bluetoothGatt?.let {
            it.disconnect() // Disconnette connessione BLE
            it.close()      // Rilascia risorse native
        }
        bluetoothGatt = null
        connectionStatus = "Disconnesso"
        resetSensorData()
    }

    /**
     * stopScan()
     * Ferma scansione BLE in corso
     * Chiamato quando utente annulla scansione o dopo timeout
     */
    @SuppressLint("MissingPermission")
    private fun stopScan() {
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothManager.adapter.bluetoothLeScanner?.stopScan(object : ScanCallback() {})
        isScanning = false
        connectionStatus = "Disconnesso"
    }

    /**
     * checkPermissionsAndScan()
     * Verifica permessi runtime e avvia scansione BLE
     * Se permessi mancanti, li richiede tramite launcher
     *
     * PERMESSI RICHIESTI:
     * - Android 12+: BLUETOOTH_SCAN, BLUETOOTH_CONNECT
     * - Tutti: ACCESS_FINE_LOCATION, ACCESS_COARSE_LOCATION
     *
     * @param launcher Launcher per richiedere permessi
     */
    private fun checkPermissionsAndScan(
        launcher: androidx.activity.result.ActivityResultLauncher<Array<String>>
    ) {
        // Verifica permesso BLUETOOTH_SCAN (Android 12+)
        val hasScan = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            ActivityCompat.checkSelfPermission(
                this,
                Manifest.permission.BLUETOOTH_SCAN
            ) == PackageManager.PERMISSION_GRANTED
        else
            true

        // Verifica permesso BLUETOOTH_CONNECT (Android 12+)
        val hasConnect = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            ActivityCompat.checkSelfPermission(
                this,
                Manifest.permission.BLUETOOTH_CONNECT
            ) == PackageManager.PERMISSION_GRANTED
        else
            true

        // Verifica permesso LOCATION (necessario per BLE scan)
        val hasLoc = ActivityCompat.checkSelfPermission(
            this,
            Manifest.permission.ACCESS_FINE_LOCATION
        ) == PackageManager.PERMISSION_GRANTED

        // Se tutti i permessi concessi, avvia scan
        if (hasScan && hasConnect && hasLoc) {
            startScan()
        } else {
            // Altrimenti richiedi permessi mancanti
            val perms = mutableListOf(
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.ACCESS_COARSE_LOCATION
            )
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                perms.add(Manifest.permission.BLUETOOTH_SCAN)
                perms.add(Manifest.permission.BLUETOOTH_CONNECT)
            }
            launcher.launch(perms.toTypedArray())
        }
    }

    /**
     * startScan()
     * Avvia scansione BLE per cercare dispositivo "VendingM"
     *
     * COMPORTAMENTO:
     * - Cerca dispositivi con nome contenente "Vending" (case-insensitive)
     * - Quando trovato, ferma scan e connette automaticamente
     * - Timeout 10 secondi se nessun dispositivo trovato
     */
    @SuppressLint("MissingPermission")
    private fun startScan() {
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val scanner = bluetoothManager.adapter.bluetoothLeScanner

        // Verifica se BLE scanner disponibile (Bluetooth attivo)
        if (scanner == null) {
            connectionStatus = "Bluetooth Spento!"
            return
        }

        connectionStatus = "Scansione..."
        isScanning = true

        // Avvia scan con callback
        scanner.startScan(object : ScanCallback() {
            /**
             * onScanResult()
             * Callback chiamato quando dispositivo BLE rilevato
             */
            override fun onScanResult(callbackType: Int, result: ScanResult?) {
                val device = result?.device ?: return

                // Recupera nome dispositivo (da name property o scanRecord)
                val nome = device.name ?: result.scanRecord?.deviceName

                // Cerca dispositivi con nome contenente "Vending"
                if (nome != null && nome.contains("Vending", ignoreCase = true)) {
                    // Dispositivo trovato! Ferma scan e connetti
                    if (isScanning) {
                        scanner.stopScan(this)
                        isScanning = false
                        connectToDevice(device)
                    }
                }
            }

            /**
             * onScanFailed()
             * Callback chiamato quando scan BLE fallisce
             */
            override fun onScanFailed(errorCode: Int) {
                isScanning = false
                connectionStatus = "Errore Scan: $errorCode"
            }
        })

        // Timeout 10 secondi: ferma scan se nessun dispositivo trovato
        mainHandler.postDelayed({
            if (isScanning) {
                scanner.stopScan(object : ScanCallback() {})
                isScanning = false
                if (connectionStatus == "Scansione...")
                    connectionStatus = "Nessun Disp. Trovato"
            }
        }, 10000)
    }

    /**
     * connectToDevice()
     * Connette a dispositivo BLE e configura callback GATT
     *
     * SEQUENZA CONNESSIONE:
     * 1. connectGatt() → connessione fisica
     * 2. onConnectionStateChange() → conferma connessione
     * 3. discoverServices() → enumera servizi GATT
     * 4. onServicesDiscovered() → abilita notifiche caratteristiche
     *
     * @param device Dispositivo BLE da connettere
     */
    @SuppressLint("MissingPermission")
    private fun connectToDevice(device: BluetoothDevice) {
        runOnUiThread { connectionStatus = "Connessione..." }

        // Connette a GATT server con callback
        bluetoothGatt = device.connectGatt(this, false, object : BluetoothGattCallback() {

            /**
             * onConnectionStateChange()
             * Callback chiamato quando stato connessione cambia
             *
             * @param gatt Istanza BluetoothGatt
             * @param status Codice stato (GATT_SUCCESS se OK)
             * @param newState Nuovo stato (STATE_CONNECTED o STATE_DISCONNECTED)
             */
            override fun onConnectionStateChange(
                gatt: BluetoothGatt?,
                status: Int,
                newState: Int
            ) {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    // Connessione riuscita
                    runOnUiThread { connectionStatus = "Connesso" }
                    gatt?.discoverServices() // Avvia discovery servizi GATT
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    // Disconnessione (volontaria o involontaria)
                    runOnUiThread {
                        connectionStatus = "Disconnesso"
                        resetSensorData() // Reset dati anche su disconnessione improvvisa
                    }
                    gatt?.close() // Rilascia risorse
                }
            }

            /**
             * onServicesDiscovered()
             * Callback chiamato quando servizi GATT sono stati enumerati
             * Abilita notifiche per caratteristiche temperatura, umidità, status
             *
             * @param gatt Istanza BluetoothGatt
             * @param status Codice stato (GATT_SUCCESS se OK)
             */
            override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
                if (status == BluetoothGatt.GATT_SUCCESS && gatt != null) {
                    val service = gatt.getService(SERVICE_UUID)
                    if (service != null) {
                        // Abilita notifica temperatura (prima caratteristica)
                        val charTemp = service.getCharacteristic(CHAR_TEMP_UUID)
                        if (charTemp != null) enableNotification(gatt, charTemp)
                    }
                }
            }

            /**
             * onDescriptorWrite()
             * Callback chiamato quando descrittore CCCD è stato scritto
             * Usato per sequenziare abilitazione notifiche multiple
             *
             * SEQUENZA:
             * 1. Abilita TEMP → onDescriptorWrite → abilita HUM
             * 2. Abilita HUM → onDescriptorWrite → abilita STATUS
             * 3. Abilita STATUS → onDescriptorWrite → seleziona ACQUA
             *
             * @param gatt Istanza BluetoothGatt
             * @param descriptor Descrittore scritto
             * @param status Codice stato (GATT_SUCCESS se OK)
             */
            override fun onDescriptorWrite(
                gatt: BluetoothGatt?,
                descriptor: BluetoothGattDescriptor?,
                status: Int
            ) {
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    val uuid = descriptor?.characteristic?.uuid
                    val service = gatt?.getService(SERVICE_UUID)

                    // Chain abilitazione notifiche con delay 100ms tra una e l'altra
                    // FIX: usa Handler invece di Thread.sleep per evitare blocco thread GATT
                    if (uuid == CHAR_TEMP_UUID) {
                        // Temperatura abilitata → abilita umidità
                        val charHum = service?.getCharacteristic(CHAR_HUM_UUID)
                        if (charHum != null) {
                            mainHandler.postDelayed({
                                enableNotification(gatt!!, charHum)
                            }, 100)
                        }
                    } else if (uuid == CHAR_HUM_UUID) {
                        // Umidità abilitata → abilita status
                        val charStatus = service?.getCharacteristic(CHAR_STATUS_UUID)
                        if (charStatus != null) {
                            mainHandler.postDelayed({
                                enableNotification(gatt!!, charStatus)
                            }, 100)
                        }
                    } else if (uuid == CHAR_STATUS_UUID) {
                        // Status abilitato → seleziona ACQUA come prodotto iniziale
                        mainHandler.post { writeCommand(1) }
                    }
                }
            }

            /**
             * onCharacteristicChanged() [Android 13+]
             * Callback chiamato quando caratteristica invia notifica (API 33+)
             *
             * @param gatt Istanza BluetoothGatt
             * @param characteristic Caratteristica che ha cambiato valore
             * @param value Nuovo valore (byte array)
             */
            override fun onCharacteristicChanged(
                gatt: BluetoothGatt,
                characteristic: BluetoothGattCharacteristic,
                value: ByteArray
            ) {
                // Questo metodo viene chiamato su Android 13+ (API 33)
                processData(characteristic.uuid, value)
            }

            /**
             * onCharacteristicChanged() [Android < 13]
             * Callback chiamato quando caratteristica invia notifica (API < 33)
             *
             * @param gatt Istanza BluetoothGatt
             * @param characteristic Caratteristica che ha cambiato valore
             */
            @Deprecated("Deprecated in Java")
            override fun onCharacteristicChanged(
                gatt: BluetoothGatt,
                characteristic: BluetoothGattCharacteristic
            ) {
                // Questo metodo viene chiamato su Android < 13
                @Suppress("DEPRECATION")
                processData(characteristic.uuid, characteristic.value)
            }
        })
    }

    /**
     * processData()
     * Processa dati ricevuti da notifiche BLE caratteristiche
     *
     * PARSING:
     * - TEMP (0xA001): 4 byte int32 little-endian → temperatura in °C
     * - HUM (0xA003): 4 byte int32 little-endian → umidità in %
     * - STATUS (0xA002): 6 byte → [credito, stato, scorte[4]]
     *
     * VALIDAZIONE:
     * - Verifica dimensione dati prima del parsing (previene crash)
     * - Conversione byte signed → unsigned con `and 0xFF`
     *
     * @param uuid UUID caratteristica che ha inviato notifica
     * @param data Dati ricevuti (byte array)
     */
    private fun processData(uuid: UUID, data: ByteArray) {
        // === TEMPERATURA ===
        if (uuid == CHAR_TEMP_UUID && data.size >= 4) {
            // Parsing int32 little-endian
            val temp = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN).int
            runOnUiThread { tempState = temp }
            android.util.Log.d("VendingMonitor", "Temp aggiornata: $temp°C")
        }
        // === UMIDITÀ ===
        else if (uuid == CHAR_HUM_UUID && data.size >= 4) {
            // Parsing int32 little-endian
            val hum = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN).int
            runOnUiThread { humState = hum }
            android.util.Log.d("VendingMonitor", "Umidità aggiornata: $hum%")
        }
        // === STATUS (credito + stato + scorte) ===
        else if (uuid == CHAR_STATUS_UUID) {
            android.util.Log.d(
                "VendingMonitor",
                "STATUS ricevuto: ${data.size} bytes = ${
                    data.joinToString(",") { (it.toInt() and 0xFF).toString() }
                }"
            )

            if (data.size >= 6) {
                // Parsing 6 byte: [credito, stato, scorta_acqua, scorta_snack, scorta_caffe, scorta_the]
                // FIX CRITICAL: `and 0xFF` converte byte signed → unsigned (0-255)
                runOnUiThread {
                    creditState = data[0].toInt() and 0xFF     // Byte 0: credito EUR
                    machineState = data[1].toInt() and 0xFF    // Byte 1: stato FSM
                    scorteAcqua = data[2].toInt() and 0xFF     // Byte 2: scorte acqua
                    scorteSnack = data[3].toInt() and 0xFF     // Byte 3: scorte snack
                    scorteCaffe = data[4].toInt() and 0xFF     // Byte 4: scorte caffè
                    scorteThe = data[5].toInt() and 0xFF       // Byte 5: scorte the
                    android.util.Log.d(
                        "VendingMonitor",
                        "Aggiornato: credito=$creditState, stato=$machineState, scorte=[$scorteAcqua,$scorteSnack,$scorteCaffe,$scorteThe]"
                    )
                }
            } else if (data.size >= 2) {
                // Fallback: parsing 2 byte legacy [credito, stato]
                // Supporta firmware vecchi che non inviano scorte
                runOnUiThread {
                    creditState = data[0].toInt() and 0xFF
                    machineState = data[1].toInt() and 0xFF
                    android.util.Log.d(
                        "VendingMonitor",
                        "Fallback 2-byte: credito=$creditState, stato=$machineState"
                    )
                }
            }
        }
    }

    /**
     * enableNotification()
     * Abilita notifiche per caratteristica BLE
     *
     * PROCEDURA:
     * 1. setCharacteristicNotification(true) → abilita localmente
     * 2. Scrivi CCCD descriptor con ENABLE_NOTIFICATION_VALUE → abilita su server
     *
     * NOTA: Gestisce differenze API Android 13+ (TIRAMISU) vs precedenti
     *
     * @param gatt Istanza BluetoothGatt
     * @param characteristic Caratteristica da abilitare
     */
    @SuppressLint("MissingPermission")
    private fun enableNotification(
        gatt: BluetoothGatt,
        characteristic: BluetoothGattCharacteristic
    ) {
        // Abilita notifiche localmente
        gatt.setCharacteristicNotification(characteristic, true)

        // Recupera descrittore CCCD (Client Characteristic Configuration Descriptor)
        val descriptor = characteristic.getDescriptor(CCCD_UUID)
        if (descriptor != null) {
            // Scrive CCCD con valore ENABLE_NOTIFICATION_VALUE (0x01 0x00)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                // Android 13+: nuova API con valore inline
                gatt.writeDescriptor(
                    descriptor,
                    BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                )
            } else {
                // Android 12-: vecchia API con proprietà value
                @Suppress("DEPRECATION")
                descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                @Suppress("DEPRECATION")
                gatt.writeDescriptor(descriptor)
            }
        }
    }
}
