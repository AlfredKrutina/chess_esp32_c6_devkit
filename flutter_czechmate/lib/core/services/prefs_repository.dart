import 'package:shared_preferences/shared_preferences.dart';

class PrefsRepository {
  PrefsRepository(this._p);

  final SharedPreferences _p;

  static const keyBaseUrl = 'czechmate.boardBaseURL';
  static const keyMock = 'czechmate.useMockBoard';
  static const keyCoachKey = 'czechmate.coachApiKey';

  String? get lastBoardBaseUrl => _p.getString(keyBaseUrl);
  Future<void> setLastBoardBaseUrl(String? v) async {
    if (v == null || v.isEmpty) {
      await _p.remove(keyBaseUrl);
    } else {
      await _p.setString(keyBaseUrl, v);
    }
  }

  bool get useMockBoard => _p.getBool(keyMock) ?? false;
  Future<void> setUseMockBoard(bool v) => _p.setBool(keyMock, v);

  String? get coachApiKey => _p.getString(keyCoachKey);
  Future<void> setCoachApiKey(String? v) async {
    if (v == null || v.isEmpty) {
      await _p.remove(keyCoachKey);
    } else {
      await _p.setString(keyCoachKey, v);
    }
  }
}
