#include "libcflat.h"
#include "x86/processor.h"
#include "x86/amd_sev.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

static int test_sev_activation(void)
{
	if (!amd_sev_is_enabled())
		return EXIT_FAILURE;
	printf("SEV is supported and is enabled\n");
	return EXIT_SUCCESS;
}

/* SEV-ES introduces GHCB page for hv-guest commn.
 * This introduces #VC handler while accessing MSR, exec CPUID etc.,
 * This fn provides test cases to check if ES is enabled and rdmsr/wrmsr
 * are handled correctly in ES.
 */
static int test_sev_es_activation(void)
{
	if (!rdmsr(MSR_SEV_STATUS) & SEV_ES_ENABLED_MASK))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

static int test_sev_es_msr(void)
{
	/*
	 * For SEV-ES, rdmsr/wrmsr triggers #VC exception. If #VC is handled correctly,
	 * rdmsr/wrmsr should work fine without crashing the VM.
	 */
	 u64 val = 0x1234;
	 wrsmr(MSR_TSC_AUX, val);
	 if (val != rdmsr(MSR_TSC_AUX)) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}


/* QEMU build for this to work:
 * qemu/build/qemu-system-x86_64 \
 *		-object sev-guest, id=sev0, cbitpos=47, reduced-phys-bits=1, policy=0x3,
 *		-memory-encryption=sev0 --cpu EPYC-Milan
 * SEV-specific magic-flags only mentioned here. 
 */
int main(void)
{
	int rtn;

	/* SEV Testing */
	rtn = test_sev_activation();
	report(rtn == EXIT_SUCCESS, "SEV activation test.");

	/* SEV-ES Testing */
	rtn = test_sev_es_activation();
	report(rtn == EXIT_SUCCESS, "SEV-ES activation test.");

	/* SEV-ES MSR Testing */
	rtn = test_sev_es_msr();
	report(rtn == EXIT_SUCCESS, "SEV-ES MSR test.");

	return report_summary(); // print either PASS/FAIL for tests
}
