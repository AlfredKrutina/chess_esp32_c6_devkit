import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import 'opening_catalog_repository.dart';
import 'opening_rationale.dart';
import 'opening_trainer_screen.dart';

/// Sdílený výběr režimu a soupeře před startem lekce (katalog + Learn L10/L12).
Future<void> pickOpeningModeAndStart({
  required BuildContext context,
  required WidgetRef ref,
  required OpeningLine line,
  List<OpeningLine>? allLines,
  VoidCallback? onReturn,
}) async {
  final progressRepo = ref.read(openingProgressRepositoryProvider);
  final progress = progressRepo.progressFor(line.id);
  final mirrorId = line.mirrorLineId;
  final mirrorOk = progressRepo.mirrorUnlocked(line.id, mirrorLineId: mirrorId);
  var opponentMode = 'physical';
  final locale = Localizations.localeOf(context).languageCode;
  final linesById = {
    for (final l in allLines ?? const <OpeningLine>[]) l.id: l,
  };
  final cs = locale.startsWith('cs');

  final mode = await showModalBottomSheet<String>(
    context: context,
    showDragHandle: true,
    isScrollControlled: true,
    builder: (ctx) {
      return StatefulBuilder(
        builder: (ctx, setSheetState) {
          return SafeArea(
            child: Padding(
              padding: EdgeInsets.fromLTRB(
                16,
                0,
                16,
                16 + MediaQuery.viewInsetsOf(ctx).bottom,
              ),
              child: SingleChildScrollView(
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Text(
                      line.nameForLocale(locale),
                      style: Theme.of(ctx).textTheme.titleMedium,
                    ),
                    Text('ECO ${line.eco} · ${line.side} · ★${progress.stars}/4'),
                    if (line.rationale != null) ...[
                      const SizedBox(height: 12),
                      OpeningRationalePanel(
                        rationale: line.rationale!,
                        locale: locale,
                        linesById: linesById,
                        onRelatedTap: (relatedId) {
                          final related = linesById[relatedId];
                          if (related == null) return;
                          Navigator.pop(ctx);
                          pickOpeningModeAndStart(
                            context: context,
                            ref: ref,
                            line: related,
                            allLines: allLines,
                            onReturn: onReturn,
                          );
                        },
                      ),
                    ],
                    const SizedBox(height: 12),
                    Text(
                      cs ? 'Soupeř' : 'Opponent',
                      style: Theme.of(ctx).textTheme.titleSmall,
                    ),
                    const SizedBox(height: 4),
                    SegmentedButton<String>(
                      segments: [
                        ButtonSegment(
                          value: 'physical',
                          label: Text(cs ? 'Fyzicky' : 'Physical'),
                          icon: const Icon(Icons.pan_tool_alt_outlined),
                        ),
                        ButtonSegment(
                          value: 'virtual',
                          label: Text(cs ? 'Virtuálně' : 'Virtual'),
                          icon: const Icon(Icons.smart_toy_outlined),
                        ),
                      ],
                      selected: {opponentMode},
                      onSelectionChanged: (s) {
                        setSheetState(() => opponentMode = s.first);
                      },
                    ),
                    Padding(
                      padding: const EdgeInsets.only(top: 4, bottom: 12),
                      child: Text(
                        opponentMode == 'physical'
                            ? (cs
                                ? 'LED ukáže tah soupeře — figurku přesuneš sám (jako proti botovi).'
                                : 'LED shows opponent moves — you move the piece (like vs bot).')
                            : (cs
                                ? 'Soupeř táhne na logické desce — po tahu může být potřeba srovnat fyzickou desku.'
                                : 'Opponent moves on the logical board — resync may be needed.'),
                        style: Theme.of(ctx).textTheme.bodySmall,
                      ),
                    ),
                    ListTile(
                      leading: const Icon(Icons.school_outlined),
                      title: Text(cs ? 'Učení' : 'Learn'),
                      subtitle: Text(cs
                          ? 'Komentáře ke každému tahu'
                          : 'Comments for each move'),
                      onTap: () => Navigator.pop(ctx, 'learn'),
                    ),
                    ListTile(
                      leading: const Icon(Icons.fitness_center_outlined),
                      title: const Text('Drill'),
                      subtitle: Text(
                        progress.stars >= 1
                            ? (cs
                                ? 'Bez komentářů, max 2 chyby pro ★★'
                                : 'No comments, ≤2 errors for ★★')
                            : (cs
                                ? 'Nejdřív dokonči režim Učení'
                                : 'Complete Learn first'),
                      ),
                      enabled: progress.stars >= 1,
                      onTap: progress.stars >= 1
                          ? () => Navigator.pop(ctx, 'drill')
                          : null,
                    ),
                    ListTile(
                      leading: const Icon(Icons.timer_outlined),
                      title: Text(cs ? 'Na čas' : 'Timed'),
                      subtitle: Text(
                        progress.stars >= 2
                            ? (cs ? '90 s na celou linii' : '90 s for the full line')
                            : (cs
                                ? 'Odemčeno po ★★ v Drillu'
                                : 'Unlock after ★★ in Drill'),
                      ),
                      enabled: progress.stars >= 2,
                      onTap: progress.stars >= 2
                          ? () => Navigator.pop(ctx, 'timed')
                          : null,
                    ),
                    if (mirrorId != null)
                      ListTile(
                        leading: const Icon(Icons.swap_horiz),
                        title: Text(cs ? 'Mirror — protistrana' : 'Mirror — other side'),
                        subtitle: Text(
                          mirrorOk
                              ? (cs
                                  ? 'Párová linie $mirrorId'
                                  : 'Paired line $mirrorId')
                              : (cs
                                  ? 'Odemčeno po ★★ na hlavní linii'
                                  : 'Unlock after ★★ on main line'),
                        ),
                        enabled: mirrorOk,
                        onTap: mirrorOk
                            ? () => Navigator.pop(ctx, 'mirror')
                            : null,
                      ),
                  ],
                ),
              ),
            ),
          );
        },
      );
    },
  );

  if (!context.mounted || mode == null) return;

  var lineId = line.id;
  if (mode == 'mirror' && mirrorId != null) {
    lineId = mirrorId;
  }

  await Navigator.of(context).push<void>(
    MaterialPageRoute<void>(
      builder: (_) => OpeningTrainerScreen(
        lineId: lineId,
        mode: mode,
        opponentMode: opponentMode,
      ),
    ),
  );
  onReturn?.call();
}

Future<void> launchOpeningLessonById({
  required BuildContext context,
  required WidgetRef ref,
  required String lineId,
  VoidCallback? onReturn,
}) async {
  final catalog = OpeningCatalogRepository();
  final line = await catalog.findById(lineId);
  if (line == null) return;
  final allLines = await catalog.loadAll();
  if (!context.mounted) return;
  await pickOpeningModeAndStart(
    context: context,
    ref: ref,
    line: line,
    allLines: allLines,
    onReturn: onReturn,
  );
}
