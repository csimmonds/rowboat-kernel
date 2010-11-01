/*
 * arch/arm/mach-omap2/board-ti8168evm.c
 *
 * Code for TI8168 EVM.
 *
 * Copyright (C) 2010 Texas Instruments, Inc. - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c/pcf857x.h>
#include <linux/i2c/at24.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <linux/phy.h>
#include <linux/err.h>
#include <linux/clk.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/cacheflush.h>
#include <asm/fiq.h>

#include <plat/mcspi.h>
#include <plat/irqs.h>
#include <plat/mux.h>
#include <plat/usb.h>
#include <plat/board.h>
#include <plat/common.h>
#include <plat/timer-gp.h>
#include <plat/asp.h>
#include <plat/mmc.h>
#include <plat/dmtimer.h>
#include <plat/gpmc.h>
#include <plat/nand.h>
#include <mach/board-ti816x.h>
#include "clock.h"
#include "mux.h"
#include "hsmmc.h"


#if defined(CONFIG_MTD_PHYSMAP) || \
    defined(CONFIG_MTD_PHYSMAP_MODULE)
#define HAS_NOR 1
#else
#define HAS_NOR 0
#endif

/* Maximum ports supported by the PCF8575 */
#define VPS_PCF8575_MAX_PORTS           (2u)

/* Macros for accessing for ports */
#define VPS_PCF8575_PORT0               (0u)
#define VPS_PCF8575_PORT1               (1u)

/* Macros for PCF8575 Pins */
#define VPS_PCF8575_PIN0                (0x1)
#define VPS_PCF8575_PIN1                (0x2)
#define VPS_PCF8575_PIN2                (0x4)
#define VPS_PCF8575_PIN3                (0x8)
#define VPS_PCF8575_PIN4                (0x10)
#define VPS_PCF8575_PIN5                (0x20)
#define VPS_PCF8575_PIN6                (0x40)
#define VPS_PCF8575_PIN7                (0x80)

#define VPS_PCF8575_PIN10               (0x1)
#define VPS_PCF8575_PIN11               (0x2)

#define VPS_THS7375_MASK                (VPS_PCF8575_PIN10 | VPS_PCF8575_PIN11)

#define VPS_THS7360_SD_MASK             (VPS_PCF8575_PIN2 | VPS_PCF8575_PIN5)

#define VPS_THS7360_SF_MASK             (VPS_PCF8575_PIN0 |                    \
                                         VPS_PCF8575_PIN1 |                    \
                                         VPS_PCF8575_PIN3 |                    \
                                         VPS_PCF8575_PIN4)


/* SPI fLash information */
struct mtd_partition ti816x_spi_partitions[] = {
	/* All the partition sizes are listed in terms of NAND block size */
	{
		.name		= "U-Boot",
		.offset		= 0,	/* Offset = 0x0 */
		.size		= 64 * SZ_4K,
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	},
	{
		.name		= "U-Boot Env",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x40000 */
		.size		= 2 * SZ_4K,
	},
	{
		.name		= "Kernel",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x42000 */
		.size		= 640 * SZ_4K,
	},
	{
		.name		= "File System",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x2C2000 */
		.size		= MTDPART_SIZ_FULL,		/* size = 1.24 MiB */
	}
};

const struct flash_platform_data ti816x_spi_flash = {
	.type		= "w25x32",
	.name		= "spi_flash",
	.parts		= ti816x_spi_partitions,
	.nr_parts	= ARRAY_SIZE(ti816x_spi_partitions),
};

struct spi_board_info __initdata ti816x_spi_slave_info[] = {
	{
		.modalias	= "m25p80",
		.platform_data	= &ti816x_spi_flash,
		.irq		= -1,
		.max_speed_hz	= 75000000,
		.bus_num	= 1,
		.chip_select	= 0,
	},
};

static void __init ti816x_spi_init(void)
{
	spi_register_board_info(ti816x_spi_slave_info,
				ARRAY_SIZE(ti816x_spi_slave_info));
}



/* NAND flash information */
static struct mtd_partition ti816x_nand_partitions[] = {
	/* All the partition sizes are listed in terms of NAND block size */
	{
		.name		= "U-Boot",
		.offset		= 0,	/* Offset = 0x0 */
		.size		= 19 * SZ_128K,
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	},
	{
		.name		= "U-Boot Env",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x260000 */
		.size		= 1 * SZ_128K,
	},
	{
		.name		= "Kernel",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x280000 */
		.size		= 34 * SZ_128K,
	},
	{
		.name		= "File System",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x6C0000 */
		.size		= 1601 * SZ_128K,
	},
	{
		.name		= "Reserved",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0xCEE0000 */
		.size		= MTDPART_SIZ_FULL,
	},

};

