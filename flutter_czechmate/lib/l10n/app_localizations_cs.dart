// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Czech (`cs`).
class AppLocalizationsCs extends AppLocalizations {
  AppLocalizationsCs([String locale = 'cs']) : super(locale);

  @override
  String get appTitle => 'CzechMate';

  @override
  String get navPlay => 'Hra';

  @override
  String get navAnalysis => 'Analýza';

  @override
  String get navPuzzle => 'Úlohy';

  @override
  String get navProgress => 'Trénink';

  @override
  String get navSettings => 'Nastavení';

  @override
  String get settingsLanguage => 'Jazyk';

  @override
  String get languageSystem => 'Systém';

  @override
  String get languageEnglish => 'Angličtina';

  @override
  String get languageCzech => 'Čeština';

  @override
  String get languageDescription =>
      'Zvol jazyk aplikace. Systém následuje jazyk zařízení; ostatní jazyky se zobrazí v angličtině.';

  @override
  String get onboardingWelcomeTitle => 'Vítej v CZECHMATE';

  @override
  String get onboardingSkip => 'Přeskočit';

  @override
  String get onboardingNext => 'Další';

  @override
  String get onboardingStart => 'Začít hrát';

  @override
  String get onboard1Title => 'Chytrá šachovnice';

  @override
  String get onboard1Body =>
      'Připoj šachovnici CZECHMATE přes Wi‑Fi nebo Bluetooth a hraj s živou synchronizací.';

  @override
  String get onboard2Title => 'Rozestav figury';

  @override
  String get onboard2Body =>
      'Před zapnutím postav figury do výchozí pozice. Aplikace se s deskou sama srovná.';

  @override
  String get onboard3Title => 'LED nápovědy';

  @override
  String get onboard3Body =>
      'Deska umí zvýraznit tahy LEDkami. Hloubku nápovědy nastavíš v Nastavení → NVS desky.';

  @override
  String get onboard4Title => 'Začni partii';

  @override
  String get onboard4Body =>
      'Na kartě Hra klepni Nová hra a zvol časovou kontrolu. Otočení strany (bílá nebo černá dole) najdeš v ovládání hry nebo v nastavení.';

  @override
  String get onboard5Title => 'Trenér a analýza';

  @override
  String get onboard5Body =>
      'Zapni výukový režim a hodnocení Stockfish na kartě Analýza pro hlubší poznatky.';

  @override
  String get netNoInternetTitle => 'Bez internetu';

  @override
  String get netNoInternetBody =>
      'Stockfish (chess-api), Lichess a další cloudové funkce vyžadují internet. Přes Bluetooth ale šachovnici použiješ i bez připojení k internetu.';

  @override
  String get netNotOnWifiTitle => 'Nejsi na Wi‑Fi';

  @override
  String get netNotOnWifiBody =>
      'Požadavky do cloudu (chess-api, Lichess) obvykle fungují přes jakékoli internetové připojení zařízení (Wi‑Fi nebo mobilní data). K rozhraní HTTP desky na adresě v místní síti má toto zařízení obvykle zapnuté Wi‑Fi ve stejné síti jako deska. Pokud používáš jen hotspot desky bez výstupu na internet, rozpoznávání adres (DNS) pro chess-api nebo Lichess může selhat — zkus Wi‑Fi na chvíli vypnout, použít jiné připojení s internetem nebo současně Wi‑Fi i mobilní data, pokud to systém umožní. Aplikace nemůže přesměrovat síťový provoz mimo Wi‑Fi, dokud je výchozí trasa přes Wi‑Fi.';

  @override
  String get errInvalidBoardUrl =>
      'Neplatná adresa desky — chybí hostitel. Použij http://192.168.4.1 nebo STA IP desky.';

  @override
  String errWifiIpThirdOctetBlocked(String host) {
    return 'Adresa $host je podle filtru 3. oktetu pro Wi‑Fi zamítnutá. Uprav seznam v Pokročilém připojení, zadej jinou URL, nebo na routeru nastav DHCP tak, aby deska nedostala adresu v tomto rozsahu.';
  }

  @override
  String get errBoardSnapshotUnreachable =>
      'Snapshot desky přes Wi‑Fi se nepodařilo načíst. Zkontroluj adresu a síť, nebo se připoj přes Bluetooth.';

  @override
  String get errNoSavedBle =>
      'Žádná uložená Bluetooth deska. Otevři Objevování desky a spáruj CZECHMATE.';

  @override
  String get errBleHostUnsupported =>
      'Bluetooth LE v této verzi aplikace není (Windows desktop). Připoj se přes Wi‑Fi: Nastavení → Připojení a zadej URL desky, nebo použij aplikaci na Androidu / iOS pro BLE.';

  @override
  String get discoveryDesktopBleUnavailableTitle =>
      'Bluetooth sken tady není k dispozici';

  @override
  String get discoveryDesktopBleUnavailableBody =>
      'Windows sestavení nepoužívá Bluetooth Low Energy stack pro tuto desku. Použij Wi‑Fi: níže Pokročilé nebo Nastavení → Připojení a zadej HTTP URL desky v lokální síti. Pro BLE sken a párování použij aplikaci na Androidu nebo iOS.';

  @override
  String get settingsBleDesktopHint =>
      'Na Windows se připoj přes Wi‑Fi URL (níže). BLE objevování v této verzi není.';

  @override
  String get errDemoNoSnapshot =>
      'U ukázkové desky není k dispozici snímek stavu.';

  @override
  String get errConnectBeforeMoves =>
      'Než odešleš tah z aplikace, připoj desku přes Wi‑Fi nebo Bluetooth.';

  @override
  String get errSetupNeedsConnection =>
      'Průvodce nastavením vyžaduje aktivní připojení k desce (Wi‑Fi nebo Bluetooth).';

  @override
  String get errOtaHttps =>
      'OTA URL musí začínat https:// (internet) nebo http:// (např. toto zařízení na hotspotu desky).';

  @override
  String get errOtaBleTransport =>
      'Nahrání firmwaru přes Bluetooth vyžaduje aktivní Bluetooth spojení (připoj přes Bluetooth, ne jen přes Wi‑Fi).';

  @override
  String get errOtaBleGatt =>
      'Bluetooth GATT není připojené — znovu připoj desku z karty Hra nebo z Objevování desky.';

  @override
  String get errOtaMock => 'OTA není dostupná u demo desky.';

  @override
  String get errOtaConnectFirst =>
      'Připoj desku přes Wi‑Fi nebo Bluetooth a pak zkus aktualizaci znovu.';

  @override
  String get errOtaBoardHttpMissingDetail =>
      'Chybí HTTP adresa desky (AP nebo STA IP). Ulož ji jako výchozí URL desky — sledování průběhu ji potřebuje i přes Bluetooth.';

  @override
  String get errOtaNoOtaPartitions =>
      'Tato deska nemá OTA oddíly (ota_0 + ota_1). Přeflashuj rozložením s dual OTA nebo aktualizuj přes USB.';

  @override
  String get errOtaStaRequiredForHttps =>
      'Připoj desku k Wi‑Fi jako stanici (STA), aby mohla stáhnout firmware přes HTTPS.';

  @override
  String errOtaWifiStatusCheckFailed(String error) {
    return 'Nepodařilo se ověřit Wi‑Fi: $error';
  }

  @override
  String get errOtaPollUnreachable =>
      'Nepodařilo se číst stav aktualizace z desky. Zkontroluj uloženou HTTP adresu desky (např. http://192.168.4.1).';

  @override
  String get errOtaPollTimeout =>
      'Vypršel čas čekání na dokončení aktualizace firmwaru.';

  @override
  String errOtaBoardReported(String message) {
    return 'Aktualizace firmwaru: $message';
  }

  @override
  String get errHintsNeedConnection =>
      'Nápovědní LED vyžadují připojení desky. Nejprve Bluetooth nebo Wi‑Fi.';

  @override
  String get errNoActiveSession =>
      'Žádné aktivní připojení k desce. Připoj ji přes Wi‑Fi nebo Bluetooth z obrazovky Objevování desky.';

  @override
  String get errBrightnessNeedsBoard =>
      'Jas vyžaduje připojenou desku (Wi‑Fi nebo Bluetooth).';

  @override
  String get errLampNeedsBoard =>
      'Ovládání lampy vyžaduje připojenou desku (Wi‑Fi nebo Bluetooth).';

  @override
  String get errGameLampNeedsBoard =>
      'Režim herní lampy vyžaduje připojenou desku (Wi‑Fi nebo Bluetooth).';

  @override
  String get errAutoLampNeedsBoard =>
      'Automatický čas lampy vyžaduje připojenou desku (Wi‑Fi nebo Bluetooth).';

  @override
  String get moveEvalExcellentBest => 'Skvělý tah — byl to nejlepší možný tah.';

  @override
  String get moveEvalExcellentEngine =>
      'Výborně — přesně jako nejlepší tah šachového programu.';

  @override
  String moveEvalWeakerNoScore(String best) {
    return 'Slabší tah (bez přesného skóre). Lepší byl $best.';
  }

  @override
  String moveEvalGoodLoss(int cp) {
    return 'Dobrý tah — malá ztráta (~$cp cp).';
  }

  @override
  String moveEvalInaccuracy(int cp, String best) {
    return 'Nepřesnost — cca o $cp cp horší. Silnější byl $best.';
  }

  @override
  String moveEvalMistake(int cp, String best) {
    return 'Chyba — cca o $cp cp horší. Lepší byl $best.';
  }

  @override
  String moveEvalBlunder(int cp, String best) {
    return 'Hrubka — cca o $cp cp horší. Mnohem lepší byl $best.';
  }

  @override
  String moveEvalFailed(String error) {
    return 'Vyhodnocení tahu selhalo: $error';
  }

  @override
  String get pgnEventHeader => 'CZECHMATE';

  @override
  String get pgnOpeningPrefix => 'Začátek';

  @override
  String get sharePgnSubject => 'Šachová partie (PGN)';

  @override
  String get shareSummaryNotReady =>
      'Shrnutí ještě není připravené — zkus to za chvíli.';

  @override
  String shareExportFailed(String error) {
    return 'Export selhal: $error';
  }

  @override
  String get sharePngEncodeError => 'Obrázek se nepodařilo zakódovat jako PNG.';

  @override
  String shareGameSummaryMeta(String result, String duration, int moves) {
    return '$result · $duration · $moves tahů';
  }

  @override
  String shareGameSummaryLine(String meta) {
    return 'Shrnutí partie — CzechMate · $meta';
  }

  @override
  String get reportNoGameData => 'Žádná data partie.';

  @override
  String get reportMoveEvaluation => 'Hodnocení tahů';

  @override
  String get reportNoStockfishData =>
      'Žádné údaje od Stockfish — zapni hodnocení tahů a odehraj partii.';

  @override
  String get reportEvalPerspective => 'Z pohledu bílého (+ pro bílého)';

  @override
  String get reportMoveTiming => 'Časování tahů';

  @override
  String get reportTimingUnavailable =>
      'Chybí časové značky tahů — grafy času nejsou k dispozici.';

  @override
  String get reportElapsedTime => 'Uplynulý čas';

  @override
  String get reportElapsedCaption => 'Součet od začátku partie po půltazích';

  @override
  String get reportHalfMoveAxis => 'půltah → minuty';

  @override
  String get reportTimePerMove => 'Čas na tah';

  @override
  String get reportSecondsSincePrev => 'Sekundy od předchozího tahu';

  @override
  String get colorWhite => 'Bílý';

  @override
  String get colorBlack => 'Černý';

  @override
  String reportMaterialCaptured(int w, int b) {
    return 'Braný materiál · Bílý $w b. · Černý $b b.';
  }

  @override
  String get reportAppBarTitle => 'Shrnutí partie';

  @override
  String get reportDone => 'Hotovo';

  @override
  String get reportResultHeading => 'Výsledek';

  @override
  String get reportShareHeading => 'Sdílení';

  @override
  String get reportExportAdvancedTitle => 'Pokročilé rozvržení';

  @override
  String get reportExportAdvancedSubtitle =>
      'Průhlednost, sekce na obrázku a jejich pořadí. Tvar určují náhledy rozvržení výše.';

  @override
  String get reportSharePreparing => 'Připravuji…';

  @override
  String get reportShareSummaryImage => 'Sdílet…';

  @override
  String get reportCopySummaryImage => 'Zkopírovat obrázek';

  @override
  String get reportCopySummaryBusy => 'Kopíruji…';

  @override
  String get reportSummaryCopiedSnack => 'Zkopírováno — vlož do jiné aplikace.';

  @override
  String get reportCopyImageFailed => 'Obrázek se nepodařilo zkopírovat.';

  @override
  String get reportCopyImageWebHint =>
      'Ve prohlížeči nejde kopírovat obrázek do schránky — použij Sdílet níže.';

  @override
  String get reportCopyImageDesktopFallback =>
      'Otevírám sdílení — obrázek ve schránce funguje nejlépe na mobilu (iPhone a Android); na počítači použij Sdílet.';

  @override
  String get reportExportSectionOrderTitle => 'Pořadí sekcí (export)';

  @override
  String get reportExportSectionOrderSubtitle => '';

  @override
  String get reportExportBlockRecap => 'Titulek průběhu';

  @override
  String get reportExportBlockBranding => 'Značka';

  @override
  String get reportExportBlockResult => 'Výsledek a důvod';

  @override
  String get reportExportBlockStats => 'Čas a tahy';

  @override
  String get reportExportBlockMaterial => 'Braný materiál';

  @override
  String get reportExportBlockBoard => 'Šachovnice';

  @override
  String get reportExportBlockEval => 'Graf hodnocení pozice';

  @override
  String get reportExportBlockTiming => 'Grafy času';

  @override
  String get reportSharePgn => 'Sdílet soubor PGN';

  @override
  String get reportExportAppearanceTitle => 'Export obrázku';

  @override
  String get reportExportAppearanceSubtitle =>
      'Zvol rozvržení níže — náhled se hned změní. Barvy grafů a pokročilé volby zůstávají k dispozici.';

  @override
  String get reportExportStylePickHint =>
      'Klepnutím zvolíš formát sdílení (vysoká karta, čtverec, story nebo široký obrázek).';

  @override
  String get reportChartPaletteChoiceHint =>
      'Předvolba mění barvy grafů hodnocení a času v Analýze i v exportu. Vlastní barvy mají u každého řádku popisek.';

  @override
  String get reportExportAspectRatio => 'Velikost plátna';

  @override
  String get reportExportAspectCard => 'Karta (vysoká)';

  @override
  String get reportExportAspectSquare => 'Čtverec 1:1';

  @override
  String get reportExportAspectStory => 'Story 9:16';

  @override
  String get reportExportAspectLandscape => 'Na šířku 16:9';

  @override
  String get reportExportTransparentBg => 'Průhledné pozadí';

  @override
  String get reportExportTransparentHint =>
      'Průhledné je jen vnější rámeček; pole šachovnice a grafy zůstávají na plných panelech.';

  @override
  String get reportChartPaletteTitle => 'Barvy grafů';

  @override
  String get reportChartPaletteSubtitle =>
      'Výrazné předvolby pro hodnocení a časové grafy — platí i v Analýze.';

  @override
  String get reportChartPaletteTheme => 'Jako aplikace';

  @override
  String get reportChartPaletteNeon => 'Neon';

  @override
  String get reportChartPaletteSunset => 'Západ';

  @override
  String get reportChartPaletteOcean => 'Oceán';

  @override
  String get reportChartPaletteCustom => 'Vlastní';

  @override
  String get reportChartPaletteEditCustom => 'Upravit vlastní barvy';

  @override
  String get reportChartPaletteCustomTitle => 'Barvy grafů';

  @override
  String get reportChartPaletteEvalLine => 'Čára hodnocení';

  @override
  String get reportChartPaletteCumulative => 'Uběhlý čas';

  @override
  String get reportChartPaletteBarWhite => 'Sloupce bílého';

  @override
  String get reportChartPaletteBarBlack => 'Sloupce černého';

  @override
  String get reportChartPaletteDone => 'Hotovo';

  @override
  String get reportExportShowBranding => 'Zobrazit záhlaví CzechMate';

  @override
  String get reportExportShowStats => 'Zobrazit čas a statistiky tahů';

  @override
  String get reportExportShowFinalBoard =>
      'Zobrazit výsledné rozložení figurek';

  @override
  String get reportExportFlipBoard => 'Otočit šachovnici (černý dole)';

  @override
  String get reportExportShowEvalChart => 'Zahrnout graf hodnocení pozice';

  @override
  String get reportExportShowCumulativeChart => 'Zahrnout graf uběhlého času';

  @override
  String get reportExportShowPerMoveChart => 'Zahrnout sloupce času na tah';

  @override
  String get reportExportPresetFull => 'Plný';

  @override
  String get reportExportPresetMinimal => 'Minimální';

