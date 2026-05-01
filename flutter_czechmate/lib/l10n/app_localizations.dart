import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:intl/intl.dart' as intl;

import 'app_localizations_cs.dart';
import 'app_localizations_en.dart';

// ignore_for_file: type=lint

/// Callers can lookup localized strings with an instance of AppLocalizations
/// returned by `AppLocalizations.of(context)`.
///
/// Applications need to include `AppLocalizations.delegate()` in their app's
/// `localizationDelegates` list, and the locales they support in the app's
/// `supportedLocales` list. For example:
///
/// ```dart
/// import 'l10n/app_localizations.dart';
///
/// return MaterialApp(
///   localizationsDelegates: AppLocalizations.localizationsDelegates,
///   supportedLocales: AppLocalizations.supportedLocales,
///   home: MyApplicationHome(),
/// );
/// ```
///
/// ## Update pubspec.yaml
///
/// Please make sure to update your pubspec.yaml to include the following
/// packages:
///
/// ```yaml
/// dependencies:
///   # Internationalization support.
///   flutter_localizations:
///     sdk: flutter
///   intl: any # Use the pinned version from flutter_localizations
///
///   # Rest of dependencies
/// ```
///
/// ## iOS Applications
///
/// iOS applications define key application metadata, including supported
/// locales, in an Info.plist file that is built into the application bundle.
/// To configure the locales supported by your app, you’ll need to edit this
/// file.
///
/// First, open your project’s ios/Runner.xcworkspace Xcode workspace file.
/// Then, in the Project Navigator, open the Info.plist file under the Runner
/// project’s Runner folder.
///
/// Next, select the Information Property List item, select Add Item from the
/// Editor menu, then select Localizations from the pop-up menu.
///
/// Select and expand the newly-created Localizations item then, for each
/// locale your application supports, add a new item and select the locale
/// you wish to add from the pop-up menu in the Value field. This list should
/// be consistent with the languages listed in the AppLocalizations.supportedLocales
/// property.
abstract class AppLocalizations {
  AppLocalizations(String locale)
      : localeName = intl.Intl.canonicalizedLocale(locale.toString());

  final String localeName;

  static AppLocalizations? of(BuildContext context) {
    return Localizations.of<AppLocalizations>(context, AppLocalizations);
  }

  static const LocalizationsDelegate<AppLocalizations> delegate =
      _AppLocalizationsDelegate();

  /// A list of this localizations delegate along with the default localizations
  /// delegates.
  ///
  /// Returns a list of localizations delegates containing this delegate along with
  /// GlobalMaterialLocalizations.delegate, GlobalCupertinoLocalizations.delegate,
  /// and GlobalWidgetsLocalizations.delegate.
  ///
  /// Additional delegates can be added by appending to this list in
  /// MaterialApp. This list does not have to be used at all if a custom list
  /// of delegates is preferred or required.
  static const List<LocalizationsDelegate<dynamic>> localizationsDelegates =
      <LocalizationsDelegate<dynamic>>[
    delegate,
    GlobalMaterialLocalizations.delegate,
    GlobalCupertinoLocalizations.delegate,
    GlobalWidgetsLocalizations.delegate,
  ];

  /// A list of this localizations delegate's supported locales.
  static const List<Locale> supportedLocales = <Locale>[
    Locale('en'),
    Locale('cs')
  ];

  /// No description provided for @appTitle.
  ///
  /// In en, this message translates to:
  /// **'CzechMate'**
  String get appTitle;

  /// No description provided for @navPlay.
  ///
  /// In en, this message translates to:
  /// **'Play'**
  String get navPlay;

  /// No description provided for @navAnalysis.
  ///
  /// In en, this message translates to:
  /// **'Analysis'**
  String get navAnalysis;

  /// No description provided for @navPuzzle.
  ///
  /// In en, this message translates to:
  /// **'Puzzle'**
  String get navPuzzle;

  /// No description provided for @navProgress.
  ///
  /// In en, this message translates to:
  /// **'Progress'**
  String get navProgress;

  /// No description provided for @navSettings.
  ///
  /// In en, this message translates to:
  /// **'Settings'**
  String get navSettings;

  /// No description provided for @settingsLanguage.
  ///
  /// In en, this message translates to:
  /// **'Language'**
  String get settingsLanguage;

  /// No description provided for @languageSystem.
  ///
  /// In en, this message translates to:
  /// **'System'**
  String get languageSystem;

  /// No description provided for @languageEnglish.
  ///
  /// In en, this message translates to:
  /// **'English'**
  String get languageEnglish;

  /// No description provided for @languageCzech.
  ///
  /// In en, this message translates to:
  /// **'Czech'**
  String get languageCzech;

  /// No description provided for @languageDescription.
  ///
  /// In en, this message translates to:
  /// **'Choose app language. System follows the device; unsupported languages default to English.'**
  String get languageDescription;