static struct omap_nand_platform_data ti816x_nand_data = {
	.options	= NAND_BUSWIDTH_16,
	.parts		= ti816x_nand_partitions,
	.nr_parts	= ARRAY_SIZE(ti816x_nand_partitions),
	.dma_channel	= -1,		/* disable DMA in OMAP NAND driver */
	.nand_setup	= NULL,
	.dev_ready	= NULL,
};

static struct resource ti816x_nand_resource = {
	.flags		= IORESOURCE_MEM,
};

static struct platform_device ti816x_nand_device = {
	.name		= "omap2-nand",
	.id		= -1,
	.dev		= {
		.platform_data	= &ti816x_nand_data,
	},
	.num_resources	= 1,
	.resource	= &ti816x_nand_resource,
};


/* NOR flash information */
static struct mtd_partition ti816x_evm_norflash_partitions[] = {
	/* bootloader (U-Boot, etc) in first 5 sectors */
	{
		.name		= "bootloader",
		.offset		= 0,
		.size		= 2 * SZ_128K,
		.mask_flags	= MTD_WRITEABLE, /* force read-only */
	},
	/* bootloader params in the next 1 sectors */
	{
		.name		= "env",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= 0,
	},
	/* kernel */
	{
		.name		= "kernel",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 2 * SZ_2M,
		.mask_flags	= 0
	},
	/* file system */
	{
		.name		= "filesystem",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 25 * SZ_2M,
		.mask_flags	= 0
	},
	/* reserved */
	{
		.name		= "reserved",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
		.mask_flags	= 0
	}
};

static struct physmap_flash_data ti816x_evm_norflash_data = {
	.width		= 2,
	.parts		= ti816x_evm_norflash_partitions,
	.nr_parts	= ARRAY_SIZE(ti816x_evm_norflash_partitions),
};

#define TI816X_EVM_NOR_BASE		0x08000000
static struct resource ti816x_evm_norflash_resource = {
	.start		= TI816X_EVM_NOR_BASE,
	.end		= TI816X_EVM_NOR_BASE + SZ_64M - 1,
	.flags		= IORESOURCE_MEM,
};

static struct platform_device ti816x_evm_norflash_device = {
	.name		= "physmap-flash",
	.id		= 0,
	.dev		= {
		.platform_data	= &ti816x_evm_norflash_data,
	},
	.num_resources	= 1,
	.resource	= &ti816x_evm_norflash_resource,
};

static struct omap2_hsmmc_info mmc[] = {
	{
		.mmc		= 1,
		.wires		= 4,
		.gpio_cd	= -EINVAL,/* Dedicated pins for CD and WP */
		.gpio_wp	= -EINVAL,
		.ocr_mask	= MMC_VDD_33_34,
	},
	{}	/* Terminator */
};

static struct omap_musb_board_data musb_board_data = {
	.interface_type		= MUSB_INTERFACE_ULPI,
#ifdef CONFIG_USB_MUSB_OTG
	.mode           = MUSB_OTG,
#elif defined(CONFIG_USB_MUSB_HDRC_HCD)
	.mode           = MUSB_HOST,
#elif defined(CONFIG_USB_GADGET_MUSB_HDRC)
	.mode           = MUSB_PERIPHERAL,
#endif
	.power			= 500,
};

static struct platform_device vpss_device = {
	.name = "vpss",
	.id = -1,
	.dev = {
		.platform_data = NULL,
	},
};

static void __init ti8168_evm_init_irq(void)
{
	omap2_gp_clockevent_set_gptimer(2);
	omap2_init_common_hw(NULL, NULL);
	omap_init_irq();
}

static struct omap_board_config_kernel generic_config[] = {
};

int __init ti_ahci_register(u8 num_inst);


static struct at24_platform_data eeprom_info = {
	.byte_len       = (256*1024) / 8,
	.page_size      = 64,
	.flags          = AT24_FLAG_ADDR16,
};


/* FIX ME: Check the address of I2C expander */

