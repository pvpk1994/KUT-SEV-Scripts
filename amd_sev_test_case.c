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

int main(void)
{
	int rtn;
	rtn = test_sev_activation();
	report(rtn == EXIT_SUCCESS, "SEV activation test.");
	return report_summary(); // print either PASS/FAIL for tests
}