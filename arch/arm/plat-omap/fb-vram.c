/*
 * linux/arch/arm/plat-omap/fb-vram.c
 *
 * Copyright (C) 2008 Nokia Corporation
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
 *
 * Some code and ideas taken from drivers/video/omap/ driver
 * by Imre Deak.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//#define DEBUG

#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/bootmem.h>

#include <asm/setup.h>

#include <mach/sram.h>
#include <mach/omapfb.h>

#ifdef DEBUG
#define DBG(format, ...) printk(KERN_DEBUG "VRAM: " format, ## __VA_ARGS__)
#else
#define DBG(format, ...)
#endif

#define OMAP2_SRAM_START		0x40200000
/* Maximum size, in reality this is smaller if SRAM is partially locked. */
#define OMAP2_SRAM_SIZE			0xa0000		/* 640k */

#define REG_MAP_SIZE(_page_cnt) \
	((_page_cnt + (sizeof(unsigned long) * 8) - 1) / 8)
#define REG_MAP_PTR(_rg, _page_nr) \
	(((_rg)->map) + (_page_nr) / (sizeof(unsigned long) * 8))
#define REG_MAP_MASK(_page_nr) \
	(1 << ((_page_nr) & (sizeof(unsigned long) * 8 - 1)))

#if defined(CONFIG_FB_OMAP2) || defined(CONFIG_FB_OMAP2_MODULE)

/* postponed regions are used to temporarily store region information at boot
 * time when we cannot yet allocate the region list */
#define MAX_POSTPONED_REGIONS 10

static int postponed_cnt __initdata;
static struct {
	unsigned long paddr;
	size_t size;
} postponed_regions[MAX_POSTPONED_REGIONS] __initdata;

struct vram_alloc {
	struct list_head list;
	unsigned long paddr;
	unsigned pages;
};

struct vram_region {
	struct list_head list;
	struct list_head alloc_list;
	unsigned long	paddr;
	void		*vaddr;
	unsigned	pages;
	unsigned	dma_alloced:1;
};

static DEFINE_MUTEX(region_mutex);
static LIST_HEAD(region_list);

static inline int region_mem_type(unsigned long paddr)
{
	if (paddr >= OMAP2_SRAM_START &&
	    paddr < OMAP2_SRAM_START + OMAP2_SRAM_SIZE)
		return OMAPFB_MEMTYPE_SRAM;
	else
		return OMAPFB_MEMTYPE_SDRAM;
}

static struct vram_region *omap_vram_create_region(unsigned long paddr,
		void *vaddr, unsigned pages)
{
	struct vram_region *rm;

	rm = kzalloc(sizeof(*rm), GFP_KERNEL);

	if (rm) {
		INIT_LIST_HEAD(&rm->alloc_list);
		rm->paddr = paddr;
		rm->vaddr = vaddr;
		rm->pages = pages;
	}

	return rm;
}

static void omap_vram_free_region(struct vram_region *vr)
{
	list_del(&vr->list);
	kfree(vr);
}

static struct vram_alloc *omap_vram_create_allocation(struct vram_region *vr,
		unsigned long paddr, unsigned pages)
{
	struct vram_alloc *va;
	struct vram_alloc *new;

	new = kzalloc(sizeof(*va), GFP_KERNEL);

	if (!new)
		return NULL;

	new->paddr = paddr;
	new->pages = pages;

	list_for_each_entry(va, &vr->alloc_list, list) {
		if (va->paddr > new->paddr)
			break;
	}

	list_add_tail(&new->list, &va->list);

	return new;
}

static void omap_vram_free_allocation(struct vram_alloc *va)
{
	list_del(&va->list);
	kfree(va);
}

static __init int omap_vram_add_region_postponed(unsigned long paddr, size_t size)
{
	if (postponed_cnt == MAX_POSTPONED_REGIONS)
		return -ENOMEM;

	postponed_regions[postponed_cnt].paddr = paddr;
	postponed_regions[postponed_cnt].size = size;

	++postponed_cnt;

	return 0;
}

/* add/remove_region can be exported if there's need to add/remove regions
 * runtime */
