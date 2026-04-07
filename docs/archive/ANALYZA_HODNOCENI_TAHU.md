
# Analýza: Jak funguje hodnocení tahů a proč je „pořád jen slabší tah“

## 1. Kde hodnocení probíhá

Hodnocení tahů se **dělá celé v prohlížeči** (v `chess_app.js`). Aplikace:

1. Stáhne z ESP32 stav hry (`/api/board`, `/api/status`, `/api/history`, `/api/captured`).
2. Při každém novém tahu (v `fetchData`, když `newHistoryLength === lastHistoryLength + 1`) zavolá `evaluateMoveAsync(lastFen, currentFen, lastMove, newHistoryLength)` – **pouze pro tahy člověka** (v režimu bota se tahy bota nehodnotí).
3. V `evaluateMoveAsync` se volá **chess-api.com** (Stockfish): nejdřív `fetchStockfishBestMove(fenBefore)`, pak (když hráč nehrál best move) `fetchStockfishBestMove(fenAfter)`.
4. Z odpovědí se bere `beforeData.eval` a `afterData.eval`. Pokud jsou oba čísla, spočítá se `scoreDrop` a z toho se určí grade (good / inaccuracy / mistake / blunder). Pokud jeden z nich chybí, zobrazí se vždy **„Slabší tah. Lepší bylo X.“** s grade `inaccuracy`.

**Z logů ESP32 (UART_TASK, GAME_TASK, WEB_SERVER_TASK) nelze zjistit**, jestli API vrací eval ani proč by byl „pořád slabší tah“. Na ESP32 jde jen o `POST /api/game/hint_highlight` (nápověda/LED). Volání na chess-api.com a zpracování eval dělá jen prohlížeč.

---

## 2. Logika stupňů (proč jen „výborný“ nebo „slabší“)

Tok v kódu:

```
1) Hráč zahrál tah → fetchData detekuje nový tah (newHistoryLength = lastHistoryLength + 1).
2) evaluateMoveAsync(fenBefore, fenAfter, playedMove, historyLength):
   - Request 1: fetchStockfishBestMove(fenBefore) → beforeData (best move + volitelně eval).
   - Pokud playedUci === bestUci → "Výborný tah!", grade = 'best', KONEC.
   - Request 2: fetchStockfishBestMove(fenAfter) → afterData (volitelně eval).
   - hasEvalBefore = (beforeData.eval != null && typeof === 'number')
   - hasEvalAfter  = (afterData.eval != null && typeof === 'number')

   - Pokud !hasEvalBefore || !hasEvalAfter:
       → vždy: msg = "Slabší tah. Lepší bylo X.", grade = 'inaccuracy'

   - Pokud oba eval jsou:
       → scoreDrop = whiteJustMoved ? (evalBefore - evalAfter) : (evalAfter - evalBefore)
       → ≤0.20 → "Dobrý tah." (good)
       → ≤0.50 → "Slabší tah..." (inaccuracy)
       → ≤1.00 → "Chyba..." (mistake)
       → >1.00  → "Vážná chyba..." (blunder)
```

**Proč vidíte „pořád jen slabší tah“ (když nehrajete best):**  
Protože v těchto případech platí **`!hasEvalBefore || !hasEvalAfter`** – tedy alespoň jeden z requestů nevrátil platné číslo v `eval`, a kód spadne do jednotné větve „Slabší tah. Lepší bylo X.“ s `inaccuracy`. Stupně „Dobrý tah“, „Chyba“, „Vážná chyba“ se zobrazí **jen když oba** `beforeData.eval` a `afterData.eval` jsou čísla.

Možné důvody chybějícího eval:

- API (chess-api.com) někdy nevrací pole `eval` nebo `centipawns` v očekávaném tvaru.
- Odpověď je v jiném obalu (např. jiný klíč než `data`/`result`/`bestMove`).
- Hodnota přijde jako řetězec s ne-ASCII minusem (U+2212) – už je ošetřeno `normalizeEvalString`; pokud se eval bere z jiného pole, může to tam chybět.
- Timeout nebo chyba druhého requestu (fenAfter) → `afterData` null nebo bez eval.

---

## 3. Co z logů opravdu vyčíst

Z přiložených logů (terminál 2):

- Vidět jsou tahy na desce (PICKUP/DROP, GAME_SUCCESS, Executing move: e2-e4, e7-e5, …).
- Vidět jsou **pouze** volání `POST /api/game/hint_highlight` (e7→e5, g8→f6, e5→d4, f8→b4) – to je nápověda/bot pro LED, **ne** hodnocení tahu.
- Žádné logy z prohlížeče (fetch na chess-api.com, výsledky eval, scoreDrop, hasEvalBefore/After). Ty se zobrazí jen v **DevTools konzoli** v prohlížeči (F12 → Console).