static struct i2c_board_info __initdata ti816x_i2c_boardinfo0[] = {
	{
		I2C_BOARD_INFO("eeprom", 0x50),
		.platform_data	= &eeprom_info,
	},
	{
		I2C_BOARD_INFO("cpld", 0x23),
	},
	{
		I2C_BOARD_INFO("tlv320aic3x", 0x18),
	},
	{
		I2C_BOARD_INFO("IO Expander", 0x20),
	},

};

static struct i2c_board_info __initdata ti816x_i2c_boardinfo1[] = {
        {
		I2C_BOARD_INFO("pcf8575_1", 0x20),
	}
};

/* FIX ME: Check on the Bit Value */

#define TI816X_EVM_CIR_UART BIT(5)

static struct i2c_client *cpld_reg0_client;

static struct i2c_client *pcf8575_1_client;
static unsigned char pcf8575_port[2] = {0, 0};

int pcf8575_ths7375_enable(enum ti816x_ths_filter_ctrl ctrl)
{
	struct i2c_msg msg = {
			.addr = pcf8575_1_client->addr,
			.flags = 0,
			.len = 2,
		};

	pcf8575_port[1] &= ~VPS_THS7375_MASK;
	pcf8575_port[1] |= (ctrl & VPS_THS7375_MASK);
	msg.buf = pcf8575_port;

	return (i2c_transfer(pcf8575_1_client->adapter, &msg, 1));
}
EXPORT_SYMBOL(pcf8575_ths7375_enable);

int pcf8575_ths7360_sd_enable(enum ti816x_ths_filter_ctrl ctrl)
{
	struct i2c_msg msg = {
		.addr = pcf8575_1_client->addr,
		.flags = 0,
		.len = 2,
	};

	pcf8575_port[0] &= ~VPS_THS7360_SD_MASK;
	switch (ctrl)
	{
		case TI816X_THSFILTER_ENABLE_MODULE:
			pcf8575_port[0] &= ~(VPS_THS7360_SD_MASK);
			break;
		case TI816X_THSFILTER_BYPASS_MODULE:
			pcf8575_port[0] |= VPS_PCF8575_PIN2;
			break;
		case TI816X_THSFILTER_DISABLE_MODULE:
			pcf8575_port[0] |= VPS_THS7360_SD_MASK;
			break;
		default:
			return -EINVAL;
	}

	msg.buf = pcf8575_port;

	return (i2c_transfer(pcf8575_1_client->adapter, &msg, 1));
}
EXPORT_SYMBOL(pcf8575_ths7360_sd_enable);

int pcf8575_ths7360_hd_enable(enum ti816x_ths7360_sf_ctrl ctrl)
{
	struct i2c_msg msg = {
		.addr = pcf8575_1_client->addr,
		.flags = 0,
		.len = 2,
	};

	pcf8575_port[0] &= ~VPS_THS7360_SF_MASK;

	switch(ctrl)
	{
		case TI816X_THS7360_DISABLE_SF:
			pcf8575_port[0] |= VPS_PCF8575_PIN4;
			break;
		case TI816X_THS7360_BYPASS_SF:
			pcf8575_port[0] |= VPS_PCF8575_PIN3;
			break;
		case TI816X_THS7360_SF_SD_MODE:
			pcf8575_port[0] &= ~(VPS_THS7360_SF_MASK);
			break;
		case TI816X_THS7360_SF_ED_MODE:
			pcf8575_port[0] |= VPS_PCF8575_PIN0;
			break;
		case TI816X_THS7360_SF_HD_MODE:
			pcf8575_port[0] |= VPS_PCF8575_PIN1;
			break;
		case TI816X_THS7360_SF_TRUE_HD_MODE:
			pcf8575_port[0] |= VPS_PCF8575_PIN0|VPS_PCF8575_PIN1;
			break;
		default:
			return -EINVAL;
	}

	msg.buf = pcf8575_port;

	return (i2c_transfer(pcf8575_1_client->adapter, &msg, 1));
}
EXPORT_SYMBOL(pcf8575_ths7360_hd_enable);

static int pcf8575_video_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	pcf8575_1_client = client;
	return 0;
}

static int __devexit pcf8575_video_remove(struct i2c_client *client)
{
	pcf8575_1_client = NULL;
	return 0;
}

static const struct i2c_device_id pcf8575_video_id[] = {
        { "pcf8575_1", 0 },
        { }
};

static struct i2c_driver pcf8575_driver = {
        .driver = {
                .name   = "pcf8575_1",
        },
        .probe          = pcf8575_video_probe,
        .remove         = pcf8575_video_remove,
        .id_table       = pcf8575_video_id,
};

