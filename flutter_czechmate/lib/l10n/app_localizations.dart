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
  /// **'OTA URL must start with https:// (internet) or http:// (e.g. phone on board hotspot).'**
  String get errOtaHttps;

  /// No description provided for @errOtaBleTransport.
  ///
  /// In en, this message translates to:
  /// **'Firmware upload over Bluetooth requires an active Bluetooth session (connect via Bluetooth, not Wi‑Fi‑only).'**
  String get errOtaBleTransport;

  /// No description provided for @errOtaBleGatt.
  ///
  /// In en, this message translates to:
  /// **'Bluetooth GATT is not connected — reconnect to the board from Play / Board discovery.'**
  String get errOtaBleGatt;

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
  /// **'Like Strava: copy the summary image and paste it into Instagram, Messages, or WhatsApp — or open the share sheet for anything else.'**
  String get reportShareExportHint;

  /// No description provided for @reportSharePreparing.
  ///
  /// In en, this message translates to:
  /// **'Preparing…'**
  String get reportSharePreparing;

  /// No description provided for @reportShareSummaryImage.
  ///
  /// In en, this message translates to:
  /// **'Share…'**
  String get reportShareSummaryImage;

  /// No description provided for @reportCopySummaryImage.
  ///
  /// In en, this message translates to:
  /// **'Copy image'**
  String get reportCopySummaryImage;

  /// No description provided for @reportCopySummaryBusy.
  ///
  /// In en, this message translates to:
  /// **'Copying…'**
  String get reportCopySummaryBusy;

  /// No description provided for @reportSummaryCopiedSnack.
  ///
  /// In en, this message translates to:
  /// **'Copied — open Instagram or Messages and paste.'**
  String get reportSummaryCopiedSnack;

  /// No description provided for @reportCopyImageFailed.
  ///
  /// In en, this message translates to:
  /// **'Couldn’t copy the image.'**
  String get reportCopyImageFailed;

  /// No description provided for @reportCopyImageWebHint.
  ///
  /// In en, this message translates to:
  /// **'Image clipboard isn’t available in the browser — use Share below.'**
  String get reportCopyImageWebHint;

  /// No description provided for @reportCopyImageDesktopFallback.
  ///
  /// In en, this message translates to:
  /// **'Sharing instead — full image clipboard works best on iPhone & Android.'**
  String get reportCopyImageDesktopFallback;

  /// No description provided for @reportExportSectionOrderTitle.
  ///
  /// In en, this message translates to:
  /// **'Section order (export)'**
  String get reportExportSectionOrderTitle;

  /// No description provided for @reportExportSectionOrderSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Drag to reorder blocks on the shared image — same idea as coach AI priority.'**
  String get reportExportSectionOrderSubtitle;

  /// No description provided for @reportExportBlockRecap.
  ///
  /// In en, this message translates to:
  /// **'Recap caption'**
  String get reportExportBlockRecap;

  /// No description provided for @reportExportBlockBranding.
  ///
  /// In en, this message translates to:
  /// **'Branding'**
  String get reportExportBlockBranding;

  /// No description provided for @reportExportBlockResult.
  ///
  /// In en, this message translates to:
  /// **'Result & reason'**
  String get reportExportBlockResult;

  /// No description provided for @reportExportBlockStats.
  ///
  /// In en, this message translates to:
  /// **'Time & move stats'**
  String get reportExportBlockStats;

  /// No description provided for @reportExportBlockMaterial.
  ///
  /// In en, this message translates to:
  /// **'Material captured'**
  String get reportExportBlockMaterial;

  /// No description provided for @reportExportBlockBoard.
  ///
  /// In en, this message translates to:
  /// **'Board'**
  String get reportExportBlockBoard;

  /// No description provided for @reportExportBlockEval.
  ///
  /// In en, this message translates to:
  /// **'Evaluation chart'**
  String get reportExportBlockEval;

  /// No description provided for @reportExportBlockTiming.
  ///
  /// In en, this message translates to:
  /// **'Timing charts'**
  String get reportExportBlockTiming;

  /// No description provided for @reportSharePgn.
  ///
  /// In en, this message translates to:
  /// **'Share PGN file'**
  String get reportSharePgn;

  /// No description provided for @reportExportAppearanceTitle.
  ///
  /// In en, this message translates to:
  /// **'Export image'**
  String get reportExportAppearanceTitle;

  /// No description provided for @reportExportAppearanceSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Choose layout, background, and which sections appear on the shared PNG. The preview matches the export.'**
  String get reportExportAppearanceSubtitle;

  /// No description provided for @reportExportAspectRatio.
  ///
  /// In en, this message translates to:
  /// **'Canvas size'**
  String get reportExportAspectRatio;

  /// No description provided for @reportExportAspectCard.
  ///
  /// In en, this message translates to:
  /// **'Card (tall)'**
  String get reportExportAspectCard;

  /// No description provided for @reportExportAspectSquare.
  ///
  /// In en, this message translates to:
  /// **'Square 1:1'**
  String get reportExportAspectSquare;

  /// No description provided for @reportExportAspectStory.
  ///
  /// In en, this message translates to:
  /// **'Story 9:16'**
  String get reportExportAspectStory;

  /// No description provided for @reportExportAspectLandscape.
  ///
  /// In en, this message translates to:
  /// **'Landscape 16:9'**
  String get reportExportAspectLandscape;

  /// No description provided for @reportExportTransparentBg.
  ///
  /// In en, this message translates to:
  /// **'Transparent background'**
  String get reportExportTransparentBg;

  /// No description provided for @reportExportTransparentHint.
  ///
  /// In en, this message translates to:
  /// **'Only the outer frame uses alpha; board squares and charts stay on solid panels.'**
  String get reportExportTransparentHint;

  /// No description provided for @reportChartPaletteTitle.
  ///
  /// In en, this message translates to:
  /// **'Chart colors'**
  String get reportChartPaletteTitle;

  /// No description provided for @reportChartPaletteSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Bold presets for eval and timing charts — also used in Analysis.'**
  String get reportChartPaletteSubtitle;

  /// No description provided for @reportChartPaletteTheme.
  ///
  /// In en, this message translates to:
  /// **'Match app'**
  String get reportChartPaletteTheme;

  /// No description provided for @reportChartPaletteNeon.
  ///
  /// In en, this message translates to:
  /// **'Neon'**
  String get reportChartPaletteNeon;

  /// No description provided for @reportChartPaletteSunset.
  ///
  /// In en, this message translates to:
  /// **'Sunset'**
  String get reportChartPaletteSunset;

  /// No description provided for @reportChartPaletteOcean.
  ///
  /// In en, this message translates to:
  /// **'Ocean'**
  String get reportChartPaletteOcean;

  /// No description provided for @reportChartPaletteCustom.
  ///
  /// In en, this message translates to:
  /// **'Custom'**
  String get reportChartPaletteCustom;

  /// No description provided for @reportChartPaletteEditCustom.
  ///
  /// In en, this message translates to:
  /// **'Edit custom colors'**
  String get reportChartPaletteEditCustom;

  /// No description provided for @reportChartPaletteCustomTitle.
  ///
  /// In en, this message translates to:
  /// **'Chart colors'**
  String get reportChartPaletteCustomTitle;

  /// No description provided for @reportChartPaletteEvalLine.
  ///
  /// In en, this message translates to:
  /// **'Evaluation line'**
  String get reportChartPaletteEvalLine;

  /// No description provided for @reportChartPaletteCumulative.
  ///
  /// In en, this message translates to:
  /// **'Elapsed time'**
  String get reportChartPaletteCumulative;

  /// No description provided for @reportChartPaletteBarWhite.
  ///
  /// In en, this message translates to:
  /// **'White’s bars'**
  String get reportChartPaletteBarWhite;

  /// No description provided for @reportChartPaletteBarBlack.
  ///
  /// In en, this message translates to:
  /// **'Black’s bars'**
  String get reportChartPaletteBarBlack;

  /// No description provided for @reportChartPaletteDone.
  ///
  /// In en, this message translates to:
  /// **'Done'**
  String get reportChartPaletteDone;

  /// No description provided for @reportExportShowBranding.
  ///
  /// In en, this message translates to:
  /// **'Show CzechMate header'**
  String get reportExportShowBranding;

  /// No description provided for @reportExportShowStats.
  ///
  /// In en, this message translates to:
  /// **'Show time & move stats'**
  String get reportExportShowStats;

  /// No description provided for @reportExportShowFinalBoard.
  ///
  /// In en, this message translates to:
  /// **'Show final position'**
  String get reportExportShowFinalBoard;

  /// No description provided for @reportExportFlipBoard.
  ///
  /// In en, this message translates to:
  /// **'Flip board (Black at bottom)'**
  String get reportExportFlipBoard;

  /// No description provided for @reportExportShowEvalChart.
  ///
  /// In en, this message translates to:
  /// **'Include evaluation chart'**
  String get reportExportShowEvalChart;

  /// No description provided for @reportExportShowCumulativeChart.
  ///
  /// In en, this message translates to:
  /// **'Include elapsed-time chart'**
  String get reportExportShowCumulativeChart;

  /// No description provided for @reportExportShowPerMoveChart.
  ///
  /// In en, this message translates to:
  /// **'Include time-per-move bars'**
  String get reportExportShowPerMoveChart;

  /// No description provided for @reportExportPresetFull.
  ///
  /// In en, this message translates to:
  /// **'Full'**
  String get reportExportPresetFull;

  /// No description provided for @reportExportPresetMinimal.
  ///
  /// In en, this message translates to:
  /// **'Minimal'**
  String get reportExportPresetMinimal;

  /// No description provided for @reportExportPresetStory.
  ///
  /// In en, this message translates to:
  /// **'Story hero'**
  String get reportExportPresetStory;

  /// No description provided for @reportExportShareGifRecap.
  ///
  /// In en, this message translates to:
  /// **'Share move recap (GIF)'**
  String get reportExportShareGifRecap;

  /// No description provided for @reportExportGifSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Animated recap for Stories or chats (9:16). Long games are sampled.'**
  String get reportExportGifSubtitle;

  /// No description provided for @reportExportGifBuilding.
  ///
  /// In en, this message translates to:
  /// **'Building animation…'**
  String get reportExportGifBuilding;

  /// No description provided for @reportExportGifShareSubject.
  ///
  /// In en, this message translates to:
  /// **'Game recap (GIF)'**
  String get reportExportGifShareSubject;

  /// No description provided for @reportExportGifFailed.
  ///
  /// In en, this message translates to:
  /// **'GIF export failed: {error}'**
  String reportExportGifFailed(String error);

  /// No description provided for @reportExportRecapMove.
  ///
  /// In en, this message translates to:
  /// **'Move {current} / {total}'**
  String reportExportRecapMove(int current, int total);

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

  /// No description provided for @discoveryBluetoothNotReady.
  ///
  /// In en, this message translates to:
  /// **'Bluetooth is not ready. Turn it on in Settings, wait a few seconds, then tap Find board again.'**
  String get discoveryBluetoothNotReady;

  /// No description provided for @discoveryBleScanFailed.
  ///
  /// In en, this message translates to:
  /// **'Bluetooth scan failed: {error}'**
  String discoveryBleScanFailed(String error);

  /// No description provided for @discoveryIntro.
  ///
  /// In en, this message translates to:
  /// **'Tap a board to connect. Bluetooth pairs first; the app then switches to HTTP over Wi‑Fi when it can reach the board (home LAN STA, or the board hotspot while your phone is on the same subnet, usually 192.168.4.x).'**
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
  /// **'e.g. http://192.168.4.1'**
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

  /// No description provided for @discoveryCloseTooltip.
  ///
  /// In en, this message translates to:
  /// **'Close'**
  String get discoveryCloseTooltip;

  /// No description provided for @discoveryConnectionStatusTitle.
  ///
  /// In en, this message translates to:
  /// **'Connection status'**
  String get discoveryConnectionStatusTitle;

  /// No description provided for @discoveryScanningBoards.
  ///
  /// In en, this message translates to:
  /// **'Scanning for boards…'**
  String get discoveryScanningBoards;

  /// No description provided for @discoveryBleDevicesDev.
  ///
  /// In en, this message translates to:
  /// **'Bluetooth ({guid})'**
  String discoveryBleDevicesDev(String guid);

  /// No description provided for @discoveryFoundBoards.
  ///
  /// In en, this message translates to:
  /// **'Found boards'**
  String get discoveryFoundBoards;

  /// No description provided for @discoveryEmptyBleList.
  ///
  /// In en, this message translates to:
  /// **'The list is empty. Tap “Find board” and wait a few seconds within range of a powered-on board.'**
  String get discoveryEmptyBleList;

  /// No description provided for @discoveryAdvancedSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi URL, connection mode, demo'**
  String get discoveryAdvancedSubtitle;

  /// No description provided for @discoveryConnectionModeHeading.
  ///
  /// In en, this message translates to:
  /// **'Connection mode'**
  String get discoveryConnectionModeHeading;

  /// No description provided for @discoveryBleSegmentShort.
  ///
  /// In en, this message translates to:
  /// **'BLE only'**
  String get discoveryBleSegmentShort;

  /// No description provided for @discoveryReconnectSavedBoard.
  ///
  /// In en, this message translates to:
  /// **'Reconnect saved board'**
  String get discoveryReconnectSavedBoard;

  /// No description provided for @discoveryManualAdvanced.
  ///
  /// In en, this message translates to:
  /// **'Manual entry / advanced'**
  String get discoveryManualAdvanced;

  /// No description provided for @discoveryWifiUrlLabel.
  ///
  /// In en, this message translates to:
  /// **'Board address (Wi‑Fi)'**
  String get discoveryWifiUrlLabel;

  /// No description provided for @discoveryEnterBoardUrlSnack.
  ///
  /// In en, this message translates to:
  /// **'Enter the board URL or IP (e.g. http://192.168.4.1).'**
  String get discoveryEnterBoardUrlSnack;

  /// No description provided for @discoveryBoardHotspotButton.
  ///
  /// In en, this message translates to:
  /// **'Board hotspot (192.168.4.1)'**
  String get discoveryBoardHotspotButton;

  /// No description provided for @connectionModeAutoShort.
  ///
  /// In en, this message translates to:
  /// **'Auto'**
  String get connectionModeAutoShort;

  /// No description provided for @settingsOverviewTitle.
  ///
  /// In en, this message translates to:
  /// **'Overview'**
  String get settingsOverviewTitle;

  /// No description provided for @settingsOverviewSubtitleError.
  ///
  /// In en, this message translates to:
  /// **'Last error — expand for details'**
  String get settingsOverviewSubtitleError;

  /// No description provided for @settingsOverviewSubtitleOk.
  ///
  /// In en, this message translates to:
  /// **'Shortcuts and connection status'**
  String get settingsOverviewSubtitleOk;

  /// No description provided for @settingsOverviewBody.
  ///
  /// In en, this message translates to:
  /// **'Game and board options apply to the Play tab and the Game controls panel — one set of toggles.'**
  String get settingsOverviewBody;

  /// No description provided for @settingsGoToPlayTab.
  ///
  /// In en, this message translates to:
  /// **'Go to Play tab'**
  String get settingsGoToPlayTab;

  /// No description provided for @settingsConnectionTitle.
  ///
  /// In en, this message translates to:
  /// **'Board connection'**
  String get settingsConnectionTitle;

  /// No description provided for @settingsConnectionSubtitle.
  ///
  /// In en, this message translates to:
  /// **'{tier} · {transport}'**
  String settingsConnectionSubtitle(String tier, String transport);

  /// No description provided for @settingsConnectionIntro.
  ///
  /// In en, this message translates to:
  /// **'Usually it’s enough to find the board via Bluetooth; the app may switch to Wi‑Fi when that makes sense.'**
  String get settingsConnectionIntro;

  /// No description provided for @settingsDisconnect.
  ///
  /// In en, this message translates to:
  /// **'Disconnect'**
  String get settingsDisconnect;

  /// No description provided for @settingsAdvanced.
  ///
  /// In en, this message translates to:
  /// **'Advanced'**
  String get settingsAdvanced;

  /// No description provided for @settingsAdvancedConnectionSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Default URL, connection mode, saved BLE'**
  String get settingsAdvancedConnectionSubtitle;

  /// No description provided for @settingsReconnectingBle.
  ///
  /// In en, this message translates to:
  /// **'Reconnecting Bluetooth to saved board…'**
  String get settingsReconnectingBle;

  /// No description provided for @settingsReconnectSavedBleShort.
  ///
  /// In en, this message translates to:
  /// **'Reconnect saved board (BLE)'**
  String get settingsReconnectSavedBleShort;

  /// No description provided for @settingsManualEntry.
  ///
  /// In en, this message translates to:
  /// **'Manual entry'**
  String get settingsManualEntry;

  /// No description provided for @settingsSavedDefaultsTitle.
  ///
  /// In en, this message translates to:
  /// **'Saved defaults'**
  String get settingsSavedDefaultsTitle;

  /// No description provided for @settingsSavedDefaultsBody.
  ///
  /// In en, this message translates to:
  /// **'Used on next connection from “Find board” or after reconnecting.'**
  String get settingsSavedDefaultsBody;

  /// No description provided for @settingsConnectionModeLabel.
  ///
  /// In en, this message translates to:
  /// **'Connection mode'**
  String get settingsConnectionModeLabel;

  /// No description provided for @settingsConnectionModeAutoBleWifi.
  ///
  /// In en, this message translates to:
  /// **'Auto (BLE → Wi‑Fi)'**
  String get settingsConnectionModeAutoBleWifi;

  /// No description provided for @settingsDefaultBoardUrl.
  ///
  /// In en, this message translates to:
  /// **'Default board URL'**
  String get settingsDefaultBoardUrl;

  /// No description provided for @settingsInvalidUrlSnack.
  ///
  /// In en, this message translates to:
  /// **'Invalid URL — enter a host (e.g. 192.168.4.1 or http://…)'**
  String get settingsInvalidUrlSnack;

  /// No description provided for @settingsSavedSnack.
  ///
  /// In en, this message translates to:
  /// **'Saved'**
  String get settingsSavedSnack;

  /// No description provided for @settingsSaveUrl.
  ///
  /// In en, this message translates to:
  /// **'Save URL'**
  String get settingsSaveUrl;

  /// No description provided for @settingsBleOnlyTitle.
  ///
  /// In en, this message translates to:
  /// **'Bluetooth only'**
  String get settingsBleOnlyTitle;

  /// No description provided for @settingsBleOnlySubtitle.
  ///
  /// In en, this message translates to:
  /// **'Don’t switch to Wi‑Fi after BLE'**
  String get settingsBleOnlySubtitle;

  /// No description provided for @settingsWebSocketTitle.
  ///
  /// In en, this message translates to:
  /// **'WebSocket snapshot'**
  String get settingsWebSocketTitle;

  /// No description provided for @settingsWebSocketSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Reconnect Wi‑Fi session after change'**
  String get settingsWebSocketSubtitle;

  /// No description provided for @transportShortDemo.
  ///
  /// In en, this message translates to:
  /// **'Demo'**
  String get transportShortDemo;

  /// No description provided for @transportShortDash.
  ///
  /// In en, this message translates to:
  /// **'—'**
  String get transportShortDash;

  /// No description provided for @settingsBoardAppearanceTitle.
  ///
  /// In en, this message translates to:
  /// **'Board appearance'**
  String get settingsBoardAppearanceTitle;

  /// No description provided for @settingsBoardAppearanceSubtitle.
  ///
  /// In en, this message translates to:
  /// **'{layout} · {zoom} · {style}'**
  String settingsBoardAppearanceSubtitle(
      String layout, String zoom, String style);

  /// No description provided for @settingsLayoutBoardOnlyShort.
  ///
  /// In en, this message translates to:
  /// **'Board only'**
  String get settingsLayoutBoardOnlyShort;

  /// No description provided for @settingsLayoutFullUiShort.
  ///
  /// In en, this message translates to:
  /// **'Full UI'**
  String get settingsLayoutFullUiShort;

  /// No description provided for @settingsPlayTabGamePanel.
  ///
  /// In en, this message translates to:
  /// **'Play tab & game panel'**
  String get settingsPlayTabGamePanel;

  /// No description provided for @settingsLayoutLabel.
  ///
  /// In en, this message translates to:
  /// **'Layout'**
  String get settingsLayoutLabel;

  /// No description provided for @settingsLayoutBoardSegment.
  ///
  /// In en, this message translates to:
  /// **'Board'**
  String get settingsLayoutBoardSegment;

  /// No description provided for @settingsLayoutBoardTooltip.
  ///
  /// In en, this message translates to:
  /// **'Board only — minimal chrome'**
  String get settingsLayoutBoardTooltip;

  /// No description provided for @settingsLayoutFullSegment.
  ///
  /// In en, this message translates to:
  /// **'Full'**
  String get settingsLayoutFullSegment;

  /// No description provided for @settingsLayoutFullTooltip.
  ///
  /// In en, this message translates to:
  /// **'Standard — clocks & controls'**
  String get settingsLayoutFullTooltip;

  /// No description provided for @settingsBoardZoom.
  ///
  /// In en, this message translates to:
  /// **'Board zoom'**
  String get settingsBoardZoom;

  /// No description provided for @settingsSquareColors.
  ///
  /// In en, this message translates to:
  /// **'Square colors'**
  String get settingsSquareColors;

  /// No description provided for @settingsBoardThemeLabel.
  ///
  /// In en, this message translates to:
  /// **'Theme'**
  String get settingsBoardThemeLabel;

  /// No description provided for @settingsBoardOptions.
  ///
  /// In en, this message translates to:
  /// **'Board options'**
  String get settingsBoardOptions;

  /// No description provided for @settingsCoordinatesTitle.
  ///
  /// In en, this message translates to:
  /// **'Coordinates'**
  String get settingsCoordinatesTitle;

  /// No description provided for @settingsCoordinatesSubtitle.
  ///
  /// In en, this message translates to:
  /// **'a–h and 1–8 labels'**
  String get settingsCoordinatesSubtitle;

  /// No description provided for @settingsPieceMotionTitle.
  ///
  /// In en, this message translates to:
  /// **'Piece motion'**
  String get settingsPieceMotionTitle;

  /// No description provided for @settingsPieceMotionSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Animated moves on the board'**
  String get settingsPieceMotionSubtitle;

  /// No description provided for @settingsFlipBoardTitle.
  ///
  /// In en, this message translates to:
  /// **'Flip board'**
  String get settingsFlipBoardTitle;

  /// No description provided for @settingsFlipBoardSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Black toward you'**
  String get settingsFlipBoardSubtitle;

  /// No description provided for @settingsRemoteMovesTitle.
  ///
  /// In en, this message translates to:
  /// **'Remote moves'**
  String get settingsRemoteMovesTitle;

  /// No description provided for @settingsRemoteMovesSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Play from the app, not only the board'**
  String get settingsRemoteMovesSubtitle;

  /// No description provided for @settingsLiveEvalTitle.
  ///
  /// In en, this message translates to:
  /// **'Live evaluation'**
  String get settingsLiveEvalTitle;

  /// No description provided for @settingsLiveEvalSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Stockfish — fills the Analysis chart while you play'**
  String get settingsLiveEvalSubtitle;

  /// No description provided for @settingsResetBoardDisplay.
  ///
  /// In en, this message translates to:
  /// **'Reset board display defaults'**
  String get settingsResetBoardDisplay;

  /// No description provided for @boardStyleWooden.
  ///
  /// In en, this message translates to:
  /// **'Wooden (default)'**
  String get boardStyleWooden;

  /// No description provided for @boardStyleModernDark.
  ///
  /// In en, this message translates to:
  /// **'Modern Dark'**
  String get boardStyleModernDark;

  /// No description provided for @boardStyleIceBlue.
  ///
  /// In en, this message translates to:
  /// **'Ice Blue'**
  String get boardStyleIceBlue;

  /// No description provided for @boardStyleForestGreen.
  ///
  /// In en, this message translates to:
  /// **'Forest Green'**
  String get boardStyleForestGreen;

  /// No description provided for @boardStyleMarbleGray.
  ///
  /// In en, this message translates to:
  /// **'Marble Gray'**
  String get boardStyleMarbleGray;

  /// No description provided for @boardStyleMidnight.
  ///
  /// In en, this message translates to:
  /// **'Midnight'**
  String get boardStyleMidnight;

  /// No description provided for @boardStyleSlate.
  ///
  /// In en, this message translates to:
  /// **'Slate'**
  String get boardStyleSlate;

  /// No description provided for @boardStyleCoral.
  ///
  /// In en, this message translates to:
  /// **'Coral'**
  String get boardStyleCoral;

  /// No description provided for @settingsAppAppearanceTitle.
  ///
  /// In en, this message translates to:
  /// **'App appearance'**
  String get settingsAppAppearanceTitle;

  /// No description provided for @settingsAppearanceLight.
  ///
  /// In en, this message translates to:
  /// **'Light look'**
  String get settingsAppearanceLight;

  /// No description provided for @settingsAppearanceDark.
  ///
  /// In en, this message translates to:
  /// **'Dark look'**
  String get settingsAppearanceDark;

  /// No description provided for @settingsAppearanceOled.
  ///
  /// In en, this message translates to:
  /// **'OLED'**
  String get settingsAppearanceOled;

  /// No description provided for @settingsAppearanceSystem.
  ///
  /// In en, this message translates to:
  /// **'Follow system'**
  String get settingsAppearanceSystem;

  /// No description provided for @settingsColorSchemeLabel.
  ///
  /// In en, this message translates to:
  /// **'Color scheme'**
  String get settingsColorSchemeLabel;

  /// No description provided for @settingsColorSchemeHelper.
  ///
  /// In en, this message translates to:
  /// **'Light / Dark apply immediately. System follows macOS appearance.'**
  String get settingsColorSchemeHelper;

  /// No description provided for @settingsThemeSystem.
  ///
  /// In en, this message translates to:
  /// **'System'**
  String get settingsThemeSystem;

  /// No description provided for @settingsThemeLight.
  ///
  /// In en, this message translates to:
  /// **'Light'**
  String get settingsThemeLight;

  /// No description provided for @settingsThemeDark.
  ///
  /// In en, this message translates to:
  /// **'Dark'**
  String get settingsThemeDark;

  /// No description provided for @settingsThemeOledBlack.
  ///
  /// In en, this message translates to:
  /// **'OLED (true black)'**
  String get settingsThemeOledBlack;

  /// No description provided for @settingsHapticsTitle.
  ///
  /// In en, this message translates to:
  /// **'Haptic feedback'**
  String get settingsHapticsTitle;

  /// No description provided for @settingsSoundTitle.
  ///
  /// In en, this message translates to:
  /// **'Sound effects'**
  String get settingsSoundTitle;

  /// No description provided for @settingsAutoGameSummaryTitle.
  ///
  /// In en, this message translates to:
  /// **'Auto-open game summary after game'**
  String get settingsAutoGameSummaryTitle;

  /// No description provided for @settingsCoachAiTitle.
  ///
  /// In en, this message translates to:
  /// **'Coach & AI'**
  String get settingsCoachAiTitle;

  /// No description provided for @settingsCoachPriorityIntro.
  ///
  /// In en, this message translates to:
  /// **'Drag to set fallback order: the app tries the first provider, then the next if it fails. Leave the list empty for offline tips only. Keys stay on this device.'**
  String get settingsCoachPriorityIntro;

  /// No description provided for @settingsCoachOfflineOnly.
  ///
  /// In en, this message translates to:
  /// **'Offline only'**
  String get settingsCoachOfflineOnly;

  /// No description provided for @settingsCoachOpenAiOnly.
  ///
  /// In en, this message translates to:
  /// **'OpenAI only'**
  String get settingsCoachOpenAiOnly;

  /// No description provided for @settingsCoachGoogleOnly.
  ///
  /// In en, this message translates to:
  /// **'Google only'**
  String get settingsCoachGoogleOnly;

  /// No description provided for @settingsCoachGroqGoogleOpenAi.
  ///
  /// In en, this message translates to:
  /// **'Groq→Google→OpenAI'**
  String get settingsCoachGroqGoogleOpenAi;

  /// No description provided for @settingsCoachOllamaGoogle.
  ///
  /// In en, this message translates to:
  /// **'Ollama→Google'**
  String get settingsCoachOllamaGoogle;

  /// No description provided for @settingsCoachPriorityTopLabel.
  ///
  /// In en, this message translates to:
  /// **'Priority (top = first try)'**
  String get settingsCoachPriorityTopLabel;

  /// No description provided for @settingsCoachNoCloudProviders.
  ///
  /// In en, this message translates to:
  /// **'No cloud providers — Coach uses short on-device tips.'**
  String get settingsCoachNoCloudProviders;

  /// No description provided for @settingsCoachRemoveFromChain.
  ///
  /// In en, this message translates to:
  /// **'Remove from chain'**
  String get settingsCoachRemoveFromChain;

  /// No description provided for @settingsCoachAddProvider.
  ///
  /// In en, this message translates to:
  /// **'Add provider'**
  String get settingsCoachAddProvider;

  /// No description provided for @settingsCoachExplanationLevel.
  ///
  /// In en, this message translates to:
  /// **'Coach explanation level'**
  String get settingsCoachExplanationLevel;

  /// No description provided for @settingsCoachExplanationLevelSubtitle.
  ///
  /// In en, this message translates to:
  /// **'1 = Beginner, 4 = Expert'**
  String get settingsCoachExplanationLevelSubtitle;

  /// No description provided for @settingsCoachLevelBeginner.
  ///
  /// In en, this message translates to:
  /// **'1 – Beginner'**
  String get settingsCoachLevelBeginner;

  /// No description provided for @settingsCoachLevelIntermediate.
  ///
  /// In en, this message translates to:
  /// **'2 – Intermediate'**
  String get settingsCoachLevelIntermediate;

  /// No description provided for @settingsCoachLevelAdvanced.
  ///
  /// In en, this message translates to:
  /// **'3 – Advanced'**
  String get settingsCoachLevelAdvanced;

  /// No description provided for @settingsCoachLevelExpert.
  ///
  /// In en, this message translates to:
  /// **'4 – Expert'**
  String get settingsCoachLevelExpert;

  /// No description provided for @settingsCoachCredentialsHeader.
  ///
  /// In en, this message translates to:
  /// **'Credentials (fill what you use)'**
  String get settingsCoachCredentialsHeader;

  /// No description provided for @settingsCoachOpenAiProvider.
  ///
  /// In en, this message translates to:
  /// **'OpenAI'**
  String get settingsCoachOpenAiProvider;

  /// No description provided for @settingsCoachApiKeyLabel.
  ///
  /// In en, this message translates to:
  /// **'API key'**
  String get settingsCoachApiKeyLabel;

  /// No description provided for @settingsCoachOpenAiKeyHelper.
  ///
  /// In en, this message translates to:
  /// **'platform.openai.com — used only for Coach.'**
  String get settingsCoachOpenAiKeyHelper;

  /// No description provided for @settingsCoachModelIdLabel.
  ///
  /// In en, this message translates to:
  /// **'Model id'**
  String get settingsCoachModelIdLabel;

  /// No description provided for @settingsCoachOpenAiKeysButton.
  ///
  /// In en, this message translates to:
  /// **'OpenAI keys'**
  String get settingsCoachOpenAiKeysButton;

  /// No description provided for @settingsCoachGoogleAiStudio.
  ///
  /// In en, this message translates to:
  /// **'Google AI Studio'**
  String get settingsCoachGoogleAiStudio;

  /// No description provided for @settingsCoachPasteGoogleKeyHint.
  ///
  /// In en, this message translates to:
  /// **'Paste Google AI Studio key'**
  String get settingsCoachPasteGoogleKeyHint;

  /// No description provided for @settingsCoachGoogleKeyHelper.
  ///
  /// In en, this message translates to:
  /// **'Get a key at aistudio.google.com — Gemini / Gemma.'**
  String get settingsCoachGoogleKeyHelper;

  /// No description provided for @settingsCoachCustomModel.
  ///
  /// In en, this message translates to:
  /// **'Custom…'**
  String get settingsCoachCustomModel;

  /// No description provided for @settingsCoachCustomModelId.
  ///
  /// In en, this message translates to:
  /// **'Custom model id'**
  String get settingsCoachCustomModelId;

  /// No description provided for @settingsCoachCustomModelHint.
  ///
  /// In en, this message translates to:
  /// **'e.g. gemini-2.5-flash'**
  String get settingsCoachCustomModelHint;

  /// No description provided for @settingsCoachOpenAiKeyHint.
  ///
  /// In en, this message translates to:
  /// **'sk-…'**
  String get settingsCoachOpenAiKeyHint;

  /// No description provided for @settingsCoachOpenAiModelHint.
  ///
  /// In en, this message translates to:
  /// **'gpt-4o-mini'**
  String get settingsCoachOpenAiModelHint;

  /// No description provided for @settingsCoachGroqKeyHint.
  ///
  /// In en, this message translates to:
  /// **'Paste Groq API key'**
  String get settingsCoachGroqKeyHint;

  /// No description provided for @settingsCoachGroqModelHint.
  ///
  /// In en, this message translates to:
  /// **'llama-3.3-70b-versatile'**
  String get settingsCoachGroqModelHint;

  /// No description provided for @settingsCoachGroqModelHelper.
  ///
  /// In en, this message translates to:
  /// **'Must match an enabled Groq model.'**
  String get settingsCoachGroqModelHelper;

  /// No description provided for @settingsCoachXaiKeyHint.
  ///
  /// In en, this message translates to:
  /// **'Paste xAI API key'**
  String get settingsCoachXaiKeyHint;

  /// No description provided for @settingsCoachXaiModelHint.
  ///
  /// In en, this message translates to:
  /// **'grok-2-latest'**
  String get settingsCoachXaiModelHint;

  /// No description provided for @settingsCoachOllamaBaseHint.
  ///
  /// In en, this message translates to:
  /// **'http://127.0.0.1:11434/v1'**
  String get settingsCoachOllamaBaseHint;

  /// No description provided for @settingsCoachOllamaModelHint.
  ///
  /// In en, this message translates to:
  /// **'llama3.2'**
  String get settingsCoachOllamaModelHint;

  /// No description provided for @settingsCoachGetGoogleKey.
  ///
  /// In en, this message translates to:
  /// **'Get Google AI key'**
  String get settingsCoachGetGoogleKey;

  /// No description provided for @settingsCoachGroqTitle.
  ///
  /// In en, this message translates to:
  /// **'Groq (OpenAI-compatible)'**
  String get settingsCoachGroqTitle;

  /// No description provided for @settingsCoachXaiTitle.
  ///
  /// In en, this message translates to:
  /// **'xAI (Grok)'**
  String get settingsCoachXaiTitle;

  /// No description provided for @settingsCoachOllamaTitle.
  ///
  /// In en, this message translates to:
  /// **'Ollama (local)'**
  String get settingsCoachOllamaTitle;

  /// No description provided for @settingsCoachBaseUrlLabel.
  ///
  /// In en, this message translates to:
  /// **'Base URL'**
  String get settingsCoachBaseUrlLabel;

  /// No description provided for @settingsCoachChainSubtitle.
  ///
  /// In en, this message translates to:
  /// **'{chain}'**
  String settingsCoachChainSubtitle(String chain);

  /// No description provided for @settingsDeveloperModeUnlockedSnack.
  ///
  /// In en, this message translates to:
  /// **'Developer mode unlocked.'**
  String get settingsDeveloperModeUnlockedSnack;

  /// No description provided for @gameControlDisplaySection.
  ///
  /// In en, this message translates to:
  /// **'Display'**
  String get gameControlDisplaySection;

  /// No description provided for @gameControlPlayModeSection.
  ///
  /// In en, this message translates to:
  /// **'Play mode'**
  String get gameControlPlayModeSection;

  /// No description provided for @gameControlSandboxLabel.
  ///
  /// In en, this message translates to:
  /// **'Sandbox'**
  String get gameControlSandboxLabel;

  /// No description provided for @gameControlMovesFromApp.
  ///
  /// In en, this message translates to:
  /// **'Moves from app'**
  String get gameControlMovesFromApp;

  /// No description provided for @gameControlUndoMove.
  ///
  /// In en, this message translates to:
  /// **'Undo move ({n})'**
  String gameControlUndoMove(int n);

  /// No description provided for @gameControlExitSandbox.
  ///
  /// In en, this message translates to:
  /// **'Exit sandbox & refresh board'**
  String get gameControlExitSandbox;

  /// No description provided for @gameControlLearningModeTitle.
  ///
  /// In en, this message translates to:
  /// **'Learning mode'**
  String get gameControlLearningModeTitle;

  /// No description provided for @gameControlLearningModeSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Coach explanations in the app'**
  String get gameControlLearningModeSubtitle;

  /// No description provided for @gameControlExplanationLevelSettings.
  ///
  /// In en, this message translates to:
  /// **'Explanation level (1–4) in Settings'**
  String get gameControlExplanationLevelSettings;

  /// No description provided for @gameControlActionsSection.
  ///
  /// In en, this message translates to:
  /// **'Actions'**
  String get gameControlActionsSection;

  /// No description provided for @gameControlNewGameEllipsis.
  ///
  /// In en, this message translates to:
  /// **'New game…'**
  String get gameControlNewGameEllipsis;

  /// No description provided for @gameControlRefreshState.
  ///
  /// In en, this message translates to:
  /// **'Refresh state'**
  String get gameControlRefreshState;

  /// No description provided for @gameControlPanelTitle.
  ///
  /// In en, this message translates to:
  /// **'Game controls'**
  String get gameControlPanelTitle;

  /// No description provided for @timerUnavailable.
  ///
  /// In en, this message translates to:
  /// **'Clock: off or unavailable'**
  String get timerUnavailable;

  /// No description provided for @timerWhiteShort.
  ///
  /// In en, this message translates to:
  /// **'White'**
  String get timerWhiteShort;

  /// No description provided for @timerBlackShort.
  ///
  /// In en, this message translates to:
  /// **'Black'**
  String get timerBlackShort;

  /// No description provided for @coachChatTypeFirst.
  ///
  /// In en, this message translates to:
  /// **'Write a question first, then Send.'**
  String get coachChatTypeFirst;

  /// No description provided for @coachChatHide.
  ///
  /// In en, this message translates to:
  /// **'Hide'**
  String get coachChatHide;

  /// No description provided for @coachChatDismiss.
  ///
  /// In en, this message translates to:
  /// **'DISMISS'**
  String get coachChatDismiss;

  /// No description provided for @manualConnTitle.
  ///
  /// In en, this message translates to:
  /// **'Manual connection'**
  String get manualConnTitle;

  /// No description provided for @manualConnWifiStatusError.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi status: {error}'**
  String manualConnWifiStatusError(String error);

  /// No description provided for @manualConnEnterUrlSnack.
  ///
  /// In en, this message translates to:
  /// **'Enter the board URL.'**
  String get manualConnEnterUrlSnack;

  /// No description provided for @manualConnStaDisconnectedSnack.
  ///
  /// In en, this message translates to:
  /// **'STA disconnected (command sent).'**
  String get manualConnStaDisconnectedSnack;

  /// No description provided for @manualConnClearWifiTitle.
  ///
  /// In en, this message translates to:
  /// **'Clear saved Wi‑Fi from NVS?'**
  String get manualConnClearWifiTitle;

  /// No description provided for @manualConnClearWifiBody.
  ///
  /// In en, this message translates to:
  /// **'The ESP will forget the stored network.'**
  String get manualConnClearWifiBody;

  /// No description provided for @manualConnNvsClearedSnack.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi credentials cleared from NVS.'**
  String get manualConnNvsClearedSnack;

  /// No description provided for @manualConnSaveUrlSnack.
  ///
  /// In en, this message translates to:
  /// **'URL saved to preferences.'**
  String get manualConnSaveUrlSnack;

  /// No description provided for @manualConnSaveUrl.
  ///
  /// In en, this message translates to:
  /// **'Save URL'**
  String get manualConnSaveUrl;

  /// No description provided for @manualConnTestConnection.
  ///
  /// In en, this message translates to:
  /// **'Test connection (GET snapshot)'**
  String get manualConnTestConnection;

  /// No description provided for @manualConnConnectSession.
  ///
  /// In en, this message translates to:
  /// **'Connect session (Wi‑Fi + poll)'**
  String get manualConnConnectSession;

  /// No description provided for @manualConnWifiStaNvs.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi STA & NVS'**
  String get manualConnWifiStaNvs;

  /// No description provided for @manualConnRefreshWifi.
  ///
  /// In en, this message translates to:
  /// **'Refresh Wi‑Fi status'**
  String get manualConnRefreshWifi;

  /// No description provided for @manualConnDisconnectSta.
  ///
  /// In en, this message translates to:
  /// **'Disconnect ESP from STA'**
  String get manualConnDisconnectSta;

  /// No description provided for @manualConnClearWifiNvs.
  ///
  /// In en, this message translates to:
  /// **'Clear Wi‑Fi from NVS'**
  String get manualConnClearWifiNvs;

  /// No description provided for @manualConnClearConfirmAction.
  ///
  /// In en, this message translates to:
  /// **'Clear'**
  String get manualConnClearConfirmAction;

  /// No description provided for @manualConnUrlLabelDev.
  ///
  /// In en, this message translates to:
  /// **'Board base URL (http://…)'**
  String get manualConnUrlLabelDev;

  /// No description provided for @manualConnUrlLabelUser.
  ///
  /// In en, this message translates to:
  /// **'Board address'**
  String get manualConnUrlLabelUser;

  /// No description provided for @manualConnUrlHintDev.
  ///
  /// In en, this message translates to:
  /// **'http://192.168.x.x'**
  String get manualConnUrlHintDev;

  /// No description provided for @manualConnUrlHintUser.
  ///
  /// In en, this message translates to:
  /// **'Address on your LAN'**
  String get manualConnUrlHintUser;

  /// No description provided for @manualConnBleInfoTile.
  ///
  /// In en, this message translates to:
  /// **'You’re connected via Bluetooth. Enter the board address for network actions.'**
  String get manualConnBleInfoTile;

  /// No description provided for @manualConnStaSectionDev.
  ///
  /// In en, this message translates to:
  /// **'Needs HTTP reachability to the board (same LAN or board AP). BLE-only without IP won’t call these endpoints.'**
  String get manualConnStaSectionDev;

  /// No description provided for @manualConnStaSectionUser.
  ///
  /// In en, this message translates to:
  /// **'Use this section to manage the board’s network connection manually.'**
  String get manualConnStaSectionUser;

  /// No description provided for @manualConnTestFailedUser.
  ///
  /// In en, this message translates to:
  /// **'Could not reach the board.'**
  String get manualConnTestFailedUser;

  /// No description provided for @haMqttTitle.
  ///
  /// In en, this message translates to:
  /// **'Home Assistant & MQTT'**
  String get haMqttTitle;

  /// No description provided for @haMqttSavedSnack.
  ///
  /// In en, this message translates to:
  /// **'MQTT saved to board.'**
  String get haMqttSavedSnack;

  /// No description provided for @haMqttHowToHa.
  ///
  /// In en, this message translates to:
  /// **'Home Assistant guide'**
  String get haMqttHowToHa;

  /// No description provided for @haMqttBrokerHeader.
  ///
  /// In en, this message translates to:
  /// **'MQTT broker'**
  String get haMqttBrokerHeader;

  /// No description provided for @haMqttSaveToBoard.
  ///
  /// In en, this message translates to:
  /// **'Save to board'**
  String get haMqttSaveToBoard;

  /// No description provided for @haMqttRefreshFromBoard.
  ///
  /// In en, this message translates to:
  /// **'Refresh status from board'**
  String get haMqttRefreshFromBoard;

  /// No description provided for @haMqttBoardStatusHeader.
  ///
  /// In en, this message translates to:
  /// **'Status on board'**
  String get haMqttBoardStatusHeader;

  /// No description provided for @haMqttModeTile.
  ///
  /// In en, this message translates to:
  /// **'Mode'**
  String get haMqttModeTile;

  /// No description provided for @haMqttMqttTile.
  ///
  /// In en, this message translates to:
  /// **'MQTT'**
  String get haMqttMqttTile;

  /// No description provided for @haMqttWifiStaTile.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi STA'**
  String get haMqttWifiStaTile;

  /// No description provided for @haMqttGuideBody.
  ///
  /// In en, this message translates to:
  /// **'1. Home Assistant must run on the same Wi‑Fi LAN as the board (ESP connected as STA to your router).\n\n2. In Home Assistant enable an MQTT broker — commonly the “Mosquitto broker” add-on (Settings → Add-ons). Note the port (usually 1883) and any username/password from the add-on config.\n\n3. Find the IP or hostname of the machine running HA (e.g. 192.168.1.42 or homeassistant.local). The board must reach it on the LAN.\n\n4. Enter that address as “Broker host” below — it’s the MQTT server address (usually the same machine as Home Assistant). Keep port 1883 unless you changed Mosquitto.\n\n5. Saving sends settings to the board over the current link (Wi‑Fi or Bluetooth). The CzechMate app does not connect to the broker — only the ESP does.'**
  String get haMqttGuideBody;

  /// No description provided for @haMqttHostFieldLabel.
  ///
  /// In en, this message translates to:
  /// **'Broker host (e.g. Home Assistant IP)'**
  String get haMqttHostFieldLabel;

  /// No description provided for @haMqttPortFieldLabel.
  ///
  /// In en, this message translates to:
  /// **'Port'**
  String get haMqttPortFieldLabel;

  /// No description provided for @haMqttUserFieldLabel.
  ///
  /// In en, this message translates to:
  /// **'Username (optional)'**
  String get haMqttUserFieldLabel;

  /// No description provided for @haMqttPasswordFieldLabel.
  ///
  /// In en, this message translates to:
  /// **'Password (optional)'**
  String get haMqttPasswordFieldLabel;

  /// No description provided for @haMqttFooterMock.
  ///
  /// In en, this message translates to:
  /// **'The demo board does not support MQTT — connect a real board.'**
  String get haMqttFooterMock;

  /// No description provided for @haMqttFooterConnectFirst.
  ///
  /// In en, this message translates to:
  /// **'Connect the board (Bluetooth or Wi‑Fi) before saving the broker.'**
  String get haMqttFooterConnectFirst;

  /// No description provided for @haMqttFooterBleSave.
  ///
  /// In en, this message translates to:
  /// **'You can save over Bluetooth; refreshing status from the board uses HTTP over Wi‑Fi.'**
  String get haMqttFooterBleSave;

  /// No description provided for @haMqttFooterTroubleshoot.
  ///
  /// In en, this message translates to:
  /// **'If MQTT stays offline, check the HA firewall, Mosquitto credentials, and that the ESP is on the same network as the broker.'**
  String get haMqttFooterTroubleshoot;

  /// No description provided for @haMqttStateConnected.
  ///
  /// In en, this message translates to:
  /// **'connected'**
  String get haMqttStateConnected;

  /// No description provided for @haMqttStateDisconnected.
  ///
  /// In en, this message translates to:
  /// **'disconnected'**
  String get haMqttStateDisconnected;

  /// No description provided for @haMqttWifiOk.
  ///
  /// In en, this message translates to:
  /// **'OK'**
  String get haMqttWifiOk;

  /// No description provided for @haMqttWifiNoLink.
  ///
  /// In en, this message translates to:
  /// **'no link'**
  String get haMqttWifiNoLink;

  /// No description provided for @firmwareDailySecondConfirmBody.
  ///
  /// In en, this message translates to:
  /// **'Start the update? The board will download firmware, write flash, and reboot. Do not interrupt power.'**
  String get firmwareDailySecondConfirmBody;

  /// No description provided for @firmwareDailySecondConfirmTitle.
  ///
  /// In en, this message translates to:
  /// **'Confirm update'**
  String get firmwareDailySecondConfirmTitle;

  /// No description provided for @profileTitle.
  ///
  /// In en, this message translates to:
  /// **'Profile & Elo'**
  String get profileTitle;

  /// No description provided for @profileDisplayName.
  ///
  /// In en, this message translates to:
  /// **'Display name'**
  String get profileDisplayName;

  /// No description provided for @profileNameSavedSnack.
  ///
  /// In en, this message translates to:
  /// **'Name saved'**
  String get profileNameSavedSnack;

  /// No description provided for @profileSaveName.
  ///
  /// In en, this message translates to:
  /// **'Save name'**
  String get profileSaveName;

  /// No description provided for @profileAvatar.
  ///
  /// In en, this message translates to:
  /// **'Avatar'**
  String get profileAvatar;

  /// No description provided for @profileFromGallery.
  ///
  /// In en, this message translates to:
  /// **'From gallery'**
  String get profileFromGallery;

  /// No description provided for @profileGalleryError.
  ///
  /// In en, this message translates to:
  /// **'Gallery: {error}'**
  String profileGalleryError(String error);

  /// No description provided for @profileNameHint.
  ///
  /// In en, this message translates to:
  /// **'Name shown on your profile'**
  String get profileNameHint;

  /// No description provided for @profilePuzzleEloLine.
  ///
  /// In en, this message translates to:
  /// **'Puzzle Elo: {elo}'**
  String profilePuzzleEloLine(String elo);

  /// No description provided for @profileWeekStatsLine.
  ///
  /// In en, this message translates to:
  /// **'Last 7 days: {solved} solved · {failed} wrong lines'**
  String profileWeekStatsLine(int solved, int failed);

  /// No description provided for @profileHeatmapTitle.
  ///
  /// In en, this message translates to:
  /// **'Last 28 days'**
  String get profileHeatmapTitle;

  /// No description provided for @profileHeatmapCaption.
  ///
  /// In en, this message translates to:
  /// **'Darker = more attempts; greener = higher success rate.'**
  String get profileHeatmapCaption;

  /// No description provided for @profileAvatarHint.
  ///
  /// In en, this message translates to:
  /// **'Preset icons or a photo from your gallery.'**
  String get profileAvatarHint;

  /// No description provided for @profileEloHelpBody.
  ///
  /// In en, this message translates to:
  /// **'Elo updates when you complete a graded puzzle with a known solution line (e.g. daily Lichess puzzles).'**
  String get profileEloHelpBody;

  /// No description provided for @firmwareManifestSavedSnack.
  ///
  /// In en, this message translates to:
  /// **'Manifest URL saved.'**
  String get firmwareManifestSavedSnack;

  /// No description provided for @firmwareUpdateBoardTitle.
  ///
  /// In en, this message translates to:
  /// **'Update board firmware?'**
  String get firmwareUpdateBoardTitle;

  /// No description provided for @firmwareNewVersionLine.
  ///
  /// In en, this message translates to:
  /// **'New version: {ver} — on board: {boardVer}.'**
  String firmwareNewVersionLine(String ver, String boardVer);

  /// No description provided for @firmwareUpdateIntroBody.
  ///
  /// In en, this message translates to:
  /// **'The board downloads the file over HTTPS. Keep power and Wi‑Fi stable.'**
  String get firmwareUpdateIntroBody;

  /// No description provided for @firmwareFinalConfirmTitle.
  ///
  /// In en, this message translates to:
  /// **'Final confirmation'**
  String get firmwareFinalConfirmTitle;

  /// No description provided for @firmwareFinalConfirmBody.
  ///
  /// In en, this message translates to:
  /// **'Start OTA on the board now?'**
  String get firmwareFinalConfirmBody;

  /// No description provided for @firmwareYesUpdate.
  ///
  /// In en, this message translates to:
  /// **'Yes, update'**
  String get firmwareYesUpdate;

  /// No description provided for @firmwareDailyRemindersTitle.
  ///
  /// In en, this message translates to:
  /// **'Daily update reminders'**
  String get firmwareDailyRemindersTitle;

  /// No description provided for @firmwareDailyRemindersSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Notify when a newer firmware is available.'**
  String get firmwareDailyRemindersSubtitle;

  /// No description provided for @firmwareSaveManifestUrl.
  ///
  /// In en, this message translates to:
  /// **'Save manifest URL'**
  String get firmwareSaveManifestUrl;

  /// No description provided for @firmwareCheckForUpdate.
  ///
  /// In en, this message translates to:
  /// **'Check for update'**
  String get firmwareCheckForUpdate;

  /// No description provided for @firmwareNewVersionChip.
  ///
  /// In en, this message translates to:
  /// **'New version {ver}'**
  String firmwareNewVersionChip(String ver);

  /// No description provided for @firmwareDownloadOnEspNote.
  ///
  /// In en, this message translates to:
  /// **'Download runs on the ESP over HTTPS.'**
  String get firmwareDownloadOnEspNote;

  /// No description provided for @firmwareConfirmUpdateTitle.
  ///
  /// In en, this message translates to:
  /// **'Confirm update'**
  String get firmwareConfirmUpdateTitle;

  /// No description provided for @firmwareConfirmUpdateBody.
  ///
  /// In en, this message translates to:
  /// **'Start firmware update on the board?'**
  String get firmwareConfirmUpdateBody;

  /// No description provided for @firmwareStartingOtaSnack.
  ///
  /// In en, this message translates to:
  /// **'Starting OTA on the board…'**
  String get firmwareStartingOtaSnack;

  /// No description provided for @firmwareDownloadToApp.
  ///
  /// In en, this message translates to:
  /// **'Download to app'**
  String get firmwareDownloadToApp;

  /// No description provided for @firmwareSendToBoard.
  ///
  /// In en, this message translates to:
  /// **'Send to board'**
  String get firmwareSendToBoard;

  /// No description provided for @firmwareTwoStepOtaHint.
  ///
  /// In en, this message translates to:
  /// **'Download the .bin while you have internet. Join the board hotspot, then send — the board pulls the file from your phone over HTTP (no Wi‑Fi STA on the board).'**
  String get firmwareTwoStepOtaHint;

  /// No description provided for @firmwareCachedInAppLine.
  ///
  /// In en, this message translates to:
  /// **'Saved in app: v{ver} (~{mb} MB)'**
  String firmwareCachedInAppLine(String ver, String mb);

  /// No description provided for @firmwareDownloadSavedSnack.
  ///
  /// In en, this message translates to:
  /// **'Firmware saved in the app.'**
  String get firmwareDownloadSavedSnack;

  /// No description provided for @firmwareDownloadFailedLine.
  ///
  /// In en, this message translates to:
  /// **'Download failed: {error}'**
  String firmwareDownloadFailedLine(String error);

  /// No description provided for @firmwareJoinHotspotForUpload.
  ///
  /// In en, this message translates to:
  /// **'Connect to the board Wi‑Fi hotspot first — your phone needs a 192.168.4.x address.'**
  String get firmwareJoinHotspotForUpload;

  /// No description provided for @firmwareNoCachedFirmware.
  ///
  /// In en, this message translates to:
  /// **'Download the firmware to the app first.'**
  String get firmwareNoCachedFirmware;

  /// No description provided for @firmwareSendToBoardTitle.
  ///
  /// In en, this message translates to:
  /// **'Upload from this phone?'**
  String get firmwareSendToBoardTitle;

  /// No description provided for @firmwareSendToBoardBody.
  ///
  /// In en, this message translates to:
  /// **'The board will download the .bin over HTTP from your phone. Stay on the hotspot and keep this screen open until finished.'**
  String get firmwareSendToBoardBody;

  /// No description provided for @firmwareOneStepHttpsOta.
  ///
  /// In en, this message translates to:
  /// **'Or: board downloads via HTTPS (needs STA)'**
  String get firmwareOneStepHttpsOta;

  /// No description provided for @firmwareSendViaBle.
  ///
  /// In en, this message translates to:
  /// **'Send via Bluetooth'**
  String get firmwareSendViaBle;

  /// No description provided for @firmwareSendViaBleBody.
  ///
  /// In en, this message translates to:
  /// **'The full firmware image will be transferred over Bluetooth only (no board hotspot or STA required). Keep the phone close to the board until the connection drops — the board will reboot.'**
  String get firmwareSendViaBleBody;

  /// No description provided for @firmwareBleUploadDoneSnack.
  ///
  /// In en, this message translates to:
  /// **'Bluetooth upload finished — the board may reboot.'**
  String get firmwareBleUploadDoneSnack;

  /// No description provided for @firmwareOtaSlotsDisabledHint.
  ///
  /// In en, this message translates to:
  /// **'This flash layout has no OTA slots (ota_0 + ota_1). Use USB/UART (esptool or idf.py flash) or rebuild firmware with a dual-OTA partition table (see project partitions CSV).'**
  String get firmwareOtaSlotsDisabledHint;

  /// No description provided for @firmwareBoardHttpMissingDetail.
  ///
  /// In en, this message translates to:
  /// **'Board HTTP address is missing (e.g. http://192.168.4.1). Set Default board URL in settings. Bluetooth alone does not provide an IP.'**
  String get firmwareBoardHttpMissingDetail;

  /// No description provided for @firmwareHttpsLinkExplainBody.
  ///
  /// In en, this message translates to:
  /// **'You only send an HTTPS link — the ESP downloads the full .bin. The board must be connected to Wi‑Fi as a station (STA). You can send the start command over Wi‑Fi HTTP or Bluetooth; progress is read over HTTP to the board IP.'**
  String get firmwareHttpsLinkExplainBody;

  /// No description provided for @firmwareSettingsSecondConfirmBody.
  ///
  /// In en, this message translates to:
  /// **'The board will write firmware and reboot. Do not cut power or lose Wi‑Fi while downloading. Continue?'**
  String get firmwareSettingsSecondConfirmBody;

  /// No description provided for @firmwareOtaFinishedMaybeRebootSnack.
  ///
  /// In en, this message translates to:
  /// **'OTA finished or connection dropped — the board may reboot.'**
  String get firmwareOtaFinishedMaybeRebootSnack;

  /// No description provided for @firmwareTileTitleUpdateAvailable.
  ///
  /// In en, this message translates to:
  /// **'Firmware — update available ({ver})'**
  String firmwareTileTitleUpdateAvailable(String ver);

  /// No description provided for @firmwareTileTitleDefault.
  ///
  /// In en, this message translates to:
  /// **'Firmware (over-the-air)'**
  String get firmwareTileTitleDefault;

  /// No description provided for @firmwareTileSubtitleIdle.
  ///
  /// In en, this message translates to:
  /// **'Manifest + Check. Use Download / Send on hotspot, or HTTPS if the board has STA.'**
  String get firmwareTileSubtitleIdle;

  /// No description provided for @firmwareRemindersOnShort.
  ///
  /// In en, this message translates to:
  /// **'reminders on'**
  String get firmwareRemindersOnShort;

  /// No description provided for @firmwareRemindersOffShort.
  ///
  /// In en, this message translates to:
  /// **'reminders off'**
  String get firmwareRemindersOffShort;

  /// No description provided for @firmwareDailyRemindersSubtitleLong.
  ///
  /// In en, this message translates to:
  /// **'If a newer version is on the server, ask again each day until you update or turn reminders off.'**
  String get firmwareDailyRemindersSubtitleLong;

  /// No description provided for @firmwareManifestUrlLabel.
  ///
  /// In en, this message translates to:
  /// **'Manifest URL (version.json)'**
  String get firmwareManifestUrlLabel;

  /// No description provided for @firmwareManifestUrlHint.
  ///
  /// In en, this message translates to:
  /// **'https://…/version.json'**
  String get firmwareManifestUrlHint;

  /// No description provided for @firmwareManifestUrlHelper.
  ///
  /// In en, this message translates to:
  /// **'Default URL is filled automatically. Clear the field and Save to use the built-in manifest only.'**
  String get firmwareManifestUrlHelper;

  /// No description provided for @firmwareBoardHttpVersionLine.
  ///
  /// In en, this message translates to:
  /// **'Board (HTTP): {ver}'**
  String firmwareBoardHttpVersionLine(String ver);

  /// No description provided for @firmwareManifestVersionLine.
  ///
  /// In en, this message translates to:
  /// **'Manifest: {ver}'**
  String firmwareManifestVersionLine(String ver);

  /// No description provided for @firmwareWifiStaConnected.
  ///
  /// In en, this message translates to:
  /// **'connected ({ip})'**
  String firmwareWifiStaConnected(String ip);

  /// No description provided for @firmwareWifiStaNotConnected.
  ///
  /// In en, this message translates to:
  /// **'not connected'**
  String get firmwareWifiStaNotConnected;

  /// No description provided for @firmwareWifiStaLine.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi STA: {status}'**
  String firmwareWifiStaLine(String status);

  /// No description provided for @firmwareNeedDefaultBoardUrlHint.
  ///
  /// In en, this message translates to:
  /// **'Set and save Default board URL (AP or STA IP) to read the board version.'**
  String get firmwareNeedDefaultBoardUrlHint;

  /// No description provided for @firmwareOtaPercentLine.
  ///
  /// In en, this message translates to:
  /// **'OTA {percent} %'**
  String firmwareOtaPercentLine(int percent);

  /// No description provided for @firmwareFooterTransportHint.
  ///
  /// In en, this message translates to:
  /// **'Connection: {transport}. HTTPS OTA needs STA; hotspot HTTP OTA works on 192.168.4.x; cached .bin can also upload over Bluetooth when GATT is connected.'**
  String firmwareFooterTransportHint(String transport);

  /// No description provided for @firmwareTransportLabelNone.
  ///
  /// In en, this message translates to:
  /// **'none'**
  String get firmwareTransportLabelNone;

  /// No description provided for @firmwareTransportLabelWifi.
  ///
  /// In en, this message translates to:
  /// **'wifi'**
  String get firmwareTransportLabelWifi;

  /// No description provided for @firmwareTransportLabelBle.
  ///
  /// In en, this message translates to:
  /// **'ble'**
  String get firmwareTransportLabelBle;

  /// No description provided for @firmwareTransportLabelMock.
  ///
  /// In en, this message translates to:
  /// **'mock'**
  String get firmwareTransportLabelMock;

  /// No description provided for @errWifiSsidEmpty.
  ///
  /// In en, this message translates to:
  /// **'Enter a Wi‑Fi network name (SSID).'**
  String get errWifiSsidEmpty;

  /// No description provided for @errWifiProvNeedsBle.
  ///
  /// In en, this message translates to:
  /// **'Connect via Bluetooth to provision Wi‑Fi on the board.'**
  String get errWifiProvNeedsBle;

  /// No description provided for @firmwareWifiBleProvStartedSnack.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi credentials sent — watch the board status.'**
  String get firmwareWifiBleProvStartedSnack;

  /// No description provided for @firmwareOtaNoLanRouteUseBle.
  ///
  /// In en, this message translates to:
  /// **'Your phone isn’t on the same LAN as the board. Use Bluetooth to upload the firmware, or join the board’s network.'**
  String get firmwareOtaNoLanRouteUseBle;

  /// No description provided for @firmwareOtaNoLanRouteNeedBle.
  ///
  /// In en, this message translates to:
  /// **'Connect via Bluetooth first — without LAN access the app will send the image over BLE.'**
  String get firmwareOtaNoLanRouteNeedBle;

  /// No description provided for @firmwareOtaPhoneNotOnLan.
  ///
  /// In en, this message translates to:
  /// **'This phone doesn’t appear to be on the board’s LAN. Join the board hotspot or the Wi‑Fi where the board has an IP.'**
  String get firmwareOtaPhoneNotOnLan;

  /// No description provided for @firmwareTileTitleGitBle.
  ///
  /// In en, this message translates to:
  /// **'Firmware — GitHub build ({ver})'**
  String firmwareTileTitleGitBle(String ver);

  /// No description provided for @firmwareTileSubtitleBleGitOnly.
  ///
  /// In en, this message translates to:
  /// **'Bluetooth only — download the .bin to the app, then send via BLE or board hotspot HTTP.'**
  String get firmwareTileSubtitleBleGitOnly;

  /// No description provided for @firmwareWifiBleProvisionTitle.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi on the board (via Bluetooth)'**
  String get firmwareWifiBleProvisionTitle;

  /// No description provided for @firmwareWifiBleProvisionSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Send SSID and password over BLE so the board can join your network as a client (STA).'**
  String get firmwareWifiBleProvisionSubtitle;

  /// No description provided for @firmwareWifiBleSsidLabel.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi name (SSID)'**
  String get firmwareWifiBleSsidLabel;

  /// No description provided for @firmwareWifiBlePasswordLabel.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi password'**
  String get firmwareWifiBlePasswordLabel;

  /// No description provided for @firmwareWifiBleUsePhoneSsidButton.
  ///
  /// In en, this message translates to:
  /// **'Use this phone’s Wi‑Fi name'**
  String get firmwareWifiBleUsePhoneSsidButton;

  /// No description provided for @firmwareWifiBleSendCredentials.
  ///
  /// In en, this message translates to:
  /// **'Send credentials to board'**
  String get firmwareWifiBleSendCredentials;

  /// No description provided for @firmwareBleStaOk.
  ///
  /// In en, this message translates to:
  /// **'STA: {ssid} · {ip}'**
  String firmwareBleStaOk(String ssid, String ip);

  /// No description provided for @firmwareBleStaWaiting.
  ///
  /// In en, this message translates to:
  /// **'Waiting for the board to connect to Wi‑Fi…'**
  String get firmwareBleStaWaiting;

  /// No description provided for @firmwareBleHttpOptionalHint.
  ///
  /// In en, this message translates to:
  /// **'Board HTTP URL is optional — you can update over Bluetooth without Wi‑Fi on the board.'**
  String get firmwareBleHttpOptionalHint;

  /// No description provided for @firmwareBleGitUnknownBoardVersion.
  ///
  /// In en, this message translates to:
  /// **'Board version unknown over HTTP — download to the app and prefer Bluetooth upload.'**
  String get firmwareBleGitUnknownBoardVersion;

  /// No description provided for @firmwareSendViaBlePrimary.
  ///
  /// In en, this message translates to:
  /// **'Send via Bluetooth (recommended)'**
  String get firmwareSendViaBlePrimary;

  /// No description provided for @firmwareSendToBoardHttpAlt.
  ///
  /// In en, this message translates to:
  /// **'Send via board HTTP (hotspot)'**
  String get firmwareSendToBoardHttpAlt;

  /// No description provided for @analysisAppBarTitle.
  ///
  /// In en, this message translates to:
  /// **'Analysis'**
  String get analysisAppBarTitle;

  /// No description provided for @analysisGameProgress.
  ///
  /// In en, this message translates to:
  /// **'Game in progress'**
  String get analysisGameProgress;

  /// No description provided for @analysisGameProgressBody.
  ///
  /// In en, this message translates to:
  /// **'Half-moves from the connected board.'**
  String get analysisGameProgressBody;

  /// No description provided for @analysisStockfishSection.
  ///
  /// In en, this message translates to:
  /// **'Stockfish'**
  String get analysisStockfishSection;

  /// No description provided for @analysisStockfishSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Evaluation line from the current board position.'**
  String get analysisStockfishSubtitle;

  /// No description provided for @analysisNoBoardPosition.
  ///
  /// In en, this message translates to:
  /// **'No position from board'**
  String get analysisNoBoardPosition;

  /// No description provided for @analysisNoBoardPositionBody.
  ///
  /// In en, this message translates to:
  /// **'Connect Wi‑Fi or Bluetooth and open Play.'**
  String get analysisNoBoardPositionBody;

  /// No description provided for @analysisOpenPlay.
  ///
  /// In en, this message translates to:
  /// **'Open Play'**
  String get analysisOpenPlay;

  /// No description provided for @analysisChartDisabledTitle.
  ///
  /// In en, this message translates to:
  /// **'Chart and move evaluation are off'**
  String get analysisChartDisabledTitle;

  /// No description provided for @analysisChartDisabledSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Enable move evaluation to see quality scores.'**
  String get analysisChartDisabledSubtitle;

  /// No description provided for @analysisEnableMoveEval.
  ///
  /// In en, this message translates to:
  /// **'Enable move evaluation (Stockfish)'**
  String get analysisEnableMoveEval;

  /// No description provided for @analysisGameOverview.
  ///
  /// In en, this message translates to:
  /// **'Game overview'**
  String get analysisGameOverview;

  /// No description provided for @analysisGameOverviewBody.
  ///
  /// In en, this message translates to:
  /// **'Material, clocks, and opening info when available.'**
  String get analysisGameOverviewBody;

  /// No description provided for @analysisMoveQuality.
  ///
  /// In en, this message translates to:
  /// **'Move quality'**
  String get analysisMoveQuality;

  /// No description provided for @analysisMoveQualityBody.
  ///
  /// In en, this message translates to:
  /// **'Average 0–100 by Stockfish grade and avg loss in cp.'**
  String get analysisMoveQualityBody;

  /// No description provided for @analysisQualitySummaryLine.
  ///
  /// In en, this message translates to:
  /// **'{quality}{avgCp} · {count} moves'**
  String analysisQualitySummaryLine(String quality, String avgCp, int count);

  /// No description provided for @analysisLastMoveEvalTitle.
  ///
  /// In en, this message translates to:
  /// **'Last move evaluation'**
  String get analysisLastMoveEvalTitle;

  /// No description provided for @analysisMoveHistoryTitle.
  ///
  /// In en, this message translates to:
  /// **'Move history'**
  String get analysisMoveHistoryTitle;

  /// No description provided for @analysisMoveHistoryTapHint.
  ///
  /// In en, this message translates to:
  /// **'Tap to open chronological playback.'**
  String get analysisMoveHistoryTapHint;

  /// No description provided for @analysisSecondLineTitle.
  ///
  /// In en, this message translates to:
  /// **'Second line'**
  String get analysisSecondLineTitle;

  /// No description provided for @analysisSecondLineBody.
  ///
  /// In en, this message translates to:
  /// **'Alternative continuation from the engine.'**
  String get analysisSecondLineBody;

  /// No description provided for @analysisCustomPositionTitle.
  ///
  /// In en, this message translates to:
  /// **'Custom position'**
  String get analysisCustomPositionTitle;

  /// No description provided for @analysisCustomPositionBody.
  ///
  /// In en, this message translates to:
  /// **'Analysis without a physical board.'**
  String get analysisCustomPositionBody;

  /// No description provided for @analysisDepthLabel.
  ///
  /// In en, this message translates to:
  /// **'Depth: {n}'**
  String analysisDepthLabel(String n);

  /// No description provided for @analysisAnalyzePosition.
  ///
  /// In en, this message translates to:
  /// **'Analyze position'**
  String get analysisAnalyzePosition;

  /// No description provided for @analysisSandboxFenSnack.
  ///
  /// In en, this message translates to:
  /// **'Sandbox from FEN — Play tab.'**
  String get analysisSandboxFenSnack;

  /// No description provided for @analysisPreviewInPlay.
  ///
  /// In en, this message translates to:
  /// **'Preview in Play'**
  String get analysisPreviewInPlay;

  /// No description provided for @analysisPreviewMoveTitle.
  ///
  /// In en, this message translates to:
  /// **'Preview position & move'**
  String get analysisPreviewMoveTitle;

  /// No description provided for @puzzleRoundExpiredSnack.
  ///
  /// In en, this message translates to:
  /// **'Round time expired.'**
  String get puzzleRoundExpiredSnack;

  /// No description provided for @puzzleConnectBoardSnack.
  ///
  /// In en, this message translates to:
  /// **'Connect the board (Wi‑Fi or BLE).'**
  String get puzzleConnectBoardSnack;

  /// No description provided for @puzzleNewGameSentSnack.
  ///
  /// In en, this message translates to:
  /// **'new_game command sent.'**
  String get puzzleNewGameSentSnack;

  /// No description provided for @puzzleBoardRiddleTitle.
  ///
  /// In en, this message translates to:
  /// **'Board puzzle'**
  String get puzzleBoardRiddleTitle;

  /// No description provided for @puzzleTryOnScreen.
  ///
  /// In en, this message translates to:
  /// **'Try on screen'**
  String get puzzleTryOnScreen;

  /// No description provided for @puzzleLoadToBoard.
  ///
  /// In en, this message translates to:
  /// **'Load to board (new_game + FEN)'**
  String get puzzleLoadToBoard;

  /// No description provided for @puzzleSetupWizardLabel.
  ///
  /// In en, this message translates to:
  /// **'Board setup wizard'**
  String get puzzleSetupWizardLabel;

  /// No description provided for @puzzleOpenLichess.
  ///
  /// In en, this message translates to:
  /// **'Open on Lichess'**
  String get puzzleOpenLichess;

  /// No description provided for @puzzleSavePositionTitle.
  ///
  /// In en, this message translates to:
  /// **'Save position'**
  String get puzzleSavePositionTitle;

  /// No description provided for @puzzleSavedSnack.
  ///
  /// In en, this message translates to:
  /// **'Saved.'**
  String get puzzleSavedSnack;

  /// No description provided for @puzzleAddToLibrary.
  ///
  /// In en, this message translates to:
  /// **'Add to library'**
  String get puzzleAddToLibrary;

  /// No description provided for @puzzleCustomTitle.
  ///
  /// In en, this message translates to:
  /// **'Custom'**
  String get puzzleCustomTitle;

  /// No description provided for @puzzleSetupOnBoard.
  ///
  /// In en, this message translates to:
  /// **'Setup on board'**
  String get puzzleSetupOnBoard;

  /// No description provided for @puzzleDailyTitle.
  ///
  /// In en, this message translates to:
  /// **'Daily {id}'**
  String puzzleDailyTitle(String id);

  /// No description provided for @puzzleDailySolveTitle.
  ///
  /// In en, this message translates to:
  /// **'Daily puzzle'**
  String get puzzleDailySolveTitle;

  /// No description provided for @puzzleSolutionMoves.
  ///
  /// In en, this message translates to:
  /// **'Solution: {n} moves'**
  String puzzleSolutionMoves(int n);

  /// No description provided for @puzzlePoolModeTitle.
  ///
  /// In en, this message translates to:
  /// **'Pool mode'**
  String get puzzlePoolModeTitle;

  /// No description provided for @puzzlePoolMixed.
  ///
  /// In en, this message translates to:
  /// **'Mixed'**
  String get puzzlePoolMixed;

  /// No description provided for @puzzlePoolBundled.
  ///
  /// In en, this message translates to:
  /// **'Bundled'**
  String get puzzlePoolBundled;

  /// No description provided for @puzzlePoolLibrary.
  ///
  /// In en, this message translates to:
  /// **'Library'**
  String get puzzlePoolLibrary;

  /// No description provided for @puzzleTimedRoundTitle.
  ///
  /// In en, this message translates to:
  /// **'Timed round'**
  String get puzzleTimedRoundTitle;

  /// No description provided for @puzzleTimedRoundBody.
  ///
  /// In en, this message translates to:
  /// **'Random position from the selected pool, countdown in seconds.'**
  String get puzzleTimedRoundBody;

  /// No description provided for @puzzleNewRound.
  ///
  /// In en, this message translates to:
  /// **'New round'**
  String get puzzleNewRound;

  /// No description provided for @puzzleStop.
  ///
  /// In en, this message translates to:
  /// **'Stop'**
  String get puzzleStop;

  /// No description provided for @puzzleRemainingSeconds.
  ///
  /// In en, this message translates to:
  /// **'Remaining: {n}s'**
  String puzzleRemainingSeconds(int n);

  /// No description provided for @puzzleSolveOnScreen.
  ///
  /// In en, this message translates to:
  /// **'Solve on screen'**
  String get puzzleSolveOnScreen;

  /// No description provided for @puzzleLoadToBoardShort.
  ///
  /// In en, this message translates to:
  /// **'Load to board'**
  String get puzzleLoadToBoardShort;

  /// No description provided for @puzzleSetupWizardShort.
  ///
  /// In en, this message translates to:
  /// **'Setup wizard'**
  String get puzzleSetupWizardShort;

  /// No description provided for @puzzleBundledOfflineTitle.
  ///
  /// In en, this message translates to:
  /// **'Bundled positions (offline)'**
  String get puzzleBundledOfflineTitle;

  /// No description provided for @puzzleThemeMate.
  ///
  /// In en, this message translates to:
  /// **'Mate'**
  String get puzzleThemeMate;

  /// No description provided for @puzzleThemeFork.
  ///
  /// In en, this message translates to:
  /// **'Fork'**
  String get puzzleThemeFork;

  /// No description provided for @puzzleThemeEndgame.
  ///
  /// In en, this message translates to:
  /// **'Endgame'**
  String get puzzleThemeEndgame;

  /// No description provided for @puzzleThemeMixed.
  ///
  /// In en, this message translates to:
  /// **'Mixed'**
  String get puzzleThemeMixed;

  /// No description provided for @progressAppBarTitle.
  ///
  /// In en, this message translates to:
  /// **'Progress'**
  String get progressAppBarTitle;

  /// No description provided for @progressSegBeginner.
  ///
  /// In en, this message translates to:
  /// **'Beg.'**
  String get progressSegBeginner;

  /// No description provided for @progressSegIntermediate.
  ///
  /// In en, this message translates to:
  /// **'Int.'**
  String get progressSegIntermediate;

  /// No description provided for @progressSegAdvanced.
  ///
  /// In en, this message translates to:
  /// **'Adv.'**
  String get progressSegAdvanced;

  /// No description provided for @progressSegExpert.
  ///
  /// In en, this message translates to:
  /// **'Exp.'**
  String get progressSegExpert;

  /// No description provided for @progressTabLearn.
  ///
  /// In en, this message translates to:
  /// **'Learn'**
  String get progressTabLearn;

  /// No description provided for @progressTabStats.
  ///
  /// In en, this message translates to:
  /// **'Stats'**
  String get progressTabStats;

  /// No description provided for @progressWelcomeTitle.
  ///
  /// In en, this message translates to:
  /// **'Welcome to CzechMate'**
  String get progressWelcomeTitle;

  /// No description provided for @progressWelcomeBody.
  ///
  /// In en, this message translates to:
  /// **'Track puzzles, coach, and board setup from one place.'**
  String get progressWelcomeBody;

  /// No description provided for @progressProfilePuzzleEloTitle.
  ///
  /// In en, this message translates to:
  /// **'Profile & puzzle Elo'**
  String get progressProfilePuzzleEloTitle;

  /// No description provided for @progressProfilePuzzleEloSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Local nickname and puzzle rating.'**
  String get progressProfilePuzzleEloSubtitle;

  /// No description provided for @progressLearnCardTitle.
  ///
  /// In en, this message translates to:
  /// **'Learn'**
  String get progressLearnCardTitle;

  /// No description provided for @progressLearnCardSubtitle.
  ///
  /// In en, this message translates to:
  /// **'LED setup wizards and AI coach'**
  String get progressLearnCardSubtitle;

  /// No description provided for @progressAiCoachTitle.
  ///
  /// In en, this message translates to:
  /// **'AI coach'**
  String get progressAiCoachTitle;

  /// No description provided for @progressAiCoachBody.
  ///
  /// In en, this message translates to:
  /// **'Turn on learning mode on Play. Chat uses the cloud API or a local fallback; Stockfish stays on Analysis.'**
  String get progressAiCoachBody;

  /// No description provided for @progressLearningModeTile.
  ///
  /// In en, this message translates to:
  /// **'Learning mode'**
  String get progressLearningModeTile;

  /// No description provided for @progressCoachLevelLabel.
  ///
  /// In en, this message translates to:
  /// **'Coach level'**
  String get progressCoachLevelLabel;

  /// No description provided for @progressCoachChatButton.
  ///
  /// In en, this message translates to:
  /// **'Coach chat'**
  String get progressCoachChatButton;

  /// No description provided for @progressPositionPlanButton.
  ///
  /// In en, this message translates to:
  /// **'Position plan'**
  String get progressPositionPlanButton;

  /// No description provided for @progressBoardErrorTitle.
  ///
  /// In en, this message translates to:
  /// **'Board error'**
  String get progressBoardErrorTitle;

  /// No description provided for @progressStartingPositionTitle.
  ///
  /// In en, this message translates to:
  /// **'Starting position'**
  String get progressStartingPositionTitle;

  /// No description provided for @progressStartingPositionBody.
  ///
  /// In en, this message translates to:
  /// **'Wizard aligns pieces using LEDs.'**
  String get progressStartingPositionBody;

  /// No description provided for @progressStartingPositionTeachingBody.
  ///
  /// In en, this message translates to:
  /// **'Teaching mode: LEDs show each square in order; the app shows which piece to place (same flow as iOS).'**
  String get progressStartingPositionTeachingBody;

  /// No description provided for @progressRunWizardStarting.
  ///
  /// In en, this message translates to:
  /// **'Run wizard — starting position'**
  String get progressRunWizardStarting;

  /// No description provided for @progressAccountCardTitle.
  ///
  /// In en, this message translates to:
  /// **'Account'**
  String get progressAccountCardTitle;

  /// No description provided for @progressAccountCardSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Local profile and activity'**
  String get progressAccountCardSubtitle;

  /// No description provided for @progressActiveTransportTitle.
  ///
  /// In en, this message translates to:
  /// **'Active transport'**
  String get progressActiveTransportTitle;

  /// No description provided for @progressNoSessionTitle.
  ///
  /// In en, this message translates to:
  /// **'No active session yet'**
  String get progressNoSessionTitle;

  /// No description provided for @progressNoSessionSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Open Find board to connect.'**
  String get progressNoSessionSubtitle;

  /// No description provided for @progressStatsSegmentSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Sessions and in-app stats'**
  String get progressStatsSegmentSubtitle;

  /// No description provided for @progressProfileIconTooltip.
  ///
  /// In en, this message translates to:
  /// **'Profile & Elo'**
  String get progressProfileIconTooltip;

  /// No description provided for @progressLearningModePlayHint.
  ///
  /// In en, this message translates to:
  /// **'Enable learning mode on Play → Game controls to use chat and position plan.'**
  String get progressLearningModePlayHint;

  /// No description provided for @progressWizardConnectHint.
  ///
  /// In en, this message translates to:
  /// **'Connect the board for LED wizards (connection chip on the Play tab).'**
  String get progressWizardConnectHint;

  /// No description provided for @progressStatsCurrentMoves.
  ///
  /// In en, this message translates to:
  /// **'Current move count'**
  String get progressStatsCurrentMoves;

  /// No description provided for @progressStatsPeakMoves.
  ///
  /// In en, this message translates to:
  /// **'Peak move count seen'**
  String get progressStatsPeakMoves;

  /// No description provided for @progressStatsPollSuccess.
  ///
  /// In en, this message translates to:
  /// **'Successful HTTP polls'**
  String get progressStatsPollSuccess;

  /// No description provided for @progressStatsPollFail.
  ///
  /// In en, this message translates to:
  /// **'Failed polls'**
  String get progressStatsPollFail;

  /// No description provided for @progressStatsWsMessages.
  ///
  /// In en, this message translates to:
  /// **'WebSocket messages'**
  String get progressStatsWsMessages;

  /// No description provided for @progressShortOn.
  ///
  /// In en, this message translates to:
  /// **'on'**
  String get progressShortOn;

  /// No description provided for @progressShortOff.
  ///
  /// In en, this message translates to:
  /// **'off'**
  String get progressShortOff;

  /// No description provided for @progressWaitingForData.
  ///
  /// In en, this message translates to:
  /// **'waiting for data'**
  String get progressWaitingForData;

  /// No description provided for @progressTransportWifi.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi'**
  String get progressTransportWifi;

  /// No description provided for @progressTransportBluetooth.
  ///
  /// In en, this message translates to:
  /// **'Bluetooth'**
  String get progressTransportBluetooth;

  /// No description provided for @progressTransportDevDetail.
  ///
  /// In en, this message translates to:
  /// **'{transport} · polling {pollState} · WS {wsState}'**
  String progressTransportDevDetail(
      String transport, String pollState, String wsState);

  /// No description provided for @progressTransportUserDetail.
  ///
  /// In en, this message translates to:
  /// **'{transport} · live sync {syncState}'**
  String progressTransportUserDetail(String transport, String syncState);

  /// No description provided for @progressStatsEmptySubtitle.
  ///
  /// In en, this message translates to:
  /// **'Connect the board on Play — stats fill in once polling runs.'**
  String get progressStatsEmptySubtitle;

  /// No description provided for @helpAppBarTitle.
  ///
  /// In en, this message translates to:
  /// **'czechmate — help'**
  String get helpAppBarTitle;

  /// No description provided for @helpConnectTitle.
  ///
  /// In en, this message translates to:
  /// **'Connecting the board'**
  String get helpConnectTitle;

  /// No description provided for @helpConnectBody.
  ///
  /// In en, this message translates to:
  /// **'Use Bluetooth to discover the board, then Wi‑Fi when available. Check Settings for manual URL and modes.'**
  String get helpConnectBody;

  /// No description provided for @helpPlayingTitle.
  ///
  /// In en, this message translates to:
  /// **'Playing a game'**
  String get helpPlayingTitle;

  /// No description provided for @helpPlayingBody.
  ///
  /// In en, this message translates to:
  /// **'Use Play for live sync, Game controls for time and flip, and Analysis for Stockfish.'**
  String get helpPlayingBody;

  /// No description provided for @helpCoachTitle.
  ///
  /// In en, this message translates to:
  /// **'Coach & analysis'**
  String get helpCoachTitle;

  /// No description provided for @helpCoachBody.
  ///
  /// In en, this message translates to:
  /// **'Enable learning mode and connect API keys in Settings for cloud coaches.'**
  String get helpCoachBody;

  /// No description provided for @helpSettingsTitle.
  ///
  /// In en, this message translates to:
  /// **'Settings & configuration'**
  String get helpSettingsTitle;

  /// No description provided for @helpSettingsBody.
  ///
  /// In en, this message translates to:
  /// **'Appearance, NVS, lamp, firmware, and MQTT live here.'**
  String get helpSettingsBody;

  /// No description provided for @coachScreenTitle.
  ///
  /// In en, this message translates to:
  /// **'AI Coach'**
  String get coachScreenTitle;

  /// No description provided for @coachScreenHintTitle.
  ///
  /// In en, this message translates to:
  /// **'Opening & plans'**
  String get coachScreenHintTitle;

  /// No description provided for @coachScreenHintBody.
  ///
  /// In en, this message translates to:
  /// **'Ask about ideas for this position.'**
  String get coachScreenHintBody;

  /// No description provided for @coachScreenLevelLabel.
  ///
  /// In en, this message translates to:
  /// **'Level {level}'**
  String coachScreenLevelLabel(String level);

  /// No description provided for @setupModeAppBar.
  ///
  /// In en, this message translates to:
  /// **'Setup mode'**
  String get setupModeAppBar;

  /// No description provided for @setupModePiecePlacementTitle.
  ///
  /// In en, this message translates to:
  /// **'Piece placement'**
  String get setupModePiecePlacementTitle;

  /// No description provided for @setupModePiecePlacementBody.
  ///
  /// In en, this message translates to:
  /// **'Use the wizard to align pieces with LED hints.'**
  String get setupModePiecePlacementBody;

  /// No description provided for @setupModeGoBack.
  ///
  /// In en, this message translates to:
  /// **'Go back'**
  String get setupModeGoBack;

  /// No description provided for @wizardSkipStep.
  ///
  /// In en, this message translates to:
  /// **'Skip step'**
  String get wizardSkipStep;

  /// No description provided for @wizardCancel.
  ///
  /// In en, this message translates to:
  /// **'Cancel'**
  String get wizardCancel;

  /// No description provided for @connDiagTitle.
  ///
  /// In en, this message translates to:
  /// **'Connection diagnostics'**
  String get connDiagTitle;

  /// No description provided for @connDiagTransport.
  ///
  /// In en, this message translates to:
  /// **'Transport'**
  String get connDiagTransport;

  /// No description provided for @connDiagWifiBaseUrl.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi base URL'**
  String get connDiagWifiBaseUrl;

  /// No description provided for @connDiagPolling.
  ///
  /// In en, this message translates to:
  /// **'Polling'**
  String get connDiagPolling;

  /// No description provided for @connDiagWebSocket.
  ///
  /// In en, this message translates to:
  /// **'WebSocket'**
  String get connDiagWebSocket;

  /// No description provided for @connDiagPollSuccessTitle.
  ///
  /// In en, this message translates to:
  /// **'Successful snapshot GETs (incl. 304)'**
  String get connDiagPollSuccessTitle;

  /// No description provided for @connDiagPollFailureTitle.
  ///
  /// In en, this message translates to:
  /// **'Failed GET / poll errors'**
  String get connDiagPollFailureTitle;

  /// No description provided for @connDiagWsFramesTitle.
  ///
  /// In en, this message translates to:
  /// **'WS messages (frames)'**
  String get connDiagWsFramesTitle;

  /// No description provided for @connDiagLastPollOk.
  ///
  /// In en, this message translates to:
  /// **'Last successful poll'**
  String get connDiagLastPollOk;

  /// No description provided for @connDiagLastErrorTitle.
  ///
  /// In en, this message translates to:
  /// **'Last error'**
  String get connDiagLastErrorTitle;

  /// No description provided for @connDiagActive.
  ///
  /// In en, this message translates to:
  /// **'active'**
  String get connDiagActive;

  /// No description provided for @connDiagOff.
  ///
  /// In en, this message translates to:
  /// **'off'**
  String get connDiagOff;

  /// No description provided for @newGameSheetTitle.
  ///
  /// In en, this message translates to:
  /// **'New game'**
  String get newGameSheetTitle;

  /// No description provided for @newGameSheetSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Pick time control and side.'**
  String get newGameSheetSubtitle;

  /// No description provided for @newGameWhiteBottom.
  ///
  /// In en, this message translates to:
  /// **'White bottom'**
  String get newGameWhiteBottom;

  /// No description provided for @newGameBlackBottom.
  ///
  /// In en, this message translates to:
  /// **'Black bottom'**
  String get newGameBlackBottom;

  /// No description provided for @newGameCustomTimeTitle.
  ///
  /// In en, this message translates to:
  /// **'Custom time (type 14)'**
  String get newGameCustomTimeTitle;

  /// No description provided for @newGameCustomTimeSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Minutes + increment sent to the board.'**
  String get newGameCustomTimeSubtitle;

  /// No description provided for @newGameMinutesPerSide.
  ///
  /// In en, this message translates to:
  /// **'Minutes per side: {n}'**
  String newGameMinutesPerSide(String n);

  /// No description provided for @newGameIncrementSeconds.
  ///
  /// In en, this message translates to:
  /// **'Increment (s): {n}'**
  String newGameIncrementSeconds(String n);

  /// No description provided for @newGameStartOnBoard.
  ///
  /// In en, this message translates to:
  /// **'Start on board'**
  String get newGameStartOnBoard;

  /// No description provided for @newGameConnectFirstSnack.
  ///
  /// In en, this message translates to:
  /// **'Connect a board first (Wi‑Fi or Bluetooth).'**
  String get newGameConnectFirstSnack;

  /// No description provided for @newGameStartedSnack.
  ///
  /// In en, this message translates to:
  /// **'New game started on the board.'**
  String get newGameStartedSnack;

  /// No description provided for @newGameErrorSnack.
  ///
  /// In en, this message translates to:
  /// **'Error: {error}'**
  String newGameErrorSnack(String error);

  /// No description provided for @newGameSheetBody.
  ///
  /// In en, this message translates to:
  /// **'Like on iOS: time control is sent to the board, then a new game starts.'**
  String get newGameSheetBody;

  /// No description provided for @newGameBoardViewSection.
  ///
  /// In en, this message translates to:
  /// **'Board view (which color is at the bottom)'**
  String get newGameBoardViewSection;

  /// No description provided for @newGameWhoStartsNote.
  ///
  /// In en, this message translates to:
  /// **'Who starts is decided on the board / game rules; here you only flip the view (saved in Settings).'**
  String get newGameWhoStartsNote;

  /// No description provided for @newGameFirmwarePresets.
  ///
  /// In en, this message translates to:
  /// **'Firmware presets'**
  String get newGameFirmwarePresets;

  /// No description provided for @newGameCustomSummary.
  ///
  /// In en, this message translates to:
  /// **'{min} min + {inc} s / move'**
  String newGameCustomSummary(int min, int inc);

  /// No description provided for @learnAppBarTitle.
  ///
  /// In en, this message translates to:
  /// **'Chess lessons'**
  String get learnAppBarTitle;

  /// No description provided for @learnBoardLedHint.
  ///
  /// In en, this message translates to:
  /// **'Connect the board and use LED hints while practicing these lessons.'**
  String get learnBoardLedHint;

  /// No description provided for @learnSnackLesson.
  ///
  /// In en, this message translates to:
  /// **'Lesson: {title} — interactive content coming soon.'**
  String learnSnackLesson(String title);

  /// No description provided for @learnSecBasics.
  ///
  /// In en, this message translates to:
  /// **'Basics'**
  String get learnSecBasics;

  /// No description provided for @learnSecSpecial.
  ///
  /// In en, this message translates to:
  /// **'Special moves'**
  String get learnSecSpecial;

  /// No description provided for @learnSecTactics.
  ///
  /// In en, this message translates to:
  /// **'Tactics'**
  String get learnSecTactics;

  /// No description provided for @learnSecStrategy.
  ///
  /// In en, this message translates to:
  /// **'Strategy'**
  String get learnSecStrategy;

  /// No description provided for @learnL1Title.
  ///
  /// In en, this message translates to:
  /// **'How pieces move'**
  String get learnL1Title;

  /// No description provided for @learnL1Desc.
  ///
  /// In en, this message translates to:
  /// **'King, queen, rook, bishop, knight, pawn — learn each piece’s moves.'**
  String get learnL1Desc;

  /// No description provided for @learnL2Title.
  ///
  /// In en, this message translates to:
  /// **'Goals of chess'**
  String get learnL2Title;

  /// No description provided for @learnL2Desc.
  ///
  /// In en, this message translates to:
  /// **'Checkmate, stalemate, draw — what end states mean.'**
  String get learnL2Desc;

  /// No description provided for @learnL3Title.
  ///
  /// In en, this message translates to:
  /// **'Piece values'**
  String get learnL3Title;

  /// No description provided for @learnL3Desc.
  ///
  /// In en, this message translates to:
  /// **'Pawn=1, Knight/Bishop=3, Rook=5, Queen=9 — why it matters.'**
  String get learnL3Desc;

  /// No description provided for @learnL4Title.
  ///
  /// In en, this message translates to:
  /// **'Castling'**
  String get learnL4Title;

  /// No description provided for @learnL4Desc.
  ///
  /// In en, this message translates to:
  /// **'Kingside and queenside — how, when, and why.'**
  String get learnL4Desc;

  /// No description provided for @learnL5Title.
  ///
  /// In en, this message translates to:
  /// **'En passant'**
  String get learnL5Title;

  /// No description provided for @learnL5Desc.
  ///
  /// In en, this message translates to:
  /// **'The special pawn capture beginners often miss.'**
  String get learnL5Desc;

  /// No description provided for @learnL6Title.
  ///
  /// In en, this message translates to:
  /// **'Promotion'**
  String get learnL6Title;

  /// No description provided for @learnL6Desc.
  ///
  /// In en, this message translates to:
  /// **'Pawn on the eighth rank — choosing the right piece.'**
  String get learnL6Desc;

  /// No description provided for @learnL7Title.
  ///
  /// In en, this message translates to:
  /// **'Fork'**
  String get learnL7Title;

  /// No description provided for @learnL7Desc.
  ///
  /// In en, this message translates to:
  /// **'Attack two pieces at once — a core tactical theme.'**
  String get learnL7Desc;

  /// No description provided for @learnL8Title.
  ///
  /// In en, this message translates to:
  /// **'Pin'**
  String get learnL8Title;

  /// No description provided for @learnL8Desc.
  ///
  /// In en, this message translates to:
  /// **'A piece can’t move because it would expose the king.'**
  String get learnL8Desc;

  /// No description provided for @learnL9Title.
  ///
  /// In en, this message translates to:
  /// **'Scholar’s Mate'**
  String get learnL9Title;

  /// No description provided for @learnL9Desc.
  ///
  /// In en, this message translates to:
  /// **'Mate in four — and how to stop it.'**
  String get learnL9Desc;

  /// No description provided for @learnL10Title.
  ///
  /// In en, this message translates to:
  /// **'Control the center'**
  String get learnL10Title;

  /// No description provided for @learnL10Desc.
  ///
  /// In en, this message translates to:
  /// **'Why e4, d4, e5, d5 matter in the opening.'**
  String get learnL10Desc;

  /// No description provided for @learnL11Title.
  ///
  /// In en, this message translates to:
  /// **'King safety'**
  String get learnL11Title;

  /// No description provided for @learnL11Desc.
  ///
  /// In en, this message translates to:
  /// **'Castle early as insurance — when and how to hide the king.'**
  String get learnL11Desc;

  /// No description provided for @learnL12Title.
  ///
  /// In en, this message translates to:
  /// **'Endgame: king & pawn'**
  String get learnL12Title;

  /// No description provided for @learnL12Desc.
  ///
  /// In en, this message translates to:
  /// **'Turn a pawn advantage into a win.'**
  String get learnL12Desc;

  /// No description provided for @devSettingsTitle.
  ///
  /// In en, this message translates to:
  /// **'Diagnostics & Developer'**
  String get devSettingsTitle;

  /// No description provided for @devStockfishFenHeader.
  ///
  /// In en, this message translates to:
  /// **'Stockfish & FEN'**
  String get devStockfishFenHeader;

  /// No description provided for @devMoveEvalTile.
  ///
  /// In en, this message translates to:
  /// **'Move evaluation (moveEvaluationEnabled)'**
  String get devMoveEvalTile;

  /// No description provided for @devHintDepthTile.
  ///
  /// In en, this message translates to:
  /// **'Hint depth (hintDepth): {n}'**
  String devHintDepthTile(String n);

  /// No description provided for @devCurrentFenLabel.
  ///
  /// In en, this message translates to:
  /// **'Current FEN from board:'**
  String get devCurrentFenLabel;

  /// No description provided for @devNetworkTransportHeader.
  ///
  /// In en, this message translates to:
  /// **'Network & transport'**
  String get devNetworkTransportHeader;

  /// No description provided for @devBoardBaseUrlTile.
  ///
  /// In en, this message translates to:
  /// **'Board base URL (ESP)'**
  String get devBoardBaseUrlTile;

  /// No description provided for @devActiveLinkTile.
  ///
  /// In en, this message translates to:
  /// **'Active link status'**
  String get devActiveLinkTile;

  /// No description provided for @devCoachTraceTile.
  ///
  /// In en, this message translates to:
  /// **'Verbose coach logs (coach trace)'**
  String get devCoachTraceTile;

  /// No description provided for @devStartConnection.
  ///
  /// In en, this message translates to:
  /// **'Start connection'**
  String get devStartConnection;

  /// No description provided for @devStopTransport.
  ///
  /// In en, this message translates to:
  /// **'Stop'**
  String get devStopTransport;

  /// No description provided for @devTransportStoppedSnack.
  ///
  /// In en, this message translates to:
  /// **'Transport stopped.'**
  String get devTransportStoppedSnack;

  /// No description provided for @devRefreshedFromPrefsSnack.
  ///
  /// In en, this message translates to:
  /// **'Reloaded from preferences.'**
  String get devRefreshedFromPrefsSnack;

  /// No description provided for @devPingSnapshot.
  ///
  /// In en, this message translates to:
  /// **'Ping snapshot (RTT): {result}'**
  String devPingSnapshot(String result);

  /// No description provided for @devConnectionDiagTile.
  ///
  /// In en, this message translates to:
  /// **'Connection diagnostics (REST / WS)'**
  String get devConnectionDiagTile;

  /// No description provided for @devDisconnectStaSnack.
  ///
  /// In en, this message translates to:
  /// **'STA disconnected.'**
  String get devDisconnectStaSnack;

  /// No description provided for @devClearWifiNvsTitle.
  ///
  /// In en, this message translates to:
  /// **'Clear Wi‑Fi from NVS?'**
  String get devClearWifiNvsTitle;

  /// No description provided for @devClearWifiNvsBody.
  ///
  /// In en, this message translates to:
  /// **'The ESP will forget the stored network.'**
  String get devClearWifiNvsBody;

  /// No description provided for @devWifiNvsClearedSnack.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi NVS cleared.'**
  String get devWifiNvsClearedSnack;

  /// No description provided for @devClearWifiNvsButton.
  ///
  /// In en, this message translates to:
  /// **'Clear saved Wi‑Fi from NVS'**
  String get devClearWifiNvsButton;

  /// No description provided for @devWifiConfigHeader.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi configuration on board'**
  String get devWifiConfigHeader;

  /// No description provided for @devWifiUnavailable.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi status unavailable. Tap refresh below.'**
  String get devWifiUnavailable;

  /// No description provided for @devNvsToolsButton.
  ///
  /// In en, this message translates to:
  /// **'Detailed tools (NVS classes)'**
  String get devNvsToolsButton;

  /// No description provided for @devErrorPrefix.
  ///
  /// In en, this message translates to:
  /// **'Error: {error}'**
  String devErrorPrefix(String error);

  /// No description provided for @devSentToEspSnack.
  ///
  /// In en, this message translates to:
  /// **'Sent to ESP.'**
  String get devSentToEspSnack;

  /// No description provided for @boardNvsTitle.
  ///
  /// In en, this message translates to:
  /// **'Board memory (NVS)'**
  String get boardNvsTitle;

  /// No description provided for @boardNvsSavedSnack.
  ///
  /// In en, this message translates to:
  /// **'Saved to board NVS.'**
  String get boardNvsSavedSnack;

  /// No description provided for @boardNvsLedHeader.
  ///
  /// In en, this message translates to:
  /// **'On-board chess hints (LED)'**
  String get boardNvsLedHeader;

  /// No description provided for @boardNvsEvalMode.
  ///
  /// In en, this message translates to:
  /// **'Eval mode (compute best moves)'**
  String get boardNvsEvalMode;

  /// No description provided for @boardNvsStockfishDepth.
  ///
  /// In en, this message translates to:
  /// **'Stockfish depth D1–D18'**
  String get boardNvsStockfishDepth;

  /// No description provided for @boardNvsLedBest.
  ///
  /// In en, this message translates to:
  /// **'Show LED rewards (best move)'**
  String get boardNvsLedBest;

  /// No description provided for @boardNvsLedGood.
  ///
  /// In en, this message translates to:
  /// **'Show LED rewards (good move)'**
  String get boardNvsLedGood;

  /// No description provided for @boardNvsLedCapture.
  ///
  /// In en, this message translates to:
  /// **'Show LED rewards (capture)'**
  String get boardNvsLedCapture;

  /// No description provided for @boardNvsUartStats.
  ///
  /// In en, this message translates to:
  /// **'Print hint stats to UART'**
  String get boardNvsUartStats;

  /// No description provided for @boardNvsLiftBeforeBotTarget.
  ///
  /// In en, this message translates to:
  /// **'Bot target LED only after lift'**
  String get boardNvsLiftBeforeBotTarget;

  /// No description provided for @boardNvsTutorialMode.
  ///
  /// In en, this message translates to:
  /// **'Tutorial mode enabled'**
  String get boardNvsTutorialMode;

  /// No description provided for @boardNvsConfirmNewGameLed.
  ///
  /// In en, this message translates to:
  /// **'Confirm new game with LED button'**
  String get boardNvsConfirmNewGameLed;

  /// No description provided for @boardNvsHintLimit.
  ///
  /// In en, this message translates to:
  /// **'Hint limit (0 = unlimited)'**
  String get boardNvsHintLimit;

  /// No description provided for @boardNvsHintTierTitle.
  ///
  /// In en, this message translates to:
  /// **'Hint type (MoveHintTier)'**
  String get boardNvsHintTierTitle;

  /// No description provided for @boardNvsHintTierSubtitle.
  ///
  /// In en, this message translates to:
  /// **'H1 = best only, H2 = top 3, H3 = full Stockfish set'**
  String get boardNvsHintTierSubtitle;

  /// No description provided for @boardNvsH1.
  ///
  /// In en, this message translates to:
  /// **'H1 – Best'**
  String get boardNvsH1;

  /// No description provided for @boardNvsH2.
  ///
  /// In en, this message translates to:
  /// **'H2 – Top 3'**
  String get boardNvsH2;

  /// No description provided for @boardNvsH3.
  ///
  /// In en, this message translates to:
  /// **'H3 – All'**
  String get boardNvsH3;

  /// No description provided for @boardNvsOpponentHeader.
  ///
  /// In en, this message translates to:
  /// **'Opponent (bot) settings'**
  String get boardNvsOpponentHeader;

  /// No description provided for @boardNvsBotMode.
  ///
  /// In en, this message translates to:
  /// **'Bot mode'**
  String get boardNvsBotMode;

  /// No description provided for @boardNvsPvp.
  ///
  /// In en, this message translates to:
  /// **'Player vs player (off)'**
  String get boardNvsPvp;

  /// No description provided for @boardNvsPvb.
  ///
  /// In en, this message translates to:
  /// **'Player vs bot'**
  String get boardNvsPvb;

  /// No description provided for @boardNvsBotStrength.
  ///
  /// In en, this message translates to:
  /// **'Bot strength (level)'**
  String get boardNvsBotStrength;

  /// No description provided for @boardNvsBotPlaysAs.
  ///
  /// In en, this message translates to:
  /// **'Bot plays as'**
  String get boardNvsBotPlaysAs;

  /// No description provided for @boardNvsDemoHeader.
  ///
  /// In en, this message translates to:
  /// **'Demo mode'**
  String get boardNvsDemoHeader;

  /// No description provided for @boardNvsSaveToBoard.
  ///
  /// In en, this message translates to:
  /// **'Save to board NVS'**
  String get boardNvsSaveToBoard;

  /// No description provided for @boardDemoStateLine.
  ///
  /// In en, this message translates to:
  /// **'On-board demo: {on}'**
  String boardDemoStateLine(String on);

  /// No description provided for @boardDemoOn.
  ///
  /// In en, this message translates to:
  /// **'on'**
  String get boardDemoOn;

  /// No description provided for @boardDemoOff.
  ///
  /// In en, this message translates to:
  /// **'off'**
  String get boardDemoOff;

  /// No description provided for @boardDemoEnabledConfig.
  ///
  /// In en, this message translates to:
  /// **'Demo enabled (configuration)'**
  String get boardDemoEnabledConfig;

  /// No description provided for @boardDemoSpeedLabel.
  ///
  /// In en, this message translates to:
  /// **'Animation speed: {ms} ms'**
  String boardDemoSpeedLabel(String ms);

  /// No description provided for @boardDemoSendConfig.
  ///
  /// In en, this message translates to:
  /// **'Send demo config'**
  String get boardDemoSendConfig;

  /// No description provided for @boardDemoStart.
  ///
  /// In en, this message translates to:
  /// **'Start demo'**
  String get boardDemoStart;

  /// No description provided for @boardLedSentSnack.
  ///
  /// In en, this message translates to:
  /// **'LED guidance sent.'**
  String get boardLedSentSnack;

  /// No description provided for @boardLedGuidanceTitle.
  ///
  /// In en, this message translates to:
  /// **'LED guidance (level {level} / 5)'**
  String boardLedGuidanceTitle(String level);

  /// No description provided for @boardLedSendLevel.
  ///
  /// In en, this message translates to:
  /// **'Send LED level'**
  String get boardLedSendLevel;

  /// No description provided for @boardGuidedCaptureTitle.
  ///
  /// In en, this message translates to:
  /// **'Guided captures'**
  String get boardGuidedCaptureTitle;

  /// No description provided for @boardGuidedCaptureSubtitle.
  ///
  /// In en, this message translates to:
  /// **'LED highlight possible captures.'**
  String get boardGuidedCaptureSubtitle;

  /// No description provided for @lampBoardLightTitle.
  ///
  /// In en, this message translates to:
  /// **'Board light'**
  String get lampBoardLightTitle;

  /// No description provided for @lampDetailTitle.
  ///
  /// In en, this message translates to:
  /// **'Lamp — detail'**
  String get lampDetailTitle;

  /// No description provided for @lampDetailsButton.
  ///
  /// In en, this message translates to:
  /// **'Lamp details'**
  String get lampDetailsButton;

  /// No description provided for @lampHeader.
  ///
  /// In en, this message translates to:
  /// **'Lamp'**
  String get lampHeader;

  /// No description provided for @lampNeedsConnection.
  ///
  /// In en, this message translates to:
  /// **'Requires board connection (Wi‑Fi or Bluetooth).'**
  String get lampNeedsConnection;

  /// No description provided for @lampOnTitle.
  ///
  /// In en, this message translates to:
  /// **'Lamp on'**
  String get lampOnTitle;

  /// No description provided for @lampBrightnessLabel.
  ///
  /// In en, this message translates to:
  /// **'Brightness {n}%'**
  String lampBrightnessLabel(String n);

  /// No description provided for @lampSendBrightness.
  ///
  /// In en, this message translates to:
  /// **'Send brightness'**
  String get lampSendBrightness;

  /// No description provided for @lampBrightnessSentSnack.
  ///
  /// In en, this message translates to:
  /// **'Brightness sent to the board.'**
  String get lampBrightnessSentSnack;

  /// No description provided for @lampColorStateSentSnack.
  ///
  /// In en, this message translates to:
  /// **'Color and lamp state sent.'**
  String get lampColorStateSentSnack;

  /// No description provided for @lampGenericCommandSnack.
  ///
  /// In en, this message translates to:
  /// **'Lamp command sent.'**
  String get lampGenericCommandSnack;

  /// No description provided for @lampBlockStatusSummary.
  ///
  /// In en, this message translates to:
  /// **'Mode: {mode} · RGB({r},{g},{b}) · hint limit: {h} · auto-off: {t}s'**
  String lampBlockStatusSummary(
      String mode, String r, String g, String b, String h, String t);

  /// No description provided for @lampRgbHint.
  ///
  /// In en, this message translates to:
  /// **'Color (R G B) — POST /api/light/command'**
  String get lampRgbHint;

  /// No description provided for @lampSendColor.
  ///
  /// In en, this message translates to:
  /// **'Send color / lamp state'**
  String get lampSendColor;

  /// No description provided for @lampGameModeButton.
  ///
  /// In en, this message translates to:
  /// **'Game lamp mode'**
  String get lampGameModeButton;

  /// No description provided for @lampColorLabel.
  ///
  /// In en, this message translates to:
  /// **'Color'**
  String get lampColorLabel;

  /// No description provided for @lampColorfulnessLabel.
  ///
  /// In en, this message translates to:
  /// **'Colorfulness'**
  String get lampColorfulnessLabel;

  /// No description provided for @lampLedBrightnessPct.
  ///
  /// In en, this message translates to:
  /// **'LED brightness (%)'**
  String get lampLedBrightnessPct;

  /// No description provided for @lampStudioColorTitle.
  ///
  /// In en, this message translates to:
  /// **'Color'**
  String get lampStudioColorTitle;

  /// No description provided for @lampStudioColorHint.
  ///
  /// In en, this message translates to:
  /// **'Tap or drag — hue and saturation (center = white).'**
  String get lampStudioColorHint;

  /// No description provided for @lampStudioValueTitle.
  ///
  /// In en, this message translates to:
  /// **'Color lightness'**
  String get lampStudioValueTitle;

  /// No description provided for @lampStudioBoardPreview.
  ///
  /// In en, this message translates to:
  /// **'Board preview'**
  String get lampStudioBoardPreview;

  /// No description provided for @lampStudioPreviewGlowHint.
  ///
  /// In en, this message translates to:
  /// **'Light spill preview'**
  String get lampStudioPreviewGlowHint;

  /// No description provided for @lampStudioPreviewLampOff.
  ///
  /// In en, this message translates to:
  /// **'Lamp off'**
  String get lampStudioPreviewLampOff;

  /// No description provided for @lampStudioScenes.
  ///
  /// In en, this message translates to:
  /// **'Scenes'**
  String get lampStudioScenes;

  /// No description provided for @lampStudioApplyToBoard.
  ///
  /// In en, this message translates to:
  /// **'Apply to board'**
  String get lampStudioApplyToBoard;

  /// No description provided for @lampStudioGameMode.
  ///
  /// In en, this message translates to:
  /// **'Game mode'**
  String get lampStudioGameMode;

  /// No description provided for @lampStudioAppliedOk.
  ///
  /// In en, this message translates to:
  /// **'Color and brightness sent to the board.'**
  String get lampStudioAppliedOk;

  /// No description provided for @lampStudioGameModeOk.
  ///
  /// In en, this message translates to:
  /// **'Game lamp mode applied.'**
  String get lampStudioGameModeOk;

  /// No description provided for @lampStudioNeedConnection.
  ///
  /// In en, this message translates to:
  /// **'Connect the board via Wi‑Fi or Bluetooth to control the lamp.'**
  String get lampStudioNeedConnection;

  /// No description provided for @lampStudioAutoOff.
  ///
  /// In en, this message translates to:
  /// **'Auto shut-off'**
  String get lampStudioAutoOff;

  /// No description provided for @lampStudioSaveTimer.
  ///
  /// In en, this message translates to:
  /// **'Save timer'**
  String get lampStudioSaveTimer;

  /// No description provided for @lampStudioTimerSavedOk.
  ///
  /// In en, this message translates to:
  /// **'Auto shut-off timer saved.'**
  String get lampStudioTimerSavedOk;

  /// No description provided for @lampStudioRgbLine.
  ///
  /// In en, this message translates to:
  /// **'RGB ({r}, {g}, {b})'**
  String lampStudioRgbLine(int r, int g, int b);

  /// No description provided for @lampPresetWarmWhite.
  ///
  /// In en, this message translates to:
  /// **'Warm white'**
  String get lampPresetWarmWhite;

  /// No description provided for @lampPresetCoolWhite.
  ///
  /// In en, this message translates to:
  /// **'Cool white'**
  String get lampPresetCoolWhite;

  /// No description provided for @lampPresetCalmBlue.
  ///
  /// In en, this message translates to:
  /// **'Calm blue'**
  String get lampPresetCalmBlue;

  /// No description provided for @lampPresetForestGreen.
  ///
  /// In en, this message translates to:
  /// **'Forest green'**
  String get lampPresetForestGreen;

  /// No description provided for @lampPresetWarmth.
  ///
  /// In en, this message translates to:
  /// **'Warmth'**
  String get lampPresetWarmth;

  /// No description provided for @lampPresetPurpleScene.
  ///
  /// In en, this message translates to:
  /// **'Purple scene'**
  String get lampPresetPurpleScene;

  /// No description provided for @lampPresetRed.
  ///
  /// In en, this message translates to:
  /// **'Red'**
  String get lampPresetRed;

  /// No description provided for @lampPresetAmber.
  ///
  /// In en, this message translates to:
  /// **'Amber'**
  String get lampPresetAmber;

  /// No description provided for @puzzleTabDaily.
  ///
  /// In en, this message translates to:
  /// **'Daily'**
  String get puzzleTabDaily;

  /// No description provided for @puzzleTabLibrary.
  ///
  /// In en, this message translates to:
  /// **'Library'**
  String get puzzleTabLibrary;

  /// No description provided for @puzzleTabTraining.
  ///
  /// In en, this message translates to:
  /// **'Training'**
  String get puzzleTabTraining;

  /// No description provided for @puzzleBackLiveTooltip.
  ///
  /// In en, this message translates to:
  /// **'Back to live game'**
  String get puzzleBackLiveTooltip;

  /// No description provided for @puzzlePoolEmptySnack.
  ///
  /// In en, this message translates to:
  /// **'No positions in this pool mode — switch to Mixed or add library items.'**
  String get puzzlePoolEmptySnack;

  /// No description provided for @puzzleLibraryEmptyHint.
  ///
  /// In en, this message translates to:
  /// **'No items yet — use the form above or Add from the daily puzzle.'**
  String get puzzleLibraryEmptyHint;

  /// No description provided for @puzzleLibraryRemoveTitle.
  ///
  /// In en, this message translates to:
  /// **'Remove from library?'**
  String get puzzleLibraryRemoveTitle;

  /// No description provided for @puzzleLibraryRemoveBody.
  ///
  /// In en, this message translates to:
  /// **'Remove “{title}” from your puzzle library? This cannot be undone.'**
  String puzzleLibraryRemoveBody(String title);

  /// No description provided for @puzzleLibraryRemoveConfirm.
  ///
  /// In en, this message translates to:
  /// **'Remove'**
  String get puzzleLibraryRemoveConfirm;

  /// No description provided for @puzzleLoadDaily.
  ///
  /// In en, this message translates to:
  /// **'Load daily puzzle'**
  String get puzzleLoadDaily;

  /// No description provided for @puzzleRefreshDaily.
  ///
  /// In en, this message translates to:
  /// **'Refresh daily puzzle'**
  String get puzzleRefreshDaily;

  /// No description provided for @puzzleAlreadyInLibrary.
  ///
  /// In en, this message translates to:
  /// **'Already in library'**
  String get puzzleAlreadyInLibrary;

  /// No description provided for @puzzleSavedToLibraryBanner.
  ///
  /// In en, this message translates to:
  /// **'Saved to library.'**
  String get puzzleSavedToLibraryBanner;

  /// No description provided for @puzzleLibNameLabel.
  ///
  /// In en, this message translates to:
  /// **'Title'**
  String get puzzleLibNameLabel;

  /// No description provided for @puzzleLibFenLabel.
  ///
  /// In en, this message translates to:
  /// **'FEN'**
  String get puzzleLibFenLabel;

  /// No description provided for @puzzleGoPlaySandbox.
  ///
  /// In en, this message translates to:
  /// **'Sandbox — Play tab.'**
  String get puzzleGoPlaySandbox;

  /// No description provided for @puzzleGoPlayPuzzle.
  ///
  /// In en, this message translates to:
  /// **'Puzzle ({title}) — Play tab.'**
  String puzzleGoPlayPuzzle(String title);

  /// No description provided for @puzzleSideBlackMove.
  ///
  /// In en, this message translates to:
  /// **'Black to move'**
  String get puzzleSideBlackMove;

  /// No description provided for @puzzleSideWhiteMove.
  ///
  /// In en, this message translates to:
  /// **'White to move'**
  String get puzzleSideWhiteMove;

  /// No description provided for @puzzleSideUnknown.
  ///
  /// In en, this message translates to:
  /// **'Side to move: ?'**
  String get puzzleSideUnknown;

  /// No description provided for @puzzleTrainingPoolStats.
  ///
  /// In en, this message translates to:
  /// **'{poolN} positions · {sessions} rounds started'**
  String puzzleTrainingPoolStats(int poolN, int sessions);

  /// No description provided for @puzzleThemeMateIn1.
  ///
  /// In en, this message translates to:
  /// **'Mate in 1'**
  String get puzzleThemeMateIn1;

  /// No description provided for @puzzleThemeAdvancedPawn.
  ///
  /// In en, this message translates to:
  /// **'Advanced pawn'**
  String get puzzleThemeAdvancedPawn;

  /// No description provided for @puzzleThemeOpening.
  ///
  /// In en, this message translates to:
  /// **'Opening'**
  String get puzzleThemeOpening;

  /// No description provided for @puzzleThemeKingsideAttack.
  ///
  /// In en, this message translates to:
  /// **'Kingside attack'**
  String get puzzleThemeKingsideAttack;

  /// No description provided for @puzzleThemeMiddlegame.
  ///
  /// In en, this message translates to:
  /// **'Middlegame'**
  String get puzzleThemeMiddlegame;

  /// No description provided for @puzzleThemeTactic.
  ///
  /// In en, this message translates to:
  /// **'Tactics'**
  String get puzzleThemeTactic;

  /// No description provided for @setupWizardErrMissingFen.
  ///
  /// In en, this message translates to:
  /// **'FEN is missing.'**
  String get setupWizardErrMissingFen;

  /// No description provided for @setupWizardErrConnectBoard.
  ///
  /// In en, this message translates to:
  /// **'Connect the board via Wi‑Fi or Bluetooth (demo mode does not support this tutorial).'**
  String get setupWizardErrConnectBoard;

  /// No description provided for @setupWizardErrNoSteps.
  ///
  /// In en, this message translates to:
  /// **'Could not build steps from this FEN.'**
  String get setupWizardErrNoSteps;

  /// No description provided for @setupWizardErrGeneric.
  ///
  /// In en, this message translates to:
  /// **'Error'**
  String get setupWizardErrGeneric;

  /// No description provided for @setupWizardTitleStandard.
  ///
  /// In en, this message translates to:
  /// **'Starting position'**
  String get setupWizardTitleStandard;

  /// No description provided for @setupWizardTitleFromFen.
  ///
  /// In en, this message translates to:
  /// **'Setup from FEN'**
  String get setupWizardTitleFromFen;

  /// No description provided for @setupWizardDoneStandard.
  ///
  /// In en, this message translates to:
  /// **'The board confirmed the starting position.'**
  String get setupWizardDoneStandard;

  /// No description provided for @setupWizardDoneFenLoaded.
  ///
  /// In en, this message translates to:
  /// **'The position was loaded on the board.'**
  String get setupWizardDoneFenLoaded;

  /// No description provided for @setupWizardDoneNoLoad.
  ///
  /// In en, this message translates to:
  /// **'Wizard finished.'**
  String get setupWizardDoneNoLoad;

  /// No description provided for @setupWizardBtnDone.
  ///
  /// In en, this message translates to:
  /// **'Done'**
  String get setupWizardBtnDone;

  /// No description provided for @setupWizardStepProgress.
  ///
  /// In en, this message translates to:
  /// **'Step {current} / {total}'**
  String setupWizardStepProgress(int current, int total);

  /// No description provided for @setupWizardPlacePieceOn.
  ///
  /// In en, this message translates to:
  /// **'Place the piece on {square}'**
  String setupWizardPlacePieceOn(String square);

  /// No description provided for @setupWizardPieceDetail.
  ///
  /// In en, this message translates to:
  /// **'{pieceName} → {square}'**
  String setupWizardPieceDetail(String pieceName, String square);

  /// No description provided for @setupWizardLedHint.
  ///
  /// In en, this message translates to:
  /// **'The board LED shows the target square. Progress is confirmed by the matrix sensor or the correct piece in the snapshot.'**
  String get setupWizardLedHint;

  /// No description provided for @setupWizardSkipStep.
  ///
  /// In en, this message translates to:
  /// **'Skip step'**
  String get setupWizardSkipStep;

  /// No description provided for @boardSetupWhiteKing.
  ///
  /// In en, this message translates to:
  /// **'white king'**
  String get boardSetupWhiteKing;

  /// No description provided for @boardSetupBlackKing.
  ///
  /// In en, this message translates to:
  /// **'black king'**
  String get boardSetupBlackKing;

  /// No description provided for @boardSetupWhiteQueen.
  ///
  /// In en, this message translates to:
  /// **'white queen'**
  String get boardSetupWhiteQueen;

  /// No description provided for @boardSetupBlackQueen.
  ///
  /// In en, this message translates to:
  /// **'black queen'**
  String get boardSetupBlackQueen;

  /// No description provided for @boardSetupWhiteRook.
  ///
  /// In en, this message translates to:
  /// **'white rook'**
  String get boardSetupWhiteRook;

  /// No description provided for @boardSetupBlackRook.
  ///
  /// In en, this message translates to:
  /// **'black rook'**
  String get boardSetupBlackRook;

  /// No description provided for @boardSetupWhiteBishop.
  ///
  /// In en, this message translates to:
  /// **'white bishop'**
  String get boardSetupWhiteBishop;

  /// No description provided for @boardSetupBlackBishop.
  ///
  /// In en, this message translates to:
  /// **'black bishop'**
  String get boardSetupBlackBishop;

  /// No description provided for @boardSetupWhiteKnight.
  ///
  /// In en, this message translates to:
  /// **'white knight'**
  String get boardSetupWhiteKnight;

  /// No description provided for @boardSetupBlackKnight.
  ///
  /// In en, this message translates to:
  /// **'black knight'**
  String get boardSetupBlackKnight;

  /// No description provided for @boardSetupWhitePawn.
  ///
  /// In en, this message translates to:
  /// **'white pawn'**
  String get boardSetupWhitePawn;

  /// No description provided for @boardSetupBlackPawn.
  ///
  /// In en, this message translates to:
  /// **'black pawn'**
  String get boardSetupBlackPawn;

  /// No description provided for @boardSetupPieceGeneric.
  ///
  /// In en, this message translates to:
  /// **'piece'**
  String get boardSetupPieceGeneric;

  /// No description provided for @gameRemoteEmptySquareSnack.
  ///
  /// In en, this message translates to:
  /// **'There is no piece on this square.'**
  String get gameRemoteEmptySquareSnack;

  /// No description provided for @gameRemoteEmptySquareHud.
  ///
  /// In en, this message translates to:
  /// **'Empty square'**
  String get gameRemoteEmptySquareHud;

  /// No description provided for @gameRemoteWrongTurnSnack.
  ///
  /// In en, this message translates to:
  /// **'Pick a piece of the side that is on move.'**
  String get gameRemoteWrongTurnSnack;

  /// No description provided for @gameRemoteWrongTurnHud.
  ///
  /// In en, this message translates to:
  /// **'Not your turn'**
  String get gameRemoteWrongTurnHud;

  /// No description provided for @gameRemoteGameFinished.
  ///
  /// In en, this message translates to:
  /// **'This game is already over.'**
  String get gameRemoteGameFinished;

  /// No description provided for @gameRemotePositionError.
  ///
  /// In en, this message translates to:
  /// **'Could not validate the move from the current board data.'**
  String get gameRemotePositionError;

  /// No description provided for @gamePromotionPickTitle.
  ///
  /// In en, this message translates to:
  /// **'Promotion — choose a piece'**
  String get gamePromotionPickTitle;

  /// No description provided for @gamePromotionQueen.
  ///
  /// In en, this message translates to:
  /// **'Queen'**
  String get gamePromotionQueen;

  /// No description provided for @gamePromotionRook.
  ///
  /// In en, this message translates to:
  /// **'Rook'**
  String get gamePromotionRook;

  /// No description provided for @gamePromotionBishop.
  ///
  /// In en, this message translates to:
  /// **'Bishop'**
  String get gamePromotionBishop;

  /// No description provided for @gamePromotionKnight.
  ///
  /// In en, this message translates to:
  /// **'Knight'**
  String get gamePromotionKnight;

  /// No description provided for @gamePromotionFailedSnack.
  ///
  /// In en, this message translates to:
  /// **'Promotion failed'**
  String get gamePromotionFailedSnack;

  /// No description provided for @semanticsChessBoard.
  ///
  /// In en, this message translates to:
  /// **'Chessboard, eight by eight squares. Tap to select squares.'**
  String get semanticsChessBoard;

  /// No description provided for @moveHistoryEmpty.
  ///
  /// In en, this message translates to:
  /// **'Move history is empty yet.'**
  String get moveHistoryEmpty;

  /// No description provided for @moveHistoryCurrentPosition.
  ///
  /// In en, this message translates to:
  /// **'Live position — current game'**
  String get moveHistoryCurrentPosition;

  /// No description provided for @moveHistoryPieceSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Piece: {piece}'**
  String moveHistoryPieceSubtitle(String piece);

  /// No description provided for @bundledPuzzleMateQg8Title.
  ///
  /// In en, this message translates to:
  /// **'Queen mate'**
  String get bundledPuzzleMateQg8Title;

  /// No description provided for @bundledPuzzleMateQg8Subtitle.
  ///
  /// In en, this message translates to:
  /// **'White · mate in 1'**
  String get bundledPuzzleMateQg8Subtitle;

  /// No description provided for @bundledPuzzleBackRankTitle.
  ///
  /// In en, this message translates to:
  /// **'Back rank'**
  String get bundledPuzzleBackRankTitle;

  /// No description provided for @bundledPuzzleBackRankSubtitle.
  ///
  /// In en, this message translates to:
  /// **'White · rook mate'**
  String get bundledPuzzleBackRankSubtitle;

  /// No description provided for @bundledPuzzleRookCornerTitle.
  ///
  /// In en, this message translates to:
  /// **'Rook vs king'**
  String get bundledPuzzleRookCornerTitle;

  /// No description provided for @bundledPuzzleRookCornerSubtitle.
  ///
  /// In en, this message translates to:
  /// **'White · endgame'**
  String get bundledPuzzleRookCornerSubtitle;

  /// No description provided for @bundledPuzzleBlackMateTitle.
  ///
  /// In en, this message translates to:
  /// **'Black to move'**
  String get bundledPuzzleBlackMateTitle;

  /// No description provided for @bundledPuzzleBlackMateSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Black · mate in 1'**
  String get bundledPuzzleBlackMateSubtitle;

  /// No description provided for @bundledPuzzlePromotionTitle.
  ///
  /// In en, this message translates to:
  /// **'Pawn advance'**
  String get bundledPuzzlePromotionTitle;

  /// No description provided for @bundledPuzzlePromotionSubtitle.
  ///
  /// In en, this message translates to:
  /// **'White · promotion'**
  String get bundledPuzzlePromotionSubtitle;

  /// No description provided for @bundledPuzzleTwoRooksTitle.
  ///
  /// In en, this message translates to:
  /// **'Two rooks'**
  String get bundledPuzzleTwoRooksTitle;

  /// No description provided for @bundledPuzzleTwoRooksSubtitle.
  ///
  /// In en, this message translates to:
  /// **'White · two rooks'**
  String get bundledPuzzleTwoRooksSubtitle;

  /// No description provided for @settingsTileSmartHomeTitle.
  ///
  /// In en, this message translates to:
  /// **'Smart home (MQTT)'**
  String get settingsTileSmartHomeTitle;

  /// No description provided for @settingsTileSmartHomeSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Home Assistant & MQTT'**
  String get settingsTileSmartHomeSubtitle;

  /// No description provided for @settingsHaMqttOpenButton.
  ///
  /// In en, this message translates to:
  /// **'Home Assistant & MQTT (guide + form)'**
  String get settingsHaMqttOpenButton;

  /// No description provided for @settingsTileBoardLightTitle.
  ///
  /// In en, this message translates to:
  /// **'Board light'**
  String get settingsTileBoardLightTitle;

  /// No description provided for @settingsTileBoardLightSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Color, brightness, scenes, auto-off'**
  String get settingsTileBoardLightSubtitle;

  /// No description provided for @settingsTileModulesTitle.
  ///
  /// In en, this message translates to:
  /// **'Modules & learning'**
  String get settingsTileModulesTitle;

  /// No description provided for @settingsTileModulesSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Tour, puzzles, profile, progress, help'**
  String get settingsTileModulesSubtitle;

  /// No description provided for @settingsNavAppTour.
  ///
  /// In en, this message translates to:
  /// **'App tour (onboarding)'**
  String get settingsNavAppTour;

  /// No description provided for @onboardingYourNameTitle.
  ///
  /// In en, this message translates to:
  /// **'What should we call you?'**
  String get onboardingYourNameTitle;

  /// No description provided for @onboardingYourNameSubtitle.
  ///
  /// In en, this message translates to:
  /// **'This name appears in your profile and shared summaries.'**
  String get onboardingYourNameSubtitle;

  /// No description provided for @onboardingNameHint.
  ///
  /// In en, this message translates to:
  /// **'Your name'**
  String get onboardingNameHint;

  /// No description provided for @onboardingPermissionsTitle.
  ///
  /// In en, this message translates to:
  /// **'Allow access'**
  String get onboardingPermissionsTitle;

  /// No description provided for @onboardingPermissionsSubtitle.
  ///
  /// In en, this message translates to:
  /// **'You can turn features on gradually — the app works best when these are allowed.'**
  String get onboardingPermissionsSubtitle;

  /// No description provided for @onboardingPermPhotosTitle.
  ///
  /// In en, this message translates to:
  /// **'Photos'**
  String get onboardingPermPhotosTitle;

  /// No description provided for @onboardingPermPhotosBody.
  ///
  /// In en, this message translates to:
  /// **'Pick a profile picture from your gallery.'**
  String get onboardingPermPhotosBody;

  /// No description provided for @onboardingPermPhotosAllow.
  ///
  /// In en, this message translates to:
  /// **'Allow photos'**
  String get onboardingPermPhotosAllow;

  /// No description provided for @onboardingPermBleTitle.
  ///
  /// In en, this message translates to:
  /// **'Bluetooth'**
  String get onboardingPermBleTitle;

  /// No description provided for @onboardingPermBleBody.
  ///
  /// In en, this message translates to:
  /// **'Find and connect to your CzechMate board.'**
  String get onboardingPermBleBody;

  /// No description provided for @onboardingPermBleAllow.
  ///
  /// In en, this message translates to:
  /// **'Allow Bluetooth'**
  String get onboardingPermBleAllow;

  /// No description provided for @onboardingPermWifiTitle.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi'**
  String get onboardingPermWifiTitle;

  /// No description provided for @onboardingPermWifiBodyAndroid.
  ///
  /// In en, this message translates to:
  /// **'Lets the app read your current network name to pre-fill Wi‑Fi setup (no rough location on Android 13+).'**
  String get onboardingPermWifiBodyAndroid;

  /// No description provided for @onboardingPermWifiBodyIos.
  ///
  /// In en, this message translates to:
  /// **'You’ll join the board’s network in Settings when connecting — no extra step here.'**
  String get onboardingPermWifiBodyIos;

  /// No description provided for @onboardingPermWifiAllow.
  ///
  /// In en, this message translates to:
  /// **'Allow Wi‑Fi access'**
  String get onboardingPermWifiAllow;

  /// No description provided for @onboardingPermGrantedShort.
  ///
  /// In en, this message translates to:
  /// **'Allowed'**
  String get onboardingPermGrantedShort;

  /// No description provided for @onboardingPermDeniedSnack.
  ///
  /// In en, this message translates to:
  /// **'You can change this later in system Settings.'**
  String get onboardingPermDeniedSnack;

  /// No description provided for @onboardingOpenSettings.
  ///
  /// In en, this message translates to:
  /// **'Open Settings'**
  String get onboardingOpenSettings;

  /// No description provided for @onboardingPermissionsDesktopBody.
  ///
  /// In en, this message translates to:
  /// **'Bluetooth and network are handled when you connect to the board from the app.'**
  String get onboardingPermissionsDesktopBody;

  /// No description provided for @settingsNavChessPuzzles.
  ///
  /// In en, this message translates to:
  /// **'Chess puzzles'**
  String get settingsNavChessPuzzles;

  /// No description provided for @settingsNavProfileElo.
  ///
  /// In en, this message translates to:
  /// **'Profile & puzzle Elo'**
  String get settingsNavProfileElo;

  /// No description provided for @settingsNavProgress.
  ///
  /// In en, this message translates to:
  /// **'Progress (learning & stats)'**
  String get settingsNavProgress;

  /// No description provided for @settingsNavHelp.
  ///
  /// In en, this message translates to:
  /// **'Help'**
  String get settingsNavHelp;

  /// No description provided for @settingsTileNvsDiagTitle.
  ///
  /// In en, this message translates to:
  /// **'Board memory & diagnostics'**
  String get settingsTileNvsDiagTitle;

  /// No description provided for @settingsTileNvsDiagSubtitle.
  ///
  /// In en, this message translates to:
  /// **'NVS rules, developer tools'**
  String get settingsTileNvsDiagSubtitle;

  /// No description provided for @settingsNavBoardNvs.
  ///
  /// In en, this message translates to:
  /// **'Board NVS rules (LED, bot)'**
  String get settingsNavBoardNvs;

  /// No description provided for @settingsNavDeveloperDiag.
  ///
  /// In en, this message translates to:
  /// **'Developer diagnostics'**
  String get settingsNavDeveloperDiag;

  /// No description provided for @settingsTileAboutTitle.
  ///
  /// In en, this message translates to:
  /// **'About'**
  String get settingsTileAboutTitle;

  /// No description provided for @settingsTileAboutSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Version, privacy, system'**
  String get settingsTileAboutSubtitle;

  /// No description provided for @settingsAboutVersionLoading.
  ///
  /// In en, this message translates to:
  /// **'Loading version…'**
  String get settingsAboutVersionLoading;

  /// No description provided for @settingsAboutVersionLine.
  ///
  /// In en, this message translates to:
  /// **'Version {version} ({build}) • Tap “Settings” in the title bar 7× to unlock developer mode.'**
  String settingsAboutVersionLine(String version, String build);

  /// No description provided for @settingsPrivacyTitle.
  ///
  /// In en, this message translates to:
  /// **'Privacy'**
  String get settingsPrivacyTitle;

  /// No description provided for @settingsPrivacyBody.
  ///
  /// In en, this message translates to:
  /// **'This build does not send analytics. Traffic goes only to your board on the local network or over Bluetooth.'**
  String get settingsPrivacyBody;

  /// No description provided for @settingsIcloudTitle.
  ///
  /// In en, this message translates to:
  /// **'iCloud'**
  String get settingsIcloudTitle;

  /// No description provided for @settingsIcloudBody.
  ///
  /// In en, this message translates to:
  /// **'Optional puzzle sync via iCloud (CloudKit) is planned for a future release; this build does not sync.'**
  String get settingsIcloudBody;

  /// No description provided for @settingsLiveActivityTitle.
  ///
  /// In en, this message translates to:
  /// **'Clock status outside the app'**
  String get settingsLiveActivityTitle;

  /// No description provided for @settingsLiveActivitySubtitle.
  ///
  /// In en, this message translates to:
  /// **'iPhone: Live Activity (Lock Screen / Dynamic Island, iOS 16.2+). Android: ongoing notification (Android 13+ may require enabling notifications). Enable from the game screen.'**
  String get settingsLiveActivitySubtitle;

  /// No description provided for @settingsLiveActivityIosDisabledSnack.
  ///
  /// In en, this message translates to:
  /// **'Live Activities are disabled system-wide. Enable them in Settings → czechmate → Live Activities.'**
  String get settingsLiveActivityIosDisabledSnack;

  /// No description provided for @settingsWearMirrorTitle.
  ///
  /// In en, this message translates to:
  /// **'Mirror clock on Wear OS'**
  String get settingsWearMirrorTitle;

  /// No description provided for @settingsWearMirrorSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Google Data Layer — same state as the ongoing notification; requires a paired Wear OS watch.'**
  String get settingsWearMirrorSubtitle;

  /// No description provided for @settingsWatchMirrorTitle.
  ///
  /// In en, this message translates to:
  /// **'Mirror clock on Apple Watch'**
  String get settingsWatchMirrorTitle;

  /// No description provided for @settingsWatchMirrorSubtitle.
  ///
  /// In en, this message translates to:
  /// **'WatchConnectivity — same state as Live Activity; basic UI on the watch with Pause/Resume.'**
  String get settingsWatchMirrorSubtitle;

  /// No description provided for @settingsFactoryTileTitle.
  ///
  /// In en, this message translates to:
  /// **'Board factory reset'**
  String get settingsFactoryTileTitle;

  /// No description provided for @settingsFactoryTileSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Clears ESP NVS — device becomes access point'**
  String get settingsFactoryTileSubtitle;

  /// No description provided for @settingsFactoryDialogTitle.
  ///
  /// In en, this message translates to:
  /// **'Factory reset'**
  String get settingsFactoryDialogTitle;

  /// No description provided for @settingsFactoryDialogBody.
  ///
  /// In en, this message translates to:
  /// **'Erase all board NVS (Wi‑Fi, passwords, MQTT, preferences)? The device will restart as a Wi‑Fi access point.'**
  String get settingsFactoryDialogBody;

  /// No description provided for @settingsFactoryErase.
  ///
  /// In en, this message translates to:
  /// **'Erase'**
  String get settingsFactoryErase;

  /// No description provided for @settingsFactoryCommandSent.
  ///
  /// In en, this message translates to:
  /// **'Command sent.'**
  String get settingsFactoryCommandSent;

  /// No description provided for @settingsFactoryError.
  ///
  /// In en, this message translates to:
  /// **'Error: {error}'**
  String settingsFactoryError(String error);

  /// No description provided for @settingsFactoryRunButton.
  ///
  /// In en, this message translates to:
  /// **'Run board factory reset'**
  String get settingsFactoryRunButton;

  /// No description provided for @settingsCoachOllamaUrlHelper.
  ///
  /// In en, this message translates to:
  /// **'Enter a URL ending with /v1 (e.g. :11434/v1). Port 11434 without /v1 used to fail — the app now appends /v1 for 11434.'**
  String get settingsCoachOllamaUrlHelper;

  /// No description provided for @settingsCoachOpenAiBaseLabel.
  ///
  /// In en, this message translates to:
  /// **'OpenAI API base'**
  String get settingsCoachOpenAiBaseLabel;

  /// No description provided for @settingsCoachModelNameLabel.
  ///
  /// In en, this message translates to:
  /// **'Model name'**
  String get settingsCoachModelNameLabel;

  /// No description provided for @settingsCoachEnterGeminiModelSnack.
  ///
  /// In en, this message translates to:
  /// **'Enter a Google AI model id (or pick a preset).'**
  String get settingsCoachEnterGeminiModelSnack;

  /// No description provided for @settingsCoachSavedSnack.
  ///
  /// In en, this message translates to:
  /// **'Coach settings saved.'**
  String get settingsCoachSavedSnack;

  /// No description provided for @settingsCoachSaveButton.
  ///
  /// In en, this message translates to:
  /// **'Save coach settings'**
  String get settingsCoachSaveButton;

  /// No description provided for @settingsCoachGoogleKeyButton.
  ///
  /// In en, this message translates to:
  /// **'Get Google AI key'**
  String get settingsCoachGoogleKeyButton;

  /// No description provided for @settingsCoachGroqConsoleButton.
  ///
  /// In en, this message translates to:
  /// **'Groq console'**
  String get settingsCoachGroqConsoleButton;

  /// No description provided for @settingsCoachXaiConsoleButton.
  ///
  /// In en, this message translates to:
  /// **'xAI console'**
  String get settingsCoachXaiConsoleButton;

  /// No description provided for @boardNvsRefreshTooltip.
  ///
  /// In en, this message translates to:
  /// **'Refresh from board'**
  String get boardNvsRefreshTooltip;

  /// No description provided for @boardNvsFetchFailedFallback.
  ///
  /// In en, this message translates to:
  /// **'Could not load data from the board. Tap ↻ to retry.'**
  String get boardNvsFetchFailedFallback;

  /// No description provided for @boardNvsMergeHint.
  ///
  /// In en, this message translates to:
  /// **'When saving, the app merges Stockfish depth ({depth}) and move-eval toggle ({eval}) into the NVS payload (same as iOS).'**
  String boardNvsMergeHint(String depth, String eval);

  /// No description provided for @boardNvsEvalOn.
  ///
  /// In en, this message translates to:
  /// **'on'**
  String get boardNvsEvalOn;

  /// No description provided for @boardNvsEvalOff.
  ///
  /// In en, this message translates to:
  /// **'off'**
  String get boardNvsEvalOff;

  /// No description provided for @boardNvsHintTierFootnote.
  ///
  /// In en, this message translates to:
  /// **'H1–H3 in the phone app is separate from on-board LED level; POST above also forwards depth/eval from app diagnostics.'**
  String get boardNvsHintTierFootnote;

  /// No description provided for @boardNvsFooterMock.
  ///
  /// In en, this message translates to:
  /// **'Demo board: NVS write does not reach real hardware.'**
  String get boardNvsFooterMock;

  /// No description provided for @boardNvsFooterWifiActive.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi session active — Save sends bot settings to ESP NVS.'**
  String get boardNvsFooterWifiActive;

  /// No description provided for @boardNvsFooterBle.
  ///
  /// In en, this message translates to:
  /// **'BLE: firmware may accept NVS/UI blob; verify status on the board web UI.'**
  String get boardNvsFooterBle;

  /// No description provided for @boardNvsFooterGeneric.
  ///
  /// In en, this message translates to:
  /// **'For reliable bot NVS writes, connect the board (Wi‑Fi recommended).'**
  String get boardNvsFooterGeneric;

  /// No description provided for @boardNvsHttpBleExplain.
  ///
  /// In en, this message translates to:
  /// **'Bluetooth is connected, but loading NVS from this screen uses HTTP. Save a valid board URL in Settings (STA IP, e.g. http://192.168.x.x), or activate a Wi‑Fi session to that IP.'**
  String get boardNvsHttpBleExplain;

  /// No description provided for @boardNvsHttpWifiNoUrl.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi session without a valid base URL. Reconnect from Play → board manager.'**
  String get boardNvsHttpWifiNoUrl;

  /// No description provided for @boardNvsHttpMissingUrl.
  ///
  /// In en, this message translates to:
  /// **'Missing valid board HTTP URL. In Settings fill “Default board URL” (full http://…) and try again (↻).'**
  String get boardNvsHttpMissingUrl;

  /// No description provided for @boardHttpMissingUrlBody.
  ///
  /// In en, this message translates to:
  /// **'The board HTTP address is missing or invalid. In Settings, save a full URL (e.g. http://192.168.4.1 or your board STA IP). Bluetooth can still work without this URL for live play.'**
  String get boardHttpMissingUrlBody;

  /// No description provided for @boardHttpFailBle.
  ///
  /// In en, this message translates to:
  /// **'Bluetooth is connected, but the HTTP request failed (wrong URL, timeout, or web lock).'**
  String get boardHttpFailBle;

  /// No description provided for @boardHttpFailWifi.
  ///
  /// In en, this message translates to:
  /// **'Wi‑Fi session active — verify the board URL and that the board responds.'**
  String get boardHttpFailWifi;

  /// No description provided for @boardHttpFailMock.
  ///
  /// In en, this message translates to:
  /// **'Demo board — HTTP to hardware does not apply.'**
  String get boardHttpFailMock;

  /// No description provided for @boardHttpFailNone.
  ///
  /// In en, this message translates to:
  /// **'No active connection to the board.'**
  String get boardHttpFailNone;

  /// No description provided for @boardHttpFailDetail.
  ///
  /// In en, this message translates to:
  /// **'{link} Detail: {detail}'**
  String boardHttpFailDetail(String link, String detail);

  /// No description provided for @boardHttpErrGeneric.
  ///
  /// In en, this message translates to:
  /// **'Board communication error: {error}'**
  String boardHttpErrGeneric(String error);

  /// No description provided for @progressLevelBeginner.
  ///
  /// In en, this message translates to:
  /// **'Beginner'**
  String get progressLevelBeginner;

  /// No description provided for @progressLevelIntermediate.
  ///
  /// In en, this message translates to:
  /// **'Intermediate'**
  String get progressLevelIntermediate;

  /// No description provided for @progressLevelAdvanced.
  ///
  /// In en, this message translates to:
  /// **'Advanced'**
  String get progressLevelAdvanced;

  /// No description provided for @progressLevelExpert.
  ///
  /// In en, this message translates to:
  /// **'Expert'**
  String get progressLevelExpert;

  /// No description provided for @progressLevelNumber.
  ///
  /// In en, this message translates to:
  /// **'Level {n}'**
  String progressLevelNumber(int n);

  /// No description provided for @progressNoSessionYet.
  ///
  /// In en, this message translates to:
  /// **'No active session yet'**
  String get progressNoSessionYet;

  /// No description provided for @analysisIntroEvalHint.
  ///
  /// In en, this message translates to:
  /// **'Move evaluation and chart — chess-api or custom URL in Settings (developer).'**
  String get analysisIntroEvalHint;

  /// No description provided for @analysisHalfMoveOrderHint.
  ///
  /// In en, this message translates to:
  /// **'Each row is one half-move in order — white first, then black.'**
  String get analysisHalfMoveOrderHint;

  /// No description provided for @analysisNoBoardPositionHint.
  ///
  /// In en, this message translates to:
  /// **'On the Play tab, connect the chessboard. Below you can analyze a custom FEN.'**
  String get analysisNoBoardPositionHint;

  /// No description provided for @analysisChartDisabledBoardSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Turn on the switch below or in Settings → Board appearance.'**
  String get analysisChartDisabledBoardSubtitle;

  /// No description provided for @analysisOverviewSubtitleEval.
  ///
  /// In en, this message translates to:
  /// **'Position eval after each move (White perspective: + favors White)'**
  String get analysisOverviewSubtitleEval;

  /// No description provided for @analysisChartFillHintLiveEvalOn.
  ///
  /// In en, this message translates to:
  /// **'The chart fills after moves once Stockfish evaluates (play on Play with internet).'**
  String get analysisChartFillHintLiveEvalOn;

  /// No description provided for @analysisChartFillHintLiveEvalOff.
  ///
  /// In en, this message translates to:
  /// **'Turn on automatic evaluation to see the curve here.'**
  String get analysisChartFillHintLiveEvalOff;

  /// No description provided for @analysisClearEvalData.
  ///
  /// In en, this message translates to:
  /// **'Clear chart and saved move evaluations'**
  String get analysisClearEvalData;

  /// No description provided for @analysisMoveQualitySideLast3.
  ///
  /// In en, this message translates to:
  /// **'Last 3 moves by side'**
  String get analysisMoveQualitySideLast3;

  /// No description provided for @analysisMoveQualitySideLast10.
  ///
  /// In en, this message translates to:
  /// **'Last 10 moves by side'**
  String get analysisMoveQualitySideLast10;

  /// No description provided for @analysisMoveQualityFullGame.
  ///
  /// In en, this message translates to:
  /// **'Full game'**
  String get analysisMoveQualityFullGame;

  /// No description provided for @analysisSecondLineComputing.
  ///
  /// In en, this message translates to:
  /// **'Computing…'**
  String get analysisSecondLineComputing;

  /// No description provided for @analysisSecondLineLoadButton.
  ///
  /// In en, this message translates to:
  /// **'Load second line (from session FEN)'**
  String get analysisSecondLineLoadButton;

  /// No description provided for @analysisSecondLineSameMove.
  ///
  /// In en, this message translates to:
  /// **'Both depths agree on {from}→{to}.'**
  String analysisSecondLineSameMove(String from, String to);

  /// No description provided for @analysisSecondLineDualMoves.
  ///
  /// In en, this message translates to:
  /// **'1) {f1}→{t1} · 2) {f2}→{t2}{suffix}'**
  String analysisSecondLineDualMoves(
      String f1, String t1, String f2, String t2, String suffix);

  /// No description provided for @analysisSecondLineEvalApprox.
  ///
  /// In en, this message translates to:
  /// **' · eval ≈ {pawns} pawns.'**
  String analysisSecondLineEvalApprox(String pawns);

  /// No description provided for @analysisFreeAnalyzing.
  ///
  /// In en, this message translates to:
  /// **'Computing…'**
  String get analysisFreeAnalyzing;

  /// No description provided for @analysisBestMoveLine.
  ///
  /// In en, this message translates to:
  /// **'Best move: {from}–{to}{evalSuffix}{extra}'**
  String analysisBestMoveLine(
      String from, String to, String evalSuffix, String extra);

  /// No description provided for @analysisAvgLossCp.
  ///
  /// In en, this message translates to:
  /// **' · avg loss {cp} cp'**
  String analysisAvgLossCp(String cp);

  /// No description provided for @analysisFenFieldLabel.
  ///
  /// In en, this message translates to:
  /// **'FEN'**
  String get analysisFenFieldLabel;

  /// No description provided for @gameSandboxLoadPositionFirst.
  ///
  /// In en, this message translates to:
  /// **'Load a position first (connect the board or use demo).'**
  String get gameSandboxLoadPositionFirst;

  /// No description provided for @gameSandboxSelectPiece.
  ///
  /// In en, this message translates to:
  /// **'Select a piece'**
  String get gameSandboxSelectPiece;

  /// No description provided for @gameSandboxWrongSide.
  ///
  /// In en, this message translates to:
  /// **'Not this side to move'**
  String get gameSandboxWrongSide;

  /// No description provided for @gameSandboxIllegalMove.
  ///
  /// In en, this message translates to:
  /// **'Illegal move'**
  String get gameSandboxIllegalMove;

  /// No description provided for @gameSandboxIllegalMoveSandbox.
  ///
  /// In en, this message translates to:
  /// **'Illegal move (sandbox)'**
  String get gameSandboxIllegalMoveSandbox;

  /// No description provided for @gameSandboxCouldNotLoadFen.
  ///
  /// In en, this message translates to:
  /// **'Could not load FEN.'**
  String get gameSandboxCouldNotLoadFen;

  /// No description provided for @gameSandboxPositionRestored.
  ///
  /// In en, this message translates to:
  /// **'Position restored — try again.'**
  String get gameSandboxPositionRestored;

  /// No description provided for @puzzleSuccessLineWithElo.
  ///
  /// In en, this message translates to:
  /// **'Nice! Correct line · +{delta} Elo'**
  String puzzleSuccessLineWithElo(int delta);

  /// No description provided for @puzzleSuccessLine.
  ///
  /// In en, this message translates to:
  /// **'Nice! Correct line.'**
  String get puzzleSuccessLine;

  /// No description provided for @puzzleWrongResetting.
  ///
  /// In en, this message translates to:
  /// **'Not the solution — resetting the position.'**
  String get puzzleWrongResetting;

  /// No description provided for @gameControlWhiteBottom.
  ///
  /// In en, this message translates to:
  /// **'White bottom'**
  String get gameControlWhiteBottom;

  /// No description provided for @gameControlBlackBottom.
  ///
  /// In en, this message translates to:
  /// **'Black bottom'**
  String get gameControlBlackBottom;

  /// No description provided for @gameControlCoordsOn.
  ///
  /// In en, this message translates to:
  /// **'Coords on'**
  String get gameControlCoordsOn;

  /// No description provided for @gameControlCoordsOff.
  ///
  /// In en, this message translates to:
  /// **'Coords off'**
  String get gameControlCoordsOff;

  /// No description provided for @gameControlBoardSizeHint.
  ///
  /// In en, this message translates to:
  /// **'Board size: Settings → Board appearance.'**
  String get gameControlBoardSizeHint;

  /// No description provided for @gameControlMoveHint.
  ///
  /// In en, this message translates to:
  /// **'Move hint'**
  String get gameControlMoveHint;

  /// No description provided for @gameControlPanelSubtitle.
  ///
  /// In en, this message translates to:
  /// **'Display · Mode · Actions'**
  String get gameControlPanelSubtitle;

  /// No description provided for @gameControlLearningCloudHint.
  ///
  /// In en, this message translates to:
  /// **'Cloud coach in the app; on-board Stockfish drives hint LEDs per NVS.'**
  String get gameControlLearningCloudHint;

  /// No description provided for @gameRemoteMovesWifi.
  ///
  /// In en, this message translates to:
  /// **'Double-tap squares to send the move to the board over Wi‑Fi.'**
  String get gameRemoteMovesWifi;

  /// No description provided for @gameRemoteMovesBle.
  ///
  /// In en, this message translates to:
  /// **'Double-tap squares to send the move over Bluetooth.'**
  String get gameRemoteMovesBle;

  /// No description provided for @gameRemoteMovesMock.
  ///
  /// In en, this message translates to:
  /// **'Demo: double-tap sends the move locally (board + clock simulation).'**
  String get gameRemoteMovesMock;

  /// No description provided for @gameRemoteMovesNoBoard.
  ///
  /// In en, this message translates to:
  /// **'Enabled, but no board connected. Connect via Bluetooth.'**
  String get gameRemoteMovesNoBoard;

  /// No description provided for @gameRemoteMovesConnect.
  ///
  /// In en, this message translates to:
  /// **'Enabled — connect a board over Wi‑Fi or Bluetooth to send moves.'**
  String get gameRemoteMovesConnect;

  /// No description provided for @gameBoardRefreshedSnack.
  ///
  /// In en, this message translates to:
  /// **'Board state refreshed'**
  String get gameBoardRefreshedSnack;

  /// No description provided for @gameDemoSnapshotReloadedSnack.
  ///
  /// In en, this message translates to:
  /// **'Demo snapshot reloaded'**
  String get gameDemoSnapshotReloadedSnack;

  /// No description provided for @timerCenterGameOver.
  ///
  /// In en, this message translates to:
  /// **'Game over'**
  String get timerCenterGameOver;

  /// No description provided for @timerCenterRunning.
  ///
  /// In en, this message translates to:
  /// **'Running'**
  String get timerCenterRunning;

  /// No description provided for @timerCenterStopped.
  ///
  /// In en, this message translates to:
  /// **'Stopped'**
  String get timerCenterStopped;

  /// No description provided for @coachSetupBannerBody.
  ///
  /// In en, this message translates to:
  /// **'Your coach priority list is non-empty, but no provider has credentials yet. Open Settings → Coach & AI and fill API keys (or Ollama URL) for at least one listed provider.'**
  String get coachSetupBannerBody;

  /// No description provided for @coachErrorSomethingWrong.
  ///
  /// In en, this message translates to:
  /// **'Something went wrong: {error}'**
  String coachErrorSomethingWrong(String error);

  /// No description provided for @coachOfflineLabel.
  ///
  /// In en, this message translates to:
  /// **'Offline'**
  String get coachOfflineLabel;

  /// No description provided for @coachDisconnectedBanner.
  ///
  /// In en, this message translates to:
  /// **'Board not connected — the coach isn’t receiving the live game position.'**
  String get coachDisconnectedBanner;

  /// No description provided for @coachEmptyPromptEmbedded.
  ///
  /// In en, this message translates to:
  /// **'Ask about plans, tactics, or specific squares.'**
  String get coachEmptyPromptEmbedded;

  /// No description provided for @coachEmptyPromptFullscreen.
  ///
  /// In en, this message translates to:
  /// **'Ask about the current position or chess in general.'**
  String get coachEmptyPromptFullscreen;

  /// No description provided for @coachInputHintEmbedded.
  ///
  /// In en, this message translates to:
  /// **'Ask the coach…'**
  String get coachInputHintEmbedded;

  /// No description provided for @coachInputHintFullscreen.
  ///
  /// In en, this message translates to:
  /// **'Ask about position or plan…'**
  String get coachInputHintFullscreen;

  /// No description provided for @coachChainFailedBanner.
  ///
  /// In en, this message translates to:
  /// **'Could not reach any configured AI provider (see Settings → Coach & AI).'**
  String get coachChainFailedBanner;

  /// No description provided for @settingsLinkDisconnected.
  ///
  /// In en, this message translates to:
  /// **'Disconnected'**
  String get settingsLinkDisconnected;

  /// No description provided for @settingsLinkConnecting.
  ///
  /// In en, this message translates to:
  /// **'Connecting…'**
  String get settingsLinkConnecting;

  /// No description provided for @settingsLinkNoResponseYet.
  ///
  /// In en, this message translates to:
  /// **'No board response yet'**
  String get settingsLinkNoResponseYet;

  /// No description provided for @settingsLinkConnectedLive.
  ///
  /// In en, this message translates to:
  /// **'Connected (live)'**
  String get settingsLinkConnectedLive;

  /// No description provided for @settingsCoachSubtitleOfflineTips.
  ///
  /// In en, this message translates to:
  /// **'Short on-device tips only (no cloud)'**
  String get settingsCoachSubtitleOfflineTips;
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