Z toho plyne: z těchto ESP32 logů **nejde** rozhodnout, jestli API vrací eval ani proč by bylo hodnocení pořád „slabší“. K tomu je potřeba konzole v prohlížeči.

---

## 4. Režim bota a kdo se hodnotí

V režimu „hra proti botovi“ platí:

- `lastMoveByBot = true` pro tahy bota (např. když je bot černý, tahy černého).
- `evaluateMoveAsync` se volá **jen když `!lastMoveByBot`** – tedy hodnotí se **pouze tahy člověka** (v typickém nastavení tahy bílého).
- V historii se badge (Výborný / Slabší / …) zobrazí jen u tahů, pro které existuje `moveEvaluations[actualIndex]` – tedy u těch, které aplikace opravdu vyhodnotila (u člověka).

Na screenshotu může být „Výborný tah! Byl to nejlepší tah.“ u posledního tahu člověka (např. c3→b5). Pokud u jiných tahů vidíte jen „Slabší tah. Lepší bylo X.“ a nikdy „Dobrý tah“, „Chyba“, „Vážná chyba“, je to v souladu s tím, že v těch případech chybí eval a používá se jednotná větev s `inaccuracy`.

---

## 5. Doporučené kroky (ověření v prohlížeči)

1. **Zapnout „Zhodnocení tahu“** v Nastavení (pokud je to zaškrtávátko).
2. **Otevřít DevTools** (F12) → záložka **Console**.
3. Zahrát několik tahů (včetně jednoho, který není best move).
4. V konzoli hledat:
   - **`[Eval Staging] Missing eval – hasEvalBefore: ... hasEvalAfter: ...`**  
     → potvrzuje, že alespoň jeden z requestů nemá platné číslo v `eval`.
   - **`[Eval Staging] evalVal still null – data.eval: ... data.centipawns: ...`**  
     → z `fetchStockfishBestMove`; ukáže, co API skutečně vrací a proč se eval neparsuje.
   - **`[Eval Staging] hasEvalBefore: true, hasEvalAfter: true, ... scoreDrop: ..., grade: ...`**  
     → když eval projde; pak by se měly objevit i „Dobrý tah“, „Chyba“, „Vážná chyba“ podle scoreDrop.

5. V záložce **Network** filtrovat např. podle „chess-api“ nebo „v1“ a u POST requestů na chess-api.com zkontrolovat **Response** – zda v JSONu je pole `eval` nebo `centipawns` a v jakém tvaru (číslo/řetězec, záporné hodnoty).

Tím zjistíte, zda problém je v (a) API neposílá eval, (b) v parsování (formát/obal/Unicode), nebo (c) v časování (např. druhý request selže).

---

## 6. Shrnutí

| Co vidíte | Příčina v kódu |
|-----------|-----------------|
| „Výborný tah!“ | Hráč zahrál přesně best move z API (`playedUci === bestUci`). |
| „Slabší tah. Lepší bylo X.“ (a nic jiného) | Pro ten tah platí `!hasEvalBefore \|\| !hasEvalAfter` → chybí eval z API nebo jeho parsování → vždy se použije jednotná větev s `inaccuracy`. |
| „Dobrý tah“ / „Chyba“ / „Vážná chyba“ | Objevují se jen když oba requesty vrátí platné číslo v `eval` a spočítá se `scoreDrop`. |

**Proč je to „pořád jen slabší tah“:** Protože v těch konkrétních tazích aplikace nedostane od API (nebo nezpracuje) obě hodnoty `eval` (před tahem a po tahu), takže nikdy nepřijde na řadu rozlišení good / mistake / blunder a vždy se zobrazí výchozí „Slabší tah. Lepší bylo X.“. Ověření musí být v prohlížeči (konzole + Network), ne v logách ESP32.

---

## 7. Úpravy po konzolové diagnostice (Missing eval)

Konzole potvrdila: **`hasEvalBefore: false`, `hasEvalAfter: false`**, a **`beforeData`/`afterData` měly jen 2 klíče** (typicky `from`, `to`) – tedy API vrací best move v zanořeném objektu (např. `raw.data`), zatímco **eval/centipawns může být na rootu** (`raw.eval`, `raw.centipawns`), ne v `raw.data`.

**Provedená úprava v `chess_app.js`:**