/* CPLD Register 0 Client: used for I/O Control */
static int cpld_reg0_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	u8 data;
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = &data,
		},
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &data,
		},
	};

	cpld_reg0_client = client;

	/* Clear UART CIR to enable cir operation. */
		i2c_transfer(client->adapter, msg, 1);
		data &= ~(TI816X_EVM_CIR_UART);
		i2c_transfer(client->adapter, msg + 1, 1);
	return 0;
}


static const struct i2c_device_id cpld_reg_ids[] = {
		{ "cpld_reg0", 0, },
		{ },
};

static struct i2c_driver ti816xevm_cpld_driver = {
	.driver.name    = "cpld_reg0",
	.id_table       = cpld_reg_ids,
	.probe          = cpld_reg0_probe,
};


static int __init ti816x_evm_i2c_init(void)
{
/*
There are two instances of I2C in TI 816x but currently only one instance
is used by TI 816x EVM. Registering a single isntance
*/
	omap_register_i2c_bus(1, 100, ti816x_i2c_boardinfo0,
		ARRAY_SIZE(ti816x_i2c_boardinfo0));
	omap_register_i2c_bus(2, 100, ti816x_i2c_boardinfo1,
		ARRAY_SIZE(ti816x_i2c_boardinfo1));
	return 0;
}

static u8 ti8168_iis_serializer_direction[] = {
	TX_MODE,	RX_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
};

static struct snd_platform_data ti8168_evm_snd_data = {
	.tx_dma_offset = 0x46800000,
	.rx_dma_offset = 0x46800000,
	.op_mode = DAVINCI_MCASP_IIS_MODE,
	.num_serializer = ARRAY_SIZE(ti8168_iis_serializer_direction),
	.tdm_slots = 2,
	.serial_dir = ti8168_iis_serializer_direction,
	.eventq_no = EVENTQ_3,
	.version = MCASP_VERSION_2,
	.txnumevt = 1,
	.rxnumevt = 1,
};

#ifdef CONFIG_TI816X_ERRATA_AXI2OCP
/*
 * dmtimer FIQ block: Use DMTIMER3 to generate FIQ every __us.
 */
#define ARM_FIQ_VEC	0xffff001c
#define FIQ_HDLR_SIZE	(12*4)		/* FIXME */


/**
 * dmtimer_setup() - Setup specified timer in periodic mode with given rate
 */
static int dmtimer_setup(int dmtimer_id, struct omap_dm_timer *dmtimer, unsigned
			int rate)
{
	u32 clk_rate, period;
	int src = OMAP_TIMER_SRC_SYS_CLK;

	WARN(IS_ERR_VALUE(omap_dm_timer_set_source(dmtimer, src)),
			"dmtimer_setup: set_source() failed\n");

	clk_rate = clk_get_rate(omap_dm_timer_get_fclk(dmtimer));

	pr_info("OMAP clockevent source: DMTIMER%d at %u Hz\n",
			dmtimer_id, clk_rate);

	period = clk_rate / rate;

	omap_dm_timer_set_int_enable(dmtimer, OMAP_TIMER_INT_OVERFLOW);
	omap_dm_timer_set_load_start(dmtimer, 1, 0xffffffff - period);

	return 0;
}

#define INTC_MIR_CLEAR0		0x0088
#define INTC_CONTROL		0x0048
#define INTC_ILR(irq)		(0x100 + (irq * 4))

#define INTC_FIQ_RST_BIT	BIT(1)
#define INTC_MAP_FIQ_BIT	BIT(0)

/**
 * intc_fiq_setup() - Hook up specified interrupt to FIQ
 */
static int intc_fiq_setup(int irq)
{
	void __iomem *base = TI816X_L4_SLOW_IO_ADDRESS(TI816X_ARM_INTC_BASE);
	int offset = irq & (~(32 - 1));

	__raw_writel(INTC_MAP_FIQ_BIT, base + INTC_ILR(irq));

	irq &= (32 - 1);
	__raw_writel((1 << irq), base + INTC_MIR_CLEAR0 + offset);

	return 0;
}

/**
 * dmtimer_fiq_hdlr() - FIQ handler installed @FIQ VECTOR
 *
 * Note1: Uses __attribute__ ((naked)) mainly to prevent clobber
 * Note2: Ignores interrupt status, assumes dmtimer4 as source
 * Note3: Forcing to use banked FIQ registers to avoid context save
 */
