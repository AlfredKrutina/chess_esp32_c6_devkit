#!/usr/bin/env bash
# Gradle Kotlin DSL na JDK 26 často spadne jen s „26.0.1“ (JavaVersion.parse).
# Použij JDK 21 nebo 17. Pozor: po brew install openjdk@21 často NEfunguje
# /usr/libexec/java_home -v 21 (keg-only) — proto má přednost přímá cesta z Homebrew.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"

if [[ ! -x ./gradlew ]]; then
  echo "Chybí ./gradlew — z kořene Flutter projektu spusť např.: flutter build apk --debug" >&2
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

  # 1) Homebrew — po brew install openjdk@21 je to spolehlivější než java_home.
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

  # 2) java_home — ověříme skutečnou major verzi (aby nešlo omylem JDK 26).
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
  echo "Nenašel jsem JDK 21 ani 17 (nebo java_home vrací nekompatibilní JVM)." >&2
  echo "Nainstaluj: brew install openjdk@21" >&2
  echo "Pak ručně:" >&2
  echo '  export JAVA_HOME="/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home"' >&2
  echo "Volitelně pro celý systém: sudo ln -sfn /opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk /Library/Java/JavaVirtualMachines/openjdk-21.jdk" >&2
  exit 1
fi

export JAVA_HOME
echo "[czechmate] JAVA_HOME=$JAVA_HOME" >&2
exec ./gradlew "$@"
