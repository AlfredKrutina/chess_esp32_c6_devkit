# Rozpoznání figur na Hall V2 — 23. října 2026

Na fyzické desce to po flashi z 23. října 2026 funguje poměrně dobře. Zdroj pravdy ve firmwaru je `components/squares_task/hall_i2c_squares.c`. Tady je stav, ke kterému se došlo měřením na skutečných figurkách, a co dalšího se ve stejné práci změnilo.

Každá figurka má jeden diskový magnet. Barva je polarita, typ je síla a šířka. Bílý král a černá dáma v sadě zatím nejsou. Stejný magnet otočený o polaritu je druhá barva, ne jiný typ. Znaménko se na typ nesmí použít.

| Figurka | Magnet | Simulovaný Bz v ose |
| --- | --- | --- |
| Pěšec | Ø10×1 N42 | 7,9 mT |
| Jezdec | Ø10×2 N42 | 14,3 mT |
| Střelec | Ø10×3 N42 | 19,6 mT |
| Věž | Ø10×5 N38 | 26,3 mT |
| Dáma | Ø10×8 N42 | 35,5 mT |
| Král | Ø12×6 N42 | 40,8 mT |

Senzory DRV5055A1 jsou 11,2 mm pod čelem magnetu, pár má rozteč 6,267 mm a každý je 3,13 mm mimo osu pole. Převod v dokumentaci simulace je 81,92 countů na mT. Věž nemá odečet energie z desky. Její pásmo je jen mezera, do které nesmí spadnout střelec ani nejslabší dáma.

## Jak se počítá pole

Energie `E = dev0² + dev1²`. Odchylka je filtrovaný ADC minus nula prázdného pole (`ctx_ref`, jinak `midpoint`). Barva je znaménko silnější odchylky: kladná bílá, záporná černá. Obsazení drží součet absolutních odchylek, zapnutí od 270, vypnutí pod 160. Mrtvé pásmo energie je 15000. Ustálení trvá 250 ms a připouští asi 4 % změnu (`HALL_STABLE_REL_DIV` 25). Sken polí je 25 ms.

Log `>>> POC sq NN changed to [BARVA TYP] (E=… d0=… d1=… ctx) <<<` píše energii i obě odchylky. Příkaz `HALL` na lavičce tiskne tři čísla: energii, dev0 a dev1.

## Pásma energie

Pásma na sebe navazují. Hystereze je 10 %, kromě dámy směrem nahoru: ta se drží jen do svého stropu, aby silný král nezůstal dámou.

| Typ | Energie od | Energie do | Z čeho |
| --- | ---: | ---: | --- |
| Pěšec | 71610 | 250080 | podlaha 8 % pod nejslabším pěšcem 77837 |
| Jezdec | 250081 | 613704 | shluk 421057, 455060, 484445, 549130, 577792 |
| Střelec | 613705 | 1380458 | shluk 651850 až 1242413 |
| Věž | 1380459 | 1724251 | neměřeno, nebrat jako skutečnou věž |
| Dáma, surově | 1724252 | 7500000 | společné pásmo dámy a krále |

Hrana jezdec/střelec je `round(sqrt(577792 × 651850))` = 613705. Ocas střelce 489376 a log 547885 jsou schválně v pásmu jezdce. Střelec 1107410 a 1242413 zůstává střelec, ne věž. Nominály pro zisk pole: pěšec 120000, jezdec 484445, střelec 787792.

Jedna energie dámu a krále neoddělí. Dáma v klidu: 2511730, 3117593, 3691172, 3837860. Stejná dáma při posunu po desce: 2745865 až 4544912, špičky 3991914, 4002925, 4027777, 4034242, 4241225. Král v klidu: 3403621, 4568597, 4611613, 4883125, 5378533, 5443562, 5651540. Posun hranice nad 4,54 milionu z dámy udělal krále na 4,57 a 4,61 milionu. Proto v tabulce královské pásmo není.

## Dáma a král

Král je širší disk (12 mm proti 10 mm), takže slabší senzor zůstává silnější. Rozhoduje součin absolutních odchylek, funkce `hall_name_royal`.

Součiny klidných odečtů:

| Figurka | Energie | Odchylky | Součin |
| --- | ---: | --- | ---: |
| Dáma | 2511730 | 973, 1251 | 1217223 |
| Dáma | 3691172 | 784, 1754 | 1375136 |
| Dáma | 3837860 | 848, 1766 | 1497568 |
| Dáma | 3117593 | 1072, 1403 | 1504016 |
| Král | 3403621 | 1314, 1295 | 1701630 |
| Král | 4611613 | 997, 1902 | 1896294 |
| Král | 4568597 | 1111, 1826 | 2028686 |
| Král | 5443562 | 1811, 1471 | 2663981 |

Král je součin aspoň **1650000**. Dáma v klidu končí na 1504016. Posun krále z 23. října dal součiny 1599136 (1232 a 1298, skoro střed) a 1704888 (972 a 1754). Hranice 1800000 z obou udělala dámu. Středový král se bere od součinu **1550000**, když je slabší senzor aspoň 1100 a oba jsou do 10 %.

Než se na obsazeném poli přepne dáma na krále nebo zpět, musí nové čtení vydržet tři ustálení, asi 0,75 s (`HALL_ROYAL_SWITCH_SCANS`). První jméno po položení padne hned po běžném ustálení. Zámek „jednou král, navždy král“ se neosvědčil: první špička při najetí na pole držela špatný typ i potom, co se figurka usadila.

Když dáma sedí přesně uprostřed citlivého pole a oba senzory jsou silné, součin může přelézt 1650000. V logu to bude vidět na `d0` a `d1`. Slabý král na kraji, kde je druhý senzor skoro tichý, zůstane dáma.

Krátký „Rook“ kolem 1,4 milionu při najíždění dámy je energie, která ještě nedosáhla královského pásma. Není to změřená věž.

## Podsvícení přípravy

Světlo domácího pole zhasne jen tehdy, když senzor pozná tu správnou figurku, typ i barvu. Samotné obsazení nestačí. Dáma a král stejné barvy se na královském domácím poli berou jako shoda. Dva bílé pěšce senzor nerozezná, takže bílý pěšec na cizím bílém pěšci světlo zhasne.

Řady 3 až 6 na polích, která motor považuje za prázdná, svítí barvou typu ze senzoru, včetně dámy a krále. Po boot animaci se průvodce jednou znovu nakreslí i ve stavu `GAME_STATE_IDLE`, takže deska s už položenými figurkami po startu nesvítí prázdná. Správná domácí pole zůstávají tmavá.

## Ruka s víc figurkami

Fáze 2b je ve zdroji: nejvýš čtyři figurky ve vzduchu, pořadí zvednutí, hromádka branek. Položení se skóruje návratem na původní pole, legálním tahem, shodou typu z Hallu a vzdáleností. Když se dvě legální skóre liší nejvýš o 10 a ani jedno není vrácení, obě výchozí pole bliknou a zahraje se vyšší skóre. Každý tah se o 0,5 s nezdržuje. Král do zkratky „druhá figurka v ruce“ nevstupuje. Rošáda a braní mimochodem rukou neprocházejí.

## Zisk pole

První krok fáze 3 je v RAM a po restartu zmizí. Na pole se učí jen pěšec, jezdec a střelec, po potvrzeném položení, čtyřech vzorcích, v rozsahu 0,75 až 1,33 nominálu. Zisk se drží mezi 0,85 a 1,15. Dáma, král a věž se neučí. Dokud pole nemá čtyři vzorky, klasifikace bere surovou energii.

## STM32 při startu

ROM bootloader na adrese `0x63` při GET_ID neodpovídá. ESP-IDF z toho udělá `ESP_ERR_INVALID_STATE`, ne tvrdou chybu Hallu. Firmware to bere jako NACK, zapíše varování, pustí STM32 zpět do aplikace a ověří, že segmenty Hall odpovídají. Obraz z partition `stm32_fw` se při tom nezapíše. Každý start pořád zkusí reset všech čtyř STM32, protože v NVS není úspěšný CRC zápisu.

## Co schválně není hotové

Naučená pásma typů, model přeslechu, obnova celé desky po výpadku napájení a tichá kontrola jen přítomnosti. Trvalý zisk polí v NVS. Měření věže. Zkrácení ustálení pro rychlé položení. Přeskočení pokusu o bootloader, když Hall už odpovídá.
