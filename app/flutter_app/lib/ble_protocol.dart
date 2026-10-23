/// SmartWatch BLE 协议 — 与 STM32 端 ble_uart.c 完全一致
///
/// 帧: [0xAA][type][len][payload][checksum]
library;

import 'dart:typed_data';

// 消息类型
const int msgHarResult = 0x01;
const int msgHrData = 0x02;
const int msgSafetyAlert = 0x03;
const int msgStatus = 0x04;
const int msgSteps = 0x05;
const int msgBattery = 0x06; // [2026-07-03]
const int msgSleepApnea = 0x07; // [2026-07-05]: 睡眠呼吸暂停报告
const int msgSleepStage = 0x08; // [2026-07-07]: 睡眠分期 P(sleep) — STM32 每30s发送
const int msgProtoResult = 0x0A; // [2026-07-14]: Proto 推理结果 (手表→手机)
const int msgSpo2Data = 0x09; // [2026-07-07]: 实时 SpO2 — STM32 周期性发送
const int msgCmdResp = 0x80; // [2026-07-04]: 命令/配置 (phone → watch)
const int msgSessionCtrl = 0x0C; // [2026-07-04]: 运动 session 控制
const int msgProtoStatus = 0x0D; // [2026-07-04]: 原型状态查询
const int msgProtoBackup = 0x0E; // [2026-07-04]: 原型备份
const int msgProtoAck = 0x10;
const int msgProtoRestore = 0x0F; // [2026-07-04]: 原型恢复
const int msgTimeSync = 0x91;

// 0x0C session 控制子命令
const int sessionStart = 0x01;
const int sessionEnd = 0x02;
const int sessionAbort = 0x03;

// 0x0E/0x0F 原型数据
const int protoFull = 0x01;
const int protoFactory = 0x02;
const int protoRequestBackup = 0x03; // [2026-07-14]: 手机请求手表发送 0x0E backup
const int protoNClasses = 7;
const int protoDim = 128;
const int protoBegin = 0xF0;
const int protoFullSize = 2 + protoNClasses * protoDim * 4;
const int protoFragDataMax = 122;
const int protoFragCount =
    (protoFullSize + protoFragDataMax - 1) ~/ protoFragDataMax;

const packetHeader = 0xAA;

const activityNames = {
  0: 'Lying',
  1: 'Sitting',
  2: 'Standing',
  3: 'Walking',
  4: 'Jogging',
  5: 'Downstairs',
  6: 'Cycling',
  255: 'Unknown',
};

class HARResult {
  final int activity;
  final double confidence;
  String get name => activityNames[activity] ?? 'Unknown';
  HARResult(this.activity, this.confidence);
}

class HRData {
  final double bpmSmoothed;
  final double bpmRaw;
  final bool valid;
  HRData(this.bpmSmoothed, this.bpmRaw, this.valid);
}

class SafetyAlert {
  final bool hrAnomaly, acousticAlert, imuAnomaly;
  final int timestampMs;
  SafetyAlert(
      this.hrAnomaly, this.acousticAlert, this.imuAnomaly, this.timestampMs);
}

class StepsData {
  final int steps, goal, kcal;
  final double distanceKm;
  StepsData(this.steps, this.goal, this.distanceKm, this.kcal);
}

class StatusData {
  final double calorie, score, currentHr;
  final int exerciseTimeS, currentAct;
  String get actName => activityNames[currentAct] ?? '?';
  StatusData(this.calorie, this.score, this.exerciseTimeS, this.currentAct,
      this.currentHr);
}

class BatteryData {
  final int mv;
  final int pct;
  BatteryData(this.mv, this.pct);
}

// ---- 解析 ----

int _u16le(Uint8List d, int off) => (d[off + 1] << 8) | d[off];
int _u32le(Uint8List d, int off) =>
    (d[off + 3] << 24) | (d[off + 2] << 16) | (d[off + 1] << 8) | d[off];
