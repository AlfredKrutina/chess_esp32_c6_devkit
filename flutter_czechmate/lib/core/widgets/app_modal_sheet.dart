import 'package:flutter/material.dart';

/// Jednotné otevírání bottom sheetů (drag handle, jednotné API).
Future<T?> showAppModalBottomSheet<T>({
  required BuildContext context,
  required WidgetBuilder builder,
  bool isScrollControlled = false,
  bool useSafeArea = false,
  bool showDragHandle = true,
  Color? backgroundColor,
  BoxConstraints? constraints,
}) {
  return showModalBottomSheet<T>(
    context: context,
    isScrollControlled: isScrollControlled,
    useSafeArea: useSafeArea,
    showDragHandle: showDragHandle,
    backgroundColor: backgroundColor,
    constraints: constraints,
    builder: builder,
  );
}
