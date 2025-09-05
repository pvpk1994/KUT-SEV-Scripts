# Script assumes the following:
# qemu-system-x86_64 ... -trace enable=kvm_convert_memory,file=/tmp/trace.out
# This trace.out serves as input to this script

# Script then computes the following:
# - Total number of shared pages
# - Total number of private pages
# - Total number of 2M pages
# - Total number of 4K pages

import re

def determine_pg_size(hex_size, num_4k_pages, num_2m_pages):

	size_4k = 0x1000
	size_2m = 0x200000

	if hex_size // size_4k < size_2m // size_4k:
		num_4k_pages += hex_size // size_4k
	else:
		num_2m_pages += hex_size // size_2m

	return (num_4k_pages, num_2m_pages)

def extract_conversions(trace_file):
	num_shared_pages = 0
	num_private_pages = 0
	state_size = (0, 0)

	with open(trace_file, 'r') as f:
		lines = f.readlines()

		for line in lines:
			tokens = line.strip().split()
			for i, token in enumerate(tokens):
				if token == "size" and i+1 < len(tokens):
					state_size = determine_pg_size(int(tokens[i+1], 16), *state_size)

				if "_to_" in token and token == "private_to_shared":
					num_shared_pages += 1
				elif "_to_" in token and token == "shared_to_private":
					num_private_pages += 1

	return {
	"pvt_pages": num_private_pages,
	"shrd_pages": num_shared_pages,
	"4k_pages": state_size[0],
	"2m_pages": state_size[1]
	}

if __name__ == "__main__":
	trace_file = "/tmp/trace.out"
	dict_stats = extract_conversions(trace_file)
	print(f"Total number of shared pages: {dict_stats['shrd_pages']}")
	print(f"Total number of private pages: {dict_stats['pvt_pages']}")
	print(f"Total number of 4K pages: {dict_stats['4k_pages']}")
	print(f"Total number of 2M pages: {dict_stats['2m_pages']}")
