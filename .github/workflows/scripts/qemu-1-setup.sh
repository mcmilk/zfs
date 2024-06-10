#!/usr/bin/env bash

######################################################################
# 1) setup the action runner to start some qemu instance
######################################################################

set -eu

# install needed packages
sudo apt-get update
sudo apt-get install axel cloud-image-utils daemonize guestfs-tools \
  virt-manager linux-modules-extra-`uname -r`

# disk usage afterwards
sudo df -h /
sudo df -h /mnt

# generate ssh keys
rm -f ~/.ssh/id_ed25519
ssh-keygen -t ed25519 -f ~/.ssh/id_ed25519 -q -N ""
