/* Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 * SOC Info Routines
 *
 */

#include <linux/types.h>
#include <linux/sysdev.h>
#include <asm/mach-types.h>
#include <mach/socinfo.h>

#include "smd_private.h"

// +++ ASUS_BSP : miniporting : support platform info in kernel
#include <linux/kernel.h>
struct asus_platform_info g_asus_plat_info={
	.cpu_ver = "Unknown",
	.pmic_ver = "Unknown",
	.hdcp=0,
	.cpu_revision = 0x0,  //Unknown
	.cpu_type = 0x0, //Unknown
	.dc=0,
};

char *CPU_VER[] = {
	[CPU_REV_UNKNOWN] = "CPU_REV_UNKNOWN",
	[CPU_REV_V1] = "V1",
	[CPU_REV_V2] = "V2",
	[CPU_REV_V3] = "V3",
	[CPU_REV_V4] = "V4",
	[CPU_REV_V5] = "V5",
	[CPU_REV_V6] = "V6",
};
char *PMIC_VER[] = {
	[PMIC_VER_UNKNOWN] = "PMIC_Unknown",
	[PMIC_VER_V1] = "PMIC_V1.1",
	[PMIC_VER_V2] = "PMIC_V2.0",
	[PMIC_VER_V3] = "PMIC_V3.0",
	[PMIC_VER_V4] = "PMIC_V4.0",
	[PMIC_VER_V5] = "PMIC_V5.0",
	[PMIC_VER_V6] = "PMIC_V6.0",
};
// --- ASUS_BSP : miniporting : support platform info in kernel

#define BUILD_ID_LENGTH 32

enum {
	HW_PLATFORM_UNKNOWN = 0,
	HW_PLATFORM_SURF    = 1,
	HW_PLATFORM_FFA     = 2,
	HW_PLATFORM_FLUID   = 3,
	HW_PLATFORM_SVLTE_FFA	= 4,
	HW_PLATFORM_SVLTE_SURF	= 5,
	HW_PLATFORM_MTP  = 8,
	HW_PLATFORM_LIQUID  = 9,
	/* Dragonboard platform id is assigned as 10 in CDT */
	HW_PLATFORM_DRAGON	= 10,
	HW_PLATFORM_QRD	= 11,
	HW_PLATFORM_INVALID
};

const char *hw_platform[] = {
	[HW_PLATFORM_UNKNOWN] = "Unknown",
	[HW_PLATFORM_SURF] = "Surf",
	[HW_PLATFORM_FFA] = "FFA",
	[HW_PLATFORM_FLUID] = "Fluid",
	[HW_PLATFORM_SVLTE_FFA] = "SVLTE_FFA",
	[HW_PLATFORM_SVLTE_SURF] = "SLVTE_SURF",
	[HW_PLATFORM_MTP] = "MTP",
	[HW_PLATFORM_LIQUID] = "Liquid",
	[HW_PLATFORM_DRAGON] = "Dragon",
	[HW_PLATFORM_QRD] = "QRD",
};

enum {
	ACCESSORY_CHIP_UNKNOWN = 0,
	ACCESSORY_CHIP_CHARM = 58,
};

enum {
	PLATFORM_SUBTYPE_UNKNOWN = 0x0,
	PLATFORM_SUBTYPE_CHARM = 0x1,
	PLATFORM_SUBTYPE_STRANGE = 0x2,
	PLATFORM_SUBTYPE_STRANGE_2A = 0x3,
	PLATFORM_SUBTYPE_INVALID,
};

const char *hw_platform_subtype[] = {
	[PLATFORM_SUBTYPE_UNKNOWN] = "Unknown",
	[PLATFORM_SUBTYPE_CHARM] = "charm",
	[PLATFORM_SUBTYPE_STRANGE] = "strange",
	[PLATFORM_SUBTYPE_STRANGE_2A] = "strange_2a,"
};

/* Used to parse shared memory.  Must match the modem. */
struct socinfo_v1 {
	uint32_t format;
	uint32_t id;
	uint32_t version;
	char build_id[BUILD_ID_LENGTH];
};

struct socinfo_v2 {
	struct socinfo_v1 v1;