double _f32le(Uint8List d, int off) {
  final buf = ByteData.view(d.buffer, d.offsetInBytes + off, 4);
  final v = buf.getFloat32(0, Endian.little);
  // [FIX] 过滤 NaN/Inf, 防止恶意设备发送异常浮点数导致 UI 异常 (#19)
  if (v.isNaN || v.isInfinite) return 0.0;
  return v;
}

HARResult parseHar(Uint8List p) =>
    HARResult(p[0] & 0x07, _u16le(p, 1) / 1000.0);

HRData parseHr(Uint8List p) =>
    HRData(_u16le(p, 0) / 10.0, _u16le(p, 2) / 10.0, p[4] != 0);

SafetyAlert parseSafety(Uint8List p) =>
    SafetyAlert(p[0] != 0, p[1] != 0, p[2] != 0, _u32le(p, 3));

StepsData parseSteps(Uint8List p) =>
    StepsData(_u32le(p, 0), _u32le(p, 4), _u16le(p, 8) / 1000.0, _u16le(p, 10));

StatusData parseStatus(Uint8List p) =>
    StatusData(_f32le(p, 0), _f32le(p, 4), _u32le(p, 8), p[12], _f32le(p, 13));

BatteryData parseBattery(Uint8List p) =>
    BatteryData(_u16le(p, 0), p[2]); /* [2026-07-03]: 3B: mV(LE) + pct */

// ---- 打包 ----

int _checksum(Uint8List data) =>
    ((~data.fold<int>(0, (s, b) => s + b)) + 1) & 0xFF;

Uint8List packFrame(int type, Uint8List payload) {
  if (payload.length > 124) {
    throw ArgumentError('Frame payload exceeds 124 bytes');
  }
  final hdr = Uint8List(3)
    ..[0] = packetHeader
    ..[1] = type
    ..[2] = payload.length;
  final frame = Uint8List.fromList([...hdr, ...payload]);
  return Uint8List.fromList([...frame, _checksum(frame)]);
}

Uint8List packTimeSync(int unixTimestamp) {
  final payload = ByteData(4)..setUint32(0, unixTimestamp, Endian.big);
  return packFrame(msgTimeSync, payload.buffer.asUint8List());
}

/// [2026-07-04] Prototype network protocol

Uint8List packSessionStart(int classId) =>
    packFrame(msgSessionCtrl, Uint8List.fromList([sessionStart, classId]));

Uint8List packSessionEnd() =>
    packFrame(msgSessionCtrl, Uint8List.fromList([sessionEnd]));

Uint8List packSessionAbort() =>
    packFrame(msgSessionCtrl, Uint8List.fromList([sessionAbort]));

class ProtoStatus {
  final int version;
  final List<int> sampleCounts;
  ProtoStatus(this.version, this.sampleCounts);
}

ProtoStatus parseProtoStatus(Uint8List p) {
  final counts = List.generate(protoNClasses, (i) => _u16le(p, 1 + i * 2));
  return ProtoStatus(p[0], counts);
}

class ProtoData {
  final int version;
  final List<Float32List> prototypes;
  ProtoData(this.version, this.prototypes);
}

ProtoData parseProtoBackup(Uint8List p) {
  if (p.length != protoFullSize || p[0] != protoFull) {
    throw const FormatException('Invalid prototype payload');
  }
  final bytes = ByteData.sublistView(p);
  final protos = List.generate(protoNClasses, (c) {
    final values = Float32List(protoDim);
    for (int d = 0; d < protoDim; d++) {
      final v = bytes.getFloat32(2 + (c * protoDim + d) * 4, Endian.little);
      if (!v.isFinite || v.abs() >= 1e10) {
        throw const FormatException('Invalid prototype value');
      }
      values[d] = v;
    }
    return values;
  });
  return ProtoData(p[1], protos);
}

