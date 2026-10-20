#!/bin/sh
# Builds MyPods-x86_64.AppImage. Runs in Ubuntu 22.04 (an old glibc, so the AppImage starts on most
# distributions); Qt 6.9 comes through aqtinstall, as Ubuntu only has Qt 6.2.
#   podman run --rm -v "$PWD:/workspace:Z" -w /workspace docker.io/library/ubuntu:22.04 ./appimage.sh
set -eu
QT_VERSION=6.9.3
QT=/opt/qt/$QT_VERSION/gcc_64
BUILD=/tmp/mypods-build

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
    g++-12 make pkg-config git python3-pip wget file ca-certificates \
    libbluetooth-dev libpulse-dev libssl-dev zlib1g-dev libsystemd-dev \
    libgl-dev libegl-dev libxkbcommon-dev libfontconfig1-dev libdbus-1-dev \
    libxcb-cursor0 libxcb-icccm4 libxcb-keysyms1 libxcb-shape0 libxkbcommon-x11-0 libwayland-cursor0 libwayland-egl1
pip3 install aqtinstall cmake
aqt install-qt linux desktop $QT_VERSION linux_gcc_64 -m qtwebsockets qtshadertools -O /opt/qt

CC=gcc-12 CXX=g++-12 cmake -S . -B $BUILD -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$QT
cmake --build $BUILD -j"$(nproc)"
# Alle Checks ohne Hardware; ein Fehler bricht den Build ab (set -e), bevor ein AppImage entsteht
QT=$QT CXX=g++-12 sh tools/run_checks.sh $BUILD

cd $BUILD
rm -rf AppDir && mkdir -p AppDir/usr/bin/modules AppDir/usr/plugins
cp modules/mypodscore AppDir/usr/bin/modules/
# linuxdeploy's Qt plugin forgets the Wayland EGL integration, and without it there is no OpenGL on Wayland
cp -r $QT/plugins/wayland-graphics-integration-client AppDir/usr/plugins/
cp "$OLDPWD/ui/src/app/qml/assets/images/logo-512.png" mypods.png
cat > mypods.desktop <<EOF
[Desktop Entry]
Type=Application
Name=MyPods
Comment=The control center for your Bluetooth headphones
Comment[de]=Die Kontrollzentrale für deine Bluetooth-Kopfhörer
Exec=mypods
Icon=mypods
Terminal=false
Categories=Utility;
StartupWMClass=MyPods
EOF

for t in linuxdeploy linuxdeploy-plugin-qt; do
    wget -q -N "https://github.com/linuxdeploy/$t/releases/download/continuous/$t-x86_64.AppImage"
    chmod +x "$t-x86_64.AppImage"
done
# no FUSE in the container
export APPIMAGE_EXTRACT_AND_RUN=1 QMAKE=$QT/bin/qmake LD_LIBRARY_PATH=$QT/lib \
    QML_SOURCES_PATHS="$OLDPWD/ui/src/app/qml" EXTRA_PLATFORM_PLUGINS=libqwayland-generic.so \
    LDAI_OUTPUT="$OLDPWD/MyPods-x86_64.AppImage"
./linuxdeploy-x86_64.AppImage --appdir AppDir -e mypods \
    --deploy-deps-only AppDir/usr/bin/modules/mypodscore \
    --deploy-deps-only AppDir/usr/plugins/wayland-graphics-integration-client \
    -l /usr/lib/x86_64-linux-gnu/libssl.so.3 \
    -d mypods.desktop -i mypods.png --plugin qt --output appimage
