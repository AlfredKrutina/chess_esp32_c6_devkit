import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:http/http.dart' as http;

import '../../app_providers.dart';
import '../connection/board_session_notifier.dart';

class CoachScreen extends ConsumerStatefulWidget {
  const CoachScreen({super.key});

  @override
  ConsumerState<CoachScreen> createState() => _CoachScreenState();
}

class _CoachScreenState extends ConsumerState<CoachScreen> {
  final _ctrl = TextEditingController();
  final _msgs = <String>[];
  bool _busy = false;

  Future<void> _ask() async {
    final snap = ref.read(boardSessionNotifierProvider).snapshot;
    final key = ref.read(prefsRepositoryProvider).coachApiKey;
    final q = _ctrl.text.trim();
    if (q.isEmpty) return;
    setState(() {
      _busy = true;
      _msgs.add('Ty: $q');
    });
    try {
      if (key != null && key.isNotEmpty) {
        final fenHint = snap != null ? 'Pozice (FEN zjednodušeně): ${snap.board}.' : '';
        final uri = Uri.parse('https://api.openai.com/v1/chat/completions');
        final res = await http.post(
          uri,
          headers: {
            'Authorization': 'Bearer $key',
            'Content-Type': 'application/json',
          },
          body: jsonEncode({
            'model': 'gpt-4o-mini',
            'messages': [
              {
                'role': 'system',
                'content':
                    'Jsi šachový trenér. Odpovídej stručně česky. $fenHint',
              },
              {'role': 'user', 'content': q},
            ],
          }),
        );
        if (res.statusCode != 200) {
          _msgs.add('Chyba API: ${res.statusCode} ${res.body}');
        } else {
          final map = jsonDecode(res.body) as Map<String, dynamic>;
          final choice = (map['choices'] as List).first as Map<String, dynamic>;
          final msg =
              (choice['message'] as Map<String, dynamic>)['content'] as String;
          _msgs.add('Coach: $msg');
        }
      } else {
        _msgs.add(
          'Coach: nastav API klíč v Nastavení, nebo použij sandbox v Hře. '
          'Lokální nápověda: zaměř se na vývoj figurek a bezpečnost krále.',
        );
      }
    } catch (e) {
      _msgs.add('Chyba: $e');
    } finally {
      setState(() => _busy = false);
    }
  }

  @override
  void dispose() {
    _ctrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Coach')),
      body: Column(
        children: [
          Expanded(
            child: ListView.builder(
              padding: const EdgeInsets.all(12),
              itemCount: _msgs.length,
              itemBuilder: (_, i) => Padding(
                padding: const EdgeInsets.only(bottom: 8),
                child: Text(_msgs[i]),
              ),
            ),
          ),
          Padding(
            padding: const EdgeInsets.all(8),
            child: Row(
              children: [
                Expanded(
                  child: TextField(
                    controller: _ctrl,
                    decoration: const InputDecoration(
                      hintText: 'Zeptej se na pozici / plán…',
                      border: OutlineInputBorder(),
                    ),
                    onSubmitted: (_) => _ask(),
                  ),
                ),
                const SizedBox(width: 8),
                FilledButton(
                  onPressed: _busy ? null : _ask,
                  child: _busy
                      ? const SizedBox(
                          width: 22,
                          height: 22,
                          child: CircularProgressIndicator(strokeWidth: 2),
                        )
                      : const Icon(Icons.send),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}
