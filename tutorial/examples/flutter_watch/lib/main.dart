import 'dart:async';
import 'package:flutter/material.dart';

void main() => runApp(const MaterialApp(home: BatteryPage()));

class BatteryPage extends StatefulWidget {
  const BatteryPage({super.key});
  @override
  State<BatteryPage> createState() => _BatteryPageState();
}

class _BatteryPageState extends State<BatteryPage> {
  int? millivolts;
  bool running = false;
  StreamSubscription<int>? subscription;

  void start() {
    if (running) return;
    setState(() => running = true);
    subscription = Stream<int>.periodic(
      const Duration(seconds: 1), (i) => 3900 + i % 100,
    ).listen((value) {
      if (!mounted) return;
      setState(() => millivolts = value);
    });
  }

  Future<void> stop() async {
    final previous = subscription;
    subscription = null;
    if (!mounted) return;
    setState(() { running = false; millivolts = null; });
    await previous?.cancel();
  }

  @override
  void dispose() {
    subscription?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => Scaffold(
    appBar: AppBar(title: const Text('Watch battery exercise')),
    body: Padding(
      padding: const EdgeInsets.all(24),
      child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Text(millivolts == null ? 'No current measurement' : '$millivolts mV'),
        const SizedBox(height: 16),
        ElevatedButton(onPressed: running ? null : start,
          child: const Text('Start simulated data')),
        ElevatedButton(onPressed: running ? stop : null,
          child: const Text('Stop and clear')),
      ]),
    ),
  );
}
