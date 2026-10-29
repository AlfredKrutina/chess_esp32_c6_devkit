# Kam CzechMate dál

Plán z 28. října 2026. Pět bodů je z inspirace [ChessUp Lite](inspirace/chessup-lite.md). Šestý je Matter bridge do Apple Home. Piece ID, TouchSense, Chess.com, zmenšení desky a displej v desce v tomhle plánu nejsou.

## 1. Nápověda jako režim

Dnes se po zvednutí figurky rozsvítí legální pole. Lekce a puzzle mají nápovědu zvlášť (`LED_CMD_HIGHLIGHT_HINT`, trénink zahájení, puzzle).

Přidat tři stupně, uložené v partii:

- vypnuto
- legální pole po zvednutí
- silnější nápověda, tedy kam tahnout

Bílá a černá mají stupeň každá svůj. V partii, která se počítá, je nápověda vypnutá. Přepnutí je příkaz z aplikace do `game_task`. Nové diody ani senzory k tomu nejsou.

## 2. Cizí tah vždy odkud–kam

Tah, který neudělala ruka u stolu, rozsvítí výchozí pole i cíl. Hráč ho provede na dřevě. Puzzle a trénink zahájení to umí. Stejná cesta má platit pro bota i pro každý další tah, který přijde z aplikace. Dva lidé u jedné desky to nepotřebují, oba tahy dělají rukama.

## 3. Po startu ukázat barvu

Když partii nehrají dva lidé u stolu (bot, lekce), diody krátce ukážou, jestli hráč dostal bílou, nebo černou. Pak zhasnou a nechá se běžný obraz tahů. U dvou hráčů u desky se to nespouští.

## 4. Hodiny hrubě na diodách

Čas počítá firmware (`timer_system`), cifry kreslí aplikace. Na diodách stačí dvě věci: komu čas běží a že zbývá poslední minuta. Přesný čas zůstane v aplikaci a na hodinkách. Světelný pruh podél hrany by chtěl nové diody. Mřížka 8×8 ciferník nenahrazuje.

## 5. Gesto, které pustí hru bez telefonu

Jedno tlačítko na GPIO23 už má obsazená gesta (`button_gesture.h`):

| Gesto | Dnes |
|---|---|
| jeden klik | hra ↔ lampa |
| dva kliky | další barva lampy |
| tři kliky | párování BLE |
| čtyři kliky | ukázka animace |
| podržení z klidu | nová partie (had) |
| klik a podržení | vypnutí |

Nový klik se nepřidává. Podržení z klidu už partii zakládá. Rozšířit ho tak, aby podle posledního režimu pustilo buď partii s hodinami, nebo poslední lekci. Telefon není potřeba k začátku. Cíl není cizí server.

## 6. Matter bridge do Apple Home

`matter_task` ve firmwaru je vypnutý, adresář komponenty neexistuje a v `main.c` je zahrabaný. Zpět se nezapíná. Wi-Fi, BLE a hra na C6 vedle Matter stacku nemají místo.

Deska už mluví s Home Assistantem přes MQTT (`ha_light_task`): lampa, barva, režim hra/lampa. Apple Home umí Matter, ne MQTT.

Bridge je Home Assistant, ne deska. HA vezme vybrané entity a vystaví je jako Matter bridge. Apple Home spáruje ten bridge jednou. Šachovnice v Home není samostatné příslušenství a nemá šachové rozhraní.

Do Home z toho má smysl pustit jen to, co Home umí nakreslit:

- lampa desky (zapnout, barva, jas), to MQTT už vozí
- senzor „partie běží“
- případně která strana je na tahu

Ovládání partie, nápověda a tahy v Apple Home nejsou. Když by deska měla být v Home bez Home Assistantu, je to jiná práce: Matter zařízení na C6. Tenhle plán to nedělá.
