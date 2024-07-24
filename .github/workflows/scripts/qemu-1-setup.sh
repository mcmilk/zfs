#!/usr/bin/env bash

######################################################################
# 1) setup the action runner to start some qemu instance
######################################################################

set -eu

# speedup virtual disk
sudo modprobe loop
sudo fallocate -l 50GB /var/blob
sudo losetup -f /var/blob
sudo mdadm --create /dev/md/raidzero -n2 -l0 /dev/loop0 /dev/sdb
sudo mkfs.ext4 -F /dev/md/raidzero

# old /mnt is hidden now
sudo mkdir -p /mnt/tests
sudo mount -o discard,noatime /dev/md/raidzero /mnt/tests
df -h / /mnt /mnt/tests

# install needed packages
  export DEBIAN_FRONTEND="noninteractive"
sudo apt-get update
sudo apt-get install axel cloud-image-utils daemonize guestfs-tools \
  virt-manager linux-modules-extra-`uname -r`

# generate ssh keys
rm -f ~/.ssh/id_ed25519
ssh-keygen -t ed25519 -f ~/.ssh/id_ed25519 -q -N ""

# ssd/nvme
for i in /sys/block/s*/queue/scheduler; do
  echo "none" | sudo tee $i
done
