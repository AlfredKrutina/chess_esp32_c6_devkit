import 'dart:async' show unawaited;

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:permission_handler/permission_handler.dart';

import '../../app_providers.dart';
import '../../core/widgets/pressable_scale.dart';
import '../../core/localization/context_l10n.dart';
import '../../l10n/app_localizations.dart';

class OnboardingScreen extends ConsumerStatefulWidget {
  const OnboardingScreen({super.key, this.onDone});

  /// Called when user dismisses onboarding. Null = use Navigator.pop.
  final VoidCallback? onDone;

  @override
  ConsumerState<OnboardingScreen> createState() => _OnboardingScreenState();
}

class _OnboardingScreenState extends ConsumerState<OnboardingScreen> {
  static const _introCount = 5;
  static const _totalSteps =
      2 + _introCount; // jméno, oprávnění, pak úvodní slidů

  int _step = 0;
  late final TextEditingController _nameCtrl;
  bool _nameLoaded = false;

  bool? _photosGranted;
  bool? _bleGranted;
  bool? _wifiGranted;
  bool _permBootstrapDone = false;

  bool get _isMobileNative =>
      !kIsWeb &&
      (defaultTargetPlatform == TargetPlatform.iOS ||
          defaultTargetPlatform == TargetPlatform.android);

  bool get _isAndroid =>
      !kIsWeb && defaultTargetPlatform == TargetPlatform.android;

