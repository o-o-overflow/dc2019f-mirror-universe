#!/bin/sh

set -e

cd /opt/usim 

./diskmaker -c disk.img
xvfb-run /opt/usim/usim
