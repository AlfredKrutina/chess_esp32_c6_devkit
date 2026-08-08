#!/usr/bin/env bash
# Gradle Kotlin DSL on JDK 26 often fails with just "26.0.1" (JavaVersion.parse).
# Use JDK 21 or 17. Note: after brew install openjdk@21,
# /usr/libexec/java_home -v 21 often does NOT work (keg-only) — so the direct Homebrew path takes precedence.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"

if [[ ! -x ./gradlew ]]; then
  echo "Missing ./gradlew — from the Flutter project root run e.g.: flutter build apk --debug" >&2
  exit 1
fi

java_home_ok() {
  local home="$1"
  [[ -x "$home/bin/java" ]] || return 1
  "$home/bin/java" -version 2>&1 | grep -qE 'version "(21|17)\.' || return 1
  return 0
}

pick_java_home() {
  local brewjdk h

  # 1) Homebrew — after brew install openjdk@21 this is more reliable than java_home.
  for brewjdk in \
    /opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home \
    /usr/local/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home \
    /opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk/Contents/Home \
    /usr/local/opt/openjdk@17/libexec/openjdk.jdk/Contents/Home; do
    if [[ -d "$brewjdk" ]] && java_home_ok "$brewjdk"; then
      echo "$brewjdk"
      return 0
    fi
  done

  # 2) java_home — verify the actual major version (so we don't accidentally get JDK 26).
  for v in 21 17; do
    h="$(/usr/libexec/java_home -v "$v" 2>/dev/null || true)"
    if [[ -n "$h" ]] && java_home_ok "$h"; then
      echo "$h"
      return 0
    fi
  done

  return 1
}

if ! JAVA_HOME="$(pick_java_home)"; then
  echo "Could not find JDK 21 or 17 (or java_home returns an incompatible JVM)." >&2
  echo "Install: brew install openjdk@21" >&2
  echo "Then manually:" >&2
  echo '  export JAVA_HOME="/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home"' >&2
  echo "Optionally system-wide: sudo ln -sfn /opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk /Library/Java/JavaVirtualMachines/openjdk-21.jdk" >&2
  exit 1
fi

export JAVA_HOME
echo "[czechmate] JAVA_HOME=$JAVA_HOME" >&2
exec ./gradlew "$@"