Uint8List encodeProtoPayload(int version, List<Float32List> prototypes) {
  if (version < 0 ||
      version > 255 ||
      prototypes.length != protoNClasses ||
      prototypes.any((p) => p.length != protoDim)) {
    throw ArgumentError('Expected a byte version and 7 x 128 prototypes');
  }
  final data = ByteData(protoFullSize)
    ..setUint8(0, protoFull)
    ..setUint8(1, version);
  for (int c = 0; c < protoNClasses; c++) {
    for (int d = 0; d < protoDim; d++) {
      final v = prototypes[c][d];
      if (!v.isFinite || v.abs() >= 1e10) {
        throw ArgumentError('Invalid prototype value');
      }
      data.setFloat32(2 + (c * protoDim + d) * 4, v, Endian.little);
    }
  }
  return data.buffer.asUint8List();
}

int protoCrc32(Uint8List data) {
  int crc = 0xffffffff;
  for (final byte in data) {
    crc ^= byte;
    for (int b = 0; b < 8; b++) {
      crc = (crc >> 1) ^ ((crc & 1) != 0 ? 0xedb88320 : 0);
    }
  }
  return (crc ^ 0xffffffff) & 0xffffffff;
}

Uint8List protoBeginPayload(int id, int crc) => (ByteData(9)
      ..setUint8(0, protoBegin)
      ..setUint32(1, id, Endian.little)
      ..setUint32(5, crc, Endian.little))
    .buffer
    .asUint8List();

List<Uint8List> protoFragments(Uint8List full) {
  if (full.length != protoFullSize) {
    throw ArgumentError('Invalid prototype length');
  }
  return List.generate(protoFragCount, (i) {
    final off = i * protoFragDataMax;
    final end = (off + protoFragDataMax).clamp(0, full.length);
    return Uint8List.fromList([i, protoFragCount, ...full.sublist(off, end)]);
  });
}

class ProtoAck {
  final int transferId, index, status;
  ProtoAck(this.transferId, this.index, this.status);
}

ProtoAck parseProtoAck(Uint8List p) {
  if (p.length != 6) throw const FormatException('Invalid prototype ACK');
  return ProtoAck(
      ByteData.sublistView(p).getUint32(0, Endian.little), p[4], p[5]);
}

/// A requested backup is accepted only after its matching BEGIN and CRC32.
class ProtoBackupAssembler {
  final int transferId;
  Uint8List? _data;
  int _mask = 0, _crc = 0, _lastMs = 0;
  bool _complete = false;
  ProtoBackupAssembler(this.transferId);

  ProtoData? add(Uint8List payload, int nowMs) {
    if (_data != null && nowMs - _lastMs > 5000) {
      _data = null;
      _mask = 0;
    }
    if (payload.length == 9 && payload[0] == protoBegin) {
      final b = ByteData.sublistView(payload);
      if (b.getUint32(1, Endian.little) != transferId) return null;
      final crc = b.getUint32(5, Endian.little);
      if (_data == null) {
        _data = Uint8List(protoFullSize);
        _mask = 0;
        _crc = crc;
        _complete = false;
      } else if (crc != _crc) {
        _data = null;
        throw const FormatException('Conflicting backup transaction');
      }
      _lastMs = nowMs;
      return null;
    }
    if (_data == null) return null;
    if (payload.length < 2 ||
        payload[1] != protoFragCount ||
        payload[0] >= protoFragCount) {
      _data = null;
      throw const FormatException('Invalid backup fragment');
    }
    final idx = payload[0], off = payload[0] * protoFragDataMax;
    final count = (protoFullSize - off).clamp(0, protoFragDataMax);
    if (payload.length != count + 2) {
      _data = null;
      throw const FormatException('Truncated backup fragment');
    }
    final bit = 1 << idx;
    if ((_mask & bit) != 0) {
      for (int i = 0; i < count; i++) {
        if (_data![off + i] != payload[i + 2]) {
          _data = null;
          throw const FormatException('Conflicting backup fragment');
        }
      }
    } else {
      _data!.setRange(off, off + count, payload, 2);
      _mask |= bit;
    }
    _lastMs = nowMs;
    if (!_complete && _mask == (1 << protoFragCount) - 1) {
      if (protoCrc32(_data!) != _crc) {
        _data = null;
        throw const FormatException('Backup checksum mismatch');
      }
      final result = parseProtoBackup(_data!);
      _complete = true;
      return result;
    }
    return null;
  }
}

