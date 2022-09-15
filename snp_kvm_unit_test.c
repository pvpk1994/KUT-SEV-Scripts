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
#define SEV_ES_GHCB_MSR_INDEX		0xc0010130

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
v	sidt(&idtr); // sidt is supposed to store contents of GDTR/IDT register in dest operand
	// Then UEFI's register val is being stored in idtr variable here. 

	idt = (idt_entry_t *)idtr.base; // idtr.base is the 1st element in idt_entry_t array
	vc_handler_idt = idt[SEV_ES_VC_HANDLER_VECTOR]; // get the #VC IR handler of UEFI
	vc_handler_idt.selector = KERNEL_CS; // get the IDT code segment

	boot_idt[SEV_ES_VC_HANDLER_VECTOR] = vc_handler_idt; // I think this is where it is 
	// getting copied into KVM-Unit-Test's IDT entry.

	// After copying, return success
	return EFI_SUCCESS;
}

/* SEV-ES introduces GHCB to conduct guest-hypervisor communication. This GHCB page must be
 * unencrypted (i.e., c-bit=0 / unset). Otherwise, guest VM will crash on #VC exception.

 * By default, KVM-unit-tests only support 2MiB pages, i.e., only level-2 PTE's are supported.
 * Unsetting GHCB level-2 pte's c-bit will still crash the guest. Therefore solution is to unset
 * level-1 pte's c-bit.

 * Steps:
 * 1. Finds GHCB level-1 PTE's.
 * 2. If none found, install level-1 pages.
 * 3. Unset GHCB level-1 PTE's c-bit.
 */
void setup_ghcb_pte(pgd_t *page_table)
{
	phys_addr_t ghcb_addr, ghcb_base_addr;
	pteval_t *pte;

	/* Read the GHCB page address @ C001_0130 */
	ghcb_addr = rdmsr(SEV_ES_GHCB_MSR_INDEX);

	/* Now search for level-1 PTE for obtained GHCB page */
	pte = get_pte_level(page_table, (void *)ghcb_addr, 1); // level-1

	/* If none level-1 PTEs found */
	if (pte == NULL) {

		/* Step-2: Install level-1 pages */
		/* For that, find level-2 page base addr */
		/* https://stackoverflow.com/questions/3023909/what-is-the-trick-in-paddress-page-size-1-to-get-the-pages-base-address */
		ghcb_base_addr = ghcb_addr & (LARGE_PAGE_SIZE -1);

		/* Install level-1 PTE's now */
		install_pages(page_table, ghcb_base_addr, LARGE_PAGE_SIZE, (void *)ghcb_base_addr);

		/* Find Level-2 pte, set as 4KB pages */
		pte = get_pte_level(page_table, (void *)ghcb_addr, 2);

		assert(pte);
		*pte = *pte & ~(PT_PAGE_SIZE_MASK); // (1ULL << 7)

		/* Find level-1 GHCB pte */
		pte = get_pte_level(page_table, (void*)ghcb_addr, 1);
		assert(pte);
	}

	/* Step-3: Unset c-bit in level-1 GHCB pte */
	*pte = *pte & ~(get_amd_c_bit_mask());
}

/* Before this patch update (below), SEV-ES guest crash when executing 'lgdt' instruction.
 * This is because lgdt triggers UEFI procedures (Ex: UEFI #VC handler) that requires UEFI
 * code and data segments. But these segments are not compatible with KUT's GDT:
 * UEFI uses 0x30 as CS and 0x38 as data segment, but in KUT's GDT: 0x30 is data segment
 * and 0x38 is a code segment. This discrepancy crashes the UEFI procedure and crashes the lgdt
 * execution.

 * Solution: copy UEFI GDT's code and data segments inot KUT's GDT, so UEFI procedures can work..
 */

static void copy_gdt_entry(gdt_entry_t *dst, gdt_entry_t *src, unsigned segment)
{
        unsigned index;

        index = segment / sizeof(gdt_entry_t);
        dst[index] = src[index];
}

/* Defined in x86/efi/efistart64.S */
extern gdt_entry_t gdt64[];

void copy_uefi_segments(void)
{
        if (!amd_sev_es_enabled()) {
                return;
	}

        /* GDT & GDTR in current UEFI */
        gdt_entry_t *gdt_curr;
        struct descriptor_table_ptr gdtr_curr;

        /* Copy code and data segments from UEFI */
        sgdt(&gdtr_curr);
        gdt_curr = (gdt_entry_t *)gdtr_curr.base;
        copy_gdt_entry(gdt64, gdt_curr, read_cs());
        copy_gdt_entry(gdt64, gdt_curr, read_ds());
}


