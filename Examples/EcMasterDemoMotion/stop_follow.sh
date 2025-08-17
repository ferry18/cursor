#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
REL_BIN="$ROOT_DIR/Bin/Linux/aarch64/Release"

kill_exact() {
  local name="$1"; local pids
  pids=$(pgrep -x "$name" || true)
  [[ -n "$pids" ]] && kill $pids 2>/dev/null || true
  sleep 0.2
  pids=$(pgrep -x "$name" || true)
  [[ -n "$pids" ]] && kill -9 $pids 2>/dev/null || true
}
kill_pattern() {
  local pat="$1"; local pids
  pids=$(pgrep -f "$pat" || true)
  [[ -n "$pids" ]] && kill $pids 2>/dev/null || true
  sleep 0.2
  pids=$(pgrep -f "$pat" || true)
  [[ -n "$pids" ]] && kill -9 $pids 2>/dev/null || true
}

kill_pattern "/ai_system_camera.py"
kill_exact "motor_controller"

# Prefer PID file created by supervised launcher
if [[ -f "$REL_BIN/EcMasterDemoMotion.pid" ]]; then
  PID=$(cat "$REL_BIN/EcMasterDemoMotion.pid" 2>/dev/null || true)
  if [[ -n "$PID" ]]; then
    sudo kill "$PID" 2>/dev/null || true
    sleep 0.3
    sudo kill -9 "$PID" 2>/dev/null || true
  fi
  rm -f "$REL_BIN/EcMasterDemoMotion.pid"
fi

# Fallback: kill by name and full path
sudo pkill -f "/EcMasterDemoMotion( |$)" 2>/dev/null || true
sudo pkill -9 -f "/EcMasterDemoMotion( |$)" 2>/dev/null || true

echo "Stopped."


