#!/usr/bin/env bash

##################################################
# install dependencies for compiling and loading #
##################################################

set -eu

# debian like
function debian() {
  export DEBIAN_FRONTEND=noninteractive

  echo "##[group]Running apt-get update+upgrade"
  sudo apt-get update -y
  sudo apt-get upgrade -y
  echo "##[endgroup]"

  echo "##[group]Install Development Tools"
  # Required development tools.
  sudo apt-get install -y build-essential autoconf libtool \
    libtool-bin gdb lcov

  # Required utilities.
  sudo apt-get install -y git alien fakeroot wget curl bc fio acl \
    sysstat lsscsi parted gdebi attr dbench watchdog ksh nfs-kernel-server \
    samba rng-tools dkms rsync

  # Required development libraries
  sudo apt-get install -y linux-headers-$(uname -r) \
    zlib1g-dev uuid-dev libblkid-dev libselinux-dev \
    xfslibs-dev libattr1-dev libacl1-dev libudev-dev libdevmapper-dev \
    libssl-dev libaio-dev libffi-dev libelf-dev libmount-dev \
    libpam0g-dev pamtester python3-dev python3-setuptools python3 \
    python3-dev python3-setuptools python3-cffi libcurl4-openssl-dev \
    python3-packaging python3-distlib
  echo "##[endgroup]"
}

# freebsd
function freebsd() {
  export ASSUME_ALWAYS_YES="YES"

  echo "##[group]Install Development Tools"
  sudo pkg install -y autoconf automake autotools git gmake python \
    devel/py-sysctl
  echo "##[endgroup]"

  echo "##[group]Install utilities for ZTS"
  sudo pkg install -y base64 checkbashisms fio ksh93 pamtester \
    devel/py-flake8 pamtester rsync
  echo "##[endgroup]"

  echo "##[group]Install Kernel Headers"
  ABI=$(uname -p)
  VERSION=$(freebsd-version -r)
  cd /tmp
  fetch https://download.freebsd.org/ftp/snapshots/${ABI}/${VERSION}/src.txz ||
  fetch https://download.freebsd.org/ftp/releases/${ABI}/${VERSION}/src.txz
  sudo tar xpf src.txz -C /
  rm src.txz
  echo "##[endgroup]"
}

# rhel
function rhel() {
  echo "##[group]Running dnf update"
  sudo dnf update -y
  echo "##[endgroup]"

  echo "##[group]Install Development Tools"
  sudo dnf group install -y "Development Tools"
  sudo dnf install -y libtool rpm-build libtirpc-devel libblkid-devel \
    libuuid-devel libudev-devel openssl-devel zlib-devel libaio-devel \
    libattr-devel elfutils-libelf-devel python3 python3-devel \
    python3-setuptools python3-cffi libffi-devel git ncompress \
    libcurl-devel python3-packaging
  sudo dnf install -y kernel-devel-$(uname -r)
  echo "##[endgroup]"

  echo "##[group]Install utilities for ZTS"
  sudo dnf install -y acl ksh bc bzip2 fio sysstat mdadm lsscsi parted attr \
    nfs-utils samba rng-tools perf rsync dbench pamtester
  echo "##[endgroup]"
}

case "$1" in
  almalinux8|centos-stream8)
    echo "##[group]Enable epel and powertools repositories"
    sudo dnf config-manager -y --set-enabled powertools
    sudo dnf install -y epel-release
    echo "##[endgroup]"
    rhel
    ;;
  almalinux9|centos-stream9)
    echo "##[group]Enable epel and crb repositories"
    sudo dnf config-manager -y --set-enabled crb
    sudo dnf install -y epel-release
    echo "##[endgroup]"
    rhel
    ;;
  debian*)
    debian
    echo "##[group]Install linux-perf"
    sudo apt-get install -yq linux-perf
    echo "##[endgroup]"
    ;;
  fedora*)
    rhel
    ;;
  freebsd*)
    freebsd
    ;;
  ubuntu*)
    debian
    echo "##[group]Install linux-tools-common"
    sudo apt-get install -yq linux-tools-common
    echo "##[endgroup]"
    ;;
esac
