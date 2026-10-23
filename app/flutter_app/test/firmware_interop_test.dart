import 'dart:io';
import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:smartwatch_app/ble_protocol.dart';

void main() {
  test('production STM32 backup frames decode to the expected model in Dart',
      () {
    final bytes = File('test/fixtures/firmware_backup.bin').readAsBytesSync();
    final assembler = ProtoBackupAssembler(0x12345678);
    ProtoData? result;
    int frames = 0;
    for (int off = 0; off < bytes.length;) {
      final length = bytes[off + 2] + 4;
      final frame = parseFrame(Uint8List.sublistView(bytes, off, off + length));
      expect(frame, isNotNull);
      expect(frame!.type, msgProtoBackup);
      final decoded = assembler.add(frame.payload, frames++);
      if (decoded != null) {
        expect(result, isNull);
        result = decoded;
      }
      off += length;
    }
    expect(frames, 31);
    expect(result!.version, 8);
    for (int c = 0; c < 7; c++) {
      expect(result.prototypes[c][c + 1], 1);
      expect(result.prototypes[c].fold<double>(0, (s, v) => s + v), 1);
    }
  });
}
