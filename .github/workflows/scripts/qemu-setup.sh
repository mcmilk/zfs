#!/usr/bin/env bash

#######################################################
# setup the action runner to start some qemu instance #
#######################################################

set -eu

# remove snap things, update+upgrade will be faster then
for x in lxd core20 snapd; do sudo snap remove $x; done
sudo apt-get purge snapd google-chrome-stable firefox

# only install qemu, leave the system as it is
sudo apt-get update
sudo apt-get install axel cloud-image-utils guestfs-tools virt-manager
sudo dmesg -c > /var/tmp/dmesg-prerun

# generate ssh keys
sudo ssh-keygen -t ed25519 -f /root/.ssh/id_ed25519 -q -N ""

# remove swap and umount fast storage
# 89GiB -> rootfs + bootfs (don't care)
# 64GiB -> /mnt with 420MB/s (new testing ssd)
sudo swapoff -a

# make sure, we use the correct /mnt mountpoint
DEV="/dev/disk/azure/resource-part1"

# reformat testing ground
sudo umount /mnt
sudo mkfs.ext4 -O ^has_journal -F $DEV
sudo mount -o noatime,barrier=0 $DEV /mnt
cd /mnt

# disk usage afterwards
sudo df -h /
sudo df -h /mnt
sudo fstrim -a
