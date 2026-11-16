# Debian 13 (trixie) package builder. Build dependencies come from debian/control.
FROM debian:trixie@sha256:9c6e381421d0f48d1b8133e573f33c185ee16cbf505eda9c4a8a4791b25ea8a6
ENV DEBIAN_FRONTEND=noninteractive
COPY src/packages/linux/debian/control /tmp/vacuum-im/debian/control
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential debhelper dpkg-dev git ca-certificates \
    && apt-get build-dep -y /tmp/vacuum-im \
    && git config --system --add safe.directory /src \
    && rm -rf /var/lib/apt/lists/* /tmp/vacuum-im
WORKDIR /src
