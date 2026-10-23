/// Full-screen emergency alert overlay with countdown and SMS notification
library;

import 'dart:async';
import 'package:flutter/material.dart';
import 'emergency_contacts.dart';
import 'emergency_service.dart';

enum AlertType { fall, bullying }

String emergencyMessage(AlertType type) => type == AlertType.fall
    ? 'EMERGENCY: The watch wearer may have fallen. Please check immediately.'
    : 'EMERGENCY: The watch wearer may be in danger. Please check immediately.';

class EmergencyAlertOverlay extends StatefulWidget {
  final AlertType type;
  final VoidCallback onDismiss;

  const EmergencyAlertOverlay(
      {super.key, required this.type, required this.onDismiss});

  @override
  State<EmergencyAlertOverlay> createState() => _EmergencyAlertOverlayState();
}

class _EmergencyAlertOverlayState extends State<EmergencyAlertOverlay> {
  int _countdown = 15;
  Timer? _timer;
  bool _sending = false;
  bool _finished = false;
  String? _statusMessage;
  final List<({EmergencyContact contact, bool sent})> _status = [];

  String get _title =>
      widget.type == AlertType.fall ? 'FALL DETECTED' : 'BULLYING DETECTED';

  @override
  void initState() {
    super.initState();
    _timer = Timer.periodic(const Duration(seconds: 1), (_) {
      if (!mounted) return;
      setState(() {
        if (_countdown > 0) {
          _countdown--;
        }
        if (_countdown == 0 && !_sending) {
          _sendAlerts();
        }
      });
    });
  }

  Future<void> _sendAlerts() async {
    setState(() => _sending = true);
    _timer?.cancel();
    await EmergencyContacts.load();
    if (!mounted) return;
    if (EmergencyContacts.all.isEmpty) {
      setState(() {
        _finished = true;
        _statusMessage =
            'No emergency contacts configured. Add a contact in Profile.';
      });
      return;
    }
    final permission = await EmergencyService.hasPermission();
    if (!mounted) return;
    if (!permission) {
      setState(() {
        _finished = true;
        _statusMessage = 'SMS permission is required. No alerts were sent.';
      });
      return;
    }
    _status.clear();
    for (final c in EmergencyContacts.all) {
      _status.add((contact: c, sent: false));
    }
    setState(() {});

    final msg = emergencyMessage(widget.type);

    for (int i = 0; i < _status.length; i++) {
      final ok = await EmergencyService.sendSms(_status[i].contact.phone, msg);
      if (!mounted) return;
      setState(() => _status[i] = (contact: _status[i].contact, sent: ok));
      // Small delay between sends to avoid carrier throttling
      if (i < _status.length - 1) {
        await Future.delayed(const Duration(milliseconds: 500));
      }
      if (!mounted) return;
    }
    if (mounted) setState(() => _finished = true);
  }

  void _cancel() {
    _timer?.cancel();
    widget.onDismiss();
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Material(
      color: const Color(0xE6B71C1C), // deep red, slightly transparent
      child: SafeArea(
        child: Center(
          child: Padding(
            padding: const EdgeInsets.all(24),
            child: Column(mainAxisSize: MainAxisSize.min, children: [
              const Icon(Icons.warning_amber_rounded,
                  size: 56, color: Colors.white),
              const SizedBox(height: 16),
              Text(_title,
                  style: const TextStyle(
                      fontSize: 28,
                      fontWeight: FontWeight.bold,
                      color: Colors.white)),
              const SizedBox(height: 24),
              if (!_sending && _countdown > 0) ...[
                // Countdown phase
                Text('Alerting in',
                    style: TextStyle(
                        fontSize: 14,
                        color: Colors.white.withValues(alpha: 0.7))),
                const SizedBox(height: 4),
                Text('$_countdown',
                    style: const TextStyle(
                        fontSize: 72,
                        fontWeight: FontWeight.bold,
                        color: Colors.white)),
                const SizedBox(height: 24),
                SizedBox(
                  width: 180,
                  height: 48,
                  child: ElevatedButton(
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.white,
                      foregroundColor: const Color(0xFFB71C1C),
                      shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(24)),
                    ),
                    onPressed: _cancel,
                    child: const Text("I'M OK",
                        style: TextStyle(
                            fontSize: 18, fontWeight: FontWeight.bold)),
                  ),
                ),
              ] else if (_sending) ...[
                // Sending phase
                if (!_finished)
                  const SizedBox(
                      width: 28,
                      height: 28,
                      child: CircularProgressIndicator(
                          strokeWidth: 3, color: Colors.white)),
                const SizedBox(height: 16),
                Text(
                    _statusMessage ??
                        (_finished
                            ? 'SMS requests submitted; check your phone for delivery status.'
                            : 'Notifying contacts...'),
                    textAlign: TextAlign.center,
                    style:
                        const TextStyle(fontSize: 14, color: Colors.white70)),
                const SizedBox(height: 16),
                ...List.generate(
                    _status.length,
                    (i) => Padding(
                          padding: const EdgeInsets.symmetric(vertical: 4),
                          child: Row(mainAxisSize: MainAxisSize.min, children: [
                            Text(
                                '${_status[i].contact.relation}: ${_status[i].contact.name}  ${_status[i].contact.phone}',
                                style: const TextStyle(
                                    fontSize: 13, color: Colors.white70)),
                            const SizedBox(width: 8),
                            Icon(
                                _status[i].sent
                                    ? Icons.check_circle
                                    : Icons.error,
                                size: 18,
                                color: _status[i].sent
                                    ? Colors.greenAccent
                                    : Colors.redAccent),
                          ]),
                        )),
                const SizedBox(height: 16),
                SizedBox(
                  width: 120,
                  height: 40,
                  child: ElevatedButton(
                    style: ElevatedButton.styleFrom(
                      backgroundColor: Colors.white,
                      foregroundColor: const Color(0xFFB71C1C),
                      shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(20)),
                    ),
                    onPressed: _cancel,
                    child: const Text('Done',
                        style: TextStyle(
                            fontSize: 16, fontWeight: FontWeight.bold)),
                  ),
                ),
              ],
            ]),
          ),
        ),
      ),
    );
  }
}
