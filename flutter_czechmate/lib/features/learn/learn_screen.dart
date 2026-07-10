import 'package:flutter/material.dart';

import '../../core/layout/form_factor.dart';
import '../../core/localization/context_l10n.dart';
import '../../core/widgets/glass_snackbar.dart';
import '../opening/opening_catalog_screen.dart';
import '../opening/opening_trainer_screen.dart';

class LearnScreen extends StatelessWidget {
  const LearnScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    return Scaffold(
      appBar: AppBar(title: Text(l10n.learnAppBarTitle)),
      body: desktopFormDetailBody(
        ListView(
          padding: const EdgeInsets.all(16),
          children: [
            Card(
              margin: const EdgeInsets.only(bottom: 12),
              child: ListTile(
                leading: Icon(Icons.menu_book,
                    color: Theme.of(context).colorScheme.primary),
                title: const Text('Trénink zahájení'),
                subtitle: const Text('10 linií — Učení, Drill, Na čas, Mirror'),
                trailing: const Icon(Icons.chevron_right),
                onTap: () {
                  Navigator.of(context).push(
                    MaterialPageRoute<void>(
                      builder: (_) => const OpeningCatalogScreen(),
                    ),
                  );
                },
              ),
            ),
            _LearnCategory(
              title: l10n.learnSecBasics,
              color: Colors.green,
              lessons: [
                _Lesson(
                    title: l10n.learnL1Title,
                    description: l10n.learnL1Desc,
                    done: true),
                _Lesson(
                    title: l10n.learnL2Title,
                    description: l10n.learnL2Desc,
                    done: true),
                _Lesson(
                    title: l10n.learnL3Title,
                    description: l10n.learnL3Desc,
                    done: false),
              ],
            ),
            _LearnCategory(
              title: l10n.learnSecSpecial,
              color: Colors.blue,
              lessons: [
                _Lesson(
                    title: l10n.learnL4Title,
                    description: l10n.learnL4Desc,
                    done: false),
                _Lesson(
                    title: l10n.learnL5Title,
                    description: l10n.learnL5Desc,
                    done: false),
                _Lesson(
                    title: l10n.learnL6Title,
                    description: l10n.learnL6Desc,
                    done: false),
              ],
            ),
            _LearnCategory(
              title: l10n.learnSecTactics,
              color: Colors.orange,
              lessons: [
                _Lesson(
                    title: l10n.learnL7Title,
                    description: l10n.learnL7Desc,
                    done: false),
                _Lesson(
                    title: l10n.learnL8Title,
                    description: l10n.learnL8Desc,
                    done: false),
                _Lesson(
                    title: l10n.learnL9Title,
                    description: l10n.learnL9Desc,
                    done: false),
              ],
            ),
            _LearnCategory(
              title: l10n.learnSecStrategy,
              color: Colors.purple,
              lessons: [
                _Lesson(
                    title: l10n.learnL10Title,
                    description: l10n.learnL10Desc,
                    done: false,
                    openingId: 'italian_giuoco_white'),
                _Lesson(
                    title: l10n.learnL11Title,
                    description: l10n.learnL11Desc,
                    done: false,
                    locked: true),
                _Lesson(
                    title: l10n.learnL12Title,
                    description: l10n.learnL12Desc,
                    done: false,
                    openingId: 'spanish_berlin_white'),
              ],
            ),
            const SizedBox(height: 16),
            Container(
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                color: Colors.amber.withValues(alpha: 0.15),
                borderRadius: BorderRadius.circular(12),
                border: Border.all(color: Colors.amber.withValues(alpha: 0.4)),
              ),
              child: Row(
                children: [
                  const Icon(Icons.lightbulb_outline, color: Colors.amber),
                  const SizedBox(width: 12),
                  Expanded(child: Text(l10n.learnBoardLedHint)),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _LearnCategory extends StatelessWidget {
  const _LearnCategory(
      {required this.title, required this.lessons, required this.color});
  final String title;
  final List<_Lesson> lessons;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.fromLTRB(0, 20, 0, 8),
          child: Row(
            children: [
              Container(width: 4, height: 20, color: color),
              const SizedBox(width: 8),
              Text(title,
                  style: const TextStyle(
                      fontWeight: FontWeight.bold, fontSize: 16)),
            ],
          ),
        ),
        ...lessons,
      ],
    );
  }
}

class _Lesson extends StatelessWidget {
  const _Lesson({
    required this.title,
    required this.description,
    this.done = false,
    this.locked = false,
    this.openingId,
  });
  final String title;
  final String description;
  final bool done;
  final bool locked;
  final String? openingId;

  @override
  Widget build(BuildContext context) {
    final l10n = context.l10n;
    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: ListTile(
        leading: Icon(
          locked
              ? Icons.lock
              : done
                  ? Icons.check_circle
                  : Icons.radio_button_unchecked,
          color: locked
              ? Colors.grey
              : done
                  ? Colors.green
                  : Theme.of(context).colorScheme.primary,
        ),
        title:
            Text(title, style: TextStyle(color: locked ? Colors.grey : null)),
        subtitle: Text(description,
            maxLines: 2,
            overflow: TextOverflow.ellipsis,
            style: TextStyle(color: locked ? Colors.grey : null)),
        trailing: locked ? null : const Icon(Icons.chevron_right),
        onTap: locked
            ? null
            : () {
                if (openingId != null) {
                  Navigator.of(context).push(
                    MaterialPageRoute<void>(
                      builder: (_) => OpeningTrainerScreen(lineId: openingId!),
                    ),
                  );
                  return;
                }
                showAppSnackBar(context, l10n.learnSnackLesson(title));
              },
      ),
    );
  }
}