Uint8List packProtoRestoreFactory() =>
    packFrame(msgProtoRestore, Uint8List.fromList([protoFactory]));

/// [2026-07-14]: 请求手表发送 0x0E proto backup
Uint8List packProtoRequestBackup(int transferId) => packFrame(
    msgProtoRestore,
    (ByteData(5)
          ..setUint8(0, protoRequestBackup)
          ..setUint32(1, transferId, Endian.little))
        .buffer
        .asUint8List());

/// [2026-07-14]: Proto 推理结果 — class(1B) + confidence(f32 LE) = 5B
class ProtoResult {
  final int protoClass;
  final double confidence;
  String get name => activityNames[protoClass] ?? 'Unknown';
  ProtoResult(this.protoClass, this.confidence);
}

ProtoResult parseProtoResult(Uint8List p) =>
    ProtoResult(p[0] & 0x07, _f32le(p, 1));

/// [2026-07-05] 睡眠呼吸暂停报告 — 13B: severity(1B)+odi(f32LE)+events(u16LE)
///   +lowestSpo2(1B)+avgDur(u16LE)+monitorMin(u16LE)+isAsleep(1B)
class SleepApnea {
  final int severity, totalEvents, lowestSpo2, avgDurS, monitorMin;
  final double odi;
  final bool isAsleep;
  SleepApnea(this.severity, this.odi, this.totalEvents, this.lowestSpo2,
      this.avgDurS, this.monitorMin, this.isAsleep);
}

SleepApnea parseSleepApnea(Uint8List p) => SleepApnea(p[0], _f32le(p, 1),
    _u16le(p, 5), p[7], _u16le(p, 8), _u16le(p, 10), p[12] != 0);

/// [2026-07-07] 睡眠分期 — 5B: p_sleep(f32 LE) + is_asleep(1B)
/// STM32 端每30s发送一帧, P(sleep)>0.5 判定为入睡
class SleepStage {
  final double pSleep;
  final bool isAsleep;
  SleepStage(this.pSleep, this.isAsleep);
}

SleepStage parseSleepStage(Uint8List p) => SleepStage(_f32le(p, 0), p[4] != 0);

/// [2026-07-07] 实时 SpO2 — 5B: spo2(f32 LE) + valid(1B)
/// 比 SleepApnea.lowestSpo2 更即时, 用于 UI 实时刷新
class Spo2Data {
  final double spo2;
  final bool valid;
  Spo2Data(this.spo2, this.valid);
}

Spo2Data parseSpo2(Uint8List p) => Spo2Data(_f32le(p, 0), p[4] != 0);

/// [2026-07-04]: 发送用户参数配置 (体重 + 年龄)
/// 帧: [0xAA][0x80][0x06][0x01][weight_kg:float LE 4B][age:1B][checksum]
/// STM32 端 ble_uart.c 收到后调 app_set_user_profile() 更新运动评分模型
Uint8List packSetProfile(double weightKg, int age) {
  final payload = ByteData(6)
    ..setUint8(0, 0x01) // sub-cmd: SET_PROFILE
    ..setFloat32(1, weightKg, Endian.little)
    ..setUint8(5, age);
  return packFrame(msgCmdResp, payload.buffer.asUint8List());
}

// ---- 接收帧解析 ----

({int type, Uint8List payload})? parseFrame(Uint8List raw) {
  if (raw.length < 4 || raw[0] != packetHeader) return null;
  final len = raw[2];
  if (raw.length < 4 + len) return null;
  final payload = raw.sublist(3, 3 + len);
  final ckRecv = raw[3 + len];
  final ckCalc = _checksum(raw.sublist(0, 3 + len));
  if (ckRecv != ckCalc) return null;
  return (type: raw[1], payload: payload);
}
