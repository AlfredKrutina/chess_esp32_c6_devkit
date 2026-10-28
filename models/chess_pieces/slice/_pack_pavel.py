import zipfile
from pathlib import Path

root = Path(r"d:\projects\Programming\Git\chess_esp32_c6_devkit")
stl = root / "models" / "chess_pieces" / "stl"
threemf = root / "models" / "chess_pieces" / "slice" / "white_h2d_pla.3mf"
out = root / "figurky_pavel_all.zip"

note = """Figurky — bílá strana, tisk na Bambu Lab H2D
================================================

Tiskni soubor white_h2d_pla.3mf. Otevři ho v Bambu Studiu a dej Print.
Neslicuj znovu. Pauzy na magnety jsou už v g-codu a nový slice by je smazal.

Tiskárna: Bambu Lab H2D, tryska 0,4 mm
Materiál: Bambu PLA Basic
Podložka: texturovaná PEI
Vrstva: 0,2 mm, bez podpěr
Odhad: asi 3 h 27 min, 98 g

Na každé pauze hlava odjede dozadu. Vlož kotouč a na displeji potvrď pokračování.
Bílá strana: severní pól kotouče směrem k desce.
Černá strana je stejný soubor, jen se kotouč vloží opačně (jižní pól k desce).

Pauza je ve chvíli, kdy je dutina hotová a strop se teprve zavře.

  2,4 mm   8 pěšců     kotouč Ø 10×1 mm
  3,4 mm   2 jezdci    kotouč Ø 10×2 mm
  4,4 mm   2 střelci   kotouč Ø 10×3 mm
  6,4 mm   2 věže      kotouč Ø 10×5 mm
  7,4 mm   1 král      kotouč Ø 12×6 mm
  9,4 mm   1 dáma      kotouč Ø 10×8 mm

Ve složce stl jsou jednotlivé figurky a deska white.stl (16 figurek, mezera 10 mm).
Ty slouží jen jako záloha. Tiskne se 3mf.
"""

with zipfile.ZipFile(out, "w", compression=zipfile.ZIP_DEFLATED) as z:
    z.writestr("figurky_pavel_all/JAK_TISKNOUT.txt", note.encode("utf-8"))
    z.write(threemf, "figurky_pavel_all/white_h2d_pla.3mf")
    for name in ("white.stl", "pawn.stl", "knight.stl", "bishop.stl", "rook.stl", "queen.stl", "king.stl"):
        z.write(stl / name, f"figurky_pavel_all/stl/{name}")

print(out, out.stat().st_size)
