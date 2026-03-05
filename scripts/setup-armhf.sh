#!/bin/bash
set -e

# Restrict all existing apt sources to amd64 only
# (GitHub Azure mirrors don't serve armhf packages)
sed -i 's/^deb \[arch=amd64\] /deb [arch=amd64] /' /etc/apt/sources.list
sed -i 's/^deb http/deb [arch=amd64] http/' /etc/apt/sources.list
sed -i 's/^deb \[arch=amd64\]  /deb [arch=amd64] /' /etc/apt/sources.list

for f in /etc/apt/sources.list.d/*.list; do
  sed -i 's/^deb \[arch=amd64\] /deb [arch=amd64] /' "$f" 2>/dev/null || true
  sed -i 's/^deb http/deb [arch=amd64] http/' "$f" 2>/dev/null || true
  sed -i 's/^deb \[arch=amd64\]  /deb [arch=amd64] /' "$f" 2>/dev/null || true
done

# Add ports mirror for armhf
cat > /etc/apt/sources.list.d/armhf-ports.list << 'SOURCES'
deb [arch=armhf] http://ports.ubuntu.com/ubuntu-ports jammy main universe restricted multiverse
deb [arch=armhf] http://ports.ubuntu.com/ubuntu-ports jammy-updates main universe restricted multiverse
SOURCES

apt-get update
apt-get install -y gcc-arm-linux-gnueabihf libsdl2-dev:armhf libmpg123-dev:armhf libvorbis-dev:armhf
