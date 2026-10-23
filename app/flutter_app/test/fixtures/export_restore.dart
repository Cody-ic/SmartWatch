import 'dart:io';
import 'dart:typed_data';
import 'package:smartwatch_app/ble_protocol.dart';

void main(List<String> args) {
  final prototypes = List.generate(7, (c) => Float32List(128)..[c + 1] = 2);
  final full = encodeProtoPayload(11, prototypes);
  final bytes = BytesBuilder()
    ..add(packFrame(
        msgProtoRestore, protoBeginPayload(0xa1b2c3d4, protoCrc32(full))));
  for (final p in protoFragments(full)) {
    bytes.add(packFrame(msgProtoRestore, p));
  }
  File(args.single).writeAsBytesSync(bytes.takeBytes());
}
