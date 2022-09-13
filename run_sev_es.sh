#!/bin/bash
#QEMU=/home/amd/Downloads/AMDSEV/usr/local/bin/qemu-system-x86_64
QEMU=/home/amd/Downloads/AMDSEV/snp-release-2022-08-22/usr/local/bin/qemu-system-x86_64
#OVMF=/home/amd/Downloads/AMDSEV/usr/local/share/qemu/OVMF_CODE.fd
OVMF=/home/amd/Downloads/AMDSEV/snp-release-2022-08-22/usr/local/share/qemu/OVMF_CODE.fd
KUT=/home/amd/Downloads/kvm-unit-tests

# Re-initialize the EFI directory
# pushd $KUT
# x86/efi/run x86/amd_sev

# PID=$!
# wait for 2 secs
# sleep 1
# kill the efi run test
# kill $PID
# popd

sudo $QEMU --no-reboot \
    -nodefaults -device pc-testdev -device isa-debug-exit,iobase=0xf4,iosize=0x4 \
    -vnc none -serial stdio -device pci-testdev -machine accel=kvm \
    -drive file=$OVMF,format=raw,if=pflash,readonly=on \
    -drive file.dir=efi-tests/amd_sev/,file.driver=vvfat,file.rw=on,format=raw,if=virtio \
    -net none -nographic -m 256 -smp 1 \
    -machine q35,confidential-guest-support=sev0 \
    -object sev-guest,id=sev0,cbitpos=51,reduced-phys-bits=1,policy=0x5 \
    -debugcon file:/tmp/debug.log 2>/tmp/qemu_sev_es_kut.err
