# Guided capture, LED nápověda a boot animace — plán a analýza

**Účel:** Jednotný referenční dokument pro úpravy firmwaru (hlavně `game_task.c`, `led_task.c`, `main.c`) — navazuje na audit logiky guided capture / LED a rozšiřuje ho o **ochranu boot animace**.

**Související kód (orientační):**

| Oblast | Soubor |
|--------|--------|
| Guided capture, pickup/drop | `components/game_task/game_task.c` |
| LED utilita, boot flag | `components/led_task/led_task.c` |
| Průběh boot + UART resume | `main/main.c` |
| Matrix → fronta příkazů | `components/matrix_task/matrix_task.c` |
| HA lampa vs boot | `components/ha_light_task/ha_light_task.c` |
| API / BLE nastavení LED | `components/web_server_task/web_server_task.c`, `config_manager` |

---

## 1. Shrnutí stavu (guided capture a LED)

### 1.1 Co už funguje rozumně

- **Dvě cesty braní:** (A) *guided capture* — nejdřív zvednutá figurka protihráče, na tahu je hráč s nápovědou útočníků; (B) *3-krokové braní* — vlastní figura → sebrání protivníka z pole → položení na fialový cíl.
- **Legálnost:** Útočníci se hledají přes `game_is_valid_move`; dokončení přes `game_execute_move`.
- **Konfigurace z aplikace:** Úroveň `led_guidance_level` (1–5) a vazba na `guided_capture_hints_enabled` (zapnuto typicky od úrovně 5) přes HTTP API a BLE.
- **Matrix:** Události jdou jako `GAME_CMD_PICKUP` / `DROP` do stejné logiky jako UART.

### 1.2 Zjištěné problémy (priorita oprav)

1. **LED po zvednutí správného útočníka**  
   Po výběru žlutě označené figury se znovu volá `game_show_guided_capture_leds()`, která stále svítí **všechny** útočníky + fialový cíl. Požadavek: ve fázi „útočník v ruce“ jen **jedno validní pole** — pole oběti (`target_row` / `target_col`, u vás fialová), případně striktně jediná LED.

2. **Úroveň nápovědy vs stav guided capture**  
   Při `led_guidance_level < 5` mohou být guided LED vypnuté, ale **stav** `guided_capture_state` se může stále aktivovat; jako náhrada se volá `game_highlight_movable_pieces()` — to **není** totéž jako „jen figury, které mohou vzít zvednutou oběť“.

3. **`game_show_guided_capture_leds()` při `guided_capture_hints_enabled == false`**  
   Funkce hned `return` bez překreslení desky → při chybové větvi (např. špatný útočník) mohou **zůstat staré LED**.

4. **Deprecated `game_handle_piece_lifted`**  
   Stále obsahuje logiku „nelze zvednout protihráče“; běžná cesta je přes frontu — riziko jen při případném znovuzapojení této funkce.

5. **En passant**  
   Guided hledání útočníků předpokládá klasický capture na poli s obětí; e.p. je okrajový případ k explicitnímu testu.

---

## 2. Plán implementace (fáze)

### Fáze A — LED fáze guided capture (jádro UX)

- Rozlišit dvě fáze:
  - **Fáze „výběr útočníka“:** fialová = pole oběti (cíl položení), žlutá = všechny legální útočníci (respektovat `guided_capture_hints_enabled` a ideálně sjednotit s úrovní nápovědy — viz Fáze B).
  - **Fáze „útočník zvednutý“:** `piece_lifted` && aktuální figura je v seznamu útočníků → zobrazit **pouze** cílové pole oběti (fialová), případně bez dalších žlutých útočníků.
- Centralizovat kreslení: buď rozšířit `game_show_guided_capture_leds()` o parametr / interní stav, nebo přidat `game_show_guided_capture_leds_attacker_lifted()` a volat konzistentně z `game_process_pickup_command` / `game_process_drop_command` / error větví.

**Akceptační kritéria:** Po zvednutí jedné z vyznačených útočných figur zmizí žluté útočníky z LED (nebo zůstane výhradně jedna sjednocená indikace — dle finální specifikace), zůstane jasný jeden cíl pro `DROP`.

### Fáze B — Sladění s `led_guidance_level`

- Rozhodnout produktově:
  - **Varianta B1:** Pod úrovní X guided capture **vůbec neaktivovat** stav `guided_capture_state` (jen 3-krokové braní / chybový flow).
  - **Varianta B2:** Stav aktivovat, ale LED škálovat podle úrovně (např. jen fialová oběť, bez žlutých útočníků na nižší úrovni).
- Zajistit, aby HTTP/BLE nastavení zůstalo konzistentní s `game_set_led_guidance_level()`.

### Fáze C — Robustnost při vypnutých hints

