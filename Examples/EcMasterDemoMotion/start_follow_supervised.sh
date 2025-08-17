#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
REL_BIN="$ROOT_DIR/Bin/Linux/aarch64/Release"

mkdir -p "$REL_BIN"
cp -f "$ROOT_DIR/Examples/EcMasterDemoMotion/config.ini" "$REL_BIN/config.ini"

# Prompt for sudo upfront (keeps credential cached for a few minutes)
echo "Obtaining sudo credentials..."
sudo -v

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

echo "Stopping any existing instances..."
kill_pattern "/ai_system_camera.py"
kill_exact "motor_controller"
sudo pkill -x EcMasterDemoMotion 2>/dev/null || true
sleep 0.2
sudo pkill -9 -x EcMasterDemoMotion 2>/dev/null || true

echo "Starting ai_system_camera.py..."
nohup python3 "$ROOT_DIR/Examples/EcMasterDemoMotion/ai_system_camera.py" >"$REL_BIN/ai_system_camera.log" 2>&1 & echo $! > "$REL_BIN/ai_system_camera.pid"
echo "  PID $(cat "$REL_BIN/ai_system_camera.pid")"

echo "Starting motor_controller..."
nohup "$REL_BIN/motor_controller" >"$REL_BIN/motor_controller.log" 2>&1 & echo $! > "$REL_BIN/motor_controller.pid"
echo "  PID $(cat "$REL_BIN/motor_controller.pid")"

echo "Starting EcMasterDemoMotion (with sudo, detached)..."
sudo bash -c "cd '$REL_BIN' && nohup ./EcMasterDemoMotion DemoConfig.xml >> EcMasterDemoMotion.log 2>&1 & echo \$! > EcMasterDemoMotion.pid"
echo "  PID $(cat "$REL_BIN/EcMasterDemoMotion.pid")"

echo "All processes started. Logs:"
echo "  $REL_BIN/ai_system_camera.log"
echo "  $REL_BIN/motor_controller.log"
echo "  $REL_BIN/EcMasterDemoMotion.log"
echo "Use: bash Examples/EcMasterDemoMotion/stop_follow.sh to stop all."


