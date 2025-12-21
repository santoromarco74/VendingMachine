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
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.*

// --- CONFIGURAZIONE UUID ---
val SERVICE_UUID: UUID = UUID.fromString("0000A000-0000-1000-8000-00805f9b34fb")
val CHAR_TEMP_UUID: UUID = UUID.fromString("0000A001-0000-1000-8000-00805f9b34fb")
val CHAR_STATUS_UUID: UUID = UUID.fromString("0000A002-0000-1000-8000-00805f9b34fb")
val CHAR_HUM_UUID: UUID = UUID.fromString("0000A003-0000-1000-8000-00805f9b34fb")
val CMD_CHAR_UUID: UUID = UUID.fromString("0000A004-0000-1000-8000-00805f9b34fb")
val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

class MainActivity : ComponentActivity() {

    // Stati UI
    private var tempState by mutableIntStateOf(0)
    private var humState by mutableIntStateOf(0)
    private var creditState by mutableIntStateOf(0)
    private var machineState by mutableIntStateOf(0)
    private var connectionStatus by mutableStateOf("Disconnesso")
    private var isScanning by mutableStateOf(false)
    private var selectedProduct by mutableStateOf("ACQUA")

    private var bluetoothGatt: BluetoothGatt? = null
    private val mainHandler = Handler(Looper.getMainLooper())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme(colorScheme = darkColorScheme()) {
                // UI FIX: statusBarsPadding e navigationBarsPadding evitano che la grafica tocchi i bordi
                Surface(
                    modifier = Modifier
                        .fillMaxSize()
                        .statusBarsPadding()
                        .navigationBarsPadding(),
                    color = Color(0xFF121212)
                ) {
                    VendingDashboard()
                }
            }
        }
    }

    @SuppressLint("MissingPermission")
    override fun onDestroy() {
        super.onDestroy()
        bluetoothGatt?.let { try { it.disconnect(); it.close() } catch (e: Exception) {} }
        bluetoothGatt = null
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
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            // HEADER
            Text("VENDING MONITOR", fontSize = 22.sp, fontWeight = FontWeight.Black, color = Color(0xFFBB86FC), letterSpacing = 2.sp)
            Spacer(modifier = Modifier.height(16.dp))
            StatusIndicator()
            Spacer(modifier = Modifier.height(24.dp))

            // --- ZONA ACQUISTO (CLIENTE) ---
            Text("SELEZIONA PRODOTTO", fontSize = 12.sp, color = Color.Gray, fontWeight = FontWeight.Bold, modifier = Modifier.align(Alignment.Start))
            Spacer(modifier = Modifier.height(8.dp))

            Card(
                colors = CardDefaults.cardColors(containerColor = Color(0xFF1E1E1E)),
                elevation = CardDefaults.cardElevation(4.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                Column(modifier = Modifier.padding(16.dp), horizontalAlignment = Alignment.CenterHorizontally) {

                    // RIGA 1: ACQUA & SNACK
                    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                        ProductButton("ACQUA", "1.00 €", Color(0xFF00E5FF), selectedProduct == "ACQUA") {
                            selectedProduct = "ACQUA"; writeCommand(1)
                        }
                        ProductButton("SNACK", "2.00 €", Color(0xFFFF4081), selectedProduct == "SNACK") {
                            selectedProduct = "SNACK"; writeCommand(2)
                        }
                    }
                    Spacer(modifier = Modifier.height(12.dp))

                    // RIGA 2: CAFFÈ & THE (NUOVI)
                    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                        ProductButton("CAFFÈ", "1.00 €", Color(0xFFFFEB3B), selectedProduct == "CAFFE") {
                            selectedProduct = "CAFFE"; writeCommand(3)
                        }
                        ProductButton("THE", "2.00 €", Color(0xFF69F0AE), selectedProduct == "THE") {
                            selectedProduct = "THE"; writeCommand(4)
                        }
                    }

                    Spacer(modifier = Modifier.height(20.dp))

                    // CREDITO
                    Text("CREDITO INSERITO", fontSize = 12.sp, color = Color.Gray)
                    Text("$creditState,00 €", fontSize = 48.sp, fontWeight = FontWeight.Bold, color = Color.White)

                    // PULSANTE ANNULLA
                    Spacer(modifier = Modifier.height(10.dp))
                    Button(
                        onClick = { writeCommand(9) },
                        colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFCF6679).copy(alpha = 0.2f)),
                        border = BorderStroke(1.dp, Color(0xFFCF6679)),
                        shape = RoundedCornerShape(50),
                        modifier = Modifier.height(40.dp)
                    ) {
                        Text("ANNULLA / RESTO", color = Color(0xFFCF6679), fontWeight = FontWeight.Bold, fontSize = 12.sp)
                    }
                }
            }

            Spacer(modifier = Modifier.height(20.dp))

            // --- ZONA MANUTENZIONE ---
            Text("DIAGNOSTICA", fontSize = 12.sp, color = Color.Gray, fontWeight = FontWeight.Bold, modifier = Modifier.align(Alignment.Start))
            Spacer(modifier = Modifier.height(8.dp))

            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                SensorMiniCard("TEMP", "$tempState°C", Color(0xFFFF9800), Modifier.weight(1f))
                SensorMiniCard("UMIDITÀ", "$humState%", Color(0xFF4FC3F7), Modifier.weight(1f))
            }

            Spacer(modifier = Modifier.height(10.dp))
            MachineStateBar()
            Spacer(modifier = Modifier.weight(1f))
            ActionButton(permissionsLauncher)
        }
    }

    // --- COMPONENTI UI ---
    @Composable
    fun ProductButton(name: String, price: String, color: Color, isSelected: Boolean, onClick: () -> Unit) {
        val bg = if(isSelected) color.copy(alpha = 0.2f) else Color.Transparent
        val border = if(isSelected) BorderStroke(2.dp, color) else BorderStroke(1.dp, Color.Gray)
        Card(
            modifier = Modifier.size(width = 140.dp, height = 100.dp).clickable { onClick() },
            colors = CardDefaults.cardColors(containerColor = bg),
            border = border
        ) {
            Column(modifier = Modifier.fillMaxSize(), verticalArrangement = Arrangement.Center, horizontalAlignment = Alignment.CenterHorizontally) {
                Text(name, fontWeight = FontWeight.Bold, fontSize = 18.sp, color = if(isSelected) color else Color.White)
                Text(price, fontSize = 14.sp, color = Color.Gray)
            }
        }
    }

    @Composable
    fun SensorMiniCard(title: String, value: String, color: Color, modifier: Modifier) {
        Card(modifier = modifier, colors = CardDefaults.cardColors(containerColor = Color(0xFF2C2C2C))) {
            Row(modifier = Modifier.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
                Box(modifier = Modifier.size(8.dp).background(color, CircleShape))
                Spacer(modifier = Modifier.width(10.dp))
                Column {
                    Text(title, fontSize = 10.sp, color = Color.Gray)
                    Text(value, fontSize = 18.sp, fontWeight = FontWeight.Bold, color = Color.White)
                }
            }
        }
    }

    @Composable
    fun MachineStateBar() {
        val (testo, colore) = when(machineState) {
            0 -> "SISTEMA PRONTO" to Color(0xFF81C784)
            1 -> "IN ATTESA..." to Color(0xFF64B5F6)
            2 -> "EROGAZIONE..." to Color(0xFFFFD54F)
            3 -> "RESTO / RIMBORSO" to Color(0xFFBA68C8)
            4 -> "ERRORE TEMP" to Color(0xFFE57373)
            else -> "--" to Color.Gray
        }
        Card(colors = CardDefaults.cardColors(containerColor = colore.copy(alpha=0.2f)), modifier = Modifier.fillMaxWidth()) {
            Row(modifier = Modifier.padding(12.dp), verticalAlignment = Alignment.CenterVertically) {
                Text("STATO:", fontSize = 12.sp, color = Color.White, fontWeight = FontWeight.Bold)
                Spacer(modifier = Modifier.width(8.dp))
                Text(testo, fontSize = 14.sp, color = colore, fontWeight = FontWeight.Bold)
            }
        }
    }

    @Composable
    fun StatusIndicator() {
        val colorStato = when {
            connectionStatus == "Connesso" -> Color(0xFF00E676)
            isScanning -> Color(0xFFFFEB3B)
            else -> Color(0xFFCF6679)
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(modifier = Modifier.size(10.dp).background(colorStato, CircleShape))
            Spacer(modifier = Modifier.width(8.dp))
            Text(connectionStatus, color = Color.Gray, fontSize = 12.sp)
        }
    }

    @Composable
    fun ActionButton(launcher: androidx.activity.result.ActivityResultLauncher<Array<String>>) {
        val buttonColor = if(connectionStatus == "Connesso") Color(0xFFCF6679) else Color(0xFFBB86FC)
        val text = if(connectionStatus == "Connesso") "DISCONNETTI" else "CONNETTI DISPOSITIVO"
        Button(
            onClick = {
                if(connectionStatus == "Connesso") disconnectDevice()
                else if(!isScanning) checkPermissionsAndScan(launcher)
                else stopScan()
            },
            colors = ButtonDefaults.buttonColors(containerColor = buttonColor),
            modifier = Modifier.fillMaxWidth().height(50.dp),
            shape = RoundedCornerShape(8.dp)
        ) { Text(text, color = Color.Black, fontWeight = FontWeight.Bold) }
    }

    // --- LOGICA BLUETOOTH (DEPRECATIONS FIXED) ---

    @SuppressLint("MissingPermission")
    private fun writeCommand(cmd: Int) {
        if (bluetoothGatt == null) return
        val service = bluetoothGatt?.getService(SERVICE_UUID) ?: return
        val charCmd = service.getCharacteristic(CMD_CHAR_UUID) ?: return
        val payload = byteArrayOf(cmd.toByte())

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            bluetoothGatt?.writeCharacteristic(charCmd, payload, BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE)
        } else {
            @Suppress("DEPRECATION")
            charCmd.value = payload
            @Suppress("DEPRECATION")
            charCmd.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
            @Suppress("DEPRECATION")
            bluetoothGatt?.writeCharacteristic(charCmd)
        }
    }

    @SuppressLint("MissingPermission")
    private fun disconnectDevice() {
        bluetoothGatt?.let { it.disconnect(); it.close() }
        bluetoothGatt = null
        connectionStatus = "Disconnesso"
        tempState=0; humState=0; creditState=0; machineState=0
    }

    @SuppressLint("MissingPermission")
    private fun stopScan() {
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothManager.adapter.bluetoothLeScanner?.stopScan(object : ScanCallback(){})
        isScanning = false
        connectionStatus = "Disconnesso"
    }

    private fun checkPermissionsAndScan(launcher: androidx.activity.result.ActivityResultLauncher<Array<String>>) {
        val hasScan = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED else true
        val hasConnect = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED else true
        val hasLoc = ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
        if (hasScan && hasConnect && hasLoc) startScan()
        else {
            val perms = mutableListOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) { perms.add(Manifest.permission.BLUETOOTH_SCAN); perms.add(Manifest.permission.BLUETOOTH_CONNECT) }
            launcher.launch(perms.toTypedArray())
        }
    }

    @SuppressLint("MissingPermission")
    private fun startScan() {
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val scanner = bluetoothManager.adapter.bluetoothLeScanner
        if (scanner == null) { connectionStatus = "Bluetooth Spento!"; return }
        connectionStatus = "Scansione..."
        isScanning = true
        scanner.startScan(object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult?) {
                val device = result?.device ?: return
                val nome = device.name ?: result.scanRecord?.deviceName
                if (nome != null && nome.contains("Vending", ignoreCase = true)) {
                    if (isScanning) { scanner.stopScan(this); isScanning = false; connectToDevice(device) }
                }
            }
            override fun onScanFailed(errorCode: Int) { isScanning = false; connectionStatus = "Errore Scan: $errorCode" }
        })
        mainHandler.postDelayed({ if (isScanning) { scanner.stopScan(object : ScanCallback(){}); isScanning = false; if(connectionStatus == "Scansione...") connectionStatus = "Nessun Disp. Trovato" } }, 10000)
    }

    @SuppressLint("MissingPermission")
    private fun connectToDevice(device: BluetoothDevice) {
        runOnUiThread { connectionStatus = "Connessione..." }
        bluetoothGatt = device.connectGatt(this, false, object : BluetoothGattCallback() {

            override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    runOnUiThread { connectionStatus = "Connesso" }
                    gatt?.discoverServices()
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    runOnUiThread { connectionStatus = "Disconnesso" }; gatt?.close()
                }
            }
            override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
                if (status == BluetoothGatt.GATT_SUCCESS && gatt != null) {
                    val service = gatt.getService(SERVICE_UUID)
                    if (service != null) {
                        val charTemp = service.getCharacteristic(CHAR_TEMP_UUID)
                        if (charTemp != null) enableNotification(gatt, charTemp)
                    }
                }
            }
            override fun onDescriptorWrite(gatt: BluetoothGatt?, descriptor: BluetoothGattDescriptor?, status: Int) {
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    val uuid = descriptor?.characteristic?.uuid
                    val service = gatt?.getService(SERVICE_UUID)
                    if (uuid == CHAR_TEMP_UUID) {
                        val charHum = service?.getCharacteristic(CHAR_HUM_UUID)
                        if (charHum != null) { Thread.sleep(100); enableNotification(gatt!!, charHum) }
                    }
                    else if (uuid == CHAR_HUM_UUID) {
                        val charStatus = service?.getCharacteristic(CHAR_STATUS_UUID)
                        if (charStatus != null) { Thread.sleep(100); enableNotification(gatt!!, charStatus) }
                    }
                    else if (uuid == CHAR_STATUS_UUID) { writeCommand(1) }
                }
            }

            // GESTIONE NUOVO CALLBACK (Android 13+) vs VECCHIO
            override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
                // Questo metodo viene chiamato su Android 13+ (API 33)
                processData(characteristic.uuid, value)
            }

            @Deprecated("Deprecated in Java")
            override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
                // Questo metodo viene chiamato su Android < 13
                @Suppress("DEPRECATION")
                processData(characteristic.uuid, characteristic.value)
            }
        })
    }

    private fun processData(uuid: UUID, data: ByteArray) {
        if (uuid == CHAR_TEMP_UUID) {
            val temp = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN).int
            runOnUiThread { tempState = temp }
        }
        else if (uuid == CHAR_HUM_UUID) {
            val hum = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN).int
            runOnUiThread { humState = hum }
        }
        else if (uuid == CHAR_STATUS_UUID && data.size >= 2) {
            runOnUiThread { creditState = data[0].toInt(); machineState = data[1].toInt() }
        }
    }

    @SuppressLint("MissingPermission")
    private fun enableNotification(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
        gatt.setCharacteristicNotification(characteristic, true)
        val descriptor = characteristic.getDescriptor(CCCD_UUID)
        if (descriptor != null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                gatt.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
            } else {
                @Suppress("DEPRECATION")
                descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                @Suppress("DEPRECATION")
                gatt.writeDescriptor(descriptor)
            }
        }
    }
}