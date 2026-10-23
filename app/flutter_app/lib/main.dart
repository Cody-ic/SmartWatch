import 'package:flutter/material.dart';
import 'dashboard_page.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const SmartWatchApp());
}

class SmartWatchApp extends StatelessWidget {
  const SmartWatchApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'SmartWatch',
      debugShowCheckedModeBanner: false,
      theme: ThemeData.dark(useMaterial3: true).copyWith(
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xFFFF1744),
          brightness: Brightness.dark,
        ),
      ),
      home: const DashboardPage(),
    );
  }
}
