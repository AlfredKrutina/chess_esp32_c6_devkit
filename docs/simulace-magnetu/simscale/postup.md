# SimScale, jeden import

Soubor je `vsechny.step`. Šest kotoučů v jedné řadě, rozteč 35 mm. Pod každým, s mezerou 6 mm, leží koupený ocelový kotouč Ø 11×1 mm. Vzduch v souboru není, obálku doplní SimScale.

Jednotky jsou milimetry. Pěšec má mít průměr 10 mm. Když SimScale ukáže 10 m, soubor se načetl špatně.

## Kroky

1. Smaž starou geometrii. Soubor nepatří do Uploaded files. Klikni **+** u **Geometries** a přetáhni `vsechny.step`.
2. Create Simulation → Electromagnetics → Magnetostatics. Nech doplnit vzduch kolem celé řady.
3. Každý `*_plech`: Soft magnetic, Nonlinear isotropic, tabulka z `ocel_bh.csv`.
4. Magnety: Permanent magnet, permeabilita 1,05, směr (0, 0, 1). Remanence 1,30 T, věž 1,24 T.
5. Síť automatická, na kovu prvek 0,4 mm. Run.
6. Magnetic flux density, proudnice na řezu y = 0.

Sondy jsou 5 mm pod spodkem plechu a 3,13 mm mimo osu. Souřadnice jsou v `sondy.csv`.
