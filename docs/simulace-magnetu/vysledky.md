# Pole magnetů v mezeře Hall

Osově souměrný výpočet kotouče, magnetizace kolmo na desku. Sklo, plast i plíšek jsou nemagnetické, takže pole tvarují jen svou tloušťkou.

Od čela magnetu směrem k sondám:

| Vrstva | Tloušťka |
|---|---:|
| plast dna figurky | 1.2 mm |
| sklo | 4.0 mm |
| plíšek (díl Folie) | 1.0 mm |
| vzduch k rovině sond | 5.0 mm |
| celkem | 11.2 mm |

Sondy nejsou na ose. Pár má rozteč 6,267 mm a střed páru je střed pole, magnet stojí uprostřed. Každá sonda je tedy 3,13 mm od osy. Osa citlivosti DRV5055A1 je kolmá k desce, bere se Bz. Obě sondy vidí stejné Bz.

Br je střed katalogového okna: N42 = 1,30 T, N38 = 1,24 T. Model kotouče má μr = 1. Energie je dev₀² + dev₁² při 81,92 dílcích ADC na mT (A1, 100 mV/mT při 5 V, poměrné k napájení, 12bit ADC na stejném napájení). Nad ±21 mT senzor končí lineární rozsah.

Export `Sklo.stl` má tloušťku 2 mm. Tady je sklo 4 mm, jak je zadané. Plíšek 1 mm je výška dílu Folie.

Ocelový protiplech na zádech magnetu v tomhle poli není. Leží na opačné straně kotouče, ne mezi sklem a sondami.

| Figurka | Kotouč | Bz na ose | Bz na sondě | Br na sondě | Energie | Po ořezu A1 | Pásmo firmwaru |
|---|---|---:|---:|---:|---:|---:|---|
| pěšec | Ø 10×1 N42 | 7.9 mT | 6.8 mT | -2.4 mT | 627665 | 627665 | střelec |
| jezdec | Ø 10×2 N42 | 14.3 mT | 12.5 mT | -4.3 mT | 2085173 | 2085173 | dáma |
| střelec | Ø 10×3 N42 | 19.6 mT | 17.1 mT | -5.7 mT | 3942191 | 3942191 | král |
| věž | Ø 10×5 N38 | 26.3 mT | 23.2 mT | -7.4 mT | 7251928 | 5919002 | nad králem |
| dáma | Ø 10×8 N42 | 35.5 mT | 31.7 mT | -9.6 mT | 13491926 | 5919002 | nad králem |
| král | Ø 12×6 N42 | 40.8 mT | 36.7 mT | -10.9 mT | 18076263 | 5919002 | nad králem |

Naměřený pěšec bez plechu je 103 050. Pásma v `hall_i2c_squares.c` jsou z jiného měření (stohy malých kotoučů).

Obrázek u každé figurky je Bz v řezu mezerou a průběh Bz v rovině sond. Čísla průběhu jsou v `*_profil.csv`.