  @override
  String get reportExportPresetStory => 'Story výřez';

  @override
  String get reportExportThumbWideLabel => 'Široký';

  @override
  String get reportExportThumbWideAspect => '16∶9';

  @override
  String get reportExportShareGifRecap => 'Sdílet průběh tahů (GIF)';

  @override
  String get reportExportGifSubtitle =>
      'Animovaný průběh pro Stories nebo chaty (9:16). U dlouhých partií se vzorkuje.';

  @override
  String get reportExportGifBuilding => 'Skládám animaci…';

  @override
  String get reportExportGifShareSubject => 'Průběh partie (GIF)';

  @override
  String reportExportGifFailed(String error) {
    return 'Export GIF selhal: $error';
  }

  @override
  String reportExportRecapMove(int current, int total) {
    return 'Tah $current / $total';
  }

  @override
  String get reportClock => 'Hodiny';

  @override
  String reportMinutesShort(String n) {
    return '$n min';
  }

  @override
  String get reportFinalFen => 'Výsledné FEN';

  @override
  String get resultWhiteWins => 'Výhra bílého';

  @override
  String get resultBlackWins => 'Výhra černého';

  @override
  String get resultDraw => 'Remíza';

  @override
  String get resultGameEnded => 'Partie skončila';

  @override
  String get endReasonUnknown => 'Neznámý výsledek';

  @override
  String get endReasonGameOver => 'Konec partie';

  @override
  String get endReasonCheckmate => 'Mat';

  @override
  String get endReasonStalemate => 'Pat';

  @override
  String get endReasonFinished => 'Dohráno';

  @override
  String get statTime => 'ČAS';

  @override
  String get statMoves => 'TAHY';

  @override
  String get statWhiteAvg => 'BÍLÝ Ø';

  @override
  String get statBlackAvg => 'ČERNÝ Ø';

  @override
  String get durationDash => '—';

  @override
  String secondsShort(int n) {
    return '${n}s';
  }

  @override
  String minutesSeconds(int m, String s) {
    return '$m:$s';
  }

  @override
  String hoursMinutesSeconds(int h, int mm, int s) {
    return '${h}h ${mm}m ${s}s';
  }

  @override
  String get commonClose => 'Zavřít';

  @override
  String get commonCancel => 'Zrušit';

  @override
  String get commonOk => 'Dobře';

  @override
  String get commonSave => 'Uložit';

  @override
  String get commonContinue => 'Pokračovat';

  @override
  String get commonRetry => 'Opakovat';

  @override
  String get commonLoading => 'Načítání…';

  @override
  String get commonError => 'Chyba';

  @override
  String get commonYes => 'Ano';

  @override
  String get commonNo => 'Ne';

  @override
  String get lastErrorTitle => 'Problém s připojením';

  @override
  String get telemetryPrivacyTitle => 'Soukromí';

  @override
  String get telemetryPrivacyBody =>
      'Tento build neodesílá analytiku. Provoz jde jen na tvou desku v lokální síti nebo přes Bluetooth.';

  @override
  String get telemetryIcloudTitle => 'iCloud';

  @override
  String get telemetryIcloudBody =>
      'Data partie zůstávají na zařízení, dokud je neexportuješ nebo nezapneš synchronizaci jinde.';

  @override
  String get discoveryHelpConnectingTitle => 'Potřebujete pomoct s připojením?';

  @override
  String get discoveryHelpConnectingSubtitle =>
      'Kroky nastavení a tipy ke skenování';

  @override
  String get discoveryTitle => 'Najít desku';

  @override
  String get discoveryBlePermissionAndroid =>
      'Povol Bluetooth oprávnění v nastavení systému, aby šlo desku vyhledat.';

  @override
  String get discoveryBlePermissionDenied =>
      'Pro vyhledání desky je potřeba přístup k Bluetooth. Povol ho v nastavení systému a zkus to znovu.';

  @override
  String get discoveryBluetoothNotReady =>
      'Bluetooth není připravené. Zapni ho v nastavení, počkej pár sekund a klepni znovu na Najít desku.';

  @override
  String discoveryBleScanFailed(String error) {
    return 'Sken Bluetooth selhal: $error';
  }

  @override
  String get discoveryIntro =>
      'Nejprve Bluetooth. Doma na stejné Wi‑Fi aplikace přejde na rychlejší HTTP, jakmile desku dosáhne.';

  @override
  String get discoveryFlowStepsTitle => 'Tři kroky';

  @override
  String get discoveryFlowStep1 => 'Zapni Bluetooth na tomto zařízení.';

  @override
  String get discoveryFlowStep2 =>
      'Klepni na Najít desku a vyber svou desku ze seznamu.';

  @override
  String get discoveryFlowStep3 =>
      'Doma na stejné Wi‑Fi jako deska aplikace přejde na rychlejší HTTP, když to jde; jinde zůstane Bluetooth.';

  @override
  String get discoveryScanCardLead =>
      'Vyhledání spustíš tlačítkem níže. Adresu zadáš v Pokročilém.';

  @override
  String get discoveryRecoveryTitle => 'Nepodařilo se připojit';

  @override
  String get discoveryRecoveryBulletBt =>
      'Bluetooth je zapnutý a deska je zapnutá poblíž.';

  @override
  String get discoveryRecoveryBulletRange =>
      'Přibliž se — zdi a kapsy někdy signál zhorší.';

  @override
  String get discoveryRecoveryBulletHomeWifi =>
      'Doma je nejjednodušší mít zařízení i desku na stejné Wi‑Fi kvůli HTTP; mimo domov stačí Bluetooth.';

  @override
  String get discoveryRecoveryBulletManual =>
      'Adresu desky můžeš zadat v Pokročilém.';

  @override
  String get discoveryRecoveryOpenAppSettings => 'Otevřít nastavení systému';

  @override
  String get discoveryRecoveryDismiss => 'Rozumím';

  @override
  String get transportDisconnected => 'Odpojeno';

  @override
  String get transportDemo => 'Demo režim';

  @override
  String get transportWifi => 'Wi‑Fi';

  @override
  String get transportBluetooth => 'Bluetooth';

  @override
  String transportConnecting(String transport) {
    return '$transport · připojuji…';
  }

  @override
  String get defaultBoardName => 'CZECHMATE deska';

  @override
  String get discoveryWifiUrlHint => 'např. http://192.168.4.1';

  @override
  String get discoveryConnectWifi => 'Připojit přes Wi‑Fi';

  @override
  String get discoveryFindBle => 'Najít desku (Bluetooth)';

  @override
  String get discoveryScanning => 'Skenuji…';

  @override
  String get discoveryStopScan => 'Zastavit sken';

  @override
  String get discoveryManualUrl => 'Ruční zadání URL';

  @override
  String get connectionModeAuto => 'Auto (Bluetooth pak Wi‑Fi)';

  @override
  String get connectionModeWifiOnly => 'Jen Wi‑Fi';

  @override
  String get connectionModeBleOnly => 'Jen Bluetooth';

  @override
  String get preferBleOnlyTitle => 'Jen Bluetooth (nepřepínat na Wi‑Fi)';

  @override
  String get preferBleOnlySubtitle =>
      'Po připojení přes BLE zůstat na Bluetooth, i když deska zná Wi‑Fi adresu.';

  @override
  String get mockBoardTileTitle => 'Demo deska (bez hardwaru)';

  @override
  String get mockBoardTileSubtitle =>
      'Používá vestavěný snímek stavu pro testování rozhraní.';

  @override
  String get sessionConnectedWifi => 'Připojeno přes Wi‑Fi';

  @override
  String get sessionConnectedBle => 'Připojeno přes Bluetooth';

  @override
  String get sessionConnectedMock => 'Aktivní demo deska';

  @override
  String get allowBluetoothSettings =>
      'Pro sken BLE desek povol Bluetooth v nastavení systému.';

  @override
  String get gameAppBarTitle => 'czechmate';

  @override
  String get gameConnectionTooltip => 'Připojení k šachovnici';

  @override
  String get gameNewGame => 'Nová hra';

  @override
  String get gameHintThinking => 'Přemýšlím…';

  @override
  String get gameHint => 'Nápověda';

  @override
  String get gamePauseClock => 'Pozastavit hodiny';

  @override
  String get gameResumeClock => 'Pokračovat v hodinách';

  @override
  String get gameClearHintLeds => 'Skrýt nápovědu na desce';

  @override
  String get gameHideBoardHintTooltip => 'Skrýt nápovědu na desce';

  @override
  String get gameNoSnapshotYet => 'Zatím není k dispozici snímek stavu desky.';

  @override
  String get gameFindBoard => 'Najít desku';

  @override
  String get gameFindBoardSubtitle =>
      'Připoj Wi‑Fi nebo Bluetooth pro živou pozici.';

  @override
  String get gameControlsTitle => 'Ovládání hry';

  @override
  String get gameHistory => 'Historie';

  @override
  String get gameReturnLive => 'Zpět na živou hru';

  @override
  String get gameExitPuzzleCheck => 'Ukončit kontrolu puzzlu (pozice zůstane)';

  @override
  String get gameFindBoardTooltip => 'Najít desku';

  @override
  String get gameLayoutTooltip => 'Režim rozvržení';

  @override
  String get gameLayoutStandard => 'Standardní';

  @override
  String get gameLayoutBoardOnly => 'Jen deska';

  @override
  String get gameCoachRailShow => 'Zobrazit sloupec AI trenéra';

  @override
  String get gameCoachRailHide => 'Skrýt sloupec AI trenéra';

  @override
  String get gameControlsTooltip => 'Ovládání hry';

  @override
  String get gameAnalysisTooltip => 'Analýza';

  @override
  String get gameSummaryTooltip => 'Shrnutí partie';

  @override
  String get gameDisconnectTooltip => 'Odpojit spojení';

  @override
  String get gameHidePanelTooltip => 'Skrýt panel';

  @override
  String gamePuzzleMovesProgress(int played, int total) {
    return 'Tahy $played / $total';
  }

  @override
  String gameHintTransient(String from, String to, String evalSuffix) {
    return 'Nápověda: $from→$to$evalSuffix';
  }

  @override
  String gameBestMoveSnack(String from, String to) {
    return 'Doporučený tah: z $from na $to';
  }

  @override
  String gameHintFailed(String error) {
    return 'Nápověda selhala: $error';
  }

  @override
  String gameDemoBoardSnack(String feature) {
    return 'Demo deska: $feature funguje jen přes Bluetooth nebo Wi‑Fi na reálný hardware.';
  }

  @override
  String get gameClockPauseSent => 'Příkaz pozastavit hodiny odeslán';

  @override
  String get gameClockResumedSnackbar => 'Hodiny pokračují';

  @override
  String get gameHintLedsClearedSnackbar => 'Nápovědní LED zrušeny';

  @override
  String get gameMatrixGuardTitle => 'Nutné srovnat desku';

  @override
  String gameMatrixGuardPaused(String squares) {
    return 'Srovnejte figurky podle LED na desce$squares. Hra pokračuje po srovnání.';
  }

  @override
  String gameMatrixGuardResync(String squares) {
    return 'Po startu nesedí deska s uloženou hrou$squares. Srovnejte podle LED.';
  }

  @override
  String get gameMatrixGuardForceClear => 'Obnovit hru';

  @override
  String get gameMatrixGuardForceClearHint => 'Jen když jsou figurky už srovnané';

  @override
  String get gameMatrixGuardForceClearSnack =>
      'Hra obnovena — zkontrolujte, že deska sedí';

  @override
  String get gameNoSnapshotBody =>
      'Klepni vlevo nahoře nebo níže pro nalezení desky (Bluetooth). Wi‑Fi a další možnosti jsou v rozšířeném nastavení.';

  @override
  String get gameBackToPanelTooltip => 'Zpět na rozvržení s panelem';

  @override
  String get gameAiCoachTitle => 'AI trenér';

  @override
  String get gameMoreOptionsTooltip => 'Další možnosti';

  @override
  String statusConnectedTransport(String transport) {
    return 'Připojeno ($transport)';
  }

  @override
  String statusConnectingTransport(String transport) {
    return 'Připojuji ($transport)…';
  }

  @override
  String statusBoardNotRespondingTransport(String transport) {
    return 'Deska neodpovídá ($transport)';
  }

  @override
  String statusBleDevLine(String name) {
    return 'BLE: $name';
  }

  @override
  String get gameDesktopPlayTitle => 'Hra — desktop';

  @override
  String get gameDesktopPlaySubtitle =>
      'Klepnutím na indikátor vlevo nahoře otevřeš správu připojení (Wi‑Fi, Bluetooth, ukázková deska) — stejně jako na iOS.';

  @override
  String stockfishEvalFailedSnack(String msg) {
    return 'Vyhodnocení selhalo: $msg';
  }

  @override
  String get firmwareDialogTitle => 'Aktualizace firmware desky';

  @override
  String firmwareDialogVersions(String serverVer, String boardVer) {
    return 'Na serveru je $serverVer, deska hlásí $boardVer.';
  }

  @override
  String get firmwareDialogHttpsNote =>
      'Deska stahuje přes HTTPS (Wi‑Fi STA). CzechMate na tomto zařízení posílá jen odkaz.';

  @override
  String get firmwareTurnOffReminders => 'Vypnout připomínky';

  @override
  String get firmwareNotNow => 'Teď ne';

  @override
  String get firmwareUpdateAction => 'Aktualizovat';

  @override
  String get discoveryCloseTooltip => 'Zavřít';

  @override
  String get discoveryConnectionStatusTitle => 'Stav připojení';

  @override
  String get discoveryScanningBoards => 'Hledám desky…';

  @override
  String discoveryBleDevicesDev(String guid) {
    return 'Bluetooth ($guid)';
  }

  @override
  String get discoveryFoundBoards => 'Nalezené desky';

  @override
  String get discoveryEmptyBleList =>
      'Zatím žádná deska. Počkej pár sekund se zapnutým Bluetooth a zapnutou deskou v dosahu.';

  @override
  String get discoveryAutoScanHint =>
      'Sken začne sám, pokud není pro příští pokus nastavené jen Wi‑Fi (Pokročilé).';

  @override
  String get discoveryAdvancedSubtitle =>
      'Adresa Wi‑Fi, uložené Bluetooth, jednorázový přenos';

  @override
  String get discoveryAdvancedBlockedOctetsTitle => 'Filtr Wi‑Fi IP (3. oktet)';

  @override
  String get discoveryAdvancedBlockedOctetsBody =>
      'Deska si seznam uloží do NVS a DHCP adresu se zakázaným 3. oktetem nepřijme (odpojí STA a zkusí znovu). CzechMate používá stejný filtr pro URL. Hodnoty se po uložení nebo při připojení přes BLE odešou šifrovaně — firmware nemá vlastní výchozí blokaci, dokud aplikace nic nepošle. Příklad: 88 zamítne všechny x.x.88.x. Prázdné pole vypne filtr na obou stranách. Dokud neuložíš jinak, aplikace výchozě používá 88.';

  @override
  String get discoveryAdvancedBlockedOctetsLabel =>
      'Blokované 3. oktety (čárkami)';

  @override
  String get discoveryAdvancedBlockedOctetsHint => 'např. 88 nebo 88,100';

  @override
  String get discoveryAdvancedBlockedOctetsSave => 'Uložit filtr';

  @override
  String get discoveryAdvancedBlockedOctetsReset => 'Obnovit výchozí (88)';

  @override
  String get discoveryAdvancedBlockedOctetsSavedSnack =>
      'Filtr Wi‑Fi IP uložen';

  @override
  String get discoveryTransportAdvancedExplanation =>
      'Aplikace ve výchozím stavu sama volí cestu (nejprve Bluetooth, potom Wi‑Fi přes HTTP, jakmile deska odpoví). Jednorázové vynucení jen Wi‑Fi nebo jen Bluetooth pro příští připojení najdeš v Nastavení → Připojení desky → Pokročilé — po dokončení pokusu se zase vrátí automatika.';

  @override
  String get discoveryOpenTransportSettingsButton =>
      'Otevřít nastavení připojení';

  @override
  String get discoveryConnectionModeHeading => 'Režim připojení';

  @override
  String get discoveryConnectionModeHint =>
      'Auto: nejdřív Bluetooth, pak Wi‑Fi, jakmile je deska dostupná. Jen Wi‑Fi přeskočí Bluetooth. Jen Bluetooth zůstane na BLE i když je známá Wi‑Fi adresa.';

  @override
  String get discoveryBleSegmentShort => 'Jen BLE';

  @override
  String get discoveryReconnectSavedBoard => 'Znovu připojit uloženou desku';

  @override
  String get discoveryManualAdvanced => 'Ruční zadání / pokročilé';

  @override
  String get discoveryWifiUrlLabel => 'Adresa desky (Wi‑Fi)';

  @override
  String get discoveryEnterBoardUrlSnack =>
      'Zadej URL nebo IP desky (např. http://192.168.4.1).';

