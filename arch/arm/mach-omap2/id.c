/*
 * linux/arch/arm/mach-omap2/id.c
 *
 * OMAP2 CPU identification code
 *
 * Copyright (C) 2005 Nokia Corporation
 * Written by Tony Lindgren <tony@atomide.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>

#include <asm/cputype.h>

#include <mach/common.h>
#include <mach/control.h>
#include <mach/cpu.h>

static struct omap_chip_id omap_chip;
static unsigned int omap_revision;

bool omap3_720mhz;

unsigned int omap_rev(void)
{
	return omap_revision;
}
EXPORT_SYMBOL(omap_rev);

/**
 * omap_chip_is - test whether currently running OMAP matches a chip type
 * @oc: omap_chip_t to test against
 *
 * Test whether the currently-running OMAP chip matches the supplied
 * chip type 'oc'.  Returns 1 upon a match; 0 upon failure.
 */
int omap_chip_is(struct omap_chip_id oci)
{
	return (oci.oc & omap_chip.oc) ? 1 : 0;
}
EXPORT_SYMBOL(omap_chip_is);

int omap_type(void)
{
	u32 val = 0;

	if (cpu_is_omap24xx()) {
		val = omap_ctrl_readl(OMAP24XX_CONTROL_STATUS);
	} else if (cpu_is_omap34xx()) {
		val = omap_ctrl_readl(OMAP343X_CONTROL_STATUS);
	} else {
		pr_err("Cannot detect omap type!\n");
		goto out;
	}

	val &= OMAP2_DEVICETYPE_MASK;
	val >>= 8;

out:
	return val;
}
EXPORT_SYMBOL(omap_type);


/*----------------------------------------------------------------------------*/

#define OMAP_TAP_IDCODE		0x0204
#define OMAP_TAP_DIE_ID_0	0x0218
#define OMAP_TAP_DIE_ID_1	0x021C
#define OMAP_TAP_DIE_ID_2	0x0220
#define OMAP_TAP_DIE_ID_3	0x0224

#define FEAT_OMAP3503	((OMAP343X_SGX_NONE << OMAP343X_FEATURE_SGX_SHIFT) | \
				OMAP343X_FEATURE_IVA2_HW_NONE)
#define FEAT_OMAP3515	OMAP343X_FEATURE_IVA2_HW_NONE
#define FEAT_OMAP3525	(OMAP343X_SGX_NONE << OMAP343X_FEATURE_SGX_SHIFT)
#define FEAT_OMAP3530	0

#define read_tap_reg(reg)	__raw_readl(tap_base  + (reg))

struct omap_id {
	u16	hawkeye;	/* Silicon type (Hawkeye id) */
	u8	dev;		/* Device type from production_id reg */
	u32	type;		/* Combined type id copied to omap_revision */
};

/* Register values to detect the OMAP version */
static struct omap_id omap_ids[] __initdata = {
	{ .hawkeye = 0xb5d9, .dev = 0x0, .type = 0x24200024 },
	{ .hawkeye = 0xb5d9, .dev = 0x1, .type = 0x24201024 },
	{ .hawkeye = 0xb5d9, .dev = 0x2, .type = 0x24202024 },
	{ .hawkeye = 0xb5d9, .dev = 0x4, .type = 0x24220024 },
	{ .hawkeye = 0xb5d9, .dev = 0x8, .type = 0x24230024 },
	{ .hawkeye = 0xb68a, .dev = 0x0, .type = 0x24300024 },
};

static void __iomem *tap_base;
static u16 tap_prod_id;

