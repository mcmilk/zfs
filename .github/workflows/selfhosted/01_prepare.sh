#!/usr/bin/env bash

# prepare -> build -> testing
set -eu

# redhat like distributions
function rhl() {
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

# debian like distributions
function debian() {
  echo "##[group]Install Development Tools"
  sudo -E apt install -yq acl alien attr autoconf bc build-essential \
    curl dbench debhelper-compat dh-python dkms fakeroot fio gdb gdebi \
    git ksh lcov libacl1-dev libaio-dev libattr1-dev libblkid-dev \
    libcurl4-openssl-dev libdevmapper-dev libelf-dev libffi-dev \
    libmount-dev libpam0g-dev libselinux1-dev libssl-dev libtool \
    libudev-dev linux-headers-generic lsscsi mdadm nfs-kernel-server \
    pamtester parted po-debconf python3 python3-all-dev python3-cffi \
    python3-dev python3-packaging python3-pip python3-setuptools \
    python3-sphinx rng-tools-debian rsync samba sysstat uuid-dev \
    watchdog wget xfslibs-dev xz-utils zlib1g-dev
  echo "##[endgroup]"
}

# freebsd
function freebsd() {
  echo "##[group]Install Development Tools"
  sudo -E pkg install autoconf automake autotools git gmake python \
    devel/py-sysctl
  echo "##[endgroup]"

  echo "##[group]Install utilities for ZTS"
  sudo -E pkg install base64 checkbashisms fio ksh93 pamtester \
    devel/py-flake8 pamtester rsync
  echo "##[endgroup]"
}

# ppc64le-centos-8 -> centos-8
NAME=`hostname -s | cut -d- -f2-`
case "$NAME" in
  almalinux-8|centos-8)
    echo "##[group]Enable epel and powertools repositories"
    sudo dnf config-manager -y --set-enabled powertools
    sudo dnf install -y epel-release
    echo "##[endgroup]"
    rhl
    ;;
  almalinux-9)
    echo "##[group]Enable epel and crb repositories"
    sudo dnf config-manager -y --set-enabled crb
    sudo dnf install -y epel-release
    echo "##[endgroup]"
    rhl
    ;;
  fedora-*)
    rhl
    ;;
  debian-*)
    export DEBIAN_FRONTEND="noninteractive"
    debian
    echo "##[group]Install linux-perf"
    sudo -E apt install -yq linux-perf
    echo "##[endgroup]"
    ;;
  ubuntu-*)
    export DEBIAN_FRONTEND="noninteractive"
    debian
    echo "##[group]Install linux-tools-common"
    sudo -E apt install -yq linux-tools-common
    echo "##[endgroup]"
    ;;
  freebsd-*)
    export ASSUME_ALWAYS_YES="YES"
    freebsd
    ;;
esac