  /// No description provided for @onboardingWelcomeTitle.
  ///
  /// In en, this message translates to:
  /// **'Welcome to CZECHMATE'**
  String get onboardingWelcomeTitle;

  /// No description provided for @onboardingSkip.
  ///
  /// In en, this message translates to:
  /// **'Skip'**
  String get onboardingSkip;

  /// No description provided for @onboardingNext.
  ///
  /// In en, this message translates to:
  /// **'Next'**
  String get onboardingNext;

  /// No description provided for @onboardingStart.
  ///
  /// In en, this message translates to:
  /// **'Start playing'**
  String get onboardingStart;

  /// No description provided for @onboard1Title.
  ///
  /// In en, this message translates to:
  /// **'Smart chessboard'**
  String get onboard1Title;

  /// No description provided for @onboard1Body.
  ///
  /// In en, this message translates to:
  /// **'Connect your CZECHMATE board over Wi‑Fi or Bluetooth and play with live sync.'**
  String get onboard1Body;

  /// No description provided for @onboard2Title.
  ///
  /// In en, this message translates to:
  /// **'Set up the pieces'**
  String get onboard2Title;

  /// No description provided for @onboard2Body.
  ///
  /// In en, this message translates to:
  /// **'Before powering on, place pieces in the starting position. The app will align with the board automatically.'**
  String get onboard2Body;

  /// No description provided for @onboard3Title.
  ///
  /// In en, this message translates to:
  /// **'LED hints'**
  String get onboard3Title;

  /// No description provided for @onboard3Body.
  ///
  /// In en, this message translates to:
  /// **'The board can highlight moves with LEDs. Hint depth is configured under Settings → Board NVS.'**
  String get onboard3Body;

  /// No description provided for @onboard4Title.
  ///
  /// In en, this message translates to:
  /// **'Start a game'**
  String get onboard4Title;

  /// No description provided for @onboard4Body.
  ///
  /// In en, this message translates to:
  /// **'On Play, tap New game and pick a time control. Flip the board (White or Black on bottom) from Game controls or Settings.'**
  String get onboard4Body;

  /// No description provided for @onboard5Title.
  ///
  /// In en, this message translates to:
  /// **'Coach & analysis'**
  String get onboard5Title;

  /// No description provided for @onboard5Body.
  ///
  /// In en, this message translates to:
  /// **'Turn on learning mode and Stockfish evaluation on Analysis for deeper insight.'**
  String get onboard5Body;

  /// No description provided for @netNoInternetTitle.
  ///
  /// In en, this message translates to:
  /// **'No internet'**
  String get netNoInternetTitle;

  /// No description provided for @netNoInternetBody.
  ///
  /// In en, this message translates to:
  /// **'Stockfish (chess-api), Lichess, and other cloud features need internet. You can still use the board over Bluetooth offline.'**
  String get netNoInternetBody;

  /// No description provided for @netNotOnWifiTitle.
  ///
  /// In en, this message translates to:
  /// **'Not on Wi‑Fi'**
  String get netNotOnWifiTitle;

  /// No description provided for @netNotOnWifiBody.
  ///
  /// In en, this message translates to:
  /// **'chess-api usually works over mobile data. To open the board’s HTTP API on a LAN IP, the phone typically needs Wi‑Fi on the same network. If you are only on the board’s hotspot without internet, DNS for chess-api / Lichess may fail — temporarily disable that Wi‑Fi, enable mobile data, or use dual‑stack (Wi‑Fi + cellular data) if your OS allows. The app cannot force traffic to cellular while Wi‑Fi is the default route.'**
  String get netNotOnWifiBody;

  /// No description provided for @errInvalidBoardUrl.
  ///
  /// In en, this message translates to:
  /// **'Invalid board URL — hostname missing. Use http://192.168.4.1 or your board STA IP.'**
  String get errInvalidBoardUrl;

  /// No description provided for @errNoSavedBle.
  ///
  /// In en, this message translates to:
  /// **'No saved Bluetooth board. Open Board discovery and pair your CZECHMATE board.'**
  String get errNoSavedBle;

  /// No description provided for @errDemoNoSnapshot.
  ///
  /// In en, this message translates to:
  /// **'Demo board has no snapshot.'**
  String get errDemoNoSnapshot;

  /// No description provided for @errConnectBeforeMoves.
  ///
  /// In en, this message translates to:
  /// **'Connect to the board over Wi‑Fi or Bluetooth before sending moves from the app.'**
  String get errConnectBeforeMoves;

  /// No description provided for @errSetupNeedsConnection.
  ///
  /// In en, this message translates to:
  /// **'Setup wizard requires an active board connection (Wi‑Fi or Bluetooth).'**
  String get errSetupNeedsConnection;

