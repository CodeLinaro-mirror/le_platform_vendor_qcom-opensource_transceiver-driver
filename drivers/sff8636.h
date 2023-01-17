/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#define SFF8636_DEVICE0 (0)
#define SFF8636_IRQ_OFFSET (3)
#define SFF8636_IRQ_PREFETCH_LEN (2)
#define SFF8636_PREFETCH_PERIOD (100)

enum {
    SFF8636_IRQ_FLAGS                   = QSFP_ADDR(0, 0,   2),
    SFF8636_TEMPERATURE                 = QSFP_ADDR(0, 0,  22),
    SFF8636_SUPPLY_VOLTAGE              = QSFP_ADDR(0, 0,  26),
    SFF8636_RX_POWER                    = QSFP_ADDR(0, 0,  34),
    SFF8636_TX_BIAS                     = QSFP_ADDR(0, 0,  42),
    SFF8636_TX_POWER                    = QSFP_ADDR(0, 0,  50),
    SFF8636_TX_DISABLE                  = QSFP_ADDR(0, 0,  86),
    SFF8636_POWER_ENABLE                = QSFP_ADDR(0, 0,  93),
    SFF8636_INTERRUPT_MASK              = QSFP_ADDR(0, 0, 100),
    SFF8636_CHANNEL_INTERRUPT_MASK      = QSFP_ADDR(0, 3, 242),
    SFF8636_CLS8_MAX_POWER              = QSFP_ADDR(0, 0, 107),
    SFF8636_FREE_SIDE_PROP              = QSFP_ADDR(0, 0, 110),
    SFF8636_CHANNEL_INFO                = QSFP_ADDR(0, 0, 113),
    SFF8636_ID                          = QSFP_ADDR(0, 0, 128),
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

/* Page 0 byte 128 */
struct sff8636_eeprom_base {
    // byte 128
    u8 phys_id;
    u8 phys_ext_id;
    u8 connector;

    // byte 131
    u8 e40g_active:1;
    u8 e40g_base_lr4:1;
    u8 e40g_base_sr4:1;
    u8 e40g_base_cr4:1;
    u8 e10g_base_sr:1;
    u8 e10g_base_lr:1;
    u8 e10g_base_lrm:1;
    u8 ecom_extended:1; // byte 192 has details

    // byte 132
    u8 sonet_oc48_short_reach:1;
    u8 sonet_oc48_smf_intermediate_reach:1;
    u8 sonet_oc48_smf_long_reach:1;
    u8 reserved_2:5;

    // byte 133
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

    // byte 135
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

    // byte 137
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

    // byte 139
    u8 encoding;
    u8 br_nominal;
    u8 ext_ratesel_spec;

    // byte 142
    u8 length[5];

    // byte 147
    u8 device_tech;

    // byte 148
    char vendor_name[16];

    // byte 164
    u8 ext_module;
    char vendor_oui[3];

    // byte 168
    char vendor_pn[16];

    // byte 184
    char vendor_rev[2];
    union {
        __be16 wavelength;
        u8 copper_atten[2];
    } __packed;
    __be16 wavelength_tolerance;

    // byte 190
    u8 max_case_temp;
    // byte 191
    u8 cc_base;
} __packed;

/* Page 0 byte 192 */
struct sff8636_eeprom_ext {
    // byte 192
    u8 link_codes;

    u8 rx_amp_prg:1;
    u8 rx_emp_prg:1;
    u8 tx_eq_prg:1;
    u8 tx_eq_auto_adap:1;
    u8 tx_adap_eq_freeze:1;
    u8 intl_gpio:1;
    u8 lpmode_gpio:1;
    u8 reserved:1;

    // byte 194
    u8 tx_squelch_impl:1;
    u8 tx_squelch_dis_impl:1;
    u8 rx_output_dis_impl:1;
    u8 rx_squelch_dis_impl:1;
    u8 rx_cdr_lol_impl:1;
    u8 tx_cdr_lol_impl:1;
    u8 rx_cdr_ctrl_impl:1;
    u8 tx_cdr_ctrl_impl:1;

    u8 page20_21:1;
    u8 tx_los_impl:1;
    u8 tx_squelch_oma_impl:1;
    u8 tx_fault_impl:1;
    u8 tx_dis_impl:1;
    u8 rate_select_impl:1;
    u8 page1:1;
    u8 page2:1;

    // byte 196
    char vendor_sn[16];

    // byte 212
    u8 datecode[8];

    // byte 220
    u8 diagmon;
    u8 enh_options;
    u8 baud_rate_nominal;

    // byte 223
    u8 cc_ext;
} __packed;

struct sff8636_eeprom_id {
    struct sff8636_eeprom_base base;
    struct sff8636_eeprom_ext  ext;
} __packed;

struct sff8636_id_stat {
    u8 phys_id;
    u8 rev_spec;

    u8 data_not_ready:1;
    u8 irq_deasserted:1;
    u8 flat_mem:1;
    u8 reserved:5;
} __packed;

struct sff8636_irq_flags {
    u8 data_not_ready:1;
    u8 intl:1;
    u8 flat_mem:1;
    u8 reserved_0:5;

    u8 los;

    u8 tx_fault:4;
    u8 tx_adap_eq_fault:4;

    u8 lol;

    u8 init_complete:1;
    u8 tc_ready:1;
    u8 reserved_1:2;
    u8 temp_alarm:4;

    u8 reserved_2:4;
    u8 volt_alarm:4;

    u8 vendor_specific1;
    u8 rx12_pow_alarm;
    u8 rx34_pow_alarm;
    u8 tx12_bias_alarm;
    u8 tx34_bias_alarm;
    u8 tx12_pow_alarm;
    u8 tx34_pow_alarm;
    u8 reserved_3[4];
    u8 vendor_specific2[3];
} __packed;

#endif