	/* only valid when format==2 */
	uint32_t raw_id;
	uint32_t raw_version;
};

struct socinfo_v3 {
	struct socinfo_v2 v2;

	/* only valid when format==3 */
	uint32_t hw_platform;
};

struct socinfo_v4 {
	struct socinfo_v3 v3;

	/* only valid when format==4 */
	uint32_t platform_version;
};

struct socinfo_v5 {
	struct socinfo_v4 v4;

	/* only valid when format==5 */
	uint32_t accessory_chip;
};

struct socinfo_v6 {
	struct socinfo_v5 v5;

	/* only valid when format==6 */
	uint32_t hw_platform_subtype;
};

struct socinfo_v7 {
	struct socinfo_v6 v6;

	/* only valid when format==7 */
	uint32_t pmic_model;
	uint32_t pmic_die_revision;
};

static union {
	struct socinfo_v1 v1;
	struct socinfo_v2 v2;
	struct socinfo_v3 v3;
	struct socinfo_v4 v4;
	struct socinfo_v5 v5;
	struct socinfo_v6 v6;
	struct socinfo_v7 v7;
} *socinfo;

static enum msm_cpu cpu_of_id[] = {

	/* 7x01 IDs */
	[1]  = MSM_CPU_7X01,
	[16] = MSM_CPU_7X01,
	[17] = MSM_CPU_7X01,
	[18] = MSM_CPU_7X01,
	[19] = MSM_CPU_7X01,
	[23] = MSM_CPU_7X01,
	[25] = MSM_CPU_7X01,
	[26] = MSM_CPU_7X01,
	[32] = MSM_CPU_7X01,
	[33] = MSM_CPU_7X01,
	[34] = MSM_CPU_7X01,
	[35] = MSM_CPU_7X01,

	/* 7x25 IDs */
	[20] = MSM_CPU_7X25,
	[21] = MSM_CPU_7X25, /* 7225 */
	[24] = MSM_CPU_7X25, /* 7525 */
	[27] = MSM_CPU_7X25, /* 7625 */
	[39] = MSM_CPU_7X25,
	[40] = MSM_CPU_7X25,
	[41] = MSM_CPU_7X25,
	[42] = MSM_CPU_7X25,
	[62] = MSM_CPU_7X25, /* 7625-1 */
	[63] = MSM_CPU_7X25, /* 7225-1 */
	[66] = MSM_CPU_7X25, /* 7225-2 */


	/* 7x27 IDs */
	[43] = MSM_CPU_7X27,
	[44] = MSM_CPU_7X27,
	[61] = MSM_CPU_7X27,
	[67] = MSM_CPU_7X27, /* 7227-1 */
	[68] = MSM_CPU_7X27, /* 7627-1 */
	[69] = MSM_CPU_7X27, /* 7627-2 */


	/* 8x50 IDs */
	[30] = MSM_CPU_8X50,
	[36] = MSM_CPU_8X50,
	[37] = MSM_CPU_8X50,
	[38] = MSM_CPU_8X50,

	/* 7x30 IDs */
	[59] = MSM_CPU_7X30,
	[60] = MSM_CPU_7X30,

	/* 8x55 IDs */
	[74] = MSM_CPU_8X55,
	[75] = MSM_CPU_8X55,
	[85] = MSM_CPU_8X55,

	/* 8x60 IDs */
	[70] = MSM_CPU_8X60,
	[71] = MSM_CPU_8X60,
	[86] = MSM_CPU_8X60,

	/* 8960 IDs */
	[87] = MSM_CPU_8960,

	/* 7x25A IDs */
	[88] = MSM_CPU_7X25A,
	[89] = MSM_CPU_7X25A,
	[96] = MSM_CPU_7X25A,

	/* 7x27A IDs */
	[90] = MSM_CPU_7X27A,
	[91] = MSM_CPU_7X27A,
	[92] = MSM_CPU_7X27A,
	[97] = MSM_CPU_7X27A,

	/* FSM9xxx ID */
	[94] = FSM_CPU_9XXX,
	[95] = FSM_CPU_9XXX,