static void __naked dmtimer_fiq_hdlr(void)
{
#if 0
	volatile register unsigned int *reg1 asm("r8");
	volatile register unsigned int *reg2 asm("r11");

	reg1 = TI816X_L4_SLOW_IO_ADDRESS(0x48042000) + (0x28 & 0xff);
	reg2 = TI816X_L4_SLOW_IO_ADDRESS(TI816X_ARM_INTC_BASE) + INTC_CONTROL;
#endif

	/* FIXME: Assuming dmtimer4 in non-posted mode */

	asm __volatile__ (
		/*"ldr r9, [%[reg1]];\n\t"*/
		"mov r12, #0xfa000000\n\t"
		"orr r9, r12, #0x00040000\n\t"
		"orr r9, r9, #0x00002000\n\t"
		"orr r9, r9, #0x00000028\n\t"
		"mov r10, #2;\n\t"
		"str r10, [r9];\n\t"
		/*:: [reg1] "r" (reg1)*/
		);

	asm __volatile__ (
		/*"ldr r9, [%[reg2]];\n\t"*/
		"orr r9, r12, #0x00200000\n\t"
		"orr r9, r9, #0x00000048\n\t"
		"mov r10, #2;\n\t"
		"str r10, [r9];\n\t"
		/*:: [reg2] "r" (reg2)*/
		);

	asm __volatile__ (
		"subs pc, lr, #4\n\t"	/* FIXME */
		);
}

/**
 * axi2ocp_fiq_fixup() - Install and start 125us FIQ
 */
static int axi2ocp_fiq_fixup(void)
{
	struct omap_dm_timer *dmtimer_fiq;

	dmtimer_fiq = omap_dm_timer_request_specific(4);
	if (dmtimer_fiq == NULL) {
		pr_err("axi2ocp_fiq_fixup: failed to get dmtimer\n");
		return -1;
	}

	local_fiq_disable();

	intc_fiq_setup(omap_dm_timer_get_irq(dmtimer_fiq));

	/* FIXME: Put actual rate for 125us */
	dmtimer_setup(4, dmtimer_fiq, 10000);

	memcpy((void *)ARM_FIQ_VEC, (void *)dmtimer_fiq_hdlr, FIQ_HDLR_SIZE);
	flush_icache_range(ARM_FIQ_VEC, ARM_FIQ_VEC + FIQ_HDLR_SIZE);

	/*
	 * FIXME: Use CONFIG_FIQ and then init_FIQ(), generic set_fiq_handler(),
	 * set_fiq_regs(), enable_fiq() (take care of avoiding any side effects
	 * though)
	 */

	local_fiq_enable();

	pr_info("axi2ocp_fiq_fixup: FIQ for DMTIMER installed successfully\n");

	return 0;
}
#endif

static void __init ti816x_nor_init(void)
{
	if (HAS_NOR) {
		u8 cs = 0;
		u8 norcs = GPMC_CS_NUM + 1;

		/* find out the chip-select on which NAND exists */
		while (cs < GPMC_CS_NUM) {
			u32 ret = 0;
			ret = gpmc_cs_read_reg(cs, GPMC_CS_CONFIG1);

			if ((ret & 0xC00) == 0x0) {
				printk(KERN_INFO "Found NAND on CS%d\n", cs);
				if (norcs > GPMC_CS_NUM)
					norcs = cs;
			}
			cs++;
		}

		if (norcs > GPMC_CS_NUM) {
			printk(KERN_INFO "NOR: Unable to find configuration in GPMC\n");
			return;
		}

		if (norcs < GPMC_CS_NUM) {
			printk(KERN_INFO "Registering NOR on CS%d\n", norcs);
			if (platform_device_register(&ti816x_evm_norflash_device) < 0)
				printk(KERN_ERR "Unable to register NOR device\n");
		}
	}
}


