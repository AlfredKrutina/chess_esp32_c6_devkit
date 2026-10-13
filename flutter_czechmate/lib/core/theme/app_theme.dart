import 'package:flutter/material.dart';

import 'app_motion.dart';

/// Graphite surfaces, one steel-blue accent. The clock of the side on move is
/// the only large coloured area; everything else stays quiet.
/// Dark is the default look; light is the second variant with the same roles.
abstract final class AppTheme {
  static const double radius = 12;

  static ThemeData dark() {
    const scheme = ColorScheme(
      brightness: Brightness.dark,
      primary: Color(0xFF8FB3E8),
      onPrimary: Color(0xFF0A1626),
      primaryContainer: Color(0xFF243A5A),
      onPrimaryContainer: Color(0xFFD6E4F8),
      secondary: Color(0xFFA3A9B2),
      onSecondary: Color(0xFF111317),
      secondaryContainer: Color(0xFF2A3546),
      onSecondaryContainer: Color(0xFFECEEF1),
      tertiary: Color(0xFFF2B544),
      onTertiary: Color(0xFF1F1300),
      tertiaryContainer: Color(0xFF4A3608),
      onTertiaryContainer: Color(0xFFFFE2AE),
      error: Color(0xFFF29A8F),
      onError: Color(0xFF2A0604),
      errorContainer: Color(0xFF5C1A14),
      onErrorContainer: Color(0xFFFFDAD5),
      surface: Color(0xFF111317),
      onSurface: Color(0xFFECEEF1),
      onSurfaceVariant: Color(0xFFA3A9B2),
      surfaceDim: Color(0xFF0C0E11),
      surfaceBright: Color(0xFF2A2E35),
      surfaceContainerLowest: Color(0xFF0C0E11),
      surfaceContainerLow: Color(0xFF16181C),
      surfaceContainer: Color(0xFF1A1D22),
      surfaceContainerHigh: Color(0xFF22262C),
      surfaceContainerHighest: Color(0xFF2A2E35),
      outline: Color(0xFF5A6069),
      outlineVariant: Color(0xFF33373E),
      shadow: Color(0xFF000000),
      scrim: Color(0xFF000000),
      inverseSurface: Color(0xFFECEEF1),
      onInverseSurface: Color(0xFF111317),
      inversePrimary: Color(0xFF224F8F),
    );
    return _build(scheme, ClockTokens.dark);
  }

  static ThemeData light() {
    const scheme = ColorScheme(
      brightness: Brightness.light,
      primary: Color(0xFF224F8F),
      onPrimary: Color(0xFFFFFFFF),
      primaryContainer: Color(0xFFD6E4F8),
      onPrimaryContainer: Color(0xFF0A1626),
      secondary: Color(0xFF474D56),
      onSecondary: Color(0xFFFFFFFF),
      secondaryContainer: Color(0xFFDCE3EE),
      onSecondaryContainer: Color(0xFF15171B),
      tertiary: Color(0xFF7A4E00),
      onTertiary: Color(0xFFFFFFFF),
      tertiaryContainer: Color(0xFFFFE2AE),
      onTertiaryContainer: Color(0xFF2A1A00),
      error: Color(0xFF9E2A20),
      onError: Color(0xFFFFFFFF),
      errorContainer: Color(0xFFFFDAD5),
      onErrorContainer: Color(0xFF410002),
      surface: Color(0xFFF4F5F7),
      onSurface: Color(0xFF15171B),
      onSurfaceVariant: Color(0xFF474D56),
      surfaceDim: Color(0xFFDDE0E5),
      surfaceBright: Color(0xFFFFFFFF),
      surfaceContainerLowest: Color(0xFFFFFFFF),
      surfaceContainerLow: Color(0xFFFFFFFF),
      surfaceContainer: Color(0xFFEDEFF2),
      surfaceContainerHigh: Color(0xFFE9EBEF),
      surfaceContainerHighest: Color(0xFFE1E4E9),
      outline: Color(0xFF7D838C),
      outlineVariant: Color(0xFFD3D7DD),
      shadow: Color(0xFF000000),
      scrim: Color(0xFF000000),
      inverseSurface: Color(0xFF22262C),
      onInverseSurface: Color(0xFFECEEF1),
      inversePrimary: Color(0xFF8FB3E8),
    );
    return _build(scheme, ClockTokens.light);
  }

