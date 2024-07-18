/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */

#ifndef LINUX_CMIS_H
#define LINUX_CMIS_H

/* Read from page 0 */
struct cmis_eeprom_base {
    /* byte 128 */
    u8 phys_id;
    char vendor_name[16];
    /* byte 145 */
    char vendor_oui[3];
    /* byte 148 */
    char vendor_pn[16];
    /* byte 164 */
    char vendor_rev[2];
    /* byte 166 */
    char vendor_sn[16];
    /* byte 182 */
    u8 datecode[8];
    /* byte 190 */
    u8 clei_code[10];

    /* byte 200 */
    u8 reserved_1:5;
    u8 power_class:3;

    u8 max_power;

    u8 baselen:6;
    u8 len_mulplier:2;

    /* byte 203 */
    u8 connector;

    u8 atten_5ghz;
    u8 atten_7ghz;
    u8 atten_12p9ghz;
    u8 atten_25p8ghz;
    u8 reserved_2[2];

    /* byte 210 */
    u8 medialane_unsup;

    u8 breakout_config:4;
    u8 reserved_3:4;

    /* byte 212 */
    u8 media_interface_tech;
    u8 reserved_4[9];

    /* byte 222 */
    u8 checksum;
} __packed;

/* Read from page 1 */
struct cmis_eeprom_ext {
    /* byte 130 */
    u8 hw_major_rev;
    u8 hw_minor_rev;

    /* byte 132 */
    u8 baselen_smf:6;
    u8 len_mulplier_smf:2;

    /* byte 133 */
    u8 len_om5;
    u8 len_om4;
    u8 len_om3;
    u8 len_om2;
    u8 reserved_0;

    /* byte 138 */
    __be16 nominal_wavelength;
    u16 wavelength_tolerance;

    /* byte 142 */
    u8 banks:2;
    u8 page3:1;
    u8 page5:1;
    u8 reserved_1:1;
    u8 diag_page13_14:1;
    u8 vdmpage20_2F:1;
    u8 netpathpage16_17:1;

    /* byte 143 */
    u8 duration[2];

    /* byte 145 */
    u8 aux_mon_obs:3;
    u8 page15:1;
    u8 epps:1;
    u8 txclk:2;
    u8 cooling_impl:1;

    /* byte 146 */
    u8 max_temp;
    u8 min_temp;
    u16 propagation_delay;
    u8 volt_min;

    /* byte 151 */
    u8 tx_dis_mod_wide:1;
    u8 tx_dis_fast:1;
    u8 rx_los_fast:1;
    u8 rx_los_type:1;
    u8 rx_pow_type:1;
    u8 rx_outeq_type:2;
    u8 optical_detector_type:1;

    u8 cdr_pow_saved_per_lane;

    /* byte 153 */
    u8 tx_inputeq_max:4;
    u8 rx_output_level0:1;
    u8 rx_output_level1:1;
    u8 rx_output_level2:1;
    u8 rx_output_level3:1;

    u8 rx_eq_precursor_max:4;
    u8 rx_eq_postcursor_max:4;

    /* byte 155 */
    u8 tx_polarity_flip:1;
    u8 tx_dis_sup:1;
    u8 tx_dis_autosquelch_sup:1;
    u8 tx_squelchforce_sup:1;
    u8 tx_squelch_method:2;
    u8 transmitter_tunable:1;
    u8 wavelen_ctrlable:1;

    /* byte 156 */
    u8 rx_polarity_flip:1;
    u8 rx_dis_sup:1;
    u8 rx_dis_autosquelch_sup:1;
    u8 reserved_2:4;
    u8 bank_broadcast:1;

    u8 tx_fault_sup:1;
    u8 tx_los_sup:1;
    u8 tx_cdr_lol_sup:1;
    u8 tx_adap_eq_fail_sup:1;
    u8 reserved_3:4;

    /* byte 158 */
    u8 reserved_4:1;
    u8 rx_los_sup:1;
    u8 rx_cdr_lol_sup:1;
    u8 reserved_5:5;

    /* byte 159 */
    u8 temp_mon_sup:1;
    u8 volt_mon_sup:1;
    u8 aux1_mon_sup:1;
    u8 aux2_mon_sup:1;
    u8 aux3_mon_sup:1;
    u8 custom_mon_sup:1;
    u8 reserved_6:2;

    u8 tx_bias_mon_sup:1;
    u8 tx_optical_pow_mon_sup:1;
    u8 rx_optical_pow_mon_sup:1;
    u8 tx_bias_cur_scal:2;
    u8 reserved_7:3;

    /* byte 161 */
    u8 tx_cdr_sup:1;
    u8 tx_cdr_bypass_sup:1;
    u8 tx_eqfixedmanual_ctrl_sup:1;
    u8 tx_adapeq_sup:1;
    u8 tx_eqfreeze_sup:1;
    u8 tx_eqrecal_buf_sup:2;
    u8 reserved_8:1;

