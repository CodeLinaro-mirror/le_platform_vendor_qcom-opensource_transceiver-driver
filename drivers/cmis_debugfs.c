/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include "transceiver_debugfs.h"

static int qsfp_debug_pages_support_adv_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
          seq_printf(s, "QSFP module is not attached\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;
        switch(cmis_id->ext.banks) {
        case 0x00:
           seq_printf(s, "Bank 0 is supported (8 lanes) for Pages 10h-2Fh \n");
           break;
        case 0x01:
           seq_printf(s, "Bank 0 and 1 is supported (16 lanes) for Pages "
                         "10h-2Fh \n");
           break;
        case 0x02:
           seq_printf(s, "Bank 0-3 is supported (32 lanes) for Pages"
                         " 10h-2Fh \n");
           break;
        case 0x03:
           seq_printf(s, "BIT[1-0] reserved (pages 10h-2Fh banks support was"
                         " not defined) \n");
           break;
        }

        cmis_id->ext.page3 & 1 ? seq_printf(s, "BIT[2]: User page 03h "
            "supported \n"):seq_printf(s, "BIT[2]: User page 03h is not"
            " supported \n");

        cmis_id->ext.page5 & 1 ? seq_printf(s, "BIT[3]: Form factor specific "
            "Page 05h is supported \n"):seq_printf(s, "BIT[3]: Form factor "
            "specific Page 05h is not supported\n");

        cmis_id->ext.diag_page13_14 & 1 ? seq_printf(s, "BIT[5]: Diagnostic "
            "pages supported (Banked page 13h-14h supported) \n"):
            seq_printf(s, "BIT[5]: Diagnostic pages not supported (Banked page"
            " 13h-14h supported) \n");

        cmis_id->ext.vdmpage20_2F & 1 ? seq_printf(s, "BIT[6]: VDM pages "
            "20h-2Fh (partially) supported\n"):seq_printf(s, "BIT[6]: "
            "VDM pages 20h-2Fh not supported \n");

        cmis_id->ext.netpathpage16_17 & 1 ? seq_printf(s, "BIT[7]: Page 16h"
            " and NP-related parts of page 17h supported \n"):
            seq_printf(s, "BIT[7]: Page 16h and NP-related parts of page 17h"
            " not supported \n");
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_pages_support_adv);

static int qsfp_debug_fault_info_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    u8 *spec_id = (u8*)&qsfp->id;
    int ret;
    u8 fault_cause = 0;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
          seq_printf(s, "QSFP module is not attached\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFPDD_CMIS:
        ret = qsfp_read(qsfp, CMIS_MOD_FAULT_CAUSE, &fault_cause,
                          sizeof(fault_cause));
        if (ret < 0) {
            seq_printf(s, "Module fault cause register read failed, ret %d\n",
                               ret);
            return 0;
        }

        seq_printf(s, "Module fault cause byte: {0x%02X} \n",fault_cause);
        switch(fault_cause) {
        case 0x00:
            seq_printf(s, "No fault detected (or field not supported) \n");
            break;
        case 0x01:
            seq_printf(s, "TEC runaway \n");
            break;
        case 0x02:
            seq_printf(s, "Data memory corrupted \n");
            break;
        case 0x03:
            seq_printf(s, "Program memory corrupted \n");
            break;
        case 0x04 ... 0x1F:
            seq_printf(s, "Reserved (fault codes) \n");
            break;
        case 0x20 ... 0x3F:
            seq_printf(s, "Custom (fault codes) \n");
            break;
        case 0x40 ... 0xFF:
            seq_printf(s, "Reserved (general) \n");
            break;
        }
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_fault_info);

