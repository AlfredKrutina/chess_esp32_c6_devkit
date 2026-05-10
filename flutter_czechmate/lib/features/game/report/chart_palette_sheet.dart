import 'package:flutter/material.dart';

import '../../../core/models/chart_palette.dart';
import '../../../core/widgets/app_modal_sheet.dart';
import '../../../l10n/app_localizations.dart';

const _kSwatches = <Color>[
  Color(0xFFFF2D95),
  Color(0xFF00E5FF),
  Color(0xFFFFEA00),
  Color(0xFFB388FF),
  Color(0xFFFF6D00),
  Color(0xFFFF4081),
  Color(0xFFFFD740),
  Color(0xFF7C4DFF),
  Color(0xFF00E676),
  Color(0xFF2979FF),
  Color(0xFF69F0AE),
  Color(0xFF448AFF),
  Color(0xFF76FF03),
  Color(0xFFFF1744),
  Color(0xFF651FFF),
  Color(0xFFFFAB00),
];

Future<ChartPaletteColors?> showChartPaletteCustomizeSheet(
  BuildContext context, {
  required ChartPaletteColors initial,
  required AppLocalizations l10n,
}) {
  return showAppModalBottomSheet<ChartPaletteColors>(
    context: context,
    isScrollControlled: true,
    showDragHandle: true,
    builder: (ctx) {
      return _ChartPaletteSheetBody(initial: initial, l10n: l10n);
    },
  );
}

class _ChartPaletteSheetBody extends StatefulWidget {
  const _ChartPaletteSheetBody({
    required this.initial,
    required this.l10n,
  });

  final ChartPaletteColors initial;
  final AppLocalizations l10n;

  @override
  State<_ChartPaletteSheetBody> createState() => _ChartPaletteSheetBodyState();
}

class _ChartPaletteSheetBodyState extends State<_ChartPaletteSheetBody> {
  late ChartPaletteColors _c;

  @override
  void initState() {
    super.initState();
    _c = widget.initial;
  }

  static bool _lightFg(Color bg) => bg.computeLuminance() > 0.45;

  Widget _swatchRow(String label, Color current, void Function(Color) apply) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(label, style: Theme.of(context).textTheme.titleSmall),
        const SizedBox(height: 6),
        SingleChildScrollView(
          scrollDirection: Axis.horizontal,
          child: Row(
            children: [
              for (final col in _kSwatches)
                Padding(
                  padding: const EdgeInsets.only(right: 6, bottom: 4),
                  child: Material(
                    color: col,
                    shape: const CircleBorder(),
                    clipBehavior: Clip.antiAlias,
                    child: InkWell(
                      onTap: () => setState(() => apply(col)),
                      customBorder: const CircleBorder(),
                      child: SizedBox(
                        width: 32,
                        height: 32,
                        child: _sameColor(current, col)
                            ? Icon(
                                Icons.check,
                                size: 18,
                                color: _lightFg(col)
                                    ? Colors.black87
                                    : Colors.white,
                              )
                            : null,
                      ),
                    ),
                  ),
                ),
            ],
          ),
        ),
      ],
    );
  }

  @override
  Widget build(BuildContext context) {
    final l10n = widget.l10n;
    final pad = MediaQuery.paddingOf(context);
    return Padding(
      padding: EdgeInsets.only(
        left: 20,
        right: 20,
        top: 8,
        bottom: pad.bottom + 16,
      ),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Text(
            l10n.reportChartPaletteCustomTitle,
            style: Theme.of(context)
                .textTheme
                .titleMedium
                ?.copyWith(fontWeight: FontWeight.w700),
          ),
          const SizedBox(height: 8),
          Text(
            l10n.reportChartPaletteChoiceHint,
            style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: Theme.of(context).colorScheme.onSurfaceVariant,
                ),
          ),
          const SizedBox(height: 16),
          _swatchRow(l10n.reportChartPaletteEvalLine, _c.evalLine,
              (v) => _c = _c.copyWith(evalLine: v)),
          const SizedBox(height: 14),
          _swatchRow(
            l10n.reportChartPaletteCumulative,
            _c.cumulative,
            (v) => _c = _c.copyWith(cumulative: v),
          ),
          const SizedBox(height: 14),
          _swatchRow(l10n.reportChartPaletteBarWhite, _c.barWhite,
              (v) => _c = _c.copyWith(barWhite: v)),
          const SizedBox(height: 14),
          _swatchRow(l10n.reportChartPaletteBarBlack, _c.barBlack,
              (v) => _c = _c.copyWith(barBlack: v)),
          const SizedBox(height: 20),
          FilledButton(
            onPressed: () => Navigator.pop(context, _c),
            child: Text(l10n.reportChartPaletteDone),
          ),
        ],
      ),
    );
  }
}

bool _sameColor(Color a, Color b) =>
    (a.r - b.r).abs() +
        (a.g - b.g).abs() +
        (a.b - b.b).abs() +
        (a.a - b.a).abs() <
    0.004;