- **Fallback parsování eval z rootu:** Pokud po vyčerpání všech zdrojů z `data` zůstane `evalVal == null` a `data !== raw`, kód zkusí stejné pole z objektu `raw` (root odpovědi): `raw.eval`, `raw.centipawns`, `raw.cp`, `raw.score`. Tím se zachytí odpovědi, kde je eval na rootu a `data` má jen `from`/`to` (nebo `move`).
- **Diagnostika při chybějícím eval:** Když `evalVal` zůstane null, do konzole se vypíše `[Eval Staging] evalVal still null – data keys: [...], raw keys: [...], data.eval: ..., raw.eval: ..., raw.centipawns: ...`, aby bylo vidět skutečnou strukturu odpovědi API.

Po nasazení by se měly začít objevovat i stupně „Dobrý tah“, „Chyba“, „Vážná chyba“, pokud API vrací eval na rootu. Pokud stále jen „Slabší tah“, v konzoli budou vidět přesné klíče `data` a `raw` pro další úpravu parsování.

---

## 8. Parsování eval – jedno místo, bez duplicity

Všechna logika pro vytažení a normalizaci eval z odpovědi API je soustředěna na **jednom místě** v `chess_app.js`:

- **`normalizeEvalString(s)`** – nahradí Unicode minus (U+2212) za ASCII minus.
- **`toPawns(v)`** – převede číslo na pawns (hodnoty s |v| > 10 se dělí 100 jako centipawns).
- **`parseEvalFromApiObject(obj)`** – z libovolného objektu (např. `data` nebo `raw`) zkusí v pořadí: `eval` (number/string), `centipawns`, `cp`, `evaluation`, `score` (number/string), `result.eval`; vrací číslo v pawns nebo `null`.

V `fetchStockfishBestMove` se pak volá:
1. `evalVal = parseEvalFromApiObject(data)`;
2. pokud je stále `null` a `raw !== data`, pak `evalVal = parseEvalFromApiObject(raw)`.

Tím odpadla duplicitní větev (parsování z `data` vs. z `raw`); úpravy formátu API nebo nové klíče se řeší jen v `parseEvalFromApiObject`.

---

## 9. Hluboká analýza: bude to fungovat správně?

### 9.1 Tok dat a správnost FEN

- **Kdy se hodnotí:** V `fetchData` platí: `newHistoryLength === lastHistoryLength + 1`, `lastFen` existuje, „Zhodnocení tahu“ zapnuto, není rošáda, tah není od bota. Pak se volá `evaluateMoveAsync(lastFen, currentFen, lastMove, newHistoryLength)`.
- **Co je lastFen / currentFen:** `lastFen` je FEN z předchozího pollu (pozice po `lastHistoryLength` tazích). `currentFen = boardAndStatusToFen(boardData, status, history)` je pozice po aktuálním stavu z API (po `newHistoryLength` tazích). Takže **fenBefore = lastFen** = pozice před posledním tahem, **fenAfter = currentFen** = pozice po posledním tahu. To je pro výpočet scoreDrop správně.
- **Aktualizace lastFen:** Po zpracování bloku se nastaví `lastFen = currentFen` a `lastHistoryLength = newHistoryLength` (pokud není rošáda). Další poll tedy vidí „jeden nový tah“ jen když opravdu přibyl jeden tah – žádná dvojitá aktualizace.

**Závěr:** FEN před/po a monotonicita `lastHistoryLength` jsou nastavené správně; hodnocení se volá pro ten jeden poslední tah.

### 9.2 Parsování eval – parseEvalFromApiObject

- **Jedno místo:** Eval se bere jen v `parseEvalFromApiObject(obj)`. Pro `data` i pro `raw` se volá stejná funkce, takže chování je konzistentní.
- **Pořadí zdrojů:** Zkouší se `eval` (number/string), `centipawns`, `cp`, `evaluation`, `score` (number/string), `result.eval`. To pokrývá typické formáty (včetně chess-api.com s eval/centipawns na rootu).
- **normalizeEvalString:** Unicode minus (U+2212) se nahradí za `-`, takže řetězce typu `"-0.15"` z API se naparsují správně.
- **toPawns:** Pro `|v| > 10` se dělí 100 (centipawns → pawns), jinak se hodnota bere jako pawns. To sedí na dokumentaci (centipawns jako celá čísla).
- **Edge case – centipawns:** Používá se `parseInt(..., 10)`. Pokud API někdy pošle centipawns jako desetinný řetězec (např. `"15.5"`), dostaneme 15 a ne 0.155. V praxi chess-api.com posílá celá čísla; pokud by formát změnili, stačí u centipawns/cp zkusit `parseFloat` a pak stejně `/ 100`.

**Závěr:** Parsování je sjednocené a pro známý formát API by mělo fungovat. Pokud API vrací eval jen na rootu (`raw`) a `data = raw.data` má jen `from`/`to`, fallback `parseEvalFromApiObject(raw)` to zachytí.

