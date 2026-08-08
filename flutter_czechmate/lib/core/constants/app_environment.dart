/// `flutter run --dart-define=STAGING=true` for debug listings.
class AppEnvironment {
  static const bool staging =
      bool.fromEnvironment('STAGING', defaultValue: false);
}
