/// SmartWatch BLE 服务 — 扫描、连接、收发
///
/// CH582M 服务 UUID: 0000FFE0-0000-1000-8000-00805F9B34FB
/// TX (手表→手机): 0000FFE3-... (Notify)
/// RX (手机→手表): 0000FFE2-... (Write No Response)
library;

import 'dart:async';
import 'dart:typed_data';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'ble_protocol.dart';
import 'proto_transfer.dart';

final serviceUuid = Guid("0000FFE0-0000-1000-8000-00805F9B34FB");
final txCharUuid = Guid("0000FFE3-0000-1000-8000-00805F9B34FB");
final rxCharUuid = Guid("0000FFE2-0000-1000-8000-00805F9B34FB");

class SmartWatchBLE {
  BluetoothDevice? _device;
  BluetoothCharacteristic? _txChar;
  BluetoothCharacteristic? _rxChar;
  final _rxBuffer = <int>[];
  Future<void> _writeTail = Future<void>.value();
  final _protoAckController = StreamController<ProtoAck>.broadcast();
  ProtoBackupAssembler? _backupAssembler;
  bool _protoBusy = false;
  int _transferId = DateTime.now().microsecondsSinceEpoch & 0xffffffff;

  int _nextTransferId() => _transferId = (_transferId + 1) & 0xffffffff;

  Future<void> _writeFrame(Uint8List frame) {
    final operation = _writeTail.then((_) async {
      final c = _rxChar;
      final device = _device;
      if (c == null || device == null || !device.isConnected) {
        throw StateError('Watch is not connected');
      }
      final maxChunk = device.mtuNow - 3;
      if (maxChunk <= 0) throw StateError('Invalid BLE MTU');
      for (int off = 0; off < frame.length; off += maxChunk) {
        final end = (off + maxChunk).clamp(0, frame.length);
        await c.write(frame.sublist(off, end),
            withoutResponse: !c.properties.write);
      }
    });
    _writeTail =
        operation.then<void>((_) {}, onError: (Object _, StackTrace __) {});
    return operation;
  }

  // [FIX P0-5/P0-8] 保存订阅引用, 防止内存泄漏, connect()/disconnect()时可以取消旧订阅
  StreamSubscription? _connStateSub;
  StreamSubscription? _notifySub;
  bool _disposed = false; // [FIX P2-10] 防 double-dispose

  final _harController = StreamController<HARResult>.broadcast();
  final _hrController = StreamController<HRData>.broadcast();
  final _safetyController = StreamController<SafetyAlert>.broadcast();
  final _stepsController = StreamController<StepsData>.broadcast();
  final _statusController = StreamController<StatusData>.broadcast();
  final _batteryController = StreamController<BatteryData>.broadcast();
  final _logController = StreamController<String>.broadcast();
  final _connController = StreamController<bool>.broadcast();

  Stream<HARResult> get onHar => _harController.stream;
  Stream<HRData> get onHr => _hrController.stream;
  Stream<SafetyAlert> get onSafety => _safetyController.stream;
  Stream<StepsData> get onSteps => _stepsController.stream;
  Stream<StatusData> get onStatus => _statusController.stream;
  Stream<BatteryData> get onBattery => _batteryController.stream;
  Stream<String> get onLog => _logController.stream;
  Stream<bool> get onConnection => _connController.stream;

  bool get isConnected => _device?.isConnected ?? false;

  void _log(String msg) => _logController.add(msg);

