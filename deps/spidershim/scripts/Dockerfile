FROM ubuntu:xenial

RUN apt-get update && apt-get -y install \
    curl clang g++ g++-4.9 git make ccache \
    libedit-dev zlib1g-dev \
    llvm-3.7-tools wget mercurial
RUN wget -q https://hg.mozilla.org/mozilla-central/raw-file/default/python/mozboot/bin/bootstrap.py -O bootstrap.py && python bootstrap.py --application-choice=browser --no-interactive

ENV PATH ~/.cargo/bin:$PATH
ENV SHELL /bin/bash
ENV TRAVIS true
ENV CCACHE_DIR /ccache
RUN mkdir /build /ccache
WORKDIR /build
