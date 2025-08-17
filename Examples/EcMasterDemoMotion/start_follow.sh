#!/usr/bin/env bash
set -euo pipefail

# Re-exec as root so sudo prompts happen once and all privileged ops succeed
if [[ $EUID -ne 0 ]]; then
  echo "Requesting sudo..."
  exec sudo -E bash "$0" "$@"
fi

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
REL_BIN="$ROOT_DIR/Bin/Linux/aarch64/Release"

# Ensure config.ini is next to motor_controller
cp -f "$ROOT_DIR/Examples/EcMasterDemoMotion/config.ini" "$REL_BIN/config.ini"

# Kill previous instances (exact names; avoid killing this script)
kill_exact() {
  local name="$1"
  local pids
  pids=$(pgrep -x "$name" || true)
  if [[ -n "${pids}" ]]; then
    kill ${pids} 2>/dev/null || true
    sleep 0.3
    pids=$(pgrep -x "$name" || true)
    if [[ -n "${pids}" ]]; then kill -9 ${pids} 2>/dev/null || true; fi
  fi
}
kill_pattern() {
  local pat="$1"
  local pids
  pids=$(pgrep -f "$pat" || true)
  if [[ -n "${pids}" ]]; then
    kill ${pids} 2>/dev/null || true
    sleep 0.3
    pids=$(pgrep -f "$pat" || true)
    if [[ -n "${pids}" ]]; then kill -9 ${pids} 2>/dev/null || true; fi
  fi
}

kill_pattern "/ai_system_camera.py"
kill_exact "motor_controller"
kill_exact "EcMasterDemoMotion"

# Start Python generator
nohup python3 "$ROOT_DIR/Examples/EcMasterDemoMotion/ai_system_camera.py" >"$REL_BIN/ai_system_camera.log" 2>&1 & AI_PID=$!
echo "Started ai_system_camera.py (PID $AI_PID)"

# Start motor_controller
nohup "$REL_BIN/motor_controller" >"$REL_BIN/motor_controller.log" 2>&1 & MC_PID=$!
echo "Started motor_controller (PID $MC_PID)"

echo "Starting EcMasterDemoMotion as root..."
cd "$REL_BIN"
set +e
./EcMasterDemoMotion DemoConfig.xml | tee -a EcMasterDemoMotion.console.log
EC_RC=$?
set -e
if [[ $EC_RC -ne 0 ]]; then
  echo "EcMasterDemoMotion exited with code $EC_RC. See logs in $REL_BIN."
fi

echo "Stopping background generators..."
kill $AI_PID $MC_PID 2>/dev/null || true


