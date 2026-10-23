/// SmartWatch 仪表盘页面
library;

import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'ble_service.dart';
import 'ble_protocol.dart';     /* [2026-07-05] SleepApnea type */
import 'emergency_contacts.dart';
import 'emergency_service.dart';
import 'emergency_alert.dart';  /* [2026-07-07] 紧急告警全屏遮罩 */

class DashboardPage extends StatefulWidget {
  final SmartWatchBLE? ble;
  const DashboardPage({super.key, this.ble});

  @override
  State<DashboardPage> createState() => _DashboardPageState();
}

class _DashboardPageState extends State<DashboardPage> {
  late final _ble = widget.ble ?? SmartWatchBLE();
  List<ScanResult> _devices = [];
  bool _scanning = false;
  bool _connected = false;

  // Dashboard data
  double _bpm = 0;
  bool _hrValid = false;
  String _activity = '--';
  double _confidence = 0;
  int _steps = 0;
  int _goal = 10000;
  double _distKm = 0;
  int _kcal = 0;
  String _alertText = 'No alerts';
  Color _alertColor = Colors.green;
  double _calorie = 0;
  double _score = 0;
  int _exMin = 0, _exSec = 0;
  String _exAct = '?';
  int _batPct = 0;

  // [2026-07-04]: 用户参数 (与固件默认值一致: 65kg, 22岁)
  double _userWeight = 65.0;
  int _userAge = 22;

  // [2026-07-04]: 原型网络 — session 控制状态
  int _tabIndex = 0;  // 0=Dash, 1=Session, 2=Profile
  bool _sessionActive = false;
  int _sessionClassId = 3; // 默认 walking
  String _sessionClassName = 'Walking';

  // [2026-07-04]: 原型个性化进度 (手表回复 0x0D)
  List<int> _protoCounts = [0, 0, 0, 0, 0, 0, 0];
  int _protoVersion = 0;
  ProtoData? _protoBackup;   // [2026-07-14]: 从手表拉取的完整原型数据
  String _protoActivity = ''; // [2026-07-14]: Proto 推理结果

  // [2026-07-05]: 睡眠 + SpO2 + 呼吸暂停
  double _spo2Value = 0.0;
  bool _spo2Valid = false;
  double _sleepProb = 0.0;   // [2026-07-07]: 实时入睡概率 (0x08)
  SleepApnea? _lastApnea;
  // 睡眠 session 跟踪: 累积入睡分钟数 + 总监测分钟数
  Timer? _spo2ExpiryTimer;
  bool _backupBusy = false;
  String _sleepStatus = 'No data yet';  // "Since 23:15 · 4h 02m" or "Last: 7h 12m (89%)"

  final _log = <String>[];

  // [2026-07-07] Emergency alert state
  bool _showEmergencyOverlay = false;
  AlertType _currentAlertType = AlertType.fall;

  // [FIX P1-10] 保存所有 14 个 stream 订阅, dispose 时显式取消, 不再依赖 Controller.close 的间接清理
  StreamSubscription? _harSub;
  StreamSubscription? _hrSub;
  StreamSubscription? _safetySub;
  StreamSubscription? _stepsSub;
  StreamSubscription? _statusSub;
  StreamSubscription? _batterySub;
  StreamSubscription? _connSub;
  StreamSubscription? _protoSub;
  StreamSubscription? _protoBackupSub;  /* [2026-07-14] */
  StreamSubscription? _protoResultSub;   /* [2026-07-14] */
  StreamSubscription? _apneaSub;
  StreamSubscription? _sleepStageSub;  /* [2026-07-07] */
  StreamSubscription? _spo2Sub;       /* [2026-07-07] */
  StreamSubscription? _logSub;