  @override
  String get discoveryBoardHotspotButton => 'Hotspot desky (192.168.4.1)';

  @override
  String get discoveryBoardApEnable => 'Zapnout Wi‑Fi hotspot desky';

  @override
  String get discoveryBoardApDisable => 'Vypnout Wi‑Fi hotspot desky';

  @override
  String get discoveryBoardApSubtitle =>
      'Toto zařízení se může připojit k síti desky kvůli HTTP nebo OTA. Na desce je hotspot ve výchozím stavu vypnutý.';

  @override
  String get discoveryBoardApCommandSent =>
      'Příkaz k hotspotu odeslán — počkej pár sekund a zkontroluj dostupné Wi‑Fi sítě.';

  @override
  String discoveryBoardApError(String error) {
    return 'Hotspot se nepodařilo změnit: $error';
  }

  @override
  String get errBoardApNeedsBle =>
      'Pro změnu hotspotu musí být navázané Bluetooth.';

  @override
  String get boardWifiProvisionSheetTitle => 'Připojit desku k Wi‑Fi';

  @override
  String get boardWifiProvisionSheetLead =>
      'Když to jde, použij stejnou domácí Wi‑Fi jako toto zařízení (často lépe funguje 2,4 GHz). CzechMate uložené heslo ze systému nepřečte — zadej ho zde jednou.';

  @override
  String get boardWifiProvisionIosSsidHint =>
      'Když se název aktuální Wi‑Fi nezobrazí (na zařízeních Apple často dokud aplikace nemá přístup k poloze kvůli informacím o Wi‑Fi), zadej SSID ručně.';

  @override
  String get boardWifiProvisionScanBoardButton =>
      'Vyhledat Wi‑Fi v okolí desky';

  @override
  String get boardWifiProvisionScanning => 'Skenuji…';

  @override
  String boardWifiProvisionVisibleYes(String ssid) {
    return 'Wi‑Fi síť tohoto zařízení „$ssid“ je v dosahu desky — zadej heslo a připoj.';
  }

  @override
  String get boardWifiProvisionVisibleNo =>
      'Tento SSID se ve výsledku skenu na desce neobjevil — zkontroluj vzdálenost nebo zkus síť na 2,4 GHz. Můžeš to i tak zkusit; hra může jet přes Bluetooth.';

  @override
  String get boardWifiProvisionSurveyFailed =>
      'Sken na desce selhal nebo vypršel čas.';

  @override
  String get boardWifiProvisionDoneSnack =>
      'Údaje k Wi‑Fi odeslány — počkej, až se deska připojí.';

  @override
  String get boardWifiProvisionStaTimeoutHint =>
      'STA zatím nepřipojené — deska síť nejspíš nevidí. Můžeš pokračovat přes Bluetooth nebo zapnout hotspot desky v Objevování desky.';

  @override
  String get boardWifiProvisionFirmwareContextHint =>
      'Stejné nastavení Wi‑Fi jako ve spodním listu po připojení z Objevování desky — jeden společný postup, ne dvě různá nastavení.';

  @override
  String get otaHttpsStaGateTitle =>
      'Stažení firmwaru z internetu vyžaduje Wi‑Fi k routeru';

  @override
  String get otaHttpsStaGateBody =>
      'Aktualizace přes HTTPS si stahuje sama deska. Připoj ji k Wi‑Fi s internetem (nejčastěji kompatibilní 2,4 GHz). Zapnutí hotspotu desky pomůže tomuto zařízení komunikovat s deskou přes HTTP, ale samo o sobě nepřinese desce internet — použij nahrání firmwaru přes Bluetooth níže, nebo nejdřív připoj desku k domácí Wi‑Fi.';

  @override
  String get otaHttpsStaGateBleUpload => 'Nahrát firmware přes Bluetooth';

  @override
  String get otaHttpsStaGateHotspotBle => 'Zapnout hotspot desky (Bluetooth)';

  @override
  String get otaHttpsStaGateWifiTips => 'Tipy k Wi‑Fi 2,4 GHz';

  @override
  String get otaHttpsStaGateCancel => 'Teď ne';

  @override
  String get otaHttpsWifiTipsTitle => 'Tipy k Wi‑Fi routeru';

  @override
  String get otaHttpsWifiTipsBody =>
      'Dej desku do dosahu routeru. Preferuj pásmo 2,4 GHz, pokud má router jiný název pro 2,4 a 5 GHz. Až bude deska na síti připojená, zkus aktualizaci znovu.';

  @override
  String get otaHttpsHotspotEnabledSnack =>
      'Hotspot zapnut — případně se k Wi‑Fi desky na tomto zařízení připoj kvůli HTTP. Stažení přes HTTPS pořád vyžaduje desku na domácí Wi‑Fi s internetem.';

  @override
  String get otaHttpsBleUploadHintSnack =>
      'V Aktualizaci firmwaru níže použij „Nahrát firmware přes Bluetooth“.';

  @override
  String get connectionModeAutoShort => 'Auto';

  @override
  String get settingsOverviewTitle => 'Přehled';

  @override
  String get settingsOverviewSubtitleError =>
      'Poslední chyba — rozbal pro detail';

  @override
  String get settingsOverviewSubtitleOk => 'Zkratky a stav připojení';

  @override
  String get settingsOverviewBody =>
      'Možnosti hry a desky platí pro kartu Hra a panel Ovládání hry — jedna sada přepínačů.';

  @override
  String get settingsGoToPlayTab => 'Přejít na kartu Hra';

  @override
  String get settingsConnectionTitle => 'Připojení desky';

  @override
  String settingsConnectionSubtitle(String tier, String transport) {
    return '$tier · $transport';
  }

  @override
  String get settingsConnectionLearnMore => 'Jak funguje připojení';

  @override
  String get settingsDisconnect => 'Odpojit';

  @override
  String get settingsAdvanced => 'Pokročilé';

  @override
  String get settingsAdvancedConnectionSubtitle =>
      'Výchozí URL, režim připojení, uložené BLE';

  @override
  String get settingsAdvancedConnectionSubtitleV2 =>
      'Výchozí URL, jednorázové připojení, uložené BLE';

  @override
  String get settingsReconnectingBle => 'Obnovuji Bluetooth k uložené desce…';

  @override
  String get settingsReconnectSavedBleShort =>
      'Znovu připojit uloženou desku (BLE)';

  @override
  String get settingsManualEntry => 'Ruční zadání';

  @override
  String get settingsSavedDefaultsTitle => 'Uložené výchozí hodnoty';

  @override
  String get settingsSavedDefaultsBody =>
      'Použijí se při příštím připojení z obrazovky „Najít desku“ nebo po znovupřipojení.';

  @override
  String get settingsConnectionModeLabel => 'Režim připojení';

  @override
  String get settingsConnectionModeAutoBleWifi => 'Auto (BLE → Wi‑Fi)';

  @override
  String get settingsNextConnectionTitle => 'Příští připojení (jednorázově)';

  @override
  String get settingsNextConnectionIntro =>
      'Výchozí je automatika. Níže platí jen pro příští pokus o připojení (Najít desku, znovupřipojení nebo obnovení aplikace), pak se znovu použije automatické řízení, aby nic „nezůstalo zamčené“.';

  @override
  String get settingsNextConnectionWifiOnceButton => 'Příště: jen Wi‑Fi';

  @override
  String get settingsNextConnectionBleOnceButton => 'Příště: jen Bluetooth';

  @override
  String get settingsNextConnectionUseAuto => 'Zpět na automatiku';

  @override
  String get settingsNextConnectionActiveWifiOnce =>
      'Příští pokus přeskočí Bluetooth a použije jen uloženou adresu Wi‑Fi.';

  @override
  String get settingsNextConnectionActiveBleOnce =>
      'Příští pokus zůstane na Bluetooth a nepřepne se sám na Wi‑Fi.';

  @override
  String get settingsDefaultBoardUrl => 'Výchozí URL desky';

  @override
  String get settingsInvalidUrlSnack =>
      'Neplatná URL — zadej hostitele (např. 192.168.4.1 nebo http://…)';

  @override
  String get settingsSavedSnack => 'Uloženo';

  @override
  String get settingsSaveUrl => 'Uložit URL';

  @override
  String get settingsBoardApiTokenLabel => 'API token desky';

  @override
  String get settingsBoardApiTokenSubtitle =>
      '64 hex znaků z UART příkazu API_TOKEN — nutné pro admin akce přes Wi‑Fi a OTA, pokud deska vyžaduje autorizaci.';

  @override
  String get settingsSaveBoardApiToken => 'Uložit token';

  @override
  String get settingsBoardApiTokenSavedSnack => 'API token uložen';

  @override
  String get settingsBoardApiTokenClearedSnack => 'API token smazán';

  @override
  String get settingsBleOnlyTitle => 'Jen Bluetooth';

  @override
  String get settingsBleOnlySubtitle => 'Nepřepínat na Wi‑Fi po BLE';

  @override
  String get settingsWebSocketTitle => 'Snímek stavu přes WebSocket';

  @override
  String get settingsWebSocketSubtitle =>
      'Po změně znovu navázat Wi‑Fi spojení';

  @override
  String get settingsAppFactoryResetTitle => 'Reset aplikace na tomto zařízení';

  @override
  String get settingsAppFactoryResetSubtitle =>
      'Obnoví aplikaci do výchozího stavu a smaže uložená data v tomto zařízení. Šachovnice tím není ovlivněna.';

  @override
  String get settingsAppFactoryResetButton => 'Smazat data aplikace';

  @override
  String get settingsAppFactoryResetConfirmTitle => 'Opravdu resetovat?';

  @override
  String get settingsAppFactoryResetConfirmBody =>
      'Akci nelze vrátit. Znovu připojíš desku a zadáš případné API klíče.';

  @override
  String get settingsAppFactoryResetConfirmAction => 'Resetovat';

  @override
  String get settingsAppFactoryResetDoneSnack => 'Data aplikace byla smazána.';

  @override
  String get settingsAppFactoryResetRestartHint =>
      'Když něco nedává smysl, aplikaci úplně ukonči a znovu spusť.';

  @override
  String get transportShortDemo => 'Demo';

  @override
  String get transportShortDash => '—';

  @override
  String get settingsBoardAppearanceTitle => 'Vzhled desky';

  @override
  String settingsBoardAppearanceSubtitle(
      String layout, String zoom, String style) {
    return '$layout · $zoom · $style';
  }

  @override
  String get settingsLayoutBoardOnlyShort => 'Jen deska';

  @override
  String get settingsLayoutFullUiShort => 'Plné rozhraní';

  @override
  String get settingsPlayTabGamePanel => 'Karta Hra a panel hry';

  @override
  String get settingsLayoutLabel => 'Rozvržení';

  @override
  String get settingsLayoutBoardSegment => 'Deska';

  @override
  String get settingsLayoutBoardTooltip => 'Jen deska — minimum rozhraní';

  @override
  String get settingsLayoutFullSegment => 'Plné';

  @override
  String get settingsLayoutFullTooltip => 'Standard — hodiny a ovládání';

  @override
  String get settingsBoardZoom => 'Zvětšení desky';

  @override
  String get settingsSquareColors => 'Barvy polí';

  @override
  String get settingsBoardThemeLabel => 'Motiv';

  @override
  String get settingsBoardOptions => 'Možnosti desky';

  @override
  String get settingsCoordinatesTitle => 'Souřadnice';

  @override
  String get settingsCoordinatesSubtitle => 'Popisky a–h a 1–8';

  @override
  String get settingsPieceMotionTitle => 'Pohyb figurek';

  @override
  String get settingsPieceMotionSubtitle => 'Animované tahy na desce';

  @override
  String get settingsFlipBoardTitle => 'Otočit desku';

  @override
  String get settingsRemoteMovesTitle => 'Tahy z aplikace';

  @override
  String get settingsRemoteMovesSubtitle => 'Hrát z aplikace, ne jen z desky';

  @override
  String get settingsLiveEvalTitle => 'Živé hodnocení';

  @override
  String get settingsLiveEvalSubtitle =>
      'Stockfish — plní graf na Analýze během hry';

  @override
  String get settingsResetBoardDisplay => 'Obnovit výchozí zobrazení desky';

  @override
  String get boardStyleWooden => 'Dřevo (výchozí)';

  @override
  String get boardStyleModernDark => 'Moderní tmavý';

  @override
  String get boardStyleIceBlue => 'Ledově modrý';

  @override
  String get boardStyleForestGreen => 'Lesní zelený';

  @override
  String get boardStyleMarbleGray => 'Mramor šedý';

  @override
  String get boardStyleMidnight => 'Půlnoční';

  @override
  String get boardStyleSlate => 'Břidlicový';

  @override
  String get boardStyleCoral => 'Korálový';

  @override
  String get settingsAppAppearanceTitle => 'Vzhled aplikace';

  @override
  String get settingsAppearanceLight => 'Světlý vzhled';

  @override
  String get settingsAppearanceDark => 'Tmavý vzhled';

  @override
  String get settingsAppearanceOled => 'OLED';

  @override
  String get settingsAppearanceSystem => 'Podle systému';

  @override
  String get settingsColorSchemeLabel => 'Barevné schéma';

  @override
  String get settingsColorSchemeHelper =>
      'Světlý a tmavý režim se projeví hned. Systém kopíruje zařízení.';

  @override
  String get settingsThemeSystem => 'Systém';

  @override
  String get settingsThemeLight => 'Světlý';

  @override
  String get settingsThemeDark => 'Tmavý';

  @override
  String get settingsThemeOledBlack => 'OLED (čistá černá)';

  @override
  String get settingsHapticsTitle => 'Haptická odezva';

  @override
  String get settingsSoundTitle => 'Zvukové efekty';

  @override
  String get settingsAutoGameSummaryTitle =>
      'Po partii automaticky otevřít shrnutí';

  @override
  String get settingsCoachAiTitle => 'Trenér a AI';

  @override
  String get settingsCoachPriorityIntro =>
      'Pořadí přetahováním: aplikace zkusí prvního poskytovatele, pak dalšího při selhání. Prázdný seznam = jen krátké tipy bez připojení k internetu. Klíče zůstávají na tomto zařízení.';

  @override
  String get settingsCoachOfflineOnly => 'Jen bez cloudu';

  @override
  String get settingsCoachOpenAiOnly => 'Jen OpenAI';

  @override
  String get settingsCoachGoogleOnly => 'Jen Google';

  @override
  String get settingsCoachGroqGoogleOpenAi => 'Groq→Google→OpenAI';

  @override
  String get settingsCoachOllamaGoogle => 'Ollama→Google';

  @override
  String get settingsCoachPriorityTopLabel => 'Priorita (nahoře = první pokus)';

  @override
  String get settingsCoachNoCloudProviders =>
      'Žádný cloud — trenér používá krátké tipy v zařízení.';

  @override
  String get settingsCoachRemoveFromChain => 'Odebrat z řetězce';

  @override
  String get settingsCoachAddProvider => 'Přidat poskytovatele';

  @override
  String get settingsCoachExplanationLevel => 'Úroveň vysvětlení trenéra';

  @override
  String get settingsCoachExplanationLevelSubtitle =>
      '1 = začátečník, 4 = expert';

  @override
  String get settingsCoachLevelBeginner => '1 – začátečník';

  @override
  String get settingsCoachLevelIntermediate => '2 – středně pokročilý';

  @override
  String get settingsCoachLevelAdvanced => '3 – pokročilý';

  @override
  String get settingsCoachLevelExpert => '4 – expert';

  @override
  String get settingsCoachCredentialsHeader =>
      'Přihlašování (vyplň co používáš)';

  @override
  String get settingsCoachOpenAiProvider => 'OpenAI';

  @override
  String get settingsCoachApiKeyLabel => 'API klíč';

  @override
  String get settingsCoachOpenAiKeyHelper =>
      'platform.openai.com — jen pro trenéra.';

  @override
  String get settingsCoachModelIdLabel => 'ID modelu';

  @override
  String get settingsCoachOpenAiKeysButton => 'OpenAI klíče';

  @override
  String get settingsCoachGoogleAiStudio => 'Google AI Studio';

  @override
  String get settingsCoachPasteGoogleKeyHint => 'Vlož klíč z Google AI Studio';

  @override
  String get settingsCoachGoogleKeyHelper =>
      'Klíč na aistudio.google.com — Gemini / Gemma.';

  @override
  String get settingsCoachCustomModel => 'Vlastní…';

  @override
  String get settingsCoachCustomModelId => 'Vlastní ID modelu';

  @override
  String get settingsCoachCustomModelHint => 'např. gemini-2.5-flash';

  @override
  String get settingsCoachOpenAiKeyHint => 'sk-…';

  @override
  String get settingsCoachOpenAiFieldLabel => 'API klíč';