  /// No description provided for @errOtaHttps.
  ///
  /// In en, this message translates to:
  /// **'OTA requires an HTTPS URL to the .bin file.'**
  String get errOtaHttps;

  /// No description provided for @errOtaMock.
  ///
  /// In en, this message translates to:
  /// **'OTA is not available with the mock board.'**
  String get errOtaMock;

  /// No description provided for @errOtaConnectFirst.
  ///
  /// In en, this message translates to:
  /// **'Connect the board over Wi‑Fi or Bluetooth, then try the update again.'**
  String get errOtaConnectFirst;

  /// No description provided for @errHintsNeedConnection.
  ///
  /// In en, this message translates to:
  /// **'Hint LEDs require a board connection. Pair via Bluetooth or connect Wi‑Fi first.'**
  String get errHintsNeedConnection;

  /// No description provided for @errNoActiveSession.
  ///
  /// In en, this message translates to:
  /// **'No active board session. Connect via Wi‑Fi or Bluetooth from Board discovery.'**
  String get errNoActiveSession;

  /// No description provided for @errBrightnessNeedsBoard.
  ///
  /// In en, this message translates to:
  /// **'Brightness control needs a connected board (Wi‑Fi or Bluetooth).'**
  String get errBrightnessNeedsBoard;

  /// No description provided for @errLampNeedsBoard.
  ///
  /// In en, this message translates to:
  /// **'Lamp control needs a connected board (Wi‑Fi or Bluetooth).'**
  String get errLampNeedsBoard;

  /// No description provided for @errGameLampNeedsBoard.
  ///
  /// In en, this message translates to:
  /// **'Game lamp mode requires a connected board (Wi‑Fi or Bluetooth).'**
  String get errGameLampNeedsBoard;

  /// No description provided for @errAutoLampNeedsBoard.
  ///
  /// In en, this message translates to:
  /// **'Auto lamp timeout requires a connected board (Wi‑Fi or Bluetooth).'**
  String get errAutoLampNeedsBoard;

  /// No description provided for @moveEvalExcellentBest.
  ///
  /// In en, this message translates to:
  /// **'Excellent move — that was the best move.'**
  String get moveEvalExcellentBest;

  /// No description provided for @moveEvalExcellentEngine.
  ///
  /// In en, this message translates to:
  /// **'Excellent — matches the engine best move.'**
  String get moveEvalExcellentEngine;

  /// No description provided for @moveEvalWeakerNoScore.
  ///
  /// In en, this message translates to:
  /// **'Weaker move (no exact score). Stronger was {best}.'**
  String moveEvalWeakerNoScore(String best);

  /// No description provided for @moveEvalGoodLoss.
  ///
  /// In en, this message translates to:
  /// **'Good move — small loss (~{cp} cp).'**
  String moveEvalGoodLoss(int cp);

  /// No description provided for @moveEvalInaccuracy.
  ///
  /// In en, this message translates to:
  /// **'Inaccuracy — about {cp} cp worse. Stronger was {best}.'**
  String moveEvalInaccuracy(int cp, String best);

  /// No description provided for @moveEvalMistake.
  ///
  /// In en, this message translates to:
  /// **'Mistake — about {cp} cp worse. Better was {best}.'**
  String moveEvalMistake(int cp, String best);

  /// No description provided for @moveEvalBlunder.
  ///
  /// In en, this message translates to:
  /// **'Blunder — about {cp} cp worse. Much better was {best}.'**
  String moveEvalBlunder(int cp, String best);

  /// No description provided for @moveEvalFailed.
  ///
  /// In en, this message translates to:
  /// **'Evaluation failed: {error}'**
  String moveEvalFailed(String error);

  /// No description provided for @pgnEventHeader.
  ///
  /// In en, this message translates to:
  /// **'CZECHMATE'**
  String get pgnEventHeader;

  /// No description provided for @pgnOpeningPrefix.
  ///
  /// In en, this message translates to:
  /// **'Start'**
  String get pgnOpeningPrefix;

  /// No description provided for @sharePgnSubject.
  ///
  /// In en, this message translates to:
  /// **'Chess game (PGN)'**
  String get sharePgnSubject;

  /// No description provided for @shareSummaryNotReady.
  ///
  /// In en, this message translates to:
  /// **'Summary isn’t ready yet — try again in a moment.'**
  String get shareSummaryNotReady;

  /// No description provided for @shareExportFailed.
  ///
  /// In en, this message translates to:
  /// **'Export failed: {error}'**
  String shareExportFailed(String error);

  /// No description provided for @sharePngEncodeError.
  ///
  /// In en, this message translates to:
  /// **'Could not encode image as PNG.'**
  String get sharePngEncodeError;

  /// No description provided for @shareGameSummaryMeta.
  ///
  /// In en, this message translates to:
  /// **'{result} · {duration} · {moves} moves'**
  String shareGameSummaryMeta(String result, String duration, int moves);

