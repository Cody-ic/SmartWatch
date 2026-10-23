import 'dart:async';
import 'dart:typed_data';
import 'ble_protocol.dart';

/// Stop-and-wait keeps at most one fragment in the BLE/UART bridge.
Future<void> restorePrototypes({
  required int transferId,
  required int version,
  required List<Float32List> prototypes,
  required Future<void> Function(Uint8List) writeFrame,
  required Stream<ProtoAck> acknowledgements,
  Duration ackTimeout = const Duration(milliseconds: 1500),
  Duration retryDelay = const Duration(milliseconds: 150),
}) async {
  final full = encodeProtoPayload(version, prototypes);
  final payloads = [
    protoBeginPayload(transferId, protoCrc32(full)),
    ...protoFragments(full)
  ];
  for (int n = 0; n < payloads.length; n++) {
    final index = n == 0 ? 255 : n - 1;
    ProtoAck? ack;
    for (int attempt = 0; attempt < 3; attempt++) {
      final response = acknowledgements
          .where((a) => a.transferId == transferId && a.index == index)
          .first
          .timeout(ackTimeout);
      try {
        // Register the ACK listener before writing, including synchronous test transports.
        final result = await Future.wait<Object?>([
          Future.sync(
              () => writeFrame(packFrame(msgProtoRestore, payloads[n]))),
          response,
        ]);
        ack = result[1] as ProtoAck;
        break;
      } catch (_) {
        if (attempt == 2) rethrow;
        await Future<void>.delayed(retryDelay);
      }
    }
    final expected = n == payloads.length - 1 ? 1 : 0;
    if (ack == null || ack.status != expected) {
      throw StateError('Watch rejected prototype fragment $index');
    }
  }
}