  // ---- 扫描 ----
  Future<List<ScanResult>> scan(
      {Duration timeout = const Duration(seconds: 5)}) async {
    _log("Scanning...");
    // [FIX P0-3] 先检查蓝牙适配器是否已开启
    try {
      await FlutterBluePlus.adapterState
          .where((s) => s == BluetoothAdapterState.on)
          .first
          .timeout(const Duration(seconds: 3));
    } catch (_) {
      _log("Bluetooth adapter is OFF or not responding");
      return [];
    }
    final results = <ScanResult>[];
    StreamSubscription? scanSub;
    try {
      // [FIX] try/finally 确保 startScan 异常时也取消订阅, 不泄漏
      scanSub = FlutterBluePlus.scanResults.listen((list) {
        for (final r in list) {
          final name = (r.advertisementData.advName.isNotEmpty
                  ? r.advertisementData.advName
                  : r.device.platformName)
              .toLowerCase();
          if ((name.contains('smartwatch') || name.contains('ch583')) &&
              !results.any((e) => e.device.remoteId == r.device.remoteId)) {
            results.add(r);
            _log("Found: $name [${r.device.remoteId}] RSSI:${r.rssi}");
          }
        }
      });
      await FlutterBluePlus.startScan(
          withServices: [serviceUuid], timeout: timeout);
      // [FIX P0-7] 等待扫描真正结束, 而非立即 stopScan
      await FlutterBluePlus.isScanning.where((v) => v == false).first;
    } catch (e) {
      _log("Scan error: $e");
    } finally {
      await scanSub?.cancel(); // [FIX] 无论成功失败都取消订阅
    }
    _log("Scan done: ${results.length} device(s)");
    return results;
  }

