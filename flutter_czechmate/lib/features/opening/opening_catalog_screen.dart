import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/layout/form_factor.dart';
import 'opening_catalog_repository.dart';
import 'opening_curriculum_unlock.dart';
import 'opening_progress_repository.dart';
import 'opening_trainer_screen.dart';

class OpeningCatalogScreen extends ConsumerStatefulWidget {
  const OpeningCatalogScreen({super.key});

  @override
  ConsumerState<OpeningCatalogScreen> createState() =>
      _OpeningCatalogScreenState();
}

class _OpeningCatalogScreenState extends ConsumerState<OpeningCatalogScreen> {
  late Future<_CatalogBundle> _bundle;

  @override
  void initState() {
    super.initState();
    _bundle = _load();
  }

  Future<_CatalogBundle> _load() async {
    final catalog = OpeningCatalogRepository();
    final curricula = await catalog.loadCurricula();
    final lines = await catalog.loadAll();
    final progress = ref.read(openingProgressRepositoryProvider).loadAll();
    return _CatalogBundle(curricula: curricula, lines: lines, progress: progress);
  }

  void _refresh() {
    setState(() {
      _bundle = _load();
    });
  }

  Future<void> _pickMode(OpeningLine line) async {
    final progressRepo = ref.read(openingProgressRepositoryProvider);
    final progress = progressRepo.progressFor(line.id);
    final mirrorId = line.mirrorLineId;
    final mirrorOk = progressRepo.mirrorUnlocked(line.id, mirrorLineId: mirrorId);

    final mode = await showModalBottomSheet<String>(
      context: context,
      showDragHandle: true,
      builder: (ctx) {
        return SafeArea(
          child: Padding(
            padding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Text(line.nameCs, style: Theme.of(ctx).textTheme.titleMedium),
                Text('ECO ${line.eco} · ${line.side} · ★${progress.stars}/4'),
                const SizedBox(height: 12),
                ListTile(
                  leading: const Icon(Icons.school_outlined),
                  title: const Text('Učení'),
                  subtitle: const Text('Komentáře ke každému tahu'),
                  onTap: () => Navigator.pop(ctx, 'learn'),
                ),
                ListTile(
                  leading: const Icon(Icons.fitness_center_outlined),
                  title: const Text('Drill'),
                  subtitle: Text(
                    progress.stars >= 1
                        ? 'Bez komentářů, max 2 chyby pro ★★'
                        : 'Nejdřív dokonči režim Učení',
                  ),
                  enabled: progress.stars >= 1,
                  onTap: progress.stars >= 1
                      ? () => Navigator.pop(ctx, 'drill')
                      : null,
                ),
                ListTile(
                  leading: const Icon(Icons.timer_outlined),
                  title: const Text('Na čas'),
                  subtitle: Text(
                    progress.stars >= 2
                        ? '90 s na celou linii'
                        : 'Odemčeno po ★★ v Drillu',
                  ),
                  enabled: progress.stars >= 2,
                  onTap: progress.stars >= 2
                      ? () => Navigator.pop(ctx, 'timed')
                      : null,
                ),
                if (mirrorId != null)
                  ListTile(
                    leading: const Icon(Icons.swap_horiz),
                    title: const Text('Mirror — protistrana'),
                    subtitle: Text(
                      mirrorOk
                          ? 'Párová linie $mirrorId'
                          : 'Odemčeno po ★★ na hlavní linii',
                    ),
                    enabled: mirrorOk,
                    onTap: mirrorOk
                        ? () => Navigator.pop(ctx, 'mirror')
                        : null,
                  ),
              ],
            ),
          ),
        );
      },
    );

    if (!mounted || mode == null) return;

    String lineId = line.id;
    if (mode == 'mirror' && mirrorId != null) {
      lineId = mirrorId;
    }

    await Navigator.of(context).push<void>(
      MaterialPageRoute<void>(
        builder: (_) => OpeningTrainerScreen(
          lineId: lineId,
          mode: mode,
        ),
      ),
    );
    _refresh();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Trénink zahájení')),
      body: desktopFormDetailBody(
        FutureBuilder<_CatalogBundle>(
          future: _bundle,
          builder: (context, snap) {
            if (!snap.hasData) {
              return const Center(child: CircularProgressIndicator());
            }
            final data = snap.data!;
            final byId = {for (final l in data.lines) l.id: l};
            final dueIds = OpeningCurriculumUnlock.linesDueForReview(
              data.lines,
              data.progress,
            );
            final dueLines = dueIds
                .map((id) => byId[id])
                .whereType<OpeningLine>()
                .toList();
            return ListView(
              padding: const EdgeInsets.all(16),
              children: [
                if (dueLines.isNotEmpty)
                  Card(
                    color: Theme.of(context).colorScheme.primaryContainer,
                    margin: const EdgeInsets.only(bottom: 16),
                    child: Padding(
                      padding: const EdgeInsets.all(12),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Row(
                            children: [
                              Icon(Icons.replay,
                                  color: Theme.of(context)
                                      .colorScheme
                                      .onPrimaryContainer),
                              const SizedBox(width: 8),
                              Text(
                                'Čas na opakování',
                                style: Theme.of(context)
                                    .textTheme
                                    .titleSmall
                                    ?.copyWith(
                                      color: Theme.of(context)
                                          .colorScheme
                                          .onPrimaryContainer,
                                    ),
                              ),
                            ],
                          ),
                          const SizedBox(height: 8),
                          ...dueLines.take(3).map(
                                (line) => ListTile(
                                  dense: true,
                                  contentPadding: EdgeInsets.zero,
                                  title: Text(line.nameCs),
                                  trailing: const Icon(Icons.chevron_right),
                                  onTap: () => _pickMode(line),
                                ),
                              ),
                        ],
                      ),
                    ),
                  ),
                for (final curriculum in data.curricula) ...[
                  Builder(builder: (context) {
                    final unlocked = OpeningCurriculumUnlock.isUnlocked(
                      curriculum,
                      allCurricula: data.curricula,
                      progress: data.progress,
                    );
                    return Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Row(
                          children: [
                            if (!unlocked)
                              const Padding(
                                padding: EdgeInsets.only(right: 6),
                                child: Icon(Icons.lock, size: 18),
                              ),
                            Expanded(
                              child: Text(
                                curriculum.nameCs,
                                style: Theme.of(context).textTheme.titleMedium,
                              ),
                            ),
                          ],
                        ),
                        if (!unlocked) ...[
                          const SizedBox(height: 4),
                          Text(
                            OpeningCurriculumUnlock.lockReasonCs(
                              curriculum,
                              allCurricula: data.curricula,
                              progress: data.progress,
                            ),
                            style: Theme.of(context).textTheme.bodySmall?.copyWith(
                                  color: Theme.of(context).colorScheme.outline,
                                ),
                          ),
                        ],
                        const SizedBox(height: 8),
                        if (unlocked)
                          ...curriculum.lineIds.map((id) {
                            final line = byId[id];
                            if (line == null) return const SizedBox.shrink();
                            final stars = data.progress[id]?.stars ?? 0;
                            return Card(
                              margin: const EdgeInsets.only(bottom: 8),
                              child: ListTile(
                                title: Text(line.nameCs),
                                subtitle: Text(
                                  'ECO ${line.eco} · ${line.side} · obtížnost ${line.difficulty}',
                                  maxLines: 2,
                                  overflow: TextOverflow.ellipsis,
                                ),
                                trailing: Row(
                                  mainAxisSize: MainAxisSize.min,
                                  children: [
                                    Text('★$stars'),
                                    const Icon(Icons.chevron_right),
                                  ],
                                ),
                                onTap: () => _pickMode(line),
                              ),
                            );
                          })
                        else
                          Card(
                            margin: const EdgeInsets.only(bottom: 8),
                            child: ListTile(
                              enabled: false,
                              title: Text(
                                '${curriculum.lineIds.length} linií uzamčeno',
                                style: TextStyle(
                                  color: Theme.of(context).disabledColor,
                                ),
                              ),
                            ),
                          ),
                        const SizedBox(height: 16),
                      ],
                    );
                  }),
                ],
              ],
            );
          },
        ),
      ),
    );
  }
}

class _CatalogBundle {
  const _CatalogBundle({
    required this.curricula,
    required this.lines,
    required this.progress,
  });

  final List<OpeningCurriculum> curricula;
  final List<OpeningLine> lines;
  final Map<String, OpeningLineProgress> progress;
}
