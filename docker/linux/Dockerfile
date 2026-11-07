FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        binutils \
        build-essential \
        ca-certificates \
        clang \
        cmake \
        git \
        ninja-build \
        python3 \
    && rm -rf /var/lib/apt/lists/*

ENV HOME=/tmp

WORKDIR /src
