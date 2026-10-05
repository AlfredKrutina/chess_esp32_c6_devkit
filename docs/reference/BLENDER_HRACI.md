# Hráči — dva lidé u CzechMate

Samostatný film. Nepřepisuje `predstaveni.mp4` ani `rozklad.mp4`.

Výstup: `models/blender/video/hraci.mp4`.
Builder: `models/blender/build_hraci.py`. Postavy skládá `make_players.py`.

## Proč ne fotorealistický člověk

Referenční záběry (okno, víno, ruka na lahvi, profil u stolu) nejsou fotografie.
Jsou to hladké kinematografické postavy: teplé okno z jedné strany, mělká
hloubka ostrosti, pleť bez pórů, oblek a svetřík, ruka v popředí.

To je cíl. MakeHuman je hladký člověk s vlastními prsty a oblečením, ne
sken pórů a ne cizí ruka přilepená na jiné tělo. EEVEE Random Walk podpovrch
neumí. Pleť v tomhle záběru patří do Cycles.

Blízký záběr očí a úst je místo, kde hladká postava přestane držet. Obličej
proto není hrdina záběru. Hlava je v profilu, v protisvětle, nebo mimo ostrost.
Ruce, stůl, šachovnice a okno ostrost mají.

## Světlo

Cycles, AgX, denoise. Jedno velké okno jako klíč (teplá plocha, ne tři studené
areály). Za sklem je HDRI západu slunce (Poly Haven, CC0). Ve dne nese barvu
místnosti, večer klesne na zbytek a svítí lampa.

Podpovrch pleti: Principled, Random Walk (Fixed Radius), váha 1, poloměr malý
(řádově 1–2 mm, červená dál než modrá). Silnější poloměr udělá vosk.
Ruce mají jemnou drsnost a slabý specular, ne mokrý plast.

Místnost je z CC0 skenů (Poly Haven), ne z krabic. Jídelní stůl a dvě židle,
parketová podlaha, štuk, modré lněné závěsy na tyči, rám okna se sklem,
pachira v rohu, odkládací stolek s vázou a závěsná lampa. Za oknem je
belfast_sunset, v záběru kopec. Obraz na stěně je krajina kloofendal:
původní textura rámu byla šedá produktová karta, tu film nepoužívá.
Šachovnice je CzechMate, figurky jsou ty z filmu, ne cizí chess set.

Naskenovaný stůl má desku 2,05 × 1,15 m. Šířka je 1,46 m, výška 0,74 m.
Hloubka je 0,60 m: při 0,82 m deska protínala břicho. Zmenšení je jen v ose Y,
takže nohy jsou v půdorysu mírně oválné. Židle jsou zmenšené na sedák pod
stehno a opěradlo za zády, tělo v nich nevězí. Ubrous z toho skenu je schovaný:
leží ve vlnách přes desku a šachovnice by jím procházela.

## Místa

Partie pokračuje. Mění se místnost, ne pravidla a ne rozestavění.

Střih je match cut na šachovnici: deska zůstane ve středu záběru, za ní se
vymění světlo a pozadí.

| Tahy | Místo | Světlo |
|---|---|---|
| 1. d4 d5, 2. c4 | Odpoledne u okna | Nízké teplé okno, závěs, prach ve vzduchu |
| 2… e6, 3. Nc3 Nf6 | Soumrak | Stejný stůl, slunce níž a červenější, okno hasne |
| 4. Bg5 Be7, 5. e3 O-O | Večer | Lampa nad stolem, okno je tma, západ za sklem jen doutná |

Široký záběr je ten od okna: dva profily, deska mezi nimi. Detail je na
pěšci 2. c4, kde má špetka kolem sebe místo. Jezdec na b1 stojí mezi věží
a střelcem, dlaň dospělé ruky se tam sousedů dotkne, a proto na něm detail není.

## Partie

Dámský gambit, odmítnutý. Každý tah je legální a navazuje.

1. d4 d5
2. c4 e6
3. Nc3 Nf6
4. Bg5 Be7
5. e3 O-O

Bílý sedí blíž kameře. Černý naproti. V jednu chvíli je ve vzduchu jen jedna
figurka. Než ruka vjede nad cizí kámen, je základna aspoň 90 mm nad deskou,
stejně jako ve představení. Sebrání v téhle pasáži není.

Odložená ruka leží dlaní na dřevě. Zápěstí je pokračování předloktí, prsty
jsou pokrčené a ukazováček nevyčnívá. Není to dráp visící nad hranou ani
ruka postavená na hranu.

Hráč, který táhne, nakloní hlavu ke kameni a sleduje ho i ve vzduchu. Na kraji desky ji navíc skloní k tomu sloupci. Druhý se přidá o zlomek později a volným ukazováčkem dvakrát klepne o stůl. Ruka se nejdřív zvedne od stolu, obloukem přejde nad desku s otevřenými prsty a sevře až na figurce. Stejně se vrací. Chytá třemi prsty: palec,
ukazováček a prostředníček na hlavě nebo na krčku. Prsteníček a malíček jsou
stočené do dlaně. Není to plochý dráp, kde
mají všechny prsty stejný ohyb. Každý tah je stejná špetka a liší se
dotažením, pár stupňů opozice palce a výškou úchopu. Figurku bere ta ruka,
na jejíž straně kámen stojí, aby předloktí nešlo přes hruď. Druhá ruka zůstane
odložená. Dlaň je proti políčku velká, proto je ruka na 80 % výchozího MakeHuman, prsty zůstávají. Dlaň míří dolů a konce palce, ukazováčku a prostředníčku sedí na hlavičce. Ukazováček na kámen nemíří, je ohnutý kolem něj.
Zápěstí zůstává nad kameny, aby předloktí neprojelo řadu. Na zadní řadě, kde kameny stojí těsně, se dlaň dotkne i souseda. To je mez
velikosti ruky, ne průchod skrz desku. Střelec na g5 je za nataženou paží
sedícího hráče, takže ruka zůstane na dosah a kámen dojede poslední centimetry sám.

Rošáda je dvě gesta za sebou: nejdřív král, pak věž. Ne najednou.

## Postavy

Dvě sedící postavy z MakeHuman, licence CC0. Sestavuje je `make_players.py`
(doplněk MPFB a balík makehuman system assets). Film je jen načítá z
`models/blender/set/people/players.blend`.

Bílý má oblek (`male_elegantsuit01`). Hráč černých figur je mladší vysoký
běloch v košili a džínách (`male_casualsuit01`), jiná pleť, vlasy i boty, aby to nebyli
dvojníci. Jméno objektu `Black` je strana šachovnice. Obličej je součást
toho modelu, ne samostatný sken přilepený na jiné tělo. Ruce mají pět prstů
a stejnou pleť jako hlava.

Kapsle z dřívějšího builderu se nepoužívají. Úchop jsou tři prsty: palec,
ukazováček a prostředníček. Dlaň nesmí projít figurkou.

## Render

24 fps, 1920×1080, Cycles, asi 128 vzorků a denoise. Film kolem 20 sekund.
EEVEE by tenhle záběr nedotáhl a do fronty k produktovým filmům nepatří.

Než se spustí celý render, musí obstát záběry: široký u okna, špetka na
figurce, soumrak a noc. Tělo a odložené ruce nejsou ve stole, sklo pod
úchopem taky ne.
