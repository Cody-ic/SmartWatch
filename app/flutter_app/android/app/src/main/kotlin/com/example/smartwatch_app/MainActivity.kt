package com.example.smartwatch_app

import android.Manifest
import android.content.pm.PackageManager
import android.telephony.SmsManager
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    private val CHANNEL = "com.example.smartwatch_app/emergency"
    private val REQUEST_SMS = 1001

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, CHANNEL).setMethodCallHandler { call, result ->
            when (call.method) {
                "sendSms" -> {
                    val phone = call.argument<String>("phone") ?: ""
                    val message = call.argument<String>("message") ?: ""
                    if (phone.isEmpty()) {
                        result.error("NO_PHONE", "Phone number is empty", null)
                        return@setMethodCallHandler
                    }
                    try {
                        val sms = SmsManager.getDefault()
                        sms.sendTextMessage(phone, null, message, null, null)
                        result.success(true)
                    } catch (e: Exception) {
                        result.error("SMS_FAILED", e.message, null)
                    }
                }
                "checkPermission" -> {
                    val granted = ContextCompat.checkSelfPermission(
                        this, Manifest.permission.SEND_SMS
                    ) == PackageManager.PERMISSION_GRANTED
                    result.success(granted)
                }
                "requestPermission" -> {
                    ActivityCompat.requestPermissions(
                        this,
                        arrayOf(Manifest.permission.SEND_SMS),
                        REQUEST_SMS
                    )
                    result.success(null)
                }
                else -> result.notImplemented()
            }
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
    }
}
