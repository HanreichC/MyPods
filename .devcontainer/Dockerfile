# Arch-Basis, damit die gebauten Binaries auch auf dem CachyOS-Host laufen.
FROM docker.io/library/archlinux:base-devel

# pipewire … swh-plugins: für `magicpodscore --emulate-airpods` (Effektkette gegen ein echtes PipeWire)
RUN pacman -Syu --noconfirm \
        cmake git pkgconf python \
        qt6-base qt6-declarative qt6-tools qt6-websockets qt6-svg layer-shell-qt \
        bluez bluez-libs libpulse openssl zlib systemd-libs \
        pipewire pipewire-pulse wireplumber libmysofa swh-plugins dbus \
    && pacman -Scc --noconfirm