  @override
  String get settingsCoachOpenAiKeyTooltip => 'Otevřít stránku klíčů OpenAI';

  @override
  String get settingsCoachOpenAiModelHint => 'gpt-4o-mini';

  @override
  String get settingsCoachGroqKeyHint => 'Vlož Groq API klíč';

  @override
  String get settingsCoachGroqModelHint => 'llama-3.3-70b-versatile';

  @override
  String get settingsCoachGroqModelHelper =>
      'Musí odpovídat povolenému modelu Groq.';

  @override
  String get settingsCoachXaiKeyHint => 'Vlož xAI API klíč';

  @override
  String get settingsCoachXaiModelHint => 'grok-2-latest';

  @override
  String get settingsCoachOllamaBaseHint => 'http://127.0.0.1:11434/v1';

  @override
  String get settingsCoachOllamaModelHint => 'llama3.2';

  @override
  String get settingsCoachGetGoogleKey => 'Získat Google AI klíč';

  @override
  String get settingsCoachGroqTitle => 'Groq (kompatibilní s OpenAI)';

  @override
  String get settingsCoachXaiTitle => 'xAI (Grok)';

  @override
  String get settingsCoachOllamaTitle => 'Ollama (lokálně)';

  @override
  String get settingsCoachBaseUrlLabel => 'Základní URL';

  @override
  String settingsCoachChainSubtitle(String chain) {
    return '$chain';
  }

  @override
  String get settingsDeveloperModeUnlockedSnack => 'Vývojářský režim odemčen.';

  @override
  String get settingsDeveloperModeDisabledSnack => 'Vývojářský režim vypnut.';

  @override
  String get gameControlDisplaySection => 'Zobrazení';

  @override
  String get gameControlPlayModeSection => 'Režim hry';

  @override
  String get gameControlSandboxLabel => 'Volná pozice';

  @override
  String get gameControlMovesFromApp => 'Tahy z aplikace';

  @override
  String gameControlUndoMove(int n) {
    return 'Vrátit tah ($n)';
  }

  @override
  String get gameControlExitSandbox => 'Ukončit volnou pozici a obnovit desku';

  @override
  String get gameControlLearningModeTitle => 'Výukový režim';

  @override
  String get gameControlLearningModeSubtitle => 'Vysvětlení trenéra v aplikaci';

  @override
  String get gameControlExplanationLevelSettings =>
      'Úroveň vysvětlení (1–4) v Nastavení';

  @override
  String get gameControlActionsSection => 'Akce';

  @override
  String get gameControlNewGameEllipsis => 'Nová hra…';

  @override
  String get gameControlRefreshState => 'Obnovit stav';

  @override
  String get gameControlPanelTitle => 'Ovládání hry';

  @override
  String get timerUnavailable => 'Časomíra: vypnutá nebo nedostupná';

  @override
  String get timerHiddenForPuzzleMode =>
      'Živá partie na desce zde neběží — hodiny skryjeme, dokud řešíte úlohu nebo zkoumáte pozici.';

  @override
  String get timerWhiteShort => 'Bílá';

  @override
  String get timerBlackShort => 'Černá';

  @override
  String get coachChatTypeFirst => 'Nejdřív napiš otázku, pak Odeslat.';

  @override
  String get coachChatExplanationLevelLabel => 'Hloubka vysvětlení';

  @override
  String get coachChatHide => 'Skrýt';

  @override
  String get coachChatDismiss => 'Zavřít';

  @override
  String get manualConnTitle => 'Ruční připojení';

  @override
  String manualConnWifiStatusError(String error) {
    return 'Stav Wi‑Fi: $error';
  }

  @override
  String get manualConnEnterUrlSnack => 'Zadej URL desky.';

  @override
  String get manualConnStaDisconnectedSnack => 'STA odpojeno (příkaz odeslán).';

  @override
  String get manualConnClearWifiTitle => 'Smazat uloženou Wi‑Fi z NVS?';

  @override
  String get manualConnClearWifiBody => 'ESP zapomene uloženou síť.';

  @override
  String get manualConnNvsClearedSnack => 'Wi‑Fi údaje v NVS vymazány.';

  @override
  String get manualConnSaveUrlSnack => 'URL uložena mezi předvolby.';

  @override
  String get manualConnSaveUrl => 'Uložit URL';

  @override
  String get manualConnTestConnection => 'Test spojení (načíst snímek GET)';

  @override
  String get manualConnConnectSession =>
      'Připojit přes Wi‑Fi (dotazy + synchronizace)';

  @override
  String get manualConnWifiStaNvs => 'Wi‑Fi STA a NVS';

  @override
  String get manualConnRefreshWifi => 'Obnovit stav Wi‑Fi';

  @override
  String get manualConnDisconnectSta => 'Odpojit ESP od STA';

  @override
  String get manualConnClearWifiNvs => 'Smazat Wi‑Fi z NVS';

  @override
  String get manualConnClearConfirmAction => 'Smazat';

  @override
  String get manualConnUrlLabelDev => 'Základní URL desky (http://…)';

  @override
  String get manualConnUrlLabelUser => 'Adresa šachovnice';

  @override
  String get manualConnUrlHintDev => 'http://192.168.x.x';

  @override
  String get manualConnUrlHintUser => 'Adresa v lokální síti';

  @override
  String get manualConnBleInfoTile =>
      'Jsi připojen přes Bluetooth. Pro síťové funkce zadej adresu desky.';

  @override
  String get manualConnStaSectionDev =>
      'Vyžaduje HTTP dosah na desku (stejná LAN nebo přístupový bod desky). Jen BLE bez IP se tato rozhraní nevolají.';

  @override
  String get manualConnStaSectionUser =>
      'Tato sekce slouží pro ruční správu síťového připojení desky.';

  @override
  String get manualConnTestFailedUser => 'Spojení se nepodařilo navázat.';

  @override
  String get haMqttTitle => 'Home Assistant a MQTT';

  @override
  String get haMqttFormLead =>
      'Zadej adresu MQTT brokeru, ke kterému se má šachovnice připojit (často stejný počítač jako Home Assistant).';

  @override
  String get haMqttSetupHelpLink => 'Jak nastavit MQTT a Home Assistant?';

  @override
  String get haMqttSavedSnack => 'MQTT uloženo na desku.';

  @override
  String get haMqttHowToHa => 'Jak na Home Assistant';

  @override
  String get haMqttBrokerHeader => 'Server MQTT';

  @override
  String get haMqttSaveToBoard => 'Uložit na šachovnici';

  @override
  String get haMqttRefreshFromBoard => 'Obnovit stav z desky';

  @override
  String get haMqttBoardStatusHeader => 'Stav na desce';

  @override
  String get haMqttModeTile => 'Režim';

  @override
  String get haMqttMqttTile => 'MQTT';

  @override
  String get haMqttWifiStaTile => 'Wi‑Fi STA';

  @override
  String get haMqttGuideBody =>
      '1. Home Assistant musí běžet ve stejné Wi‑Fi síti jako šachovnice (ESP připojené jako klient STA k routeru).\n\n2. V Home Assistantu zapni MQTT broker — nejčastěji doplněk „Mosquitto broker“ (Nastavení → Doplňky). Poznamenej si port (obvykle 1883) a případné uživatelské jméno a heslo z konfigurace doplňku.\n\n3. Zjisti IP adresu nebo síťové jméno počítače / Raspberry Pi, kde HA běží (např. 192.168.1.42 nebo homeassistant.local). Šachovnice musí na tuto adresu v síti dosáhnout.\n\n4. Níže zadej stejnou adresu jako u položky „Adresa serveru MQTT“ — jde o adresu MQTT serveru (u běžné instalace stejný stroj jako Home Assistant). Port nech 1883, pokud jsi v Mosquitto neměnil výchozí.\n\n5. Uložením odešleš nastavení firmwaru desky přes aktuální spojení (Wi‑Fi nebo Bluetooth). Aplikace CZECHMATE se k MQTT serveru sama nepřipojuje — data posílá jen ESP.';

  @override
  String get haMqttHostFieldLabel =>
      'Adresa serveru MQTT (např. IP počítače s Home Assistant)';

  @override
  String get haMqttPortFieldLabel => 'Port';

  @override
  String get haMqttUserFieldLabel => 'Uživatel (volitelné)';

  @override
  String get haMqttPasswordFieldLabel => 'Heslo (volitelné)';

  @override
  String get haMqttFooterMock =>
      'Ukázková šachovnice MQTT nepodporuje — připoj reálnou desku.';

  @override
  String get haMqttFooterConnectFirst =>
      'Nejdřív připoj šachovnici (Bluetooth nebo Wi‑Fi), pak můžeš uložit nastavení MQTT.';

  @override
  String get haMqttFooterBleSave =>
      'Nastavení MQTT uložíš i přes Bluetooth; stav z desky (tlačítko výše) se načte až přes HTTP po Wi‑Fi.';

  @override
  String get haMqttFooterTroubleshoot =>
      'Pokud zůstane MQTT odpojené, zkontroluj firewall na počítači s HA, přístupové údaje Mosquitto a že ESP je ve stejné síti jako MQTT server.';

  @override
  String get haMqttStateConnected => 'připojeno';

  @override
  String get haMqttStateDisconnected => 'nepřipojeno';

  @override
  String get haMqttWifiOk => 'Dobře';

  @override
  String get haMqttWifiNoLink => 'bez spojení';

  @override
  String get firmwareDailySecondConfirmBody =>
      'Spustit aktualizaci? Deska stáhne firmware, zapíše flash a restartuje. Nepřerušuj napájení.';

  @override
  String get firmwareDailySecondConfirmTitle => 'Potvrdit aktualizaci';

  @override
  String get profileTitle => 'Profil a Elo';

  @override
  String get profileDisplayName => 'Zobrazované jméno';

  @override
  String get profileNameSavedSnack => 'Jméno uloženo';

  @override
  String get profileSaveName => 'Uložit jméno';

  @override
  String get profileAvatar => 'Avatar';

  @override
  String get profileFromGallery => 'Z galerie';

  @override
  String profileGalleryError(String error) {
    return 'Galerie: $error';
  }

  @override
  String get profileNameHint => 'Jméno zobrazené v profilu';

  @override
  String profilePuzzleEloLine(String elo) {
    return 'Puzzle Elo: $elo';
  }

  @override
  String profileWeekStatsLine(int solved, int failed) {
    return 'Posledních 7 dní: $solved vyřešeno · $failed špatných linií';
  }

  @override
  String get profileHeatmapTitle => 'Posledních 28 dní';

  @override
  String get profileHeatmapCaption =>
      'Tmavší = více pokusů; zelenější = vyšší úspěšnost.';

  @override
  String get profileAvatarHint => 'Předvolené ikony nebo fotka z galerie.';

  @override
  String get profileEloHelpBody =>
      'Hodnocení Elo se mění po vyřešení klasifikovaného hlavolamu se známou výherní linií (např. denní úlohy na Lichess).';

  @override
  String get firmwareManifestSavedSnack => 'URL manifestu uložena.';

  @override
  String get firmwareUpdateBoardTitle => 'Aktualizovat firmware desky?';

  @override
  String firmwareNewVersionLine(String ver, String boardVer) {
    return 'Nová verze: $ver — na desce: $boardVer.';
  }

  @override
  String get firmwareUpdateIntroBody =>
      'Deska stahuje soubor přes HTTPS. Udrž napájení a Wi‑Fi stabilní.';

  @override
  String get firmwareFinalConfirmTitle => 'Konečné potvrzení';

  @override
  String get firmwareFinalConfirmBody => 'Spustit OTA na desce teď?';

  @override
  String get firmwareYesUpdate => 'Ano, aktualizovat';

  @override
  String get firmwareDailyRemindersTitle => 'Denní připomínky aktualizace';

  @override
  String get firmwareDailyRemindersSubtitle =>
      'Upozornit, když je dostupnější firmware.';

  @override
  String get firmwareSaveManifestUrl => 'Uložit URL manifestu';

  @override
  String get firmwareCheckForUpdate => 'Zkontrolovat aktualizaci';

  @override
  String firmwareNewVersionChip(String ver) {
    return 'Nová verze $ver';
  }

  @override
  String firmwareDeveloperReflashChipTitle(String ver) {
    return 'Znovu nahrát $ver (vývojář)';
  }

  @override
  String firmwareDeveloperDowngradeChipTitle(String ver) {
    return 'Starší build $ver (vývojář)';
  }

  @override
  String get firmwareDeveloperSameVersionBanner =>
      'Vývojářský režim: manifest má stejnou verzi jako deska — stejný build můžeš znovu stáhnout a nahrát (oprava / ověření).';

  @override
  String get firmwareDeveloperOlderManifestBanner =>
      'Vývojářský režim: manifest má nižší verzi než firmware na desce. Downgrade může rozbít kompatibilitu nebo data — pokračuj jen pokud víš, co děláš.';

  @override
  String get firmwareDownloadOnEspNote =>
      'Stahování probíhá na ESP přes HTTPS.';

  @override
  String get firmwareConfirmUpdateTitle => 'Potvrdit aktualizaci';

  @override
  String get firmwareConfirmUpdateBody =>
      'Spustit aktualizaci firmware na desce?';

  @override
  String get firmwareStartingOtaSnack => 'Spouštím OTA na desce…';

  @override
  String get firmwareDownloadToApp => 'Stáhnout do aplikace';

  @override
  String get firmwareSendToBoard => 'Odeslat na desku';

  @override
  String get firmwareTwoStepOtaHint =>
      'Soubor .bin stáhni, dokud máš internet. Pak se připoj k hotspotu desky a odešli — deska si soubor stáhne z tohoto zařízení přes HTTP (na desce nemusí být Wi‑Fi STA).';

  @override
  String firmwareCachedInAppLine(String ver, String mb) {
    return 'Uloženo v aplikaci: v$ver (~$mb MB)';
  }

  @override
  String firmwareSavedFirmwareChipTitle(String ver) {
    return 'Uložený firmware v$ver';
  }

  @override
  String get firmwareOfflineSavedFirmwareBanner =>
      'Živý manifest se nepodařilo načíst. Pořád můžeš nahrát soubor firmwaru uložený v aplikaci.';

  @override
  String firmwareCachedDiffersFromManifestHint(
      String remoteVer, String savedVer) {
    return 'V aplikaci máš uložený firmware v$savedVer; manifest teď ukazuje v$remoteVer. Soubor jsme nesmazali — můžeš ho kdykoli nahrát, nebo stáhnout znovu.';
  }

  @override
  String get firmwareDownloadSavedSnack => 'Firmware uložen v aplikaci.';

  @override
  String firmwareDownloadFailedLine(String error) {
    return 'Stahování selhalo: $error';
  }

  @override
  String get firmwareJoinHotspotForUpload =>
      'Nejdřív se připoj k Wi‑Fi hotspotu desky — toto zařízení potřebuje adresu v rozsahu 192.168.4.x.';

  @override
  String get firmwareNoCachedFirmware =>
      'Nejdřív si firmware stáhni do aplikace.';

  @override
  String get firmwareSendToBoardTitle => 'Nahrát z tohoto zařízení?';

  @override
  String get firmwareSendToBoardBody =>
      'Deska si soubor .bin stáhne přes HTTP z tohoto zařízení. Zůstaň na hotspotu a nech tuto obrazovku otevřenou do dokončení.';

  @override
  String get firmwareOneStepHttpsOta =>
      'Nebo: deska stáhne přes HTTPS (potřebuje klienta STA)';

  @override
  String get firmwareSendViaBle => 'Odeslat přes Bluetooth';

  @override
  String get firmwareSendViaBleBody =>
      'Celý obraz firmwaru se přenese jen přes Bluetooth (nepotřebuješ hotspot desky ani STA). Drž toto zařízení blízko desky, dokud spojení neskončí — deska se restartuje.';

  @override
  String get firmwareBleUploadDoneSnack =>
      'Odeslání přes Bluetooth dokončeno — deska se může restartovat.';

  @override
  String get firmwareBleOtaReturnedFromBackgroundSnack =>
      'Vrátil(a) ses během přenosu firmwaru přes Bluetooth. Pokud se nic neděje, nech CzechMate v popředí a toto zařízení blízko desky.';

  @override
  String get firmwareOtaHttpMayLeaveAppHint =>
      'Systém se při práci s HTTPS odkazem může na chvíli přepnout jinam — to je v pořádku. Znovu otevři CzechMate a sleduj průběh přes HTTP adresu desky.';

  @override
  String get firmwareBleOtaKeepForegroundWarning =>
      'Nech CzechMate v popředí a obrazovku nezamykaj, dokud přenos přes Bluetooth neskončí.';

  @override
  String get firmwareBleOtaApHotspotTip =>
      'Nahráváš-li přes hotspot desky, nejdřív se připoj k Wi‑Fi desky, aby toto zařízení dosáhlo na http://192.168.4.1.';