  /// No description provided for @shareGameSummaryLine.
  ///
  /// In en, this message translates to:
  /// **'Game summary — CzechMate · {meta}'**
  String shareGameSummaryLine(String meta);

  /// No description provided for @reportNoGameData.
  ///
  /// In en, this message translates to:
  /// **'No game data.'**
  String get reportNoGameData;

  /// No description provided for @reportMoveEvaluation.
  ///
  /// In en, this message translates to:
  /// **'Move evaluation'**
  String get reportMoveEvaluation;

  /// No description provided for @reportNoStockfishData.
  ///
  /// In en, this message translates to:
  /// **'No Stockfish data — enable move evaluation and play a game.'**
  String get reportNoStockfishData;

  /// No description provided for @reportEvalPerspective.
  ///
  /// In en, this message translates to:
  /// **'From White’s perspective (+ favors White)'**
  String get reportEvalPerspective;

  /// No description provided for @reportMoveTiming.
  ///
  /// In en, this message translates to:
  /// **'Move timing'**
  String get reportMoveTiming;

  /// No description provided for @reportTimingUnavailable.
  ///
  /// In en, this message translates to:
  /// **'Move timestamps weren’t included — timing charts are unavailable.'**
  String get reportTimingUnavailable;

  /// No description provided for @reportElapsedTime.
  ///
  /// In en, this message translates to:
  /// **'Elapsed time'**
  String get reportElapsedTime;

  /// No description provided for @reportElapsedCaption.
  ///
  /// In en, this message translates to:
  /// **'Running total from game start by half-move'**
  String get reportElapsedCaption;

  /// No description provided for @reportHalfMoveAxis.
  ///
  /// In en, this message translates to:
  /// **'half-move → minutes'**
  String get reportHalfMoveAxis;

  /// No description provided for @reportTimePerMove.
  ///
  /// In en, this message translates to:
  /// **'Time per move'**
  String get reportTimePerMove;

  /// No description provided for @reportSecondsSincePrev.
  ///
  /// In en, this message translates to:
  /// **'Seconds since previous move'**
  String get reportSecondsSincePrev;

  /// No description provided for @colorWhite.
  ///
  /// In en, this message translates to:
  /// **'White'**
  String get colorWhite;

  /// No description provided for @colorBlack.
  ///
  /// In en, this message translates to:
  /// **'Black'**
  String get colorBlack;

  /// No description provided for @reportMaterialCaptured.
  ///
  /// In en, this message translates to:
  /// **'Material captured · White {w} pts · Black {b} pts'**
  String reportMaterialCaptured(int w, int b);

  /// No description provided for @reportAppBarTitle.
  ///
  /// In en, this message translates to:
  /// **'Game summary'**
  String get reportAppBarTitle;

  /// No description provided for @reportDone.
  ///
  /// In en, this message translates to:
  /// **'Done'**
  String get reportDone;

  /// No description provided for @reportResultHeading.
  ///
  /// In en, this message translates to:
  /// **'Result'**
  String get reportResultHeading;

  /// No description provided for @reportShareHeading.
  ///
  /// In en, this message translates to:
  /// **'Share'**
  String get reportShareHeading;

  /// No description provided for @reportShareExportHint.
  ///
  /// In en, this message translates to:
  /// **'Export the summary card above as a PNG, then use the system share sheet.'**
  String get reportShareExportHint;

  /// No description provided for @reportSharePreparing.
  ///
  /// In en, this message translates to:
  /// **'Preparing…'**
  String get reportSharePreparing;

  /// No description provided for @reportShareSummaryImage.
  ///
  /// In en, this message translates to:
  /// **'Share summary image'**
  String get reportShareSummaryImage;

  /// No description provided for @reportSharePgn.
  ///
  /// In en, this message translates to:
  /// **'Share PGN file'**
  String get reportSharePgn;

  /// No description provided for @reportClock.
  ///
  /// In en, this message translates to:
  /// **'Clock'**
  String get reportClock;

  /// No description provided for @reportMinutesShort.
  ///
  /// In en, this message translates to:
  /// **'{n} min'**
  String reportMinutesShort(String n);

  /// No description provided for @reportFinalFen.
  ///
  /// In en, this message translates to:
  /// **'Final FEN'**
  String get reportFinalFen;

  /// No description provided for @resultWhiteWins.
  ///
  /// In en, this message translates to:
  /// **'White wins'**
  String get resultWhiteWins;

  /// No description provided for @resultBlackWins.
  ///
  /// In en, this message translates to:
  /// **'Black wins'**
  String get resultBlackWins;

  /// No description provided for @resultDraw.
  ///
  /// In en, this message translates to:
  /// **'Draw'**
  String get resultDraw;