  @override
  void initState() {
    super.initState();
    EmergencyContacts.load().then((_) { if (mounted) setState(() {}); });  // [2026-07-07]: 加载紧急联系人
    _harSub = _ble.onHar.listen((r) => setState(() { _activity = r.name; _confidence = r.confidence; }));
    _hrSub = _ble.onHr.listen((r) => setState(() { _bpm = r.bpmSmoothed; _hrValid = r.valid; }));
    _safetySub = _ble.onSafety.listen((r) {
      final a = <String>[];
      if (r.hrAnomaly) a.add('HR');
      if (r.acousticAlert) a.add('Shout');
      if (r.imuAnomaly) a.add('Motion');
      setState(() {
        _alertText = a.isEmpty ? 'No alerts' : 'ALERT: ${a.join(', ')}';
        _alertColor = a.isEmpty ? Colors.green : Colors.red;
      });
      // [2026-07-07]: IMU异常(跌倒/霸凌) → 全屏告警 + 短信通知
      if (r.imuAnomaly && !_showEmergencyOverlay) {
        setState(() {
          _currentAlertType = r.acousticAlert ? AlertType.bullying : AlertType.fall;
          _showEmergencyOverlay = true;
        });
      }
    });
    _stepsSub = _ble.onSteps.listen((r) => setState(() {
      _steps = r.steps; _goal = r.goal; _distKm = r.distanceKm; _kcal = r.kcal;
    }));
    _statusSub = _ble.onStatus.listen((r) => setState(() {
      _calorie = r.calorie; _score = r.score;
      _exMin = r.exerciseTimeS ~/ 60; _exSec = r.exerciseTimeS % 60;
      _exAct = r.actName;
    }));
    _batterySub = _ble.onBattery.listen((r) => setState(() { _batPct = r.pct; }));
    _connSub = _ble.onConnection.listen((c) => setState(() {
      _connected = c;
      if (!c) {
        _spo2ExpiryTimer?.cancel();
        _spo2Valid = false;
        _spo2Value = 0;
        _sleepProb = 0;
      }
    }));
    _protoSub = _ble.onProtoStatus.listen((r) => setState(() {
      _protoVersion = r.version;
      _protoCounts = r.sampleCounts;
    }));
    /* [2026-07-14]: 接收手表发送的完整原型 backup */
    _protoBackupSub = _ble.onProtoBackup.listen((r) => setState(() {
      _protoBackup = r;
    }));
    /* [2026-07-14]: Proto 推理结果 */
    _protoResultSub = _ble.onProtoResult.listen((r) => setState(() {
      _protoActivity = 'Proto: ${r.name} (${(r.confidence*100).toStringAsFixed(0)}%)';
    }));
    _apneaSub = _ble.onSleepApnea.listen((r) => setState(() {
      _lastApnea = r;
      final h = r.monitorMin ~/ 60, m = r.monitorMin % 60;
      final duration = '${h}h ${m.toString().padLeft(2, '0')}m';
      _sleepStatus = r.isAsleep
          ? 'Sleep monitoring: $duration'
          : 'Last monitoring: $duration';
      // The report is cumulative; its lowest SpO2 is historical, not a live reading.
    }));
    /* [2026-07-07]: 0x08 实时睡眠概率 — 更新入睡/觉醒趋势 */
    _sleepStageSub = _ble.onSleepStage.listen((r) => setState(() {
      _sleepProb = r.pSleep;
      if (r.isAsleep && _sleepStatus == 'No data yet') {
        _sleepStatus = 'Falling asleep...';
      }
    }));
    /* [2026-07-07]: 0x09 实时 SpO2 — 比 0x07 lowestSpo2 更即时 */
    _spo2Sub = _ble.onSpo2.listen((r) {
      _spo2ExpiryTimer?.cancel();
      setState(() {
        _spo2Valid = r.valid && r.spo2.isFinite && r.spo2 > 0 && r.spo2 <= 100;
        _spo2Value = _spo2Valid ? r.spo2 : 0;
      });
      if (_spo2Valid) {
        _spo2ExpiryTimer = Timer(const Duration(seconds: 5), () {
          if (mounted) setState(() { _spo2Valid = false; _spo2Value = 0; });
        });
      }
    });
    _logSub = _ble.onLog.listen((m) => setState(() {
      _log.insert(0, '${DateTime.now().toString().substring(11, 19)} $m');
      if (_log.length > 200) _log.removeLast();
    }));
  }

