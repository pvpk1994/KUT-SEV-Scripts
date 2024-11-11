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
NUM_MEM_REG="$2"
VM_MEM_SIZE="$3"
MAX_MEM_HP=10240

offer_help() {
	echo "Usage: $0 [options]"
	echo
	echo "example: ./start_snp_hotplug.sh arg_1 arg_2 arg_3"
	echo "arg_1: Needs to have the same name as the QEMU's monitor name"
	echo "arg_2: Number of memory regions to be hotplugged"
	echo "arg_3: Memory size of each memory region (only in Megs)"
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

if [ $((NUM_MEM_REG * VM_MEM_SIZE)) -gt $MAX_MEM_HP ]; then
	echo "Number of memory regions to be hotplugged exceed the maximum memory allocated for hotplugging..."
	offer_help
fi

if [ -z "$GUEST_VM" ]; then
	echo "Please provide the name of the SNP guest VM that is currently running..."
	offer_help
fi

if [ -z "$NUM_MEM_REG" ]; then
	echo "Please provide the number of memory regions to be hotplugged..."
	offer_help
fi

if [ -z "$VM_MEM_SIZE" ]; then
	echo "Please provide the size of the memory to be hotplugged..."
	echo "NOTE: All memory regions will have the same amount of memory that will be hotplugged"
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
	for reg in $(seq 1 $NUM_MEM_REG)
	do
		issue_command "object_add memory-backend-memfd,id=mem$reg,size=${VM_MEM_SIZE}M"
		issue_command "device_add pc-dimm,id=dimm$reg,memdev=mem$reg"
		sleep 1
	done
}

# verify the addition of hotplugged memory via QEMU mtr
verify_hotplug() {
	# New memory-backend should be shown here
	issue_command "info memdev"
	# Check the status of guest VM
	issue_command "info status"
}

add_mem_block
echo "Memory hotplugging performed for $NUM_MEM_REG memory blocks"
verify_hotplug
