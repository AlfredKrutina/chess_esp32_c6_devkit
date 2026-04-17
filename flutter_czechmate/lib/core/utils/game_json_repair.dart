/// Parita s `GameJSONRepair.swift` — oprava starého firmware JSON.
class GameJsonRepair {
  static String repairStatusString(String s) {
    const broken =
        '"error_state":{"active":false,"restore_state"';
    const fixed = '"error_state":{"active":false},"restore_state"';
    if (s.contains(broken)) {
      return s.replaceAll(broken, fixed);
    }
    return s;
  }

  static List<int> repairStatusBytes(List<int> data) {
    final s = String.fromCharCodes(data);
    final r = repairStatusString(s);
    if (identical(r, s)) return data;
    return r.codeUnits;
  }
}
