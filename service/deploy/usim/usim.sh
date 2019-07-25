#!/bin/sh

set -e

zcat /opt/usim/disk.img.gz > /opt/usim/disk.img
xvfb-run /opt/usim/usim
