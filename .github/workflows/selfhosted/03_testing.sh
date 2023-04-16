#!/usr/bin/env bash

# prepare -> build -> testing
set -eu

function linux() {
  export LANG=C

  echo "##[group]Loading module"
  sudo -E ./scripts/zfs.sh
  echo "##[endgroup]"

  echo "##[group]Testing"
  ./scripts/zfs-tests.sh -vKR -s 3G -r sanity | ./scripts/zfs-tests-color.sh
  echo "##[endgroup]"
}

function freebsd() {
  echo "##[group]Loading module"
  sudo -E ./scripts/zfs.sh
  echo "##[endgroup]"

  echo "##[group]Testing"
  ./scripts/zfs-tests.sh -vKR -s 3G -r sanity
  echo "##[endgroup]"
}

NAME=`hostname -s | cut -d- -f2-`
case "$NAME" in
  freebsd-*)
    freebsd
    ;;
  *)
    linux
    ;;
esac
