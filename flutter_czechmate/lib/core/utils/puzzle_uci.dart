/// UCI tah z sandbox výběru (malá písmena, promo jako jedno písmeno q/r/b/n).
String sandboxMoveToUci(String from, String to, [String? promotionLower]) {
  final f = from.toLowerCase().trim();
  final t = to.toLowerCase().trim();
  if (promotionLower != null && promotionLower.isNotEmpty) {
    return '$f$t${promotionLower.substring(0, 1).toLowerCase()}';
  }
  return '$f$t';
}

bool puzzleUciListsMatch(List<String> expected, List<String> played) {
  if (expected.length != played.length) return false;
  for (var i = 0; i < expected.length; i++) {
    if (expected[i].toLowerCase().trim() != played[i].toLowerCase().trim()) {
      return false;
    }
  }
  return true;
}
