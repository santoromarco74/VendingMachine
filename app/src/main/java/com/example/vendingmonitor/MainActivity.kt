package com.example.vendingmonitor // <--- VERIFICA CHE QUESTO SIA IL TUO PACKAGE REALE!

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
import androidx.compose.foundation.layout.*
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
// Base UUID standard + Parte Mbed
val SERVICE_UUID: UUID = UUID.fromString("0000A000-0000-1000-8000-00805f9b34fb")
val CHAR_TEMP_UUID: UUID = UUID.fromString("0000A001-0000-1000-8000-00805f9b34fb")
val CHAR_STATUS_UUID: UUID = UUID.fromString("0000A002-0000-1000-8000-00805f9b34fb")
val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb") // Descriptor standard

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
            VendingDashboard()
        }
    }

    @Composable
    fun VendingDashboard() {
        // Launcher per richiesta permessi
        val permissionsLauncher = rememberLauncherForActivityResult(
            ActivityResultContracts.RequestMultiplePermissions()
        ) { perms ->
            val allGranted = perms.values.all { it }
            if (allGranted) {
                println("DEBUG: Permessi ottenuti dal popup. Avvio scan...")
                startScan()
            } else {
                Toast.makeText(this, "Permessi negati! Impossibile scansionare.", Toast.LENGTH_LONG).show()
            }
        }

        MaterialTheme {
            Column(
                modifier = Modifier.fillMaxSize().padding(16.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(20.dp)
            ) {
                Text("Vending Monitor", fontSize = 28.sp, fontWeight = FontWeight.Bold, color = Color(0xFF6200EE))

                // Stato Connessione
                StatusCard("Stato Bluetooth", connectionStatus, if(connectionStatus=="Connesso") Color.Green else Color.Red)

                // Pulsante Connetti con logica Permessi Aggressiva
                Button(
                    onClick = {
                        checkPermissionsAndScan(permissionsLauncher)
                    },
                    enabled = !isScanning && connectionStatus != "Connesso",
                    modifier = Modifier.fillMaxWidth().height(50.dp)
                ) {
                    Text(if (isScanning) "Scansione in corso..." else "Connetti a VendingM")
                }

                Spacer(modifier = Modifier.height(10.dp))

                // Dati Sensori
                Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                    DataBox("Temperatura", "$tempState°C", Color(0xFFFF9800), Modifier.weight(1f).padding(4.dp))
                    DataBox("Credito", "$creditState €", Color(0xFF03DAC5), Modifier.weight(1f).padding(4.dp))
                }

                // Decodifica Stato Testuale
                val statoTesto = when(machineState) {
                    0 -> "RIPOSO (Eco Mode)"
                    1 -> "ATTESA MONETA"
                    2 -> "EROGAZIONE..."
                    3 -> "RESTO"
                    4 -> "ERRORE !!"
                    else -> "Sconosciuto ($machineState)"
                }
                DataBox("Stato Macchina", statoTesto, Color(0xFF2196F3), Modifier.fillMaxWidth())
            }
        }
    }

    // --- LOGICA PERMESSI ---
    private fun checkPermissionsAndScan(launcher: androidx.activity.result.ActivityResultLauncher<Array<String>>) {
        val hasScan = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED
        } else true

        val hasConnect = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED
        } else true

        val hasLoc = ActivityCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED

        if (hasScan && hasConnect && hasLoc) {
            println("DEBUG: Permessi già presenti. Avvio Scan immediato.")
            startScan()
        } else {
            println("DEBUG: Mancano permessi. Richiedo...")
            val perms = mutableListOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                perms.add(Manifest.permission.BLUETOOTH_SCAN)
                perms.add(Manifest.permission.BLUETOOTH_CONNECT)
            }
            launcher.launch(perms.toTypedArray())
        }
    }

    // --- LOGICA SCANSIONE ---
    @SuppressLint("MissingPermission")
    private fun startScan() {
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val scanner = bluetoothManager.adapter.bluetoothLeScanner
        if (scanner == null) {
            connectionStatus = "Errore: Bluetooth spento?"
            return
        }

        connectionStatus = "Scansione..."
        isScanning = true

        scanner.startScan(object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult?) {
                val device = result?.device ?: return
                // Cerchiamo il nome OVUNQUE (Device o ScanRecord)
                val nome = device.name ?: result.scanRecord?.deviceName

                if (nome != null && nome.contains("Vending", ignoreCase = true)) {
                    println("DEBUG: TROVATO TARGET: $nome [${device.address}]")
                    if (isScanning) {
                        scanner.stopScan(this)
                        isScanning = false
                        connectToDevice(device)
                    }
                }
            }

            override fun onScanFailed(errorCode: Int) {
                println("DEBUG: Scan fallito con codice $errorCode")
                isScanning = false
                connectionStatus = "Scan Fail: $errorCode"
            }
        })

        // Timeout di sicurezza dopo 10 secondi
        mainHandler.postDelayed({
            if (isScanning) {
                println("DEBUG: Timeout scansione.")
                scanner.stopScan(object : ScanCallback(){})
                isScanning = false
                if(connectionStatus == "Scansione...") connectionStatus = "Non trovato"
            }
        }, 10000)
    }

    // --- LOGICA CONNESSIONE GATT ---
    @SuppressLint("MissingPermission")
    private fun connectToDevice(device: BluetoothDevice) {
        runOnUiThread { connectionStatus = "Connessione in corso..." }
        println("DEBUG: ConnectGatt a ${device.address}")

        bluetoothGatt = device.connectGatt(this, false, object : BluetoothGattCallback() {

            // Variabile temporanea per ricordarci di attivare lo stato dopo
            private var charStatoDaAttivare: BluetoothGattCharacteristic? = null

            override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    println("DEBUG: CONNESSO! Scopro servizi...")
                    runOnUiThread { connectionStatus = "Connesso" }
                    gatt?.discoverServices()
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    runOnUiThread { connectionStatus = "Disconnesso" }
                }
            }

            override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
                if (status == BluetoothGatt.GATT_SUCCESS && gatt != null) {
                    val service = gatt.getService(SERVICE_UUID)
                    if (service != null) {
                        println("DEBUG: Servizio Trovato. Avvio sequenza notifiche...")

                        val charTemp = service.getCharacteristic(CHAR_TEMP_UUID)
                        val charStatus = service.getCharacteristic(CHAR_STATUS_UUID)

                        // Salviamo il secondo per dopo
                        charStatoDaAttivare = charStatus

                        // 1. PASSO UNO: Attiviamo PRIMA la Temperatura
                        if (charTemp != null) {
                            enableNotification(gatt, charTemp, "TEMP")
                        } else {
                            // Se manca la temp, proviamo subito lo stato
                            if (charStatus != null) enableNotification(gatt, charStatus, "STATO")
                        }
                    }
                }
            }

            // --- QUI LA MAGIA: Quando finisce la prima, parte la seconda ---
            override fun onDescriptorWrite(gatt: BluetoothGatt?, descriptor: BluetoothGattDescriptor?, status: Int) {
                if (status == BluetoothGatt.GATT_SUCCESS) {
                    println("DEBUG: Iscrizione completata per ${descriptor?.characteristic?.uuid}")

                    // Se abbiamo appena finito di attivare la TEMP (A001)...
                    if (descriptor?.characteristic?.uuid == CHAR_TEMP_UUID) {
                        // ... e abbiamo uno STATO (A002) in attesa
                        if (charStatoDaAttivare != null) {
                            println("DEBUG: Sequenza: Ora attivo STATO...")
                            // Piccolo respiro per il bus BLE (sicurezza)
                            Thread.sleep(100)
                            enableNotification(gatt!!, charStatoDaAttivare!!, "STATO")
                        }
                    }
                } else {
                    println("DEBUG: Errore scrittura descriptor: $status")
                }
            }

            override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
                val data = characteristic.value

                if (characteristic.uuid == CHAR_TEMP_UUID) {
                    val temp = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN).int
                    runOnUiThread { tempState = temp }
                }
                else if (characteristic.uuid == CHAR_STATUS_UUID) {
                    if (data.size >= 2) {
                        // Byte 0 = Credito, Byte 1 = Stato
                        val cred = data[0].toInt()
                        val state = data[1].toInt()
                        println("DEBUG: Dati Stato -> Credito: $cred, Stato: $state")
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
    private fun enableNotification(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, tag: String) {
        // 1. Abilita Localmente
        gatt.setCharacteristicNotification(characteristic, true)

        // 2. Abilita Remotamente (CCCD Descriptor)
        val descriptor = characteristic.getDescriptor(CCCD_UUID)
        if (descriptor != null) {
            descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
            val success = gatt.writeDescriptor(descriptor)
            println("DEBUG: Scrittura CCCD per $tag -> Successo? $success")
        } else {
            println("DEBUG: ERRORE - Descriptor CCCD mancante per $tag")
        }
    }

    // --- COMPONENTI UI ---
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
            Column(modifier = Modifier.padding(20.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                Text(title, color = Color.Black, fontWeight = FontWeight.Bold, fontSize = 16.sp)
                Text(value, fontSize = 22.sp, color = Color.Black, fontWeight = FontWeight.ExtraBold)
            }
        }
    }
}