static int qsfp_debug_global_ctrl_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    u8 *spec_id = (u8*)&qsfp->id;
    u8 mod_gbl_ctrl = 0;
    int ret;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
          seq_printf(s, "QSFP module is not attached\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFPDD_CMIS:
        ret = qsfp_read(qsfp, CMIS_MOD_GLOBAL_CTRL, &mod_gbl_ctrl,
                          sizeof(mod_gbl_ctrl));
        if (ret < 0) {
            seq_printf(s, "Module global control register read failed,"
                          "ret %d\n", ret);
            return 0;
        }

        seq_printf(s, "Module global control register byte: {0x%02X} \n",
                      mod_gbl_ctrl);

        mod_gbl_ctrl & BIT(7) ? seq_printf(s, "BIT[7]: Bank broadcast for "
            "lane-banked pages enabled \n"):seq_printf(s, "BIT[7]: Bank "
            "broadcast for lane-banked pages disabled \n");

        mod_gbl_ctrl & BIT(6) ? seq_printf(s, "BIT[6]: Module evaluates "
            "the LowPwrRequestHW signal \n"):seq_printf(s, "BIT[6]: Module "
            "ignores the LowPwrRequestHW signal \n");

        mod_gbl_ctrl & BIT(5) ? seq_printf(s, "BIT[5]: Squelching of Tx"
            " output reduces Pav \n"):seq_printf(s, "BIT[5]: Squelching "
            "of Tx output reduces OMA \n");

        mod_gbl_ctrl & BIT(4) ? seq_printf(s, "BIT[4]: Request for the module"
            " to stay in, or to return into, Low Power mode \n"):
            seq_printf(s, "BIT[4]: No software low power request \n");

        mod_gbl_ctrl & BIT(3) ? seq_printf(s, "BIT[3]: Software Reset"
            " bit set, Self-clear after module reset \n"):
            seq_printf(s, "BIT[3]: Software Reset bit not set \n");
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_global_ctrl);

