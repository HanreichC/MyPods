# Arch-Basis, damit die gebauten Binaries auch auf dem CachyOS-Host laufen.
FROM docker.io/library/archlinux:base-devel

RUN pacman -Syu --noconfirm \
        cmake git pkgconf python \
        qt6-base qt6-declarative qt6-tools qt6-websockets qt6-svg layer-shell-qt \
        bluez bluez-libs libpulse openssl zlib systemd-libs \
    && pacman -Scc --noconfirm
