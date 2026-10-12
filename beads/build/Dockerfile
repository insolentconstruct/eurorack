# GNU Arm Embedded 9-2020-q2, with 4.9.3 for the golden build.
FROM docker.io/library/debian:bookworm-slim@sha256:7c7b2c966bc9ee8cedfeef67e0e279108992c77681fa595db4a9d65c06ccc587 AS compiler
SHELL ["nice", "-n", "10", "/bin/sh", "-c"]

ARG DEBIAN_SNAPSHOT=20260916T000000Z

ARG TC_URL=https://launchpad.net/gcc-arm-embedded/4.9/4.9-2015-q2-update/+download/gcc-arm-none-eabi-4_9-2015q2-20150609-linux.tar.bz2
ARG TC_SHA256=3a574c5becfca496bafe07bbd76e9deae1fd0cc99c5a9de0f8c610750f2f89ac

RUN rm -f /etc/apt/sources.list.d/debian.sources \
 && printf '%s\n' \
      "deb http://snapshot.debian.org/archive/debian/${DEBIAN_SNAPSHOT} bookworm main" \
      "deb http://snapshot.debian.org/archive/debian/${DEBIAN_SNAPSHOT} bookworm-updates main" \
      "deb http://snapshot.debian.org/archive/debian-security/${DEBIAN_SNAPSHOT} bookworm-security main" \
      > /etc/apt/sources.list \
 && dpkg --add-architecture i386 \
 && apt-get -o Acquire::Check-Valid-Until=false -o Acquire::Retries=5 update \
 && apt-get install -y --no-install-recommends \
        ca-certificates curl bzip2 git make python3 python3-numpy python3-scipy g++ \
        qemu-user-static:amd64=1:7.2+dfsg-7+deb12u18+b3 \
        libc6:i386 libstdc++6:i386 zlib1g:i386 \
 && rm -rf /var/lib/apt/lists/* \
 && dpkg-query -W -f='${binary:Package} ${Version}\n' | LC_ALL=C sort > /usr/local/share/beads-build-kit-packages.txt

ARG TC_LOCAL=
COPY . /kit-context/
RUN if [ -n "$TC_LOCAL" ]; then cp "/kit-context/$TC_LOCAL" /tmp/tc.tar.bz2; else curl -fsSL -o /tmp/tc.tar.bz2 "$TC_URL"; fi \
 && echo "$TC_SHA256  /tmp/tc.tar.bz2" | sha256sum -c - \
 && tar -xjf /tmp/tc.tar.bz2 -C /opt \
 && rm -f /tmp/tc.tar.bz2

ARG GCC9_URL=https://developer.arm.com/-/media/Files/downloads/gnu-rm/9-2020q2/gcc-arm-none-eabi-9-2020-q2-update-x86_64-linux.tar.bz2
ARG GCC9_SHA256=5adc2ee03904571c2de79d5cfc0f7fe2a5c5f54f44da5b645c17ee57b217f11f
ARG GCC9_LOCAL=
# Verified Arm archive.
RUN if [ -n "$GCC9_LOCAL" ]; then cp "/kit-context/$GCC9_LOCAL" /tmp/gcc9.tar.bz2; else curl -fsSL -o /tmp/gcc9.tar.bz2 "$GCC9_URL"; fi \
 && echo "$GCC9_SHA256  /tmp/gcc9.tar.bz2" | sha256sum -c - \
 && echo "2b9eeccc33470f9d3cda26983b9d2dc6  /tmp/gcc9.tar.bz2" | md5sum -c - \
 && tar -xjf /tmp/gcc9.tar.bz2 -C /opt \
 && rm -f /tmp/gcc9.tar.bz2

ENV GOLDEN_TOOLCHAIN_PATH=/opt/gcc-arm-none-eabi-4_9-2015q2/
ENV TOOLCHAIN_PATH=/opt/gcc-arm-none-eabi-9-2020-q2-update/
ENV PATH=/opt/gcc-arm-none-eabi-9-2020-q2-update/bin:$PATH

# Smoke test.
RUN arm-none-eabi-gcc --version | sed -n 1p \
 && printf '#include <math.h>\n#include <stdint.h>\nfloat f(float x){return __builtin_sqrtf(x);}\n' > /tmp/probe.c \
 && arm-none-eabi-gcc -c -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16 /tmp/probe.c -o /tmp/probe.o \
 && rm -f /tmp/probe.c /tmp/probe.o

LABEL org.opencontainers.image.title="beads-build-kit" \
      org.beads.toolchain="gcc-arm-none-eabi-9-2020-q2-update"

WORKDIR /src