static void __init ti816x_nand_init(void)
{
	u8 cs = 0;
	u8 nandcs = GPMC_CS_NUM + 1;
	u32 gpmc_base_add = TI816X_GPMC_VIRT;

	/* find out the chip-select on which NAND exists */
	while (cs < GPMC_CS_NUM) {
		u32 ret = 0;
		ret = gpmc_cs_read_reg(cs, GPMC_CS_CONFIG1);

		if ((ret & 0xC00) == 0x800) {
			printk(KERN_INFO "Found NAND on CS%d\n", cs);
			if (nandcs > GPMC_CS_NUM)
				nandcs = cs;
		}
		cs++;
	}

	if (nandcs > GPMC_CS_NUM) {
		printk(KERN_INFO "NAND: Unable to find configuration "
				 "in GPMC\n ");
		return;
	}

	if (nandcs < GPMC_CS_NUM) {
		ti816x_nand_data.cs = nandcs;
		ti816x_nand_data.phys_base = TI816X_GPMC_BASE;
		ti816x_nand_data.devsize = NAND_BUSWIDTH_16;
		ti816x_nand_data.gpmc_cs_baseaddr = (void *)
			(gpmc_base_add + GPMC_CS0_BASE + nandcs * GPMC_CS_SIZE);
		ti816x_nand_data.gpmc_baseaddr = (void *) (gpmc_base_add);

		printk(KERN_INFO "Registering NAND on CS%d\n", nandcs);
		if (platform_device_register(&ti816x_nand_device) < 0)
			printk(KERN_ERR "Unable to register NAND device\n");
	}
}

static void __init ti816x_vpss_init(void)
{
	/*FIXME add platform data here*/

	i2c_add_driver(&pcf8575_driver);

	if (platform_device_register(&vpss_device))
		printk(KERN_ERR "failed to register ti816x_vpss device\n");
	else
		printk(KERN_INFO "registered ti816x_vpss device \n");
}

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#else
#define board_mux	NULL
#endif


#ifdef CONFIG_USB_ANDROID

#include <linux/usb/android_composite.h>

#define GOOGLE_VENDOR_ID                0x18d1
#define GOOGLE_PRODUCT_ID               0x9018
#define GOOGLE_ADB_PRODUCT_ID           0x9015

static char *usb_functions_adb[] = {
	"adb",
};

static char *usb_functions_all[] = {
	"adb",
};

static struct android_usb_product usb_products[] = {
	{
		.product_id     = GOOGLE_PRODUCT_ID,
		.num_functions  = ARRAY_SIZE(usb_functions_adb),
		.functions      = usb_functions_adb,
	},
};

static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id      = GOOGLE_VENDOR_ID,
	.product_id     = GOOGLE_PRODUCT_ID,
	.functions      = usb_functions_all,
	.products       = usb_products,
	.version        = 0x0100,
	.product_name   = "rowboat gadget",
	.manufacturer_name      = "rowboat",
	.serial_number  = "20100720",
	.num_functions  = ARRAY_SIZE(usb_functions_all),
};

static struct platform_device androidusb_device = {
	.name   = "android_usb",
	.id     = -1,
	.dev    = {
		.platform_data = &android_usb_pdata,
	},
};

static void ti8168evm_android_gadget_init(void)
{
	platform_device_register(&androidusb_device);
}

#endif


static void __init ti8168_evm_init(void)
{
	ti816x_mux_init(board_mux);
	omap_board_config = generic_config;
	omap_board_config_size = ARRAY_SIZE(generic_config);
	omap_serial_init();

	omap2_hsmmc_init(mmc);
	/* initialize usb */
	usb_musb_init(&musb_board_data);
	/* register ahci interface for 2 SATA ports */
	ti_ahci_register(2);

	ti816x_evm_i2c_init();
	i2c_add_driver(&ti816xevm_cpld_driver);
	ti816x_nand_init();
	ti816x_nor_init();
	ti816x_spi_init();
	ti816x_register_mcasp(0, &ti8168_evm_snd_data);
	ti816x_vpss_init();

#ifdef CONFIG_TI816X_ERRATA_AXI2OCP
	/* FIXME: Move further up in initialization sequence */
	pr_info("AXI2OCP: Entering fixup code...\n");
	axi2ocp_fiq_fixup();
	pr_info("AXI2OCP: done with fixup\n");
#endif

#ifdef CONFIG_USB_ANDROID
        ti8168evm_android_gadget_init();
#endif
}

static void __init ti8168_evm_map_io(void)
{
	omap2_set_globals_ti816x();
	ti816x_map_common_io();
}

MACHINE_START(TI8168_EVM, "ti8168evm")
	/* Maintainer: Texas Instruments */
	.phys_io	= 0x48000000,
	.io_pg_offst	= ((0xfa000000) >> 18) & 0xfffc,
	.boot_params	= 0x80000100,
	.map_io		= ti8168_evm_map_io,
	.reserve         = ti81xx_reserve,
	.init_irq	 = ti8168_evm_init_irq,
	.init_machine	= ti8168_evm_init,
	.timer		= &omap_timer,
MACHINE_END
