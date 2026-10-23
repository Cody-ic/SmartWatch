import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:smartwatch_app/ble_protocol.dart';
import 'package:smartwatch_app/proto_transfer.dart';

List<Float32List> prototypes() => List.generate(
    7,
    (c) =>
        Float32List.fromList(List.generate(128, (d) => (c * 128 + d) / 1024)));

void main() {
  test('unaligned little-endian payload decodes and owns its values', () {
    final full = encodeProtoPayload(7, prototypes());
    final storage = Uint8List(full.length + 1)
      ..setRange(1, full.length + 1, full);
    final parsed = parseProtoBackup(Uint8List.sublistView(storage, 1));
    expect(parsed.version, 7);
    expect(parsed.prototypes[6][127], 895 / 1024);
    storage.fillRange(0, storage.length, 0);
    expect(parsed.prototypes[6][127], 895 / 1024);
  });
  test('rejects short payloads, NaN, dimensions and oversized frames', () {
    expect(() => parseProtoBackup(Uint8List.fromList([1, 1])),
        throwsFormatException);
    final full = encodeProtoPayload(1, prototypes());
    ByteData.sublistView(full).setFloat32(2, double.nan, Endian.little);
    expect(() => parseProtoBackup(full), throwsFormatException);
    expect(() => encodeProtoPayload(1, []), throwsArgumentError);
    expect(
        () => packFrame(msgProtoRestore, Uint8List(3586)), throwsArgumentError);
  });
  test('CRC32 matches the standard known vector', () {
    expect(
        protoCrc32(Uint8List.fromList(ascii.encode('123456789'))), 0xcbf43926);
  });
  test('backup needs its transaction header and every unique fragment', () {
    final full = encodeProtoPayload(7, prototypes());
    final parts = protoFragments(full);
    final rx = ProtoBackupAssembler(42);
    expect(rx.add(parts[0], 0), isNull);
    expect(rx.add(protoBeginPayload(99, protoCrc32(full)), 1), isNull);
    expect(rx.add(parts[0], 2), isNull);
    rx.add(protoBeginPayload(42, protoCrc32(full)), 3);
    for (int i = parts.length - 1; i >= 1; i--) {
      expect(rx.add(parts[i], 4), isNull);
      expect(rx.add(parts[i], 5), isNull);
    }
    final result = rx.add(parts[0], 6);
    expect(result!.prototypes[6][127], 895 / 1024);
    expect(rx.add(parts[0], 7), isNull);
  });
  test('backup rejects truncated, conflicting and corrupt fragments', () {
    final full = encodeProtoPayload(7, prototypes());
    final parts = protoFragments(full);
    ProtoBackupAssembler start() => ProtoBackupAssembler(42)
      ..add(protoBeginPayload(42, protoCrc32(full)), 0);
    final short = start();
    expect(() => short.add(Uint8List.fromList([0, 30, 1]), 1),
        throwsFormatException);
    final dup = start()..add(parts[0], 1);
    final changed = Uint8List.fromList(parts[0])..[3] ^= 1;
    expect(() => dup.add(changed, 2), throwsFormatException);
    final corrupt = start();
    for (int i = 0; i < parts.length - 1; i++) {
      corrupt.add(i == 0 ? changed : parts[i], 1);
    }
    expect(() => corrupt.add(parts.last, 2), throwsFormatException);
  });
  test('expired backup cannot reuse fragments from a previous attempt', () {
    final full = encodeProtoPayload(7, prototypes()),
        parts = protoFragments(encodeProtoPayload(7, prototypes()));
    final rx = ProtoBackupAssembler(42)
      ..add(protoBeginPayload(42, protoCrc32(full)), 0);
    rx.add(parts[0], 1);
    for (final p in parts.skip(1)) {
      expect(rx.add(p, 6002), isNull);
    }
  });
  test('restore retries lost ACK and ignores ACK from an older transaction',
      () async {
    final acks = StreamController<ProtoAck>.broadcast(sync: true);
    final attempts = <int, int>{};
    await restorePrototypes(
      transferId: 42,
      version: 7,
      prototypes: prototypes(),
      acknowledgements: acks.stream,
      ackTimeout: const Duration(milliseconds: 10),
      retryDelay: Duration.zero,
      writeFrame: (frame) async {
        final p = parseFrame(frame)!.payload;
        final idx = p.length == 9 ? 255 : p[0];
        attempts[idx] = (attempts[idx] ?? 0) + 1;
        acks.add(ProtoAck(99, idx, 0));
        if (idx == 2 && attempts[idx] == 1) return;
        acks.add(ProtoAck(42, idx, idx == 29 ? 1 : 0));
      },
    );
    expect(attempts.length, 31);
    expect(attempts[2], 2);
    await acks.close();
  });
  test('restore has bounded retries and does not claim success without ACK',
      () async {
    final acks = StreamController<ProtoAck>.broadcast();
    int writes = 0;
    await expectLater(
        restorePrototypes(
          transferId: 42,
          version: 7,
          prototypes: prototypes(),
          acknowledgements: acks.stream,
          ackTimeout: const Duration(milliseconds: 5),
          retryDelay: Duration.zero,
          writeFrame: (_) async {
            writes++;
          },
        ),
        throwsA(isA<TimeoutException>()));
    expect(writes, 3);
    await acks.close();
  });
  test('restore stops immediately when the watch rejects a fragment', () async {
    final acks = StreamController<ProtoAck>.broadcast(sync: true);
    int writes = 0;
    await expectLater(
        restorePrototypes(
          transferId: 42,
          version: 7,
          prototypes: prototypes(),
          acknowledgements: acks.stream,
          writeFrame: (_) async {
            writes++;
            acks.add(ProtoAck(42, 255, 2));
          },
        ),
        throwsStateError);
    expect(writes, 1);
    await acks.close();
  });
}
