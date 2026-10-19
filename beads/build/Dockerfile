# GNU ARM Embedded 4.9-2015-q2 (amd64), hash-checked.
FROM python@sha256:d8fac68ebdc45b8d66d53f1ed6c1532da81109a8f5532a6ca0c951ed31107d70 AS encoder
# Python 2.7 for the audio update encoder.
RUN printf '%s\n' 'numpy==1.16.6 --hash=sha256:1680c8d5086a88d293dfd1a10b6429a09140cacee878034fa2308472ec835db4' > /tmp/requirements.txt \
 && nice -n 10 python -m pip install --no-cache-dir --only-binary=:all: --require-hashes -r /tmp/requirements.txt


# QEMU with a fixed CPU model, so the encoder output is identical on any host.
RUN curl -fsSL https://archive.debian.org/debian/pool/main/q/qemu/qemu-user-static_3.1+dfsg-8+deb10u8_amd64.deb -o /tmp/qemu.deb \
 && echo '34d01a0ff83d44fe4f39e35c7f86346f5e5afa3731ce7623a12688b194ea8ca3  /tmp/qemu.deb' | sha256sum -c - \
 && dpkg-deb -x /tmp/qemu.deb /tmp/qemu \
 && cp /tmp/qemu/usr/bin/qemu-x86_64-static /usr/local/bin/ \
 && mkdir -p /usr/local/share/doc/qemu-user-static \
 && cp /tmp/qemu/usr/share/doc/qemu-user-static/copyright /usr/local/share/doc/qemu-user-static/ \
 && rm -rf /tmp/qemu /tmp/qemu.deb \
 && qemu-x86_64-static --version

FROM docker.io/library/debian:trixie-slim@sha256:a29215f6a35e51e22adffa17f89e9d2ef06214e64a2bad10d765c46aea49f11f AS compiler
SHELL ["nice", "-n", "10", "/bin/sh", "-c"]

ARG DEBIAN_SNAPSHOT=20260916T000000Z

ARG TC_URL=https://launchpad.net/gcc-arm-embedded/4.9/4.9-2015-q2-update/+download/gcc-arm-none-eabi-4_9-2015q2-20150609-linux.tar.bz2
ARG TC_SHA256=3a574c5becfca496bafe07bbd76e9deae1fd0cc99c5a9de0f8c610750f2f89ac

RUN rm -f /etc/apt/sources.list.d/debian.sources \
 && printf '%s\n' \
      "deb http://snapshot.debian.org/archive/debian/${DEBIAN_SNAPSHOT} trixie main" \
      "deb http://snapshot.debian.org/archive/debian/${DEBIAN_SNAPSHOT} trixie-updates main" \
      "deb http://snapshot.debian.org/archive/debian-security/${DEBIAN_SNAPSHOT} trixie-security main" \
      > /etc/apt/sources.list \
 && dpkg --add-architecture i386 \
 && apt-get -o Acquire::Check-Valid-Until=false -o Acquire::Retries=5 update \
 && apt-get install -y --no-install-recommends \
        ca-certificates curl bzip2 git make python3 python3-numpy python3-scipy g++ \
        libc6:i386 libstdc++6:i386 zlib1g:i386 \
 && rm -rf /var/lib/apt/lists/* \
 && dpkg-query -W -f='${binary:Package} ${Version}\n' | LC_ALL=C sort > /usr/local/share/beads-build-kit-packages.txt

ARG TC_LOCAL=
COPY . /kit-context/
RUN if [ -n "$TC_LOCAL" ]; then cp "/kit-context/$TC_LOCAL" /tmp/tc.tar.bz2; else curl -fsSL -o /tmp/tc.tar.bz2 "$TC_URL"; fi \
 && echo "$TC_SHA256  /tmp/tc.tar.bz2" | sha256sum -c - \
 && tar -xjf /tmp/tc.tar.bz2 -C /opt \
 && rm -f /tmp/tc.tar.bz2

ENV TOOLCHAIN_PATH=/opt/gcc-arm-none-eabi-4_9-2015q2/
ENV PATH=/opt/gcc-arm-none-eabi-4_9-2015q2/bin:$PATH

# Smoke test.
RUN arm-none-eabi-gcc --version | sed -n 1p \
 && printf '#include <math.h>\n#include <stdint.h>\nfloat f(float x){return __builtin_sqrtf(x);}\n' > /tmp/probe.c \
 && arm-none-eabi-gcc -c -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16 /tmp/probe.c -o /tmp/probe.o \
 && rm -f /tmp/probe.c /tmp/probe.o

LABEL org.opencontainers.image.title="beads-build-kit" \
      org.beads.toolchain="gcc-arm-none-eabi-4_9-2015q2"

WORKDIR /src