- V error větvích guided flow vždy buď:
  - zavolat variantu „bez hintů“ (např. `led_clear_board_only()` + `game_highlight_movable_pieces()`), nebo
  - upravit `game_show_guided_capture_leds()` tak, aby při `!guided_capture_hints_enabled` explicitně obnovila neutrální/herní stav, ne „nic“.

### Fáze D — Údržba deprecated cesty

- Buď zarovnat `game_handle_piece_lifted` s `game_process_pickup_command`, nebo v kódu/README jasně uvést „nepoužívat“ a ověřit, že žádný task ji nevolá.

### Fáze E — Testy a regrese

- Manuální / scénáře: dva útočníci na stejnou oběť; zrušení (návrat oběti / útočníka); `guided_capture_hints` on/off; úroveň 4 vs 5; matrix během aktivního guided stavu.
- En passant: jeden cílený test, zda guided flow dává smysl nebo má být vyloučen.

---

## 3. Boot animace — jak funguje dnes

### 3.1 Řízení z `main.c`

1. Po vytvoření tasků následuje krátká pauza (`vTaskDelay` 1 s).
2. **`show_boot_animation_and_board()`** (běží v kontextu **main task**, blokující):
   - ASCII banner + textový progress bar (0…200 kroků, ~25 ms/krok).
   - Každý krok: `led_boot_animation_step(progress)` — postupně rozsvěcuje deskové LED 0–63 zeleně (`led_set_pixel_internal` + `led_force_immediate_update()`).
   - Na konci: **`led_boot_animation_fade_out()`** — postupné ztlumení zeleně na všech deskových LED, `led_clear_board_only()`, nastavení **`led_booting_active = false`** (`led_task.c`).

3. Poté **`initialize_chess_game()`** — NVS / `GAME_CMD_NEW_GAME`, tlačítka, případně `game_refresh_leds()`.

4. **UART task** je do konce boot animace **suspendovaný** (`vTaskResume` až po animaci), aby výstup nerušil logo.

### 3.2 Globální příznak

- `led_booting_active` je ve výchozím stavu **`true`** (`led_task.c`).
- `bool led_is_booting(void)` vrací tento příznak.
- Komentář v kódu: alternativní funkce `led_booting_animation()` (plná šachovnicová animace) je **vypnutá**; aktivní je jen kroková animace z `main.c` + fade-out.

---

## 4. Boot animace — co ji může rušit (analýza)

### 4.1 Ochrany, které už existují (nízké riziko rozbití animace)

| Komponenta | Chování během `led_is_booting() == true` |
|------------|------------------------------------------|
| **game_task** | Hlavní smyčka: `continue` — **nezpracovává** frontu příkazů (žádné tahy, žádné herní LED z game logiky). |
| **ha_light_task** | Smyčka: `continue` — **neposílá** lampové / herní LED příkazy z HA fronty. |
| **game_start_new_game / FEN** | `while (led_is_booting())` čeká před promotion LED a highlighty. |
| **led_highlight_pieces_that_can_move()** | Pokud boot běží, highlight **zablokován**. |

→ Hlavní záměr „během bootu neťukej do herní logiky a HA LED“ je **splněn**.

### 4.2 Co boot animaci stále může teoreticky narušit (střední / nízké riziko)

1. **`led_set_pixel_safe()` neobsahuje kontrolu `led_is_booting()`**  
   Na rozdíl od některých vyšších funkcí jde přímo na `led_set_pixel_internal`. Jakýkoli kód, který během bootu **přímo** volá `led_set_pixel_safe` / `led_clear_board_only` / interní set z **jiného tasku než main**, může **přepsat** pixely uprostřed `led_boot_animation_step` / fade-out.

2. **LED task — zpracování fronty příkazů**  
   Pokud existuje fronta příkazů do `led_task` a ta se obsluhuje i během bootu **bez** globálního guardu `led_is_booting()`, mohly by se během animace provést jiné efekty. (Vyžaduje ověření konkrétní smyčky `led_task` u příchozích příkazů — doporučení: při auditu implementace přidat jednotnou kontrolu nebo frontu drain až po `led_boot_animation_fade_out`.)

3. **Web server / BLE task**  
   Běží souběžně s `show_boot_animation_and_board()`. Výjimečný HTTP požadavek nebo BLE příkaz, který spustí okamžitý zápis LED mimo ochranu `led_is_booting()`, může způsobit **krátkodobý glitch**. Pravděpodobnost závisí na tom, zda klient připojí a zavolá LED v prvních ~5 s — nízká, ale nenulová.

4. **Nízká úroveň hardware / RMT**  
   Souběžné volání refresh z main (`led_force_immediate_update`) a z LED tasku by mohlo způsobit race na mutexu — v kódu je explicitní komentář, že ochrana je řešena skipováním v **game_task**; **ne** globálním zákazem všech LED zápisů.

