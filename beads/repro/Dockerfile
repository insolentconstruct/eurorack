# Copyright 2026 Insolent Construct.
# Author: Insolent Construct
FROM docker.io/library/python:3.12-slim-bookworm
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends g++ make \
 && rm -rf /var/lib/apt/lists/* \
 && pip install --no-cache-dir numpy==2.1.3
WORKDIR /src