	/*  7x25AA ID */
	[98] = MSM_CPU_7X25AA,
	[99] = MSM_CPU_7X25AA,
	[100] = MSM_CPU_7X25AA,

	/*  7x27AA ID */
	[101] = MSM_CPU_7X27AA,
	[102] = MSM_CPU_7X27AA,
	[103] = MSM_CPU_7X27AA,
	[136] = MSM_CPU_7X27AA,

	/* 9x15 ID */
	[104] = MSM_CPU_9615,
	[105] = MSM_CPU_9615,
	[106] = MSM_CPU_9615,
	[107] = MSM_CPU_9615,

	/* 8064 IDs */
	[109] = MSM_CPU_8064,

	/* 8930 IDs */
	[116] = MSM_CPU_8930,
	[117] = MSM_CPU_8930,
	[118] = MSM_CPU_8930,
	[119] = MSM_CPU_8930,
	[179] = MSM_CPU_8930,

	/* 8627 IDs */
	[120] = MSM_CPU_8627,
	[121] = MSM_CPU_8627,

	/* 8660A ID */
	[122] = MSM_CPU_8960,

	/* 8260A ID */
	[123] = MSM_CPU_8960,

	/* 8060A ID */
	[124] = MSM_CPU_8960,

	/* 8974 IDs */
	[126] = MSM_CPU_8974,

	/* 8625 IDs */
	[127] = MSM_CPU_8625,
	[128] = MSM_CPU_8625,
	[129] = MSM_CPU_8625,

	/* 8064 MPQ ID */
	[130] = MSM_CPU_8064,

	/* 7x25AB IDs */
	[131] = MSM_CPU_7X25AB,
	[132] = MSM_CPU_7X25AB,
	[133] = MSM_CPU_7X25AB,
	[135] = MSM_CPU_7X25AB,

	/* 9625 IDs */
	[134] = MSM_CPU_9625,

	/* 8960AB IDs */
	[138] = MSM_CPU_8960AB,
	[139] = MSM_CPU_8960AB,
	[140] = MSM_CPU_8960AB,
	[141] = MSM_CPU_8960AB,

	/* 8930AA IDs */
	[142] = MSM_CPU_8930AA,
	[143] = MSM_CPU_8930AA,
	[144] = MSM_CPU_8930AA,
	[160] = MSM_CPU_8930AA,
	[180] = MSM_CPU_8930AA,

	/* 8226 IDs */
	[145] = MSM_CPU_8226,

	/* 8092 IDs */
	[146] = MSM_CPU_8092,

	/* 8064AB IDs */
	[153] = MSM_CPU_8064AB,

	/* 8930AB IDs */
	[154] = MSM_CPU_8930AB,
	[155] = MSM_CPU_8930AB,
	[156] = MSM_CPU_8930AB,
	[157] = MSM_CPU_8930AB,
	[181] = MSM_CPU_8930AB,

	/* 8064AA IDs */
	[172] = MSM_CPU_8064AA,

	/* Uninitialized IDs are not known to run Linux.
	   MSM_CPU_UNKNOWN is set to 0 to ensure these IDs are
	   considered as unknown CPU. */
};

static enum msm_cpu cur_cpu;

static struct socinfo_v1 dummy_socinfo = {
	.format = 1,
	.version = 1,
};

uint32_t socinfo_get_id(void)
{
	return (socinfo) ? socinfo->v1.id : 0;
}
EXPORT_SYMBOL_GPL(socinfo_get_id);

uint32_t socinfo_get_version(void)
{
	return (socinfo) ? socinfo->v1.version : 0;
}

char *socinfo_get_build_id(void)
{
	return (socinfo) ? socinfo->v1.build_id : NULL;
}

uint32_t socinfo_get_raw_id(void)
{
	return socinfo ?
		(socinfo->v1.format >= 2 ? socinfo->v2.raw_id : 0)
		: 0;
}

uint32_t socinfo_get_raw_version(void)
{
	return socinfo ?
		(socinfo->v1.format >= 2 ? socinfo->v2.raw_version : 0)
		: 0;
}

uint32_t socinfo_get_platform_type(void)
{
	return socinfo ?
		(socinfo->v1.format >= 3 ? socinfo->v3.hw_platform : 0)
		: 0;
}