### 9.3 scoreDrop a perspektiva

- **evalToWhitePerspective:** S `API_EVAL_SIDE_TO_MOVE = false` se hodnota nemění (API už vrací perspektivu bílého). Oba eval před/po jsou tedy ve stejné perspektivě.
- **whiteJustMoved:** `(historyLength - 1) % 2 === 0` = true pro bílého. **scoreDrop:** bílý hrál → `evalBefore - evalAfter` (pokud bílý zhoršil, eval klesl → kladný drop). Černý hrál → `evalAfter - evalBefore` (černý zhoršil = vzrostla výhoda bílého → kladný drop). **scoreDrop** se dále ořezává na `max(0, scoreDrop)`. Prahové hodnoty 0.20 / 0.50 / 1.00 (pawns) odpovídají obvyklému dělení good / inaccuracy / mistake / blunder.

**Závěr:** Výpočet scoreDrop a přiřazení stupně jsou v pořádku.

### 9.4 Asynchronní běh a platnost hodnocení

- **isEvaluationStillValid(historyLength):** Kontroluje `historyData.length === historyLength`. Po dokončení requestů může mezitím přijít další poll a rozšířit historii – pak by `historyLength` byl „starý“ a UI by se nemělo přepsat špatným indexem. V kódu se po úspěšném vyhodnocení zapisuje `moveEvaluations[historyLength - 1]`; pokud by už v té chvíli bylo `historyData.length !== historyLength`, badge by se mohl vztahovat k jinému indexu. V typickém scénáři (jeden tah mezi pollováním) je `historyData` v momentě odpovědi stále stejná jako při volání, takže platnost kontrolovaná v callbacku je rozumná.
- **Dva tahy v jednom pollu:** Pokud server vrátí najednou např. 8 tahů (dříve bylo 6), platí `newHistoryLength === lastHistoryLength + 1` jen pro 7. V tomtéž běhu se tedy hodnotí jen jeden „nový“ tah; při skoku 6 → 8 se nevolá evaluateMoveAsync (8 !== 6+1). Hodnocení „prostředního“ tahu (7) se pak nespustí. To je **známé omezení**: při velmi rychlých tazích a pomalém pollu může jedno hodnocení vypadnout.

**Závěr:** Pro běžné hraní (jeden tah mezi cykly fetchData) je chování správné. Při skoku v historii jedno hodnocení může chybět.

### 9.5 Co může stále zlobit

1. **API skutečně nevrací eval:** Pokud chess-api.com u části requestů nevrací `eval` ani `centipawns` (ani na rootu, ani v `data`), zůstane `evalVal === null` a v evaluateMoveAsync bude `hasEvalBefore` nebo `hasEvalAfter` false → vždy „Slabší tah. Lepší bylo X.“ Jediné ověření je konzole prohlížeče (`[Eval Staging] evalVal still null – data keys: ..., raw keys: ...`) a záložka Network (Response z chess-api.com).
2. **Jiný obal odpovědi:** Pokud API změní strukturu (např. eval v `raw.analysis.score`), je potřeba rozšířit `parseEvalFromApiObject` o tento klíč – jedna změna na jednom místě.
3. **Timeout / síť:** Pád druhého requestu (fenAfter) vede na chybu v .catch a zobrazení „Zhodnocení nebylo k dispozici“. Není to tichý pád do „Slabší tah“.

### 9.6 Shrnutí

| Oblast | Stav | Poznámka |
|--------|------|----------|
| FEN před/po a lastFen/lastHistoryLength | OK | Správně jeden nový tah, správná pozice před/po |
| parseEvalFromApiObject (data + raw) | OK | Jedno místo, stejná logika pro data i raw |
| scoreDrop a stupně (good / mistake / blunder) | OK | Perspektiva a prahy odpovídají |
| Platnost při async (isEvaluationStillValid) | OK | Pro jeden tah mezi pollováním v pořádku |
| Dva tahy v jednom pollu | Omezení | Hodnotí se jen jeden tah, jedno může chybět |
| Závislost na tom, že API vrací eval | Riziko | Pokud API eval neposílá, zůstane jen „Slabší tah“ |

**Celkově:** Implementace je konzistentní a pro běžné použití a známý formát chess-api.com by měla fungovat správně. To, zda se místo „Slabší tah“ objeví i „Dobrý tah“ / „Chyba“ / „Vážná chyba“, závisí na tom, zda API v obou requestech (fenBefore, fenAfter) skutečně vrací eval – to lze ověřit pouze v prohlížeči (konzole + Network).
