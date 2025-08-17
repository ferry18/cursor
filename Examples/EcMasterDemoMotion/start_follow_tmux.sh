#!/usr/bin/env bash
set -euo pipefail

# Re-exec as root once, then run tmux session so all panes inherit privileges
if [[ $EUID -ne 0 ]]; then
  echo "Requesting sudo..."
  exec sudo -E bash "$0" "$@"
fi

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
REL_BIN="$ROOT_DIR/Bin/Linux/aarch64/Release"

cp -f "$ROOT_DIR/Examples/EcMasterDemoMotion/config.ini" "$REL_BIN/config.ini"

SESSION="ec_follow"
tmux kill-session -t "$SESSION" 2>/dev/null || true
tmux new-session -d -s "$SESSION" -n ai

# Pane 1: Python generator
tmux send-keys -t "$SESSION":ai "python3 $ROOT_DIR/Examples/EcMasterDemoMotion/ai_system_camera.py 2>&1 | tee $REL_BIN/ai_system_camera.log" C-m

# Pane 2: motor_controller
tmux split-window -v -t "$SESSION":ai
tmux send-keys -t "$SESSION":ai.2 "$REL_BIN/motor_controller 2>&1 | tee $REL_BIN/motor_controller.log" C-m

# Pane 3: EcMasterDemoMotion
tmux split-window -h -t "$SESSION":ai
tmux send-keys -t "$SESSION":ai.3 "cd $REL_BIN && ./EcMasterDemoMotion DemoConfig.xml 2>&1 | tee -a EcMasterDemoMotion.console.log" C-m

tmux select-layout -t "$SESSION":ai tiled
tmux attach -t "$SESSION"


