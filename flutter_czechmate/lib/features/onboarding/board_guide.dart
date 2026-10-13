import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../../l10n/app_localizations.dart';

/// One step of the board guide (onboarding chapter + Help/Settings entry).
class BoardGuideStep {
  const BoardGuideStep({
    required this.id,
    required this.icon,
    required this.title,
    required this.body,
    required this.mediaBasePath,
  });

  final String id;
  final IconData icon;
  final String Function(AppLocalizations l10n) title;
  final String Function(AppLocalizations l10n) body;

  /// Path without extension. The slot shows `<base>.webp`, else `<base>.png`.
  final String mediaBasePath;
}

/// Media slots for Blender exports. Each folder is listed in `pubspec.yaml`.
///
/// Drop one file per step (same base name as the folder):
/// - still image: render a PNG in Blender → `<name>.png`
/// - motion: render a PNG sequence and convert it to an animated WebP, e.g.
///   `ffmpeg -framerate 30 -i frame_%04d.png -loop 0 -q:v 80 <name>.webp`
///   (Flutter plays animated WebP natively; MP4 would need a video plugin
///   that does not run on Windows desktop).
/// Recommended frame: 16:9, 1280×720. A missing file leaves a calm empty
/// panel with the step icon.
const List<BoardGuideStep> kBoardGuideSteps = [
  BoardGuideStep(
    id: 'connect',
    icon: Icons.bluetooth_connected_rounded,
    title: _connectTitle,
    body: _connectBody,
    // Blender export: assets/guide/connect/connect.webp or connect.png
    mediaBasePath: 'assets/guide/connect/connect',
  ),
  BoardGuideStep(
    id: 'move',
    icon: Icons.pan_tool_alt_outlined,
    title: _moveTitle,
    body: _moveBody,
    // Blender export: assets/guide/move/move.webp or move.png
    mediaBasePath: 'assets/guide/move/move',
  ),
  BoardGuideStep(
    id: 'hint',
    icon: Icons.tips_and_updates_outlined,
    title: _hintTitle,
    body: _hintBody,
    // Blender export: assets/guide/hint/hint.webp or hint.png
    mediaBasePath: 'assets/guide/hint/hint',
  ),
  BoardGuideStep(
    id: 'clock',
    icon: Icons.timer_outlined,
    title: _clockTitle,
    body: _clockBody,
    // Blender export: assets/guide/clock/clock.webp or clock.png
    mediaBasePath: 'assets/guide/clock/clock',
  ),
  BoardGuideStep(
    id: 'resign',
    icon: Icons.flag_outlined,
    title: _resignTitle,
    body: _resignBody,
    // Blender export: assets/guide/resign/resign.webp or resign.png
    mediaBasePath: 'assets/guide/resign/resign',
  ),
];

String _connectTitle(AppLocalizations l) => l.boardGuideConnectTitle;
String _connectBody(AppLocalizations l) => l.boardGuideConnectBody;
String _moveTitle(AppLocalizations l) => l.boardGuideMoveTitle;
String _moveBody(AppLocalizations l) => l.boardGuideMoveBody;
String _hintTitle(AppLocalizations l) => l.boardGuideHintTitle;
String _hintBody(AppLocalizations l) => l.boardGuideHintBody;
String _clockTitle(AppLocalizations l) => l.boardGuideClockTitle;
String _clockBody(AppLocalizations l) => l.boardGuideClockBody;
String _resignTitle(AppLocalizations l) => l.boardGuideResignTitle;
String _resignBody(AppLocalizations l) => l.boardGuideResignBody;

const List<String> _kMediaExtensions = ['webp', 'png'];

Future<Set<String>>? _bundledAssets;

Future<Set<String>> _loadBundledAssets(AssetBundle bundle) {
  return _bundledAssets ??= () async {
    try {
      final manifest = await AssetManifest.loadFromAssetBundle(bundle);
      return manifest.listAssets().toSet();
    } catch (_) {
      return <String>{};
    }
  }();
}

