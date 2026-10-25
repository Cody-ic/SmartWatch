// Standalone Dart: dart run dart_async.dart
import 'dart:async';

Future<int> readLater() async {
  await Future<void>.delayed(const Duration(milliseconds: 20));
  return 4000;
}

Future<void> main() async {
  print('1: request');
  final result = readLater();
  print('2: keep handling other events');
  print('3: voltage = ${await result}');

  final events = StreamController<int>();
  final received = <int>[];
  final subscription = events.stream.listen(received.add);
  events.add(3900);
  events.add(4000);
  await events.close();
  await subscription.cancel();
  print(received);
}
