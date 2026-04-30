#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

pick_flutter() {
  if command -v flutter >/dev/null 2>&1; then
    command -v flutter
    return
  fi
  if [[ -x /opt/homebrew/bin/flutter ]]; then
    echo /opt/homebrew/bin/flutter
    return
  fi
  for d in /opt/homebrew/Caskroom/flutter/*/flutter/bin/flutter; do
    if [[ -x "$d" ]]; then
      echo "$d"
      return
    fi
  done
  echo "Chybí Flutter v PATH. Viz context/flutter_czechmate_port/INSTALACE_FLUTTER.txt" >&2
  exit 127
}

FLUTTER="$(pick_flutter)"
"$FLUTTER" pub get
if [[ ! -d android ]] || [[ ! -d ios ]]; then
  "$FLUTTER" create . --project-name czechmate
fi
exec "$FLUTTER" run --dart-define=STAGING=true "$@"
