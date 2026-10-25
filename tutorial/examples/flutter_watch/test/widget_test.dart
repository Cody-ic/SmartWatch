import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:tutorial_watch/main.dart';

void main() {
  testWidgets('data stops and old reading is cleared', (tester) async {
    await tester.pumpWidget(const MaterialApp(home: BatteryPage()));
    expect(find.text('No current measurement'), findsOneWidget);
    await tester.tap(find.text('Start simulated data'));
    await tester.pump();
    await tester.pump(const Duration(seconds: 1));
    expect(find.text('3900 mV'), findsOneWidget);
    await tester.tap(find.text('Stop and clear'));
    await tester.pump();
    await tester.pump(const Duration(seconds: 2));
    expect(find.text('No current measurement'), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });
}