static int omap_vram_add_region(unsigned long paddr, size_t size)
{
	struct vram_region *rm;
	void *vaddr;
	unsigned pages;

	DBG("adding region paddr %08lx size %d\n",
			paddr, size);

	size &= PAGE_MASK;
	pages = size >> PAGE_SHIFT;

	vaddr = ioremap_wc(paddr, size);
	if (vaddr == NULL)
		return -ENOMEM;

	rm = omap_vram_create_region(paddr, vaddr, pages);
	if (rm == NULL) {
		iounmap(vaddr);
		return -ENOMEM;
	}

	list_add(&rm->list, &region_list);

	return 0;
}

#if 0
int omap_vram_remove_region(unsigned long paddr)
{
	struct region *rm;
	unsigned i;

	DBG("remove region paddr %08lx\n", paddr);
	list_for_each_entry(rm, &region_list, list)
		if (rm->paddr != paddr)
			continue;

	if (rm->paddr != paddr)
		return -EINVAL;

	for (i = 0; i < rm->page_cnt; i++)
		if (region_page_reserved(rm, i))
			return -EBUSY;

	iounmap(rm->vaddr);

	list_del(&rm->list);

	kfree(rm);

	return 0;
}
#endif

int omap_vram_free(unsigned long paddr, void *vaddr, size_t size)
{
	struct vram_region *rm;
	struct vram_alloc *alloc;
	unsigned start, end;

	DBG("free mem paddr %08lx vaddr %p size %d\n",
			paddr, vaddr, size);

	size = PAGE_ALIGN(size);

	mutex_lock(&region_mutex);

	list_for_each_entry(rm, &region_list, list) {
		list_for_each_entry(alloc, &rm->alloc_list, list) {
			start = alloc->paddr;
			end = alloc->paddr + (alloc->pages >> PAGE_SHIFT);

			if (start >= paddr && end < paddr + size)
				goto found;
		}
	}

	mutex_unlock(&region_mutex);
	return -EINVAL;

found:
	if (rm->dma_alloced) {
		DBG("freeing dma-alloced\n");
		dma_free_writecombine(NULL, size, vaddr, paddr);
		omap_vram_free_allocation(alloc);
		omap_vram_free_region(rm);
	} else {
		omap_vram_free_allocation(alloc);
	}

	mutex_unlock(&region_mutex);
	return 0;
}
EXPORT_SYMBOL(omap_vram_free);

#if 0
void *omap_vram_reserve(unsigned long paddr, size_t size)
{

	struct region *rm;
	unsigned start_page;
	unsigned end_page;
	unsigned i;
	void *vaddr;

	size = PAGE_ALIGN(size);

	rm = region_find_region(paddr, size);

	DBG("reserve mem paddr %08lx size %d\n",
			paddr, size);

	BUG_ON(rm == NULL);

	start_page = (paddr - rm->paddr) >> PAGE_SHIFT;
	end_page = start_page + (size >> PAGE_SHIFT);
	for (i = start_page; i < end_page; i++)
		region_reserve_page(rm, i);

	vaddr = rm->vaddr + (start_page << PAGE_SHIFT);

	return vaddr;
}
EXPORT_SYMBOL(omap_vram_reserve);
#endif
static void *_omap_vram_alloc(int mtype, unsigned pages, unsigned long *paddr)
{
	struct vram_region *rm;
	struct vram_alloc *alloc;
	void *vaddr;

	list_for_each_entry(rm, &region_list, list) {
		unsigned long start, end;

		DBG("checking region %lx %d\n", rm->paddr, rm->pages);

		if (region_mem_type(rm->paddr) != mtype)
			continue;

		start = rm->paddr;

		list_for_each_entry(alloc, &rm->alloc_list, list) {
			end = alloc->paddr;

			if (end - start >= pages << PAGE_SHIFT)
				goto found;

			start = alloc->paddr + (alloc->pages << PAGE_SHIFT);
		}

		end = rm->paddr + (rm->pages << PAGE_SHIFT);
found:
		if (end - start < pages << PAGE_SHIFT)
			continue;

		DBG("FOUND %lx, end %lx\n", start, end);

		if (omap_vram_create_allocation(rm, start, pages) == NULL)
			return NULL;

		*paddr = start;
		vaddr = rm->vaddr + (start - rm->paddr);

		return vaddr;
	}

	return NULL;
}

