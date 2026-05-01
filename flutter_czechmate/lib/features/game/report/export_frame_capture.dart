import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';

/// Captures a [RepaintBoundary] after layout (same frame pipeline as share export).
Future<ui.Image?> captureRepaintBoundaryImage({
  required GlobalKey boundaryKey,
  double pixelRatio = 3,
}) async {
  await WidgetsBinding.instance.endOfFrame;
  final boundary =
      boundaryKey.currentContext?.findRenderObject() as RenderRepaintBoundary?;
  if (boundary == null) return null;
  return boundary.toImage(pixelRatio: pixelRatio);
}

/// Inserts an off-screen overlay, paints one frame, captures, removes overlay.
Future<ui.Image?> captureWidgetOffscreen({
  required BuildContext context,
  required Widget child,
  required Size logicalSize,
  double pixelRatio = 3,
}) async {
  final key = GlobalKey();
  final overlay = Overlay.maybeOf(context);
  if (overlay == null) return null;

  late OverlayEntry entry;
  entry = OverlayEntry(
    builder: (ctx) => Positioned(
      left: -8000,
      top: 0,
      child: Material(
        color: Colors.transparent,
        child: RepaintBoundary(
          key: key,
          child: SizedBox(
            width: logicalSize.width,
            height: logicalSize.height,
            child: child,
          ),
        ),
      ),
    ),
  );

  overlay.insert(entry);
  await WidgetsBinding.instance.endOfFrame;
  await Future<void>.delayed(Duration.zero);
  ui.Image? image;
  try {
    final boundary = key.currentContext?.findRenderObject() as RenderRepaintBoundary?;
    image = await boundary?.toImage(pixelRatio: pixelRatio);
  } finally {
    entry.remove();
  }
  return image;
}
