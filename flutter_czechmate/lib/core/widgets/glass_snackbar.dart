import 'dart:async';
import 'dart:math' as math;
import 'dart:ui';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';

bool _macTopRightGlassToastPlacement() {
  if (kIsWeb) return false;
  return defaultTargetPlatform == TargetPlatform.macOS;
}

/// Floating snack bar with blur + translucent surface (liquid-glass style).
///
/// Na **macOS** se zobrazí v **pravém horním rohu** a vjede zprava (overlay),
/// aby nepřekrýval spodek okna. Jinak spodní plovoucí [SnackBar] se stejným
/// skleněným obsahem.
void showGlassSnackBar(
  BuildContext context,
  String message, {
  Duration duration = const Duration(seconds: 4),
  bool errorStyle = false,
}) {
  if (_macTopRightGlassToastPlacement()) {
    _showMacTopRightGlassOverlay(context, message, duration, errorStyle);
    return;
  }

  final theme = Theme.of(context);
  final cs = theme.colorScheme;

  ScaffoldMessenger.of(context).showSnackBar(
    SnackBar(
      elevation: 0,
      backgroundColor: Colors.transparent,
      behavior: SnackBarBehavior.floating,
      padding: EdgeInsets.zero,
      margin: const EdgeInsets.fromLTRB(14, 0, 14, 18),
      duration: duration,
      content: _GlassSnackBody(
        theme: theme,
        colorScheme: cs,
        message: message,
        errorStyle: errorStyle,
      ),
    ),
  );
}

/// Jednotná textová zpětná vazba — vždy stejný liquid-glass styl jako
/// [showGlassSnackBar]; [errorStyle] jen mění barvy (chyba vs. neutrál).
void showAppSnackBar(
  BuildContext context,
  String message, {
  Duration duration = const Duration(seconds: 4),
  bool errorStyle = false,
}) {
  showGlassSnackBar(
    context,
    message,
    duration: duration,
    errorStyle: errorStyle,
  );
}

void _showMacTopRightGlassOverlay(
  BuildContext context,
  String message,
  Duration duration,
  bool errorStyle,
) {
  final navigator = Navigator.maybeOf(context, rootNavigator: true);
  final overlay = navigator?.overlay ?? Overlay.maybeOf(context);
  if (overlay == null) {
    final theme = Theme.of(context);
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        elevation: 0,
        backgroundColor: Colors.transparent,
        behavior: SnackBarBehavior.floating,
        padding: EdgeInsets.zero,
        margin: const EdgeInsets.fromLTRB(14, 0, 14, 18),
        duration: duration,
        content: _GlassSnackBody(
          theme: theme,
          colorScheme: theme.colorScheme,
          message: message,
          errorStyle: errorStyle,
        ),
      ),
    );
    return;
  }

  late OverlayEntry entry;
  entry = OverlayEntry(
    builder: (ctx) => _MacTopRightGlassToast(
      message: message,
      duration: duration,
      theme: Theme.of(context),
      errorStyle: errorStyle,
      onDisposeEntry: () {
        entry.remove();
      },
    ),
  );
  overlay.insert(entry);
}

class _GlassSnackBody extends StatelessWidget {
  const _GlassSnackBody({
    required this.theme,
    required this.colorScheme,
    required this.message,
    this.errorStyle = false,
  });

  final ThemeData theme;
  final ColorScheme colorScheme;
  final String message;
  final bool errorStyle;