static void *_omap_vram_alloc_dma(unsigned pages, unsigned long *paddr)
{
	struct vram_region *rm;
	void *vaddr;

	vaddr = dma_alloc_writecombine(NULL, pages << PAGE_SHIFT,
			(dma_addr_t *)paddr, GFP_KERNEL);

	if (vaddr == NULL)
		return NULL;

	rm = omap_vram_create_region(*paddr, vaddr, pages);
	if (rm == NULL) {
		dma_free_writecombine(NULL, pages << PAGE_SHIFT, vaddr,
				(dma_addr_t)*paddr);
		return NULL;
	}

	rm->dma_alloced = 1;

	if (omap_vram_create_allocation(rm, *paddr, pages) == NULL) {
		dma_free_writecombine(NULL, pages << PAGE_SHIFT, vaddr,
				(dma_addr_t)*paddr);
		kfree(rm);
		return NULL;
	}

	list_add(&rm->list, &region_list);

	return vaddr;
}

void *omap_vram_alloc(int mtype, size_t size, unsigned long *paddr)
{
	void *vaddr;
	unsigned pages;

	BUG_ON(mtype > OMAPFB_MEMTYPE_MAX || !size);

	DBG("alloc mem type %d size %d\n", mtype, size);

	size = PAGE_ALIGN(size);
	pages = size >> PAGE_SHIFT;

	mutex_lock(&region_mutex);

	vaddr = _omap_vram_alloc(mtype, pages, paddr);

	if (vaddr == NULL && mtype == OMAPFB_MEMTYPE_SDRAM) {
		DBG("fallback to dma_alloc\n");

		vaddr = _omap_vram_alloc_dma(pages, paddr);
	}

	mutex_unlock(&region_mutex);

	return vaddr;
}
EXPORT_SYMBOL(omap_vram_alloc);

#ifdef CONFIG_PROC_FS
static void *r_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct list_head *l = v;

	(*pos)++;

	if (list_is_last(l, &region_list))
		return 0;

	return l->next;
}

static void *r_start(struct seq_file *m, loff_t *pos)
{
	loff_t p = *pos;
	struct list_head *l = &region_list;

	mutex_lock(&region_mutex);

	do {
		l = l->next;
		if (l == &region_list)
			return NULL;
	} while (p--);

	return l;
}

static void r_stop(struct seq_file *m, void *v)
{
	mutex_unlock(&region_mutex);
}

static int r_show(struct seq_file *m, void *v)
{
	struct vram_region *vr;
	struct vram_alloc *va;
	unsigned size;

	vr = list_entry(v, struct vram_region, list);

	size = vr->pages << PAGE_SHIFT;
	seq_printf(m, "%08lx-%08lx v:%p-%p (%d bytes) %s\n",
			vr->paddr, vr->paddr + size,
			vr->vaddr, vr->vaddr + size,
			size,
			vr->dma_alloced ? "dma_alloc" : "");

	list_for_each_entry(va, &vr->alloc_list, list) {
		size = va->pages << PAGE_SHIFT;
		seq_printf(m, "    %08lx-%08lx (%d bytes)\n",
				va->paddr, va->paddr + size,
				size);
	}



	return 0;
}

static const struct seq_operations resource_op = {
	.start	= r_start,
	.next	= r_next,
	.stop	= r_stop,
	.show	= r_show,
};

static int vram_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &resource_op);
}

