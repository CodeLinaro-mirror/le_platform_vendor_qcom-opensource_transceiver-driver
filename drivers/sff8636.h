/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Code is derived from http://git.armlinux.org.uk/cgit/linux-arm.git/
 * tree/drivers/net/phy/sfp.h?h=cex7
 *
 */
#ifndef LINUX_SFF8636_H
#define LINUX_SFF8636_H

#define SFF8636_POWER_CLASS_LOW (BIT(1) | BIT(0))
#define SFF8636_POWER_CLASS_HIGH (BIT(2) | BIT(0))
#define SFF8636_POWER_CLASS_1TO4 (BIT(0))
#define SFF8636_STAT_FLAT_MEM (BIT(2))
#define SFF8636_INTERRUPT_FLAG_SIZE (19)
#define SFF8636_TX_LOS_IMPLEMENTED (BIT(1))
#define SFF8636_TX_FAULT_IMPLEMENTED (BIT(3))
#define SFF8636_TX_DISABLE_IMPLEMENTED (BIT(4))

enum {
#define SFF8636_ADDR(page, addr)    ((page) << 8 | (addr))
    SFF8636_STAT                        = SFF8636_ADDR(0,   2),
    SFF8636_IRQ_FLAGS                   = SFF8636_ADDR(0,   3),
    SFF8636_TX_DISABLE                  = SFF8636_ADDR(0,  86),
    SFF8636_POWER_ENABLE                = SFF8636_ADDR(0,  93),
    SFF8636_TEMPERATURE_INTERRUPT_MASK  = SFF8636_ADDR(0, 103),
    SFF8636_CLS8_MAX_POWER              = SFF8636_ADDR(0, 107),
    SFF8636_ID                          = SFF8636_ADDR(0, 128),
};

enum {
    // rev_spec
    // SFF8636_REV_UNSPEC can be used up to SFF-8636 rev 2.5 exclusive.
    SFF8636_REV_UNSPEC          = 0,
    SFF8636_REV_8436_4_8        = 1,
    SFF8636_REV_8436_4_8P       = 2,
    SFF8636_REV_8636_1_3        = 3,
    SFF8636_REV_8636_1_4        = 4,
    SFF8636_REV_8636_1_5        = 5,
    SFF8636_REV_8636_2_0        = 6,
    SFF8636_REV_8636_2_5        = 7,
    SFF8636_REV_8636_2_8        = 8,

};

struct sff8636_eeprom_base {
    u8 phys_id;
    u8 phys_ext_id;
    u8 connector;

    u8 e40g_active:1;
    u8 e40g_base_lr4:1;
    u8 e40g_base_sr4:1;
    u8 e40g_base_cr4:1;
    u8 e10g_base_sr:1;
    u8 e10g_base_lr:1;
    u8 e10g_base_lrm:1;
    u8 ecom_extended:1; // byte 192 has details

    u8 sonet_oc48_short_reach:1;
    u8 sonet_oc48_smf_intermediate_reach:1;
    u8 sonet_oc48_smf_long_reach:1;
    u8 reserved_2:5;

    u8 reserved_3:4;
    u8 sas_3gbps:1;
    u8 sas_6gbps:1;
    u8 sas_12gbps:1;
    u8 sas_24gbps:1;

    u8 e1000_base_sx:1;
    u8 e1000_base_lx:1;
    u8 e1000_base_cx:1;
    u8 e1000_base_t:1;
    u8 reserved_4:4;

    u8 fc_tech_electrical_inter_enclosure:1;
    u8 fc_tech_lc:1;
    u8 reserved_5:1;
    u8 fc_ll_m:1;
    u8 fc_ll_l:1;
    u8 fc_ll_i:1;
    u8 fc_ll_s:1;
    u8 fc_ll_v:1;

    u8 reserved_6:4;
    u8 longwave_laser:1;
    u8 longwave_laser_w_ofc:1;
    u8 longwave_laser_wo_ofc:1;
    u8 electrical_intra_enclosure:1;

    u8 fc_media_sm:1;
    u8 fc_media_om3:1;
    u8 fc_media_m5:1;
    u8 fc_media_m6:1;
    u8 fc_media_tv:1;
    u8 fc_media_mi:1;
    u8 fc_media_tp:1;
    u8 fc_media_tw:1;

    u8 fc_speed_100:1;
    u8 fc_extended:1;  // check byte 192
    u8 fc_speed_200:1;
    u8 fc_speed_3200:1;
    u8 fc_speed_400:1;
    u8 fc_speed_1600:1;
    u8 fc_speed_800:1;
    u8 fc_speed_1200:1;

    u8 encoding;
    u8 br_nominal;
    u8 ext_ratesel_spec;
    u8 length[5];
    u8 device_tech;
    char vendor_name[16];
    u8 ext_module;
    char vendor_oui[3];
    char vendor_pn[16];
    char vendor_rev[2];
    union {
        __be16 wavelength;
        u8 copper_atten[2];
    } __packed;
    __be16 wavelength_tolerance;
    u8 max_case_temp;
    u8 cc_base;
} __packed;

struct sff8636_eeprom_ext {
    u8 link_codes;
    u8 options[3];
    char vendor_sn[16];
    u8 datecode[8];
    u8 diagmon;
    u8 enh_options;
    u8 baud_rate_nominal;
    u8 cc_ext;
} __packed;

struct sff8636_eeprom_id {
    struct sff8636_eeprom_base base;
    struct sff8636_eeprom_ext  ext;
} __packed;

struct sff8636_id_stat {
    u8 phys_id;
    u8 rev_spec;
    u8 status;
} __packed;

#endif
