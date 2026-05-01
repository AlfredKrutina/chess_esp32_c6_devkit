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
  String get navPuzzle => 'Puzzle';

  @override
  String get navProgress => 'Pokrok';

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
      'Zvol jazyk aplikace. Systém následuje telefon; ostatní jazyky se zobrazí v angličtině.';

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
      'Zapni výukový režim a Stockfish eval na Analýze pro hlubší poznatky.';

  @override
  String get netNoInternetTitle => 'Bez internetu';

  @override
  String get netNoInternetBody =>
      'Stockfish (chess-api), Lichess a další cloudové funkce vyžadují internet. Po Bluetooth ale desku používáš i offline.';

  @override
  String get netNotOnWifiTitle => 'Nejsi na Wi‑Fi';

  @override
  String get netNotOnWifiBody =>
      'chess-api většinou funguje přes mobilní data. K HTTP API desky na LAN IP telefon typicky potřebuje Wi‑Fi ve stejné síti. Pokud jsi jen na hotspotu desky bez internetu, DNS pro chess-api / Lichess může selhat — zkus Wi‑Fi na chvíli vypnout, zapnout mobilní data nebo dual‑stack, pokud systém dovolí. Aplikace nemůže donutit provoz na LTE, když výchozí trasa je Wi‑Fi.';

  @override
  String get errInvalidBoardUrl =>
      'Neplatná adresa desky — chybí hostitel. Použij http://192.168.4.1 nebo STA IP desky.';

  @override
  String get errNoSavedBle =>
      'Žádná uložená Bluetooth deska. Otevři Objevování desky a spáruj CZECHMATE.';

  @override
  String get errDemoNoSnapshot => 'Demo deska nemá snapshot.';

  @override
  String get errConnectBeforeMoves =>
      'Než odešleš tah z aplikace, připoj desku přes Wi‑Fi nebo Bluetooth.';

  @override
  String get errSetupNeedsConnection =>
      'Průvodce nastavením vyžaduje aktivní připojení k desce (Wi‑Fi nebo Bluetooth).';

  @override
  String get errOtaHttps => 'OTA vyžaduje HTTPS URL souboru .bin.';

  @override
  String get errOtaMock => 'OTA není dostupná u demo desky.';

  @override
  String get errOtaConnectFirst =>
      'Připoj desku přes Wi‑Fi nebo Bluetooth a pak zkus aktualizaci znovu.';

  @override
  String get errHintsNeedConnection =>
      'Nápovědní LED vyžadují připojení desky. Nejprve Bluetooth nebo Wi‑Fi.';

  @override
  String get errNoActiveSession =>
      'Žádná aktivní session desky. Připoj ji přes Wi‑Fi nebo Bluetooth z Objevování desky.';

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
  String get moveEvalExcellentEngine => 'Výborně — přesně jako engine.';

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
    return 'Evaluace selhala: $error';
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
      'Žádná Stockfish data — zapni hodnocení tahů a odehraj partii.';

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
  String get reportShareExportHint =>
      'Exportuj kartu výše jako PNG a pak použij systémové sdílení.';

  @override
  String get reportSharePreparing => 'Připravuji…';

  @override
  String get reportShareSummaryImage => 'Sdílet obrázek shrnutí';

  @override
  String get reportSharePgn => 'Sdílet soubor PGN';

  @override
  String get reportExportAppearanceTitle => 'Export obrázku';

  @override
  String get reportExportAppearanceSubtitle =>
      'Zvol rozvržení, pozadí a sekce na sdíleném PNG. Náhled odpovídá exportu.';

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
      'PNG alfa — nejlépe na barevném nebo fotografickém pozadí ve Stories.';

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
  String get reportExportShowEvalChart => 'Zahrnout graf evaluace';

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
  String get commonOk => 'OK';

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
  String get lastErrorTitle => 'Poslední chyba';

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
  String get discoveryTitle => 'Najít desku';

  @override
  String get discoveryBlePermissionAndroid =>
      'Povol Bluetooth oprávnění v nastavení systému, aby šlo desku vyhledat.';

  @override
  String discoveryBleScanFailed(String error) {
    return 'Sken Bluetooth selhal: $error';
  }

  @override
  String get discoveryIntro =>
      'Klepni na desku v seznamu. Aplikace se přes Bluetooth spojí a podle nastavení může přejít na Wi‑Fi (STA), pokud je deska online v síti a telefon má Wi‑Fi.';

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
  String get mockBoardTileSubtitle => 'Používá zabalený snapshot pro test UI.';

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
  String get gameClearHintLeds => 'Zrušit nápovědní LED';

  @override
  String get gameNoSnapshotYet => 'Zatím žádný snapshot desky.';

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
  String get gameDisconnectTooltip => 'Odpojit session';

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
    return 'Nejlepší tah: $from→$to';
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
      'Klepnutím na indikátor vlevo nahoře otevřeš správu připojení (Wi‑Fi, Bluetooth, mock) — stejně jako na iOS.';

  @override
  String stockfishEvalFailedSnack(String msg) {
    return 'Evaluace selhala: $msg';
  }

  @override
  String get firmwareDialogTitle => 'Aktualizace firmware desky';

  @override
  String firmwareDialogVersions(String serverVer, String boardVer) {
    return 'Na serveru je $serverVer, deska hlásí $boardVer.';
  }

  @override
  String get firmwareDialogHttpsNote =>
      'Deska stahuje přes HTTPS (Wi‑Fi STA). Telefon posílá jen odkaz.';

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
      'Seznam je prázdný. Klepni na „Najít desku“ a počkej pár sekund v dosahu zapnuté desky.';

  @override
  String get discoveryAdvancedSubtitle => 'Wi‑Fi URL, režim připojení, demo';

  @override
  String get discoveryConnectionModeHeading => 'Režim připojení';

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
  String get settingsConnectionIntro =>
      'Obvykle stačí najít desku přes Bluetooth; aplikace ji pak případně sama přepne na Wi‑Fi, pokud to dává smysl.';

  @override
  String get settingsDisconnect => 'Odpojit';

  @override
  String get settingsAdvanced => 'Pokročilé';

  @override
  String get settingsAdvancedConnectionSubtitle =>
      'Výchozí URL, režim připojení, uložené BLE';

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
  String get settingsDefaultBoardUrl => 'Výchozí URL desky';

  @override
  String get settingsInvalidUrlSnack =>
      'Neplatná URL — zadej hostitele (např. 192.168.4.1 nebo http://…)';

  @override
  String get settingsSavedSnack => 'Uloženo';

  @override
  String get settingsSaveUrl => 'Uložit URL';

  @override
  String get settingsBleOnlyTitle => 'Jen Bluetooth';

  @override
  String get settingsBleOnlySubtitle => 'Nepřepínat na Wi‑Fi po BLE';

  @override
  String get settingsWebSocketTitle => 'WebSocket snapshot';

  @override
  String get settingsWebSocketSubtitle =>
      'Po změně znovu připojit Wi‑Fi session';

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
  String get settingsLayoutFullUiShort => 'Plné UI';

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
  String get settingsFlipBoardSubtitle => 'Černá k sobě';

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
      'Světlý / Tmavý se projeví hned. Systém sleduje vzhled macOS.';

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
      'Pořadí přetahováním: aplikace zkusí prvního poskytovatele, pak dalšího při selhání. Prázdný seznam = jen krátké tipy offline. Klíče zůstávají na tomto zařízení.';

  @override
  String get settingsCoachOfflineOnly => 'Jen offline';

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
  String get gameControlDisplaySection => 'Zobrazení';

  @override
  String get gameControlPlayModeSection => 'Režim hry';

  @override
  String get gameControlSandboxLabel => 'Sandbox';

  @override
  String get gameControlMovesFromApp => 'Tahy z aplikace';

  @override
  String gameControlUndoMove(int n) {
    return 'Vrátit tah ($n)';
  }

  @override
  String get gameControlExitSandbox => 'Ukončit sandbox a obnovit desku';

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
  String get timerWhiteShort => 'Bílá';

  @override
  String get timerBlackShort => 'Černá';

  @override
  String get coachChatTypeFirst => 'Nejdřív napiš otázku, pak Odeslat.';

  @override
  String get coachChatHide => 'Skrýt';

  @override
  String get coachChatDismiss => 'ZAVŘÍT';

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
  String get manualConnSaveUrlSnack => 'URL uložena do prefs.';

  @override
  String get manualConnSaveUrl => 'Uložit URL';

  @override
  String get manualConnTestConnection => 'Test spojení (GET snapshot)';

  @override
  String get manualConnConnectSession => 'Připojit session (Wi‑Fi + poll)';

  @override
  String get manualConnWifiStaNvs => 'Wi‑Fi STA a NVS';

  @override
  String get manualConnRefreshWifi => 'Obnovit stav Wi‑Fi';

  @override
  String get manualConnDisconnectSta => 'Odpojit ESP od STA';

  @override
  String get manualConnClearWifiNvs => 'Smazat Wi‑Fi z NVS';

  @override
  String get haMqttTitle => 'Home Assistant a MQTT';

  @override
  String get haMqttSavedSnack => 'MQTT uloženo na desku.';

  @override
  String get haMqttHowToHa => 'Jak na Home Assistant';

  @override
  String get haMqttBrokerHeader => 'MQTT broker';

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
      '1. Home Assistant musí běžet ve stejné Wi‑Fi síti jako šachovnice (ESP připojené jako klient STA k routeru).\n\n2. V Home Assistantu zapni MQTT broker — nejčastěji doplněk „Mosquitto broker“ (Settings → Add-ons). Poznamenej si port (obvykle 1883) a případné uživatelské jméno a heslo z konfigurace doplňku.\n\n3. Zjisti IP adresu nebo hostname počítače / Raspberry Pi, kde HA běží (např. 192.168.1.42 nebo homeassistant.local). Šachovnice musí na tuto adresu v síti dosáhnout.\n\n4. Níže zadej stejnou adresu jako „Broker (host)“ — jde o adresu MQTT serveru (u běžné instalace stejný stroj jako Home Assistant). Port nech 1883, pokud jsi v Mosquitto neměnil výchozí.\n\n5. Uložením odešleš nastavení do firmware desky přes aktuální spojení (Wi‑Fi nebo Bluetooth). Aplikace CZECHMATE sama k brokeru nepřipojuje — data posílá jen ESP.';

  @override
  String get haMqttHostFieldLabel => 'Broker (host), např. IP Home Assistant';

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
      'Nejdřív připoj šachovnici (Bluetooth nebo Wi‑Fi), pak můžeš uložit broker.';

  @override
  String get haMqttFooterBleSave =>
      'Broker uložíš i přes Bluetooth; stav z desky (tlačítko výše) se načte až při HTTP přes Wi‑Fi.';

  @override
  String get haMqttFooterTroubleshoot =>
      'Pokud MQTT zůstane offline, zkontroluj firewall na HA, heslo Mosquitto a že ESP je ve stejné síti jako broker.';

  @override
  String get haMqttStateConnected => 'připojeno';

  @override
  String get haMqttStateDisconnected => 'nepřipojeno';

  @override
  String get haMqttWifiOk => 'OK';

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
      'Elo se mění po vyřešení hodnoceného puzzlu se známou řešitelnou linií (např. denní puzzle Lichess).';

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
  String get analysisAppBarTitle => 'Analýza';

  @override
  String get analysisGameProgress => 'Průběh partie';

  @override
  String get analysisGameProgressBody => 'Půltahy z připojené desky.';

  @override
  String get analysisStockfishSection => 'Stockfish';

  @override
  String get analysisStockfishSubtitle => 'Eval řádek z aktuální pozice desky.';

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
  String get analysisSandboxFenSnack => 'Sandbox z FEN — záložka Hra.';

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
  String get puzzleDailySolveTitle => 'Denní puzzle';

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
  String get puzzleStop => 'Stop';

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
  String get puzzleBundledOfflineTitle => 'Vestavěné pozice (offline)';

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
  String get progressTabLearn => 'Učení';

  @override
  String get progressTabStats => 'Statistiky';

  @override
  String get progressWelcomeTitle => 'Vítej v CzechMate';

  @override
  String get progressWelcomeBody =>
      'Sleduj puzzle, trenéra a nastavení desky na jednom místě.';

  @override
  String get progressProfilePuzzleEloTitle => 'Profil a puzzle Elo';

  @override
  String get progressProfilePuzzleEloSubtitle =>
      'Místní přezdívka a puzzle rating.';

  @override
  String get progressLearnCardTitle => 'Výuka';

  @override
  String get progressLearnCardSubtitle => 'LED průvodci a AI trenér';

  @override
  String get progressAiCoachTitle => 'AI trenér';

  @override
  String get progressAiCoachBody => 'Ptej se a dostávej nápovědy k pozici.';

  @override
  String get progressLearningModeTile => 'Výukový režim';

  @override
  String get progressCoachLevelLabel => 'Úroveň trenéra';

  @override
  String get progressCoachChatButton => 'Chat s trenérem';

  @override
  String get progressPositionPlanButton => 'Plán pozice';

  @override
  String get progressBoardErrorTitle => 'Chyba desky';

  @override
  String get progressStartingPositionTitle => 'Výchozí postavení';

  @override
  String get progressStartingPositionBody =>
      'Průvodce srovná figury pomocí LED.';

  @override
  String get progressRunWizardStarting =>
      'Spustit průvodce — výchozí postavení';

  @override
  String get progressAccountCardTitle => 'Účet';

  @override
  String get progressAccountCardSubtitle => 'Místní profil a aktivita';

  @override
  String get progressActiveTransportTitle => 'Aktivní transport';

  @override
  String get progressNoSessionTitle => 'Zatím žádná aktivní session';

  @override
  String get progressNoSessionSubtitle => 'Otevři Najít desku pro připojení.';

  @override
  String get helpAppBarTitle => 'czechmate — nápověda';

  @override
  String get helpConnectTitle => 'Připojení desky';

  @override
  String get helpConnectBody =>
      'Pro vyhledání použij Bluetooth, pak Wi‑Fi pokud je dostupné. V Nastavení najdeš ruční URL a režimy.';

  @override
  String get helpPlayingTitle => 'Hraní partie';

  @override
  String get helpPlayingBody =>
      'Karta Hra pro živou synchronizaci, Ovládání hry pro čas a otočení, Analýza pro Stockfish.';

  @override
  String get helpCoachTitle => 'Trenér a analýza';

  @override
  String get helpCoachBody =>
      'Zapni výukový režim a API klíče v Nastavení pro cloud trenéry.';

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
  String get connDiagPolling => 'Polling';

  @override
  String get connDiagWebSocket => 'WebSocket';

  @override
  String get connDiagPollSuccessTitle => 'Úspěšné snapshot GET (včetně 304)';

  @override
  String get connDiagPollFailureTitle => 'Chybné GET / výjimky při poll';

  @override
  String get connDiagWsFramesTitle => 'WS zprávy (rámce)';

  @override
  String get connDiagLastPollOk => 'Poslední úspěšný poll';

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
  String get newGameCustomTimeTitle => 'Vlastní čas (typ 14)';

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
      'Stejně jako na iOS: časová kontrola se odešle na desku a pak začne nová hra.';

  @override
  String get newGameBoardViewSection => 'Pohled na desku (která barva je dole)';

  @override
  String get newGameWhoStartsNote =>
      'Kdo začíná určuje deska / pravidla; zde jen otáčíš pohled (uloženo v Nastavení).';

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
  String get devMoveEvalTile => 'Eval tahů (moveEvaluationEnabled)';

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
  String get devCoachTraceTile => 'Podrobné logy trenéra (coach trace)';

  @override
  String get devStartConnection => 'Spustit připojení';

  @override
  String get devStopTransport => 'Zastavit';

  @override
  String get devTransportStoppedSnack => 'Transport zastaven.';

  @override
  String get devRefreshedFromPrefsSnack => 'Obnoveno z prefs.';

  @override
  String devPingSnapshot(String result) {
    return 'Ping snapshot (RTT): $result';
  }

  @override
  String get devConnectionDiagTile => 'Diagnostika připojení (REST / WS)';

  @override
  String get devDisconnectStaSnack => 'STA odpojeno.';

  @override
  String get devClearWifiNvsTitle => 'Smazat Wi‑Fi z NVS?';

  @override
  String get devClearWifiNvsBody => 'ESP ztratí uloženou síť.';

  @override
  String get devWifiNvsClearedSnack => 'Wi‑Fi NVS vymazáno.';

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
  String get boardNvsEvalMode => 'Eval mód (počítat nejlepší tahy)';

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
  String get boardNvsLiftBeforeBotTarget => 'LED cíl u bota až po zvednutí';

  @override
  String get boardNvsTutorialMode => 'Výukový mód (tutorials)';

  @override
  String get boardNvsConfirmNewGameLed => 'Potvrzovat novou hru LED tlačítkem';

  @override
  String get boardNvsHintLimit => 'Limit nápověd (0 = neomezeno)';

  @override
  String get boardNvsHintTierTitle => 'Typ nápovědy (MoveHintTier)';

  @override
  String get boardNvsHintTierSubtitle =>
      'H1 = jen nejlepší tah, H2 = top 3, H3 = vše dle Stockfish';

  @override
  String get boardNvsH1 => 'H1 – Nejlepší';

  @override
  String get boardNvsH2 => 'H2 – Top 3';

  @override
  String get boardNvsH3 => 'H3 – Vše';

  @override
  String get boardNvsOpponentHeader => 'Nastavení oponenta (bota)';

  @override
  String get boardNvsBotMode => 'Režim bota';

  @override
  String get boardNvsPvp => 'Hráč vs hráč (vypnuto)';

  @override
  String get boardNvsPvb => 'Hráč vs bot';

  @override
  String get boardNvsBotStrength => 'Síla bota (úroveň)';

  @override
  String get boardNvsBotPlaysAs => 'Za koho hraje bot';

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
      Object b, Object g, Object h, Object mode, Object r, Object t) {
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
  String get puzzleGoPlaySandbox => 'Sandbox — záložka Hra.';

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
      'Připoj desku přes Wi‑Fi nebo Bluetooth (mock režim tutoriál nepodporuje).';

  @override
  String get setupWizardErrNoSteps => 'Z FEN nešlo sestavit žádné kroky.';

  @override
  String get setupWizardErrGeneric => 'Chyba';

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
      'LED na desce ukazuje cílové pole. Postup se potvrdí senzorem matice nebo správnou figurkou ve snapshotu.';

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
  String get settingsHaMqttOpenButton =>
      'Home Assistant a MQTT (návod + formulář)';

  @override
  String get settingsTileBoardLightTitle => 'Světlo desky';

  @override
  String get settingsTileBoardLightSubtitle =>
      'Barva, jas, scény, auto‑vypnutí';

  @override
  String get settingsTileModulesTitle => 'Moduly a učení';

  @override
  String get settingsTileModulesSubtitle =>
      'Úvod, puzzle, profil, pokrok, nápověda';

  @override
  String get settingsNavAppTour => 'Průvodce aplikací (úvod)';

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
  String get settingsTileAboutTitle => 'O aplikaci';

  @override
  String get settingsTileAboutSubtitle => 'Verze, soukromí, systém';

  @override
  String get settingsAboutVersionLoading => 'Načítám verzi…';

  @override
  String settingsAboutVersionLine(String version, String build) {
    return 'Verze $version ($build) • Klepnutím na „Nastavení“ v liště 7× odemkneš vývojářský režim.';
  }

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
      'iPhone: Live Activity (Lock Screen / Dynamic Island, iOS 16.2+). Android: probíhající notifikace (Android 13+ je potřeba povolit oznámení). Zapíná se z herní obrazovky.';

  @override
  String get settingsLiveActivityIosDisabledSnack =>
      'Live Activities jsou v systému vypnuté. Zapněte je v Nastavení → czechmate → Live Activities.';

  @override
  String get settingsWearMirrorTitle => 'Zrcadlit časomíru na Wear OS';

  @override
  String get settingsWearMirrorSubtitle =>
      'Google Data Layer — stejný stav jako probíhající notifikace; vyžaduje párování hodinek Wear OS.';

  @override
  String get settingsWatchMirrorTitle => 'Zrcadlit časomíru na Apple Watch';

  @override
  String get settingsWatchMirrorSubtitle =>
      'WatchConnectivity — stejný stav jako Live Activity; na hodinkách je základní UI + Pauza/Pokračovat.';

  @override
  String get settingsFactoryTileTitle => 'Tovární reset desky';

  @override
  String get settingsFactoryTileSubtitle =>
      'Smaže NVS na ESP — zařízení jako přístupový bod';

  @override
  String get settingsFactoryDialogTitle => 'Tovární reset';

  @override
  String get settingsFactoryDialogBody =>
      'Smazat celé NVS desky (Wi‑Fi, hesla, MQTT, předvolby)? Zařízení se restartuje jako Wi‑Fi přístupový bod.';

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
      'Zadej URL končící na /v1 (např. :11434/v1). Jen :11434 bez /v1 dřív nefungovalo — u portu 11434 to teď appka doplní.';

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
    return 'Při uložení na desku se z aplikace sloučí Stockfish hloubka ($depth) a přepínač eval ($eval) do NVS payloadu (stejně jako iOS).';
  }

  @override
  String get boardNvsEvalOn => 'zap.';

  @override
  String get boardNvsEvalOff => 'vyp.';

  @override
  String get boardNvsHintTierFootnote =>
      'H1–H3 v telefonu (`czechmate.moveHintTier`) je samostatné od LED úrovně na desce; při POST výše se do NVS propíše i hloubka/eval z diagnostiky aplikace.';

  @override
  String get boardNvsFooterMock =>
      'Ukázková deska: NVS zápis se neprovede na reálný hardware.';

  @override
  String get boardNvsFooterWifiActive =>
      'Wi‑Fi session je aktivní — tlačítkem „Uložit“ odešleš bot režim do NVS na ESP.';

  @override
  String get boardNvsFooterBle =>
      'BLE: podle firmware lze posílat NVS/UI blob; pro jistotu ověř stav na webu desky.';

  @override
  String get boardNvsFooterGeneric =>
      'Pro spolehlivý zápis bota do NVS připoj desku (Wi‑Fi doporučeno).';

  @override
  String get boardNvsHttpBleExplain =>
      'Bluetooth je připojený, ale načtení NVS z této obrazovky probíhá přes HTTP. V Nastavení ulož platnou adresu desky (STA IP, např. http://192.168.x.x), nebo aktivuj Wi‑Fi session na tuto IP.';

  @override
  String get boardNvsHttpWifiNoUrl =>
      'Wi‑Fi session bez platné základní URL. Obnov připojení na záložce Hra → správa šachovnice.';

  @override
  String get boardNvsHttpMissingUrl =>
      'Chybí platná HTTP adresa desky. V Nastavení vyplň „Default board URL“ (kompletní http://…) a zkus znovu (ikonka ↻).';

  @override
  String get boardHttpMissingUrlBody =>
      'Adresa HTTP desky chybí nebo je neplatná. V Nastavení ulož celou URL (např. http://192.168.4.1 nebo STA IP desky). Bluetooth může fungovat i bez této URL pro živou hru.';

  @override
  String get boardHttpFailBle =>
      'Bluetooth je připojený, ale HTTP požadavek selhal (špatná URL, timeout nebo web lock).';

  @override
  String get boardHttpFailWifi =>
      'Wi‑Fi session aktivní — ověř URL desky a že deska odpovídá.';

  @override
  String get boardHttpFailMock => 'Ukázková deska — HTTP na hardware neplatí.';

  @override
  String get boardHttpFailNone => 'Žádné aktivní připojení k desce.';

  @override
  String boardHttpFailDetail(String link, String detail) {
    return '$link Detail: $detail';
  }

  @override
  String boardHttpErrGeneric(String error) {
    return 'Chyba komunikace s deskou: $error';
  }

  @override
  String get progressSegmentBeg => 'Zač.';

  @override
  String get progressSegmentInt => 'Stř.';

  @override
  String get progressSegmentAdv => 'Pokr.';

  @override
  String get progressSegmentExp => 'Exp.';

  @override
  String get progressLearningModeTitle => 'Výukový režim';

  @override
  String get progressCoachLevelTitle => 'Úroveň trenéra';

  @override
  String get progressTransportActiveTitle => 'Aktivní přenos';

  @override
  String get progressNoSessionYet => 'Zatím žádná aktivní session';

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
  String get analysisIntroEvalHint =>
      'Eval tahů a graf — chess-api nebo vlastní URL v Nastavení (vývojář).';

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
      'Eval pozice po tahu (perspektiva bílého: + výhoda bílého)';

  @override
  String get analysisChartFillHintLiveEvalOn =>
      'Graf se naplní po tazích, až Stockfish vyhodnotí pozici (hraj na kartě Hra s internetem).';

  @override
  String get analysisChartFillHintLiveEvalOff =>
      'Po zapnutí automatického vyhodnocení uvidíš křivku zde.';

  @override
  String get analysisClearEvalData => 'Vymazat graf a uložené evaluace tahů';

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
      'Načíst druhou variantu (ze session FEN)';

  @override
  String analysisSecondLineSameMove(String from, String to) {
    return 'Obě hloubiny dávají stejný tah $from→$to.';
  }

  @override
  String analysisSecondLineEvalApprox(String pawns) {
    return ' · eval ≈ $pawns pěš.';
  }

  @override
  String get analysisFreeAnalyzing => 'Počítám…';

  @override
  String analysisBestMoveLine(
      String from, String to, String evalSuffix, String extra) {
    return 'Nejlepší tah: $from–$to$evalSuffix$extra';
  }

  @override
  String analysisQualitySummaryLine(String quality, String avgCp, int count) {
    return '$quality$avgCp · $count tahů';
  }

  @override
  String analysisAvgLossCp(String cp) {
    return ' · Ø ztráta $cp cp';
  }

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
  String get gameSandboxIllegalMoveSandbox => 'Neplatný tah (sandbox)';

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
  String get gameDemoSnapshotReloadedSnack => 'Demo snapshot znovu načten';

  @override
  String get timerCenterGameOver => 'Konec partie';

  @override
  String get timerCenterRunning => 'Běží';

  @override
  String get timerCenterStopped => 'Stojí';

  @override
  String get coachSetupBannerBody =>
      'Seznam priorit trenéra není prázdný, ale žádný poskytovatel nemá vyplněné přihlašování. Otevři Nastavení → Trenér a AI a doplň API klíče (nebo URL Ollamy) pro alespoň jednoho z nich.';

  @override
  String coachErrorSomethingWrong(String error) {
    return 'Něco se pokazilo: $error';
  }

  @override
  String get coachOfflineLabel => 'Offline';

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
