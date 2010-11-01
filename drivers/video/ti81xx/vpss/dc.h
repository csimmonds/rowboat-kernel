/*
 *
 * Display Controller Internal Header file for TI 81XX VPSS
 *
 * Copyright (C) 2009 TI
 * Author: Yihe Hu <yihehu@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
 */

#ifndef __DRIVERS_VIDEO_TI81XX_VPSS_DC_H__
#define __DRIVERS_VIDEO_TI81XX_VPSS_DC_H__

#ifdef __KERNEL__

#define VPS_DC_MAX_NODE_NUM   26
#define HDMI 0
#ifdef CONFIG_ARCH_TI816X
#define HDCOMP 1
#define DVO2 2
#define SDVENC 3
#define VPS_DC_VENC_MASK 0xF
#else
#define DVO2 1
#define SDVENC 2
#define VPS_DC_VENC_MASK 0x7
#endif
#define MAX_INPUT_NODES_BLENDER   5



enum dc_idtype {
	DC_BLEND_ID = 0,
	DC_VENC_ID,
	DC_MODE_ID,
	DC_NODE_ID
};


struct dc_vencmode_info {
	char                 *name;
	u16                  width;
	u16                  height;
	u8                   scformat;
	enum fvid2_standard  mid;
};

struct dc_vencname_info {
	char *name;
	int  vid;
	int  blendid;
	/*0:hdmi, 1: HDCOMP, 2: DVO2, 3:SD*/
	int  idx;
};

struct dc_blender_info {
	char                     *name;
	u32                      idx;
	bool                     enabled;
	struct kobject           kobj;
	u32                      actnodes;
	struct vps_dctiminginfo  *tinfo;
	struct vps_dispctrl      *dctrl;
	struct vps_dcvencclksrc  clksrc;
};

struct vps_dispctrl {
	struct mutex               dcmutex;
	struct kobject             kobj;
	struct dc_blender_info     blenders[VPS_DC_MAX_VENC];
	/*start of shared structure*/
	struct vps_dcconfig        *dccfg;
	u32                        dccfg_phy;
	struct vps_dcvencinfo      *vinfo;
	u32                        vinfo_phy;
	struct vps_dcnodeinput     *nodeinfo;
	u32                        ninfo_phy;
	struct vps_dcoutputinfo    *opinfo;
	u32                        opinfo_phy;
	struct vps_dcvencclksrc    *clksrc;
	u32                        clksrc_phy;
	u32                        *dis_vencs;
	u32                        dis_vencsphy;
	/*end of shared structure*/
	int                        tiedvenc;
	int                        enabled_venc_ids;
	void                       *fvid2_handle;
	bool                       ishdmion;
	/*automode: will set pll clock auto*/
	bool                       automode;
};

int vps_dc_get_id(char *name, int *id, enum dc_idtype type);

int vps_dc_get_node_name(int id, char *name);

int vps_dc_get_outpfmt(int id, u32 *width, u32 *height,
		      u8 *scformat, enum dc_idtype type);

int vps_dc_set_node(u8 nodeid, u8 inputid, u8 enable);

int vps_dc_get_vencinfo(struct vps_dcvencinfo *vinfo);

int vps_dc_get_tiedvenc(u8 *tiedvenc);


#endif
#endif
