/// Human-readable opening trainer messages (parita s web `opening_trainer.js` §8.9).
class OpeningFeedbackL10n {
  const OpeningFeedbackL10n._();

  static bool _cs(String locale) => locale.startsWith('cs');

  static String moveLabel({
    required String locale,
    required int playerPlyIndex,
    required int playerPlyTotal,
    required String from,
    required String to,
    required String feedback,
    required bool drillLike,
    required bool isSetupPhase,
    required bool isCheckpoint,
    required bool physicalSynced,
    required int mismatchCount,
    required bool physicalMatch,
    required String opponentMode,
  }) {
    if (isSetupPhase) {
      if (physicalMatch == false) {
        return _cs(locale)
            ? 'Deska nesedí se startem — použij průvodce rozestavením.'
            : 'Board does not match the start — use the setup wizard.';
      }
      return _cs(locale)
          ? 'Kontroluji fyzickou desku…'
          : 'Checking the physical board…';
    }

    if (isCheckpoint) {
      if (physicalSynced) {
        return _cs(locale)
            ? 'Deska sedí — potvrď a pokračuj.'
            : 'Board matches — confirm and continue.';
      }
      if (mismatchCount == 0) {
        return _cs(locale)
            ? 'Srovnej fyzickou desku s logickou pozicí…'
            : 'Sync the physical board to the logical position…';
      }
      return _cs(locale)
          ? 'Nejdřív srovnej desku ($mismatchCount rozdílů).'
          : 'Sync the board first ($mismatchCount differences).';
    }

    switch (feedback) {
      case 'complete':
        return _cs(locale) ? 'Linie dokončena.' : 'Line complete.';
      case 'mistake_hint':
        return _cs(locale)
            ? 'Po 3 chybách — správný tah: $from → $to'
            : 'After 3 errors — correct move: $from → $to';
      case 'opponent_turn':
        return _cs(locale)
            ? 'Tah soupeře — zvedni z $from a polož na $to'
            : 'Opponent move — pick up from $from and place on $to';
      case 'wrong':
        return drillLike
            ? (_cs(locale) ? 'Chyba' : 'Wrong')
            : (_cs(locale)
                ? 'To není plánovaný tah. Zkus znovu.'
                : 'Not the planned move. Try again.');
      case 'illegal':
        return drillLike
            ? (_cs(locale) ? 'Chyba' : 'Wrong')
            : (_cs(locale) ? 'Tah není legální.' : 'Illegal move.');
      case 'checkpoint':
        return _cs(locale)
            ? 'Srovnej desku s logickou pozicí.'
            : 'Sync the board to the logical position.';
      default:
        if (drillLike) {
          return _cs(locale)
              ? 'Tah ${playerPlyIndex + 1}/$playerPlyTotal'
              : 'Move ${playerPlyIndex + 1}/$playerPlyTotal';
        }
        return _cs(locale)
            ? 'Táhni na desce: $from → $to'
            : 'Play on the board: $from → $to';
    }
  }

  static String? opponentModeHint(String locale, String opponentMode) {
    if (opponentMode == 'physical') {
      return _cs(locale)
          ? 'Fyzický režim — LED ukáže tah soupeře, figurku přesuneš sám.'
          : 'Physical mode — LED shows the opponent move; you move the piece.';
    }
    if (opponentMode == 'virtual') {
      return _cs(locale)
          ? 'Virtuální režim — po tahu soupeře může být potřeba srovnat desku.'
          : 'Virtual mode — you may need to resync the board after opponent moves.';
    }
    return null;
  }

  static String modeLabel(String locale, String mode) {
    switch (mode) {
      case 'drill':
        return _cs(locale) ? 'Drill' : 'Drill';
      case 'timed':
        return _cs(locale) ? 'Na čas' : 'Timed';
      case 'mirror':
        return _cs(locale) ? 'Mirror' : 'Mirror';
      default:
        return _cs(locale) ? 'Učení' : 'Learn';
    }
  }
}
