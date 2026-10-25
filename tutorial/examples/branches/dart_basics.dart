// Standalone Dart: dart run dart_basics.dart
class Measurement {
  final int millivolts;
  Measurement(this.millivolts);
  String describe() => '$millivolts mV';
}

class BatteryMeasurement extends Measurement {
  BatteryMeasurement(super.millivolts);
  @override
  String describe() => 'Battery: ${super.describe()}';
}

int add(int a, int b, {int offset = 0}) => a + b + offset;

void main() {
  var count = 2;
  count++;
  final readings = <int>[3900, 3950];
  readings.add(4000); // The variable is final; this list remains mutable.
  const channels = <String>['red', 'infrared'];
  final names = <int, String>{0: 'Lying', 3: 'Walking'};
  String? deviceName;
  print(deviceName?.length ?? 0);
  deviceName = 'SmartWatch';
  print('$deviceName: ${deviceName.length}');
  print(add(count, readings.length, offset: 1));
  print('${channels[0]} ${names[3]}');
  final measurement = BatteryMeasurement(4000);
  void Function() onPressed = () => print(measurement.describe());
  onPressed();
}
