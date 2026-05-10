import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app_providers.dart';
import '../../../core/localization/context_l10n.dart';
import '../../../core/utils/user_facing_error_message.dart';
import '../../../l10n/app_localizations.dart';
import '../../../core/widgets/pressable_scale.dart';
import '../../connection/board_session_notifier.dart';
import '../../connection/board_session_state.dart';

int _colorChannelByte(double normalized) =>
    (normalized * 255.0).round().clamp(0, 255);

/// Barva + štítek pro předvolby (Hue-like scény).
class BoardLampPreset {
  const BoardLampPreset(this.label, this.r, this.g, this.b, this.icon);

  final String label;
  final int r;
  final int g;
  final int b;
  final IconData icon;
}

List<BoardLampPreset> boardLampPresets(AppLocalizations l10n) => [
      BoardLampPreset(l10n.lampPresetWarmWhite, 255, 214, 170,
          Icons.wb_incandescent_outlined),
      BoardLampPreset(
          l10n.lampPresetCoolWhite, 220, 235, 255, Icons.wb_cloudy_outlined),
      BoardLampPreset(
          l10n.lampPresetCalmBlue, 70, 140, 255, Icons.water_drop_outlined),
      BoardLampPreset(
          l10n.lampPresetForestGreen, 66, 200, 120, Icons.forest_outlined),
      BoardLampPreset(l10n.lampPresetWarmth, 255, 120, 48,
          Icons.local_fire_department_outlined),
      BoardLampPreset(
          l10n.lampPresetPurpleScene, 160, 96, 255, Icons.auto_awesome),
      BoardLampPreset(l10n.lampPresetRed, 255, 48, 48, Icons.favorite_outline),
      BoardLampPreset(
          l10n.lampPresetAmber, 255, 180, 32, Icons.light_mode_outlined),
    ];

/// Čtyři doplňkové barvy k předvolbám (12 swatchů celkem).
const _lampGridExtraRgb = [
  [255, 255, 255],
  [0, 255, 255],
  [255, 0, 255],
  [140, 140, 155],
];

/// Disk hue × saturace (střed = bílá, okraj = plná barva), hodnota V zvlášť.
class _HueSatDiskPainter extends CustomPainter {
  _HueSatDiskPainter({required this.step});

  final double step;

  @override
  void paint(Canvas canvas, Size size) {
    final c = Offset(size.width / 2, size.height / 2);
    final r = size.shortestSide / 2;
    for (double y = -r; y <= r; y += step) {
      for (double x = -r; x <= r; x += step) {
        final p = Offset(x, y);
        final dist = p.distance;
        if (dist > r) continue;
        final sat = dist <= 1 ? 0.0 : (dist / r).clamp(0.0, 1.0);
        var hue = 0.0;
        if (dist > 1.5) {
          hue = (math.atan2(p.dy, p.dx) * 180 / math.pi + 90) % 360;
          if (hue < 0) hue += 360;
        }
        final color = HSVColor.fromAHSV(1.0, hue, sat, 1.0).toColor();
        canvas.drawRect(
          Rect.fromCenter(center: c + p, width: step + 1, height: step + 1),
          Paint()..color = color,
        );
      }
    }
    final rim = Paint()
      ..color = Colors.white.withValues(alpha: 0.12)
      ..style = PaintingStyle.stroke
      ..strokeWidth = 1.5;
    canvas.drawCircle(c, r, rim);
  }

  @override
  bool shouldRepaint(covariant _HueSatDiskPainter oldDelegate) =>
      oldDelegate.step != step;
}

class _HueSatThumbPainter extends CustomPainter {
  _HueSatThumbPainter({
    required this.hue,
    required this.saturation,
    required this.diskRadius,
  });

  final double hue;
  final double saturation;
  final double diskRadius;

  @override
  void paint(Canvas canvas, Size size) {
    final c = Offset(size.width / 2, size.height / 2);
    final rThumb = (diskRadius * saturation).clamp(0.0, diskRadius);
    final angle = hue * math.pi / 180 - math.pi / 2;
    final pos = c + Offset(math.cos(angle), math.sin(angle)) * rThumb;
    final fill =
        HSVColor.fromAHSV(1.0, hue, saturation.clamp(0, 1), 1.0).toColor();
    canvas.drawCircle(
      pos,
      10,
      Paint()
        ..color = fill
        ..isAntiAlias = true,
    );
    canvas.drawCircle(
      pos,
      10,
      Paint()
        ..color = Colors.white.withValues(alpha: 0.9)
        ..style = PaintingStyle.stroke
        ..strokeWidth = 2,
    );
  }