  @override
  void initState() {
    super.initState();
    _nameCtrl = TextEditingController();
  }

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (!_nameLoaded) {
      _nameLoaded = true;
      final s =
          ref.read(prefsRepositoryProvider).profileDisplayNameStoredOrNull;
      if (s != null && s.isNotEmpty) {
        _nameCtrl.text = s;
      }
    }
  }

  @override
  void dispose() {
    _nameCtrl.dispose();
    super.dispose();
  }

  List<_IntroSlide> _introSlides(AppLocalizations l10n) => [
        _IntroSlide(
          icon: Icons.check_box_outline_blank,
          iconColor: Colors.blueAccent,
          title: l10n.onboard1Title,
          body: l10n.onboard1Body,
        ),
        _IntroSlide(
          icon: Icons.cable,
          iconColor: Colors.orange,
          title: l10n.onboard2Title,
          body: l10n.onboard2Body,
        ),
        _IntroSlide(
          icon: Icons.lightbulb_outline,
          iconColor: Colors.amber,
          title: l10n.onboard3Title,
          body: l10n.onboard3Body,
        ),
        _IntroSlide(
          icon: Icons.sports_esports,
          iconColor: Colors.deepPurple,
          title: l10n.onboard4Title,
          body: l10n.onboard4Body,
        ),
        _IntroSlide(
          icon: Icons.school,
          iconColor: Colors.green,
          title: l10n.onboard5Title,
          body: l10n.onboard5Body,
        ),
      ];

  Future<void> _persistName() async {
    await ref
        .read(prefsRepositoryProvider)
        .setProfileDisplayName(_nameCtrl.text);
    ref.invalidate(prefsRepositoryProvider);
  }

  Future<void> _refreshPhotoStatus() async {
    if (!_isMobileNative) return;
    final g = await Permission.photos.isGranted;
    if (mounted) setState(() => _photosGranted = g);
  }

  Future<void> _refreshBleStatus() async {
    if (!_isMobileNative) return;
    if (defaultTargetPlatform == TargetPlatform.iOS) {
      final g = await Permission.bluetooth.isGranted;
      if (mounted) setState(() => _bleGranted = g);
      return;
    }
    final c = await Permission.bluetoothConnect.isGranted;
    final s = await Permission.bluetoothScan.isGranted;
    if (mounted) setState(() => _bleGranted = c && s);
  }

  Future<void> _refreshWifiStatus() async {
    if (!_isAndroid) return;
    final g = await Permission.nearbyWifiDevices.isGranted;
    if (mounted) setState(() => _wifiGranted = g);
  }

  Future<void> _requestPhotos() async {
    if (!_isMobileNative) return;
    final r = await Permission.photos.request();
    await _refreshPhotoStatus();
    if (!mounted) return;
    if (!r.isGranted && r != PermissionStatus.limited) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(context.l10n.onboardingPermDeniedSnack)),
      );
    }
  }

  Future<void> _requestBle() async {
    if (!_isMobileNative) return;
    if (defaultTargetPlatform == TargetPlatform.iOS) {
      await Permission.bluetooth.request();
    } else {
      await Permission.bluetoothConnect.request();
      await Permission.bluetoothScan.request();
    }
    await _refreshBleStatus();
    if (!mounted) return;
    final ok = _bleGranted == true;
    if (!ok) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(context.l10n.onboardingPermDeniedSnack)),
      );
    }
  }

  Future<void> _requestWifi() async {
    if (_isAndroid) {
      final r = await Permission.nearbyWifiDevices.request();
      await _refreshWifiStatus();
      if (!mounted) return;
      if (!r.isGranted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(context.l10n.onboardingPermDeniedSnack)),
        );
      }
    }
  }

  void _ensurePermBootstrap() {
    if (_step != 1 || !_isMobileNative || _permBootstrapDone) return;
    _permBootstrapDone = true;
    WidgetsBinding.instance.addPostFrameCallback((_) async {
      await _refreshPhotoStatus();
      await _refreshBleStatus();
      if (_isAndroid) await _refreshWifiStatus();
    });
  }

  Future<void> _next() async {
    if (_step == 0) {
      await _persistName();
      setState(() => _step++);
      return;
    }
    if (_step == 1) {
      setState(() => _step++);
      return;
    }
    if (_step < _totalSteps - 1) {
      setState(() => _step++);
      return;
    }
    if (widget.onDone != null) {
      widget.onDone!();
    } else {
      Navigator.of(context).pop();
    }
  }

  Future<void> _finishOrSkip() async {
    if (_step == 0) await _persistName();
    if (!mounted) return;
    if (widget.onDone != null) {
      widget.onDone!();
    } else {
      Navigator.of(context).pop();
    }
  }

  Widget _buildNameStep(AppLocalizations l10n, ThemeData theme) {
    return LayoutBuilder(
      builder: (context, constraints) {
        return SingleChildScrollView(
          child: ConstrainedBox(
            constraints: BoxConstraints(minHeight: constraints.maxHeight),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Icon(Icons.person_outline,
                    size: 88, color: theme.colorScheme.primary),
                const SizedBox(height: 24),
                Text(
                  l10n.onboardingYourNameTitle,
                  style: theme.textTheme.headlineSmall
                      ?.copyWith(fontWeight: FontWeight.bold),
                  textAlign: TextAlign.center,
                ),
                const SizedBox(height: 12),
                Text(
                  l10n.onboardingYourNameSubtitle,
                  textAlign: TextAlign.center,
                  style: theme.textTheme.bodyMedium?.copyWith(
                    color: theme.colorScheme.onSurfaceVariant,
                  ),
                ),
                const SizedBox(height: 28),
                TextField(
                  controller: _nameCtrl,
                  textInputAction: TextInputAction.done,
                  textCapitalization: TextCapitalization.words,
                  decoration: InputDecoration(
                    labelText: l10n.onboardingNameHint,
                    border: const OutlineInputBorder(),
                  ),
                  onSubmitted: (_) => unawaited(_next()),
                ),
              ],
            ),
          ),
        );
      },
    );
  }

  Widget _buildPermissionsStep(AppLocalizations l10n, ThemeData theme) {
    return SingleChildScrollView(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Icon(Icons.shield_outlined,
              size: 72, color: theme.colorScheme.primary),
          const SizedBox(height: 16),
          Text(
            l10n.onboardingPermissionsTitle,
            style: theme.textTheme.headlineSmall
                ?.copyWith(fontWeight: FontWeight.bold),
            textAlign: TextAlign.center,
          ),
          const SizedBox(height: 8),
          Text(
            l10n.onboardingPermissionsSubtitle,
            textAlign: TextAlign.center,
            style: theme.textTheme.bodyMedium?.copyWith(
              color: theme.colorScheme.onSurfaceVariant,
            ),
          ),
          const SizedBox(height: 24),
          if (_isMobileNative) ...[
            _PermissionCard(
              icon: Icons.photo_library_outlined,
              title: l10n.onboardingPermPhotosTitle,
              body: l10n.onboardingPermPhotosBody,
              granted: _photosGranted,
              actionLabel: l10n.onboardingPermPhotosAllow,
              onAction: _requestPhotos,
              onOpenSettings: () => openAppSettings(),
              settingsLabel: l10n.onboardingOpenSettings,
              grantedLabel: l10n.onboardingPermGrantedShort,
            ),
            const SizedBox(height: 12),
            _PermissionCard(
              icon: Icons.bluetooth_searching,
              title: l10n.onboardingPermBleTitle,
              body: l10n.onboardingPermBleBody,
              granted: _bleGranted,
              actionLabel: l10n.onboardingPermBleAllow,
              onAction: _requestBle,
              onOpenSettings: () => openAppSettings(),
              settingsLabel: l10n.onboardingOpenSettings,
              grantedLabel: l10n.onboardingPermGrantedShort,
            ),
            const SizedBox(height: 12),
            if (_isAndroid)
              _PermissionCard(
                icon: Icons.wifi,
                title: l10n.onboardingPermWifiTitle,
                body: l10n.onboardingPermWifiBodyAndroid,
                granted: _wifiGranted,
                actionLabel: l10n.onboardingPermWifiAllow,
                onAction: _requestWifi,
                onOpenSettings: () => openAppSettings(),
                settingsLabel: l10n.onboardingOpenSettings,
                grantedLabel: l10n.onboardingPermGrantedShort,
              )
            else
              _PermissionCard(
                icon: Icons.wifi,
                title: l10n.onboardingPermWifiTitle,
                body: l10n.onboardingPermWifiBodyIos,
                granted: null,
                actionLabel: '',
                onAction: null,
                onOpenSettings: null,
                settingsLabel: null,
                grantedLabel: null,
                infoOnly: true,
              ),
          ] else
            Text(
              l10n.onboardingPermissionsDesktopBody,
              textAlign: TextAlign.center,
              style: theme.textTheme.bodyMedium,
            ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final theme = Theme.of(context);
    final intros = _introSlides(l10n);
    final introIndex = _step - 2;

    _ensurePermBootstrap();

    late final Widget body;
    if (_step == 0) {
      body = _buildNameStep(l10n, theme);
    } else if (_step == 1) {
      body = _buildPermissionsStep(l10n, theme);
    } else {
      final slide = intros[introIndex.clamp(0, intros.length - 1)];
      body = LayoutBuilder(
        builder: (context, constraints) {
          return SingleChildScrollView(
            child: ConstrainedBox(
              constraints: BoxConstraints(minHeight: constraints.maxHeight),
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Icon(slide.icon, size: 100, color: slide.iconColor),
                  const SizedBox(height: 32),
                  Text(
                    slide.title,
                    style: theme.textTheme.headlineSmall
                        ?.copyWith(fontWeight: FontWeight.bold),
                    textAlign: TextAlign.center,
                  ),
                  const SizedBox(height: 16),
                  Text(
                    slide.body,
                    textAlign: TextAlign.center,
                    style: theme.textTheme.bodyLarge,
                  ),
                ],
              ),
            ),
          );
        },
      );
    }

    final lastStep = _step >= _totalSteps - 1;

    return Scaffold(
      appBar: AppBar(
        title: Text(l10n.onboardingWelcomeTitle),
        automaticallyImplyLeading: false,
        actions: [
          TextButton(
            onPressed: () => unawaited(_finishOrSkip()),
            child: Text(l10n.onboardingSkip),
          ),
        ],
      ),
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(
            children: [
              Expanded(child: body),
              const SizedBox(height: 16),
              Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: List.generate(
                  _totalSteps,
                  (i) => Container(
                    margin: const EdgeInsets.symmetric(horizontal: 4),
                    width: 10,
                    height: 10,
                    decoration: BoxDecoration(
                      shape: BoxShape.circle,
                      color: _step == i
                          ? theme.colorScheme.primary
                          : Colors.grey.withValues(alpha: 0.4),
                    ),
                  ),
                ),
              ),
              const SizedBox(height: 24),
              SizedBox(
                width: double.infinity,
                child: PressableScale(
                  child: FilledButton(
                    onPressed: () => unawaited(_next()),
                    style: FilledButton.styleFrom(
                        padding: const EdgeInsets.all(16)),
                    child: Text(
                        lastStep ? l10n.onboardingStart : l10n.onboardingNext),
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _IntroSlide {
  const _IntroSlide({
    required this.icon,
    required this.iconColor,
    required this.title,
    required this.body,
  });
  final IconData icon;
  final Color iconColor;
  final String title;
  final String body;
}

class _PermissionCard extends StatelessWidget {
  const _PermissionCard({
    required this.icon,
    required this.title,
    required this.body,
    this.granted,
    required this.actionLabel,
    required this.onAction,
    required this.onOpenSettings,
    required this.settingsLabel,
    required this.grantedLabel,
    this.infoOnly = false,
  });

  final IconData icon;
  final String title;
  final String body;
  final bool? granted;
  final String actionLabel;
  final VoidCallback? onAction;
  final VoidCallback? onOpenSettings;
  final String? settingsLabel;
  final String? grantedLabel;
  final bool infoOnly;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final ok = granted == true;
    return Card(
      elevation: 0,
      color: theme.colorScheme.surfaceContainerHighest.withValues(alpha: 0.35),
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(icon, color: theme.colorScheme.primary),
                const SizedBox(width: 10),
                Expanded(
                  child: Text(title, style: theme.textTheme.titleMedium),
                ),
                if (ok && grantedLabel != null)
                  Text(
                    grantedLabel!,
                    style: theme.textTheme.labelMedium?.copyWith(
                      color: theme.colorScheme.primary,
                      fontWeight: FontWeight.w600,
                    ),
                  ),
              ],
            ),
            const SizedBox(height: 8),
            Text(body, style: theme.textTheme.bodySmall),
            if (!infoOnly && onAction != null) ...[
              const SizedBox(height: 12),
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  FilledButton.tonal(
                    onPressed: ok ? null : onAction,
                    child: Text(actionLabel),
                  ),
                  if (!ok && onOpenSettings != null && settingsLabel != null)
                    TextButton(
                      onPressed: onOpenSettings,
                      child: Text(settingsLabel!),
                    ),
                ],
              ),
            ],
          ],
        ),
      ),
    );
  }
}