  /// No description provided for @resultGameEnded.
  ///
  /// In en, this message translates to:
  /// **'Game ended'**
  String get resultGameEnded;

  /// No description provided for @endReasonUnknown.
  ///
  /// In en, this message translates to:
  /// **'Unknown result'**
  String get endReasonUnknown;

  /// No description provided for @endReasonGameOver.
  ///
  /// In en, this message translates to:
  /// **'Game over'**
  String get endReasonGameOver;

  /// No description provided for @endReasonCheckmate.
  ///
  /// In en, this message translates to:
  /// **'Checkmate'**
  String get endReasonCheckmate;

  /// No description provided for @endReasonStalemate.
  ///
  /// In en, this message translates to:
  /// **'Stalemate'**
  String get endReasonStalemate;

  /// No description provided for @endReasonFinished.
  ///
  /// In en, this message translates to:
  /// **'Finished'**
  String get endReasonFinished;

  /// No description provided for @statTime.
  ///
  /// In en, this message translates to:
  /// **'TIME'**
  String get statTime;

  /// No description provided for @statMoves.
  ///
  /// In en, this message translates to:
  /// **'MOVES'**
  String get statMoves;

  /// No description provided for @statWhiteAvg.
  ///
  /// In en, this message translates to:
  /// **'WHITE Ø'**
  String get statWhiteAvg;

  /// No description provided for @statBlackAvg.
  ///
  /// In en, this message translates to:
  /// **'BLACK Ø'**
  String get statBlackAvg;

  /// No description provided for @durationDash.
  ///
  /// In en, this message translates to:
  /// **'—'**
  String get durationDash;

  /// No description provided for @secondsShort.
  ///
  /// In en, this message translates to:
  /// **'{n}s'**
  String secondsShort(int n);

  /// No description provided for @minutesSeconds.
  ///
  /// In en, this message translates to:
  /// **'{m}:{s}'**
  String minutesSeconds(int m, String s);

  /// No description provided for @hoursMinutesSeconds.
  ///
  /// In en, this message translates to:
  /// **'{h}h {mm}m {s}s'**
  String hoursMinutesSeconds(int h, int mm, int s);

  /// No description provided for @commonClose.
  ///
  /// In en, this message translates to:
  /// **'Close'**
  String get commonClose;

  /// No description provided for @commonCancel.
  ///
  /// In en, this message translates to:
  /// **'Cancel'**
  String get commonCancel;

  /// No description provided for @commonOk.
  ///
  /// In en, this message translates to:
  /// **'OK'**
  String get commonOk;

  /// No description provided for @commonSave.
  ///
  /// In en, this message translates to:
  /// **'Save'**
  String get commonSave;

  /// No description provided for @commonContinue.
  ///
  /// In en, this message translates to:
  /// **'Continue'**
  String get commonContinue;

  /// No description provided for @commonRetry.
  ///
  /// In en, this message translates to:
  /// **'Retry'**
  String get commonRetry;

  /// No description provided for @commonLoading.
  ///
  /// In en, this message translates to:
  /// **'Loading…'**
  String get commonLoading;

  /// No description provided for @commonError.
  ///
  /// In en, this message translates to:
  /// **'Error'**
  String get commonError;

  /// No description provided for @commonYes.
  ///
  /// In en, this message translates to:
  /// **'Yes'**
  String get commonYes;

  /// No description provided for @commonNo.
  ///
  /// In en, this message translates to:
  /// **'No'**
  String get commonNo;

  /// No description provided for @lastErrorTitle.
  ///
  /// In en, this message translates to:
  /// **'Last error'**
  String get lastErrorTitle;

  /// No description provided for @telemetryPrivacyTitle.
  ///
  /// In en, this message translates to:
  /// **'Privacy'**
  String get telemetryPrivacyTitle;

  /// No description provided for @telemetryPrivacyBody.
  ///
  /// In en, this message translates to:
  /// **'This build does not send analytics. Traffic goes only to your board on the local network or over Bluetooth.'**
  String get telemetryPrivacyBody;

  /// No description provided for @telemetryIcloudTitle.
  ///
  /// In en, this message translates to:
  /// **'iCloud'**
  String get telemetryIcloudTitle;

  /// No description provided for @telemetryIcloudBody.
  ///
  /// In en, this message translates to:
  /// **'Game data stays on device unless you export or enable sync elsewhere.'**
  String get telemetryIcloudBody;

  /// No description provided for @discoveryTitle.
  ///
  /// In en, this message translates to:
  /// **'Find board'**
  String get discoveryTitle;

  /// No description provided for @discoveryBlePermissionAndroid.
  ///
  /// In en, this message translates to:
  /// **'Allow Bluetooth permissions in system settings to scan for BLE boards.'**
  String get discoveryBlePermissionAndroid;

