import '../../l10n/app_localizations.dart';

/// Parita `BundledPuzzleCatalog.swift`.
class BundledPuzzleItem {
  const BundledPuzzleItem({
    required this.id,
    required this.title,
    required this.subtitle,
    required this.fen,
    this.themes = const [],
    this.solution = const [],
  });

  final String id;
  final String title;
  final String subtitle;
  final String fen;
  final List<String> themes;
  /// UCI tahy referenční lině (prázdné = jen sandbox bez auto-kontroly).
  final List<String> solution;
}

const bundledPuzzleCatalog = <BundledPuzzleItem>[
  BundledPuzzleItem(
    id: 'mate_qg8',
    title: 'Mat dámou',
    subtitle: 'Bílý · mat v 1 tahu',
    fen: '7k/5K2/6Q1/8/8/8/8/8 w - - 0 1',
    themes: ['mateIn1'],
    solution: ['g6g8'],
  ),
  BundledPuzzleItem(
    id: 'mate_back_rank',
    title: 'Zadní řada',
    subtitle: 'Bílý · mat věží',
    fen: '6k1/5ppp/8/8/8/8/5PPP/R3K3 w Q - 0 1',
    themes: ['mateIn1', 'endgame'],
    solution: ['a1a8'],
  ),
  BundledPuzzleItem(
    id: 'mate_rook_corner',
    title: 'Věž proti králi',
    subtitle: 'Bílý · koncovka',
    fen: '7k/R7/6K1/8/8/8/8/8 w - - 0 1',
    themes: ['mateIn1', 'endgame'],
    solution: ['a7a8'],
  ),
  BundledPuzzleItem(
    id: 'black_mate_1',
    title: 'Černý na tahu',
    subtitle: 'Černý · mat v 1 tahu',
    fen: '8/8/8/8/8/3q2k1/8/6K1 b - - 0 1',
    themes: ['mateIn1'],
    solution: ['d3d1'],
  ),
  BundledPuzzleItem(
    id: 'promotion_hint',
    title: 'Postup pěšce',
    subtitle: 'Bílý · proměna',
    fen: '8/P7/8/1k6/8/8/8/4K3 w - - 0 1',
    themes: ['advancedPawn', 'endgame'],
  ),
  BundledPuzzleItem(
    id: 'two_rooks',
    title: 'Dvě věže',
    subtitle: 'Bílý · dvě věže',
    fen: '3k4/8/8/8/8/8/5R2/4RK2 w - - 0 1',
    themes: ['endgame'],
  ),
];

extension BundledPuzzleItemL10n on BundledPuzzleItem {
  String localizedTitle(AppLocalizations l) {
    switch (id) {
      case 'mate_qg8':
        return l.bundledPuzzleMateQg8Title;
      case 'mate_back_rank':
        return l.bundledPuzzleBackRankTitle;
      case 'mate_rook_corner':
        return l.bundledPuzzleRookCornerTitle;
      case 'black_mate_1':
        return l.bundledPuzzleBlackMateTitle;
      case 'promotion_hint':
        return l.bundledPuzzlePromotionTitle;
      case 'two_rooks':
        return l.bundledPuzzleTwoRooksTitle;
      default:
        return title;
    }
  }

  String localizedSubtitle(AppLocalizations l) {
    switch (id) {
      case 'mate_qg8':
        return l.bundledPuzzleMateQg8Subtitle;
      case 'mate_back_rank':
        return l.bundledPuzzleBackRankSubtitle;
      case 'mate_rook_corner':
        return l.bundledPuzzleRookCornerSubtitle;
      case 'black_mate_1':
        return l.bundledPuzzleBlackMateSubtitle;
      case 'promotion_hint':
        return l.bundledPuzzlePromotionSubtitle;
      case 'two_rooks':
        return l.bundledPuzzleTwoRooksSubtitle;
      default:
        return subtitle;
    }
  }
}
