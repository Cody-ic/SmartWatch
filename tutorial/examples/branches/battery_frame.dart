// One-frame decoder for the first protocol exercise.
// The real app assembles fragmented notifications before decoding.
import 'dart:typed_data';

int? batteryMillivolts(Uint8List frame) {
  if (frame.length != 7 || frame[0] != 0xAA ||
      frame[1] != 0x06 || frame[2] != 3) {
    return null;
  }
  var sum = 0;
  for (var i = 0; i < frame.length - 1; i++) {
    sum = (sum + frame[i]) & 0xFF;
  }
  if (((-sum) & 0xFF) != frame.last) return null;
  return ByteData.sublistView(frame, 3, 5).getUint16(0, Endian.little);
}

void main() {
  final frame = Uint8List.fromList([0xAA, 0x06, 3, 0xA0, 0x0F, 60, 0x62]);
  assert(batteryMillivolts(frame) == 4000);
  print(batteryMillivolts(frame));
  frame[3] ^= 1;
  assert(batteryMillivolts(frame) == null);
  print(batteryMillivolts(frame));
}