  @override
  String get firmwareBleOtaPausedReconnectDetail =>
      'Přenos přes Bluetooth pozastaven — obnovuji spojení s deskou…';

  @override
  String get firmwareBleOtaResumedTransferDetail =>
      'Spojení obnoveno — pokračuji v přenosu firmwaru.';

  @override
  String get firmwareOtaSlotsDisabledHint =>
      'Toto rozložení flash nemá sloty OTA (ota_0 + ota_1). Použij USB/UART (esptool nebo idf.py flash), nebo přebuduj firmware s tabulkou oddílů pro dvě OTA (viz CSV oddílů v projektu).';

  @override
  String get firmwareBoardHttpMissingDetail =>
      'Chybí HTTP adresa desky (např. http://192.168.4.1). V nastavení ulož výchozí URL desky. Samotné Bluetooth IP neposkytuje.';

  @override
  String get firmwareHttpsLinkExplainBody =>
      'Ty jen pošleš odkaz HTTPS — celý soubor .bin stáhne ESP. Deska musí být připojená k Wi‑Fi jako klient (STA). Příkaz ke startu můžeš poslat přes HTTP na Wi‑Fi nebo přes Bluetooth; průběh se čte přes HTTP na IP desky.';

  @override
  String get firmwareSettingsSecondConfirmBody =>
      'Deska zapíše firmware a restartuje. Nepřerušuj napájení ani Wi‑Fi během stahování. Pokračovat?';

  @override
  String get firmwareOtaFinishedMaybeRebootSnack =>
      'OTA skončila nebo spadlo spojení — deska se může restartovat.';

  @override
  String get firmwareOtaSuccessTitle => 'Firmware nahrán';

  @override
  String firmwareOtaSuccessBody(String installedVer, String reportedVer) {
    return 'Nahraná verze: $installedVer.\n\nVerze desky v aplikaci: $reportedVer.\n\nKdyž se deska restartovala, počkej na obnovení spojení nebo se znovu připoj v Nastavení → Připojení.';
  }

  @override
  String firmwareTileTitleUpdateAvailable(String ver) {
    return 'Firmware — dostupná aktualizace ($ver)';
  }

  @override
  String get firmwareOtaRollbackBannerTitle =>
      'Po neúspěšném startu je zpět předchozí firmware';

  @override
  String firmwareOtaRollbackBannerBodyWithAttempt(
      String current, String slot, String attempt) {
    return 'Deska běží na $current: nový obraz se nespustil. Neúspěšná aktualizace ($attempt) zůstává ve slotu $slot. Až bude opravený build, nainstaluj ho.';
  }

  @override
  String firmwareOtaRollbackBannerBodyNoAttempt(String current, String slot) {
    return 'Deska běží na $current: nový obraz se nespustil. Neúspěšný obraz je ve slotu $slot. Až bude opravený build, nainstaluj ho.';
  }

  @override
  String get firmwareTileTitleDefault => 'Firmware (bezdrátově)';

  @override
  String get firmwareTileSubtitleIdle =>
      'Manifest a Kontrola. Na hotspotu použij Stáhnout / Odeslat, nebo HTTPS, pokud má deska klienta STA.';

  @override
  String get firmwareRemindersOnShort => 'připomínky zapnuté';

  @override
  String get firmwareRemindersOffShort => 'připomínky vypnuté';

  @override
  String get firmwareDailyRemindersSubtitleLong =>
      'Je-li na serveru novější verze, připomínej každý den, dokud neaktualizuješ nebo nepřepneš připomínky.';

  @override
  String get firmwareDeveloperManifestSectionTitle => 'Možnosti pro vývojáře';

  @override
  String get firmwareDeveloperManifestSectionBody =>
      'URL manifestu měň jen při vlastním kanálu firmware. Jinak aplikace používá výchozdí zdroj.';

  @override
  String get firmwareManifestUrlLabel => 'URL manifestu (version.json)';

  @override
  String get firmwareManifestUrlHint => 'https://…/version.json';

  @override
  String get firmwareManifestUrlHelper =>
      'Výchozí URL se doplní sama. Vymaž pole a ulož, pokud chceš jen vestavěný manifest.';

  @override
  String firmwareBoardHttpVersionLine(String ver) {
    return 'Deska (HTTP): $ver';
  }

  @override
  String firmwareManifestVersionLine(String ver) {
    return 'Manifest: $ver';
  }

  @override
  String firmwareWifiStaConnected(String ip) {
    return 'připojeno ($ip)';
  }

  @override
  String get firmwareWifiStaNotConnected => 'nepřipojeno';

  @override
  String firmwareWifiStaLine(String status) {
    return 'Wi‑Fi STA: $status';
  }

  @override
  String get firmwareNeedDefaultBoardUrlHint =>
      'Pro čtení verze desky nastav a ulož výchozí URL desky (IP AP nebo STA).';

  @override
  String firmwareOtaPercentLine(int percent) {
    return 'OTA $percent %';
  }

  @override
  String firmwareFooterTransportHint(String transport) {
    return 'Spojení: $transport. HTTPS OTA vyžaduje STA; OTA přes hotspot HTTP funguje na 192.168.4.x; uložený .bin lze nahrát i přes Bluetooth, pokud je navázané GATT.';
  }

  @override
  String get firmwareTransportLabelNone => 'žádné';

  @override
  String get firmwareTransportLabelWifi => 'Wi‑Fi';

  @override
  String get firmwareTransportLabelBle => 'Bluetooth';

  @override
  String get firmwareTransportLabelMock => 'simulace';

  @override
  String get errWifiSsidEmpty => 'Zadej název Wi‑Fi sítě (SSID).';

  @override
  String get errWifiProvNeedsBle =>
      'Pro nastavení Wi‑Fi na desce se připoj přes Bluetooth.';

  @override
  String get firmwareWifiBleProvStartedSnack =>
      'Údaje Wi‑Fi odeslány — sleduj stav na desce.';

  @override
  String get firmwareOtaNoLanRouteUseBle =>
      'Toto zařízení není ve stejné síti jako deska. Nahraj firmware přes Bluetooth, nebo se připoj k síti desky.';

  @override
  String get firmwareOtaNoLanRouteNeedBle =>
      'Nejdřív se připoj přes Bluetooth — bez přístupu do LAN aplikace obrázek pošle přes BLE.';

  @override
  String get firmwareOtaPhoneNotOnLan =>
      'Toto zařízení zřejmě není v síti desky. Připoj se k hotspotu desky nebo k Wi‑Fi, kde má deska IP.';

  @override
  String firmwareTileTitleGitBle(String ver) {
    return 'Firmware — sestavení z GitHubu ($ver)';
  }

  @override
  String firmwareTileTitleDeveloperReflash(String ver) {
    return 'Firmware — znovu nahrát ($ver)';
  }

  @override
  String firmwareTileTitleDeveloperDowngrade(String ver) {
    return 'Firmware — starší manifest ($ver)';
  }

  @override
  String get firmwareTileSubtitleBleGitOnly =>
      'Jen Bluetooth — stáhni .bin do aplikace a odešli přes BLE nebo HTTP na hotspotu desky.';

  @override
  String get firmwareTileSubtitleDeveloperReflash =>
      'Vývojářský režim — manifest sedí s deskou; otevři obrazovku a znovu nahraj stejný build.';

  @override
  String get firmwareTileSubtitleDeveloperDowngrade =>
      'Vývojářský režim — manifest je starší než deska; volitelný downgrade z této URL.';

  @override
  String get firmwareWifiBleProvisionTitle => 'Wi‑Fi na desce (přes Bluetooth)';

  @override
  String get firmwareWifiBleProvisionSubtitle =>
      'Pošli SSID a heslo přes BLE, aby se deska připojila k tvé síti jako klient (STA).';

  @override
  String get firmwareWifiBleSsidLabel => 'Název Wi‑Fi (SSID)';

  @override
  String get firmwareWifiBlePasswordLabel => 'Heslo Wi‑Fi';

  @override
  String get firmwareWifiBleUsePhoneSsidButton =>
      'Použít název Wi‑Fi z tohoto zařízení';

  @override
  String get firmwareWifiBleSendCredentials => 'Odeslat údaje na desku';

  @override
  String firmwareBleStaOk(String ssid, String ip) {
    return 'STA: $ssid · $ip';
  }

  @override
  String get firmwareBleStaWaiting => 'Čekám na připojení desky k Wi‑Fi…';

  @override
  String get firmwareBleHttpOptionalHint =>
      'HTTP adresa desky je volitelná — aktualizaci zvládneš přes Bluetooth i bez Wi‑Fi na desce.';

  @override
  String get firmwareBleGitUnknownBoardVersion =>
      'Verze desky přes HTTP neznámá — stáhni do aplikace a preferuj nahrání přes Bluetooth.';

  @override
  String get firmwareSendViaBlePrimary => 'Odeslat přes Bluetooth (doporučeno)';

  @override
  String get firmwareSendToBoardHttpAlt => 'Odeslat přes HTTP desky (hotspot)';

  @override
  String get analysisAppBarTitle => 'Analýza';

  @override
  String get analysisGameProgress => 'Průběh partie';

  @override
  String get analysisGameProgressBody => 'Půltahy z připojené desky.';

  @override
  String get analysisStockfishSection => 'Stockfish';

  @override
  String get analysisStockfishSubtitle =>
      'Řádek hodnocení z aktuální pozice na desce.';

  @override
  String get analysisNoBoardPosition => 'Žádná pozice z desky';

  @override
  String get analysisNoBoardPositionBody =>
      'Připoj Wi‑Fi nebo Bluetooth a otevři Hru.';

  @override
  String get analysisOpenPlay => 'Otevřít Hru';

  @override
  String get analysisChartDisabledTitle => 'Graf a hodnocení tahů jsou vypnuté';

  @override
  String get analysisChartDisabledSubtitle =>
      'Zapni hodnocení tahů pro skóre kvality.';

  @override
  String analysisChartJumpedToMove(int moveNumber) {
    return 'V Hře je otevřen polo-tah $moveNumber.';
  }

  @override
  String get analysisScoresheetEmpty => 'Zatím žádné tahy.';

  @override
  String get analysisEnableMoveEval => 'Povolit hodnocení tahů (Stockfish)';

  @override
  String get analysisGameOverview => 'Přehled partie';

  @override
  String get analysisGameOverviewBody =>
      'Materiál, hodiny a zahájení, pokud jsou k dispozici.';

  @override
  String get analysisMoveQuality => 'Kvalita tahů';

  @override
  String get analysisMoveQualityBody =>
      'Průměr 0–100 podle stupně Stockfish a Ø ztráta v cp.';

  @override
  String analysisQualitySummaryLine(String quality, String avgCp, int count) {
    return '$quality$avgCp · $count tahů';
  }

  @override
  String get analysisLastMoveEvalTitle => 'Poslední zhodnocení tahu';

  @override
  String get analysisMoveHistoryTitle => 'Historie tahů';

  @override
  String get analysisMoveHistoryTapHint =>
      'Klepnutím otevřeš chronologický průběh.';

  @override
  String get analysisSecondLineTitle => 'Druhá varianta';

  @override
  String get analysisSecondLineBody => 'Alternativní pokračování z enginu.';

  @override
  String get analysisCustomPositionTitle => 'Vlastní pozice';

  @override
  String get analysisCustomPositionBody => 'Analýza bez fyzické desky.';

  @override
  String analysisDepthLabel(String n) {
    return 'Hloubka: $n';
  }

  @override
  String get analysisAnalyzePosition => 'Analyzovat pozici';

  @override
  String get analysisSandboxFenSnack => 'Trénink z FEN — záložka Hra.';

  @override
  String get analysisPreviewInPlay => 'Náhled v Hře';

  @override
  String get analysisPreviewMoveTitle => 'Náhled pozice a tahu';

  @override
  String get puzzleRoundExpiredSnack => 'Čas kola vypršel.';

  @override
  String get puzzleConnectBoardSnack => 'Připoj desku (Wi‑Fi nebo BLE).';

  @override
  String get puzzleNewGameSentSnack => 'Příkaz new_game odeslán.';

  @override
  String get puzzleBoardRiddleTitle => 'Hádanka na desce';

  @override
  String get puzzleTryOnScreen => 'Zkusit vyřešit na obrazovce';

  @override
  String get puzzleLoadToBoard => 'Nahrát na desku (new_game + FEN)';

  @override
  String get puzzleSetupWizardLabel => 'Průvodce postavením na desce';

  @override
  String get puzzleOpenLichess => 'Otevřít na Lichess';

  @override
  String get puzzleSavePositionTitle => 'Uložit pozici';

  @override
  String get puzzleSavedSnack => 'Uloženo.';

  @override
  String get puzzleAddToLibrary => 'Přidat do knihovny';

  @override
  String get puzzleCustomTitle => 'Vlastní';

  @override
  String get puzzleSetupOnBoard => 'Průvodce na desce';

  @override
  String puzzleDailyTitle(String id) {
    return 'Denní $id';
  }

  @override
  String get puzzleDailySolveTitle => 'Denní hlavolam';

  @override
  String puzzleSolutionMoves(int n) {
    return 'Řešení: $n tahů';
  }

  @override
  String get puzzlePoolModeTitle => 'Režim fondu';

  @override
  String get puzzlePoolMixed => 'Mix';

  @override
  String get puzzlePoolBundled => 'Vestavěné';

  @override
  String get puzzlePoolLibrary => 'Knihovna';

  @override
  String get puzzleTimedRoundTitle => 'Časované kolo';

  @override
  String get puzzleTimedRoundBody =>
      'Náhodná pozice z vybraného fondu, odpočet v sekundách.';

  @override
  String get puzzleNewRound => 'Nové kolo';

  @override
  String get puzzleStop => 'Zastavit';

  @override
  String puzzleRemainingSeconds(int n) {
    return 'Zbývá: ${n}s';
  }

  @override
  String get puzzleSolveOnScreen => 'Řešit na obrazovce';

  @override
  String get puzzleLoadToBoardShort => 'Nahrát na desku';

  @override
  String get puzzleSetupWizardShort => 'Průvodce postavením';

  @override
  String get puzzleBundledOfflineTitle => 'Vestavěné pozice (bez internetu)';

  @override
  String get puzzleThemeMate => 'Mat';

  @override
  String get puzzleThemeFork => 'Vidlička';

  @override
  String get puzzleThemeEndgame => 'Koncovka';

  @override
  String get puzzleThemeMixed => 'Mix';

  @override
  String get progressAppBarTitle => 'Pokrok';

  @override
  String get progressSegBeginner => 'Zač.';

  @override
  String get progressSegIntermediate => 'Str.';

  @override
  String get progressSegAdvanced => 'Pokr.';

  @override
  String get progressSegExpert => 'Exp.';

  @override
  String get progressTabLearn => 'Výuka';

  @override
  String get progressTabStats => 'Statistiky';

  @override
  String get progressWelcomeTitle => 'Vítej v CzechMate';

  @override
  String get progressWelcomeBody =>
      'Sleduj hlavolamy, trenéra a nastavení desky na jednom místě.';

  @override
  String get progressProfilePuzzleEloTitle => 'Profil a puzzle Elo';

  @override
  String get progressProfilePuzzleEloSubtitle =>
      'Místní přezdívka a hodnocení v hlavolamech.';

  @override
  String get progressLearnCardTitle => 'Výuka';

  @override
  String get progressLearnCardSubtitle => 'LED průvodci postavením a AI trenér';

  @override
  String get progressAiCoachTitle => 'AI trenér';

  @override
  String get progressAiCoachBody =>
      'Zapni výukový režim na kartě Hra. Chat používá cloudové rozhraní API nebo lokální zálohu; Stockfish zůstává na kartě Analýza.';

  @override
  String get progressLearningModeTile => 'Výukový režim';

  @override
  String get progressCoachLevelLabel => 'Úroveň trenéra';

  @override
  String get progressCoachChatButton => 'Chat s trenérem';

  @override
  String get progressPositionPlanButton => 'Plán pozice';

  @override
  String get progressBoardErrorTitle => 'Problém s připojením';

  @override
  String get progressStartingPositionTitle => 'Výchozí postavení';

  @override
  String get progressStartingPositionBody =>
      'Průvodce srovná figury pomocí světel na desce.';

  @override
  String get progressStartingPositionTeachingBody =>
      'Výukový režim: světla na desce ukazují pole postupně; aplikace říká, kterou figurku položit (stejný postup jako na iOS).';

  @override
  String get progressRunWizardStarting => 'Průvodce výchozím postavením';

  @override
  String get progressAccountCardTitle => 'Účet';

  @override
  String get progressAccountCardSubtitle => 'Místní profil a aktivita';

  @override
  String get progressActiveTransportTitle => 'Aktivní přenos';

  @override
  String get progressNoSessionTitle => 'Zatím žádné aktivní připojení k desce';

  @override
  String get progressNoSessionSubtitle => 'Otevři Najít desku pro připojení.';