  static ThemeData _build(ColorScheme scheme, ClockTokens clocks) {
    final rounded = RoundedRectangleBorder(
      borderRadius: BorderRadius.circular(radius),
    );
    final text = ThemeData(colorScheme: scheme, useMaterial3: true).textTheme;
    final indicator = scheme.primary.withValues(alpha: 0.16);
    return ThemeData(
      colorScheme: scheme,
      useMaterial3: true,
      scaffoldBackgroundColor: scheme.surface,
      canvasColor: scheme.surface,
      visualDensity: VisualDensity.adaptivePlatformDensity,
      extensions: [clocks],
      textTheme: text.copyWith(
        headlineSmall:
            text.headlineSmall?.copyWith(fontWeight: FontWeight.w700, height: 1.15),
        titleLarge:
            text.titleLarge?.copyWith(fontWeight: FontWeight.w700, height: 1.2),
        titleMedium:
            text.titleMedium?.copyWith(fontWeight: FontWeight.w600, height: 1.25),
        bodyMedium: text.bodyMedium?.copyWith(height: 1.4),
        bodySmall: text.bodySmall?.copyWith(height: 1.35),
        labelSmall: text.labelSmall?.copyWith(letterSpacing: 0.2),
      ),
      appBarTheme: AppBarTheme(
        centerTitle: false,
        elevation: 0,
        scrolledUnderElevation: 0,
        backgroundColor: scheme.surface,
        surfaceTintColor: Colors.transparent,
        titleTextStyle: text.titleLarge?.copyWith(
          color: scheme.onSurface,
          fontWeight: FontWeight.w700,
        ),
      ),
      cardTheme: CardThemeData(
        elevation: 0,
        color: scheme.surfaceContainer,
        surfaceTintColor: Colors.transparent,
        margin: EdgeInsets.zero,
        shape: rounded,
      ),
      inputDecorationTheme: InputDecorationTheme(
        filled: true,
        fillColor: scheme.surfaceContainerHigh,
        contentPadding: const EdgeInsets.symmetric(horizontal: 14, vertical: 14),
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(radius),
          borderSide: BorderSide(color: scheme.outlineVariant),
        ),
        enabledBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(radius),
          borderSide: BorderSide(color: scheme.outlineVariant),
        ),
        focusedBorder: OutlineInputBorder(
          borderRadius: BorderRadius.circular(radius),
          borderSide: BorderSide(color: scheme.primary, width: 2),
        ),
      ),
      listTileTheme: ListTileThemeData(
        contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 2),
        minVerticalPadding: 12,
        shape: rounded,
        iconColor: scheme.onSurfaceVariant,
      ),
      dialogTheme: DialogThemeData(
        shape: rounded,
        backgroundColor: scheme.surfaceContainer,
        surfaceTintColor: Colors.transparent,
      ),
      bottomSheetTheme: BottomSheetThemeData(
        backgroundColor: scheme.surfaceContainer,
        surfaceTintColor: Colors.transparent,
        showDragHandle: true,
        dragHandleColor: scheme.outline,
      ),
      popupMenuTheme: PopupMenuThemeData(
        color: scheme.surfaceContainerHigh,
        surfaceTintColor: Colors.transparent,
        shape: rounded,
      ),
      dividerTheme: DividerThemeData(
        color: scheme.outlineVariant,
        thickness: 1,
        space: 1,
      ),
      filledButtonTheme: FilledButtonThemeData(
        style: FilledButton.styleFrom(
          shape: rounded,
          minimumSize: const Size(64, 52),
          padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
          textStyle: text.titleSmall?.copyWith(fontWeight: FontWeight.w600),
          animationDuration: AppMotion.microInteraction,
        ),
      ),
      outlinedButtonTheme: OutlinedButtonThemeData(
        style: OutlinedButton.styleFrom(
          shape: rounded,
          minimumSize: const Size(64, 48),
          padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 12),
          side: BorderSide(color: scheme.outlineVariant),
          animationDuration: AppMotion.microInteraction,
        ),
      ),
      textButtonTheme: TextButtonThemeData(
        style: TextButton.styleFrom(
          shape: rounded,
          minimumSize: const Size(48, 44),
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 10),
          animationDuration: AppMotion.microInteraction,
        ),
      ),
      iconButtonTheme: IconButtonThemeData(
        style: IconButton.styleFrom(
          minimumSize: const Size(44, 44),
          foregroundColor: scheme.onSurface,
          disabledForegroundColor: scheme.onSurface.withValues(alpha: 0.32),
          animationDuration: AppMotion.microInteraction,
        ),
      ),
      segmentedButtonTheme: SegmentedButtonThemeData(
        style: ButtonStyle(
          minimumSize: const WidgetStatePropertyAll(Size(48, 40)),
          shape: WidgetStatePropertyAll(rounded),
          side: WidgetStatePropertyAll(BorderSide(color: scheme.outlineVariant)),
          backgroundColor: WidgetStateProperty.resolveWith(
            (s) => s.contains(WidgetState.selected) ? indicator : null,
          ),
          foregroundColor: WidgetStateProperty.resolveWith(
            (s) => s.contains(WidgetState.selected)
                ? scheme.onSurface
                : scheme.onSurfaceVariant,
          ),
        ),
      ),
      switchTheme: SwitchThemeData(
        trackColor: WidgetStateProperty.resolveWith(
          (s) => s.contains(WidgetState.selected) ? scheme.primary : null,
        ),
        thumbColor: WidgetStateProperty.resolveWith(
          (s) => s.contains(WidgetState.selected) ? scheme.onPrimary : null,
        ),
      ),
      navigationBarTheme: NavigationBarThemeData(
        height: 56,
        elevation: 0,
        backgroundColor: scheme.surface,
        surfaceTintColor: Colors.transparent,
        indicatorColor: indicator,
        labelBehavior: NavigationDestinationLabelBehavior.onlyShowSelected,
        iconTheme: WidgetStateProperty.resolveWith(
          (s) => IconThemeData(
            size: 24,
            color: s.contains(WidgetState.selected)
                ? scheme.onSurface
                : scheme.onSurfaceVariant,
          ),
        ),
        labelTextStyle: WidgetStatePropertyAll(
          text.labelMedium?.copyWith(
            color: scheme.onSurface,
            fontWeight: FontWeight.w600,
          ),
        ),
      ),
      navigationRailTheme: NavigationRailThemeData(
        backgroundColor: scheme.surface,
        indicatorColor: indicator,
        selectedIconTheme: IconThemeData(color: scheme.onSurface),
        unselectedIconTheme: IconThemeData(color: scheme.onSurfaceVariant),
        selectedLabelTextStyle: TextStyle(
          color: scheme.onSurface,
          fontWeight: FontWeight.w600,
        ),
      ),
      tabBarTheme: TabBarThemeData(
        indicatorColor: scheme.primary,
        labelColor: scheme.onSurface,
        unselectedLabelColor: scheme.onSurfaceVariant,
        dividerColor: scheme.outlineVariant,
      ),
      progressIndicatorTheme: ProgressIndicatorThemeData(color: scheme.primary),
      snackBarTheme: SnackBarThemeData(
        behavior: SnackBarBehavior.floating,
        backgroundColor: scheme.inverseSurface,
        contentTextStyle:
            text.bodyMedium?.copyWith(color: scheme.onInverseSurface),
        shape: rounded,
      ),
      tooltipTheme: TooltipThemeData(
        waitDuration: const Duration(milliseconds: 400),
        decoration: BoxDecoration(
          color: scheme.inverseSurface,
          borderRadius: BorderRadius.circular(8),
        ),
        textStyle: text.labelMedium?.copyWith(color: scheme.onInverseSurface),
      ),
      pageTransitionsTheme: AppMotion.pageTransitionsTheme,
    );
  }
}