void __init omap24xx_check_revision(void)
{
	int i, j;
	u32 idcode, prod_id;
	u16 hawkeye;
	u8  dev_type, rev;

	idcode = read_tap_reg(OMAP_TAP_IDCODE);
	prod_id = read_tap_reg(tap_prod_id);
	hawkeye = (idcode >> 12) & 0xffff;
	rev = (idcode >> 28) & 0x0f;
	dev_type = (prod_id >> 16) & 0x0f;

	pr_debug("OMAP_TAP_IDCODE 0x%08x REV %i HAWKEYE 0x%04x MANF %03x\n",
		 idcode, rev, hawkeye, (idcode >> 1) & 0x7ff);
	pr_debug("OMAP_TAP_DIE_ID_0: 0x%08x\n",
		 read_tap_reg(OMAP_TAP_DIE_ID_0));
	pr_debug("OMAP_TAP_DIE_ID_1: 0x%08x DEV_REV: %i\n",
		 read_tap_reg(OMAP_TAP_DIE_ID_1),
		 (read_tap_reg(OMAP_TAP_DIE_ID_1) >> 28) & 0xf);
	pr_debug("OMAP_TAP_DIE_ID_2: 0x%08x\n",
		 read_tap_reg(OMAP_TAP_DIE_ID_2));
	pr_debug("OMAP_TAP_DIE_ID_3: 0x%08x\n",
		 read_tap_reg(OMAP_TAP_DIE_ID_3));
	pr_debug("OMAP_TAP_PROD_ID_0: 0x%08x DEV_TYPE: %i\n",
		 prod_id, dev_type);

	/* Check hawkeye ids */
	for (i = 0; i < ARRAY_SIZE(omap_ids); i++) {
		if (hawkeye == omap_ids[i].hawkeye)
			break;
	}

	if (i == ARRAY_SIZE(omap_ids)) {
		printk(KERN_ERR "Unknown OMAP CPU id\n");
		return;
	}

	for (j = i; j < ARRAY_SIZE(omap_ids); j++) {
		if (dev_type == omap_ids[j].dev)
			break;
	}

	if (j == ARRAY_SIZE(omap_ids)) {
		printk(KERN_ERR "Unknown OMAP device type. "
				"Handling it as OMAP%04x\n",
				omap_ids[i].type >> 16);
		j = i;
	}

	pr_info("OMAP%04x", omap_rev() >> 16);
	if ((omap_rev() >> 8) & 0x0f)
		pr_info("ES%x", (omap_rev() >> 12) & 0xf);
	pr_info("\n");
}

static u32 __init omap34xx_get_features(char *feat_name)
{
	u32 features, module;

	features = omap_ctrl_readl(OMAP343X_CONTROL_FEATURE_OMAP_STATUS) &
				(OMAP343X_FEATURE_SGX_MASK |
					OMAP343X_FEATURE_IVA2_HW_NONE);

	module = (features & OMAP343X_FEATURE_SGX_MASK) >>
					OMAP343X_FEATURE_SGX_SHIFT;
	switch (module) {
	case OMAP343X_SGX_FULL:
		strcat(feat_name, "full speed SGX, ");
		break;
	case OMAP343X_SGX_HALF:
		strcat(feat_name, "half speed SGX, ");
		break;
	case OMAP343X_SGX_NONE:
		strcat(feat_name, "no SGX, ");
		break;
	default:
		strcat(feat_name, "unknown SGX, ");
		break;
	}

	module = features & OMAP343X_FEATURE_IVA2_HW_NONE;
	switch (module) {
	case 0:
		strcat(feat_name, "IVA2");
		break;
	case OMAP343X_FEATURE_IVA2_HW_NONE:
		strcat(feat_name, "no IVA2");
		break;
	default:
		break;
	}

	/*
	 * Does it support 720MHz?
	 */
	omap3_720mhz = ((OMAP3_SKUID_MASK & read_tap_reg(OMAP3_PRODID))
				& OMAP3_SKUID_720MHZ) ? 1 : 0 ;

	return features;
}

static void __init omap34xx_set_revision(u8 rev, char *rev_name, char *features)
{
	u32 coprocessors;

	coprocessors = omap34xx_get_features(features);

	switch (rev) {
	case 0:
		omap_revision = OMAP3430_REV_ES2_0;
		strcat(rev_name, "ES2.0");
		break;
	case 2:
		omap_revision = OMAP3430_REV_ES2_1;
		strcat(rev_name, "ES2.1");
		break;
	case 3:
		omap_revision = OMAP3430_REV_ES3_0;
		strcat(rev_name, "ES3.0");
		break;
	case 4:
		omap_revision = OMAP3430_REV_ES3_1;
		strcat(rev_name, "ES3.1");
		break;
	default:
		/* Use the latest known revision as default */
		omap_revision = OMAP3430_REV_ES3_1;
		strcat(rev_name, "Unknown revision");
	}
}

