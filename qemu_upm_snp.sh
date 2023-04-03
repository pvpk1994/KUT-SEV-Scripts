#!/bin/bash

# this OVMF was built from commit ca573b8615, after that Dionna's 
# lazy-acceptance got committed and they seem to be breaking KUT
ovmf=/home/amd/Downloads/AMDSEV/usr/local/share/qemu/OVMF_CODE.fd
kut=/home/amd/Documents/kvm-unit-tests
qemu_binary=/home/amd/Documents/AMDSEV/snp-release-2023-03-31/usr/local/bin/qemu-system-x86_64
#qemu_binary=$qemu_build/x86_64-softmmu/qemu-system-x86_64

if [ "$1" == "rebuild" ]; then
    pushd $kut
    rm -fr efi-tests/
    ./configure --target-efi
    make -j32
    popd
fi

# Re-initialize the EFI directory and run the tests
echo "Running KUT..."
pushd $kut
export EFI_UEFI=$ovmf
export QEMU=$qemu_binary
# SNP+UPM
x86/efi/run x86/amd_sev \
    -object memory-backend-memfd-private,id=ram1,size=256M,share=true \
    -machine q35,confidential-guest-support=sev0,memory-backend=ram1,kvm-type=protected \
    -object sev-snp-guest,id=sev0,cbitpos=51,reduced-phys-bits=1 \
    -cpu EPYC-v4 \
    -debugcon file:debug-kut-snp.log -global isa-debugcon.iobase=0x402
# SEV legacy
#x86/efi/run x86/amd_sev \
#    -machine q35,confidential-guest-support=sev0 \
#    -object sev-guest,id=sev0,cbitpos=51,reduced-phys-bits=1 \
#    -cpu EPYC-Milan-v2 \
#       -debugcon file:debug-kut-sev.log -global 
#       isa-debugcon.iobase=0x402
ret=$?
popd

exit $ret