/// Clock face colours. Every pair keeps at least 7:1 contrast so the time is
/// readable from across the table. The watch reuses the same values.
@immutable
class ClockTokens extends ThemeExtension<ClockTokens> {
  const ClockTokens({
    required this.idleBackground,
    required this.idleForeground,
    required this.activeBackground,
    required this.activeForeground,
    required this.lowBackground,
    required this.lowForeground,
    required this.expiredBackground,
    required this.expiredForeground,
    required this.mutedForeground,
  });

  final Color idleBackground;
  final Color idleForeground;
  final Color activeBackground;
  final Color activeForeground;
  final Color lowBackground;
  final Color lowForeground;
  final Color expiredBackground;
  final Color expiredForeground;

  /// Small state icons on an idle face; never used for the digits.
  final Color mutedForeground;

  static const dark = ClockTokens(
    idleBackground: Color(0xFF1A1D22),
    idleForeground: Color(0xFFD4D8DE),
    activeBackground: Color(0xFFA9C8F2),
    activeForeground: Color(0xFF0A1626),
    lowBackground: Color(0xFFF2B544),
    lowForeground: Color(0xFF1F1300),
    expiredBackground: Color(0xFFF29A8F),
    expiredForeground: Color(0xFF2A0604),
    mutedForeground: Color(0xFFA3A9B2),
  );

