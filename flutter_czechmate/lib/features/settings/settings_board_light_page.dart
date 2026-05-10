import 'package:flutter/material.dart';

import '../../core/localization/context_l10n.dart';
import 'widgets/board_lamp_studio.dart';

class SettingsBoardLightPage extends StatelessWidget {
  const SettingsBoardLightPage({super.key});

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    return Scaffold(
      appBar: AppBar(title: Text(l10n.settingsTileBoardLightTitle)),
      body: ListView(
        padding: const EdgeInsets.fromLTRB(16, 20, 16, 32),
        children: const [
          BoardLampStudioPanel(),
        ],
      ),
    );
  }
}