  @override
  String get progressStatsSegmentSubtitle =>
      'Přehled her a statistik v aplikaci';

  @override
  String get progressProfileIconTooltip => 'Profil a Elo';

  @override
  String get progressLearningModePlayHint =>
      'Zapni výukový režim na kartě Hra → Ovládání hry pro chat a plán pozice.';

  @override
  String get progressWizardConnectHint =>
      'Pro průvodce se světly na desce ji připoj (indikátor připojení na kartě Hra).';

  @override
  String get userFacingErrBle =>
      'Bluetooth spojení selhalo. Zapni Bluetooth, zůstaň blízko desky a zkus to znovu.';

  @override
  String get userFacingErrBlePairingReset =>
      'Bluetooth párování už nesedí s deskou. iOS / iPadOS: Nastavení → Bluetooth → ⓘ u desky → Zapomenout toto zařízení. Android: Nastavení Bluetooth → deska → Zapomenout / Odpárovat. Po továrním resetu desky vymaž i párování na desce a znovu připoj přes Najít desku.';

  @override
  String get userFacingErrNetworkReach =>
      'Desku nebo server se nepodařilo kontaktovat. Zkontroluj Wi‑Fi, přibliž se a ověř, že je deska zapnutá.';

  @override
  String get userFacingErrTimeout =>
      'Spojení vypršelo. Zkus to znovu, až bude deska zapnutá a v dosahu.';

  @override
  String get userFacingErrTls =>
      'Zabezpečené spojení (HTTPS) selhalo. Zkontroluj adresu a zda deska vyžaduje HTTPS.';

  @override
  String get userFacingErrWebSocket =>
      'Živý přenos z desky byl přerušen. Znovu připoj přes Najít desku, pokud se to opakuje.';

  @override
  String get userFacingErrGeneric =>
      'Něco se pokazilo. Zkus to znovu nebo se připoj znovu přes Najít desku.';

  @override
  String get userFacingErrDailyPuzzle =>
      'Dnešní úlohu se nepodařilo načíst. Zkontroluj internet a klepni na obnovit.';

  @override
  String get userFacingShowTechnicalDetails => 'Zobrazit technické podrobnosti';

  @override
  String get userFacingHideTechnicalDetails => 'Skrýt technické podrobnosti';

  @override
  String get userFacingBoardWebLocked =>
      'Webové rozhraní desky blokuje přístup k API. Zavři ho nebo odemkni API a zkus znovu.';

  @override
  String get userFacingBoardApiToken =>
      'Tato deska vyžaduje API token. Doplň ho v Nastavení → Uložené výchozí hodnoty.';

  @override
  String get userFacingBoardBadRequest =>
      'Deska požadavek odmítla. Zkontroluj platnost tahu nebo povinná pole.';

  @override
  String get userFacingBoardUnauthorized =>
      'Přístup byl odmítnut. Zkontroluj přihlašovací údaje, pokud je deska chráněná.';

  @override
  String get userFacingBoardNotFound =>
      'API cesta neexistuje. Firmware desky může být starší, než očekává tato aplikace.';

  @override
  String get userFacingBoardServerError =>
      'Deska nahlásila vnitřní chybu. Zkus to znovu nebo desku restartuj.';

  @override
  String get userFacingBoardUnavailable =>
      'Deska je zaneprázdněná nebo nedostupná. Počkej chvíli a zkus znovu.';

  @override
  String get userFacingBoardHttpOther =>
      'HTTP chyba z desky. Zkontroluj adresu desky a režim připojení.';

  @override
  String get userFacingErrBoardUrlHostMissing =>
      'V adrese desky chybí hostitel. V nastavení ulož celou adresu, např. http://192.168.4.1, na stejné síti jako deska.';

  @override
  String get puzzleMoreActionsTooltip => 'Další akce';

  @override
  String get gameClockSectionLabel => 'Hodiny a LED na desce';

  @override
  String get progressStatsCurrentMoves => 'Aktuální počet tahů';

  @override
  String get progressStatsPeakMoves => 'Max. pozorovaný počet tahů';

  @override
  String get progressStatsPollSuccess => 'Úspěšné HTTP dotazy';

  @override
  String get progressStatsPollFail => 'Neúspěšné dotazy';

  @override
  String get progressStatsWsMessages => 'Zprávy WebSocket';

  @override
  String get progressShortOn => 'zap.';

  @override
  String get progressShortOff => 'vyp.';

  @override
  String get progressWaitingForData => 'čekání na data';

  @override
  String get progressTransportWifi => 'Wi‑Fi';

  @override
  String get progressTransportBluetooth => 'Bluetooth';

  @override
  String progressTransportDevDetail(
      String transport, String pollState, String wsState) {
    return '$transport · dotazy $pollState · WS $wsState';
  }

  @override
  String progressTransportUserDetail(String transport, String syncState) {
    return '$transport · živá synchronizace $syncState';
  }

  @override
  String get progressStatsEmptySubtitle =>
      'Na kartě Hra připoj desku — statistiky se doplní po startu dotazů.';

  @override
  String get helpAppBarTitle => 'czechmate — nápověda';

  @override
  String get helpConnectTitle => 'Připojení desky';

  @override
  String get helpConnectBody =>
      'Nejprve desku spáruj přes Bluetooth. Aplikace ji pak ovládá přes HTTP v lokální síti — typicky domácí Wi‑Fi, nebo hotspot desky (často 192.168.4.x) po zapnutí z Objevení desky při aktivním Bluetooth. Hotspot je ve výchozím stavu vypnutý. Ruční URL a režimy zůstávají v Nastavení.';

  @override
  String get helpPlayingTitle => 'Hraní partie';

  @override
  String get helpPlayingBody =>
      'Karta Hra pro živou synchronizaci, Ovládání hry pro čas a otočení, Analýza pro Stockfish.';

  @override
  String get helpCoachTitle => 'Trenér a analýza';

  @override
  String get helpCoachBody =>
      'Zapni výukový režim a v Nastavení vyplň přístupové klíče k cloudovým trenérům.';

  @override
  String get helpSettingsTitle => 'Nastavení a konfigurace';

  @override
  String get helpSettingsBody =>
      'Vzhled, NVS, lampa, firmware a MQTT jsou zde.';

  @override
  String get coachScreenTitle => 'AI trenér';

  @override
  String get coachScreenHintTitle => 'Zahájení a plány';

  @override
  String get coachScreenHintBody => 'Zeptej se na nápady k této pozici.';

  @override
  String coachScreenLevelLabel(String level) {
    return 'Úroveň $level';
  }

  @override
  String get setupModeAppBar => 'Režim nastavení';

  @override
  String get setupModePiecePlacementTitle => 'Rozestavení figurek';

  @override
  String get setupModePiecePlacementBody =>
      'Použij průvodce pro srovnání figur pomocí LED.';

  @override
  String get setupModeGoBack => 'Vrátit se zpět';

  @override
  String get wizardSkipStep => 'Přeskočit krok';

  @override
  String get wizardCancel => 'Zrušit';

  @override
  String get connDiagTitle => 'Diagnostika připojení';

  @override
  String get connDiagTransport => 'Transport';

  @override
  String get connDiagWifiBaseUrl => 'Základní Wi‑Fi URL';

  @override
  String get connDiagPolling => 'Periodické dotazy';

  @override
  String get connDiagWebSocket => 'WebSocket';

  @override
  String get connDiagPollSuccessTitle =>
      'Úspěšné GET snímku stavu (včetně 304)';

  @override
  String get connDiagPollFailureTitle =>
      'Chybné GET / chyby při periodickém čtení';

  @override
  String get connDiagWsFramesTitle => 'WS zprávy (rámce)';

  @override
  String get connDiagLastPollOk => 'Poslední úspěšné načtení';

  @override
  String get connDiagLastErrorTitle => 'Poslední chyba';

  @override
  String get connDiagActive => 'aktivní';

  @override
  String get connDiagOff => 'vypnuto';

  @override
  String get newGameSheetTitle => 'Nová hra';

  @override
  String get newGameSheetSubtitle => 'Zvol časovou kontrolu a stranu.';

  @override
  String get newGameWhiteBottom => 'Bílá dole';

  @override
  String get newGameBlackBottom => 'Černá dole';

  @override
  String get newGameCustomTimeTitle => 'Vlastní čas';

  @override
  String get newGameCustomTimeSubtitle =>
      'Minuty + přírůstek odeslané na desku.';

  @override
  String newGameMinutesPerSide(String n) {
    return 'Minuty na stranu: $n';
  }

  @override
  String newGameIncrementSeconds(String n) {
    return 'Přírůstek (s): $n';
  }

  @override
  String get newGameStartOnBoard => 'Spustit na desce';

  @override
  String get newGameConnectFirstSnack =>
      'Nejdřív připoj desku (Wi‑Fi nebo Bluetooth).';

  @override
  String get newGameStartedSnack => 'Nová hra na desce spuštěna.';

  @override
  String newGameErrorSnack(String error) {
    return 'Chyba: $error';
  }

  @override
  String get newGameSheetBody =>
      'Vyberte časovou kontrolu a orientaci šachovnice.';

  @override
  String get newGameBoardViewSection => 'Orientace šachovnice';

  @override
  String get newGameBoardViewInfoTitle => 'Orientace šachovnice';

  @override
  String get newGameWhoStartsNote => '';

  @override
  String get newGameFirmwarePresets => 'Předvolby firmware';

  @override
  String newGameCustomSummary(int min, int inc) {
    return '$min min + $inc s / tah';
  }

  @override
  String get learnAppBarTitle => 'Výuka šachu';

  @override
  String get learnBoardLedHint =>
      'Propoj šachovnici a využij LED nápovědy přímo na desce při procvičování těchto lekcí.';

  @override
  String learnSnackLesson(String title) {
    return 'Lekce: $title — interaktivní obsah připravujeme.';
  }

  @override
  String get learnSecBasics => 'Základy';

  @override
  String get learnSecSpecial => 'Zvláštní tahy';

  @override
  String get learnSecTactics => 'Taktiky';

  @override
  String get learnSecStrategy => 'Strategie';

  @override
  String get learnL1Title => 'Jak se pohybují figurky';

  @override
  String get learnL1Desc =>
      'Král, dáma, věž, střelec, jezdec, pěšák — nauč se pohyby každé figurky.';

  @override
  String get learnL2Title => 'Cíl šachové hry';

  @override
  String get learnL2Desc =>
      'Šachmat, pat, remíza — co znamenají stavy konce partie.';

  @override
  String get learnL3Title => 'Hodnota figurek';

  @override
  String get learnL3Desc =>
      'Pěšák=1, Jezdec/Střelec=3, Věž=5, Dáma=9 — proč na tom záleží.';

  @override
  String get learnL4Title => 'Rošáda';

  @override
  String get learnL4Desc => 'Velká a malá rošáda — jak, kdy a proč.';

  @override
  String get learnL5Title => 'Braní mimochodem';

  @override
  String get learnL5Desc =>
      'Zvláštní braní pěšákem — pravidlo, na které začátečníci zapomínají.';

  @override
  String get learnL6Title => 'Promoce';

  @override
  String get learnL6Desc => 'Pěšák na osmé řadě — jak zvolit správnou figuru.';

  @override
  String get learnL7Title => 'Vidlička';

  @override
  String get learnL7Desc =>
      'Napadnout dvě figurky najednou — základní taktický motiv.';

  @override
  String get learnL8Title => 'Svázání';

  @override
  String get learnL8Desc => 'Figurka nemůže táhnout, protože by odkryla krále.';

  @override
  String get learnL9Title => 'Scholar’s Mate';

  @override
  String get learnL9Desc => 'Šachmat ve čtyřech tazích — a jak mu čelit.';

  @override
  String get learnL10Title => 'Ovládnutí středu';

  @override
  String get learnL10Desc =>
      'Proč jsou pole e4, d4, e5, d5 klíčová v zahájení.';

  @override
  String get learnL11Title => 'Bezpečnost krále';

  @override
  String get learnL11Desc =>
      'Rošáda včas jako pojistka — kdy a jak krále schovat.';

  @override
  String get learnL12Title => 'Koncovka: král a pěšák';

  @override
  String get learnL12Desc => 'Jak přeměnit výhodu pěšáka na výhru v koncovce.';

  @override
  String get devSettingsTitle => 'Diagnostika a vývojář';

  @override
  String get devStockfishFenHeader => 'Stockfish a FEN';

  @override
  String get devMoveEvalTile =>
      'Hodnocení tahů (přepínač moveEvaluationEnabled)';

  @override
  String devHintDepthTile(String n) {
    return 'Hloubka nápovědy (hintDepth): $n';
  }

  @override
  String get devCurrentFenLabel => 'Aktuální FEN z desky:';

  @override
  String get devNetworkTransportHeader => 'Síť a transport';

  @override
  String get devBoardBaseUrlTile => 'Základní URL desky (ESP)';

  @override
  String get devActiveLinkTile => 'Stav spojení (Active Link)';

  @override
  String get devCoachTraceTile => 'Podrobné záznamy trenéra (trasování)';

  @override
  String get devStartConnection => 'Spustit připojení';

  @override
  String get devStopTransport => 'Zastavit';

  @override
  String get devTransportStoppedSnack => 'Transport zastaven.';

  @override
  String get devRefreshedFromPrefsSnack => 'Obnoveno z předvoleb.';

  @override
  String devPingSnapshot(String result) {
    return 'Ping snímku stavu (RTT): $result';
  }

  @override
  String get devConnectionDiagTile =>
      'Diagnostika připojení (rozhraní REST a WebSocket)';

  @override
  String get devDisconnectStaSnack => 'STA odpojeno.';

  @override
  String get devClearWifiNvsTitle => 'Smazat Wi‑Fi z NVS?';

  @override
  String get devClearWifiNvsBody => 'ESP ztratí uloženou síť.';

  @override
  String get devWifiNvsClearedSnack => 'Údaje Wi‑Fi v NVS vymazány.';

  @override
  String get devClearWifiNvsButton => 'Smazat uloženou Wi‑Fi z NVS';

  @override
  String get devWifiConfigHeader => 'Konfigurace Wi‑Fi na desce';

  @override
  String get devWifiUnavailable => 'Stav Wi‑Fi není k dispozici. Obnov níže.';

  @override
  String get devNvsToolsButton => 'Detailní nástroje (třídy NVS)';

  @override
  String devErrorPrefix(String error) {
    return 'Chyba: $error';
  }

  @override
  String get devSentToEspSnack => 'Odesláno na ESP.';

  @override
  String get boardNvsTitle => 'Paměť šachovnice (NVS)';

  @override
  String get boardNvsSavedSnack => 'Uloženo do NVS desky.';

  @override
  String get boardNvsLedHeader => 'Šachová nápověda na desce (LED)';

  @override
  String get boardNvsEvalMode => 'Režim hodnocení (počítat nejlepší tahy)';

  @override
  String get boardNvsStockfishDepth => 'Stockfish hloubka D1–D18';

  @override
  String get boardNvsLedBest => 'Zobrazit LED odměny (nejlepší tah)';

  @override
  String get boardNvsLedGood => 'Zobrazit LED odměny (dobrý tah)';

  @override
  String get boardNvsLedCapture => 'Zobrazit LED odměny (braní)';

  @override
  String get boardNvsUartStats => 'Zobrazit statistiku nápovědy v UART';

  @override
  String get boardNvsLiftBeforeBotTarget =>
      'Cílové světlo u počítače až po zvednutí figurky';

  @override
  String get boardNvsTutorialMode => 'Výukový režim zapnutý';

  @override
  String get boardNvsConfirmNewGameLed => 'Potvrzovat novou hru LED tlačítkem';

  @override
  String get boardNvsHintLimit => 'Limit nápověd (0 = neomezeno)';

  @override
  String get boardNvsHintTierTitle => 'Typ nápovědy (MoveHintTier)';

  @override
  String get boardNvsHintTierSubtitle =>
      'H1 = jen nejlepší tah, H2 = 3 nejlepší tahy, H3 = celá sada dle Stockfish';

  @override
  String get boardNvsH1 => 'H1 – Nejlepší';

  @override
  String get boardNvsH2 => 'H2 – 3 nejlepší';

  @override
  String get boardNvsH3 => 'H3 – Vše';

  @override
  String get boardNvsOpponentHeader => 'Nastavení protihráče (počítače)';

  @override
  String get boardNvsBotMode => 'Režim počítačového soupeře';

  @override
  String get boardNvsPvp => 'Hráč vs hráč (vypnuto)';

  @override
  String get boardNvsPvb => 'Hráč proti počítači';

  @override
  String get boardNvsBotStrength => 'Síla počítače (úroveň)';

  @override
  String get boardNvsBotPlaysAs => 'Počítač hraje za';

  @override
  String get boardNvsDemoHeader => 'Demo režim';