  static const light = ClockTokens(
    idleBackground: Color(0xFFE4E7EC),
    idleForeground: Color(0xFF1A1D22),
    activeBackground: Color(0xFF1F4E8C),
    activeForeground: Color(0xFFFFFFFF),
    lowBackground: Color(0xFF7A4E00),
    lowForeground: Color(0xFFFFFFFF),
    expiredBackground: Color(0xFF9E2A20),
    expiredForeground: Color(0xFFFFFFFF),
    mutedForeground: Color(0xFF474D56),
  );

  static ClockTokens of(BuildContext context) {
    final theme = Theme.of(context);
    return theme.extension<ClockTokens>() ??
        (theme.brightness == Brightness.dark ? dark : light);
  }

  @override
  ClockTokens copyWith({
    Color? idleBackground,
    Color? idleForeground,
    Color? activeBackground,
    Color? activeForeground,
    Color? lowBackground,
    Color? lowForeground,
    Color? expiredBackground,
    Color? expiredForeground,
    Color? mutedForeground,
  }) {
    return ClockTokens(
      idleBackground: idleBackground ?? this.idleBackground,
      idleForeground: idleForeground ?? this.idleForeground,
      activeBackground: activeBackground ?? this.activeBackground,
      activeForeground: activeForeground ?? this.activeForeground,
      lowBackground: lowBackground ?? this.lowBackground,
      lowForeground: lowForeground ?? this.lowForeground,
      expiredBackground: expiredBackground ?? this.expiredBackground,
      expiredForeground: expiredForeground ?? this.expiredForeground,
      mutedForeground: mutedForeground ?? this.mutedForeground,
    );
  }

  @override
  ClockTokens lerp(ClockTokens? other, double t) {
    if (other == null) return this;
    return ClockTokens(
      idleBackground: Color.lerp(idleBackground, other.idleBackground, t)!,
      idleForeground: Color.lerp(idleForeground, other.idleForeground, t)!,
      activeBackground: Color.lerp(activeBackground, other.activeBackground, t)!,
      activeForeground: Color.lerp(activeForeground, other.activeForeground, t)!,
      lowBackground: Color.lerp(lowBackground, other.lowBackground, t)!,
      lowForeground: Color.lerp(lowForeground, other.lowForeground, t)!,
      expiredBackground:
          Color.lerp(expiredBackground, other.expiredBackground, t)!,
      expiredForeground:
          Color.lerp(expiredForeground, other.expiredForeground, t)!,
      mutedForeground: Color.lerp(mutedForeground, other.mutedForeground, t)!,
    );
  }
}