5. **Stará funkce `led_booting_animation()`**  
   Pokud by ji někdo znovu zapojil paralelně s `main` sekvencí, došlo by ke kolizi dvou stylů animace. Aktuálně je v `led_task` zakomentovaná — riziko spíš při budoucí úpravě.

### 4.3 Co boot animaci typicky neruší

- **Matrix PICKUP/DROP** během bootu: příkazy se **zařadí do fronty**; `game_task` je zprací až po `led_is_booting() == false` — animace v main mezitím doběhne.
- **UART** během animace: suspended — žádné příkazy z terminálu.

### 4.4 Doporučení (volitelná vylepšení, mimo minimální scope)

- **Jednotný guard:** V kritické cestě zápisu desky (např. batch commit mimo boot step) respektovat `led_is_booting()` **nebo** dokumentovat, že jediný writer během bootu má být `main` přes `led_boot_animation_step` / `fade_out`.
- **Fronta LED příkazů:** Během `led_is_booting()` pouze enqueue s pozdním provedením, ne okamžitý commit (pokud dnes není).
- **Staging log:** Při podezření na glitch přidat jednorázový log, kdo zapsal LED při `led_is_booting()==true`.

---

## 5. Shrnutí rizik boot vs. guided capture

- **Guided capture a boot** spolu **přímo nekolidují**: dokud běží boot, `game_task` neběží herní logiku, takže guided capture se **nezapne**.
- **Po fade-out** se `led_is_booting` vypne a hra / matrix / web mohou normálně kreslit LED — včetně guided capture po úpravách z Fáze A–C.

---

## 6. Checklist před merge

- [x] Po zvednutí útočníka jen jedna jasná indikace cíle (spec dle produktu).
- [x] Chybové větve guided flow při vypnutých hints překreslí desku rozumně.
- [x] Úroveň nápovědy odpovídá chování stavu i LED (výběr útočníka: žlutí jen útočníci při úrovni ≥ 3 nebo plných hintech).
- [x] Boot: `led_set_pixel_safe` a `led_execute_command_new` ignorují zápis během `led_is_booting()`.
- [ ] Manuální test: reboot zařízení — animace doběhne, poté highlight / NVS / matrix guard.
- [ ] Manuální testy podle § 7.1 (Fáze E).

---

## 7. Stav implementace (firmwar)

- **Fáze A (LED po zvednutí útočníka):** Hotovo — při zvednutém vybraném útočníkovi jen fialové pole oběti.
- **Fáze B (úroveň vs LED):** Hotovo — ve fázi výběru útočníka: vždy fialová oběť; žlutí **pouze** na polích legálních útočníků, pokud `guided_capture_hints_enabled` **nebo** `led_guidance_level >= 3`; při úrovni 1–2 jen fialová oběť (bez „všech pohyblivých“).
- **Fáze C (hints OFF / chybové větve):** Hotovo — sjednocený renderer; `game_set_guided_capture_hints_enabled` volá vždy `game_show_guided_capture_leds()`.
- **Fáze D:** Hotovo — komentář u `game_handle_piece_lifted`; v repu ji **nikdo nevolá** (jen lokální prototyp v `game_task.c`).
- **En passant (plán §1.2 / E):** Zdokumentováno u `game_find_legal_attackers_to_square` — guided capture se typicky neaktivuje (útok jde na jiné pole než oběť).
- **Boot animace (§4.4):** Hotovo — během bootu se neprovedou příkazy v `led_execute_command_new`; `led_set_pixel_safe` je no-op (main dál používá `led_set_pixel_internal` v krokové animaci a fade-out).

### 7.1 Fáze E — manuální scénáře (pro testéra)

1. Dva útočníci na stejnou oběť — výběr fáze žlutí oba; po zvednutí jednoho jen fialová oběť; dokončení tahem.
2. Zrušení: znovu zvednout oběť na cílovém poli → ukončení guided.
3. Zrušení: položit útočníka zpět na výchozí pole → ukončení guided.
4. `guided_capture_hints` ON vs OFF (API úroveň 5 vs 4) — při 4 a úrovni ≥ 3 stále žlutí jen útočníci + fialová oběť.
5. Úroveň LED 1–2 během guided výběru — jen fialová oběť (bez žlutých útočníků).
6. Matrix během aktivního guided — lift/drop konzistentní s UART.
7. Reboot — boot LED animace bez glitchů z web/BLE; po dokončení normální hra.

---

*Dokument slouží jako kontext; firmwarové fáze A–D, boot ochrana a poznámka k en passant jsou v kódu zapracované; manuální testy § 7.1 zůstávají na zařízení.*
