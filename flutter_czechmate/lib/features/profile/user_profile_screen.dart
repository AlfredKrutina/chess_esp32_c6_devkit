import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:image_picker/image_picker.dart';
import 'package:path_provider/path_provider.dart';

import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/services/prefs_repository.dart';

/// Player profile — display name, avatar (preset icons or gallery photo), puzzle Elo, activity.
class UserProfileScreen extends ConsumerStatefulWidget {
  const UserProfileScreen({super.key});

  @override
  ConsumerState<UserProfileScreen> createState() => _UserProfileScreenState();
}

class _UserProfileScreenState extends ConsumerState<UserProfileScreen> {
  late final TextEditingController _nameCtrl;

  static const _defaultIcons = <IconData>[
    Icons.face_rounded,
    Icons.pets_rounded,
    Icons.sports_esports_rounded,
    Icons.military_tech_rounded,
    Icons.emoji_events_rounded,
  ];

  @override
  void initState() {
    super.initState();
    final p = ref.read(prefsRepositoryProvider);
    _nameCtrl = TextEditingController(text: p.profileDisplayName);
  }

  @override
  void dispose() {
    _nameCtrl.dispose();
    super.dispose();
  }

  Future<void> _pickGallery(PrefsRepository prefs) async {
    final picker = ImagePicker();
    try {
      final x = await picker.pickImage(
        source: ImageSource.gallery,
        maxWidth: 1024,
        imageQuality: 85,
      );
      if (x == null) return;
      final dir = await getApplicationDocumentsDirectory();
      final destName =
          'avatar_${DateTime.now().millisecondsSinceEpoch}.jpg';
      final sep = Platform.pathSeparator;
      final destPath = '${dir.path}$sep$destName';
      await File(x.path).copy(destPath);
      await prefs.setProfileAvatarSpec('file:$destPath');
      ref.invalidate(prefsRepositoryProvider);
      if (mounted) setState(() {});
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(context.l10n.profileGalleryError('$e'))),
        );
      }
    }
  }

  Future<void> _setDefaultAvatar(PrefsRepository prefs, int index) async {
    await prefs.setProfileAvatarSpec('default:$index');
    ref.invalidate(prefsRepositoryProvider);
    setState(() {});
  }

  Widget _avatarPreview(PrefsRepository prefs, double radius) {
    final spec = prefs.profileAvatarSpec;
    if (spec.startsWith('file:')) {
      final path = spec.substring(5);
      final f = File(path);
      if (f.existsSync()) {
        return CircleAvatar(
          radius: radius,
          backgroundImage: FileImage(f),
        );
      }
    }
    final raw = spec.startsWith('default:') ? spec.substring(8) : '0';
    final idx = int.tryParse(raw)?.clamp(0, _defaultIcons.length - 1) ?? 0;
    return CircleAvatar(
      radius: radius,
      backgroundColor: Theme.of(context).colorScheme.primaryContainer,
      child: Icon(
        _defaultIcons[idx],
        size: radius * 1.15,
        color: Theme.of(context).colorScheme.onPrimaryContainer,
      ),
    );
  }

  Widget _heatmap(BuildContext context, PrefsRepository prefs) {
    final map = prefs.puzzleActivityByDay();
    final now = DateTime.now();
    final cs = Theme.of(context).colorScheme;
    final rows = <Widget>[];
    for (var week = 3; week >= 0; week--) {
      final cells = <Widget>[];
      for (var dow = 0; dow < 7; dow++) {
        final dayIndex = week * 7 + dow;
        final d = DateTime(now.year, now.month, now.day)
            .subtract(Duration(days: 27 - dayIndex));
        final key =
            '${d.year.toString().padLeft(4, '0')}-${d.month.toString().padLeft(2, '0')}-${d.day.toString().padLeft(2, '0')}';
        final e = map[key];
        final solved = e?['s'] ?? 0;
        final failed = e?['f'] ?? 0;
        final total = solved + failed;
        Color bg;
        if (total == 0) {
          bg = cs.surfaceContainerHighest.withValues(alpha: 0.35);
        } else {
          final ratio = solved / total;
          final blended = Color.lerp(
                cs.errorContainer,
                cs.primaryContainer,
                ratio,
              ) ??
              cs.primaryContainer;
          bg = blended.withValues(
              alpha: 0.35 + 0.5 * (total.clamp(1, 8) / 8));
        }
        cells.add(
          Tooltip(
            message: '$key · ✓ $solved · ✗ $failed',
            child: Container(
              width: 13,
              height: 13,
              margin: const EdgeInsets.all(2),
              decoration: BoxDecoration(
                color: bg,
                borderRadius: BorderRadius.circular(3),
                border: Border.all(color: cs.outline.withValues(alpha: 0.2)),
              ),
            ),
          ),
        );
      }
      rows.add(Row(mainAxisSize: MainAxisSize.min, children: cells));
    }
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          context.l10n.profileHeatmapTitle,
          style: Theme.of(context).textTheme.titleSmall,
        ),
        const SizedBox(height: 8),
        Column(children: rows),
        const SizedBox(height: 6),
        Text(
          context.l10n.profileHeatmapCaption,
          style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: cs.onSurfaceVariant,
              ),
        ),
      ],
    );
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final prefs = ref.watch(prefsRepositoryProvider);
    final cs = Theme.of(context).colorScheme;
    final week = prefs.puzzleStatsLastDays(7);

    return Scaffold(
      appBar: AppBar(title: Text(l10n.profileTitle)),
      body: ListView(
        padding: const EdgeInsets.all(20),
        children: [
          Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              _avatarPreview(prefs, 44),
              const SizedBox(width: 16),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      prefs.profileDisplayName,
                      style: Theme.of(context).textTheme.headlineSmall,
                    ),
                    const SizedBox(height: 4),
                    Text(
                      l10n.profilePuzzleEloLine('${prefs.puzzleElo}'),
                      style: Theme.of(context).textTheme.titleMedium?.copyWith(
                            color: cs.primary,
                            fontWeight: FontWeight.w600,
                          ),
                    ),
                    Text(
                      l10n.profileWeekStatsLine(week.solved7d, week.failed7d),
                      style: Theme.of(context).textTheme.bodySmall?.copyWith(
                            color: cs.onSurfaceVariant,
                          ),
                    ),
                  ],
                ),
              ),
            ],
          ),
          const Divider(height: 36),
          Text(l10n.profileDisplayName,
              style: Theme.of(context).textTheme.titleSmall),
          const SizedBox(height: 8),
          TextField(
            controller: _nameCtrl,
            decoration: InputDecoration(
              border: const OutlineInputBorder(),
              hintText: l10n.profileNameHint,
            ),
            onSubmitted: (v) async {
              await prefs.setProfileDisplayName(v);
              ref.invalidate(prefsRepositoryProvider);
              setState(() {});
            },
          ),
          Align(
            alignment: Alignment.centerRight,
            child: TextButton(
              onPressed: () async {
                final messenger = ScaffoldMessenger.of(context);
                await prefs.setProfileDisplayName(_nameCtrl.text);
                ref.invalidate(prefsRepositoryProvider);
                if (!mounted) return;
                setState(() {});
                messenger.showSnackBar(
                  SnackBar(content: Text(l10n.profileNameSavedSnack)),
                );
              },
              child: Text(l10n.profileSaveName),
            ),
          ),
          const Divider(height: 28),
          Text(l10n.profileAvatar, style: Theme.of(context).textTheme.titleSmall),
          const SizedBox(height: 8),
          Text(
            l10n.profileAvatarHint,
            style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: cs.onSurfaceVariant,
                ),
          ),
          const SizedBox(height: 12),
          Wrap(
            spacing: 10,
            runSpacing: 10,
            children: [
              for (var i = 0; i < _defaultIcons.length; i++)
                InkWell(
                  onTap: () => _setDefaultAvatar(prefs, i),
                  borderRadius: BorderRadius.circular(999),
                  child: CircleAvatar(
                    radius: 26,
                    backgroundColor: cs.primaryContainer,
                    child: Icon(
                      _defaultIcons[i],
                      color: cs.onPrimaryContainer,
                    ),
                  ),
                ),
              OutlinedButton.icon(
                onPressed: () => _pickGallery(prefs),
                icon: const Icon(Icons.photo_library_outlined),
                label: Text(l10n.profileFromGallery),
              ),
            ],
          ),
          const Divider(height: 36),
          _heatmap(context, prefs),
          const SizedBox(height: 24),
          Text(
            l10n.profileEloHelpBody,
            style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: cs.onSurfaceVariant,
                ),
          ),
        ],
      ),
    );
  }
}
