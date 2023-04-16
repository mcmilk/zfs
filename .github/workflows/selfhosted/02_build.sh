#!/usr/bin/env bash

# prepare -> build -> testing
set -eu

function linux() {
  export PATH="$PATH:/sbin:/usr/sbin:/usr/local/sbin"

  echo "##[group]Autogen.sh"
  ./autogen.sh
  echo "##[endgroup]"

  echo "##[group]Configure"
  ./configure --enable-debug --enable-debuginfo
  echo "##[endgroup]"

  echo "##[group]Build"
  make -j$(nproc)
  echo "##[endgroup]"

  echo "##[group]Install"
  sudo -E make install
  echo "##[endgroup]"
}

function freebsd() {
  echo "##[group]Autogen.sh"
  ./autogen.sh
  echo "##[endgroup]"

  echo "##[group]Configure"
  env MAKE=gmake ./configure --enable-debug --enable-debuginfo
  echo "##[endgroup]"

  echo "##[group]Build"
  gmake -j`sysctl -n hw.ncpu`
  echo "##[endgroup]"

  echo "##[group]Install"
  sudo -E gmake install
  echo "##[endgroup]"
}

# ppc64le-centos-8 -> centos-8
NAME=`hostname -s | cut -d- -f2-`
case "$NAME" in
  freebsd-*)
    freebsd
    ;;
  *)
    linux
    ;;
esac
