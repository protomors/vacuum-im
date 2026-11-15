# Match the current development system; this is an Arch/system-library archive.
FROM archlinux:base-devel@sha256:fa2d4ce3310ddd90a4208e5168be8ad367da5129af1245f38fd2d803bcbe3239
ARG ARCH_SNAPSHOT=2026/11/14
RUN printf 'Server = https://archive.archlinux.org/repos/%s/$repo/os/$arch\n' "$ARCH_SNAPSHOT" > /etc/pacman.d/mirrorlist \
    && pacman -Syyuu --noconfirm --needed \
       cmake ninja git pkgconf python ccache \
       qt6-base qt6-svg qt6-multimedia qt6-multimedia-ffmpeg qt6-tools qt6-translations qt6-wayland \
       libx11 libxss zlib minizip libidn hunspell openssl \
       gstreamer gst-plugins-base gst-plugins-good gst-plugins-bad libnice opus \
    && git config --system --add safe.directory /src \
    && pacman -Scc --noconfirm
WORKDIR /src
