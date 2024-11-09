#!/bin/bash

# This script assumes the following components (qemu, ovmf, SNP-guest
# kernel (qcow2 image)) are all built and installed from
# AMDESE/snp-latest (https://github.com/AMDESE/AMDSEV/tree/snp-latest)

################## PREREQs ###################################
# Start SEV-SNP guest as follows using qemu-launch.sh
# ./launch-qemu.sh -hda {GUEST_VM}.qcow2 -sev-snp -monitor snp
##############################################################

# Run this script as follows: ./start_snp_hotplug.sh snp 512M
GUEST_VM="$1"
VM_MEM_SIZE="$2"

offer_help() {
	echo "Usage: $0 [options]"
	echo "example: ./start_snp_hotplug.sh VM_NAME MEM"
	echo
	echo "Options:"
	echo " -h, --help offers help and example usage"
	exit 1
}

# Run through the command line args provided to the script
while [ -n "$1" ]; do
	case "$1" in
		-h | --help)
			offer_help
			exit 0
			;;
	esac
	shift
done

if [ -z "$GUEST_VM" ]; then
	echo "Please provide the name of the SNP guest VM that is currently running..."
	offer_help
fi

if [ -z "$VM_MEM_SIZE" ]; then
	echo "Please provide the size of the memory to be hotplugged..."
	offer_help
fi

# Obtain the QEMU monitor
QEMU_MONITOR="$(pwd)/${GUEST_VM}"

# Issue commands to QEMU monitor
issue_command() {
	local cmd=$1
	# Install socat if not installed (apt install socat).
	echo "$cmd" | socat - UNIX-CONNECT:$QEMU_MONITOR
}

# Initiate memory hotplugging...
add_mem_block() {
	issue_command "object_add memory-backend-memfd,id=mem1,size=$VM_MEM_SIZE"
	issue_command "device_add pc-dimm,id=dimm1,memdev=mem1"
}

# verify the addition of hotplugged memory via QEMU mtr
verify_hotplug() {
	# New memory-backend should be shown here
	issue_command "info memdev"
	# Check the status of guest VM
	issue_command "info status"
}

add_mem_block
echo "Memory hotplugging performed. Added $VM_MEM_SIZE Megs to $GUEST_VM guest"
verify_hotplug