  /// No description provided for @discoveryBleScanFailed.
  ///
  /// In en, this message translates to:
  /// **'Bluetooth scan failed: {error}'**
  String discoveryBleScanFailed(String error);

  /// No description provided for @discoveryIntro.
  ///
  /// In en, this message translates to:
  /// **'Tap a board in the list. The app connects over Bluetooth and may switch to Wi‑Fi (STA) when the board is online on the network and your phone has Wi‑Fi.'**
  String get discoveryIntro;

  /// No description provided for @transportDisconnected.
  ///
  /// In en, this message translates to:
  /// **'Disconnected'**
  String get transportDisconnected;

  /// No description provided for @transportDemo.
  ///
  /// In en, this message translates to:
  /// **'Demo mode'**
  String get transportDemo;

  /// No description provided for @transportWifi.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi'**
  String get transportWifi;

  /// No description provided for @transportBluetooth.
  ///
  /// In en, this message translates to:
  /// **'Bluetooth'**
  String get transportBluetooth;

  /// No description provided for @transportConnecting.
  ///
  /// In en, this message translates to:
  /// **'{transport} · connecting…'**
  String transportConnecting(String transport);

  /// No description provided for @defaultBoardName.
  ///
  /// In en, this message translates to:
  /// **'CZECHMATE board'**
  String get defaultBoardName;

  /// No description provided for @discoveryWifiUrlHint.
  ///
  /// In en, this message translates to:
  /// **'Board URL (http://…)'**
  String get discoveryWifiUrlHint;

  /// No description provided for @discoveryConnectWifi.
  ///
  /// In en, this message translates to:
  /// **'Connect via Wi‑Fi'**
  String get discoveryConnectWifi;

  /// No description provided for @discoveryFindBle.
  ///
  /// In en, this message translates to:
  /// **'Find board (Bluetooth)'**
  String get discoveryFindBle;

  /// No description provided for @discoveryScanning.
  ///
  /// In en, this message translates to:
  /// **'Scanning…'**
  String get discoveryScanning;

  /// No description provided for @discoveryStopScan.
  ///
  /// In en, this message translates to:
  /// **'Stop scan'**
  String get discoveryStopScan;

  /// No description provided for @discoveryManualUrl.
  ///
  /// In en, this message translates to:
  /// **'Manual URL entry'**
  String get discoveryManualUrl;

  /// No description provided for @connectionModeAuto.
  ///
  /// In en, this message translates to:
  /// **'Auto (Bluetooth then Wi‑Fi)'**
  String get connectionModeAuto;

  /// No description provided for @connectionModeWifiOnly.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi only'**
  String get connectionModeWifiOnly;

  /// No description provided for @connectionModeBleOnly.
  ///
  /// In en, this message translates to:
  /// **'Bluetooth only'**
  String get connectionModeBleOnly;

  /// No description provided for @preferBleOnlyTitle.
  ///
  /// In en, this message translates to:
  /// **'Bluetooth only (don’t switch to Wi‑Fi)'**
  String get preferBleOnlyTitle;

  /// No description provided for @preferBleOnlySubtitle.
  ///
  /// In en, this message translates to:
  /// **'After BLE connect, stay on Bluetooth even if the board knows a Wi‑Fi URL.'**
  String get preferBleOnlySubtitle;

  /// No description provided for @mockBoardTileTitle.
  ///
  /// In en, this message translates to:
  /// **'Demo board (no hardware)'**
  String get mockBoardTileTitle;

  /// No description provided for @mockBoardTileSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Uses bundled snapshot for UI testing.'**
  String get mockBoardTileSubtitle;

  /// No description provided for @sessionConnectedWifi.
  ///
  /// In en, this message translates to:
  /// **'Connected via Wi‑Fi'**
  String get sessionConnectedWifi;

  /// No description provided for @sessionConnectedBle.
  ///
  /// In en, this message translates to:
  /// **'Connected via Bluetooth'**
  String get sessionConnectedBle;

  /// No description provided for @sessionConnectedMock.
  ///
  /// In en, this message translates to:
  /// **'Demo board active'**
  String get sessionConnectedMock;

  /// No description provided for @allowBluetoothSettings.
  ///
  /// In en, this message translates to:
  /// **'Allow Bluetooth permissions in settings to scan for BLE boards.'**
  String get allowBluetoothSettings;

  /// No description provided for @gameAppBarTitle.
  ///
  /// In en, this message translates to:
  /// **'czechmate'**
  String get gameAppBarTitle;

  /// No description provided for @gameConnectionTooltip.
  ///
  /// In en, this message translates to:
  /// **'Board connection'**
  String get gameConnectionTooltip;

  /// No description provided for @gameNewGame.
  ///
  /// In en, this message translates to:
  /// **'New game'**
  String get gameNewGame;