  @override
  String get boardNvsSaveToBoard => 'Uložit do NVS desky';

  @override
  String boardDemoStateLine(String on) {
    return 'Stav na desce: demo $on';
  }

  @override
  String get boardDemoOn => 'zapnuto';

  @override
  String get boardDemoOff => 'vypnuto';

  @override
  String get boardDemoEnabledConfig => 'Demo zapnuto (konfigurace)';

  @override
  String boardDemoSpeedLabel(String ms) {
    return 'Rychlost animace: $ms ms';
  }

  @override
  String get boardDemoSendConfig => 'Odeslat konfiguraci demo';

  @override
  String get boardDemoStart => 'Spustit demo';

  @override
  String get boardLedSentSnack => 'LED nápověda odeslána.';

  @override
  String boardLedGuidanceTitle(String level) {
    return 'LED nápověda (úroveň $level / 5)';
  }

  @override
  String get boardLedSendLevel => 'Odeslat LED úroveň';

  @override
  String get boardGuidedCaptureTitle => 'Navigovaná braní';

  @override
  String get boardGuidedCaptureSubtitle => 'LED zvýrazní možná braní.';

  @override
  String get lampBoardLightTitle => 'Světlo desky';

  @override
  String get lampDetailTitle => 'Lampa — detail';

  @override
  String get lampDetailsButton => 'Podrobnosti lampy';

  @override
  String get lampHeader => 'Lampa';

  @override
  String get lampNeedsConnection =>
      'Vyžaduje připojení k desce (Wi‑Fi nebo Bluetooth).';

  @override
  String get lampOnTitle => 'Lampa zapnutá';

  @override
  String lampBrightnessLabel(String n) {
    return 'Jas $n%';
  }

  @override
  String get lampSendBrightness => 'Odeslat jas';

  @override
  String get lampBrightnessSentSnack => 'Jas odeslán na desku.';

  @override
  String get lampColorStateSentSnack => 'Barva a stav lampy odeslány.';

  @override
  String get lampGenericCommandSnack => 'Příkaz lampy odeslán.';

  @override
  String lampBlockStatusSummary(
      String mode, String r, String g, String b, String h, String t) {
    return 'Režim: $mode · RGB($r,$g,$b) · limit nápověd: $h · auto zhasnutí: $t s';
  }

  @override
  String get lampRgbHint => 'Barva (R G B) — POST /api/light/command';

  @override
  String get lampSendColor => 'Odeslat barvu / stav lampy';

  @override
  String get lampGameModeButton => 'Režim hry (LED podle partie)';

  @override
  String get lampColorLabel => 'Barva';

  @override
  String get lampColorfulnessLabel => 'Sytost barvy';

  @override
  String get lampLedBrightnessPct => 'Jas LED (%)';

  @override
  String get lampStudioColorTitle => 'Barva';

  @override
  String get lampStudioColorHint =>
      'Klepnutí nebo táhnutí — odstín a sytost (střed = bílá).';

  @override
  String get lampStudioValueTitle => 'Světlost barvy';

  @override
  String get lampStudioBoardPreview => 'Náhled desky';

  @override
  String get lampStudioPreviewGlowHint => 'Náhled rozptylu světla';

  @override
  String get lampStudioPreviewLampOff => 'Lampa vypnutá';

  @override
  String get lampStudioScenes => 'Scény';

  @override
  String get lampStudioPresetColorsTitle => 'Předvolené barvy';

  @override
  String get lampStudioFineTuneSection => 'Doladit barvu';

  @override
  String get lampStudioApplyToBoard => 'Použít na desku';

  @override
  String get lampStudioGameMode => 'Režim partie';

  @override
  String get lampStudioAppliedOk => 'Barva a jas odeslány na desku.';

  @override
  String get lampStudioGameModeOk => 'Režim lampy podle partie nastaven.';

  @override
  String get lampStudioNeedConnection =>
      'Pro ovládání lampy připoj desku přes Wi‑Fi nebo Bluetooth.';

  @override
  String get lampStudioAutoOff => 'Automatické zhasnutí';

  @override
  String get lampStudioSaveTimer => 'Uložit časovač';

  @override
  String get lampStudioTimerSavedOk => 'Časovač auto zhasnutí uložen.';

  @override
  String get lampStudioSelectedSwatchLabel => 'Zvolená barva';

  @override
  String get lampStudioDeveloperRgbTitle => 'Vývojář · hodnoty RGB';

  @override
  String lampStudioRgbLine(int r, int g, int b) {
    return 'RGB ($r, $g, $b)';
  }

  @override
  String get lampPresetWarmWhite => 'Teplá bílá';

  @override
  String get lampPresetCoolWhite => 'Studená bílá';

  @override
  String get lampPresetCalmBlue => 'Klidná modrá';

  @override
  String get lampPresetForestGreen => 'Lesní zelená';

  @override
  String get lampPresetWarmth => 'Teplo';

  @override
  String get lampPresetPurpleScene => 'Fialová scéna';

  @override
  String get lampPresetRed => 'Červená';

  @override
  String get lampPresetAmber => 'Jantar';

  @override
  String get puzzleTabDaily => 'Denní';

  @override
  String get puzzleTabLibrary => 'Knihovna';

  @override
  String get puzzleTabTraining => 'Trénink';

  @override
  String get puzzleBackLiveTooltip => 'Zpět na živou partii';

  @override
  String get puzzlePoolEmptySnack =>
      'Žádné pozice v tomto režimu fondu — přepni na Mix nebo přidej knihovnu.';

  @override
  String get puzzleLibraryEmptyHint =>
      'Zatím žádné položky — použij tlačítko výše nebo „Přidat“ z denního puzzlu.';

  @override
  String get puzzleLibraryRemoveTitle => 'Odstranit z knihovny?';

  @override
  String puzzleLibraryRemoveBody(String title) {
    return 'Odstranit „$title“ z knihovny puzzlů? Tento krok nelze vrátit.';
  }

  @override
  String get puzzleLibraryRemoveConfirm => 'Odstranit';

  @override
  String get puzzleLoadDaily => 'Načíst denní puzzle';

  @override
  String get puzzleRefreshDaily => 'Obnovit denní úlohu';

  @override
  String get puzzleAlreadyInLibrary => 'Už je v knihovně';

  @override
  String get puzzleSavedToLibraryBanner => 'Uloženo do knihovny.';

  @override
  String get puzzleLibNameLabel => 'Název';

  @override
  String get puzzleLibFenLabel => 'FEN';

  @override
  String get puzzleGoPlaySandbox => 'Volná pozice — záložka Hra.';

  @override
  String puzzleGoPlayPuzzle(String title) {
    return 'Puzzle ($title) — záložka Hra.';
  }

  @override
  String get puzzleSideBlackMove => 'Černý na tahu';

  @override
  String get puzzleSideWhiteMove => 'Bílý na tahu';

  @override
  String get puzzleSideUnknown => 'Na tahu: ?';

  @override
  String puzzleTrainingPoolStats(int poolN, int sessions) {
    return '$poolN pozic · Zahájených kol: $sessions';
  }

  @override
  String get puzzleThemeMateIn1 => 'Mat 1 tahem';

  @override
  String get puzzleThemeAdvancedPawn => 'Volný pěšec';

  @override
  String get puzzleThemeOpening => 'Zahájení';

  @override
  String get puzzleThemeKingsideAttack => 'Útok na krále';

  @override
  String get puzzleThemeMiddlegame => 'Střední hra';

  @override
  String get puzzleThemeTactic => 'Taktika';

  @override
  String get setupWizardErrMissingFen => 'Chybí FEN.';

  @override
  String get setupWizardErrConnectBoard =>
      'Připoj desku přes Wi‑Fi nebo Bluetooth (ukázkový režim tento postup nepodporuje).';

  @override
  String get setupWizardErrNoSteps => 'Z FEN nešlo sestavit žádné kroky.';

  @override
  String get setupWizardErrGeneric => 'Chyba';

  @override
  String get setupWizardErrPhysicalMismatch =>
      'Deska stále hlásí režim nastavení pozice, nebo fyzické figury nesedí — uprav figury na zvýrazněných polích a zkus to znovu.';

  @override
  String get setupWizardTitleStandard => 'Základní postavení';

  @override
  String get setupWizardTitleFromFen => 'Postavení z FEN';

  @override
  String get setupWizardDoneStandard => 'Deska potvrdila základní postavení.';

  @override
  String get setupWizardDoneFenLoaded => 'Pozice byla nahrána na desku.';

  @override
  String get setupWizardDoneNoLoad => 'Průvodce dokončen.';

  @override
  String get setupWizardBtnDone => 'Hotovo';

  @override
  String setupWizardStepProgress(int current, int total) {
    return 'Krok $current / $total';
  }

  @override
  String setupWizardPlacePieceOn(String square) {
    return 'Polož figurku na $square';
  }

  @override
  String setupWizardPieceDetail(String pieceName, String square) {
    return '$pieceName → $square';
  }

  @override
  String get setupWizardLedHint =>
      'Světlo na desce ukazuje cílové pole. Postup se potvrdí senzorem matice nebo správnou figurkou na snímku stavu.';

  @override
  String get setupWizardSkipStep => 'Přeskočit krok';

  @override
  String get boardSetupWhiteKing => 'bílý král';

  @override
  String get boardSetupBlackKing => 'černý král';

  @override
  String get boardSetupWhiteQueen => 'bílá dáma';

  @override
  String get boardSetupBlackQueen => 'černá dáma';

  @override
  String get boardSetupWhiteRook => 'bílá věž';

  @override
  String get boardSetupBlackRook => 'černá věž';

  @override
  String get boardSetupWhiteBishop => 'bílý střelec';

  @override
  String get boardSetupBlackBishop => 'černý střelec';

  @override
  String get boardSetupWhiteKnight => 'bílý jezdec';

  @override
  String get boardSetupBlackKnight => 'černý jezdec';

  @override
  String get boardSetupWhitePawn => 'bílý pěšec';

  @override
  String get boardSetupBlackPawn => 'černý pěšec';

  @override
  String get boardSetupPieceGeneric => 'figurka';

  @override
  String get gameRemoteEmptySquareSnack => 'Na tomto poli není figurka.';

  @override
  String get gameRemoteEmptySquareHud => 'Prázdné pole';

  @override
  String get gameRemoteWrongTurnSnack =>
      'Vyber figurku barvy, která je zrovna na tahu.';

  @override
  String get gameRemoteWrongTurnHud => 'Nejsi na tahu';

  @override
  String get gameRemoteGameFinished => 'Tato partie už skončila.';

  @override
  String get gameRemotePositionError =>
      'Z aktuálních dat desky nejde tah ověřit.';

  @override
  String get gamePromotionPickTitle => 'Promoce — vyber figuru';

  @override
  String get gamePromotionQueen => 'Dáma';

  @override
  String get gamePromotionRook => 'Věž';

  @override
  String get gamePromotionBishop => 'Střelec';

  @override
  String get gamePromotionKnight => 'Jezdec';

  @override
  String get gamePromotionFailedSnack => 'Promoce se nepodařila';

  @override
  String get semanticsChessBoard =>
      'Šachovnice, osm krát osm polí. Klepnutím vybírej pole.';

  @override
  String get moveHistoryEmpty => 'Historie tahů zatím prázdná';

  @override
  String get moveHistoryCurrentPosition => 'Živá pozice — aktuální partie';

  @override
  String moveHistoryPieceSubtitle(String piece) {
    return 'Figura: $piece';
  }

  @override
  String get bundledPuzzleMateQg8Title => 'Mat dámou';

  @override
  String get bundledPuzzleMateQg8Subtitle => 'Bílý · mat v 1 tahu';

  @override
  String get bundledPuzzleBackRankTitle => 'Zadní řada';

  @override
  String get bundledPuzzleBackRankSubtitle => 'Bílý · mat věží';

  @override
  String get bundledPuzzleRookCornerTitle => 'Věž proti králi';

  @override
  String get bundledPuzzleRookCornerSubtitle => 'Bílý · koncovka';

  @override
  String get bundledPuzzleBlackMateTitle => 'Černý na tahu';

  @override
  String get bundledPuzzleBlackMateSubtitle => 'Černý · mat v 1 tahu';

  @override
  String get bundledPuzzlePromotionTitle => 'Postup pěšce';

  @override
  String get bundledPuzzlePromotionSubtitle => 'Bílý · proměna';

  @override
  String get bundledPuzzleTwoRooksTitle => 'Dvě věže';

  @override
  String get bundledPuzzleTwoRooksSubtitle => 'Bílý · dvě věže';

  @override
  String get settingsTileSmartHomeTitle => 'Smart home (MQTT)';

  @override
  String get settingsTileSmartHomeSubtitle => 'Home Assistant a MQTT';

  @override
  String get settingsHaMqttOpenButton => 'MQTT a Home Assistant';

  @override
  String get settingsTileBoardLightTitle => 'Světlo desky';

  @override
  String get settingsTileBoardLightSubtitle =>
      'Barva, jas, scény, auto‑vypnutí';

  @override
  String get settingsTileModulesTitle => 'Nápověda a průvodci';

  @override
  String get settingsTileModulesSubtitle => 'Dokumentace a znovu spustit úvod';

  @override
  String get settingsNavAppTour => 'Průvodce aplikací (úvod)';

  @override
  String get onboardingYourNameTitle => 'Jak ti máme říkat?';

  @override
  String get onboardingYourNameSubtitle =>
      'Toto jméno se ukáže v profilu a ve sdílených přehledech.';

  @override
  String get onboardingNameHint => 'Tvoje jméno';

  @override
  String get onboardingPermissionsTitle => 'Povolit přístup';

  @override
  String get onboardingPermissionsSubtitle =>
      'Všechno jde povolit postupně — aplikace funguje nejlépe, když přístup povolíš.';

  @override
  String get onboardingPermPhotosTitle => 'Fotky';

  @override
  String get onboardingPermPhotosBody => 'Profilová fotka z galerie.';

  @override
  String get onboardingPermPhotosAllow => 'Povolit fotky';

  @override
  String get onboardingPermBleTitle => 'Bluetooth';

  @override
  String get onboardingPermBleBody => 'Najít desku CzechMate a připojit se.';

  @override
  String get onboardingPermBleAllow => 'Povolit Bluetooth';

  @override
  String get onboardingPermWifiTitle => 'Wi‑Fi';

  @override
  String get onboardingPermWifiBodyAndroid =>
      'Aplikace si může přečíst název aktuální sítě pro doplnění Wi‑Fi nastavení (na Androidu 13+ bez přístupu k poloze).';

  @override
  String get onboardingPermWifiBodyIos =>
      'K síti desky se připojíš v Nastavení při párování — tady nic dalšího nepotřebuješ.';

  @override
  String get onboardingPermWifiBodyDesktop =>
      'Na počítači často nejde spolehlivě přečíst název aktuální Wi‑Fi — SSID zadej ručně při nastavení desky. HTTP na desku pořád používá uloženou adresu.';

  @override
  String get onboardingPermWifiAllow => 'Povolit přístup k Wi‑Fi';

  @override
  String get onboardingPermGrantedShort => 'Povoleno';

  @override
  String get onboardingPermDeniedSnack =>
      'To později změníš v nastavení systému.';

  @override
  String get onboardingOpenSettings => 'Otevřít nastavení';

  @override
  String get onboardingPermissionsDesktopBody =>
      'Když systém nabídne Bluetooth, povol ho — pak půjde desku vyhledat. HTTP používej přes Wi‑Fi / ruční URL.';

  @override
  String get onboardingPermissionsDesktopWindowsBody =>
      'Tato Windows verze nepoužívá Bluetooth Low Energy k desce. Připoj se jen přes Wi‑Fi: zadej HTTP adresu desky v pokročilém / ručním připojení (např. http://192.168.4.1 na hotspotu desky nebo IP v domácí síti).';

  @override
  String get settingsNavChessPuzzles => 'Šachové puzzle';

  @override
  String get settingsNavProfileElo => 'Profil a puzzle Elo';

  @override
  String get settingsNavProgress => 'Pokrok (učení a statistiky)';

  @override
  String get settingsNavHelp => 'Nápověda';

  @override
  String get settingsTileNvsDiagTitle => 'Paměť desky a diagnostika';

  @override
  String get settingsTileNvsDiagSubtitle => 'NVS pravidla, vývojářské nástroje';

  @override
  String get settingsNavBoardNvs => 'Pravidla NVS desky (LED, bot)';

  @override
  String get settingsNavDeveloperDiag => 'Vývojářská diagnostika';

  @override
  String get settingsDiagnosticsBoardNvsLockedHint =>
      'Pro surové NVS desky zapni vývojářský režim v O aplikaci (7× klepnutí na řádek verze nebo titulek Nastavení).';

  @override
  String get settingsTileAboutTitle => 'O aplikaci';

  @override
  String get settingsTileAboutSubtitle => 'Verze, soukromí, systém';

