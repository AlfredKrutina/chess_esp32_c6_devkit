import 'package:flutter/material.dart';

class LearnScreen extends StatelessWidget {
  const LearnScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Výuka šachu')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          _LearnCategory(
            title: 'Základy',
            color: Colors.green,
            lessons: const [
              _Lesson(title: 'Jak se pohybují figurky', description: 'Král, dáma, věž, střelec, jezdec, pěšák — naučte se pohyby každé figurky.', done: true),
              _Lesson(title: 'Cíl šachové hry', description: 'Šachmat, pat, remíza — co znamenají stavy konce partie?', done: true),
              _Lesson(title: 'Hodnota figurek', description: 'Pěšák=1, Jezdec/Střelec=3, Věž=5, Dáma=9 — proč na tom záleží?', done: false),
            ],
          ),
          _LearnCategory(
            title: 'Zvláštní tahy',
            color: Colors.blue,
            lessons: const [
              _Lesson(title: 'Rošáda', description: 'Velká a malá rošáda — jak, kdy a proč ji provádět.', done: false),
              _Lesson(title: 'En Passant', description: 'Zvláštní braní pěšákem — pravidlo, na které začátečníci zapomínají.', done: false),
              _Lesson(title: 'Promoce', description: 'Pěšák na osmé řadě — jak zvolit správnou figuru.', done: false),
            ],
          ),
          _LearnCategory(
            title: 'Taktiky',
            color: Colors.orange,
            lessons: const [
              _Lesson(title: 'Vidlička', description: 'Napadnout dvě figurky najednou — nejčastější taktický motiv.', done: false),
              _Lesson(title: 'Svázání', description: 'Figurka nemůže táhnout, protože by odkryla krále.', done: false),
              _Lesson(title: 'Šachová studie: Scholar\'s Mate', description: 'Šachmat ve 4 tazích — a jak mu čelit.', done: false),
            ],
          ),
          _LearnCategory(
            title: 'Strategie',
            color: Colors.purple,
            lessons: const [
              _Lesson(title: 'Ovládnutí středu', description: 'Proč jsou pole e4, d4, e5, d5 klíčová v zahájení?', done: false, locked: true),
              _Lesson(title: 'Bezpečnost krále', description: 'Rošáda čas jako pojistka — kdy a jak krále schovat.', done: false, locked: true),
              _Lesson(title: 'Endgame: Král a pěšák', description: 'Jak přeměnit výhodu pěšáka na výhru v koncovce.', done: false, locked: true),
            ],
          ),
          const SizedBox(height: 16),
          Container(
            padding: const EdgeInsets.all(16),
            decoration: BoxDecoration(
              color: Colors.amber.withOpacity(0.15),
              borderRadius: BorderRadius.circular(12),
              border: Border.all(color: Colors.amber.withOpacity(0.4)),
            ),
            child: const Row(
              children: [
                Icon(Icons.lightbulb_outline, color: Colors.amber),
                SizedBox(width: 12),
                Expanded(child: Text('Propojte šachovnici a využijte LED nápovědy přímo na desce při procvičování těchto lekcí.')),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _LearnCategory extends StatelessWidget {
  const _LearnCategory({required this.title, required this.lessons, required this.color});
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
              Text(title, style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 16)),
            ],
          ),
        ),
        ...lessons,
      ],
    );
  }
}

class _Lesson extends StatelessWidget {
  const _Lesson({required this.title, required this.description, this.done = false, this.locked = false});
  final String title;
  final String description;
  final bool done;
  final bool locked;

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: ListTile(
        leading: Icon(
          locked ? Icons.lock : done ? Icons.check_circle : Icons.radio_button_unchecked,
          color: locked ? Colors.grey : done ? Colors.green : Theme.of(context).colorScheme.primary,
        ),
        title: Text(title, style: TextStyle(color: locked ? Colors.grey : null)),
        subtitle: Text(description, maxLines: 2, overflow: TextOverflow.ellipsis, style: TextStyle(color: locked ? Colors.grey : null)),
        trailing: locked ? null : Icon(Icons.chevron_right, color: locked ? Colors.grey : null),
        onTap: locked ? null : () {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('Lekce: $title — připravujeme interaktivní obsah')),
          );
        },
      ),
    );
  }
}