  /// No description provided for @gameHintThinking.
  ///
  /// In en, this message translates to:
  /// **'Thinking…'**
  String get gameHintThinking;

  /// No description provided for @gameHint.
  ///
  /// In en, this message translates to:
  /// **'Hint'**
  String get gameHint;

  /// No description provided for @gamePauseClock.
  ///
  /// In en, this message translates to:
  /// **'Pause clock'**
  String get gamePauseClock;

  /// No description provided for @gameResumeClock.
  ///
  /// In en, this message translates to:
  /// **'Resume clock'**
  String get gameResumeClock;

  /// No description provided for @gameClearHintLeds.
  ///
  /// In en, this message translates to:
  /// **'Clear hint LEDs'**
  String get gameClearHintLeds;

  /// No description provided for @gameNoSnapshotYet.
  ///
  /// In en, this message translates to:
  /// **'No board snapshot yet.'**
  String get gameNoSnapshotYet;

  /// No description provided for @gameFindBoard.
  ///
  /// In en, this message translates to:
  /// **'Find board'**
  String get gameFindBoard;

  /// No description provided for @gameFindBoardSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Connect Wi‑Fi or Bluetooth to load the live position.'**
  String get gameFindBoardSubtitle;

  /// No description provided for @gameControlsTitle.
  ///
  /// In en, this message translates to:
  /// **'Game controls'**
  String get gameControlsTitle;

  /// No description provided for @gameHistory.
  ///
  /// In en, this message translates to:
  /// **'History'**
  String get gameHistory;

  /// No description provided for @gameReturnLive.
  ///
  /// In en, this message translates to:
  /// **'Return to live game'**
  String get gameReturnLive;

  /// No description provided for @gameExitPuzzleCheck.
  ///
  /// In en, this message translates to:
  /// **'Exit puzzle check (keep position)'**
  String get gameExitPuzzleCheck;

  /// No description provided for @gameFindBoardTooltip.
  ///
  /// In en, this message translates to:
  /// **'Find board'**
  String get gameFindBoardTooltip;

  /// No description provided for @gameLayoutTooltip.
  ///
  /// In en, this message translates to:
  /// **'Layout mode'**
  String get gameLayoutTooltip;

  /// No description provided for @gameLayoutStandard.
  ///
  /// In en, this message translates to:
  /// **'Standard'**
  String get gameLayoutStandard;

  /// No description provided for @gameLayoutBoardOnly.
  ///
  /// In en, this message translates to:
  /// **'Board only'**
  String get gameLayoutBoardOnly;

  /// No description provided for @gameCoachRailShow.
  ///
  /// In en, this message translates to:
  /// **'Show AI coach column'**
  String get gameCoachRailShow;

  /// No description provided for @gameCoachRailHide.
  ///
  /// In en, this message translates to:
  /// **'Hide AI coach column'**
  String get gameCoachRailHide;

  /// No description provided for @gameControlsTooltip.
  ///
  /// In en, this message translates to:
  /// **'Game controls'**
  String get gameControlsTooltip;

  /// No description provided for @gameAnalysisTooltip.
  ///
  /// In en, this message translates to:
  /// **'Analysis'**
  String get gameAnalysisTooltip;

  /// No description provided for @gameSummaryTooltip.
  ///
  /// In en, this message translates to:
  /// **'Game summary'**
  String get gameSummaryTooltip;

  /// No description provided for @gameDisconnectTooltip.
  ///
  /// In en, this message translates to:
  /// **'Disconnect session'**
  String get gameDisconnectTooltip;

  /// No description provided for @gameHidePanelTooltip.
  ///
  /// In en, this message translates to:
  /// **'Hide panel'**
  String get gameHidePanelTooltip;

  /// No description provided for @gamePuzzleMovesProgress.
  ///
  /// In en, this message translates to:
  /// **'Moves {played} / {total}'**
  String gamePuzzleMovesProgress(int played, int total);

  /// No description provided for @gameHintTransient.
  ///
  /// In en, this message translates to:
  /// **'Hint: {from}→{to}{evalSuffix}'**
  String gameHintTransient(String from, String to, String evalSuffix);

  /// No description provided for @gameBestMoveSnack.
  ///
  /// In en, this message translates to:
  /// **'Best move: {from}→{to}'**
  String gameBestMoveSnack(String from, String to);

  /// No description provided for @gameHintFailed.
  ///
  /// In en, this message translates to:
  /// **'Hint failed: {error}'**
  String gameHintFailed(String error);

  /// No description provided for @gameDemoBoardSnack.
  ///
  /// In en, this message translates to:
  /// **'Demo board: {feature} only works over Bluetooth or Wi‑Fi to real hardware.'**
  String gameDemoBoardSnack(String feature);

  /// No description provided for @gameClockPauseSent.
  ///
  /// In en, this message translates to:
  /// **'Clock pause sent'**
  String get gameClockPauseSent;

