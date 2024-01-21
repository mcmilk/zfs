#!/usr/bin/env bash

##################################################
# configure and build openzfs modules            #
##################################################

set -eu

function freebsd() {
  sudo mv -f /usr/bin/make /usr/bin/make-NONO
  export MAKE="gmake"

  echo "##[group]Autogen.sh"
  ./autogen.sh
  echo "##[endgroup]"

  echo "##[group]Configure"
  ./configure \
    --prefix=/usr \
    --enable-debug \
    --enable-debuginfo
  echo "##[endgroup]"

  echo "##[group]Build"
  make -j`sysctl -n hw.ncpu`
  echo "##[endgroup]"

  echo "##[group]Install"
  sudo make install
  echo "##[endgroup]"
}

function linux() {
  export PATH="$PATH:/sbin:/usr/sbin:/usr/local/sbin"

  echo "##[group]Autogen.sh"
  ./autogen.sh
  echo "##[endgroup]"

  echo "##[group]Configure"
  ./configure \
    --prefix=/usr \
    --enable-debug \
    --enable-debuginfo
  echo "##[endgroup]"

  echo "##[group]Build"
  make -j$(nproc)
  echo "##[endgroup]"

  echo "##[group]Install"
  sudo make install
  echo "##[endgroup]"
}

case "$1" in
  freebsd-*)
    freebsd
    ;;
  *)
    linux
    ;;
esac