  // ---- 连接 ----
  Future<bool> connect(BluetoothDevice device) async {
    try {
      // [FIX P0-8] 先取消旧订阅, 再保存新订阅, 防止每次 connect() 泄漏
      await _connStateSub?.cancel();
      await _notifySub?.cancel();
      _txChar = null;
      _rxChar = null;
      _rxBuffer.clear();
      _backupAssembler = null;
      _device = device;
      await device.connect(timeout: const Duration(seconds: 10));
      _connController.add(true);
      _log("Connected to ${device.platformName}");

      // [FIX P0-8] 保存 connectionState 订阅引用, disconnect/dispose 时可取消
      _connStateSub = device.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected) {
          _log("Device disconnected unexpectedly");
          _rxBuffer.clear();
          _backupAssembler = null;
          _txChar = null;
          _rxChar = null;
          _connController.add(false);
        }
      });

      // 发现服务
      final services = await device.discoverServices();
      for (final s in services) {
        for (final c in s.characteristics) {
          if (c.uuid == txCharUuid) {
            _txChar = c;
            await c.setNotifyValue(true);
            // [FIX P0-5] 保存 lastValueStream 订阅引用, 防止泄漏 + 旧数据重放
            _notifySub = c.lastValueStream.listen(_onNotify);
            _log("Subscribed to TX notify");
          }
          if (c.uuid == rxCharUuid) {
            _rxChar = c;
          }
        }
      }

      // [FIX P1-11] 检查 _txChar 是否找到, TX 特征值缺失则无法接收任何数据
      if (_txChar == null) {
        _log(
            "WARNING: TX notify characteristic not found — no data will be received");
      }
      // 发送用户参数（默认值，与固件一致）
      await sendUserProfile(65.0, 22);

      // 发送授时
      if (_rxChar == null) {
        _log("WARNING: RX characteristic not found, time sync skipped");
      } else {
        final unix = DateTime.now().millisecondsSinceEpoch ~/ 1000;
        final frame = packTimeSync(unix);
        await _writeFrame(frame);
        _log("Time sync sent: $unix");
      }

      return true;
    } catch (e) {
      _log("Connect failed: $e");
      return false;
    }
  }

  Future<void> disconnect() async {
    // [FIX P0-5/P0-8] 取消所有活跃订阅
    await _connStateSub?.cancel();
    _connStateSub = null;
    await _notifySub?.cancel();
    _notifySub = null;
    await _device?.disconnect();
    _device = null;
    _txChar = null;
    _rxChar = null;
    _rxBuffer.clear();
    _backupAssembler = null;
    _connController.add(false);
    _log("Disconnected");
  }

  /// [2026-07-04]: 发送用户参数配置到手表 (体重 + 年龄)
  /// STM32 端: ble_uart_dispatch → app_set_user_profile() → exercise_scoring 更新
  Future<void> sendUserProfile(double weightKg, int age) async {
    if (_rxChar == null) {
      _log("Cannot send profile: not connected");
      return;
    }
    final frame = packSetProfile(weightKg, age);
    await _writeFrame(frame);
    _log("Profile sent: ${weightKg.toStringAsFixed(1)}kg, $age years");
  }

  /// [2026-07-04]: 原型网络 — session 控制 + 原型备份/恢复

  Future<void> sendSessionStart(int classId) async {
    if (_rxChar == null) return;
    await _writeFrame(packSessionStart(classId));
    _log("Session start: class=$classId");
  }

  Future<void> sendSessionEnd() async {
    if (_rxChar == null) return;
    await _writeFrame(packSessionEnd());
    _log("Session end");
  }

  Future<void> sendSessionAbort() async {
    if (_rxChar == null) return;
    await _writeFrame(packSessionAbort());
    _log("Session abort");
  }

  /// Restore succeeds only after the watch has checked and committed all bytes.
  Future<void> sendProtoRestoreFull(
      int version, List<Float32List> prototypes) async {
    if (_protoBusy) throw StateError('A prototype transfer is already running');
    _protoBusy = true;
    try {
      await restorePrototypes(
        transferId: _nextTransferId(),
        version: version,
        prototypes: prototypes,
        writeFrame: _writeFrame,
        acknowledgements: _protoAckController.stream,
      );
      _log('Proto restore committed: version=$version');
    } finally {
      _protoBusy = false;
    }
  }

  Future<void> sendProtoRestoreFactory() async {
    if (_protoBusy) throw StateError('A prototype transfer is already running');
    await _writeFrame(packProtoRestoreFactory());
    _log('Proto factory reset requested');
  }

  Future<void> sendProtoRequestBackup() async {
    if (_protoBusy) throw StateError('A prototype transfer is already running');
    _protoBusy = true;
    final id = _nextTransferId();
    _backupAssembler = ProtoBackupAssembler(id);
    try {
      final response = _protoBackupController.stream.first
          .timeout(const Duration(seconds: 12));
      await Future.wait<Object?>([
        _writeFrame(packProtoRequestBackup(id)),
        response,
      ]);
      _log('Proto backup received and checksum verified');
    } finally {
      _backupAssembler = null;
      _protoBusy = false;
    }
  }

  Stream<ProtoStatus> get onProtoStatus => _protoStatusController.stream;
  final _protoStatusController = StreamController<ProtoStatus>.broadcast();

  Stream<ProtoResult> get onProtoResult =>
      _protoResultController.stream; /* [2026-07-14] */
  Stream<ProtoData> get onProtoBackup => _protoBackupController.stream;
  Stream<SleepStage> get onSleepStage =>
      _sleepStageController.stream; /* [2026-07-07] */
  Stream<Spo2Data> get onSpo2 => _spo2Controller.stream; /* [2026-07-07] */
  Stream<SleepApnea> get onSleepApnea => _sleepApneaController.stream;
  final _sleepApneaController = StreamController<SleepApnea>.broadcast();
  final _sleepStageController =
      StreamController<SleepStage>.broadcast(); /* [2026-07-07] */
  final _spo2Controller =
      StreamController<Spo2Data>.broadcast(); /* [2026-07-07] */
  final _protoResultController =
      StreamController<ProtoResult>.broadcast(); /* [2026-07-14] */
  final _protoBackupController = StreamController<ProtoData>.broadcast();

  // ---- 通知回调 ----
  void _onNotify(List<int> data) {
    _rxBuffer.addAll(data);
    while (_rxBuffer.isNotEmpty) {
      // 找 0xAA 帧头
      if (_rxBuffer[0] != packetHeader) {
        _rxBuffer.removeAt(0);
        continue;
      }
      if (_rxBuffer.length < 3) break;
      final payloadLen = _rxBuffer[2];
      // [FIX P2-3] 上限校验: 最大常规消息 17B, proto backup 分片 ≤124B, 留余量 128B
      // [2026-07-14]: 从 32 → 128, 否则 proto backup (0x0E) 分片全部被丢弃
      if (payloadLen > 124) {
        _rxBuffer.removeAt(0);
        _log("[OVERFLOW len=$payloadLen]");
        continue;
      }
      final total = 4 + payloadLen;
      if (_rxBuffer.length < total) break;

      final frame = Uint8List.fromList(_rxBuffer.sublist(0, total));
      _rxBuffer.removeRange(0, total);
      final parsed = parseFrame(frame);
      // [FIX] 记录解析失败的帧, 便于调试, 与 Python 端 [BAD FRAME] 日志保持一致 (#14)
      if (parsed == null) {
        _log("[BAD FRAME] ${frame.length}B");
        continue;
      }
      _dispatch(parsed.type, parsed.payload);
    }
  }

  void _dispatch(int type, Uint8List payload) {
    try {
      switch (type) {
        case msgHarResult:
          if (payload.length >= 5) _harController.add(parseHar(payload));
        case msgHrData:
          if (payload.length >= 5) _hrController.add(parseHr(payload));
        case msgSafetyAlert:
          if (payload.length >= 8) _safetyController.add(parseSafety(payload));
        case msgSteps:
          if (payload.length >= 16) _stepsController.add(parseSteps(payload));
        case msgStatus:
          if (payload.length >= 17) _statusController.add(parseStatus(payload));
        case msgBattery:
          if (payload.length >= 3) {
            _batteryController.add(parseBattery(payload));
          }
        case msgProtoResult: /* [2026-07-14] */
          if (payload.length >= 5) {
            _protoResultController.add(parseProtoResult(payload));
          }
        case msgProtoStatus:
          if (payload.length >= 15) {
            _protoStatusController.add(parseProtoStatus(payload));
          }
        case msgSleepApnea:
          if (payload.length >= 13) {
            _sleepApneaController.add(parseSleepApnea(payload));
          }
        case msgSleepStage: /* [2026-07-07]: STM32 每30s发送 */
          if (payload.length >= 5) {
            _sleepStageController.add(parseSleepStage(payload));
          }
        case msgSpo2Data: /* [2026-07-07]: 实时 SpO2 */
          if (payload.length >= 5) _spo2Controller.add(parseSpo2(payload));
        case msgProtoAck:
          _protoAckController.add(parseProtoAck(payload));
        case msgProtoBackup:
          _handleProtoBackupFrag(payload); /* [2026-07-14]: 分片重组 */
        // [FIX P2-5] 未知消息类型记录日志, 与 Python 端 [UNKNOWN] 一致
        default:
          _log(
              "[UNKNOWN 0x${type.toRadixString(16).padLeft(2, '0')}] ${payload.length}B");
      }
    } catch (e) {
      // [FIX] 记录解析异常, 不再静默吞噬, 便于调试 (#5)
      _log("[PARSE ERROR 0x${type.toRadixString(16).padLeft(2, '0')}] $e");
    }
  }

  void _handleProtoBackupFrag(Uint8List payload) {
    final result =
        _backupAssembler?.add(payload, DateTime.now().millisecondsSinceEpoch);
    if (result != null) _protoBackupController.add(result);
  }

  void dispose() {
    // [FIX P2-10] 防 double-dispose
    if (_disposed) return;
    _disposed = true;
    _protoAckController.close();
    _backupAssembler = null;
    // [FIX P1-9] 发送断连请求 + 取消订阅 (fire-and-forget, dispose 不需要 await)
    _device?.disconnect();
    _connStateSub?.cancel();
    _notifySub?.cancel();
    // [FIX P2-10] isClosed 检查防 double-close
    if (!_harController.isClosed) _harController.close();
    if (!_hrController.isClosed) _hrController.close();
    if (!_safetyController.isClosed) _safetyController.close();
    if (!_stepsController.isClosed) _stepsController.close();
    if (!_statusController.isClosed) _statusController.close();
    if (!_batteryController.isClosed) _batteryController.close();
    if (!_protoResultController.isClosed) {
      _protoResultController.close(); /* [2026-07-14] */
    }
    if (!_protoStatusController.isClosed) _protoStatusController.close();
    if (!_sleepApneaController.isClosed) _sleepApneaController.close();
    if (!_sleepStageController.isClosed) {
      _sleepStageController.close(); /* [2026-07-07] */
    }
    if (!_spo2Controller.isClosed) _spo2Controller.close(); /* [2026-07-07] */
    if (!_protoBackupController.isClosed) _protoBackupController.close();
    if (!_logController.isClosed) _logController.close();
    if (!_connController.isClosed) _connController.close();
  }
}