  /// No description provided for @gameClockResumedSnackbar.
  ///
  /// In en, this message translates to:
  /// **'Clock resumed'**
  String get gameClockResumedSnackbar;

  /// No description provided for @gameHintLedsClearedSnackbar.
  ///
  /// In en, this message translates to:
  /// **'Hint LEDs cleared'**
  String get gameHintLedsClearedSnackbar;

  /// No description provided for @gameNoSnapshotBody.
  ///
  /// In en, this message translates to:
  /// **'Tap top-left or below to find a board (Bluetooth). Wi‑Fi and more options are under advanced settings.'**
  String get gameNoSnapshotBody;

  /// No description provided for @gameBackToPanelTooltip.
  ///
  /// In en, this message translates to:
  /// **'Back to layout with panel'**
  String get gameBackToPanelTooltip;

  /// No description provided for @gameAiCoachTitle.
  ///
  /// In en, this message translates to:
  /// **'AI coach'**
  String get gameAiCoachTitle;

  /// No description provided for @gameMoreOptionsTooltip.
  ///
  /// In en, this message translates to:
  /// **'More options'**
  String get gameMoreOptionsTooltip;

  /// No description provided for @statusConnectedTransport.
  ///
  /// In en, this message translates to:
  /// **'Connected ({transport})'**
  String statusConnectedTransport(String transport);

  /// No description provided for @statusConnectingTransport.
  ///
  /// In en, this message translates to:
  /// **'Connecting ({transport})…'**
  String statusConnectingTransport(String transport);

  /// No description provided for @statusBoardNotRespondingTransport.
  ///
  /// In en, this message translates to:
  /// **'Board not responding ({transport})'**
  String statusBoardNotRespondingTransport(String transport);

  /// No description provided for @statusBleDevLine.
  ///
  /// In en, this message translates to:
  /// **'BLE: {name}'**
  String statusBleDevLine(String name);

  /// No description provided for @gameDesktopPlayTitle.
  ///
  /// In en, this message translates to:
  /// **'Play — desktop'**
  String get gameDesktopPlayTitle;

  /// No description provided for @gameDesktopPlaySubtitle.
  ///
  /// In en, this message translates to:
  /// **'Tap the indicator top-left to manage connection (Wi‑Fi, Bluetooth, mock) — same as iOS.'**
  String get gameDesktopPlaySubtitle;

  /// No description provided for @stockfishEvalFailedSnack.
  ///
  /// In en, this message translates to:
  /// **'Evaluation failed: {msg}'**
  String stockfishEvalFailedSnack(String msg);

  /// No description provided for @firmwareDialogTitle.
  ///
  /// In en, this message translates to:
  /// **'Board firmware update'**
  String get firmwareDialogTitle;

  /// No description provided for @firmwareDialogVersions.
  ///
  /// In en, this message translates to:
  /// **'Server has {serverVer}, your board reports {boardVer}.'**
  String firmwareDialogVersions(String serverVer, String boardVer);

  /// No description provided for @firmwareDialogHttpsNote.
  ///
  /// In en, this message translates to:
  /// **'The board downloads over HTTPS (Wi‑Fi STA). The phone only sends the link.'**
  String get firmwareDialogHttpsNote;

  /// No description provided for @firmwareTurnOffReminders.
  ///
  /// In en, this message translates to:
  /// **'Turn off reminders'**
  String get firmwareTurnOffReminders;

  /// No description provided for @firmwareNotNow.
  ///
  /// In en, this message translates to:
  /// **'Not now'**
  String get firmwareNotNow;

  /// No description provided for @firmwareUpdateAction.
  ///
  /// In en, this message translates to:
  /// **'Update'**
  String get firmwareUpdateAction;
}

class _AppLocalizationsDelegate
    extends LocalizationsDelegate<AppLocalizations> {
  const _AppLocalizationsDelegate();

  @override
  Future<AppLocalizations> load(Locale locale) {
    return SynchronousFuture<AppLocalizations>(lookupAppLocalizations(locale));
  }

  @override
  bool isSupported(Locale locale) =>
      <String>['cs', 'en'].contains(locale.languageCode);

  @override
  bool shouldReload(_AppLocalizationsDelegate old) => false;
}

AppLocalizations lookupAppLocalizations(Locale locale) {
  // Lookup logic when only language code is specified.
  switch (locale.languageCode) {
    case 'cs':
      return AppLocalizationsCs();
    case 'en':
      return AppLocalizationsEn();
  }

  throw FlutterError(
      'AppLocalizations.delegate failed to load unsupported locale "$locale". This is likely '
      'an issue with the localizations generation tool. Please file an issue '
      'on GitHub with a reproducible sample app and the gen-l10n configuration '
      'that was used.');
}