    /* byte 162 */
    u8 rx_cdr_sup:1;
    u8 rx_cdr_bypass_sup:1;
    u8 rx_ampl_ctrl_sup:1;
    u8 rx_eq_ctrl_sup:2;
    u8 staged_set1_sup:1;
    u8 unidir_reconfig_sup:1;
    u8 reserved_9:1;

    u8 cdb[4];

    /* byte 167 */
    u8 max_dur_mod_powup:4;
    u8 max_dur_mod_powdown:4;

    u8 max_dur_dptx_on:4;
    u8 max_dur_dptx_off:4;

    /* byte 169 */
    u8 reserved_10[7];

    /* byte 176 */
    u8 medialane_app[15];
    u8 custom[32];

    /* byte 223 */
    u8 apps_descp[28];
    /* byte 251 */
    u8 reserved_11[4];
    /* byte 255 */
    u8 checksum;
} __packed;

struct cmis_id_stat {
    u8 phys_id;
    u8 rev_spec;

    u8 reserved_1:2;
    u8 mci_maxspeed:2;
    u8 reserved_2:2;
    u8 step_config_only:1;
    u8 flat_mem:1;

} __packed;

struct cmis_eeprom_id {
    struct cmis_eeprom_base base;
    struct cmis_eeprom_ext  ext;
} __packed;

struct cmis_mod_state {
    u8 irq_deasserted:1;
    u8 mod_state:3;
    u8 reserved_1:4;
} __packed;

struct cmis_temp_volt {
    /* byte 9 */
    u8 temp_alarm_warn:4;
    u8 volt_alarm_warn:4;
} __packed;

/* Read from page 0x11 */
struct cmis_lane_flags {
    u8 high_alarm;
    u8 low_alarm;
    u8 high_warn;
    u8 low_warn;
} __packed;

struct cmis_thresholds {
    u16 temp_high_alarm;
    __be16 temp_low_alarm;
    u16 temp_high_warn;
    __be16 temp_low_warn;
    u16 volt_high_alarm;
    u16 volt_low_alarm;
    u16 volt_high_warn;
    u16 volt_low_warn;
    u16 aux1_high_alarm;
    u16 aux1_low_alarm;
    u16 aux1_high_warn;
    u16 aux1_low_warn;
    u16 aux2_high_alarm;
    u16 aux2_low_alarm;
    u16 aux2_high_warn;
    u16 aux2_low_warn;
    u16 aux3_high_alarm;
    u16 aux3_low_alarm;
    u16 aux3_high_warn;
    u16 aux3_low_warn;
    u16 custom_high_alarm;
    u16 custom_low_alarm;
    u16 custom_high_warn;
    u16 custom_low_warn;
    u16 txpwr_high_alarm;
    u16 txpwr_low_alarm;
    u16 txpwr_high_warn;
    u16 txpwr_low_warn;
    u16 bias_high_alarm;
    u16 bias_low_alarm;
    u16 bias_high_warn;
    u16 bias_low_warn;
    u16 rxpwr_high_alarm;
    u16 rxpwr_low_alarm;
    u16 rxpwr_high_warn;
    u16 rxpwr_low_warn;
}__packed;

enum {
    CMIS_MOD_CFG                     = QSFP_ADDR(0, 0x0,   2),
    CMIS_MOD_STATE                   = QSFP_ADDR(0, 0x0,   3),
    CMIS_TEMP_VOLT_FLAGS             = QSFP_ADDR(0, 0x0,   9),
    CMIS_MOD_TEMPMON                 = QSFP_ADDR(0, 0x0,  14),
    CMIS_MOD_VCCMON                  = QSFP_ADDR(0, 0x0,  16),
    CMIS_MOD_AUX2_MON                = QSFP_ADDR(0, 0x0,  20),
    CMIS_MOD_AUX3_MON                = QSFP_ADDR(0, 0x0,  22),
    CMIS_MOD_GLOBAL_CTRL             = QSFP_ADDR(0, 0x0,  26),
    CMIS_MODULE_MASKS                = QSFP_ADDR(0, 0x0,  31),
    CMIS_ACTIVE_FIRMWARE             = QSFP_ADDR(0, 0x0,  39),
    CMIS_MOD_FAULT_CAUSE             = QSFP_ADDR(0, 0x0,  41),
    CMIS_MEDIA_TYPE_ENCODING         = QSFP_ADDR(0, 0x0,  85),
    CMIS_MEDIA_INTERFACE_ID          = QSFP_ADDR(0, 0x0,  87),
    CMIS_ID                          = QSFP_ADDR(0, 0x0, 128),
    CMIS_POWER_CLASS                 = QSFP_ADDR(0, 0x0, 200),
    CMIS_LANE_INFO                   = QSFP_ADDR(0, 0x0, 210),

