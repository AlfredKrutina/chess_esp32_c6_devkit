import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../../core/layout/form_factor.dart';
import 'opening_catalog_repository.dart';
import 'opening_curriculum_unlock.dart';
import 'opening_mode_picker.dart';
import 'opening_progress_repository.dart';

class OpeningCatalogScreen extends ConsumerStatefulWidget {
  const OpeningCatalogScreen({super.key});

  @override
  ConsumerState<OpeningCatalogScreen> createState() =>
      _OpeningCatalogScreenState();
}

class _OpeningCatalogScreenState extends ConsumerState<OpeningCatalogScreen> {
  late Future<_CatalogBundle> _bundle;
  String _searchQuery = '';
  String? _sideFilter;

  @override
  void initState() {
    super.initState();
    _bundle = _load();
  }

  bool get _filterActive =>
      _searchQuery.trim().isNotEmpty || _sideFilter != null;

  List<OpeningLine> _filterLines(List<OpeningLine> lines) {
    final q = _searchQuery.trim().toLowerCase();
    return lines.where((line) {
      if (_sideFilter != null && line.side != _sideFilter) return false;
      if (q.isEmpty) return true;
      return line.id.toLowerCase().contains(q) ||
          line.eco.toLowerCase().contains(q) ||
          line.nameCs.toLowerCase().contains(q) ||
          line.nameEn.toLowerCase().contains(q) ||
          (line.rationale?.summaryForLocale('cs') ?? '')
              .toLowerCase()
              .contains(q);
    }).toList();
  }

  Future<void> _pickMode(OpeningLine line, {List<OpeningLine>? allLines}) {
    return pickOpeningModeAndStart(
      context: context,
      ref: ref,
      line: line,
      allLines: allLines,
      onReturn: _refresh,
    );
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

  Widget _buildFilters(BuildContext context) {
    final cs = Localizations.localeOf(context).languageCode.startsWith('cs');
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        TextField(
          decoration: InputDecoration(
            hintText: cs ? 'Hledat název, ECO…' : 'Search name, ECO…',
            prefixIcon: const Icon(Icons.search),
            border: const OutlineInputBorder(),
            isDense: true,
          ),
          onChanged: (v) => setState(() => _searchQuery = v),
        ),
        const SizedBox(height: 8),
        Wrap(
          spacing: 8,
          children: [
            FilterChip(
              label: Text(cs ? 'Vše' : 'All'),
              selected: _sideFilter == null,
              onSelected: (_) => setState(() => _sideFilter = null),
            ),
            FilterChip(
              label: Text(cs ? 'Bílé' : 'White'),
              selected: _sideFilter == 'white',
              onSelected: (_) => setState(() => _sideFilter = 'white'),
            ),
            FilterChip(
              label: Text(cs ? 'Černé' : 'Black'),
              selected: _sideFilter == 'black',
              onSelected: (_) => setState(() => _sideFilter = 'black'),
            ),
          ],
        ),
        const SizedBox(height: 16),
      ],
    );
  }

  Widget _buildLineTile(
    BuildContext context,
    OpeningLine line,
    int stars,
    List<OpeningLine> allLines,
  ) {
    final locale = Localizations.localeOf(context).languageCode;
    final subtitle = line.subtitleForLocale(locale);
    final cs = locale.startsWith('cs');
    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: ListTile(
        title: Text(line.nameForLocale(locale)),
        subtitle: Text(
          subtitle != null && subtitle.isNotEmpty
              ? '$subtitle\nECO ${line.eco} · ${line.side} · ${cs ? 'obtížnost' : 'difficulty'} ${line.difficulty}'
              : 'ECO ${line.eco} · ${line.side} · ${cs ? 'obtížnost' : 'difficulty'} ${line.difficulty}',
          maxLines: 3,
          overflow: TextOverflow.ellipsis,
        ),
        trailing: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Text('★$stars'),
            const Icon(Icons.chevron_right),
          ],
        ),
        onTap: () => _pickMode(line, allLines: allLines),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final cs = Localizations.localeOf(context).languageCode.startsWith('cs');
    return Scaffold(
      appBar: AppBar(
        title: Text(cs ? 'Trénink zahájení' : 'Opening training'),
      ),
      body: desktopFormDetailBody(
        FutureBuilder<_CatalogBundle>(
          future: _bundle,
          builder: (context, snap) {
            if (!snap.hasData) {
              return const Center(child: CircularProgressIndicator());
            }
            final data = snap.data!;
            final byId = {for (final l in data.lines) l.id: l};
            final filtered = _filterLines(data.lines);
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
                _buildFilters(context),
                if (_filterActive) ...[
                  Text(
                    cs
                        ? 'Nalezeno ${filtered.length} linií'
                        : '${filtered.length} lines found',
                    style: Theme.of(context).textTheme.titleSmall,
                  ),
                  const SizedBox(height: 8),
                  ...filtered.map(
                    (line) => _buildLineTile(
                      context,
                      line,
                      data.progress[line.id]?.stars ?? 0,
                      data.lines,
                    ),
                  ),
                ] else ...[
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
                                  onTap: () => _pickMode(line, allLines: data.lines),
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
                            return _buildLineTile(
                              context,
                              line,
                              stars,
                              data.lines,
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
