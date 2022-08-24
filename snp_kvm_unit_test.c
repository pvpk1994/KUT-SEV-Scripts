/* AMD SEV-SNP support for kvm-unit-tests
 * Author: Pavan Kumar Paluri
 */

#include "amd_sev.h"
#include <x86/processor.h>
#include <x86/vm.h>

#define PT_ADDR_UPPER_BOUND_DEFAULT 51

/* If UEFI is enabled */
#ifdef TARGET_EFI
#define PT_ADDR_UPPER_BOUND 	(get_amd_sev_addr_upperbound())
#else
#define PT_ADDR_UPPER_BOUND 	(PT_ADDR_UPPER_BOUND_DEFAULT)
#endif /* TARGET_EFI */

#define PT_ADDR_LOWER_BOUND		    (PAGE_SHIFT) /* 12 for PAGE_SIZE 4k in x-86 */

/* GENMASK_ULL(high, low) - sets address bits in between this range specified */
#define PT_ADDR_MASK				GENMASK_ULL(PT_ADDR_UPPER_BOUND, PT_ADDR_LOWER_BOUND)

/* APM Vol-2 - Section #VC exception */
#define SEV_ES_VC_HANDLER_VECTOR	29

/* Global Variable(s) */
static unsigned short amd_sev_c_bit_pos;

/* Step-1: Query SEV features 
 * Step-2: Check if SEV is supported
 * Step-3: Check if SEV is enabled 
 */

bool amd_sev_is_enabled(void)
{
	struct cpuid cpuid_output;
	static bool initialized = false;
	static bool sev_status = false;

	if(!initialized) {
		/* Step-1: Query SEV features */
		cpuid_output = cpuid(CPUID_FN_LARGEST_EXT_FUNC_NUM);

		/* value returned in EAX register provides largest extended fn number : cpuid_output.a */
		/* Largest function number returned in EAX register of 0x8000_0000h has to be greater than
	 	 * 0x8000_001Fh for memory encryption capability to be enabled
	 	 */
		if (cpuid_output.a < CPUID_FN_ENCRYPT_MEM_CAPABILITY)
			return sev_status; // false

		/* Step-2: Test if SEV is supported */
		cpuid_output = cpuid(CPUID_FN_ENCRYPT_MEM_CAPABILITY);
		if (!(cpuid_output.a & SEV_SUPPORT_BIT))
			return sev_status; // false

		/* Step-3: Test if SEV is enabled : Read SEV_STATUS MSR */
		if (readmsr(SEV_ENABLE_MSR) & SEV_ENABLE_BIT)
			sev_status = true; // SEV status is enabled after 3 checks

	return sev_status;
}


/* Setup c-bit for each page-table entry to enable SEV */
typedef unsigned long efi_status_t;
efi_status_t setup_amd_sev(void)
{
	struct cpuid cpuid_output;
	
	/* If SEV is disabled: EFI NOT SUPPORTED */
	if (!amd_sev_is_enabled)
		return EFI_UNSUPPORTED;

	/* Extract c-bit from CPUID Fn 8000_001F_EBX[5:0] */
	cpuid_output = cpuid(CPUID_FN_ENCRYPT_MEM_CAPABILITY);
	amd_sev_c_bit_pos = (unsigned short)(cpuid_output.b & 0x3f);

	return EFI_SUCCESS;
}

/* Having obtained C bit pos; now get the Bit mask of it */
unsigned long long get_amd_c_bit_mask(void)
{
	if (amd_sev_is_enabled())
		return 1ull << amd_sev_c_bit_pos;
	else
		return 0;
}


/* AMD SEV-ES Support for KVM-unit-tests */
bool amd_sev_es_enabled(void)
{
	static bool sev_es_enabled;
	static bool initialized = false;

	if (!initialized) {
		/* No point doing this - if in case UEFI later changes it, then we might need it (probably) */
		sev_es_enabled = false;
		initialized = true;

		/* If SEV is not enabled, do not enable ES */
		if (!amd_sev_is_enabled)
			return sev_es_enabled;

		/* Is it safe to bypass ES-support checks here? - Check for SEV support is sufficient 
		 * to know it is a SEV guest to say the least. Then proceed with MSR check for ES support.
		 */

		/* ES enable check */
		if (readmsr(SEV_ENABLE_MSR) & SEV_ES_ENABLE_BIT)
			sev_es_enabled = true;
	}
	return sev_es_enabled;
}

/* If AMD-SEV is enabled, upperbound of guest physical address is c_bit_pos-1
 * However, if AMD-SEV is not enabled, then upperbound of GPA is 51
 */
unsigned long long get_amd_sev_addr_upperbound(void)
{
	if (amd_sev_is_enabled())
		return amd_sev_c_bit_pos-1;
	else
		return PT_ADDR_UPPER_BOUND_DEFAULT
}

/* Setup AMD SEV-ES For KVM-unit-tests (Part of a different Patch) */ 
efi_status_t setup_amd_sev_es(void)
{
	/* This is same as struct idt_ptr_struct in x86 */
	/* This struct defines a ptr to an array of interrupt handlers */
	struct descriptor_table_ptr idtr; 
	idt_entry_t *idt; // Pointer to IDT entry
	idt_entry_t vc_handler_idt;

	// Check if AMD-SEV is enabled
	if (!amd_sev_is_enabled())
		return EFI_UNSUPPORTED;

	/* Copy UEFI's #VC IDT entry so that KVM unit tests can reuse it and does not have
	 * to re-implement a #VC handler. While at it, update the #VC IDT code segment to
	 * use KVM-unit-tests segments, KERNEL_CS, so that we do not have to copy the UEFI
	 * GDT entries to KVM-unit-tests GDT.
	 */
	sidt(&idtr); // sidt is supposed to store contents of GDTR/IDT register in dest operand
	// Then UEFI's register val is being stored in idtr variable here. 

	idt = (idt_entry_t *)idtr.base; // idtr.base is the 1st element in idt_entry_t array
	vc_handler_idt = idt[SEV_ES_VC_HANDLER_VECTOR]; // get the #VC IR handler of UEFI
	vc_handler_idt.selector = KERNEL_CS; // get the IDT code segment

	boot_idt[SEV_ES_VC_HANDLER_VECTOR] = vc_handler_idt; // I think this is where it is 
	// getting copied into KVM-Unit-Test's IDT entry.

	// After copying, return success
	return EFI_SUCCESS;
}

