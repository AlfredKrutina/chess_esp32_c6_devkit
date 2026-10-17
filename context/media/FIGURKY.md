# Figurky — Cybertruck sada

Low-poly figurky pro tisk i pro filmy. Veřejná dokumentace tohle nemá; zdroj pravdy je OpenSCAD.

- Model: `models/chess_pieces/cybertruck_pieces.scad`
- Tisknutelné STL: `models/chess_pieces/stl/` (`pawn`, `rook`, `knight`, `bishop`, `queen`, `king`, `set`)
- Jezdec do Blenderu: `models/blender/meshes/knight_low.stl` (part `knight_low`)
- Hladší řada vedle, ve filmech se nepoužívá: `models/chess_pieces/organic_cyber_pieces.scad` a `stl/organic/`
- Scéna: `models/blender/build_scene.py` (`load_piece`, materiál `printed`), `models/blender/cine_common.py` (`piece_materials`, `piece_templates`, `make_piece`)

## Tvar

Tělo není socha. Je to sled komolých kuželů (`frustum`) mezi osmiúhelníkem patky (`body_fn = 8`) a šestiúhelníkem hlavy (`head_fn = 6`).

Tisk základnou dolů, bez podpěr. Převis nejvýš 40° od svislice: při rozšiřování musí být výška ≥ (d1 − d0) / 2 / tan(40°). Zužování a svislá stěna jsou v pořádku. Žádný objem nezačíná ve vzduchu. Hroty jsou tupé, alespoň 1,6 mm. Dřík na úchop má napříč alespoň 13 mm.

| Figurka | Patka | Čtení tvaru |
|---|---|---|
| Pěšec | 28 mm, výška 45 mm | osmihran, nahoře šestihranná hlava |
| Věž | 28 mm | hranol, čtyři úzká cimbuří |
| Střelec | 28 mm | štíhlý dřík, šestihranná mitra se zářezem (mitra je třetina výšky) |
| Dáma | 34 mm | koruna z hrotů |
| Král | 34 mm, kříž sahá zhruba na 94 mm | osm cimbuří a kříž (dřík ční nad ramena) |
| Jezdec | 28 mm | výjimka, viz níž |

Širší patka u dámy a krále je kvůli stabilitě. Král 80 mm na patce 28 mm má mez překlopení jen kolem 24°. Hmotnost je ve patce.

Jezdec ve scéně není tisknutelný `knight`. Ten má uši, oči a hubu. Film bere `knight_low`: jedna nakloněná hlava, úzký pas, bez uší, huby a očí. Čumák přečnívá, takže bez podpěr se netiskne. Černý jezdec se ve scéně otočí o 180°, ať čumáky stojí proti sobě.

`knight_sketch` je plochý výtlaček profilu v `models/chess_pieces/archive/`. Do scény nepatří.

## Magnety (rozpoznání figurky)

Rozpoznání nedělá tvar, ale magnety v patce. Hall V2: dva analogové senzory na pole, rozteč `sensor_pitch` 6,267 mm, citlivá osa kolmo k desce. Senzor je asi 7 mm pod horní hranou hrací plochy. Figurka stojí na té hraně, takže každý milimetr plastu pod magnetem zvětšuje vzduchovou mezeru. Dno dutiny je proto jen 0,8 mm (čtyři vrstvy 0,2 mm).

Dutina: disk 6 mm + vůle, 6,2 × 2,2 mm. Slicer pauza `M600` ve výšce `magnet_z_offset + magnet_thickness` = 3,0 mm. Všechny dutiny jedné figurky jsou v téže výšce — na pauze se vloží všechny její magnety najednou, pak tisk strop zavře.

Polarita (značka na magnetu dolů, k desce): +1 sever dolů, −1 jih dolů. Kódy jsou zvolené tak, aby se mraky Hall senzorů nepřekrývaly při mezeře 6–8 mm.

| Figurka | Magnety | Otočení na desce |
|---|---|---|
| Pěšec | 1× střed, sever | libovolné |
| Věž | 1× střed, jih | libovolné |
| Jezdec | 1× vpravo, sever | čumák dopředu nebo dozadu; bokem splyne s pěšcem |
| Střelec | vpravo sever + vlevo jih | čelem dopředu nebo dozadu; otočení o 90° se vyruší a pole je prázdné |
| Dáma | 4× na kříži, sever | libovolné |
| Král | 4× na kříži, jih | libovolné |

Dva 6,2 mm otvory vedle sebe potřebují mezeru. Senzory jsou 6,267 mm od sebe, takže dutina sedí 0,5 mm mimo střed senzoru a mezi otvory zbývá 1 mm stěny.

Ve filmu je polarita zjednodušená. `cine_common.py` dává bílé figurce sever k desce a černé jih, bez ohledu na typ. V záběru rentgenu se patka zprůhlední a disky se rozsvítí: sever červeně, jih modře. Disk ve scéně má sever na +Z a jih na −Z; bílá se proto otočí, černá zůstane.

## Ve scéně

STL jsou v milimetrech. `load_piece` je importuje s `global_scale = 0.001`. Šablony jsou schované; `make_piece` z nich dělá kopii meshe a přebarví slot materiálu na objektu (bílá / černá sdílí geometrii).

Materiál `printed` je matný FDM plast, ne kov. Jemné pruhy (`Wave`, směr Z, scale 900) a slabý bump, ať fazety pořád čtou jako tisk.

- Bílá: `(0.84, 0.83, 0.79)`, roughness 0,5, specular 0,35
- Černá: `(0.008, 0.008, 0.009)`, roughness 0,55, specular 0,4

Organic řada má hladké tělo (`rotate_extrude`), cyber detaily (V-drážka, zářez, pláty, kříž) a stejnou osmihrannou patku i magnety. Převis je přísnější (poloměr smí růst nejvýš o tan(28°) na milimetr). Filmy ji neberou.
