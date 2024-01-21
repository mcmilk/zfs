#!/usr/bin/env bash

##############################################################
# start qemu with some operating system, init via cloud-init #
##############################################################

OS="$1"
ID="$2"
GH="$3"
REPO="$4"

# valid ostypes: virt-install --os-variant list
OSv=$OS

case "$OS" in
  almalinux8)
    URL="https://repo.almalinux.org/almalinux/8/cloud/x86_64/images/AlmaLinux-8-GenericCloud-latest.x86_64.qcow2"
    ;;
  almalinux9)
    URL="https://repo.almalinux.org/almalinux/9/cloud/x86_64/images/AlmaLinux-9-GenericCloud-latest.x86_64.qcow2"
    ;;
  centos-stream8)
    URL="https://cloud.centos.org/centos/8-stream/x86_64/images/CentOS-Stream-GenericCloud-8-latest.x86_64.qcow2"
    ;;
  centos-stream9)
    URL="https://cloud.centos.org/centos/9-stream/x86_64/images/CentOS-Stream-GenericCloud-9-latest.x86_64.qcow2"
    ;;
  debian10)
    URL="https://cloud.debian.org/images/cloud/buster/latest/debian-10-generic-amd64.qcow2"
    ;;
  debian11)
    URL="https://cloud.debian.org/images/cloud/bullseye/latest/debian-11-generic-amd64.qcow2"
    ;;
  debian12)
    URL="https://cloud.debian.org/images/cloud/bookworm/latest/debian-12-generic-amd64.qcow2"
    ;;
  fedora38)
    URL="https://download.fedoraproject.org/pub/fedora/linux/releases/38/Cloud/x86_64/images/Fedora-Cloud-Base-38-1.6.x86_64.qcow2"
    ;;
  fedora39)
    URL="https://download.fedoraproject.org/pub/fedora/linux/releases/39/Cloud/x86_64/images/Fedora-Cloud-Base-39-1.5.x86_64.qcow2"
    OSv="fedora38"
    ;;
  freebsd13)
    # freebsd images don't have clout-init within it! :(
    # workaround: provide own images
    URL="https://openzfs.de/freebsd/amd64-freebsd-13.2.qcow2"
    OSv="freebsd13.0"
    BASH="/usr/local/bin/bash"
    ;;
  freebsd14)
    URL="https://openzfs.de/freebsd/amd64-freebsd-14.0.qcow2"
    OSv="freebsd13.0"
    BASH="/usr/local/bin/bash"
    ;;
  freebsd15)
    URL="https://openzfs.de/freebsd/amd64-freebsd-15.0.qcow2"
    OSv="freebsd13.0"
    BASH="/usr/local/bin/bash"
    ;;
  ubuntu22)
    arch="amd64"
    URL="https://cloud-images.ubuntu.com/jammy/current/jammy-server-cloudimg-amd64.img"
    OSv="ubuntu22.04"
    ;;
  ubuntu24)
    URL="https://cloud-images.ubuntu.com/noble/current/noble-server-cloudimg-amd64.img"
    OSv="ubuntu24.04"
    ;;
  *)
    echo "Wrong value for variable OS!"
    exit 111
    ;;
esac

IMG="/mnt/original.qcow2"
DISK="/mnt/openzfs.qcow2"

echo "Loading image $URL ..."
sudo axel -q "$URL" -o "$IMG" || exit 111

echo "Converting image ..."
sudo qemu-img convert -f qcow2 -O qcow2 -c -o compression_type=zstd,preallocation=off $IMG $DISK || exit 111
sudo rm -f /mnt/original.qcow2 || exit 111

echo "Resizing image to 60GiB ..."
sudo qemu-img resize $DISK 60G || exit 111

PUBKEY=`sudo cat /root/.ssh/id_ed25519.pub`
cat <<EOF > /tmp/user-data
#cloud-config

fqdn: $OS

users:
- name: root
  shell: $BASH
- name: zfs
  sudo: ALL=(ALL) NOPASSWD:ALL
  shell: $BASH
  ssh_authorized_keys:
    - $PUBKEY

growpart:
  mode: auto
  devices: ['/']
  ignore_growroot_disabled: false

write_files:
  - content: |
      #!/usr/bin/env bash

      exec 2>\$HOME/stderr-init.log
      cd \$HOME

      # download github action runner package
      function download_action_runner() {
        REL="2.312.0"
        URL="https://github.com/actions/runner/releases"
        while :; do
          # https://github.com/actions/runner/releases/download/v2.305.0/actions-runner-linux-arm64-2.305.0.tar.gz
          curl -s -o ghar.tgz -L "\$URL/download/v\$REL/actions-runner-linux-\$1-\$REL.tar.gz"
          [[ \$? == 0 ]] && break
          sleep 5
        done
        tar xzf ./ghar.tgz

        # in case sth. is missing /TR
        sudo ./bin/installdependencies.sh
      }

      # download github-act-runner package
      function download_act_runner() {
        REL="v0.6.7"
        URL="https://github.com/ChristopherHX/github-act-runner/releases"
        TGZ="download/\$REL/binary-\$1.tar.gz"
        while :; do
          curl -s -o gar.tgz -L "\$URL/\$TGZ"
          [[ \$? == 0 ]] && break
          sleep 5
        done
        tar xzf ./gar.tgz
      }

      case $OS in
      debian*|ubuntu*)
        export DEBIAN_FRONTEND="noninteractive"
        sudo systemctl stop unattended-upgrades
        sudo systemctl disable unattended-upgrades
        download_action_runner "x64"
        ;;
      freebsd*)
        export ASSUME_ALWAYS_YES="YES"
        sudo pkg update -y
        download_act_runner "freebsd-amd64"
        ;;
      *)
        download_action_runner "x64"
        ;;
      esac

      # start action runner
      ./config.sh \
        --name "$ID" \
        --labels "$ID" \
        --replace \
        --ephemeral \
        --unattended \
        --pat "$GH" \
        --url "$REPO"
      exec /tmp/runner-start.sh
    path: /tmp/runner-init.sh
    permissions: '0755'
  - content: |
      #!/usr/bin/env bash

      exec 2>\$HOME/stderr-start.log
      cd \$HOME

      sudo rm -f /tmp/runner-init.sh
      # start testings
      ./run.sh
      sudo poweroff
    path: /tmp/runner-start.sh
    permissions: '0755'

runcmd:
  - sudo -u zfs /tmp/runner-init.sh
EOF

# we could extend this with more virtual disks for example /TR
echo "Starting machine for runner $ID ..."
sudo virt-install \
  --os-variant $OSv \
  --name "openzfs" \
  --cpu host-passthrough \
  --vcpus=4,sockets=1 \
  --memory 14336 \
  --graphics none \
  --network bridge=virbr0,model=virtio \
  --cloud-init user-data=/tmp/user-data \
  --disk $DISK,format=qcow2,bus=virtio \
  --import --noautoconsole
sudo rm -f /tmp/user-data
echo "Starting $ID -> result=$?"
