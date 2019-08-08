#!/bin/sh

set -e

cd /opt/usim 

./diskmaker -c disk.img
xvfb-run /usr/bin/timeout 200s /opt/usim/usim