  @override
  void dispose() {
    _spo2ExpiryTimer?.cancel();
    // [FIX P1-10] 显式取消所有订阅
    _harSub?.cancel();
    _hrSub?.cancel();
    _safetySub?.cancel();
    _stepsSub?.cancel();
    _statusSub?.cancel();
    _batterySub?.cancel();
    _connSub?.cancel();
    _protoSub?.cancel();
    _protoBackupSub?.cancel();  /* [2026-07-14] */
    _protoResultSub?.cancel();   /* [2026-07-14] */
    _apneaSub?.cancel();
    _sleepStageSub?.cancel();  /* [2026-07-07] */
    _spo2Sub?.cancel();        /* [2026-07-07] */
    _logSub?.cancel();
    // [FIX P1-9] _ble.dispose() 内部已处理断连 + 订阅清理, 不再需要显式调用 disconnect()
    _ble.dispose();
    super.dispose();
  }

  // ---- 扫描 ----
  Future<void> _scan() async {
    setState(() { _scanning = true; _devices = []; });
    _devices = await _ble.scan();
    // [FIX] 异步操作后检查 mounted, 防止页面已销毁时 setState 抛异常 (#11)
    if (!mounted) return;
    setState(() => _scanning = false);
  }

  Future<void> _connect(ScanResult r) async {
    await _ble.connect(r.device);
  }

  Future<void> _disconnect() async {
    await _ble.disconnect();
  }

  // [2026-07-04]: 原型网络 — session 控制 + 个性化进度
  static const _sessionClasses = {3: 'Walking', 4: 'Jogging', 5: 'Downstairs', 6: 'Cycling'};
  static const _activityNames = ['Lying','Sitting','Standing','Walking','Jogging','Downstairs','Cycling'];

  Future<void> _startSession() async {
    await _ble.sendSessionStart(_sessionClassId);
    setState(() { _sessionActive = true; _sessionClassName = _sessionClasses[_sessionClassId]!; });
  }

  Future<void> _endSession() async {
    await _ble.sendSessionEnd();
    setState(() => _sessionActive = false);
  }

  Future<void> _abortSession() async {
    await _ble.sendSessionAbort();
    setState(() => _sessionActive = false);
  }

