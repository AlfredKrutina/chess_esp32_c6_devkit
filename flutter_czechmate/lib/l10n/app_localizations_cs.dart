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
  String get discoveryWifiUrlHint => 'Adresa desky (http://…)';

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
}