uint32_t socinfo_get_platform_version(void)
{
	return socinfo ?
		(socinfo->v1.format >= 4 ? socinfo->v4.platform_version : 0)
		: 0;
}

/* This information is directly encoded by the machine id */
/* Thus no external callers rely on this information at the moment */
static uint32_t socinfo_get_accessory_chip(void)
{
	return socinfo ?
		(socinfo->v1.format >= 5 ? socinfo->v5.accessory_chip : 0)
		: 0;
}

uint32_t socinfo_get_platform_subtype(void)
{
	return socinfo ?
		(socinfo->v1.format >= 6 ? socinfo->v6.hw_platform_subtype : 0)
		: 0;
}

enum pmic_model socinfo_get_pmic_model(void)
{
	return socinfo ?
		(socinfo->v1.format >= 7 ? socinfo->v7.pmic_model
			: PMIC_MODEL_UNKNOWN)
		: PMIC_MODEL_UNKNOWN;
}

uint32_t socinfo_get_pmic_die_revision(void)
{
	return socinfo ?
		(socinfo->v1.format >= 7 ? socinfo->v7.pmic_die_revision : 0)
		: 0;
}

enum msm_cpu socinfo_get_msm_cpu(void)
{
	return cur_cpu;
}
EXPORT_SYMBOL_GPL(socinfo_get_msm_cpu);

