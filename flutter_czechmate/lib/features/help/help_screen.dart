import 'package:flutter/material.dart';

class HelpScreen extends StatelessWidget {
  const HelpScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('czechmate — help')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: const [
          _HelpSection(
            title: 'Connecting the board',
            items: [
              _HelpItem(
                q: 'How do I connect over Wi‑Fi?',
                a:
                    '1. Turn the board on.\n2. Join the board’s Wi‑Fi hotspot (e.g. CZECHMATE-AP).\n3. Open Board discovery and choose Wi‑Fi.\n4. Enter the board IP (default: http://192.168.4.1) and connect.',
              ),
              _HelpItem(
                q: 'How do I connect over Bluetooth?',
                a:
                    'In Board discovery tap Bluetooth and run a scan. Pick your board from the list — Bluetooth must be allowed in system settings.',
              ),
              _HelpItem(
                q: 'BLE scan doesn’t find the board',
                a:
                    'Make sure the board is on and within range (~10 m). Check that Bluetooth permission is granted for this app.',
              ),
              _HelpItem(
                q: 'What is the mock board?',
                a:
                    'Mock mode simulates a board for testing without hardware. Enable it from Board discovery → Demo (mock).',
              ),
              _HelpItem(
                q: 'Chess API / Lichess fails while on the board hotspot',
                a:
                    'The board’s hotspot usually has no internet. Turn off that Wi‑Fi or use mobile data for cloud services. Commands to the board’s LAN IP need your phone on the same network as the board.',
              ),
            ],
          ),
          _HelpSection(
            title: 'Playing a game',
            items: [
              _HelpItem(
                q: 'How do I move pieces from the app?',
                a:
                    'In Settings, turn on “Allow moves from app (remote)”. Then tap the from-square, then the to-square.',
              ),
              _HelpItem(
                q: 'How do I start a new game?',
                a:
                    'On Play, open Game controls (tune icon in the top bar) or use “New game” above the board and pick a time control.',
              ),
              _HelpItem(
                q: 'How do I flip the board (White / Black on bottom)?',
                a:
                    'Use Game controls or Settings → “Flip perspective (Black at bottom)”. Who moves first is decided by the board or your new-game choice.',
              ),
              _HelpItem(
                q: 'What do the LEDs mean?',
                a:
                    'Green: legal / suggested moves. Red: illegal move or conflict. Yellow: piece lifted.',
              ),
              _HelpItem(
                q: 'How do I open the game summary?',
                a:
                    'After the game ends it opens automatically if enabled in Settings. You can also open it from the chart icon in the app bar.',
              ),
            ],
          ),
          _HelpSection(
            title: 'Coach & analysis',
            items: [
              _HelpItem(
                q: 'How does AI Coach work?',
                a:
                    'Coach uses the live board position when connected. In Settings → Coach & AI, build a priority list (e.g. Groq → Google AI → OpenAI → Ollama). The app tries each provider in order until one answers. Each provider has its own fields (API key, model, or Ollama URL). Chat history is sent so follow-up questions stay in context.',
              ),
              _HelpItem(
                q: 'Can I use Coach without an API key?',
                a:
                    'Yes — clear the provider list (Offline only) or leave every key empty: you get short on-device tips. Add keys only for the backends you use.',
              ),
            ],
          ),
          _HelpSection(
            title: 'Settings & configuration',
            items: [
              _HelpItem(
                q: 'How do I set MQTT for Home Assistant?',
                a:
                    'Go to Settings → Smart home (MQTT). Enter broker host, port (default 1883), username, and password, then save to the board.',
              ),
              _HelpItem(
                q: 'What does Factory reset do?',
                a:
                    'It wipes board NVS — Wi‑Fi, MQTT, and stored preferences. The board restarts in access-point mode.',
              ),
              _HelpItem(
                q: 'How do I unlock developer mode?',
                a:
                    'Tap the Settings title in the app bar seven times quickly. A confirmation banner appears when it succeeds.',
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _HelpSection extends StatelessWidget {
  const _HelpSection({required this.title, required this.items});
  final String title;
  final List<_HelpItem> items;

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.symmetric(vertical: 12),
          child: Text(title,
              style: TextStyle(
                  fontWeight: FontWeight.bold,
                  fontSize: 16,
                  color: Theme.of(context).colorScheme.primary)),
        ),
        ...items,
        const Divider(),
      ],
    );
  }
}

class _HelpItem extends StatelessWidget {
  const _HelpItem({required this.q, required this.a});
  final String q;
  final String a;

  @override
  Widget build(BuildContext context) {
    return ExpansionTile(
      title: Text(q, style: const TextStyle(fontWeight: FontWeight.w500)),
      children: [
        Padding(
          padding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
          child: Text(a),
        ),
      ],
    );
  }
}