/// First bundled file for [basePath] (`.webp` before `.png`), or null.
String? resolveGuideMedia(String basePath, Set<String> bundled) {
  for (final ext in _kMediaExtensions) {
    final p = '$basePath.$ext';
    if (bundled.contains(p)) return p;
  }
  return null;
}

/// Title, short text and a fixed 16:9 media slot. Side by side on wide
/// windows, stacked on phones.
class BoardGuideStepView extends StatelessWidget {
  const BoardGuideStepView({
    super.key,
    required this.step,
    required this.index,
    required this.count,
  });

  final BoardGuideStep step;
  final int index;
  final int count;

  @override
  Widget build(BuildContext context) {
    final l10n = AppLocalizations.of(context)!;
    final theme = Theme.of(context);
    final cs = theme.colorScheme;
    final title = step.title(l10n);

    final text = Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      mainAxisSize: MainAxisSize.min,
      children: [
        Text(
          '${l10n.boardGuideTitle} · ${l10n.boardGuideStepCounter(index + 1, count)}',
          style: theme.textTheme.labelLarge?.copyWith(
            color: cs.primary,
            fontWeight: FontWeight.w600,
          ),
        ),
        const SizedBox(height: 8),
        Semantics(
          header: true,
          child: Text(
            title,
            style: theme.textTheme.headlineSmall
                ?.copyWith(fontWeight: FontWeight.bold),
          ),
        ),
        const SizedBox(height: 12),
        Text(
          step.body(l10n),
          style: theme.textTheme.bodyLarge?.copyWith(height: 1.45),
        ),
      ],
    );

    final media = BoardGuideMediaSlot(step: step, semanticLabel: title);

    return LayoutBuilder(
      builder: (context, constraints) {
        if (constraints.maxWidth >= 760) {
          return Center(
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 1100),
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.center,
                children: [
                  Expanded(flex: 6, child: media),
                  const SizedBox(width: 32),
                  Expanded(
                    flex: 5,
                    child: SingleChildScrollView(child: text),
                  ),
                ],
              ),
            ),
          );
        }
        return SingleChildScrollView(
          padding: const EdgeInsets.only(bottom: 8),
          child: Center(
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 640),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                mainAxisSize: MainAxisSize.min,
                children: [
                  media,
                  const SizedBox(height: 24),
                  text,
                ],
              ),
            ),
          ),
        );
      },
    );
  }
}

/// Fixed 16:9 frame. Shows the bundled Blender export when present, else a
/// quiet surface with the step icon (never a broken-image glyph).
class BoardGuideMediaSlot extends StatelessWidget {
  const BoardGuideMediaSlot({
    super.key,
    required this.step,
    required this.semanticLabel,
  });

  final BoardGuideStep step;
  final String semanticLabel;

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    final empty = ExcludeSemantics(
      child: Center(
        child: Icon(
          step.icon,
          size: 72,
          color: cs.onSurfaceVariant.withValues(alpha: 0.45),
        ),
      ),
    );
    return AspectRatio(
      aspectRatio: 16 / 9,
      child: DecoratedBox(
        decoration: BoxDecoration(
          color: cs.surfaceContainerHigh,
          borderRadius: BorderRadius.circular(20),
        ),
        child: ClipRRect(
          borderRadius: BorderRadius.circular(20),
          child: FutureBuilder<Set<String>>(
            future: _loadBundledAssets(DefaultAssetBundle.of(context)),
            builder: (context, snap) {
              final path = snap.hasData
                  ? resolveGuideMedia(step.mediaBasePath, snap.data!)
                  : null;
              if (path == null) return empty;
              return Image.asset(
                path,
                key: ValueKey<String>(path),
                fit: BoxFit.contain,
                gaplessPlayback: true,
                semanticLabel: semanticLabel,
                errorBuilder: (_, __, ___) => empty,
              );
            },
          ),
        ),
      ),
    );
  }
}
