# Copyright 2026 Insolent Construct.
# Author: Insolent Construct
FROM docker.io/library/python:3.12-slim-trixie@sha256:a6e34c598f2467ed0e9a8d349809fcd8b5c603269512df273a0bb1784edc11b1
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends g++ make \
 && rm -rf /var/lib/apt/lists/* \
 && pip install --no-cache-dir numpy==2.1.3 unicorn==2.1.4 \
      capstone==5.0.7 pyelftools==0.33
WORKDIR /src