static int qsfp_debug_inactive_revision_info_show(struct seq_file *s,
                                                  void *data)
{
    struct qsfp *qsfp = s->private;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id = (u8*)&qsfp->id;
    u8 mod_inactive_rev[2] = {0};
    int ret;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
          seq_printf(s, "QSFP module is not attached\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;
        ret = qsfp_read(qsfp, CMIS_INACTIVE_FIRMWARE, &mod_inactive_rev,
                          sizeof(mod_inactive_rev));
        if (ret < 0) {
            seq_printf(s, "Module in-active firmware revision register read "
                          "failed, ret %d\n", ret);
            return 0;
        }

        if( mod_inactive_rev[0] == 0x00 && mod_inactive_rev[1] == 0x00)
            seq_printf(s, "Module does not have in-active firmware image \n");
        else if( mod_inactive_rev[0] == 0xFF && mod_inactive_rev[1] == 0xFF)
            seq_printf(s, "Module in-active firmware image is invalid \n");
        else
            seq_printf(s, "Module in-active firmware: \n    major "
                          "revision: %d\n    minor revision: %d\n",
                          mod_inactive_rev[0], mod_inactive_rev[1]);

        seq_printf(s, "Module hardware revision: \n    major revision: %d\n"
                      "    minor revision: %d\n", cmis_id->ext.hw_major_rev,
                      cmis_id->ext.hw_minor_rev);
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_inactive_revision_info);

static int qsfp_debug_module_characteristics_adv_show(struct seq_file *s,
                                                              void *data)
{
    struct qsfp *qsfp = s->private;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
          seq_printf(s, "QSFP module is not attached\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        cmis_id->ext.aux_mon_obs & BIT(0)?seq_printf(s, "Aux 1 monitor"
            " monitors TEC current \n"):seq_printf(s, "Aux 1 monitor "
            "is custom \n");

        cmis_id->ext.aux_mon_obs & BIT(1)?seq_printf(s, "Aux 2 monitor "
            "monitors TEC current \n"):seq_printf(s, "Aux 2 monitor "
            "monitors Laser Temperature \n");

        cmis_id->ext.aux_mon_obs & BIT(2)?seq_printf(s, "Aux 3 monitor "
            "monitors Vcc2  \n"):seq_printf(s, "Aux 3 monitor "
            "monitors Laser Temperature \n");

        cmis_id->ext.page15 & 1?seq_printf(s, "Timing characteristics "
            "(Page 15h) supported \n"):seq_printf(s, "Timing "
            "characteristics (Page 15h) not supported\n");

        cmis_id->ext.epps & 1?seq_printf(s, "Enhanced Pulse Per Second "
            "timing signal supported \n"):seq_printf(s, "Enhanced Pulse "
            "Per Second timing signal not supported \n");

        switch(cmis_id->ext.txclk) {
        case 0x00:
            seq_printf(s, "Tx input lanes 1-8 must be frequency "
                "synchronous \n");
            break;
        case 0x01:
            seq_printf(s, "Tx input lanes 1-4 & 5-8 must be frequency "
                "synchronous \n");
            break;
        case 0x02:
            seq_printf(s, "Tx input lanes 1-2, 3-4, 5-6, 7-8 must be "
                          "frequency synchronous \n");
            break;
        case 0x03:
            seq_printf(s, "Lanes are asynchronous in frequency, "
                          "more than 8 lanes are supported\n");
            break;
        }

        cmis_id->ext.cooling_impl & 1?seq_printf(s, "Cooled transmitter \n"):
            seq_printf(s, "Uncooled transmitter device\n");

        seq_printf(s, "Maximum allowed module case temperature");
        cmis_id->ext.max_temp == 0x00 ? seq_printf(s, " not specified\n"):
                        seq_printf(s, ": %d °C\n", cmis_id->ext.max_temp);

        seq_printf(s, "Minimum allowed module case temperature");
        cmis_id->ext.min_temp == 0x00 ? seq_printf(s, " not specified\n"):
                         seq_printf(s, ": %d °C\n",cmis_id->ext.min_temp);

        seq_printf(s, "Minimum supported module operating voltage");
        cmis_id->ext.volt_min == 0x00 ? seq_printf(s, " not specified\n"):
                    /* 20 mV increments */
                    seq_printf(s, ": %u mV\n",cmis_id->ext.volt_min * 20);

        cmis_id->ext.optical_detector_type & 1?seq_printf(s, "APD "
                  "OpticalDetector \n"):
                  seq_printf(s, "PIN OpticalDetector \n");

        seq_printf(s, "Rx Power Measurement Type: ");
        cmis_id->ext.rx_pow_type & 1?seq_printf(s, "Average power \n"):
            seq_printf(s, "OMA \n");

        cmis_id->ext.rx_los_type & 1?seq_printf(s, "Rx LOS responds to"
             " Pav \n"):seq_printf(s, "Rx LOS responds to OMA \n");

        cmis_id->ext.rx_los_fast & 1?seq_printf(s, "Module raises Rx LOS "
            "within fast mod timing limits\n"):seq_printf(s, "Module "
            "raises Rx LOS within regular timing limits \n");

        cmis_id->ext.tx_dis_fast & 1?seq_printf(s, "Module responds to Tx "
            "Output Disable in fast mod timing limits\n"):
            seq_printf(s, "Module responds to Tx Output Disable with "
            "regular timing \n");

        cmis_id->ext.tx_dis_mod_wide & 1?seq_printf(s, "All Tx output lanes "
            "disabled when any OutputDisableTx set\n"):
            seq_printf(s, "Tx output disable is controlled per lane \n");

        seq_printf(s, "RxOutputEqType: \n");
        switch(cmis_id->ext.rx_outeq_type) {
        case 0x00:
            seq_printf(s, "    Peak-to-peak (p-p) amplitude stays constant, "
                          "or not implemented, or no information \n");
            break;
        case 0x01:
            seq_printf(s, "    Steady-state amplitude stays constant \n");
            break;
        case 0x02:
            seq_printf(s, "    Average of p-p and steady-state amplitude "
                          "stays constant\n");
            break;
        case 0x03:
            seq_printf(s, "    Reserved\n");
            break;
        }

        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_module_characteristics_adv);

static int qsfp_debug_controls_support_adv_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
          seq_printf(s, "QSFP module is not attached\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        cmis_id->ext.tx_polarity_flip & 1?seq_printf(s, "Input polarity flipTx"
            "control supported \n"):seq_printf(s, "Input polarity flipTx "
            "control not supported \n");

        cmis_id->ext.tx_dis_sup & 1?seq_printf(s, "Host can disable "
            "Tx outputs using the  OutputDisableTx register \n"):
            seq_printf(s, "Host cannot disable Tx outputs using the "
            "OutputDisableTx register \n");

        cmis_id->ext.tx_dis_autosquelch_sup & 1?seq_printf(s, "Host can "
           "disable automatic squelching of Tx outputs using "
           "AutoSquelchDisableTx \n"):seq_printf(s, "Host cannot disable "
           "automatic squelching of Tx outputs using AutoSquelchDisableTx \n");

        cmis_id->ext.tx_squelchforce_sup & 1?seq_printf(s, "Host can force "
            "squelching of Tx outputs using OutputSquelchForceTx \n"):
            seq_printf(s, "Host cannot force squelching of Tx outputs using "
            "OutputSquelchForceTx \n");

        cmis_id->ext.transmitter_tunable & 1?seq_printf(s, "Transmitter is "
            "tunable (Pages 04h & 12h supported) \n"):
            seq_printf(s, "Transmitter not tunable \n");

        cmis_id->ext.wavelen_ctrlable & 1?seq_printf(s, "Active wavelength "
            "control supported \n"):seq_printf(s, "Wavelength control is not "
            "supported \n");

        seq_printf(s, "SquelchMethodTx: ");
        switch(cmis_id->ext.tx_squelch_method) {
        case 0x00:
            seq_printf(s, "Tx output squelching function is not "
                          "supported \n");
            break;
        case 0x01:
            seq_printf(s, "Tx output squelching function reduces OMA \n");
            break;
        case 0x02:
            seq_printf(s, "Tx output squelching function reduces Pav \n");
            break;
        case 0x03:
            seq_printf(s, "Host controls the method for Tx output "
                          "squelching, reducing OMA or Pav \n");
            break;
        }

        cmis_id->ext.rx_polarity_flip & 1?seq_printf(s, "PolarityFlipRx "
            "supported \n"):seq_printf(s, "PolarityFlipRx not supported  \n");

        cmis_id->ext.rx_dis_sup & 1?seq_printf(s, "Host can disable Rx outputs"
            "using the OutputDisableRx register \n"):seq_printf(s, "Host "
            "cannot disable Rx outputs using the OutputDisableRx register \n");

        cmis_id->ext.rx_dis_autosquelch_sup & 1?seq_printf(s, "Host can"
            " disable automatic squelching of Rx outputs using the "
            "AutoSquelchDisableRx register \n"):seq_printf(s, "Host cannot "
            "disable automatic squelching of Rx outputs using the "
            "AutoSquelchDisableRx register \n");

        cmis_id->ext.bank_broadcast & 1?seq_printf(s, "The BankBroadcastEnable"
            "control is supported \n"):seq_printf(s, "The BankBroadcastEnable"
            " control is not supported \n");

        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_controls_support_adv);

