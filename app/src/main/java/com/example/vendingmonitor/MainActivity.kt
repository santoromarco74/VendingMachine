package com.example.vendingmonitor

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
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.app.ActivityCompat
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.*

// --- CONFIGURAZIONE UUID ---
val SERVICE_UUID: UUID = UUID.fromString("0000A000-0000-1000-8000-00805f9b34fb")
val CHAR_TEMP_UUID: UUID = UUID.fromString("0000A001-0000-1000-8000-00805f9b34fb")
val CHAR_STATUS_UUID: UUID = UUID.fromString("0000A002-0000-1000-8000-00805f9b34fb")
val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

class MainActivity : ComponentActivity() {

    // Stati UI
    private var tempState by mutableIntStateOf(0)
    private var creditState by mutableIntStateOf(0)
    private var machineState by mutableIntStateOf(0)
    private var connectionStatus by mutableStateOf("Disconnesso")
    private var isScanning by mutableStateOf(false)

    private var bluetoothGatt: BluetoothGatt? = null
    private val mainHandler = Handler(Looper.getMainLooper())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            // Tema Scuro forzato per look "Tech"
            MaterialTheme(colorScheme = darkColorScheme()) {
                Surface(modifier = Modifier.fillMaxSize(), color = Color(0xFF121212)) {
                    VendingDashboard()
                }
            }
        }
    }

    @Composable
    fun VendingDashboard() {
        val permissionsLauncher = rememberLauncherForActivityResult(
            ActivityResultContracts.RequestMultiplePermissions()
        ) { perms ->
            if (perms.values.all { it }) startScan()
            else Toast.makeText(this, "Permessi necessari!", Toast.LENGTH_SHORT).show()
        }

        Column(
            modifier = Modifier.fillMaxSize().padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.SpaceBetween // Distribuisce gli elementi
        ) {
            // --- HEADER ---
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text(
                    "VENDING MONITOR",
                    fontSize = 26.sp,
                    fontWeight = FontWeight.Black,
                    color = Color(0xFFBB86FC),
                    letterSpacing = 2.sp
                )
                Text(
                    "IoT Control Panel",
                    fontSize = 14.sp,
                    color = Color.Gray
                )
            }

            // --- STATUS INDICATOR (CERCHIO CENTRALE) ---
            StatusIndicator()

            // --- GRIGLIA DATI ---
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                // Card Temperatura
                SensorCard(
                    title = "TEMPERATURA",
                    value = "$tempState°C",
                    icon = "🌡️",
                    color = Color(0xFFFF9800), // Arancione
                    modifier = Modifier.weight(1f)
                )
                // Card Credito
                SensorCard(
                    title = "CREDITO",
                    value = "$creditState,00 €",
                    icon = "🪙",
                    color = Color(0xFF03DAC5), // Turchese
                    modifier = Modifier.weight(1f)
                )
            }

            // --- CARD STATO MACCHINA ---
            MachineStateCard()

            // --- PULSANTE AZIONE (IL CUORE DELLA LOGICA) ---
            ActionButton(permissionsLauncher)
        }
    }

    // --- COMPONENTI GRAFICI MIGLIORATI ---

    @Composable
    fun StatusIndicator() {
        val colorStato = when {
            connectionStatus == "Connesso" -> Color(0xFF00E676) // Verde brillante
            isScanning -> Color(0xFFFFEB3B) // Giallo
            else -> Color(0xFFCF6679) // Rosso spento
        }

        Card(
            shape = RoundedCornerShape(50),
            border = BorderStroke(2.dp, colorStato.copy(alpha = 0.5f)),
            colors = CardDefaults.cardColors(containerColor = Color(0xFF1E1E1E)),
            modifier = Modifier.fillMaxWidth().height(60.dp)
        ) {
            Box(contentAlignment = Alignment.Center, modifier = Modifier.fillMaxSize()) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    // Pallino colorato
                    Box(modifier = Modifier.size(12.dp).background(colorStato, CircleShape))
                    Spacer(modifier = Modifier.width(10.dp))
                    Text(
                        text = connectionStatus.uppercase(),
                        fontWeight = FontWeight.Bold,
                        color = Color.White,
                        letterSpacing = 1.sp
                    )
                }
            }
        }
    }

    @Composable
    fun SensorCard(title: String, value: String, icon: String, color: Color, modifier: Modifier) {
        Card(
            modifier = modifier.height(140.dp),
            colors = CardDefaults.cardColors(containerColor = Color(0xFF1E1E1E)),
            elevation = CardDefaults.cardElevation(8.dp)
        ) {
            Column(
                modifier = Modifier.fillMaxSize().padding(16.dp),
                verticalArrangement = Arrangement.SpaceBetween,
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Text(icon, fontSize = 32.sp)
                Text(value, fontSize = 28.sp, fontWeight = FontWeight.Bold, color = color)
                Text(title, fontSize = 10.sp, color = Color.Gray, fontWeight = FontWeight.Bold)
            }
        }
    }

    @Composable
    fun MachineStateCard() {
        val (testo, colore) = when(machineState) {
            0 -> "ECO MODE / RIPOSO" to Color(0xFF81C784)
            1 -> "INSERISCI MONETA" to Color(0xFF64B5F6)
            2 -> "EROGAZIONE IN CORSO..." to Color(0xFFFFD54F)
            3 -> "RESTO / RIMBORSO" to Color(0xFFBA68C8)
            4 -> "⚠️ ERRORE TEMP ⚠️" to Color(0xFFE57373)
            else -> "NON SINCRONIZZATO" to Color.Gray
        }

        Card(
            modifier = Modifier.fillMaxWidth().height(80.dp),
            colors = CardDefaults.cardColors(containerColor = Color(0xFF2C2C2C)),
            border = BorderStroke(1.dp, colore)
        ) {
            Column(
                modifier = Modifier.fillMaxSize(),
                verticalArrangement = Arrangement.Center,
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Text("STATO SISTEMA", fontSize = 10.sp, color = Color.Gray)
                Text(testo, fontSize = 18.sp, fontWeight = FontWeight.Bold, color = colore)
            }
        }
    }

    @Composable
    fun ActionButton(launcher: androidx.activity.result.ActivityResultLauncher<Array<String>>) {
        val buttonColor = when {
            connectionStatus == "Connesso" -> Color(0xFFCF6679) // Rosso (Disconnetti)
            isScanning -> Color(0xFFFFB74D) // Arancione (Stop)
            else -> Color(0xFFBB86FC) // Viola (Connetti)
        }

        val buttonText = when {
            connectionStatus == "Connesso" -> "DISCONNETTI"
            isScanning -> "STOP SCANSIONE"
            else -> "CONNETTI DISPOSITIVO"
        }

        Button(
            onClick = {
                when {
                    connectionStatus == "Connesso" -> disconnectDevice()
                    isScanning -> stopScan()
                    else -> checkPermissionsAndScan(launcher)
                }
            },
            colors = ButtonDefaults.buttonColors(containerColor = buttonColor),
            modifier = Modifier.fillMaxWidth().height(55.dp),
            shape = RoundedCornerShape(12.dp)
        ) {
            Text(buttonText, fontWeight = FontWeight.Bold, fontSize = 16.sp, color = Color.Black)
        }
    }

    // --- LOGICA BLUETOOTH AGGIORNATA ---

    @SuppressLint("MissingPermission")
    private fun disconnectDevice() {
        bluetoothGatt?.let { gatt ->
            println("DEBUG: Richiesta disconnessione manuale...")
            gatt.disconnect()
            gatt.close()
        }
        bluetoothGatt = null
        connectionStatus = "Disconnesso"
        // Reset Valori UI per pulizia
        tempState = 0
        creditState = 0
        machineState = 0
    }

    @SuppressLint("MissingPermission")
    private fun stopScan() {
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothManager.adapter.bluetoothLeScanner?.stopScan(object : ScanCallback(){})
        isScanning = false
        connectionStatus = "Disconnesso"
    }

    private fun checkPermissionsAndScan(launcher: androidx.activity.result.ActivityResultLauncher<Array<String>>) {
        val hasScan = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED
        } else true
        val hasConnect = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED
        } else true
        val hasLoc = ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED

        if (hasScan && hasConnect && hasLoc) {
            startScan()
        } else {
            val perms = mutableListOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                perms.add(Manifest.permission.BLUETOOTH_SCAN)
                perms.add(Manifest.permission.BLUETOOTH_CONNECT)
            }
            launcher.launch(perms.toTypedArray())
        }
    }

    @SuppressLint("MissingPermission")
    private fun startScan() {
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val scanner = bluetoothManager.adapter.bluetoothLeScanner
        if (scanner == null) {
            connectionStatus = "Bluetooth Spento!"
            return
        }

        connectionStatus = "Scansione..."
        isScanning = true

        scanner.startScan(object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult?) {
                val device = result?.device ?: return
                val nome = device.name ?: result.scanRecord?.deviceName

                if (nome != null && nome.contains("Vending", ignoreCase = true)) {
                    if (isScanning) {
                        scanner.stopScan(this)
                        isScanning = false
                        connectToDevice(device)
                    }
                }
            }
            override fun onScanFailed(errorCode: Int) {
                isScanning = false
                connectionStatus = "Errore Scan: $errorCode"
            }
        })

        mainHandler.postDelayed({
            if (isScanning) {
                scanner.stopScan(object : ScanCallback(){})
                isScanning = false
                if(connectionStatus == "Scansione...") connectionStatus = "Nessun Disp. Trovato"
            }
        }, 10000)
    }

    @SuppressLint("MissingPermission")
    private fun connectToDevice(device: BluetoothDevice) {
        runOnUiThread { connectionStatus = "Connessione..." }
        bluetoothGatt = device.connectGatt(this, false, object : BluetoothGattCallback() {
            private var charStatoDaAttivare: BluetoothGattCharacteristic? = null

            override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    runOnUiThread { connectionStatus = "Connesso" }
                    gatt?.discoverServices()
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    runOnUiThread { connectionStatus = "Disconnesso" }
                    gatt?.close() // Importante chiudere sempre
                }
            }

            override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
                if (status == BluetoothGatt.GATT_SUCCESS && gatt != null) {
                    val service = gatt.getService(SERVICE_UUID)
                    if (service != null) {
                        val charTemp = service.getCharacteristic(CHAR_TEMP_UUID)
                        val charStatus = service.getCharacteristic(CHAR_STATUS_UUID)
                        charStatoDaAttivare = charStatus

                        if (charTemp != null) enableNotification(gatt, charTemp)
                        else if (charStatus != null) enableNotification(gatt, charStatus)
                    }
                }
            }

            override fun onDescriptorWrite(gatt: BluetoothGatt?, descriptor: BluetoothGattDescriptor?, status: Int) {
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    if (descriptor?.characteristic?.uuid == CHAR_TEMP_UUID && charStatoDaAttivare != null) {
                        Thread.sleep(100)
                        enableNotification(gatt!!, charStatoDaAttivare!!)
                    }
                }
            }

            override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
                val data = characteristic.value
                if (characteristic.uuid == CHAR_TEMP_UUID) {
                    val temp = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN).int
                    runOnUiThread { tempState = temp }
                } else if (characteristic.uuid == CHAR_STATUS_UUID) {
                    if (data.size >= 2) {
                        runOnUiThread {
                            creditState = data[0].toInt()
                            machineState = data[1].toInt()
                        }
                    }
                }
            }
        })
    }

    @SuppressLint("MissingPermission")
    private fun enableNotification(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
        gatt.setCharacteristicNotification(characteristic, true)
        val descriptor = characteristic.getDescriptor(CCCD_UUID)
        if (descriptor != null) {
            descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
            gatt.writeDescriptor(descriptor)
        }
    }
}