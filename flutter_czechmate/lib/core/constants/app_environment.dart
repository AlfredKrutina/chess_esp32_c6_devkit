/// `flutter run --dart-define=STAGING=true` pro debug výpisy.
class AppEnvironment {
  static const bool staging =
      bool.fromEnvironment('STAGING', defaultValue: false);
}
