#!/bin/sh

set -e

cd /opt/usim 

chmod 666 disk.img
su diskmaker -c "./diskmaker -c disk.img"
killall -u diskmaker || true
chmod 644 disk.img
su cadr -c "xvfb-run /usr/bin/timeout 200s /opt/usim/usim"