static int qsfp_debug_flags_support_adv_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
          seq_printf(s, "QSFP module is not attached\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        cmis_id->ext.tx_fault_sup & 1?seq_printf(s, "Tx fault flags "
            "supported \n"):seq_printf(s, "Tx fault flags not supported \n");

        cmis_id->ext.tx_los_sup & 1?seq_printf(s, "Tx loss of signal flags "
            "supported \n"):seq_printf(s, "Tx loss of signal flags not "
            "supported \n");

        cmis_id->ext.tx_cdr_lol_sup & 1?seq_printf(s, "Tx CDR loss of lock "
            "flags supported \n"):seq_printf(s, "Tx CDR loss of lock flags "
            "not supported \n");

        cmis_id->ext.tx_adap_eq_fail_sup & 1?seq_printf(s, "Tx adaptive input"
            " eq fail flags supported \n"):seq_printf(s, "Tx adaptive input eq"
            " fail flags not supported \n");

        cmis_id->ext.rx_los_sup & 1?seq_printf(s, "Rx loss of signal flags "
            "supported \n"):seq_printf(s, "Rx loss of signal flags not "
            "supported \n");

        cmis_id->ext.rx_cdr_lol_sup & 1?seq_printf(s, "Rx CDR loss of lock "
            "flags supported \n"):seq_printf(s, "Rx CDR loss of lock flags "
            "not supported \n");
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_flags_support_adv);

