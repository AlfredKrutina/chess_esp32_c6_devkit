import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

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
    final st = session.snapshot!.status;
    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('Světlo desky', style: Theme.of(context).textTheme.titleSmall),
            Text(
              '${st.lightMode ?? "?"} • ${st.lightState == true ? "zapnuto" : "vypnuto"} • jas ${st.brightness ?? "?"} %',
              style: Theme.of(context).textTheme.bodySmall,
            ),
            const SizedBox(height: 8),
            OutlinedButton.icon(
              onPressed: () => Navigator.push(
                context,
                MaterialPageRoute<void>(
                  builder: (_) => Scaffold(
                    appBar: AppBar(title: const Text('Lampa — detail')),
                    body: const SingleChildScrollView(
                      padding: EdgeInsets.all(16),
                      child: BoardLampBlock(showTitle: true),
                    ),
                  ),
                ),
              ),
              icon: const Icon(Icons.tune),
              label: const Text('Podrobnosti lampy'),
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
    String successMessage = 'Příkaz lampy odeslán',
  }) async {
    final s = ref.read(boardSessionNotifierProvider);
    final w = s.wifiBaseUrl?.trim();
    final can = (s.transport == BoardTransport.wifi &&
            w != null &&
            w.isNotEmpty) ||
        s.transport == BoardTransport.ble;
    if (!can) return;
    setState(() => _busy = true);
    try {
      await fn();
      await ref.read(boardSessionNotifierProvider.notifier).refreshNow();
      if (mounted) {
        _syncFromSnapshot();
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(successMessage)),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('$e')));
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
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
          const Text('Lampa', style: TextStyle(fontWeight: FontWeight.bold)),
          const SizedBox(height: 8),
        ],
        if (st != null)
          Text(
            'Stav: ${st.lightMode ?? "?"}, RGB(${st.lightR ?? 0},${st.lightG ?? 0},${st.lightB ?? 0}), '
            'limit nápověd: ${st.chessHintLimit ?? "?"}, auto zhasnutí: ${st.autoLampTimeoutSec ?? "?"} s',
            style: Theme.of(context).textTheme.bodySmall,
          ),
        if (!can)
          const Padding(
            padding: EdgeInsets.only(top: 8),
            child: Text('Vyžaduje připojení k desce (Wi‑Fi nebo Bluetooth).'),
          )
        else ...[
          SwitchListTile(
            title: const Text('Lampa zapnutá'),
            value: _lampOn,
            onChanged: _busy
                ? null
                : (v) => setState(() {
                      _lampOn = v;
                    }),
          ),
          Text('Jas ${_bright.round()} %'),
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
                      successMessage: 'Jas lampy odeslán',
                    ),
            child: const Text('Odeslat jas'),
          ),
          const Divider(height: 24),
          Text('Barva (R G B) — POST /api/light/command'),
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
                      successMessage: 'Barva a stav lampy odeslány',
                    ),
            child: const Text('Odeslat barvu / stav lampy'),
          ),
          const SizedBox(height: 8),
          OutlinedButton(
            onPressed: _busy
                ? null
                : () => _run(
                      () => ref
                          .read(boardSessionNotifierProvider.notifier)
                          .postBoardLightGameMode(),
                      successMessage: 'Režim lampy podle partie nastaven',
                    ),
            child: const Text('Režim hry (LED podle partie)'),
          ),
        ],
      ],
    );
  }
}
