/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */

#ifndef LINUX_CMIS_H
#define LINUX_CMIS_H

/* Read from page 0 */
struct cmis_eeprom_base {
    // byte 128
    u8 phys_id;
    char vendor_name[16];
    // byte 145
    char vendor_oui[3];
    // byte 148
    char vendor_pn[16];
    // byte 164
    char vendor_rev[2];
    // byte 166
    char vendor_sn[16];
    // byte 182
    u8 datecode[8];
    // byte 190
    u8 clei_code[10];

    // byte 200
    u8 reserved_1:5;
    u8 power_class:3;

    u8 max_power;

    u8 baselen:6;
    u8 len_mulplier:2;

    // byte 203
    u8 connector;

    u8 atten_5ghz;
    u8 atten_7ghz;
    u8 atten_12p9ghz;
    u8 atten_25p8ghz;
    u8 reserved_2[2];

    // byte 210
    u8 medialane_unsup;

    u8 breakout_config:4;
    u8 reserved_3:4;

    // byte 212
    u8 media_interface_tech;
    u8 reserved_4[9];

    // byte 222
    u8 checksum;
    //u8 custom[33];
} __packed;

/* Read from page 1 */
struct cmis_eeprom_ext {
    // byte 130
    u8 hw_major_rev;
    u8 hw_minor_rev;

    // byte 132
    u8 baselen_smf:6;
    u8 len_mulplier_smf:2;

    // byte 133
    u8 len_om5;
    u8 len_om4;
    u8 len_om3;
    u8 len_om2;
    u8 reserved_0;

    // byte 138
    u16 nominal_wavelength;
    u16 wavelength_tolerance;

    // byte 142
    u8 banks:2;
    u8 page3:1;
    u8 page5:1;
    u8 reserved_1:1;
    u8 diag_page13_14:1;
    u8 vdmpage20_2F:1;
    u8 netpathpage16_17:1;

    // byte 143
    u8 duration[2];

    // byte 145
    u8 aux_mon_obs:3;
    u8 page15:1;
    u8 epps:1;
    u8 txclk:2;
    u8 cooling_impl:1;

    // byte 146
    u8 max_temp;
    u8 min_temp;
    u16 propagation_delay;
    u8 volt_min;

    // byte 151
    u8 tx_dis_mod_wide:1;
    u8 tx_dis_fast:1;
    u8 rx_los_fast:1;
    u8 rx_los_type:1;
    u8 rx_pow_type:1;
    u8 rx_outeq_type:2;
    u8 optical_detector_type:1;

    u8 cdr_pow_saved_per_lane;

    // byte 153
    u8 tx_inputeq_max:4;
    u8 rx_output_level0:1;
    u8 rx_output_level1:1;
    u8 rx_output_level2:1;
    u8 rx_output_level3:1;

    u8 rx_eq_precursor_max:4;
    u8 rx_eq_postcursor_max:4;

    // byte 155
    u8 tx_polarity_flip:1;
    u8 tx_dis_sup:1;
    u8 tx_dis_autosquelch_sup:1;
    u8 tx_squelchforce_sup:1;
    u8 tx_squelch_method:2;
    u8 transmitter_tunable:1;
    u8 wavelen_ctrlable:1;

    // byte 156
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

    // byte 158
    u8 reserved_4:1;
    u8 rx_los_sup:1;
    u8 rx_cdr_lol_sup:1;
    u8 reserved_5:5;

    // byte 159
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

    // byte 161
    u8 tx_cdr_sup:1;
    u8 tx_cdr_bypass_sup:1;
    u8 tx_eqfixedmanual_ctrl_sup:1;
    u8 tx_adapeq_sup:1;
    u8 tx_eqfreeze_sup:1;
    u8 tx_eqrecal_buf_sup:2;
    u8 reserved_8:1;

    // byte 162
    u8 rx_cdr_sup:1;
    u8 rx_cdr_bypass_sup:1;
    u8 rx_ampl_ctrl_sup:1;
    u8 rx_eq_ctrl_sup:2;
    u8 staged_set1_sup:1;
    u8 unidir_reconfig_sup:1;
    u8 reserved_9:1;

    u8 cdb[4];

    // byte 167
    u8 max_dur_mod_powup:4;
    u8 max_dur_mod_powdown:4;

    u8 max_dur_dptx_on:4;
    u8 max_dur_dptx_off:4;

    // byte 169
    u8 reserved_10[7];

    // byte 176
    u8 medialane_app[15];
    u8 custom[32];