  @override
  String get settingsAboutVersionLoading => 'Načítám verzi…';

  @override
  String settingsAboutVersionLine(String version, String build) {
    return 'Verze $version ($build) • Klepni 7× na tento řádek nebo na „Nastavení“ v hlavní liště nastavení (do cca sekundy mezi klepnutími) pro odemčení vývojářského režimu.';
  }

  @override
  String get settingsAppUpdateBannerTitle => 'Je dostupná nová verze aplikace';

  @override
  String settingsAppUpdateBannerSubtitle(String current, String latest) {
    return 'Máš $current; nejnovější je $latest.';
  }

  @override
  String settingsAppUpdateBannerSubtitleRequired(
      String current, String requiredVersion) {
    return 'Verze $current je pod minimální podporovanou. Aktualizuj prosím na alespoň $requiredVersion.';
  }

  @override
  String get settingsAppUpdateOpenDownloads => 'Otevřít stránku ke stažení';

  @override
  String get settingsAboutAppUpdateLineTitle => 'Nová verze aplikace';

  @override
  String settingsAboutAppUpdateLineBody(String current, String latest) {
    return 'Nainstalováno: $current. Nejnovější: $latest.';
  }

  @override
  String get settingsAboutDeveloperModeTitle => 'Vývojářský režim';

  @override
  String get settingsAboutDeveloperModeSubtitleOn =>
      'Zobrazí rozšířenou diagnostiku a volitelné OTA stejné verze. Vypnutím je zase schováš.';

  @override
  String get settingsPrivacyTitle => 'Soukromí';

  @override
  String get settingsPrivacyBody =>
      'Tento build neodesílá analytiku. Provoz jde jen na tvou desku v lokální síti nebo přes Bluetooth.';

  @override
  String get settingsIcloudTitle => 'iCloud';

  @override
  String get settingsIcloudBody =>
      'Synchronizace puzzlů přes iCloud (CloudKit) je plánovaná; tento build nesynchronizuje.';

  @override
  String get settingsLiveActivityTitle => 'Stav časomíry mimo aplikaci';

  @override
  String get settingsLiveActivitySubtitle =>
      'Telefony a tablety — iPhone: Live Activity (Lock Screen / Dynamic Island, iOS 16.2+). Android: probíhající notifikace (Android 13+ je potřeba povolit oznámení). Zapíná se z herní obrazovky. (Na počítači se nepoužívá.)';

  @override
  String get settingsLiveActivityIosDisabledSnack =>
      'Živé aktivity jsou v systému vypnuté. Zapni je v Nastavení → czechmate → Živé aktivity.';

  @override
  String get settingsHomeScreenWidgetHint =>
      'Kde to systém podporuje (domovská obrazovka nebo widgety na ploše), přidej CzechMate z galerie widgetů — rychlý náhled stavu na pozadí aplikace (vedle živých aktivit na podporovaných iPhonech).';

  @override
  String get settingsWearMirrorTitle => 'Zrcadlit časomíru na Wear OS';

  @override
  String get settingsWearMirrorSubtitle =>
      'Google Data Layer — stejný stav jako probíhající notifikace; vyžaduje párování hodinek Wear OS.';

  @override
  String get settingsWatchMirrorTitle => 'Zrcadlit časomíru na Apple Watch';

  @override
  String get settingsWatchMirrorSubtitle =>
      'WatchConnectivity — stejný stav jako živá aktivita; na hodinkách je základní rozhraní s Pauza/Pokračovat.';

  @override
  String get settingsFactoryTileTitle => 'Tovární reset desky';

  @override
  String get settingsFactoryTileSubtitle =>
      'Smaže NVS na ESP — Wi‑Fi údaje a předvolby se resetují';

  @override
  String get settingsFactoryDialogTitle => 'Tovární reset';

  @override
  String get settingsFactoryDialogBody =>
      'Smazat celé NVS desky (Wi‑Fi, hesla, MQTT, předvolby)? Zařízení se restartuje; hotspot desky zůstane vypnutý, dokud ho nezapneš z aplikace.';

  @override
  String get settingsFactoryErase => 'Smazat';

  @override
  String get settingsFactoryCommandSent => 'Příkaz odeslán.';

  @override
  String settingsFactoryError(String error) {
    return 'Chyba: $error';
  }

  @override
  String get settingsFactoryRunButton => 'Spustit tovární reset desky';

  @override
  String get settingsCoachOllamaUrlHelper =>
      'Zadej URL končící na /v1 (např. :11434/v1). Jen :11434 bez /v1 dřív nefungovalo — u portu 11434 to teď aplikace doplní.';

  @override
  String get settingsCoachOpenAiBaseLabel => 'OpenAI API základna';

  @override
  String get settingsCoachModelNameLabel => 'Název modelu';

  @override
  String get settingsCoachEnterGeminiModelSnack =>
      'Zadej ID modelu Google AI (nebo zvol předvolbu).';

  @override
  String get settingsCoachSavedSnack => 'Nastavení trenéra uloženo.';

  @override
  String get settingsCoachSaveButton => 'Uložit nastavení trenéra';

  @override
  String get settingsCoachGoogleKeyButton => 'Získat Google AI klíč';

  @override
  String get settingsCoachGroqConsoleButton => 'Konzole Groq';

  @override
  String get settingsCoachXaiConsoleButton => 'Konzole xAI';

  @override
  String get boardNvsRefreshTooltip => 'Obnovit z desky';

  @override
  String get boardNvsFetchFailedFallback =>
      'Data z desky se nepodařilo načíst. Klepnutím na ↻ to zkus znovu.';

  @override
  String boardNvsMergeHint(String depth, String eval) {
    return 'Při uložení na desku se z aplikace sloučí hloubka Stockfish ($depth) a přepínač hodnocení tahů ($eval) do dat NVS (stejně jako na iOS).';
  }

  @override
  String get boardNvsEvalOn => 'zap.';

  @override
  String get boardNvsEvalOff => 'vyp.';

  @override
  String get boardNvsHintTierFootnote =>
      'H1–H3 v CzechMate (`czechmate.moveHintTier`) jsou samostatné od úrovně světel na desce; při POST výše se do NVS propíše i hloubka a hodnocení z diagnostiky aplikace.';

  @override
  String get boardNvsFooterMock =>
      'Ukázková deska: NVS zápis se neprovede na reálný hardware.';

  @override
  String get boardNvsFooterWifiActive =>
      'Wi‑Fi spojení je aktivní — tlačítkem „Uložit“ odešleš nastavení počítačového soupeře do NVS na ESP.';

  @override
  String get boardNvsFooterBle =>
      'BLE: podle firmware lze posílat NVS/UI blob; pro jistotu ověř stav na webu desky.';

  @override
  String get boardNvsFooterGeneric =>
      'Pro spolehlivý zápis nastavení soupeře do NVS připoj desku (doporučeno přes Wi‑Fi).';

  @override
  String get boardNvsHttpBleExplain =>
      'Bluetooth je připojený, ale načtení NVS z této obrazovky probíhá přes HTTP. V Nastavení ulož platnou adresu desky (IP klienta STA, např. http://192.168.x.x), nebo aktivuj Wi‑Fi spojení k této IP.';

  @override
  String get boardNvsHttpWifiNoUrl =>
      'Wi‑Fi spojení bez platné základní adresy. Obnov připojení na záložce Hra → správa šachovnice.';

  @override
  String get boardNvsHttpMissingUrl =>
      'Chybí platná HTTP adresa desky. V Nastavení vyplň „Výchozí URL desky“ (kompletní http://…) a zkus znovu (ikonka ↻).';

  @override
  String get boardHttpMissingUrlBody =>
      'Adresa HTTP desky chybí nebo je neplatná. V Nastavení ulož celou URL (např. http://192.168.4.1 nebo STA IP desky). Bluetooth může fungovat i bez této URL pro živou hru.';

  @override
  String get boardHttpFailBle =>
      'Bluetooth je připojený, ale HTTP požadavek selhal (špatná adresa, vypršel čas nebo je aktivní zámek webu na desce).';

  @override
  String get boardHttpFailWifi =>
      'Wi‑Fi spojení je aktivní — ověř adresu desky a že deska odpovídá.';

  @override
  String get boardHttpFailMock => 'Ukázková deska — HTTP na hardware neplatí.';

  @override
  String get boardHttpFailNone => 'Žádné aktivní připojení k desce.';

  @override
  String boardHttpFailDetail(String link, String detail) {
    return '$link Podrobnost: $detail';
  }

  @override
  String boardHttpErrGeneric(String error) {
    return 'Chyba komunikace s deskou: $error';
  }

  @override
  String get progressLevelBeginner => 'Začátečník';

  @override
  String get progressLevelIntermediate => 'Středně pokročilý';

  @override
  String get progressLevelAdvanced => 'Pokročilý';

  @override
  String get progressLevelExpert => 'Expert';

  @override
  String progressLevelNumber(int n) {
    return 'Úroveň $n';
  }

  @override
  String get progressNoSessionYet => 'Zatím žádné aktivní připojení k desce';

  @override
  String get analysisIntroEvalHint =>
      'Hodnocení tahů a graf — chess-api nebo vlastní adresa v Nastavení (vývojář).';

  @override
  String get analysisHalfMoveOrderHint =>
      'Každý řádek je jeden tah v pořadí — nejdřív bílý, pak černý.';

  @override
  String get analysisNoBoardPositionHint =>
      'Na záložce Hra připoj šachovnici. Níže můžeš analyzovat vlastní FEN.';

  @override
  String get analysisChartDisabledBoardSubtitle =>
      'Zapni přepínač níže nebo v Nastavení → Vzhled šachovnice.';

  @override
  String get analysisOverviewSubtitleEval =>
      'Hodnocení pozice po každém tahu (z pohledu bílého: + znamená výhodu bílého)';

  @override
  String get analysisChartFillHintLiveEvalOn =>
      'Graf se naplní po tazích, až Stockfish vyhodnotí pozici (hraj na kartě Hra s internetem).';

  @override
  String get analysisChartFillHintLiveEvalOff =>
      'Po zapnutí automatického vyhodnocení uvidíš křivku zde.';

  @override
  String get analysisClearEvalData => 'Vymazat graf a hodnocení';

  @override
  String get analysisClearEvalConfirmTitle => 'Vymazat data analýzy?';

  @override
  String get analysisClearEvalConfirmBody =>
      'Odstraní graf hodnocení a uložené statistiky kvality tahů. Akci nelze vrátit zpět.';

  @override
  String get analysisClearEvalConfirmAction => 'Vymazat';

  @override
  String get analysisMoveQualitySideLast3 => 'Poslední 3 tahy strany';

  @override
  String get analysisMoveQualitySideLast10 => 'Posledních 10 tahů strany';

  @override
  String get analysisMoveQualityFullGame => 'Celá partie';

  @override
  String get analysisSecondLineComputing => 'Počítám…';

  @override
  String get analysisSecondLineLoadButton =>
      'Načíst druhou variantu (z FEN aktuální partie)';

  @override
  String analysisSecondLineSameMove(String from, String to) {
    return 'Obě hloubiny dávají stejný tah $from→$to.';
  }

  @override
  String analysisSecondLineDualMoves(
      String f1, String t1, String f2, String t2, String suffix) {
    return '1) $f1→$t1 · 2) $f2→$t2$suffix';
  }

  @override
  String analysisSecondLineEvalApprox(String pawns) {
    return ' · hodnocení ≈ $pawns pěš.';
  }

  @override
  String get analysisFreeAnalyzing => 'Počítám…';

  @override
  String analysisBestMoveLine(
      String from, String to, String evalSuffix, String extra) {
    return 'Nejlepší tah: $from–$to$evalSuffix$extra';
  }

  @override
  String analysisAvgLossCp(String cp) {
    return ' · průměrná ztráta $cp cp';
  }

  @override
  String get analysisFenFieldLabel => 'FEN';

  @override
  String get gameSandboxLoadPositionFirst =>
      'Nejdřív načti pozici (připoj desku nebo použij demo).';

  @override
  String get gameSandboxSelectPiece => 'Vyber figurku';

  @override
  String get gameSandboxWrongSide => 'Není tah této barvy';

  @override
  String get gameSandboxIllegalMove => 'Neplatný tah';

  @override
  String get gameSandboxIllegalMoveSandbox => 'Neplatný tah (volná pozice)';

  @override
  String get gameSandboxCouldNotLoadFen => 'Nepodařilo se načíst FEN.';

  @override
  String get gameSandboxPositionRestored => 'Pozice obnovena — zkus znovu.';

  @override
  String puzzleSuccessLineWithElo(int delta) {
    return 'Správně! Linie · +$delta Elo';
  }

  @override
  String get puzzleSuccessLine => 'Správně! Linie.';

  @override
  String get puzzleCelebrationTitle => 'Výborně!';

  @override
  String get puzzleCelebrationBodyPlain => 'Puzzle jsi vyřešil.';

  @override
  String puzzleCelebrationBodyElo(int delta) {
    return 'Puzzle jsi vyřešil — rating +$delta.';
  }

  @override
  String get puzzleSaveNeedPosition =>
      'Nejdřív otevři puzzle na kartě Hra, ať se načte pozice, pak ji ulož sem.';

  @override
  String get puzzleLibSavedPositionCaption =>
      'Aktuální pozice (technický údaj)';

  @override
  String get puzzleWrongResetting => 'To není řešení — obnovuji pozici.';

  @override
  String get gameControlWhiteBottom => 'Bílá dole';

  @override
  String get gameControlBlackBottom => 'Černá dole';

  @override
  String get gameControlCoordsOn => 'Souřadnice zap.';

  @override
  String get gameControlCoordsOff => 'Souřadnice vyp.';

  @override
  String get gameControlBoardSizeHint =>
      'Velikost desky: Nastavení → Vzhled desky.';

  @override
  String get gameControlMoveHint => 'Nápověda tahu';

  @override
  String get gameControlPanelSubtitle => 'Zobrazení · Režim · Akce';

  @override
  String get gameControlLearningCloudHint =>
      'Cloud trenér v aplikaci; Stockfish na desce řídí nápovědní LED dle NVS.';

  @override
  String get gameRemoteMovesWifi =>
      'Dvojitým klepnutím na pole odešleš tah na desku přes Wi‑Fi.';

  @override
  String get gameRemoteMovesBle =>
      'Dvojitým klepnutím na pole odešleš tah přes Bluetooth.';

  @override
  String get gameRemoteMovesMock =>
      'Demo: dvojité klepnutí odešle tah lokálně (simulace desky + hodin).';

  @override
  String get gameRemoteMovesNoBoard =>
      'Zapnuto, ale deska není připojená. Připoj přes Bluetooth.';

  @override
  String get gameRemoteMovesConnect =>
      'Zapnuto — připoj desku přes Wi‑Fi nebo Bluetooth pro odesílání tahů.';

  @override
  String get gameBoardRefreshedSnack => 'Stav desky obnoven';

  @override
  String get gameDemoSnapshotReloadedSnack =>
      'Snímek ukázkové desky znovu načten';

  @override
  String get timerCenterGameOver => 'Konec partie';

  @override
  String get timerCenterRunning => 'Běží';

  @override
  String get timerCenterStopped => 'Stojí';

  @override
  String get coachSetupBannerBody =>
      'AI trenér ještě není nastavený. V nastavení přidej poskytovatele a API klíč, aby mohl odpovídat.';

  @override
  String get coachSetupBannerAction => 'Otevřít nastavení trenéra';

  @override
  String coachErrorSomethingWrong(String error) {
    return 'Něco se pokazilo: $error';
  }

  @override
  String get coachOfflineLabel => 'Nepřipojeno';

  @override
  String get coachDisconnectedBanner =>
      'Deska není připojená — AI nedostává aktuální pozici z partie.';

  @override
  String get coachEmptyPromptEmbedded =>
      'Zeptej se na plán, taktiku nebo konkrétní pole.';

  @override
  String get coachEmptyPromptFullscreen =>
      'Zeptej se na aktuální pozici nebo obecně na šachy.';

  @override
  String get coachInputHintEmbedded => 'Zeptej se trenéra…';

  @override
  String get coachInputHintFullscreen => 'Zeptej se na pozici nebo plán…';

  @override
  String get coachChainFailedBanner =>
      'Nepodařilo se kontaktovat žádného nastaveného AI poskytovatele (viz Nastavení → Trenér a AI).';

  @override
  String get settingsLinkDisconnected => 'Odpojeno';

  @override
  String get settingsLinkConnecting => 'Připojuji…';

  @override
  String get settingsLinkNoResponseYet => 'Deska zatím neodpovídá';

  @override
  String get settingsLinkConnectedLive => 'Připojeno (živě)';

  @override
  String get settingsCoachSubtitleOfflineTips =>
      'Jen krátké tipy v zařízení (bez cloudu)';
}