static const struct file_operations proc_vram_operations = {
	.open		= vram_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int __init omap_vram_create_proc(void)
{
	proc_create("omap-vram", 0, NULL, &proc_vram_operations);

	return 0;
}
#endif

static __init int omap_vram_init(void)
{
	int i, r;

	for (i = 0; i < postponed_cnt; i++)
		omap_vram_add_region(postponed_regions[i].paddr,
				postponed_regions[i].size);

#ifdef CONFIG_PROC_FS
	r = omap_vram_create_proc();
	if (r)
		return -ENOMEM;
#endif

	return 0;
}

arch_initcall(omap_vram_init);

/* boottime vram alloc stuff */
static u32 omapfb_sram_vram_start __initdata;
static u32 omapfb_sram_vram_size __initdata;

static u32 omapfb_sdram_vram_start __initdata;
static u32 omapfb_sdram_vram_size __initdata;

static u32 omapfb_def_sdram_vram_size __initdata;

static void __init omapfb_early_vram(char **p)
{
	unsigned long size;
	size  = memparse(*p, p);
	omapfb_def_sdram_vram_size = size;
}
__early_param("vram=", omapfb_early_vram);

/*
 * Called from map_io. We need to call to this early enough so that we
 * can reserve the fixed SDRAM regions before VM could get hold of them.
 */
void __init omapfb_reserve_sdram(void)
{
	struct bootmem_data	*bdata;
	unsigned long		sdram_start, sdram_size;
	unsigned long		reserved;
	u32 paddr;
	u32 size;

	bdata = NODE_DATA(0)->bdata;
	sdram_start = bdata->node_min_pfn << PAGE_SHIFT;
	sdram_size = (bdata->node_low_pfn << PAGE_SHIFT) - sdram_start;
	reserved = 0;

	/* cmdline arg overrides the board file definition */
	if (omapfb_def_sdram_vram_size) {
		size = omapfb_def_sdram_vram_size;
		paddr = 0;
	} else {
		size = omapfb_sdram_vram_size;
		paddr = omapfb_sdram_vram_start;
	}

	if (size) {
		if (paddr) {
			if (paddr < sdram_start ||
					paddr + size > sdram_start + sdram_size) {
				printk(KERN_ERR "Illegal SDRAM region for VRAM\n");
				return;
			}

			reserve_bootmem(paddr, size, BOOTMEM_DEFAULT);
		} else {
			if (size > sdram_size) {
				printk(KERN_ERR "Illegal SDRAM size for VRAM\n");
				return;
			}

			paddr = virt_to_phys(alloc_bootmem(size));
		}

		reserved += size;
		omap_vram_add_region_postponed(paddr, size);
	}

	if (reserved)
		pr_info("Reserving %lu bytes SDRAM for VRAM\n", reserved);
}

/*
 * Called at sram init time, before anything is pushed to the SRAM stack.
 * Because of the stack scheme, we will allocate everything from the
 * start of the lowest address region to the end of SRAM. This will also
 * include padding for page alignment and possible holes between regions.
 *
 * As opposed to the SDRAM case, we'll also do any dynamic allocations at
 * this point, since the driver built as a module would have problem with
 * freeing / reallocating the regions.
 */
unsigned long __init omapfb_reserve_sram(unsigned long sram_pstart,
				  unsigned long sram_vstart,
				  unsigned long sram_size,
				  unsigned long pstart_avail,
				  unsigned long size_avail)
{
	unsigned long			pend_avail;
	unsigned long			reserved;
	u32 paddr;
	u32 size;

	paddr = omapfb_sram_vram_start;
	size = omapfb_sram_vram_size;

	reserved = 0;
	pend_avail = pstart_avail + size_avail;


	if (!paddr) {
		/* Dynamic allocation */
		if ((size_avail & PAGE_MASK) < size) {
			printk(KERN_ERR "Not enough SRAM for VRAM\n");
			return 0;
		}
		size_avail = (size_avail - size) & PAGE_MASK;
		paddr = pstart_avail + size_avail;
	}

	if (paddr < sram_pstart ||
			paddr + size > sram_pstart + sram_size) {
		printk(KERN_ERR "Illegal SRAM region for VRAM\n");
		return 0;
	}

	/* Reserve everything above the start of the region. */
	if (pend_avail - paddr > reserved)
		reserved = pend_avail - paddr;
	size_avail = pend_avail - reserved - pstart_avail;

	/*
	 * We have a kernel mapping for this already, so the
	 * driver won't have to make one.
	 */
	/* XXX do we need the vaddr? */
	/* rg.vaddr = (void *)(sram_vstart + paddr - sram_pstart); */

	omap_vram_add_region_postponed(paddr, size);

	if (reserved)
		pr_info("Reserving %lu bytes SRAM for VRAM\n", reserved);

	return reserved;
}

void __init omap2_set_sdram_vram(u32 size, u32 start)
{
	omapfb_sdram_vram_start = start;
	omapfb_sdram_vram_size = size;
}

void __init omap2_set_sram_vram(u32 size, u32 start)
{
	omapfb_sram_vram_start = start;
	omapfb_sram_vram_size = size;
}

#endif

