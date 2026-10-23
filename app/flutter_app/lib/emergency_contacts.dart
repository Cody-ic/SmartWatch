/// Emergency contact data model and local storage
library;

import 'dart:convert';
import 'package:shared_preferences/shared_preferences.dart';

class EmergencyContact {
  String name;
  String phone;
  String relation;

  EmergencyContact(
      {required this.name, required this.phone, this.relation = ''});

  Map<String, dynamic> toJson() =>
      {'name': name, 'phone': phone, 'relation': relation};
  factory EmergencyContact.fromJson(Map<String, dynamic> j) => EmergencyContact(
      name: j['name'] ?? '',
      phone: j['phone'] ?? '',
      relation: j['relation'] ?? '');
}

class EmergencyContacts {
  static const _key = 'emergency_contacts';
  static const _migrationKey = 'emergency_contacts_no_demo_v1';
  static List<EmergencyContact> _cache = [];

  static List<EmergencyContact> get all => List.unmodifiable(_cache);

  static Future<void> load() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString(_key);
    _cache = raw == null || raw.isEmpty
        ? []
        : (jsonDecode(raw) as List)
            .map((e) => EmergencyContact.fromJson(e))
            .toList();
    if (prefs.getBool(_migrationKey) != true) {
      final before = _cache.length;
      _cache.removeWhere((c) =>
          (c.name == 'Son Zhao Ming' &&
              c.phone == '13800138001' &&
              c.relation == 'Son') ||
          (c.name == 'Daughter Zhao Li' &&
              c.phone == '13900139002' &&
              c.relation == 'Daughter'));
      if (_cache.length != before && raw != null) {
        // Preserve the original data for recovery, but never send to demo records.
        await prefs.setString('emergency_contacts_before_demo_migration', raw);
      }
      await _save();
      await prefs.setBool(_migrationKey, true);
    }
  }

  static Future<void> _save() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(
        _key, jsonEncode(_cache.map((c) => c.toJson()).toList()));
  }

  static Future<void> add(EmergencyContact c) async {
    _cache.add(c);
    await _save();
  }

  static Future<void> remove(int index) async {
    if (index >= 0 && index < _cache.length) {
      _cache.removeAt(index);
      await _save();
    }
  }

  static Future<void> update(int index, EmergencyContact c) async {
    if (index >= 0 && index < _cache.length) {
      _cache[index] = c;
      await _save();
    }
  }
}