  Widget _buildSessionPanel() {
    if (!_connected) {
      return const Card(
        child: Padding(padding: EdgeInsets.all(16), child: Column(children: [
          Icon(Icons.bluetooth_disabled, size: 40, color: Colors.grey),
          SizedBox(height: 8),
          Text('Connect your SmartWatch to start a recording session',
              style: TextStyle(color: Colors.grey, fontSize: 14), textAlign: TextAlign.center),
        ])));
    }
    return Card(
      child: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
        Row(children: [
          const Icon(Icons.fitness_center, size: 18, color: Colors.teal),
          const SizedBox(width: 8),
          const Text('Session', style: TextStyle(color: Colors.teal, fontWeight: FontWeight.bold)),
          const Spacer(),
          if (_sessionActive) Chip(label: Text(_sessionClassName, style: const TextStyle(fontSize: 11)), backgroundColor: Colors.teal.withValues(alpha: 0.2)),
        ]),
        const SizedBox(height: 8),
        if (!_sessionActive)
          Row(children: [
            Expanded(
              child: DropdownButtonFormField<int>(
                initialValue: _sessionClassId, isExpanded: true,
                decoration: const InputDecoration(labelText: 'Activity', isDense: true),
                items: _sessionClasses.entries.map((e) => DropdownMenuItem(value: e.key, child: Text(e.value))).toList(),
                onChanged: (v) { if (v != null) setState(() => _sessionClassId = v); },
              ),
            ),
            const SizedBox(width: 8),
            ElevatedButton.icon(
              icon: const Icon(Icons.play_arrow, size: 18), label: const Text('Start'),
              onPressed: _startSession,
              style: ElevatedButton.styleFrom(backgroundColor: Colors.teal),
            ),
          ])
        else
          Row(mainAxisAlignment: MainAxisAlignment.center, children: [
            const SizedBox(width: 20, height: 20, child: CircularProgressIndicator(strokeWidth: 2, color: Colors.teal)),
            const SizedBox(width: 12), const Text('Recording...', style: TextStyle(color: Colors.teal)),
            const Spacer(),
            TextButton.icon(icon: const Icon(Icons.stop, size: 18, color: Colors.red), label: const Text('End'), onPressed: _endSession),
            TextButton.icon(icon: const Icon(Icons.close, size: 16, color: Colors.grey), label: const Text('Abort', style: TextStyle(fontSize: 11)), onPressed: _abortSession),
          ]),
      ])),
    );
  }

  Widget _buildProfileSettings() {
    return Card(
      child: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
        const Row(children: [Icon(Icons.person, size: 18, color: Colors.orange), SizedBox(width: 8),
          Text('User Profile', style: TextStyle(color: Colors.orange, fontWeight: FontWeight.bold))]),
        const SizedBox(height: 12),
        Row(children: [
          Expanded(child: TextFormField(
            initialValue: _userWeight.toStringAsFixed(1),
            decoration: const InputDecoration(labelText: 'Weight (kg)', isDense: true),
            keyboardType: const TextInputType.numberWithOptions(decimal: true),
            onChanged: (v) { final w = double.tryParse(v); if (w != null && w >= 20 && w <= 250) _userWeight = w; },
          )),
          const SizedBox(width: 12),
          Expanded(child: TextFormField(
            initialValue: '$_userAge',
            decoration: const InputDecoration(labelText: 'Age', isDense: true),
            keyboardType: TextInputType.number,
            onChanged: (v) { final a = int.tryParse(v); if (a != null && a >= 5 && a <= 120) _userAge = a; },
          )),
        ]),
        const SizedBox(height: 8),
        SizedBox(width: double.infinity, child: ElevatedButton.icon(
          icon: const Icon(Icons.send, size: 16),
          label: const Text('Send to Watch'),
          onPressed: _connected ? () async {
            await _ble.sendUserProfile(_userWeight, _userAge);
            if (mounted) {
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(content: Text('Profile sent'), duration: Duration(seconds: 1)));
            }
          } : null,
        )),
      ])),
    );
  }

  Widget _buildProtoProgress() {
    if (_protoVersion == 0) {
      return const Card(child: Padding(padding: EdgeInsets.all(16), child: Column(children: [
        Icon(Icons.self_improvement, size: 40, color: Colors.grey),
        SizedBox(height: 8),
        Text('Personalization data will appear here\nafter recording your first session',
            style: TextStyle(color: Colors.grey, fontSize: 14), textAlign: TextAlign.center),
      ])));
    }
    final maxCount = _protoCounts.reduce((a, b) => a > b ? a : b);
    return Card(
      child: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
        Row(children: [
          const Icon(Icons.self_improvement, size: 18, color: Colors.purple),
          const SizedBox(width: 8),
          Text('Personalization v$_protoVersion', style: const TextStyle(color: Colors.purple, fontWeight: FontWeight.bold, fontSize: 12)),
        ]),
        const SizedBox(height: 8),
        ...List.generate(7, (i) => Row(children: [
          SizedBox(width: 70, child: Text(_activityNames[i], style: const TextStyle(fontSize: 10, color: Colors.grey))),
          Expanded(child: LinearProgressIndicator(
            value: maxCount > 0 ? _protoCounts[i] / maxCount : 0, backgroundColor: Colors.white10,
            color: _protoCounts[i] >= 200 ? Colors.green : Colors.purple)),
          SizedBox(width: 30, child: Text('${_protoCounts[i]}', textAlign: TextAlign.end, style: const TextStyle(fontSize: 10, color: Colors.white54))),
        ])),
      ])),
    );
  }

  /// [2026-07-14]: Proto backup 请求 + 显示手表原型数据
  Widget _buildProtoBackupCard() {
    return Card(
      child: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
        Row(children: [
          const Icon(Icons.cloud_download, size: 18, color: Colors.blue),
          const SizedBox(width: 8),
          const Text('Proto Backup', style: TextStyle(color: Colors.blue, fontWeight: FontWeight.bold, fontSize: 12)),
          const Spacer(),
          ElevatedButton.icon(
            onPressed: _connected && !_backupBusy ? () async {
              setState(() => _backupBusy = true);
              try {
                await _ble.sendProtoRequestBackup();
              } catch (e) {
                if (mounted) {
                  ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Backup failed: $e')));
                }
              } finally {
                if (mounted) setState(() => _backupBusy = false);
              }
            } : null,
            icon: const Icon(Icons.download, size: 16),
            label: Text(_backupBusy ? 'Receiving...' : 'Pull', style: const TextStyle(fontSize: 12)),
          ),
        ]),
        if (_protoBackup != null && _protoBackup!.version > 0) ...[
          const SizedBox(height: 8),
          Text('Received: v${_protoBackup!.version}  ·  ${_protoBackup!.prototypes.length} classes  ·  ${_protoBackup!.prototypes[0].length}D each',
              style: const TextStyle(fontSize: 10, color: Colors.white54)),
        ],
      ])),
    );
  }

  // [2026-07-04]: 用户参数设置对话框 — 体重(kg) + 年龄
  // [2026-07-04]: Profile settings now inline in _buildProfileSettings() tab

  // ---- UI ----
  @override
  Widget build(BuildContext context) {
    return Stack(children: [
      Scaffold(
      appBar: AppBar(
        title: const Text('SmartWatch'),
        actions: [
          if (_batPct > 0)
            Row(mainAxisSize: MainAxisSize.min, children: [
              Icon(_batPct > 20 ? Icons.battery_full : Icons.battery_alert,
                   color: _batPct > 50 ? Colors.green : (_batPct > 20 ? Colors.orange : Colors.red), size: 20),
              Text('$_batPct%', style: const TextStyle(fontSize: 12)),
              const SizedBox(width: 8),
            ]),
          Icon(_connected ? Icons.bluetooth_connected : Icons.bluetooth_disabled,
               color: _connected ? Colors.green : Colors.grey),
          const SizedBox(width: 16),
        ],
      ),
      body: Column(children: [
        _buildConnectBar(),
        const Divider(height: 1),
        Expanded(child: [
          // Tab 0: Dashboard
          ListView(padding: const EdgeInsets.all(12), children: [
            _buildHeartRate(), const SizedBox(height: 12),
            _buildSleepSpo2Card(), const SizedBox(height: 12),
            _buildActivity(), const SizedBox(height: 12),
            _buildSafety(), const SizedBox(height: 12),
            _buildExerciseScore(), const SizedBox(height: 12),
            const Text('Log', style: TextStyle(color: Colors.grey, fontSize: 12)),
            const Divider(),
            ..._log.take(30).map((l) => Text(l, style: const TextStyle(fontSize: 10, fontFamily: 'monospace'))),
          ]),
          // Tab 1: Session
          ListView(padding: const EdgeInsets.all(12), children: [
            _buildSessionPanel(),
            const SizedBox(height: 12),
            if (_connected) _buildProtoProgress(),
          ]),
          // Tab 2: Profile
          ListView(padding: const EdgeInsets.all(12), children: [
            _buildProtoProgress(),
            const SizedBox(height: 12),
            if (_connected) _buildProtoBackupCard(),
            const SizedBox(height: 12),
            _buildProfileSettings(),
            const SizedBox(height: 12),
            _buildEmergencyContactsCard(),
          ]),
        ][_tabIndex]),
      ]),
      bottomNavigationBar: BottomNavigationBar(
        currentIndex: _tabIndex,
        onTap: (i) => setState(() => _tabIndex = i),
        items: [
          const BottomNavigationBarItem(icon: Icon(Icons.dashboard), label: 'Dashboard'),
          BottomNavigationBarItem(icon: Icon(_sessionActive ? Icons.fitness_center : Icons.play_circle_outline), label: 'Session'),
          const BottomNavigationBarItem(icon: Icon(Icons.person), label: 'Profile'),
        ],
      ),
      ),  // end Scaffold

      // [2026-07-07]: Emergency alert overlay — full-screen when triggered
      if (_showEmergencyOverlay)
        EmergencyAlertOverlay(
          type: _currentAlertType,
          onDismiss: () => setState(() => _showEmergencyOverlay = false),
        ),
    ]);  // end Stack
  }

  Widget _buildConnectBar() {
    return Container(
      padding: const EdgeInsets.all(8),
      child: Row(children: [
        Expanded(
          child: ElevatedButton.icon(
            onPressed: _scanning ? null : _scan,
            icon: _scanning
                ? const SizedBox(width: 16, height: 16, child: CircularProgressIndicator(strokeWidth: 2))
                : const Icon(Icons.search),
            label: Text(_scanning ? 'Scanning...' : 'Scan'),
          ),
        ),
        if (_devices.isNotEmpty) ...[
          const SizedBox(width: 8),
          Expanded(
            flex: 2,
            child: DropdownButtonFormField<ScanResult>(
              isExpanded: true,
              hint: const Text('Select device'),
              items: _devices.map((d) => DropdownMenuItem(
                value: d,
                child: Text(d.advertisementData.advName.isNotEmpty ? d.advertisementData.advName : d.device.platformName),
              )).toList(),
              onChanged: _connected ? null : (r) { if (r != null) _connect(r); },
            ),
          ),
        ],
        if (_connected)
          IconButton(icon: const Icon(Icons.link_off), onPressed: _disconnect),
      ]),
    );
  }

  Widget _buildSleepSpo2Card() {
    return Card(
      child: Padding(padding: const EdgeInsets.all(12), child: Row(children: [
        // Sleep summary
        Expanded(flex: 3, child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
          const Text('Sleep', style: TextStyle(color: Colors.indigo, fontSize: 11)),
          const SizedBox(height: 4),
          Text(_sleepStatus, style: const TextStyle(fontSize: 13, fontWeight: FontWeight.bold, color: Colors.white70)),
          /* [2026-07-07]: 0x08 实时入睡概率 — 辅助判断睡眠趋势 */
          if (_sleepProb > 0)
            Text('P(sleep) ${(_sleepProb * 100).toInt()}%',
                style: TextStyle(fontSize: 10, color: _sleepProb > 0.5 ? Colors.indigo : Colors.white38)),
        ])),
        Container(width: 1, height: 30, color: Colors.white12),
        // SpO2 — [2026-07-07] 优先使用 0x09 实时值, fallback 到 0x07 lowestSpo2
        Expanded(flex: 2, child: Column(children: [
          const Text('SpO2', style: TextStyle(color: Colors.teal, fontSize: 11)),
          Text(_spo2Valid ? '${_spo2Value.toInt()}%' : '--', style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold,
              color: _spo2Valid ? (_spo2Value >= 95 ? Colors.green : _spo2Value >= 90 ? Colors.orange : Colors.red) : Colors.grey)),
        ])),
        // Apnea
        if (_lastApnea != null && _lastApnea!.severity > 0) ...[
          Container(width: 1, height: 30, color: Colors.white12),
          Expanded(flex: 2, child: Column(children: [
            const Text('AHI', style: TextStyle(color: Colors.red, fontSize: 11)),
            Text('${_lastApnea!.odi.toInt()}', style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold, color: Colors.red)),
          ])),
        ],
      ])),
    );
  }

  Widget _buildHeartRate() {
    return Card(
      child: Padding(padding: const EdgeInsets.all(16), child: Column(children: [
        const Text('HEART RATE', style: TextStyle(color: Colors.grey, fontSize: 12)),
        Text('${_bpm.toInt()} BPM',
             style: TextStyle(fontSize: 56, fontWeight: FontWeight.bold,
                              color: _hrValid ? const Color(0xFFFF1744) : Colors.orange)),
        Text(_hrValid ? 'Valid signal' : 'No signal',
             style: TextStyle(color: _hrValid ? Colors.green : Colors.orange, fontSize: 12)),
      ])),
    );
  }

  Widget _buildActivity() {
    return Card(
      child: Padding(padding: const EdgeInsets.all(12), child: Row(children: [
        Expanded(child: Column(children: [
          const Text('Activity', style: TextStyle(color: Colors.grey, fontSize: 12)),
          Text(_activity, style: const TextStyle(fontSize: 22, fontWeight: FontWeight.bold)),
          Text('${(_confidence * 100).toInt()}%', style: const TextStyle(color: Colors.cyan)),
          if (_protoActivity.isNotEmpty)
            Text(_protoActivity, style: const TextStyle(color: Colors.purple, fontSize: 11)),
        ])),
        Container(width: 1, height: 50, color: Colors.white12),
        Expanded(child: Column(children: [
          const Text('Steps', style: TextStyle(color: Colors.grey, fontSize: 12)),
          Text('$_steps', style: const TextStyle(fontSize: 22, fontWeight: FontWeight.bold)),
          Text('/ $_goal  |  ${_distKm.toStringAsFixed(2)}km  |  $_kcal kcal',
               style: const TextStyle(color: Colors.orange, fontSize: 10)),
        ])),
      ])),
    );
  }

  Widget _buildSafety() {
    return Card(
      color: _alertColor.withValues(alpha: 0.15),
      child: Padding(padding: const EdgeInsets.all(12), child: Row(children: [
        Icon(Icons.security, color: _alertColor, size: 28),
        const SizedBox(width: 12),
        Text(_alertText, style: TextStyle(color: _alertColor, fontSize: 16, fontWeight: FontWeight.bold)),
      ])),
    );
  }

  static const _metTable = {
    'Lying': 1.0, 'Sitting': 1.2, 'Standing': 1.5,
    'Walking': 3.5, 'Jogging': 7.0, 'Downstairs': 3.8,
    'Cycling': 6.0,
  };

  Color _scoreColor(double v) {
    if (v >= 80) return Colors.green;
    if (v >= 60) return Colors.lightGreen;
    if (v >= 40) return Colors.orange;
    return Colors.red;
  }

  String _scoreLabel(double v) {
    if (v >= 80) return 'Excellent';
    if (v >= 60) return 'Good';
    if (v >= 40) return 'Fair';
    return 'Light';
  }

  String _metLevel(double met) {
    if (met >= 6) return 'Vigorous';
    if (met >= 3) return 'Moderate';
    return 'Light';
  }

  Widget _buildExerciseScore() {
    final met = _metTable[_exAct] ?? 1.5;
    final color = _scoreColor(_score);
    return Card(
      child: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
        const Text('EXERCISE EFFECT', style: TextStyle(color: Colors.grey, fontSize: 12)),
        const SizedBox(height: 8),
        Row(children: [
          // Score gauge (Simple arc built with Stack + ClipOval)
          SizedBox(width: 80, height: 80, child: Stack(alignment: Alignment.center, children: [
            SizedBox(width: 70, height: 70,
              child: CircularProgressIndicator(value: _score / 100.0, strokeWidth: 6,
                  backgroundColor: Colors.white10, color: color)),
            Column(mainAxisSize: MainAxisSize.min, children: [
              Text('${_score.toInt()}', style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold, color: color)),
              Text('pts', style: TextStyle(fontSize: 9, color: color.withValues(alpha: 0.7))),
            ]),
          ])),
          const SizedBox(width: 12),
          // Text stats
          Expanded(child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
            Row(children: [
              Container(padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                  decoration: BoxDecoration(color: color.withValues(alpha: 0.2), borderRadius: BorderRadius.circular(4)),
                  child: Text(_scoreLabel(_score), style: TextStyle(fontSize: 12, color: color, fontWeight: FontWeight.bold))),
              const SizedBox(width: 8),
              Text('MET ${met.toStringAsFixed(1)} ${_metLevel(met)}',
                  style: const TextStyle(fontSize: 10, color: Colors.white54)),
            ]),
            const SizedBox(height: 8),
            Row(children: [
              _statChip('Duration', '${_exMin}m${_exSec.toString().padLeft(2, '0')}s', Colors.green),
              const SizedBox(width: 16),
              _statChip('Calories', '${_calorie.toInt()} kcal', Colors.orange),
            ]),
          ])),
        ]),
      ])),
    );
  }

  Widget _statChip(String label, String value, Color color) {
    return Column(children: [
      Text(value, style: TextStyle(color: color, fontSize: 14, fontWeight: FontWeight.bold)),
      Text(label, style: const TextStyle(color: Colors.grey, fontSize: 9)),
    ]);
  }

  // [2026-07-07]: Emergency contacts card — Profile tab
  Widget _buildEmergencyContactsCard() {
    return Card(
      child: Padding(padding: const EdgeInsets.all(12), child: Column(children: [
        Row(children: [
          const Icon(Icons.contacts, size: 18, color: Colors.redAccent),
          const SizedBox(width: 8),
          const Text('EMERGENCY CONTACTS',
              style: TextStyle(color: Colors.redAccent, fontWeight: FontWeight.bold, fontSize: 12)),
          const Spacer(),
          GestureDetector(
            onTap: () async {
              if (!await EmergencyService.hasPermission()) {
                await EmergencyService.requestPermission();
                await Future.delayed(const Duration(milliseconds: 500));
              }
              if (!mounted) return;
              _addContactDialog(context);
            },
            child: const Icon(Icons.add_circle_outline, size: 20, color: Colors.white38),
          ),
        ]),
        const SizedBox(height: 8),
        if (EmergencyContacts.all.isEmpty)
          const Text('No contacts added. Add contacts who will be notified in an emergency.',
              style: TextStyle(color: Colors.white38, fontSize: 11))
        else
          ...EmergencyContacts.all.asMap().entries.map((e) {
            final i = e.key; final c = e.value;
            return Padding(
              padding: const EdgeInsets.symmetric(vertical: 2),
              child: Row(children: [
                const Icon(Icons.person, size: 14, color: Colors.white54),
                const SizedBox(width: 4),
                Text('${c.relation}: ${c.name}   ${c.phone}',
                    style: const TextStyle(fontSize: 12, color: Colors.white70)),
                const Spacer(),
                GestureDetector(
                  onTap: () => EmergencyContacts.remove(i).then((_) => setState(() {})),
                  child: const Icon(Icons.close, size: 14, color: Colors.white24),
                ),
              ]),
            );
          }),
      ])),
    );
  }

  void _addContactDialog(BuildContext ctx) {
    final nameCtl  = TextEditingController();
    final phoneCtl = TextEditingController();
    final relCtl   = TextEditingController(text: 'Son');
    showDialog(context: ctx, builder: (_) => AlertDialog(
      title: const Text('Add Contact'),
      content: Column(mainAxisSize: MainAxisSize.min, children: [
        TextField(controller: nameCtl,  decoration: const InputDecoration(labelText: 'Name')),
        TextField(controller: phoneCtl, decoration: const InputDecoration(labelText: 'Phone'),
            keyboardType: TextInputType.phone),
        TextField(controller: relCtl,   decoration: const InputDecoration(labelText: 'Relation (e.g. Son)')),
      ]),
      actions: [
        TextButton(onPressed: () => Navigator.pop(_), child: const Text('Cancel')),
        ElevatedButton(onPressed: () {
          final name = nameCtl.text.trim();
          final phone = phoneCtl.text.trim();
          if (name.isNotEmpty && phone.isNotEmpty) {
            EmergencyContacts.add(EmergencyContact(name: name, phone: phone, relation: relCtl.text.trim()));
            setState(() {});
          }
          Navigator.pop(_);
        }, child: const Text('Add')),
      ],
    ));
  }
}