  @override
  Widget build(BuildContext context) {
    final cs = colorScheme;
    final gradientColors = errorStyle
        ? [
            Color.alphaBlend(
              cs.error.withValues(alpha: 0.22),
              cs.surface.withValues(alpha: 0.82),
            ),
            Color.alphaBlend(
              cs.error.withValues(alpha: 0.38),
              cs.surface.withValues(alpha: 0.52),
            ),
          ]
        : [
            cs.surface.withValues(alpha: 0.78),
            cs.surface.withValues(alpha: 0.48),
          ];
    final borderColor = errorStyle
        ? cs.error.withValues(alpha: 0.42)
        : cs.onSurface.withValues(alpha: 0.12);
    final textColor = errorStyle ? cs.error : cs.onSurface;

    return ClipRRect(
      borderRadius: BorderRadius.circular(22),
      child: BackdropFilter(
        filter: ImageFilter.blur(sigmaX: 28, sigmaY: 28),
        child: DecoratedBox(
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(22),
            gradient: LinearGradient(
              begin: Alignment.topLeft,
              end: Alignment.bottomRight,
              colors: gradientColors,
            ),
            border: Border.all(color: borderColor),
            boxShadow: [
              BoxShadow(
                color: Colors.black.withValues(alpha: 0.12),
                blurRadius: 32,
                offset: const Offset(0, 12),
                spreadRadius: -4,
              ),
            ],
          ),
          child: Padding(
            padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 14),
            child: DefaultTextStyle.merge(
              style: theme.textTheme.bodyMedium!.copyWith(
                color: textColor,
                height: 1.35,
                fontWeight: errorStyle ? FontWeight.w600 : null,
              ),
              child: Text(message),
            ),
          ),
        ),
      ),
    );
  }
}

class _MacTopRightGlassToast extends StatefulWidget {
  const _MacTopRightGlassToast({
    required this.message,
    required this.duration,
    required this.theme,
    required this.errorStyle,
    required this.onDisposeEntry,
  });

  final String message;
  final Duration duration;
  final ThemeData theme;
  final bool errorStyle;
  final VoidCallback onDisposeEntry;

  @override
  State<_MacTopRightGlassToast> createState() => _MacTopRightGlassToastState();
}

class _MacTopRightGlassToastState extends State<_MacTopRightGlassToast>
    with SingleTickerProviderStateMixin {
  late final AnimationController _controller = AnimationController(
    vsync: this,
    duration: const Duration(milliseconds: 280),
  );
  late final Animation<Offset> _slide = Tween<Offset>(
    begin: const Offset(1.08, 0),
    end: Offset.zero,
  ).animate(CurvedAnimation(
    parent: _controller,
    curve: Curves.easeOutCubic,
    reverseCurve: Curves.easeInCubic,
  ));

  Timer? _hideTimer;
  bool _closing = false;

  @override
  void initState() {
    super.initState();
    _controller.forward();
    _hideTimer = Timer(widget.duration, () {
      unawaited(_dismiss());
    });
  }

  Future<void> _dismiss() async {
    if (_closing || !mounted) return;
    _closing = true;
    _hideTimer?.cancel();
    _hideTimer = null;
    await _controller.reverse();
    if (!mounted) return;
    widget.onDisposeEntry();
  }

  @override
  void dispose() {
    _hideTimer?.cancel();
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final mq = MediaQuery.of(context);
    final cs = widget.theme.colorScheme;
    final screenW = mq.size.width;
    const edge = 16.0;
    final toastW = math.min(380.0, screenW - edge * 2);

    return Stack(
      fit: StackFit.expand,
      children: [
        Positioned(
          top: mq.padding.top + 12,
          right: edge,
          width: toastW,
          child: SlideTransition(
            position: _slide,
            child: GestureDetector(
              behavior: HitTestBehavior.opaque,
              onTap: () => unawaited(_dismiss()),
              onHorizontalDragEnd: (details) {
                final v = details.primaryVelocity ?? 0;
                if (v > 280) {
                  unawaited(_dismiss());
                }
              },
              child: MouseRegion(
                cursor: SystemMouseCursors.click,
                child: Material(
                  color: Colors.transparent,
                  child: Semantics(
                    button: true,
                    label: 'Dismiss notification',
                    child: _GlassSnackBody(
                      theme: widget.theme,
                      colorScheme: cs,
                      message: widget.message,
                      errorStyle: widget.errorStyle,
                    ),
                  ),
                ),
              ),
            ),
          ),
        ),
      ],
    );
  }
}
