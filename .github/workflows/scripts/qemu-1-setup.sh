#!/usr/bin/env bash

######################################################################
# 1) setup the action runner to start some qemu instance
######################################################################

set -eu

# Filesystem      Size  Used Avail Use% Mounted on
# /dev/root        72G   42G   31G  58% /     -> 2x11GB
# /dev/sdb1        74G  4.1G   66G   6% /mnt  -> 6x11GB

# speedup virtual disk
sudo swapoff -a
sudo modprobe loop max_loop=8
for i in `seq 1 2`; do
  sudo fallocate -l 11GB /var/blob$i
  sudo losetup -f /var/blob$i
done
for i in `seq 1 6`; do
  sudo fallocate -l 11GB /mnt/blob$i
  sudo losetup -f /mnt/blob$i
done
df -h / /mnt
sudo mdadm --create /dev/md/raidzero -n8 -l0 /dev/loop{0..7}
sudo hdparm -t /dev/md/raidzero
sudo mkfs.ext4 -F /dev/md/raidzero

# old /mnt is hidden now
sudo mount -o discard,noatime /dev/md/raidzero /mnt
sudo fallocate -l 8GB /mnt/swapfile
sudo chmod 600 /mnt/swapfile
sudo mkswap /mnt/swapfile
sudo swapon /mnt/swapfile

# install needed packages
sudo apt-get update
sudo apt-get install axel cloud-image-utils daemonize guestfs-tools \
  ksmtuned virt-manager linux-modules-extra-`uname -r`
sudo systemctl start ksm
sudo systemctl start ksmtuned

# generate ssh keys
rm -f ~/.ssh/id_ed25519
ssh-keygen -t ed25519 -f ~/.ssh/id_ed25519 -q -N ""

# ssd/nvme
for i in /sys/block/s*/queue/scheduler; do
  echo "none" | sudo tee $i
done
