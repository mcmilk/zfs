#!/usr/bin/env bash

##################################################
# load openzfs modules and start testings        #
##################################################

set -eu

function freebsd() {
  echo "##[group]Loading module"
  sudo -E ./scripts/zfs.sh
  echo "##[endgroup]"

  echo "##[group]Testing"
  /usr/share/zfs/zfs-tests.sh -vKR -s 3G -r sanity
  echo "##[endgroup]"
}

function linux() {
  export LANG=C

  echo "##[group]Loading module"
  sudo -E ./scripts/zfs.sh
  echo "##[endgroup]"

  echo "##[group]Testing"
  /usr/share/zfs/zfs-tests.sh -vKR -s 3G -r sanity | scripts/zfs-tests-color.sh
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
