#!/bin/sh -e
set -x

command -v tmux

SESSION="mirror"

tmux new-session -d -s $SESSION -n chaos -c ./service/src/chaos/ ./chaosd

tmux new-window -d -n usim -c ./service/src/usim ./usim

tmux new-window -d -n worm-hole -c ./service/src/worm_hole python3 worm_hole.py --debug

tmux attach-session -t $SESSION
