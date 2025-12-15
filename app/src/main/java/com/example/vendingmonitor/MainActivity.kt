package com.example.vendingmonitor // Assicurati che il package sia il tuo!

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
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
// Devono corrispondere a quelli Mbed + Base UUID standard
val SERVICE_UUID: UUID = UUID.fromString("0000A000-0000-1000-8000-00805f9b34fb")
val CHAR_TEMP_UUID: UUID = UUID.fromString("0000A001-0000-1000-8000-00805f9b34fb")
val CHAR_STATUS_UUID: UUID = UUID.fromString("0000A002-0000-1000-8000-00805f9b34fb")

class MainActivity : ComponentActivity() {

    // Variabili di stato per la UI
    private var tempState by mutableIntStateOf(0)
    private var creditState by mutableIntStateOf(0)
    private var machineState by mutableIntStateOf(0) // 0=Riposo, 1=Attesa...
    private var connectionStatus by mutableStateOf("Disconnesso")
    private var isScanning by mutableStateOf(false)

    private var bluetoothGatt: BluetoothGatt? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            VendingDashboard()
        }
    }

    @Composable
    fun VendingDashboard() {
        // Gestione permessi al volo
        val permissionsLauncher = rememberLauncherForActivityResult(
            ActivityResultContracts.RequestMultiplePermissions()
        ) { perms ->
            if (perms.values.all { it }) startScan()
            else Toast.makeText(this, "Permessi necessari!", Toast.LENGTH_SHORT).show()
        }

        MaterialTheme {
            Column(
                modifier = Modifier.fillMaxSize().padding(16.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(20.dp)
            ) {
                Text("Vending Machine IoT", fontSize = 28.sp, fontWeight = FontWeight.Bold, color = Color(0xFF6200EE))

                // Card Stato Connessione
                StatusCard("Stato Bluetooth", connectionStatus, if(connectionStatus=="Connesso") Color.Green else Color.Red)

                Button(
                    onClick = {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                            permissionsLauncher.launch(arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT))
                        } else {
                            permissionsLauncher.launch(arrayOf(Manifest.permission.ACCESS_FINE_LOCATION))
                        }
                    },
                    enabled = !isScanning && connectionStatus != "Connesso"
                ) {
                    Text(if (isScanning) "Scansione in corso..." else "Connetti a VendingM")
                }

                Spacer(modifier = Modifier.height(20.dp))

                // Card Dati Sensori
                Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                    DataBox("Temperatura", "$tempState°C", Color(0xFFFF9800))
                    DataBox("Credito", "$creditState €", Color(0xFF03DAC5))
                }

                // Card Stato Macchina (Decodifica enum)
                val statoTesto = when(machineState) {
                    0 -> "RIPOSO (Eco Mode)"
                    1 -> "ATTESA MONETA"
                    2 -> "EROGAZIONE..."
                    3 -> "RESTO"
                    4 -> "ERRORE !!"
                    else -> "Sconosciuto"
                }
                DataBox("Stato Macchina", statoTesto, Color(0xFF2196F3), Modifier.fillMaxWidth())
            }
        }
    }

    @Composable
    fun StatusCard(title: String, value: String, color: Color) {
        Card(modifier = Modifier.fillMaxWidth(), colors = CardDefaults.cardColors(containerColor = Color.White), elevation = CardDefaults.cardElevation(4.dp)) {
            Column(modifier = Modifier.padding(16.dp)) {
                Text(title, fontSize = 14.sp, color = Color.Gray)
                Text(value, fontSize = 20.sp, fontWeight = FontWeight.Bold, color = color)
            }
        }
    }

    @Composable
    fun DataBox(title: String, value: String, bgColor: Color, modifier: Modifier = Modifier) {
        Card(modifier = modifier, colors = CardDefaults.cardColors(containerColor = bgColor)) {
            Column(modifier = Modifier.padding(24.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                Text(title, color = Color.Black, fontWeight = FontWeight.Bold)
                Text(value, fontSize = 24.sp, color = Color.Black, fontWeight = FontWeight.ExtraBold)
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun startScan() {
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val scanner = bluetoothManager.adapter.bluetoothLeScanner
        if (scanner == null) {
            connectionStatus = "BT spento!"
            return
        }

        connectionStatus = "Scansione..."
        isScanning = true

        scanner.startScan(object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult?) {
                val device = result?.device ?: return

                // --- MODIFICA FONDAMENTALE ---
                // Cerchiamo il nome sia nell'oggetto Device che nel Record di Scansione (più affidabile)
                val nomeDispositivo = device.name ?: result.scanRecord?.deviceName

                // Log per debug (lo vedi nel Logcat di Android Studio)
                if (nomeDispositivo != null) {
                    println("Trovato dispositivo: $nomeDispositivo [${device.address}]")
                }

                // Controllo flessibile (ignora maiuscole/minuscole e spazi extra)
                if (nomeDispositivo != null && nomeDispositivo.trim().equals("VendingM", ignoreCase = true)) {
                    connectionStatus = "Trovato VendingM! Connessione..."
                    scanner.stopScan(this)
                    isScanning = false
                    connectToDevice(device)
                }
            }

            override fun onScanFailed(errorCode: Int) {
                connectionStatus = "Errore Scansione: $errorCode"
                isScanning = false
            }
        })
    }

    @SuppressLint("MissingPermission")
    private fun connectToDevice(device: BluetoothDevice) {
        bluetoothGatt = device.connectGatt(this, false, object : BluetoothGattCallback() {
            override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    runOnUiThread { connectionStatus = "Connesso" }
                    gatt?.discoverServices()
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    runOnUiThread { connectionStatus = "Disconnesso" }
                }
            }

            override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
                val service = gatt?.getService(SERVICE_UUID)
                if (service != null) {
                    enableNotification(gatt, service.getCharacteristic(CHAR_TEMP_UUID))
                    enableNotification(gatt, service.getCharacteristic(CHAR_STATUS_UUID))
                }
            }

            override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
                val data = characteristic.value

                if (characteristic.uuid == CHAR_TEMP_UUID) {
                    // Mbed invia int (4 byte) Little Endian
                    val temp = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN).int
                    runOnUiThread { tempState = temp }
                }
                else if (characteristic.uuid == CHAR_STATUS_UUID) {
                    // Byte 0 = Credito, Byte 1 = Stato
                    if (data.size >= 2) {
                        val cred = data[0].toInt()
                        val state = data[1].toInt()
                        runOnUiThread {
                            creditState = cred
                            machineState = state
                        }
                    }
                }
            }
        })
    }

    @SuppressLint("MissingPermission")
    private fun enableNotification(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic?) {
        if (characteristic == null) return
        gatt.setCharacteristicNotification(characteristic, true)

        // Trick necessario per attivare il descriptor CCCD (Client Characteristic Configuration Descriptor)
        val CONFIG_DESCRIPTOR = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        val descriptor = characteristic.getDescriptor(CONFIG_DESCRIPTOR)
        if (descriptor != null) {
            descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
            gatt.writeDescriptor(descriptor)
        }
    }
}