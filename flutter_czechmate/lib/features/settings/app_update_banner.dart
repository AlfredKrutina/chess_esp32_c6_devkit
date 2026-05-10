import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:http/http.dart' as http;
import 'package:package_info_plus/package_info_plus.dart';
import 'package:url_launcher/url_launcher.dart';

import '../../app_providers.dart';
import '../../core/constants/app_update_defaults.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/models/app_update_manifest.dart';
import '../../core/models/board_firmware_models.dart';

class AppUpdateSuggestion {
  const AppUpdateSuggestion({
    required this.manifest,
    required this.currentVersion,
    required this.isBelowMinSupported,
  });

  final AppUpdateManifest manifest;
  final String currentVersion;
  final bool isBelowMinSupported;
}

Future<AppUpdateManifest?> _fetchAppUpdateManifest() async {
  try {
    final res = await http
        .get(Uri.parse(kAppUpdateJsonUrl))
        .timeout(const Duration(seconds: 10));
    if (res.statusCode != 200) {
      return null;
    }
    final decoded = jsonDecode(res.body);
    if (decoded is! Map<String, dynamic>) {
      return null;
    }
    final m = AppUpdateManifest.fromJson(decoded);
    return m.isValid ? m : null;
  } catch (_) {
    return null;
  }
}

final appUpdateSuggestionProvider =
    FutureProvider<AppUpdateSuggestion?>((ref) async {
  final remote = await _fetchAppUpdateManifest();
  if (remote == null) {
    return null;
  }
  final pkg = await PackageInfo.fromPlatform();
  final cur = pkg.version.trim();
  final latest = remote.latestVersion.trim();
  if (latest.isEmpty || compareSemverLoose(cur, latest) >= 0) {
    return null;
  }
  var belowMin = false;
  final min = remote.minSupportedVersion?.trim();
  if (min != null && min.isNotEmpty) {
    belowMin = compareSemverLoose(cur, min) < 0;
  }
  return AppUpdateSuggestion(
    manifest: remote,
    currentVersion: cur,
    isBelowMinSupported: belowMin,
  );
});

Future<void> launchAppReleasePage(AppUpdateManifest manifest) async {
  final raw =
      manifest.releasePageUrl?.trim().isNotEmpty == true
          ? manifest.releasePageUrl!.trim()
          : kDefaultAppReleasePageUrl;
  final uri = Uri.tryParse(raw);
  if (uri == null) {
    return;
  }
  if (await canLaunchUrl(uri)) {
    await launchUrl(uri, mode: LaunchMode.externalApplication);
  }
}

/// První karta v Nastavení — dostupná novější verze aplikace z GitHub Pages.
class AppUpdateSettingsCallout extends ConsumerWidget {
  const AppUpdateSettingsCallout({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final asyncSug = ref.watch(appUpdateSuggestionProvider);
    final dismissed =
        ref.watch(prefsRepositoryProvider).appUpdateBannerDismissedLatest;

    return asyncSug.when(
      loading: () => const SizedBox.shrink(),
      error: (_, __) => const SizedBox.shrink(),
      data: (sug) {
        if (sug == null) {
          return const SizedBox.shrink();
        }
        final latest = sug.manifest.latestVersion.trim();
        if (dismissed == latest) {
          return const SizedBox.shrink();
        }
        final l10n = context.l10n;
        final cs = Theme.of(context).colorScheme;
        final urgent = sug.isBelowMinSupported;
        final minRaw = sug.manifest.minSupportedVersion?.trim();
        final requiredFloor =
            minRaw != null && minRaw.isNotEmpty ? minRaw : latest;
        return Padding(
          padding: const EdgeInsets.only(bottom: 12),
          child: Material(
            color: urgent
                ? cs.errorContainer.withValues(alpha: 0.55)
                : cs.primaryContainer.withValues(alpha: 0.45),
            borderRadius: BorderRadius.circular(12),
            child: Padding(
              padding: const EdgeInsets.fromLTRB(14, 12, 8, 12),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Icon(
                        Icons.system_update_outlined,
                        color: urgent ? cs.onErrorContainer : cs.primary,
                      ),
                      const SizedBox(width: 10),
                      Expanded(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(
                              l10n.settingsAppUpdateBannerTitle,
                              style: Theme.of(context)
                                  .textTheme
                                  .titleSmall
                                  ?.copyWith(fontWeight: FontWeight.w700),
                            ),
                            const SizedBox(height: 6),
                            Text(
                              urgent
                                  ? l10n.settingsAppUpdateBannerSubtitleRequired(
                                      sug.currentVersion,
                                      requiredFloor,
                                    )
                                  : l10n.settingsAppUpdateBannerSubtitle(
                                      sug.currentVersion,
                                      latest,
                                    ),
                              style: Theme.of(context).textTheme.bodySmall,
                            ),
                          ],
                        ),
                      ),
                      IconButton(
                        tooltip: MaterialLocalizations.of(context)
                            .closeButtonTooltip,
                        onPressed: () async {
                          await ref
                              .read(prefsRepositoryProvider)
                              .setAppUpdateBannerDismissedLatest(latest);
                          ref.invalidate(prefsRepositoryProvider);
                        },
                        icon: const Icon(Icons.close),
                      ),
                    ],
                  ),
                  const SizedBox(height: 8),
                  Align(
                    alignment: Alignment.centerRight,
                    child: FilledButton(
                      onPressed: () => launchAppReleasePage(sug.manifest),
                      child: Text(l10n.settingsAppUpdateOpenDownloads),
                    ),
                  ),
                ],
              ),
            ),
          ),
        );
      },
    );
  }
}

/// Kompaktní řádek na stránce O aplikaci.
class AppUpdateAboutHint extends ConsumerWidget {
  const AppUpdateAboutHint({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final asyncSug = ref.watch(appUpdateSuggestionProvider);
    final dismissed =
        ref.watch(prefsRepositoryProvider).appUpdateBannerDismissedLatest;

    return asyncSug.when(
      loading: () => const SizedBox.shrink(),
      error: (_, __) => const SizedBox.shrink(),
      data: (sug) {
        if (sug == null) {
          return const SizedBox.shrink();
        }
        final latest = sug.manifest.latestVersion.trim();
        if (dismissed == latest) {
          return const SizedBox.shrink();
        }
        final l10n = context.l10n;
        return Padding(
          padding: const EdgeInsets.only(bottom: 12),
          child: Card(
            margin: EdgeInsets.zero,
            child: ListTile(
              leading: Icon(
                Icons.system_update_alt_outlined,
                color: Theme.of(context).colorScheme.primary,
              ),
              title: Text(l10n.settingsAboutAppUpdateLineTitle),
              subtitle: Text(
                l10n.settingsAboutAppUpdateLineBody(
                  sug.currentVersion,
                  latest,
                ),
              ),
              trailing: const Icon(Icons.open_in_new),
              onTap: () => launchAppReleasePage(sug.manifest),
            ),
          ),
        );
      },
    );
  }
}
