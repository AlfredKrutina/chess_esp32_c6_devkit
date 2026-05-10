import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../core/localization/context_l10n.dart';
import '../../../core/widgets/glass_snackbar.dart';
import '../../../core/utils/user_facing_error_message.dart';
import '../../connection/board_session_notifier.dart';
import '../../connection/board_session_state.dart';

/// Rychlý pruh jasu + odkaz na detail (parita `BoardLampQuickStrip`).
class BoardLampQuickStrip extends ConsumerWidget {
  const BoardLampQuickStrip({super.key});

  static bool _visible(BoardSessionState s) {
    if (s.snapshot == null) return false;
    final w = s.wifiBaseUrl?.trim();
    if (s.transport == BoardTransport.wifi && w != null && w.isNotEmpty) {
      return true;
    }
    return s.transport == BoardTransport.ble;
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final session = ref.watch(boardSessionNotifierProvider);
    if (!_visible(session)) return const SizedBox.shrink();
    final l10n = context.l10n;
    final st = session.snapshot!.status;
    final onOff = st.lightState == true ? l10n.boardDemoOn : l10n.boardDemoOff;
    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(l10n.lampBoardLightTitle,
                style: Theme.of(context).textTheme.titleSmall),
            Text(
              '${st.lightMode ?? "?"} • $onOff • ${l10n.lampBrightnessLabel("${st.brightness ?? "?"}")}',
              style: Theme.of(context).textTheme.bodySmall,
            ),
            const SizedBox(height: 8),
            OutlinedButton.icon(
              onPressed: () => Navigator.push(
                context,
                MaterialPageRoute<void>(
                  builder: (ctx) => Scaffold(
                    appBar: AppBar(title: Text(ctx.l10n.lampDetailTitle)),
                    body: const SingleChildScrollView(
                      padding: EdgeInsets.all(16),
                      child: BoardLampBlock(showTitle: true),
                    ),
                  ),
                ),
              ),
              icon: const Icon(Icons.tune),
              label: Text(l10n.lampDetailsButton),
            ),
          ],
        ),
      ),
    );
  }
}

/// Plný panel lampy — jas, RGB, režim hry (HTTP).
class BoardLampBlock extends ConsumerStatefulWidget {
  const BoardLampBlock({super.key, this.showTitle = true});

  final bool showTitle;

  @override
  ConsumerState<BoardLampBlock> createState() => _BoardLampBlockState();
}

class _BoardLampBlockState extends ConsumerState<BoardLampBlock> {
  double _bright = 50;
  double _r = 255;
  double _g = 255;
  double _b = 255;
  bool _lampOn = true;
  bool _busy = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) => _syncFromSnapshot());
  }

  void _syncFromSnapshot() {
    final s = ref.read(boardSessionNotifierProvider).snapshot?.status;
    if (s == null) return;
    setState(() {
      if (s.brightness != null) _bright = s.brightness!.toDouble();
      if (s.lightR != null) _r = s.lightR!.toDouble();
      if (s.lightG != null) _g = s.lightG!.toDouble();
      if (s.lightB != null) _b = s.lightB!.toDouble();
      if (s.lightState != null) _lampOn = s.lightState!;
    });
  }

  Future<void> _run(
    Future<void> Function() fn, {
    required String successMessage,
  }) async {
    final s = ref.read(boardSessionNotifierProvider);
    final w = s.wifiBaseUrl?.trim();
    final can =
        (s.transport == BoardTransport.wifi && w != null && w.isNotEmpty) ||
            s.transport == BoardTransport.ble;
    if (!can) return;
    setState(() => _busy = true);
    try {
      await fn();
      await ref.read(boardSessionNotifierProvider.notifier).refreshNow();
      if (mounted) {
        _syncFromSnapshot();
        showAppSnackBar(context, successMessage);
      }
    } catch (e) {
      if (mounted) {
        final l10n = context.l10n;
        showAppSnackBar(
          context,
          userFacingErrorSummary(l10n, e),
          errorStyle: true,
        );
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final session = ref.watch(boardSessionNotifierProvider);
    final st = session.snapshot?.status;
    final w = session.wifiBaseUrl?.trim();
    final can = (session.transport == BoardTransport.wifi &&
            w != null &&
            w.isNotEmpty) ||
        session.transport == BoardTransport.ble;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        if (widget.showTitle) ...[
          Text(l10n.lampHeader,
              style: const TextStyle(fontWeight: FontWeight.bold)),
          const SizedBox(height: 8),
        ],
        if (st != null)
          Text(
            l10n.lampBlockStatusSummary(
              st.lightMode ?? "?",
              '${st.lightR ?? 0}',
              '${st.lightG ?? 0}',
              '${st.lightB ?? 0}',
              '${st.chessHintLimit ?? "?"}',
              '${st.autoLampTimeoutSec ?? "?"}',
            ),
            style: Theme.of(context).textTheme.bodySmall,
          ),
        if (!can)
          Padding(
            padding: const EdgeInsets.only(top: 8),
            child: Text(l10n.lampNeedsConnection),
          )
        else ...[
          SwitchListTile(
            title: Text(l10n.lampOnTitle),
            value: _lampOn,
            onChanged: _busy
                ? null
                : (v) => setState(() {
                      _lampOn = v;
                    }),
          ),
          Text(l10n.lampBrightnessLabel('${_bright.round()}')),
          Slider(
            min: 0,
            max: 100,
            divisions: 20,
            value: _bright.clamp(0, 100),
            onChanged: _busy ? null : (v) => setState(() => _bright = v),
          ),
          FilledButton.tonal(
            onPressed: _busy
                ? null
                : () => _run(
                      () => ref
                          .read(boardSessionNotifierProvider.notifier)
                          .postBoardBrightness(_bright.round()),
                      successMessage: l10n.lampBrightnessSentSnack,
                    ),
            child: Text(l10n.lampSendBrightness),
          ),
          const Divider(height: 24),
          Text(l10n.lampRgbHint),
          Row(
            children: [
              Expanded(child: Text('R ${_r.round()}')),
              Expanded(child: Text('G ${_g.round()}')),
              Expanded(child: Text('B ${_b.round()}')),
            ],
          ),
          Slider(
            min: 0,
            max: 255,
            value: _r.clamp(0, 255),
            onChanged: _busy ? null : (v) => setState(() => _r = v),
            label: 'R',
          ),
          Slider(
            min: 0,
            max: 255,
            value: _g.clamp(0, 255),
            onChanged: _busy ? null : (v) => setState(() => _g = v),
          ),
          Slider(
            min: 0,
            max: 255,
            value: _b.clamp(0, 255),
            onChanged: _busy ? null : (v) => setState(() => _b = v),
          ),
          FilledButton.tonal(
            onPressed: _busy
                ? null
                : () => _run(
                      () => ref
                          .read(boardSessionNotifierProvider.notifier)
                          .postBoardLightCommand(
                            lampState: _lampOn,
                            r: _r.round(),
                            g: _g.round(),
                            b: _b.round(),
                          ),
                      successMessage: l10n.lampColorStateSentSnack,
                    ),
            child: Text(l10n.lampSendColor),
          ),
          const SizedBox(height: 8),
          OutlinedButton(
            onPressed: _busy
                ? null
                : () => _run(
                      () => ref
                          .read(boardSessionNotifierProvider.notifier)
                          .postBoardLightGameMode(),
                      successMessage: l10n.lampStudioGameModeOk,
                    ),
            child: Text(l10n.lampGameModeButton),
          ),
        ],
      ],
    );
  }
}