  @override
  bool shouldRepaint(covariant _HueSatThumbPainter oldDelegate) =>
      oldDelegate.hue != hue ||
      oldDelegate.saturation != saturation ||
      oldDelegate.diskRadius != diskRadius;
}

/// Náhled 8×8 — odhad rozptylu RGB pod desku (ne přesné LED mapování).
class _BoardGlowPreview extends StatelessWidget {
  const _BoardGlowPreview({
    required this.rgb,
    required this.intensity,
    required this.lampOn,
  });

  final Color rgb;
  final double intensity;
  final bool lampOn;

  @override
  Widget build(BuildContext context) {
    final cs = Theme.of(context).colorScheme;
    final glow = lampOn ? rgb : rgb.withValues(alpha: 0.25);
    final v = lampOn ? intensity : 0.12;

    return AspectRatio(
      aspectRatio: 1,
      child: ClipRRect(
        borderRadius: BorderRadius.circular(16),
        child: Stack(
          fit: StackFit.expand,
          children: [
            CustomPaint(
              painter: _GlowSquaresPainter(glow: glow, blend: v),
            ),
            DecoratedBox(
              decoration: BoxDecoration(
                gradient: RadialGradient(
                  colors: [
                    glow.withValues(alpha: 0.35 * v),
                    Colors.transparent,
                  ],
                  stops: const [0.0, 0.85],
                ),
              ),
            ),
            Align(
              alignment: Alignment.bottomCenter,
              child: Padding(
                padding: const EdgeInsets.all(8),
                child: Text(
                  lampOn
                      ? context.l10n.lampStudioPreviewGlowHint
                      : context.l10n.lampStudioPreviewLampOff,
                  style: Theme.of(context).textTheme.labelSmall?.copyWith(
                        color: cs.onSurface.withValues(alpha: 0.75),
                      ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _GlowSquaresPainter extends CustomPainter {
  _GlowSquaresPainter({required this.glow, required this.blend});

  final Color glow;
  final double blend;

  @override
  void paint(Canvas canvas, Size size) {
    final sq = size.width / 8;
    for (var row = 0; row < 8; row++) {
      for (var col = 0; col < 8; col++) {
        final dark = (row + col).isOdd;
        final base = dark ? const Color(0xFF151C24) : const Color(0xFF1E2730);
        final lit = Color.alphaBlend(
          glow.withValues(alpha: (dark ? 0.07 : 0.05) + 0.42 * blend),
          base,
        );
        canvas.drawRect(
          Rect.fromLTWH(col * sq, row * sq, sq + 0.5, sq + 0.5),
          Paint()..color = lit,
        );
      }
    }
  }

  @override
  bool shouldRepaint(covariant _GlowSquaresPainter oldDelegate) =>
      oldDelegate.glow != glow || oldDelegate.blend != blend;
}

/// Hue-like studio pro lampu desky — výběr barvy, jas, náhled, předvolby.
class BoardLampStudioPanel extends ConsumerStatefulWidget {
  const BoardLampStudioPanel({super.key});

  @override
  ConsumerState<BoardLampStudioPanel> createState() =>
      _BoardLampStudioPanelState();
}

class _BoardLampStudioPanelState extends ConsumerState<BoardLampStudioPanel> {
  double _hue = 0;
  double _saturation = 0;
  double _value = 1;
  double _boardBright = 50;
  bool _lampOn = true;
  bool _busy = false;
  int _autoTimeoutSec = 300;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) => _syncFromSnapshot());
  }

  void _syncFromSnapshot() {
    final st = ref.read(boardSessionNotifierProvider).snapshot?.status;
    if (st == null || !mounted) return;
    final r = st.lightR ?? 255;
    final g = st.lightG ?? 255;
    final b = st.lightB ?? 255;
    final hsv = HSVColor.fromColor(Color.fromARGB(255, r, g, b));
    setState(() {
      _hue = hsv.hue;
      _saturation = hsv.saturation;
      _value = hsv.value;
      if (st.brightness != null) _boardBright = st.brightness!.toDouble();
      if (st.lightState != null) _lampOn = st.lightState!;
      if (st.autoLampTimeoutSec != null) {
        _autoTimeoutSec = st.autoLampTimeoutSec!.clamp(5, 7200);
      }
    });
  }

  Color _previewRgb() {
    final c = HSVColor.fromAHSV(
            1.0, _hue, _saturation.clamp(0, 1), _value.clamp(0, 1))
        .toColor();
    return Color.fromARGB(
      255,
      _colorChannelByte(c.r),
      _colorChannelByte(c.g),
      _colorChannelByte(c.b),
    );
  }

  List<int> _rgbInts() {
    final c = _previewRgb();
    return [
      _colorChannelByte(c.r),
      _colorChannelByte(c.g),
      _colorChannelByte(c.b),
    ];
  }

  Future<void> _run(Future<void> Function() fn, {String? ok}) async {
    final session = ref.read(boardSessionNotifierProvider);
    if (!_canControl(session)) return;
    setState(() => _busy = true);
    try {
      await fn();
      await ref.read(boardSessionNotifierProvider.notifier).refreshNow();
      if (mounted) {
        _syncFromSnapshot();
        if (ok != null) {
          ScaffoldMessenger.of(context)
              .showSnackBar(SnackBar(content: Text(ok)));
        }
      }
    } catch (e) {
      if (mounted) {
        final l10n = context.l10n;
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(userFacingErrorSummary(l10n, e))),
        );
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  bool _canControl(BoardSessionState session) {
    final w = session.wifiBaseUrl?.trim();
    return (session.transport == BoardTransport.wifi &&
            w != null &&
            w.isNotEmpty) ||
        session.transport == BoardTransport.ble;
  }

  bool _rgbMatches(int r, int g, int b) {
    final c = _rgbInts();
    const t = 14;
    return (c[0] - r).abs() <= t &&
        (c[1] - g).abs() <= t &&
        (c[2] - b).abs() <= t;
  }

  void _applyRgbInts(int r, int g, int b) {
    final hsv = HSVColor.fromColor(Color.fromARGB(255, r, g, b));
    setState(() {
      _hue = hsv.hue;
      _saturation = hsv.saturation;
      _value = hsv.value;
    });
  }

  Widget _lampColorDot(
    BuildContext context, {
    required int r,
    required int g,
    required int b,
    required String tooltip,
    required bool can,
  }) {
    final cs = Theme.of(context).colorScheme;
    final sel = _rgbMatches(r, g, b);
    return Tooltip(
      message: tooltip,
      child: InkWell(
        customBorder: const CircleBorder(),
        onTap: _busy || !can ? null : () => _applyRgbInts(r, g, b),
        child: Container(
          width: 42,
          height: 42,
          decoration: BoxDecoration(
            shape: BoxShape.circle,
            color: Color.fromARGB(255, r, g, b),
            border: Border.all(
              width: sel ? 3 : 1,
              color: sel ? cs.primary : cs.outline.withValues(alpha: 0.45),
            ),
          ),
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final session = ref.watch(boardSessionNotifierProvider);
    final devMode = ref.watch(sharedPreferencesProvider).getBool(
              'czechmate.developerModeUnlocked',
            ) ??
        false;
    final can = _canControl(session);
    final rgb = _previewRgb();
    final cs = Theme.of(context).colorScheme;
    final diskSize = math.min(220.0, MediaQuery.sizeOf(context).width - 72);
    final diskR = diskSize / 2 - 12;

    Widget diskPickerColumn() => Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(l10n.lampStudioColorTitle,
                style: Theme.of(context).textTheme.titleSmall),
            const SizedBox(height: 6),
            Text(
              l10n.lampStudioColorHint,
              style: Theme.of(context).textTheme.bodySmall,
            ),
            const SizedBox(height: 8),
            Center(
              child: SizedBox(
                width: diskSize,
                height: diskSize,
                child: GestureDetector(
                  behavior: HitTestBehavior.opaque,
                  onPanDown: (d) => _onDiskDrag(d.localPosition, diskSize),
                  onPanUpdate: (d) => _onDiskDrag(d.localPosition, diskSize),
                  child: Stack(
                    alignment: Alignment.center,
                    children: [
                      ClipOval(
                        child: CustomPaint(
                          size: Size.square(diskSize),
                          painter: _HueSatDiskPainter(
                            step: math.max(3.0, diskSize / 55),
                          ),
                        ),
                      ),
                      CustomPaint(
                        size: Size.square(diskSize),
                        painter: _HueSatThumbPainter(
                          hue: _hue,
                          saturation: _saturation,
                          diskRadius: diskR,
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ),
          ],
        );

    final slidersColumn = Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text(l10n.lampStudioValueTitle,
            style: Theme.of(context).textTheme.titleSmall),
        Slider(
          value: _value.clamp(0.01, 1.0),
          min: 0.05,
          max: 1.0,
          onChanged: _busy || !can ? null : (v) => setState(() => _value = v),
        ),
        Row(
          crossAxisAlignment: CrossAxisAlignment.baseline,
          textBaseline: TextBaseline.alphabetic,
          children: [
            Expanded(
              child: Text(
                l10n.lampLedBrightnessPct,
                style: Theme.of(context).textTheme.titleSmall,
              ),
            ),
            Text(
              '${_boardBright.round()}%',
              style: Theme.of(context).textTheme.titleSmall?.copyWith(
                color: cs.primary,
                fontFeatures: const [FontFeature.tabularFigures()],
              ),
            ),
          ],
        ),
        Slider(
          value: _boardBright.clamp(0, 100),
          max: 100,
          divisions: 20,
          label: '${_boardBright.round()}%',
          onChanged:
              _busy || !can ? null : (v) => setState(() => _boardBright = v),
        ),
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: Text(l10n.lampOnTitle),
          value: _lampOn,
          onChanged: _busy || !can ? null : (v) => setState(() => _lampOn = v),
        ),
        const SizedBox(height: 8),
        Container(
          height: 52,
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(12),
            color: rgb.withValues(alpha: _lampOn ? 0.28 : 0.1),
            border: Border.all(color: cs.outline.withValues(alpha: 0.35)),
          ),
          padding: const EdgeInsets.symmetric(horizontal: 12),
          child: Row(
            children: [
              ClipRRect(
                borderRadius: BorderRadius.circular(10),
                child: SizedBox(
                  width: 36,
                  height: 36,
                  child: ColoredBox(color: rgb),
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: Text(
                  l10n.lampStudioSelectedSwatchLabel,
                  style: Theme.of(context).textTheme.bodyMedium,
                ),
              ),
            ],
          ),
        ),
      ],
    );

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        if (!can)
          Padding(
            padding: const EdgeInsets.only(bottom: 12),
            child: Text(
              l10n.lampStudioNeedConnection,
              style: Theme.of(context)
                  .textTheme
                  .bodySmall
                  ?.copyWith(color: cs.error),
            ),
          ),
        Text(l10n.lampStudioBoardPreview,
            style: Theme.of(context).textTheme.titleSmall),
        const SizedBox(height: 8),
        ConstrainedBox(
          constraints: const BoxConstraints(maxHeight: 280),
          child: _BoardGlowPreview(
            rgb: rgb,
            intensity: (_boardBright / 100).clamp(0.05, 1.0),
            lampOn: _lampOn,
          ),
        ),
        const SizedBox(height: 16),
        diskPickerColumn(),
        const SizedBox(height: 16),
        Text(l10n.lampStudioPresetColorsTitle,
            style: Theme.of(context).textTheme.titleSmall),
        const SizedBox(height: 8),
        Wrap(
          spacing: 10,
          runSpacing: 10,
          children: [
            for (final p in boardLampPresets(l10n))
              _lampColorDot(
                context,
                r: p.r,
                g: p.g,
                b: p.b,
                tooltip: p.label,
                can: can,
              ),
            for (final row in _lampGridExtraRgb)
              _lampColorDot(
                context,
                r: row[0],
                g: row[1],
                b: row[2],
                tooltip: '${row[0]}, ${row[1]}, ${row[2]}',
                can: can,
              ),
          ],
        ),
        const SizedBox(height: 16),
        slidersColumn,
        if (devMode) ...[
          Theme(
            data: Theme.of(context).copyWith(dividerColor: Colors.transparent),
            child: ExpansionTile(
              tilePadding: EdgeInsets.zero,
              title: Text(l10n.lampStudioDeveloperRgbTitle),
              children: [
                Padding(
                  padding: const EdgeInsets.only(bottom: 12),
                  child: SelectableText(
                    l10n.lampStudioRgbLine(
                      _colorChannelByte(rgb.r),
                      _colorChannelByte(rgb.g),
                      _colorChannelByte(rgb.b),
                    ),
                    style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      fontFeatures: const [FontFeature.tabularFigures()],
                    ),
                  ),
                ),
              ],
            ),
          ),
        ],
        const SizedBox(height: 16),
        Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            PressableScale(
              child: FilledButton.icon(
                style: FilledButton.styleFrom(
                  minimumSize: const Size(double.infinity, 48),
                  padding: const EdgeInsets.symmetric(horizontal: 12),
                ),
                onPressed: !can || _busy
                    ? null
                    : () {
                        final t = _rgbInts();
                        _run(
                          () async {
                            final n =
                                ref.read(boardSessionNotifierProvider.notifier);
                            await n.postBoardBrightness(_boardBright.round());
                            await n.postBoardLightCommand(
                              lampState: _lampOn,
                              r: t[0],
                              g: t[1],
                              b: t[2],
                            );
                          },
                          ok: l10n.lampStudioAppliedOk,
                        );
                      },
                icon: _busy
                    ? const SizedBox(
                        width: 18,
                        height: 18,
                        child: CircularProgressIndicator(strokeWidth: 2),
                      )
                    : const Icon(Icons.send_rounded),
                label: Text(l10n.lampStudioApplyToBoard),
              ),
            ),
            const SizedBox(height: 10),
            OutlinedButton.icon(
              style: OutlinedButton.styleFrom(
                minimumSize: const Size(double.infinity, 48),
                padding: const EdgeInsets.symmetric(horizontal: 12),
                foregroundColor: cs.onSurface,
                side: BorderSide(color: cs.outline),
              ),
              onPressed: !can || _busy
                  ? null
                  : () => _run(
                        () => ref
                            .read(boardSessionNotifierProvider.notifier)
                            .postBoardLightGameMode(),
                        ok: l10n.lampStudioGameModeOk,
                      ),
              icon: const Icon(Icons.sports_esports_outlined),
              label: Text(l10n.lampStudioGameMode),
            ),
          ],
        ),
        const Divider(height: 32),
        Text(l10n.lampStudioAutoOff,
            style: Theme.of(context).textTheme.titleSmall),
        Text(
          '${_autoTimeoutSec}s (${(_autoTimeoutSec / 60).toStringAsFixed(1)} min)',
          style: Theme.of(context).textTheme.bodySmall,
        ),
        Slider(
          min: 5,
          max: 7200,
          value: _autoTimeoutSec.toDouble().clamp(5, 7200),
          onChanged: !can || _busy
              ? null
              : (v) => setState(() => _autoTimeoutSec = v.round()),
        ),
        FilledButton.tonal(
          onPressed: !can || _busy
              ? null
              : () => _run(
                    () => ref
                        .read(boardSessionNotifierProvider.notifier)
                        .postBoardAutoLampTimeout(_autoTimeoutSec),
                    ok: l10n.lampStudioTimerSavedOk,
                  ),
          child: Text(l10n.lampStudioSaveTimer),
        ),
      ],
    );
  }

  void _onDiskDrag(Offset local, double diskSize) {
    final c = Offset(diskSize / 2, diskSize / 2);
    final maxR = diskSize / 2 - 8;
    final d = local - c;
    final dist = d.distance;
    final sat = dist <= 2 ? 0.0 : (dist / maxR).clamp(0.0, 1.0);
    double hue = _hue;
    if (dist > 2) {
      hue = (math.atan2(d.dy, d.dx) * 180 / math.pi + 90) % 360;
      if (hue < 0) hue += 360;
    }
    setState(() {
      _hue = hue;
      _saturation = sat;
    });
  }
}