static int qsfp_debug_monitors_support_adv_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
          seq_printf(s, "QSFP module is not attached\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        cmis_id->ext.temp_mon_sup & 1?seq_printf(s, "Temperature monitor "
            "supported \n"):seq_printf(s, "Temperature monitor not "
            "supported \n");

        cmis_id->ext.volt_mon_sup & 1?seq_printf(s, "Internal 3.3 V monitor "
            "supported \n"):seq_printf(s, "Internal 3.3 V monitor not "
            "supported \n");

        cmis_id->ext.aux1_mon_sup & 1?seq_printf(s, "Aux 1 monitor "
            "supported \n"):seq_printf(s, "Aux 1 monitor not "
            "supported \n");

        cmis_id->ext.aux2_mon_sup & 1?seq_printf(s, "Aux 2 monitor "
            "supported \n"):seq_printf(s, "Aux 2 monitor not "
            "supported \n");

        cmis_id->ext.aux3_mon_sup & 1?seq_printf(s, "Aux 3 monitor "
            "supported \n"):seq_printf(s, "Aux 3 monitor not supported \n");

        cmis_id->ext.custom_mon_sup & 1?seq_printf(s, "Custom monitor "
            "supported \n"):seq_printf(s, "Custom monitor not supported \n");

        cmis_id->ext.tx_bias_mon_sup & 1?seq_printf(s, "Tx Bias monitor "
            "supported \n"):seq_printf(s, "Tx Bias monitor not supported \n");

        cmis_id->ext.tx_optical_pow_mon_sup & 1?seq_printf(s, "Tx Output "
            "Optical Power monitor supported \n"):seq_printf(s, "Tx Output "
            "Optical Power monitor not supported \n");

        cmis_id->ext.rx_optical_pow_mon_sup & 1?seq_printf(s, "Rx Optical "
            "Input Power monitor supported \n"):seq_printf(s, "Rx Optical "
            "Input Power monitor not supporte \n");

        seq_printf(s, "TxBiasCurrentScalingFactor: ");
        switch(cmis_id->ext.tx_bias_cur_scal) {
        case 0x00:
            seq_printf(s, "Multiply x1 \n");
            break;
        case 0x01:
            seq_printf(s, "Multiply x2 \n");
            break;
        case 0x02:
            seq_printf(s, "Multiply x4 \n");
            break;
        case 0x03:
            seq_printf(s, "Reserved \n");
            break;
        }
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_monitors_support_adv);

static int qsfp_debug_global_status_info_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    u8 *spec_id = (u8*)&qsfp->id;
    u8 g_status = 0;
    int ret;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
          seq_printf(s, "QSFP module is not attached\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFPDD_CMIS:
        ret = qsfp_read(qsfp, CMIS_IRQ_FLAGS, &g_status, sizeof(g_status));
        if (ret < 0) {
            seq_printf(s, "Failed to read QSFP IRQ status. "
                           "ret %d\n", ret);
            return 0;
        }

        g_status & BIT(0) ? seq_printf(s, "BIT[0]: Interrupt not asserted"
          "(default value)\n"):seq_printf(s, "BIT[0]: Interrupt asserted \n");

        /*BIT[3-1] represent module state */
        g_status >>= 1;
        g_status &= 0x07;
        seq_printf(s, "Module state encoding BIT[3-1]: {0x%02X} ", g_status);

        switch(g_status) {
        case 0x00:
        case 0x06:
        case 0x07:
        default:
            seq_printf(s, " Reserved \n");
            break;
        case 0x01:
            seq_printf(s, " Module low power \n");
            break;
        case 0x02:
            seq_printf(s, " Module power up \n");
            break;
        case 0x03:
            seq_printf(s, " Module ready \n");
            break;
        case 0x04:
            seq_printf(s, " Module power down \n");
            break;
        case 0x05:
            seq_printf(s, " Module fault \n");
            break;
        }
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_global_status_info);

static int qsfp_debug_module_properties_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    u8 *spec_id = (u8*)&qsfp->id;
    u8 mod_prop = 0;
    int ret;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
          seq_printf(s, "QSFP module is not attached\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFPDD_CMIS:
        ret = qsfp_read(qsfp, CMIS_MOD_CFG, &mod_prop, sizeof(mod_prop));
        if (ret < 0) {
            seq_printf(s, "Failed to read QSFP management characteristics"
                          ".byte-2 ret %d\n", ret);
            return 0;
        }

        mod_prop & BIT(7) ? seq_printf(s, "BIT[7]: flat memory module "
            "support only page-00h \n"):seq_printf(s, "BIT[7]: paged memory "
            "module supported page 00h-02h & 10h 11h  \n");

        mod_prop & BIT(6) ? seq_printf(s, "BIT[6]: module supports only "
            "step-by-step reconfiguration \n"):seq_printf(s, "BIT[6]: module "
            "supports intervention-free reconfiguration \n");

        /*BIT[3-2] represent mcimaxspeed */
        mod_prop >>= 2;
        mod_prop &= 0x03;
        seq_printf(s, "Maximum supported MCI clock speed [3-2]: "
                      "{0x%02X} ", mod_prop);

        switch(mod_prop) {
        case 0x00:
            seq_printf(s, " Module supports up to 400 kHz \n");
            break;
        case 0x01:
            seq_printf(s, " Module supports up to 1 MHz \n");
            break;
        case 0x02 ... 0x03:
        default:
            seq_printf(s, " Reserved \n");
            break;
        }
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_module_properties);

