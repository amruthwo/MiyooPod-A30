#!/bin/bash
set -e

# Install cross-compiler (amd64 package, always works)
apt-get install -y gcc-arm-linux-gnueabihf

# Download armhf SDL2 debs and extract without installing
# This avoids the multiarch conflicts with base system libraries
mkdir -p /tmp/armhf-extract

cd /tmp/armhf-extract

PORTS="http://ports.ubuntu.com/ubuntu-ports/pool"

# Download debs directly
curl -L "$PORTS/main/libs/libsdl2/libsdl2-2.0-0_2.0.20+dfsg-2ubuntu1.22.04.1_armhf.deb" -o libsdl2.deb
curl -L "$PORTS/universe/libs/libsdl2/libsdl2-dev_2.0.20+dfsg-2ubuntu1.22.04.1_armhf.deb" -o libsdl2-dev.deb
curl -L "$PORTS/main/m/mpg123/libmpg123-0_1.29.3-1build1_armhf.deb" -o libmpg123.deb
curl -L "$PORTS/main/libv/libvorbis/libvorbis0a_1.3.7-1build2_armhf.deb" -o libvorbis.deb
curl -L "$PORTS/main/libv/libvorbis/libvorbisfile3_1.3.7-1build2_armhf.deb" -o libvorbisfile.deb

# Extract each deb
for deb in *.deb; do
    dpkg -x "$deb" /tmp/armhf-extract/root
done

# Copy libs to the expected location
cp -v /tmp/armhf-extract/root/usr/lib/arm-linux-gnueabihf/*.so* /usr/lib/arm-linux-gnueabihf/ 2>/dev/null || true

echo "armhf libs installed"
ls /usr/lib/arm-linux-gnueabihf/libSDL2* 2>/dev/null || echo "WARNING: libSDL2 not found"
