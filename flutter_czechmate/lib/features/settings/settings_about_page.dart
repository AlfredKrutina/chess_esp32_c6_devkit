import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:permission_handler/permission_handler.dart';

import '../../app_providers.dart';
import '../../core/localization/context_l10n.dart';

class SettingsAboutPage extends ConsumerStatefulWidget {
  const SettingsAboutPage({super.key});

  @override
  ConsumerState<SettingsAboutPage> createState() => _SettingsAboutPageState();
}

class _SettingsAboutPageState extends ConsumerState<SettingsAboutPage> {
  @override
  Widget build(BuildContext context) {
    final prefs = ref.watch(prefsRepositoryProvider);
    final l10n = context.l10n;
    final cs = Theme.of(context).colorScheme;

    return Scaffold(
      appBar: AppBar(title: Text(l10n.settingsTileAboutTitle)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 32),
        children: [
          Text(
            l10n.settingsTileAboutSubtitle,
            style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                  color: cs.onSurfaceVariant,
                ),
          ),
          const SizedBox(height: 16),
          FutureBuilder<PackageInfo>(
            future: PackageInfo.fromPlatform(),
            builder: (context, snap) {
              final v = snap.data;
              return Card(
                margin: EdgeInsets.zero,
                child: ListTile(
                  title: Text(l10n.gameAppBarTitle),
                  subtitle: Text(
                    v == null
                        ? l10n.settingsAboutVersionLoading
                        : l10n.settingsAboutVersionLine(
                            v.version, v.buildNumber),
                  ),
                  leading: const Icon(Icons.info_outline),
                ),
              );
            },
          ),
          const SizedBox(height: 12),
          Card(
            margin: EdgeInsets.zero,
            child: Column(
              children: [
                ListTile(
                  leading: const Icon(Icons.lock_outline),
                  title: Text(l10n.settingsPrivacyTitle),
                  subtitle: Text(l10n.settingsPrivacyBody),
                ),
                const Divider(height: 1),
                ListTile(
                  leading: const Icon(Icons.cloud_outlined),
                  title: Text(l10n.settingsIcloudTitle),
                  subtitle: Text(l10n.settingsIcloudBody),
                ),
              ],
            ),
          ),
          const SizedBox(height: 8),
          SwitchListTile(
            secondary: const Icon(Icons.live_tv_outlined),
            title: Text(l10n.settingsLiveActivityTitle),
            subtitle: Text(l10n.settingsLiveActivitySubtitle),
            value: prefs.liveActivityEnabled,
            onChanged: (v) async {
              if (v &&
                  !kIsWeb &&
                  defaultTargetPlatform == TargetPlatform.android) {
                await Permission.notification.request();
              }
              await prefs.setLiveActivityEnabled(v);
              if (v && !kIsWeb && defaultTargetPlatform == TargetPlatform.iOS) {
                final allowed = await ref
                    .read(liveActivityServiceProvider)
                    .iosLiveActivitiesAllowedByUser();
                if (!allowed && context.mounted) {
                  ScaffoldMessenger.of(context).showSnackBar(
                    SnackBar(
                      content: Text(l10n.settingsLiveActivityIosDisabledSnack),
                    ),
                  );
                }
              }
              if (!v) {
                await ref.read(liveActivityServiceProvider).endAll();
              }
              setState(() {});
            },
          ),
          if (!kIsWeb &&
              (defaultTargetPlatform == TargetPlatform.iOS ||
                  defaultTargetPlatform == TargetPlatform.android))
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 0, 16, 8),
              child: Text(
                l10n.settingsHomeScreenWidgetHint,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: cs.onSurfaceVariant,
                    ),
              ),
            ),
          if (!kIsWeb && defaultTargetPlatform == TargetPlatform.android)
            SwitchListTile(
              secondary: const Icon(Icons.watch_outlined),
              title: Text(l10n.settingsWearMirrorTitle),
              subtitle: Text(l10n.settingsWearMirrorSubtitle),
              value: prefs.wearDataLayerMirrorEnabled,
              onChanged: (v) async {
                if (v) {
                  await Permission.notification.request();
                }
                await prefs.setWearDataLayerMirrorEnabled(v);
                setState(() {});
              },
            ),
          if (!kIsWeb && defaultTargetPlatform == TargetPlatform.iOS)
            SwitchListTile(
              secondary: const Icon(Icons.watch_outlined),
              title: Text(l10n.settingsWatchMirrorTitle),
              subtitle: Text(l10n.settingsWatchMirrorSubtitle),
              value: prefs.watchCompanionMirrorEnabled,
              onChanged: (v) async {
                await prefs.setWatchCompanionMirrorEnabled(v);
                setState(() {});
              },
            ),
        ],
      ),
    );
  }
}
