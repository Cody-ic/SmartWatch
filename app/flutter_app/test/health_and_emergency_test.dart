import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:smartwatch_app/ble_protocol.dart';
import 'package:smartwatch_app/ble_service.dart';
import 'package:smartwatch_app/dashboard_page.dart';
import 'package:smartwatch_app/emergency_contacts.dart';
import 'package:smartwatch_app/emergency_alert.dart';

class FakeWatch extends SmartWatchBLE {
  final oxygen = StreamController<Spo2Data>.broadcast();
  final connection = StreamController<bool>.broadcast();
  final sleep = StreamController<SleepApnea>.broadcast();
  @override
  Stream<Spo2Data> get onSpo2 => oxygen.stream;
  @override
  Stream<bool> get onConnection => connection.stream;
  @override
  Stream<SleepApnea> get onSleepApnea => sleep.stream;
  @override
  void dispose() {
    oxygen.close();
    connection.close();
    sleep.close();
    super.dispose();
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();
  const channel = MethodChannel('com.example.smartwatch_app/emergency');
  final sent = <MethodCall>[];
  bool permission = true;
  final userContact = {
    'name': 'Recipient',
    'phone': '0000000000',
    'relation': 'Test'
  };
  final demo = {
    'name': 'Son Zhao Ming',
    'phone': '13800138001',
    'relation': 'Son'
  };

  setUp(() {
    SharedPreferences.setMockInitialValues({});
    sent.clear();
    permission = true;
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (call) async {
      if (call.method == 'checkPermission') return permission;
      if (call.method == 'sendSms') {
        sent.add(call);
        return true;
      }
      return null;
    });
  });
  tearDown(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, null);
  });
  test('fresh install never creates SMS recipients', () async {
    await EmergencyContacts.load();
    expect(EmergencyContacts.all, isEmpty);
  });
  test('migration removes only exact demo entries and preserves original data',
      () async {
    final original = jsonEncode([demo, userContact]);
    SharedPreferences.setMockInitialValues({'emergency_contacts': original});
    await EmergencyContacts.load();
    expect(EmergencyContacts.all.single.name, 'Recipient');
    final prefs = await SharedPreferences.getInstance();
    expect(
        prefs.getString('emergency_contacts_before_demo_migration'), original);
    await EmergencyContacts.add(EmergencyContact.fromJson(demo));
    await EmergencyContacts.load();
    expect(EmergencyContacts.all.length,
        2); // Explicit user addition after migration survives.
  });
  test('alert subject is the wearer, independent of recipient names', () {
    expect(emergencyMessage(AlertType.fall), contains('watch wearer'));
    expect(emergencyMessage(AlertType.bullying), isNot(contains('Recipient')));
  });

  Future<void> showAlert(WidgetTester tester) async {
    tester.view.physicalSize = const Size(1000, 1800);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    await tester.pumpWidget(MaterialApp(
        home: EmergencyAlertOverlay(type: AlertType.fall, onDismiss: () {})));
    await tester.pump(const Duration(seconds: 15));
    await tester.pumpAndSettle();
  }

  testWidgets('empty or migrated demo contacts do not call SMS channel',
      (tester) async {
    SharedPreferences.setMockInitialValues({
      'emergency_contacts': jsonEncode([demo])
    });
    await showAlert(tester);
    expect(sent, isEmpty);
    expect(find.textContaining('No emergency contacts configured'),
        findsOneWidget);
  });
  testWidgets('missing SMS permission produces an explicit failure',
      (tester) async {
    SharedPreferences.setMockInitialValues({
      'emergency_contacts': jsonEncode([userContact])
    });
    permission = false;
    await showAlert(tester);
    expect(sent, isEmpty);
    expect(find.textContaining('No alerts were sent'), findsOneWidget);
  });
  testWidgets(
      'configured contact receives wearer message through mocked SMS only',
      (tester) async {
    SharedPreferences.setMockInitialValues({
      'emergency_contacts': jsonEncode([userContact])
    });
    await showAlert(tester);
    expect(sent, hasLength(1));
    expect(sent.single.arguments['phone'], '0000000000');
    expect(sent.single.arguments['message'], contains('watch wearer'));
    expect(sent.single.arguments['message'], isNot(contains('Recipient')));
  });

  Future<FakeWatch> showDashboard(WidgetTester tester) async {
    tester.view.physicalSize = const Size(1000, 1800);
    tester.view.devicePixelRatio = 1;
    addTearDown(tester.view.resetPhysicalSize);
    addTearDown(tester.view.resetDevicePixelRatio);
    final watch = FakeWatch();
    await tester.pumpWidget(MaterialApp(home: DashboardPage(ble: watch)));
    await tester.pumpAndSettle();
    return watch;
  }

  testWidgets('SpO2 clears on invalid data, disconnect and silence',
      (tester) async {
    final watch = await showDashboard(tester);
    watch.oxygen.add(Spo2Data(97, true));
    await tester.pumpAndSettle();
    expect(find.text('97%'), findsOneWidget);
    watch.oxygen.add(Spo2Data(0, false));
    await tester.pumpAndSettle();
    expect(find.text('97%'), findsNothing);
    watch.oxygen.add(Spo2Data(97, true));
    await tester.pumpAndSettle();
    watch.connection.add(false);
    await tester.pumpAndSettle();
    expect(find.text('97%'), findsNothing);
    watch.oxygen.add(Spo2Data(97, true));
    await tester.pumpAndSettle();
    await tester.pump(const Duration(seconds: 6));
    expect(find.text('97%'), findsNothing);
    await tester.pumpWidget(const SizedBox());
  });
  testWidgets(
      'cumulative monitoring reports are not added together or shown as live SpO2',
      (tester) async {
    final watch = await showDashboard(tester);
    for (final minutes in [5, 10, 15]) {
      watch.sleep.add(SleepApnea(0, 0, 0, 97, 0, minutes, true));
      await tester.pumpAndSettle();
    }
    expect(find.text('Sleep monitoring: 0h 15m'), findsOneWidget);
    expect(find.text('97%'), findsNothing);
    watch.sleep.add(SleepApnea(0, 0, 0, 97, 0, 17, false));
    await tester.pumpAndSettle();
    expect(find.text('Last monitoring: 0h 17m'), findsOneWidget);
    await tester.pumpWidget(const SizedBox());
  });
}