    // byte 223
    u8 apps_descp[28];
    // byte 251
    u8 reserved_11[4];
    // byte 255
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

    u8 irq_deasserted:1;
    u8 mod_state:3;
    u8 reserved_3:4;
} __packed;

struct cmis_eeprom_id {
    struct cmis_eeprom_base base;
    struct cmis_eeprom_ext  ext;
} __packed;

struct cmis_irq_flags {
    u8 irq_deasserted:1;
    u8 mod_state:3;
    u8 reserved_1:4;

    u8 bank0_page11:1;
    u8 bank0_page12:1;
    u8 bank0_page14:1;
    u8 bank0_page2c:1;
    u8 reserved_2:4;

    u8 bank1:4;
    u8 reserved_3:4;

    u8 bank2:4;
    u8 reserved_4:4;

    u8 bank3:4;
    u8 reserved_5:4;

    u8 mod_state_change:1;
    u8 mod_fw_err:1;
    u8 data_fw_err:1;
    u8 reserved_6:3;
    u8 cdb_cmd_complete1:1;
    u8 cdb_cmd_complete2:1;

    u8 temp_high_alarm:1;
    u8 temp_low_alarm:1;
    u8 temp_high_warn:1;
    u8 temp_low_warn:1;
    u8 vcc_high_alarm:1;
    u8 vcc_low_alarm:1;
    u8 vcc_high_warn:1;
    u8 vcc_low_warn:1;

    u8 aux1_high_alarm:1;
    u8 aux1_low_alarm:1;
    u8 aux1_high_warn:1;
    u8 aux1_low_warn:1;
    u8 aux2_high_alarm:1;
    u8 aux2_low_alarm:1;
    u8 aux2_high_warn:1;
    u8 aux2_low_warn:1;

    u8 aux3_high_alarm:1;
    u8 aux3_low_alarm:1;
    u8 aux3_high_warn:1;
    u8 aux3_low_warn:1;
    u8 custom_high_alarm:1;
    u8 custom_low_alarm:1;
    u8 custom_high_warn:1;
    u8 custom_low_warn:1;
} __packed;


struct cmis_bank0_page11 {
    u8 dp_state_lane1:4;
    u8 dp_state_lane2:4;

    u8 dp_state_lane3:4;
    u8 dp_state_lane4:4;

    u8 dp_state_lane5:4;
    u8 dp_state_lane6:4;

    u8 dp_state_lane7:4;
    u8 dp_state_lane8:4;

    u8 rx_output_status;
    u8 tx_output_status;

    u8 dp_state_changed;
    u8 tx_failure;
    u8 tx_los;
    u8 tx_cdr_lol;
    u8 tx_adap_eq_fail;

    u8 tx_power_high_alarm;
    u8 tx_power_low_alarm;
    u8 tx_power_high_warn;
    u8 tx_power_low_warn;

    u8 tx_bias_high_alarm;
    u8 tx_bias_low_alarm;
    u8 tx_bias_high_warn;
    u8 tx_bias_low_warn;

    u8 rx_los;
    u8 rx_cdr_lol;
    u8 rx_power_high_alarm;
    u8 rx_power_low_alarm;
    u8 rx_power_high_warn;
    u8 rx_power_low_warn;
    u8 rx_output_status_changed;
} __packed;

enum {
#define CMIS_ADDR(page, addr)    ((page) << 8 | (addr))
    CMIS_IRQ_FLAGS                   = CMIS_ADDR(0,   3),
    CMIS_ID                          = CMIS_ADDR(0, 128),
    CMIS_POWER_CLASS                 = CMIS_ADDR(0, 200),
    CMIS_MODULE_CONTROLS             = CMIS_ADDR(0,  26),

    CMIS_ID_EXT                      = CMIS_ADDR(1, 130),

    CMIS_TX_DISABLE                  = CMIS_ADDR(0x10,130),
    CMIS_LANE_MASKS                  = CMIS_ADDR(0x10,213),

    CMIS_LANE_FLAGS                  = CMIS_ADDR(0x11,128),
    CMIS_BANK0_PAGE12                = CMIS_ADDR(0x12,128),
    CMIS_BANK0_PAGE14                = CMIS_ADDR(0x14,128),
    CMIS_BANK0_PAGE2C                = CMIS_ADDR(0x2C,128),

};

#define CMIS_LOW_POWER_REQ_SW   (BIT(4))
#define CMIS_LOW_POWER_ALLOW_HW (BIT(6))

#endif
