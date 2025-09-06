/*
 * SEV-SNP guest module to determine if a page has its encryption bit set or not.
 * Accordingly determine if the page is a shared page or a private one.
 * While at it, also output its contents when it is encrypted vs decrypted.
 *
 * Procedure to run:
 * insmod get_enc_stat.ko
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

extern int set_memory_decrypted(unsigned long addr, int numpages);
extern void msleep(unsigned int msecs);

static char *addr;
static const char *msg = "CREDIT CARD 0123 4567 8910 1112";

static struct cmd_show_struct {
	int cmd;
	unsigned long pte_entry;
} cmd_and_show;

static int dumpsetpte_show(struct seq_file *m, void *v)
{
	struct cmd_show_struct *c = (struct cmd_show_struct *)m->private;
	seq_printf(m, "0x%lx\n", c->pte_entry);
	return 0;
}

static int dumpsetpte_open(struct inode *i, struct file *f)
{
	return single_open(f, dumpsetpte_show, pde_data(i));
}

static ssize_t dumpsetpte_write(struct file *file, const char __user *buf,
				size_t count, loff_t *pos)
{
	char cmd[3];
	size_t len = min(count, sizeof(cmd));

	if (copy_from_user(cmd, buf, len))
		return -EFAULT;

	cmd[len] = '\0';
	sscanf(cmd, "%d", &cmd_and_show.cmd);

	return count;
}

static const struct proc_ops dumpsetpte_ops = {
	.proc_open		=	dumpsetpte_open,
	.proc_read		= 	seq_read,
	.proc_lseek		=	seq_lseek,
	.proc_release		=	single_release,
	.proc_write		=	dumpsetpte_write,
};

static struct proc_dir_entry *dir;

static int __init begin_enc_stat(void)
{
	pr_notice("Entering %s\n", __func__);
	static struct page *page;
	int level;
	pgd_t *pgd;
	pte_t *pte;
	int ret;

	/* Initiate a function pointer */
	pte_t * (*lookup_address_in_pgd)(pgd_t *pgd, unsigned long address, unsigned int *level);

	/* lookup the address for lookup_address_in_pgd via kallsyms_lookup_name */
	lookup_address_in_pgd = (pte_t * (*) (pgd_t *, unsigned long, unsigned int *))
				kallsyms_lookup_name("lookup_address_in_pgd");

	if (!lookup_address_in_pgd) {
		pr_err("Failed to resolve lookup_address_in_pgd via kallsyms\n");
		return -EINVAL;
	}

	/* Get the dir for module setup i.e., /proc/getsetpte */
	dir = proc_mkdir("getsetpte", NULL);
	if (!dir)
		return -EINVAL;

	/* Now the file: /proc/getsetpte/pte */
	if (!proc_create_data("pte", S_IRUSR|S_IWUSR, dir, &dumpsetpte_ops,
				(void *)&cmd_and_show)) {
		pr_err("Failed to create /proc/getspte/pte\n");
		goto err;
	}

	/* Allocate a zero-initialized page */
	page = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (page) {
		addr = page_address(page);

		pgd = __va(read_cr3_pa());

		/* Same as using pgd_offset() since pgd points to base of PT */
		pgd += pgd_index((unsigned long)addr);
		pte = lookup_address_in_pgd((pgd_t *)pgd, (unsigned long)addr, &level);

		if (!pte) {
			pr_err("Failed to obtain pte for gpa=0x%lx\n", (unsigned long)addr);
			goto err1;
		}

		strcpy(addr, msg);
		pr_notice("*SECRET: %s\n", addr);
		cmd_and_show.pte_entry = pte;
		pr_notice("*pte=0x%lx level=%d\n", pte, level);

		while(!cmd_and_show.cmd)
			msleep(100);

		if (!cmd_and_show.cmd || cmd_and_show.cmd > 1)
			goto skip;

		ret = set_memory_decrypted((unsigned long)addr, 1);
		pgd = __va(read_cr3_pa());
		pgd += pgd_index((unsigned long)addr);
		pte = lookup_address_in_pgd((pgd_t *)pgd, (unsigned long)addr, &level);
		if (!pte) {
			pr_err("Failed to get pte for gpa=0x%lx\n", (unsigned long)addr);
			goto err1;
		}

		pr_notice("*pte=0x%lx level=%d\n", pte, level);
		strcpy(addr, msg);
		pr_notice("*SECRET: %s\n", addr);
	} else
		goto err;

	pr_notice("exiting %s\n\n", __func__);
	return 0;

/* For non-graceful exits */
/* Else, clearing out is done in end_enc_stat() */
skip:
err1:
	free_page((unsigned long)addr);
	addr = NULL;
err:
	if (remove_proc_subtree("getsetpte", NULL))
		pr_notice("exit: Could not remove /proc/getsetpte\n");
	return -1;

}

module_init(begin_enc_stat);

module_exit(end_enc_stat);

MODULE_LICENSE("GPL");

