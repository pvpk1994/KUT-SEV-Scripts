#!/bin/bash

qemu=/home/amd/Documents/AMDSEV/snp-release-2023-03-31/usr/local/bin/qemu-system-x86_64
ovmf=/home/amd/Documents/AMDSEV/usr/local/share/qemu/OVMF_CODE.fd

sudo $qemu --no-reboot \
    -nodefaults -device pc-testdev -device isa-debug-exit,iobase=0xf4,iosize=0x4 \
    -vnc none -serial stdio -device pci-testdev -machine accel=kvm \
    -drive file=$ovmf,format=raw,if=pflash,readonly=on \
    -drive file.dir=efi-tests/amd_sev/,file.driver=vvfat,file.rw=on,format=raw,if=virtio \
    -net none -nographic -m 256 -smp 1 \
    -machine q35,confidential-guest-support=sev0 -cpu EPYC-v4 \
    -object sev-snp-guest,id=sev0,cbitpos=51,reduced-phys-bits=1 \
    -debugcon file:/tmp/debug.log 2>/home/amd/Documents/qemu.err    
