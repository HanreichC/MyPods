# Arch, so the binaries built here also run on the CachyOS host.
FROM docker.io/library/archlinux:base-devel

# pipewire … swh-plugins: for `mypodscore --emulate-airpods` (the effect chain against a real PipeWire)
RUN pacman -Syu --noconfirm \
        cmake git pkgconf python \
        qt6-base qt6-declarative qt6-tools qt6-websockets qt6-svg layer-shell-qt kwindowsystem qt6-shadertools \
        bluez bluez-libs libpulse openssl zlib systemd-libs \
        pipewire pipewire-pulse wireplumber libmysofa swh-plugins dbus \
    && pacman -Scc --noconfirm
