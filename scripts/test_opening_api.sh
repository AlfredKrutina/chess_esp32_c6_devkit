#!/usr/bin/env bash
# Smoke test for POST /api/game/opening (requires board at BASE_URL).
set -euo pipefail
BASE_URL="${1:-http://192.168.4.1}"

BODY='{
  "action": "start",
  "line_id": "italian_giuoco_white",
  "mode": "learn",
  "side": "white",
  "start_fen": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
  "line_uci": ["e2e4","e7e5","g1f3","b8c6","f1c4","f8c5","c2c3","g8f6"],
  "player_ply_indices": [0,2,4,6],
  "checkpoint_ply_indices": [4]
}'

echo "POST $BASE_URL/api/game/opening"
curl -sf -X POST "$BASE_URL/api/game/opening" -H 'Content-Type: application/json' -d "$BODY"
echo
curl -sf "$BASE_URL/api/status" | head -c 400
echo
