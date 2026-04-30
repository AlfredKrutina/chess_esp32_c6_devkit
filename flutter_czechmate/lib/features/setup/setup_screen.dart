import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../app_providers.dart';
import '../connection/board_session_notifier.dart';

class SetupScreen extends ConsumerWidget {
  const SetupScreen({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    return Scaffold(
      appBar: AppBar(title: const Text('Setup Režim')),
      body: Center(
        child: Padding(
          padding: const EdgeInsets.all(24.0),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              const Icon(Icons.dashboard_customize, size: 80, color: Colors.blueAccent),
              const SizedBox(height: 24),
              const Text('Rozestavení figurek', style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold)),
              const SizedBox(height: 16),
              const Text(
                'Přesuňte figurky na šachovnici libovolným způsobem. Deska i aplikace budou sledovat jejich polohu mimo standardní šachová pravidla. Pro ukončení Setup módu vyvolejte novou hru.',
                textAlign: TextAlign.center,
              ),
              const SizedBox(height: 32),
              FilledButton(
                onPressed: () => Navigator.pop(context),
                child: const Text('Vrátit se zpět'),
              )
            ],
          ),
        ),
      ),
    );
  }
}
