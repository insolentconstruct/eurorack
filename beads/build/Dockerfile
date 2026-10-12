# Copyright 2026 insolentconstruct.
#
# Author: insolentconstruct
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# See http://creativecommons.org/licenses/MIT/ for more information.

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

FROM docker.io/library/node:22-bookworm-slim@sha256:83f487e0a63425e5b4d146fb5e5be574bcbe1b7b843d3ebafdd95eaf7767a7e5 AS compiler
SHELL ["nice", "-n", "10", "/bin/sh", "-c"]

ARG DEBIAN_SNAPSHOT=20260916T000000Z

ARG TC_URL=https://launchpad.net/gcc-arm-embedded/4.9/4.9-2015-q2-update/+download/gcc-arm-none-eabi-4_9-2015q2-20150609-linux.tar.bz2
ARG TC_SHA256=3a574c5becfca496bafe07bbd76e9deae1fd0cc99c5a9de0f8c610750f2f89ac
ARG TC_TREE_SHA256=7eed4a71b66c3ca6719b20428b9754b40cd802b93cf256c2443dfc3738a11c06

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
        libc6:i386 libstdc++6:i386 zlib1g:i386 \
 && rm -rf /var/lib/apt/lists/* \
 && dpkg-query -W -f='${binary:Package} ${Version}\n' | LC_ALL=C sort > /usr/local/share/beads-build-kit-packages.txt

# Toolchain tree hash.
RUN printf '%s\n' \
      '#!/bin/bash' \
      'set -euo pipefail' \
      'cd /opt' \
      'find gcc-arm-none-eabi-4_9-2015q2 \( -type f -o -type l \) -print0 | LC_ALL=C sort -z \' \
      '  | while IFS= read -r -d "" f; do' \
      '      if [ -L "$f" ]; then printf "L %s %s\n" "$f" "$(readlink "$f")";' \
      '      else printf "F %s %s\n" "$f" "$(sha256sum < "$f" | cut -c1-64)"; fi; done \' \
      '  | sha256sum | cut -c1-64' \
      > /usr/local/bin/toolchain-tree-sha256 \
 && chmod 755 /usr/local/bin/toolchain-tree-sha256

ARG TC_LOCAL=
COPY . /kit-context/
RUN if [ -n "$TC_LOCAL" ]; then cp "/kit-context/$TC_LOCAL" /tmp/tc.tar.bz2; else curl -fsSL -o /tmp/tc.tar.bz2 "$TC_URL"; fi \
 && echo "$TC_SHA256  /tmp/tc.tar.bz2" | sha256sum -c - \
 && tar -xjf /tmp/tc.tar.bz2 -C /opt \
 && rm -f /tmp/tc.tar.bz2 \
 && got="$(toolchain-tree-sha256)" \
 && if [ "$got" != "$TC_TREE_SHA256" ]; then echo "toolchain tree sha256 $got, expected $TC_TREE_SHA256" >&2; exit 1; fi \
 && echo "toolchain tree sha256 $got"

ENV TOOLCHAIN_PATH=/opt/gcc-arm-none-eabi-4_9-2015q2/
ENV PATH=/opt/gcc-arm-none-eabi-4_9-2015q2/bin:$PATH

# Smoke test.
RUN arm-none-eabi-gcc --version | sed -n 1p \
 && printf '#include <math.h>\n#include <stdint.h>\nfloat f(float x){return __builtin_sqrtf(x);}\n' > /tmp/probe.c \
 && arm-none-eabi-gcc -c -mcpu=cortex-m7 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16 /tmp/probe.c -o /tmp/probe.o \
 && rm -f /tmp/probe.c /tmp/probe.o

LABEL org.opencontainers.image.title="beads-build-kit" \
      org.beads.toolchain="gcc-arm-none-eabi-4_9-2015q2" \
      org.beads.toolchain-tree-sha256="7eed4a71b66c3ca6719b20428b9754b40cd802b93cf256c2443dfc3738a11c06"

WORKDIR /src