static ssize_t
socinfo_show_id(struct sys_device *dev,
		struct sysdev_attribute *attr,
		char *buf)
{
	if (!socinfo) {
		pr_err("%s: No socinfo found!\n", __func__);
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%u\n", socinfo_get_id());
}

static ssize_t
socinfo_show_version(struct sys_device *dev,
		     struct sysdev_attribute *attr,
		     char *buf)
{
	uint32_t version;

	if (!socinfo) {
		pr_err("%s: No socinfo found!\n", __func__);
		return 0;
	}

	version = socinfo_get_version();
	return snprintf(buf, PAGE_SIZE, "%u.%u\n",
			SOCINFO_VERSION_MAJOR(version),
			SOCINFO_VERSION_MINOR(version));
}

static ssize_t
socinfo_show_build_id(struct sys_device *dev,
		      struct sysdev_attribute *attr,
		      char *buf)
{
	if (!socinfo) {
		pr_err("%s: No socinfo found!\n", __func__);
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%-.32s\n", socinfo_get_build_id());
}

static ssize_t
socinfo_show_raw_id(struct sys_device *dev,
		    struct sysdev_attribute *attr,
		    char *buf)
{
	if (!socinfo) {
		pr_err("%s: No socinfo found!\n", __func__);
		return 0;
	}
	if (socinfo->v1.format < 2) {
		pr_err("%s: Raw ID not available!\n", __func__);
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%u\n", socinfo_get_raw_id());
}

static ssize_t
socinfo_show_raw_version(struct sys_device *dev,
			 struct sysdev_attribute *attr,
			 char *buf)
{
	if (!socinfo) {
		pr_err("%s: No socinfo found!\n", __func__);
		return 0;
	}
	if (socinfo->v1.format < 2) {
		pr_err("%s: Raw version not available!\n", __func__);
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%u\n", socinfo_get_raw_version());
}

static ssize_t
socinfo_show_platform_type(struct sys_device *dev,
			 struct sysdev_attribute *attr,
			 char *buf)
{
	uint32_t hw_type;

	if (!socinfo) {
		pr_err("%s: No socinfo found!\n", __func__);
		return 0;
	}
	if (socinfo->v1.format < 3) {
		pr_err("%s: platform type not available!\n", __func__);
		return 0;
	}

	hw_type = socinfo_get_platform_type();
	if (hw_type >= HW_PLATFORM_INVALID) {
		pr_err("%s: Invalid hardware platform type found\n",
								   __func__);
		hw_type = HW_PLATFORM_UNKNOWN;
	}

	return snprintf(buf, PAGE_SIZE, "%-.32s\n", hw_platform[hw_type]);
}

static ssize_t
socinfo_show_platform_version(struct sys_device *dev,
			 struct sysdev_attribute *attr,
			 char *buf)
{

	if (!socinfo) {
		pr_err("%s: No socinfo found!\n", __func__);
		return 0;
	}
	if (socinfo->v1.format < 4) {
		pr_err("%s: platform version not available!\n", __func__);
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%u\n",
		socinfo_get_platform_version());
}

static ssize_t
socinfo_show_accessory_chip(struct sys_device *dev,
			struct sysdev_attribute *attr,
			char *buf)
{
	if (!socinfo) {
		pr_err("%s: No socinfo found!\n", __func__);
		return 0;
	}
	if (socinfo->v1.format < 5) {
		pr_err("%s: accessory chip not available!\n", __func__);
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%u\n",
		socinfo_get_accessory_chip());
}

static ssize_t
socinfo_show_platform_subtype(struct sys_device *dev,
			struct sysdev_attribute *attr,
			char *buf)
{
	uint32_t hw_subtype;
	if (!socinfo) {
		pr_err("%s: No socinfo found!\n", __func__);
		return 0;
	}
	if (socinfo->v1.format < 6) {
		pr_err("%s: platform subtype not available!\n", __func__);
		return 0;
	}

	hw_subtype = socinfo_get_platform_subtype();
	if (hw_subtype >= PLATFORM_SUBTYPE_INVALID) {
		pr_err("%s: Invalid hardware platform sub type found\n",
								   __func__);
		hw_subtype = PLATFORM_SUBTYPE_UNKNOWN;
	}
	return snprintf(buf, PAGE_SIZE, "%-.32s\n",
		hw_platform_subtype[hw_subtype]);
}

static ssize_t
socinfo_show_pmic_model(struct sys_device *dev,
			struct sysdev_attribute *attr,
			char *buf)
{
	if (!socinfo) {
		pr_err("%s: No socinfo found!\n", __func__);
		return 0;
	}
	if (socinfo->v1.format < 7) {
		pr_err("%s: pmic_model not available!\n", __func__);
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%u\n",
		socinfo_get_pmic_model());
}

static ssize_t
socinfo_show_pmic_die_revision(struct sys_device *dev,
			       struct sysdev_attribute *attr,
			       char *buf)
{
	if (!socinfo) {
		pr_err("%s: No socinfo found!\n", __func__);
		return 0;
	}
	if (socinfo->v1.format < 7) {
		pr_err("%s: pmic_die_revision not available!\n", __func__);
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%u\n",
		socinfo_get_pmic_die_revision());
}

static struct sysdev_attribute socinfo_v1_files[] = {
	_SYSDEV_ATTR(id, 0444, socinfo_show_id, NULL),
	_SYSDEV_ATTR(version, 0444, socinfo_show_version, NULL),
	_SYSDEV_ATTR(build_id, 0444, socinfo_show_build_id, NULL),
};

static struct sysdev_attribute socinfo_v2_files[] = {
	_SYSDEV_ATTR(raw_id, 0444, socinfo_show_raw_id, NULL),
	_SYSDEV_ATTR(raw_version, 0444, socinfo_show_raw_version, NULL),
};

static struct sysdev_attribute socinfo_v3_files[] = {
	_SYSDEV_ATTR(hw_platform, 0444, socinfo_show_platform_type, NULL),
};

static struct sysdev_attribute socinfo_v4_files[] = {
	_SYSDEV_ATTR(platform_version, 0444,
			socinfo_show_platform_version, NULL),
};

static struct sysdev_attribute socinfo_v5_files[] = {
	_SYSDEV_ATTR(accessory_chip, 0444,
			socinfo_show_accessory_chip, NULL),
};

static struct sysdev_attribute socinfo_v6_files[] = {
	_SYSDEV_ATTR(platform_subtype, 0444,
			socinfo_show_platform_subtype, NULL),
};

static struct sysdev_attribute socinfo_v7_files[] = {
	_SYSDEV_ATTR(pmic_model, 0444,
			socinfo_show_pmic_model, NULL),
	_SYSDEV_ATTR(pmic_die_revision, 0444,
			socinfo_show_pmic_die_revision, NULL),
};

static struct sysdev_class soc_sysdev_class = {
	.name = "soc",
};

static struct sys_device soc_sys_device = {
	.id = 0,
	.cls = &soc_sysdev_class,
};

static int __init socinfo_create_files(struct sys_device *dev,
					struct sysdev_attribute files[],
					int size)
{
	int i;
	for (i = 0; i < size; i++) {
		int err = sysdev_create_file(dev, &files[i]);
		if (err) {
			pr_err("%s: sysdev_create_file(%s)=%d\n",
			       __func__, files[i].attr.name, err);
			return err;
		}
	}
	return 0;
}

static int __init socinfo_init_sysdev(void)
{
	int err;

	if (!socinfo) {
		pr_err("%s: No socinfo found!\n", __func__);
		return -ENODEV;
	}

	err = sysdev_class_register(&soc_sysdev_class);
	if (err) {
		pr_err("%s: sysdev_class_register fail (%d)\n",
		       __func__, err);
		return err;
	}
	err = sysdev_register(&soc_sys_device);
	if (err) {
		pr_err("%s: sysdev_register fail (%d)\n",
		       __func__, err);
		return err;
	}
	socinfo_create_files(&soc_sys_device, socinfo_v1_files,
				ARRAY_SIZE(socinfo_v1_files));
	if (socinfo->v1.format < 2)
		return err;
	socinfo_create_files(&soc_sys_device, socinfo_v2_files,
				ARRAY_SIZE(socinfo_v2_files));

	if (socinfo->v1.format < 3)
		return err;

	socinfo_create_files(&soc_sys_device, socinfo_v3_files,
				ARRAY_SIZE(socinfo_v3_files));

	if (socinfo->v1.format < 4)
		return err;

	socinfo_create_files(&soc_sys_device, socinfo_v4_files,
				ARRAY_SIZE(socinfo_v4_files));

	if (socinfo->v1.format < 5)
		return err;

	socinfo_create_files(&soc_sys_device, socinfo_v5_files,
				ARRAY_SIZE(socinfo_v5_files));

	if (socinfo->v1.format < 6)
		return err;

	socinfo_create_files(&soc_sys_device, socinfo_v6_files,
				ARRAY_SIZE(socinfo_v6_files));

	if (socinfo->v1.format < 7)
		return err;

	return socinfo_create_files(&soc_sys_device, socinfo_v7_files,
				ARRAY_SIZE(socinfo_v7_files));
}

arch_initcall(socinfo_init_sysdev);

static void * __init setup_dummy_socinfo(void)
{
	if (machine_is_msm8960_cdp())
		dummy_socinfo.id = 87;
	else if (machine_is_msm9615_mtp() || machine_is_msm9615_cdp())
		dummy_socinfo.id = 104;
	else if (early_machine_is_msm8974()) {
		dummy_socinfo.id = 126;
		strlcpy(dummy_socinfo.build_id, "msm8974 - ",
			sizeof(dummy_socinfo.build_id));
	} else if (early_machine_is_msm9625()) {
		dummy_socinfo.id = 134;
		strlcpy(dummy_socinfo.build_id, "msm9625 - ",
			sizeof(dummy_socinfo.build_id));
	} else if (early_machine_is_msm8226()) {
		dummy_socinfo.id = 145;
		strlcpy(dummy_socinfo.build_id, "msm8226 - ",
			sizeof(dummy_socinfo.build_id));
	} else if (machine_is_msm8625_rumi3())
		dummy_socinfo.id = 127;
	strlcat(dummy_socinfo.build_id, "Dummy socinfo",
		sizeof(dummy_socinfo.build_id));
	return (void *) &dummy_socinfo;
}

int __init socinfo_init(void)
{
	socinfo = smem_alloc(SMEM_HW_SW_BUILD_ID, sizeof(struct socinfo_v7));

	if (!socinfo)
		socinfo = smem_alloc(SMEM_HW_SW_BUILD_ID,
				sizeof(struct socinfo_v6));

	if (!socinfo)
		socinfo = smem_alloc(SMEM_HW_SW_BUILD_ID,
				sizeof(struct socinfo_v5));

	if (!socinfo)
		socinfo = smem_alloc(SMEM_HW_SW_BUILD_ID,
				sizeof(struct socinfo_v4));

	if (!socinfo)
		socinfo = smem_alloc(SMEM_HW_SW_BUILD_ID,
				sizeof(struct socinfo_v3));

	if (!socinfo)
		socinfo = smem_alloc(SMEM_HW_SW_BUILD_ID,
				sizeof(struct socinfo_v2));

	if (!socinfo)
		socinfo = smem_alloc(SMEM_HW_SW_BUILD_ID,
				sizeof(struct socinfo_v1));

	if (!socinfo) {
		pr_warn("%s: Can't find SMEM_HW_SW_BUILD_ID; falling back on "
			"dummy values.\n", __func__);
		socinfo = setup_dummy_socinfo();
	}

	WARN(!socinfo_get_id(), "Unknown SOC ID!\n");
	WARN(socinfo_get_id() >= ARRAY_SIZE(cpu_of_id),
		"New IDs added! ID => CPU mapping might need an update.\n");

	if (socinfo->v1.id < ARRAY_SIZE(cpu_of_id))
		cur_cpu = cpu_of_id[socinfo->v1.id];

	switch (socinfo->v1.format) {
	case 1:
		pr_info("%s: v%u, id=%u, ver=%u.%u\n",
			__func__, socinfo->v1.format, socinfo->v1.id,
			SOCINFO_VERSION_MAJOR(socinfo->v1.version),
			SOCINFO_VERSION_MINOR(socinfo->v1.version));
		break;
	case 2:
		pr_info("%s: v%u, id=%u, ver=%u.%u, "
			 "raw_id=%u, raw_ver=%u\n",
			__func__, socinfo->v1.format, socinfo->v1.id,
			SOCINFO_VERSION_MAJOR(socinfo->v1.version),
			SOCINFO_VERSION_MINOR(socinfo->v1.version),
			socinfo->v2.raw_id, socinfo->v2.raw_version);
		break;
	case 3:
		pr_info("%s: v%u, id=%u, ver=%u.%u, "
			 "raw_id=%u, raw_ver=%u, hw_plat=%u\n",
			__func__, socinfo->v1.format, socinfo->v1.id,
			SOCINFO_VERSION_MAJOR(socinfo->v1.version),
			SOCINFO_VERSION_MINOR(socinfo->v1.version),
			socinfo->v2.raw_id, socinfo->v2.raw_version,
			socinfo->v3.hw_platform);
		break;
	case 4:
		pr_info("%s: v%u, id=%u, ver=%u.%u, "
			 "raw_id=%u, raw_ver=%u, hw_plat=%u, hw_plat_ver=%u\n",
			__func__, socinfo->v1.format, socinfo->v1.id,
			SOCINFO_VERSION_MAJOR(socinfo->v1.version),
			SOCINFO_VERSION_MINOR(socinfo->v1.version),
			socinfo->v2.raw_id, socinfo->v2.raw_version,
			socinfo->v3.hw_platform, socinfo->v4.platform_version);
		break;
	case 5:
		pr_info("%s: v%u, id=%u, ver=%u.%u, "
			 "raw_id=%u, raw_ver=%u, hw_plat=%u,  hw_plat_ver=%u\n"
			" accessory_chip=%u\n", __func__, socinfo->v1.format,
			socinfo->v1.id,
			SOCINFO_VERSION_MAJOR(socinfo->v1.version),
			SOCINFO_VERSION_MINOR(socinfo->v1.version),
			socinfo->v2.raw_id, socinfo->v2.raw_version,
			socinfo->v3.hw_platform, socinfo->v4.platform_version,
			socinfo->v5.accessory_chip);
		break;
	case 6:
		pr_info("%s: v%u, id=%u, ver=%u.%u, "
			 "raw_id=%u, raw_ver=%u, hw_plat=%u,  hw_plat_ver=%u\n"
			" accessory_chip=%u hw_plat_subtype=%u\n", __func__,
			socinfo->v1.format,
			socinfo->v1.id,
			SOCINFO_VERSION_MAJOR(socinfo->v1.version),
			SOCINFO_VERSION_MINOR(socinfo->v1.version),
			socinfo->v2.raw_id, socinfo->v2.raw_version,
			socinfo->v3.hw_platform, socinfo->v4.platform_version,
			socinfo->v5.accessory_chip,
			socinfo->v6.hw_platform_subtype);
		break;
	case 7:
		pr_info("%s: v%u, id=%u, ver=%u.%u, raw_id=%u, raw_ver=%u, hw_plat=%u, hw_plat_ver=%u\n accessory_chip=%u, hw_plat_subtype=%u, pmic_model=%u, pmic_die_revision=%u\n",
			__func__,
			socinfo->v1.format,
			socinfo->v1.id,
			SOCINFO_VERSION_MAJOR(socinfo->v1.version),
			SOCINFO_VERSION_MINOR(socinfo->v1.version),
			socinfo->v2.raw_id, socinfo->v2.raw_version,
			socinfo->v3.hw_platform, socinfo->v4.platform_version,
			socinfo->v5.accessory_chip,
			socinfo->v6.hw_platform_subtype,
			socinfo->v7.pmic_model,
			socinfo->v7.pmic_die_revision);
		break;
	default:
		pr_err("%s: Unknown format found\n", __func__);
		break;
	}

	// +++ ASUS_BSP : support platform info in kernel
	sprintf(g_asus_plat_info.cpu_ver, "CPU_V%d.%d",SOCINFO_VERSION_MAJOR(socinfo->v1.version),SOCINFO_VERSION_MINOR(socinfo->v1.version));
	printk("[KERNEL] g_asus_plat_info.cpu_ver = %s \n",g_asus_plat_info.cpu_ver);
	if(socinfo->v2.raw_id==0x6B1)
	{
		g_asus_plat_info.hdcp=1;
	}
	printk("[KERNEL] g_asus_plat_info.hdcp = %d \n",g_asus_plat_info.hdcp);
	// --- ASUS_BSP : support platform info in kernel
	
	return 0;
}

const int get_core_count(void)
{
	if (!(read_cpuid_mpidr() & BIT(31)))
		return 1;

	if (read_cpuid_mpidr() & BIT(30))
		return 1;

	/* 1 + the PART[1:0] field of MIDR */
	return ((read_cpuid_id() >> 4) & 3) + 1;
}

const int read_msm_cpu_type(void)
{
	if (socinfo_get_msm_cpu() != MSM_CPU_UNKNOWN)
		return socinfo_get_msm_cpu();

	switch (read_cpuid_id()) {
	case 0x510F02D0:
	case 0x510F02D2:
	case 0x510F02D4:
		return MSM_CPU_8X60;

	case 0x510F04D0:
	case 0x510F04D1:
	case 0x510F04D2:
	case 0x511F04D0:
	case 0x512F04D0:
		return MSM_CPU_8960;

	case 0x51404D11: /* We can't get here unless we are in bringup */
		return MSM_CPU_8930;

	case 0x510F06F0:
		return MSM_CPU_8064;

	case 0x511F06F1:
	case 0x512F06F0:
		return MSM_CPU_8974;

	default:
		return MSM_CPU_UNKNOWN;
	};
}

const int cpu_is_krait(void)
{
	return ((read_cpuid_id() & 0xFF00FC00) == 0x51000400);
}

const int cpu_is_krait_v1(void)
{
	switch (read_cpuid_id()) {
	case 0x510F04D0:
	case 0x510F04D1:
	case 0x510F04D2:
		return 1;

	default:
		return 0;
	};
}

const int cpu_is_krait_v2(void)
{
	switch (read_cpuid_id()) {
	case 0x511F04D0:
	case 0x511F04D1:
	case 0x511F04D2:
	case 0x511F04D3:
	case 0x511F04D4:

	case 0x510F06F0:
	case 0x510F06F1:
	case 0x510F06F2:
		return 1;

	default:
		return 0;
	};
}

const int cpu_is_krait_v3(void)
{
	switch (read_cpuid_id()) {
	case 0x512F04D0:
	case 0x511F06F0:
	case 0x511F06F1:
	case 0x510F05D0:
		return 1;

	default:
		return 0;
	};
}
