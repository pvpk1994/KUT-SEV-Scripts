#ifndef _X86_AMD_SEV_H_
#define _X86_AMD_SEV_H_

/* Target EFI should have been setup by now */
#ifdef TARGET_EFI

#include "libcflat.h"
#include "desc.h"
#include "asm/page.h"
#include "efi.h"

/* APM Vol:3 */ 
#define CPUID_FN_LARGEST_EXT_FUNC_NUM 	0x80000000
#define CPUID_FN_ENCRYPT_MEM_CAPABILITY 0x8000001F
#define SEV_SUPPORT_BIT					0b10

/* APM Vol:2 */
#define SEV_ENABLE_MSR					0xc0010010
#define SEV_ENABLE_BIT					0b01
#define SEV_ES_ENABLE_BIT				0b10 // Bit 2
#define SEV_SNP_ENABLE_BIT				0b100 // Bit 3

bool amd_sev_is_enabled(void);
efi_status_t setup_amd_sev(void);

unsigned long long get_amd_bit_mask(void);

#endif // TARGET_EFI
#endif // _X86_AMD_SEV_H_