static int qsfp_debug_active_firmware_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    u8 *spec_id = (u8*)&qsfp->id;
    u8 mod_firmware[2] = {0};
    u8 major_rev;
    u8 minor_rev;
    int ret;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
          seq_printf(s, "QSFP module is not attached\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFPDD_CMIS:
        ret = qsfp_read(qsfp, CMIS_ACTIVE_FIRMWARE, &mod_firmware,
                        sizeof(mod_firmware));
        if (ret < 0) {
            seq_printf(s, "Failed to read QSFP module active firmware version"
                          " ret %d\n", ret);
            return 0;
        }
        major_rev = mod_firmware[0];
        minor_rev = mod_firmware[1];

        if( major_rev == 0x00 && minor_rev == 0x00)
            seq_printf(s, "Module does not have active firmware image \n");
        else if( major_rev == 0xFF && minor_rev == 0xFF)
            seq_printf(s, "Module active firmware image is invalid \n");
        else
            seq_printf(s, "Module active firmware major revision: %d minor "
                          "revision %d \n",major_rev, minor_rev);
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_active_firmware);

static int qsfp_debug_module_power_info_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
          seq_printf(s, "QSFP module is not attached\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFPDD_CMIS:
        seq_printf(s, "Module power class %u \nMaximum power %u mW \n",
                      qsfp->module_power_class, qsfp->module_power_mW);
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_module_power_info);

static int qsfp_debug_link_length_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id = (u8*)&qsfp->id;
    u16 linklength = 0;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
          seq_printf(s, "QSFP module is not attached\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if((cmis_id->base.len_mulplier == 0x03) &&
           (cmis_id->base.baselen == 0x3F)) {
            seq_printf(s, "Module link length  greater than 6300 m\n");
        }
        else if((cmis_id->base.len_mulplier == 0x00) &&
           (cmis_id->base.baselen == 0x00)) {
            seq_printf(s, "Module with separable optical media\n");
        }
        else if(cmis_id->base.baselen == 0x00) {
            seq_printf(s, "Undefined link length, physical media "
                          "disconnected from the module \n");
        }
        else {
            switch(cmis_id->base.len_mulplier) {
            case 0x00:
                /* baselen in mm = (baselen * 1000)/10 */
                linklength = cmis_id->base.baselen*100;

                seq_printf(s, "Link Length: %d.%d m\n",
                              linklength/1000, linklength%1000);
                break;
            case 0x01:
                seq_printf(s, "Link Length: %d m\n", cmis_id->base.baselen);
                break;
            case 0x02:
                seq_printf(s, "Link Length: %d m\n" ,
                              cmis_id->base.baselen*10);
                break;
            case 0x03:
                seq_printf(s, "Link Length: %d m\n" ,
                              cmis_id->base.baselen*100 );
                break;
            }
        }
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_link_length);

static int qsfp_debug_lane_info_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    u8 *spec_id = (u8*)&qsfp->id;
    u8 laneinfo = 0;
    int ret, i;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
          seq_printf(s, "QSFP module is not attached\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFPDD_CMIS:
        ret = qsfp_read(qsfp, CMIS_LANE_INFO, &laneinfo,
                          sizeof(laneinfo));
        if (ret < 0) {
            seq_printf(s, "Channel register read failed, ret %d\n",
                               ret);
            return 0;
        }

        seq_printf(s, "Lane info: 0x%X \n", laneinfo);

        laneinfo = ~laneinfo;
        for (i = 0 ; i < 8 ; i++) {
            if (laneinfo & 1)
                seq_printf(s, "Media lane %d supported \n", i+1);
            else
                seq_printf(s, "Media lane %d not supported \n", i+1);
            laneinfo >>= 1;
        }
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_lane_info);