    CMIS_INACTIVE_FIRMWARE           = QSFP_ADDR(0, 0x1, 128),
    CMIS_ID_EXT                      = QSFP_ADDR(0, 0x1, 130),

    CMIS_DDM_TH                      = QSFP_ADDR(0, 0x02, 128),

    CMIS_TX_DISABLE                  = QSFP_ADDR(0, 0x10, 130),
    CMIS_PAGE10_MASKS                = QSFP_ADDR(0, 0x10, 213),

    CMIS_TX_FAULT                    = QSFP_ADDR(0, 0x11, 135),
    CMIS_TX_LOS                      = QSFP_ADDR(0, 0x11, 136),
    CMIS_TX_CDR_LOL                  = QSFP_ADDR(0, 0x11, 137),
    CMIS_TX_ADAP_EQ_IN_FAIL          = QSFP_ADDR(0, 0x11, 138),
    CMIS_TX_POWER_FLAGS              = QSFP_ADDR(0, 0x11, 139),
    CMIS_TX_BIAS_FLAGS               = QSFP_ADDR(0, 0x11, 143),
    CMIS_RX_LOS                      = QSFP_ADDR(0, 0x11, 147),
    CMIS_RX_CDR_LOL                  = QSFP_ADDR(0, 0x11, 148),
    CMIS_RX_POWER_FLAGS              = QSFP_ADDR(0, 0x11, 149),
    CMIS_TX_POWER                    = QSFP_ADDR(0, 0x11, 154),
    CMIS_TX_POWER_LANE2              = QSFP_ADDR(0, 0x11, 156),
    CMIS_TX_POWER_LANE3              = QSFP_ADDR(0, 0x11, 158),
    CMIS_TX_POWER_LANE4              = QSFP_ADDR(0, 0x11, 160),
    CMIS_TX_POWER_LANE5              = QSFP_ADDR(0, 0x11, 162),
    CMIS_TX_POWER_LANE6              = QSFP_ADDR(0, 0x11, 164),
    CMIS_TX_POWER_LANE7              = QSFP_ADDR(0, 0x11, 166),
    CMIS_TX_POWER_LANE8              = QSFP_ADDR(0, 0x11, 168),
    CMIS_TX_BIAS                     = QSFP_ADDR(0, 0x11, 170),
    CMIS_TX_BIAS_LANE2               = QSFP_ADDR(0, 0x11, 172),
    CMIS_TX_BIAS_LANE3               = QSFP_ADDR(0, 0x11, 174),
    CMIS_TX_BIAS_LANE4               = QSFP_ADDR(0, 0x11, 176),
    CMIS_TX_BIAS_LANE5               = QSFP_ADDR(0, 0x11, 178),
    CMIS_TX_BIAS_LANE6               = QSFP_ADDR(0, 0x11, 180),
    CMIS_TX_BIAS_LANE7               = QSFP_ADDR(0, 0x11, 182),
    CMIS_TX_BIAS_LANE8               = QSFP_ADDR(0, 0x11, 184),
    CMIS_RX_POWER                    = QSFP_ADDR(0, 0x11, 186),
    CMIS_RX_POWER_LANE2              = QSFP_ADDR(0, 0x11, 188),
    CMIS_RX_POWER_LANE3              = QSFP_ADDR(0, 0x11, 190),
    CMIS_RX_POWER_LANE4              = QSFP_ADDR(0, 0x11, 192),
    CMIS_RX_POWER_LANE5              = QSFP_ADDR(0, 0x11, 194),
    CMIS_RX_POWER_LANE6              = QSFP_ADDR(0, 0x11, 196),
    CMIS_RX_POWER_LANE7              = QSFP_ADDR(0, 0x11, 198),
    CMIS_RX_POWER_LANE8              = QSFP_ADDR(0, 0x11, 200),

    CMIS_PAGE12_MASKS                = QSFP_ADDR(0, 0x12, 239),

    CMIS_PAGE13_MASKS                = QSFP_ADDR(0, 0x13, 206),

    CMIS_PAGE17_MASKS                = QSFP_ADDR(0, 0x17, 192),
};

#define CMIS_LOW_POWER_REQ_SW   (BIT(4))
#define CMIS_LOW_POWER_ALLOW_HW (BIT(6))
#define ETH_MODULE_CMIS_MAX_LEN (2431)
#define CMIS_MODULE_STATE_READY (0x3)

#define CMIS_MMF_ENCODING (1)
#define CMIS_SMF_ENCODING (2)
#define CMIS_CR_ENCODING  (3)

#endif
