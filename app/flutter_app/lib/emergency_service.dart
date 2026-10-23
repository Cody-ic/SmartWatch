/// Emergency SMS service — sends alerts via Android SmsManager
library;

import 'package:flutter/services.dart';

class EmergencyService {
  static const _channel = MethodChannel('com.example.smartwatch_app/emergency');

  /// Check if SEND_SMS permission is granted
  static Future<bool> hasPermission() async {
    try {
      final result = await _channel.invokeMethod('checkPermission');
      return result == true;
    } catch (_) {
      return false;
    }
  }

  /// Request SEND_SMS permission (pops system dialog)
  static Future<void> requestPermission() async {
    try {
      await _channel.invokeMethod('requestPermission');
    } catch (_) {}
  }

  /// Send SMS to a phone number. Returns true if sent successfully.
  static Future<bool> sendSms(String phone, String message) async {
    try {
      await _channel.invokeMethod('sendSms', {'phone': phone, 'message': message});
      return true;
    } catch (e) {
      return false;
    }
  }
}
