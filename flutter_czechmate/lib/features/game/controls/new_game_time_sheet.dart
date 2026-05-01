import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../../app_providers.dart';
import '../../../core/widgets/glass_snackbar.dart';
import '../../../core/constants/app_environment.dart';
import '../../../core/constants/chess_time_control_presets.dart';
import '../../connection/board_session_notifier.dart';
import '../../connection/board_session_state.dart';
import '../state/game_ui_notifier.dart';

/// Bottom sheet (iOS `NewGameSetupSheet` parity) — time control + `timer_config` + `new_game`.
Future<void> showNewGameWithTimeSheet(BuildContext context) async {
  await showModalBottomSheet<void>(
    context: context,
    isScrollControlled: true,
    showDragHandle: true,
    useSafeArea: true,
    builder: (ctx) {
      final h = MediaQuery.sizeOf(ctx).height * 0.9;
      return SizedBox(
        height: h,
        child: const NewGameTimeSheet(),
      );
    },
  );
}

class NewGameTimeSheet extends ConsumerStatefulWidget {
  const NewGameTimeSheet({super.key});

  @override
  ConsumerState<NewGameTimeSheet> createState() => _NewGameTimeSheetState();
}

class _NewGameTimeSheetState extends ConsumerState<NewGameTimeSheet> {
  bool _useCustom = false;
  ChessTimeControlPreset _preset = ChessTimeControlPreset.blitz5_0;
  double _customMin = 10;
  double _customInc = 0;
  bool _busy = false;

  @override
  void initState() {
    super.initState();
    final raw = ref.read(prefsRepositoryProvider).lastNewGameTimeControl;
    if (raw == null || raw.isEmpty) return;
    if (raw.startsWith('p:')) {
      final name = raw.substring(2);
      final p = ChessTimeControlPreset.tryParseName(name);
      if (p != null) {
        _preset = p;
        _useCustom = false;
      }
    } else if (raw.startsWith('c:')) {
      final parts = raw.split(':');
      if (parts.length >= 3) {
        _useCustom = true;
        _customMin = (int.tryParse(parts[1]) ?? 10).toDouble().clamp(1, 180);
        _customInc = (int.tryParse(parts[2]) ?? 0).toDouble().clamp(0, 60);
      }
    }
  }

  Future<void> _submit() async {
    final session = ref.read(boardSessionNotifierProvider);
    if (session.transport != BoardTransport.wifi &&
        session.transport != BoardTransport.ble &&
        session.transport != BoardTransport.mock) {
      if (mounted) {
        showGlassSnackBar(
          context,
          'Connect a board first (Wi‑Fi or Bluetooth).',
        );
      }
      return;
    }
    setState(() => _busy = true);
    final prefs = ref.read(prefsRepositoryProvider);
    final n = ref.read(boardSessionNotifierProvider.notifier);
    try {
      if (_useCustom) {
        final m = _customMin.round().clamp(1, 180);
        final inc = _customInc.round().clamp(0, 60);
        await n.startNewGameWithTimeControl(
          type: 14,
          customMinutes: m,
          customIncrement: inc,
        );
        await prefs.setLastNewGameTimeControl('c:$m:$inc');
      } else {
        await n.startNewGameWithTimeControl(type: _preset.firmwareType);
        await prefs.setLastNewGameTimeControl('p:${_preset.name}');
      }
      if (AppEnvironment.staging) {
        debugPrint('[staging] New game prefs saved: ${prefs.lastNewGameTimeControl}');
      }
      if (mounted) {
        Navigator.of(context).pop();
        showGlassSnackBar(context, 'New game started on the board.');
      }
    } catch (e) {
      if (mounted) {
        showGlassSnackBar(context, 'Error: $e');
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final boardUi = ref.watch(gameUiNotifierProvider);
    final boardUiN = ref.read(gameUiNotifierProvider.notifier);
    final kb = MediaQuery.viewInsetsOf(context).bottom;
    return Padding(
      padding: EdgeInsets.only(bottom: kb),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Expanded(
            child: SingleChildScrollView(
              padding: const EdgeInsets.fromLTRB(16, 8, 16, 8),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  Text('New game', style: Theme.of(context).textTheme.titleLarge),
                  const SizedBox(height: 4),
                  Text(
                    'Like on iOS: time control is sent to the board, then a new game starts.',
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                  const SizedBox(height: 14),
                  Text(
                    'Board view (which color is at the bottom)',
                    style: Theme.of(context).textTheme.titleSmall,
                  ),
                  const SizedBox(height: 6),
                  SegmentedButton<bool>(
                    segments: const [
                      ButtonSegment(value: false, label: Text('White bottom')),
                      ButtonSegment(value: true, label: Text('Black bottom')),
                    ],
                    selected: {boardUi.boardFlipped},
                    onSelectionChanged: _busy
                        ? null
                        : (s) {
                            if (s.isEmpty) return;
                            final want = s.first;
                            if (want != boardUi.boardFlipped) boardUiN.toggleFlip();
                          },
                  ),
                  const SizedBox(height: 4),
                  Text(
                    'Who starts is decided on the board / game rules; here you only flip the view (saved in Settings).',
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                  const SizedBox(height: 12),
                  SwitchListTile(
                    contentPadding: EdgeInsets.zero,
                    title: const Text('Custom time (type 14)'),
                    subtitle: Text(
                      _useCustom
                          ? '${_customMin.round()} min + ${_customInc.round()} s / move'
                          : 'Firmware presets',
                    ),
                    value: _useCustom,
                    onChanged: _busy
                        ? null
                        : (v) => setState(() {
                              _useCustom = v;
                            }),
                  ),
                  if (_useCustom) ...[
                    Text('Minutes per side: ${_customMin.round()}'),
                    Slider(
                      min: 1,
                      max: 180,
                      divisions: 179,
                      value: _customMin.clamp(1, 180),
                      onChanged: _busy
                          ? null
                          : (x) => setState(() => _customMin = x),
                    ),
                    Text('Increment (s): ${_customInc.round()}'),
                    Slider(
                      min: 0,
                      max: 60,
                      divisions: 60,
                      value: _customInc.clamp(0, 60),
                      onChanged: _busy
                          ? null
                          : (x) => setState(() => _customInc = x),
                    ),
                  ] else
                    ...ChessTimeControlPreset.values.map((p) {
                      return RadioListTile<ChessTimeControlPreset>(
                        dense: true,
                        value: p,
                        groupValue: _preset,
                        title: Text(p.title),
                        subtitle: Text(p.subtitle),
                        onChanged: _busy
                            ? null
                            : (v) {
                                if (v == null) return;
                                setState(() => _preset = v);
                              },
                      );
                    }),
                ],
              ),
            ),
          ),
          const Divider(height: 1),
          SafeArea(
            top: false,
            child: Padding(
              padding: const EdgeInsets.fromLTRB(16, 12, 16, 16),
              child: SizedBox(
                width: double.infinity,
                child: FilledButton(
                  onPressed: _busy ? null : _submit,
                  child: _busy
                      ? const SizedBox(
                          height: 22,
                          width: 22,
                          child: CircularProgressIndicator(strokeWidth: 2),
                        )
                      : const Text('Start on board'),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