static int qsfp_debug_breakout_config_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id = (u8*)&qsfp->id;
    u8 bout_cfg_t;

    /* Ensure that the module is attached before processing  */
    if (qsfp->sm_mod_state < QSFP_MOD_PROBE) {
          seq_printf(s, "QSFP module is not attached\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;
        bout_cfg_t = cmis_id->base.breakout_config;
        bout_cfg_t &= 0x1F;

        seq_printf(s, "Breakout configuration {0x%02X}: ", bout_cfg_t);

        switch (bout_cfg_t) {
        case 0x00:
        default:
            seq_printf(s, "Undefined or Module with detachable media \n");
            break;
        case 0x01:
            seq_printf(s, "\' abcdefgh \' \n");
            break;
        case 0x02:
            seq_printf(s, "\' aaaaaaaa or G-a8 \' \n");
            break;
        case 0x03:
            seq_printf(s, "\' aaaaeeee or G-a4-e4 \' \n");
            break;
        case 0x04:
            seq_printf(s, "\' abcdeeee or G-a1-b1-c1-d1-e4 \' \n");
            break;
        case 0x05:
            seq_printf(s, "\' abcceeee or G-a1-b1-c2-e4 \' \n");
            break;
        case 0x06:
            seq_printf(s, "\' aacdeeee or G-a2-c1-d1-e4 \' \n");
            break;
        case 0x07:
            seq_printf(s, "\' aacceeee or G-a2-c2-e4 \' \n");
            break;
        case 0x08:
            seq_printf(s, "\' aaaaefgh or G-a4-e1-f1-g1-h1 \' \n");
            break;
        case 0x09:
            seq_printf(s, "\' aaaaefgg or G-a4-e1-f1-g2 \' \n");
            break;
        case 0x0A:
            seq_printf(s, "\' aaaaeegh or G-a4-e2-g1-h1 \' \n");
            break;
        case 0x0B:
            seq_printf(s, "\' aaaaeegg or G-a4-e2-g2 \' \n");
            break;
        case 0x0C:
            seq_printf(s, "\' aacceegg or G-a2-c2-e2-g2 \' \n");
            break;
        case 0x0D:
            seq_printf(s, "\' abcceegg or G-a1-b1-c2-e2-g2 \' \n");
            break;
        case 0x0E:
            seq_printf(s, "\' aacdeegg or G-a2-c1-d1-e2-g2 \' \n");
            break;
        case 0x0F:
            seq_printf(s, "\' abcdeegg or G-a1-b1-c1-d1-e2-g2 \' \n");
            break;
        case 0x10:
            seq_printf(s, "\' aaccefgg or G-a2-c2-e1-f1-g2 \' \n");
            break;
        case 0x11:
            seq_printf(s, "\' abccefgg or G-a1-b1-c2-e1-f1-g2 \' \n");
            break;
        case 0x12:
            seq_printf(s, "\' aacdefgg or G-a2-c1-d1-e1-f1-g2 \' \n");
            break;
        case 0x13:
            seq_printf(s, "\' abcdefgg or G-a1-b1-c1-d1-e1-f1-g2 \' \n");
            break;
        case 0x14:
            seq_printf(s, "\' aacceegh or G-a2-c2-e2-g1-h1 \' \n");
            break;
        case 0x15:
            seq_printf(s, "\' abcceegh or G-a1-b1-c2-e2-g1-h1 \' \n");
            break;
        case 0x16:
            seq_printf(s, "\' aacdeegh or G-a2-c1-d1-e2-g1-h1 \' \n");
            break;
        case 0x17:
            seq_printf(s, "\' abcdeegh or G-a1-b1-c1-d1-e2-g1-h1 \' \n");
            break;
        case 0x18:
            seq_printf(s, "\' aaccefgh or G-a2-c2-e1-f1-g1-h1 \' \n");
            break;
        case 0x19:
            seq_printf(s, "\' abccefgh or G-a1-b1-c2-e1-f1-g1-h1 \' \n");
            break;
        case 0x1A:
            seq_printf(s, "\' aacdefgh or G-a2-c1-d1-e1-f1-g1-h1 \' \n");
            break;
        case 0x1B ... 0x1F:
            seq_printf(s, "\' Reserved \' \n");
            break;
        }
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_breakout_config);

int create_page_debugfs_files(struct qsfp *qsfp)
{
    struct dentry *file;

    if (qsfp->module_flat_mem == 0x01) {
        /* Flat memory (Page 00h supported only) */
        dev_warn(qsfp->dev, "%s: QSFP was not supported for paged memory \n",
                            __func__);
        return 0;
    }

    file = debugfs_create_file("pages_support_adv", 0600,
                               qsfp->module_debugfs_dir, qsfp,
                               &qsfp_debug_pages_support_adv_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp pages_support_adv debugfs_create_file"
                           " fail, error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return 0;
    }

    file = debugfs_create_file("fault_info", 0600, qsfp->module_debugfs_dir, qsfp,
                               &qsfp_debug_fault_info_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp fault_info debugfs_create_file fail,"
                           "error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return 0;
    }

    file = debugfs_create_file("global_ctrl", 0600, qsfp->module_debugfs_dir,
                               qsfp, &qsfp_debug_global_ctrl_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp global_ctrl debugfs_create_file fail,"
                           "error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return 0;
    }

    file = debugfs_create_file("inactive_revision_info", 0600,
                               qsfp->module_debugfs_dir, qsfp,
                               &qsfp_debug_inactive_revision_info_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp inactive_revision_info "
                           "debugfs_create_file fail, error %ld\n",
                           __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return 0;
    }

    file = debugfs_create_file("module_characteristics_adv", 0600,
                               qsfp->module_debugfs_dir, qsfp,
                               &qsfp_debug_module_characteristics_adv_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp module_characteristics_adv "
                           "debugfs_create_file fail, error %ld\n",
                           __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return 0;
    }

    file = debugfs_create_file("controls_support_adv", 0600,
                               qsfp->module_debugfs_dir, qsfp,
                               &qsfp_debug_controls_support_adv_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp controls_support_adv debugfs_create_file"
                           "fail, error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return 0;
    }

    file = debugfs_create_file("flags_support_adv", 0600,
                               qsfp->module_debugfs_dir, qsfp,
                               &qsfp_debug_flags_support_adv_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp flags_support_adv debugfs_create_file "
                           "fail, error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return 0;
    }

    file = debugfs_create_file("monitors_support_adv", 0600,
                               qsfp->module_debugfs_dir, qsfp,
                               &qsfp_debug_monitors_support_adv_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp monitors_support_adv debugfs_create_file"
                           " fail, error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
    }

    return 0;
}

int cmis_create_debugfs_files(struct qsfp *qsfp)
{

    struct dentry *file;
    int ret;

    ret = module_debugfs_init(qsfp);
    if (ret != 0)
    {
        dev_err(qsfp->dev, "%s: qsfp module_spec_info debugfs dir fail \n",
                            __func__);
        return 0;
    }

    ret = create_common_debugfs_files(qsfp);
    if(ret != 0)
    {
        dev_err(qsfp->dev, "%s: common debugfs files create fail \n",
                            __func__);
        return 0;
    }

    file = debugfs_create_file("global_status_info", 0600,
                               qsfp->module_debugfs_dir, qsfp,
                               &qsfp_debug_global_status_info_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp global_status_info debugfs_create_file "
                           "fail, error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return 0;
    }

    file = debugfs_create_file("module_properties", 0600,
                               qsfp->module_debugfs_dir, qsfp,
                               &qsfp_debug_module_properties_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp module_properties debugfs_create_file"
                           "fail, error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return 0;
    }

    file = debugfs_create_file("active_firmware", 0600,
                               qsfp->module_debugfs_dir, qsfp,
                               &qsfp_debug_active_firmware_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp active_firmware debugfs_create_file"
                           " fail, error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return 0;
    }

    file = debugfs_create_file("module_power_info", 0600,
                               qsfp->module_debugfs_dir, qsfp,
                               &qsfp_debug_module_power_info_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp module_power_info debugfs_create_file"
                           " fail, error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return 0;
    }

    file = debugfs_create_file("link_length", 0600,
                               qsfp->module_debugfs_dir, qsfp,
                               &qsfp_debug_link_length_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp link_length debugfs_create_file fail,"
                           "error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return 0;
    }

    file = debugfs_create_file("lane_info", 0600, qsfp->module_debugfs_dir,
                               qsfp, &qsfp_debug_lane_info_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp lane_info debugfs_create_file fail,"
                           "error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return 0;
    }

    file = debugfs_create_file("breakout_config", 0600,
                               qsfp->module_debugfs_dir, qsfp,
                               &qsfp_debug_breakout_config_fops);
    if (!file || IS_ERR(file)) {
        dev_err(qsfp->dev, "%s: qsfp breakout_config debugfs_create_file fail,"
                           "error %ld\n", __func__,PTR_ERR(file));
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        return 0;
    }

    create_page_debugfs_files(qsfp);

    return 0;
}

