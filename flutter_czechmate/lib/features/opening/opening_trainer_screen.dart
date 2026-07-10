import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/layout/form_factor.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/widgets/glass_snackbar.dart';
import '../connection/board_session_notifier.dart';
import 'opening_catalog_repository.dart';

class OpeningTrainerScreen extends ConsumerStatefulWidget {
  const OpeningTrainerScreen({super.key, required this.lineId, this.mode = 'learn'});

  final String lineId;
  final String mode;

  @override
  ConsumerState<OpeningTrainerScreen> createState() =>
      _OpeningTrainerScreenState();
}

class _OpeningTrainerScreenState extends ConsumerState<OpeningTrainerScreen> {
  OpeningLine? _line;
  String? _error;
  bool _busy = false;
  String _feedback = 'none';
  int _playerPly = 0;
  int _playerTotal = 0;

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    try {
      final line = await OpeningCatalogRepository().findById(widget.lineId);
      if (!mounted) return;
      if (line == null) {
        setState(() => _error = 'Opening not found');
        return;
      }
      setState(() {
        _line = line;
        _playerTotal = line.playerPlyIndices.length;
      });
      await _startLesson(line);
    } catch (e) {
      if (mounted) setState(() => _error = e.toString());
    }
  }

  Future<void> _startLesson(OpeningLine line) async {
    setState(() => _busy = true);
    try {
      await ref
          .read(boardSessionNotifierProvider.notifier)
          .postOpeningAction(line.toStartPayload(mode: widget.mode));
      if (mounted) {
        showGlassSnackBar(context, 'Lekce spuštěna — táhni na desce');
      }
    } catch (e) {
      if (mounted) setState(() => _error = e.toString());
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _hint() async {
    final n = ref.read(boardSessionNotifierProvider.notifier);
    await n.postOpeningRaw({'action': 'hint'});
  }

  Future<void> _cancel() async {
    final n = ref.read(boardSessionNotifierProvider.notifier);
    await n.postOpeningRaw({'action': 'cancel'});
    if (mounted) Navigator.of(context).pop();
  }

  Future<void> _checkpointAck() async {
    final n = ref.read(boardSessionNotifierProvider.notifier);
    await n.postOpeningRaw({'action': 'checkpoint_ack'});
    if (mounted) {
      showGlassSnackBar(context, 'Deska srovnaná — pokračuj');
    }
  }

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    final session = ref.watch(boardSessionNotifierProvider);
    final opening = session.snapshot?.status.openingTraining;
    if (opening != null) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        setState(() {
          _feedback = opening.feedback ?? 'none';
          _playerPly = opening.playerPlyIndex ?? 0;
          _playerTotal = opening.playerPlyTotal ?? _playerTotal;
        });
      });
    }

    final line = _line;
    return Scaffold(
      appBar: AppBar(
        title: Text(line?.nameForLocale(Localizations.localeOf(context).languageCode) ??
            l10n.learnAppBarTitle),
        actions: [
          IconButton(onPressed: _busy ? null : _cancel, icon: const Icon(Icons.close)),
        ],
      ),
      body: desktopFormDetailBody(
        Padding(
          padding: const EdgeInsets.all(16),
          child: _error != null
              ? Text(_error!, style: const TextStyle(color: Colors.redAccent))
              : line == null
                  ? const Center(child: CircularProgressIndicator())
                  : Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        Text('ECO ${line.eco} · ${line.side}',
                            style: Theme.of(context).textTheme.titleSmall),
                        const SizedBox(height: 8),
                        if (line.ideaCs != null)
                          Text(line.ideaCs!, style: Theme.of(context).textTheme.bodyLarge),
                        const SizedBox(height: 16),
                        LinearProgressIndicator(
                          value: _playerTotal == 0
                              ? null
                              : (_playerPly + 1) / _playerTotal,
                        ),
                        const SizedBox(height: 8),
                        Text('Tah hráče ${_playerPly + 1} / $_playerTotal'),
                        Text('Stav: $_feedback'),
                        const Spacer(),
                        if (_feedback == 'checkpoint')
                          FilledButton(
                            onPressed: _busy ? null : _checkpointAck,
                            child: const Text('Deska srovnaná — pokračovat'),
                          ),
                        const SizedBox(height: 8),
                        OutlinedButton(
                          onPressed: _busy ? null : _hint,
                          child: const Text('LED nápověda'),
                        ),
                        if (_feedback == 'complete')
                          Padding(
                            padding: const EdgeInsets.only(top: 12),
                            child: FilledButton(
                              onPressed: () => Navigator.of(context).pop(),
                              child: const Text('Hotovo'),
                            ),
                          ),
                      ],
                    ),
        ),
      ),
    );
  }
}