static void __init omap35xx_set_revision(u8 rev, char *rev_name, char *features)
{
	u32 coprocessors;

	/* Get the subrevision based on the onboard coprocessors */
	coprocessors = omap34xx_get_features(features);
	switch (coprocessors) {
	case FEAT_OMAP3503:
		omap_revision |= OMAP3503_MASK;
		break;
	case FEAT_OMAP3515:
		omap_revision |= OMAP3515_MASK;
		break;
	case FEAT_OMAP3525:
		omap_revision |= OMAP3525_MASK;
		break;
	case FEAT_OMAP3530:
		omap_revision |= OMAP3530_MASK;
		break;
	default:
		break;
	}

	/* Get the silicon revision */
	switch (rev) {
	case 0:	/* Take care of some older boards */
	case 1:
		omap_revision |= OMAP35XX_MASK_ES2_0;
		strcat(rev_name, "ES2.0");
		break;
	case 2:
		omap_revision |= OMAP35XX_MASK_ES2_1;
		strcat(rev_name, "ES2.1");
		break;
	case 3:
		omap_revision |= OMAP35XX_MASK_ES3_0;
		strcat(rev_name, "ES3.0");
		break;
	case 4:
		omap_revision |= OMAP35XX_MASK_ES3_1;
		strcat(rev_name, "ES3.1");
		break;
	default:
		/* Use the latest known revision as default */
		omap_revision |= OMAP35XX_MASK_ES3_0;
		strcat(rev_name, "Unknown revision");
	}
}

void __init omap34xx_check_revision(void)
{
	u32 cpuid, idcode;
	u16 hawkeye;
	u8 rev;
	char rev_name[32] = "", feat_name[32] = "";

	/*
	 * We cannot access revision registers on ES1.0.
	 * If the processor type is Cortex-A8 and the revision is 0x0
	 * it means its Cortex r0p0 which is 3430 ES1.0.
	 */
	cpuid = read_cpuid(CPUID_ID);
	if ((((cpuid >> 4) & 0xfff) == 0xc08) && ((cpuid & 0xf) == 0x0)) {
		omap_revision = OMAP3430_REV_ES1_0;
		goto out;
	}

	/*
	 * Detection for 34xx ES2.0 and above can be done with just
	 * hawkeye and rev. See TRM 1.5.2 Device Identification.
	 * Note that rev does not map directly to our defined processor
	 * revision numbers as ES1.0 uses value 0.
	 */
	idcode = read_tap_reg(OMAP_TAP_IDCODE);
	hawkeye = (idcode >> 12) & 0xffff;
	rev = (idcode >> 28) & 0xff;

	if (hawkeye == 0xb7ae) {
		if (cpu_is_omap35xx())
			omap35xx_set_revision(rev, rev_name, feat_name);
		else
			omap34xx_set_revision(rev, rev_name, feat_name);
	}

out:
	pr_info("OMAP%04x %s (%s)\n", omap_rev() >> 16, rev_name, feat_name);
}

/*
 * Try to detect the exact revision of the omap we're running on
 */
void __init omap2_check_revision(void)
{
	/*
	 * At this point we have an idea about the processor revision set
	 * earlier with omap2_set_globals_tap().
	 */
	if (cpu_is_omap24xx())
		omap24xx_check_revision();
	else if (cpu_is_omap34xx())
		omap34xx_check_revision();
	else
		pr_err("OMAP revision unknown, please fix!\n");

	/*
	 * OK, now we know the exact revision. Initialize omap_chip bits
	 * for powerdowmain and clockdomain code.
	 */
	if (cpu_is_omap243x()) {
		/* Currently only supports 2430ES2.1 and 2430-all */
		omap_chip.oc |= CHIP_IS_OMAP2430;
	} else if (cpu_is_omap242x()) {
		/* Currently only supports 2420ES2.1.1 and 2420-all */
		omap_chip.oc |= CHIP_IS_OMAP2420;
	} else if (cpu_is_omap343x()) {
		omap_chip.oc = CHIP_IS_OMAP3430;
		if (omap_rev() == OMAP3430_REV_ES1_0)
			omap_chip.oc |= CHIP_IS_OMAP3430ES1;
		else if (omap_rev() > OMAP3430_REV_ES1_0)
			omap_chip.oc |= CHIP_IS_OMAP3430ES2;
	} else if (cpu_is_omap35xx()) {
		/* 35xx are treated as 3430ES2 for power and clockdomains */
		omap_chip.oc = CHIP_IS_OMAP3430;
		omap_chip.oc |= CHIP_IS_OMAP3430ES2;
	} else {
		pr_err("Uninitialized omap_chip, please fix!\n");
	}
}

/*
 * Set up things for map_io and processor detection later on. Gets called
 * pretty much first thing from board init. For multi-omap, this gets
 * cpu_is_omapxxxx() working accurately enough for map_io. Then we'll try to
 * detect the exact revision later on in omap2_detect_revision() once map_io
 * is done.
 */
void __init omap2_set_globals_tap(struct omap_globals *omap2_globals)
{
	omap_revision = omap2_globals->class;
	tap_base = omap2_globals->tap;

	if (cpu_is_omap34xx())
		tap_prod_id = 0x0210;
	else
		tap_prod_id = 0x0208;
}
