/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */
#include "qsfp.h"

static int cmis_mod_probe(struct qsfp *qsfp, bool report)
{
    /* QSFP module inserted - read I2C data */
    struct cmis_id_stat id_stat = {0};
    struct qsfp_eeprom_id id = {0};
    u8 check;
    int ret;

    ret = qsfp_read(qsfp, 0, &id_stat, sizeof(id_stat));
    if (ret < 0) {
        if (report)
            TRX_LOG_ERR(qsfp, "Failed to read EEPROM: %d", ret);
        return -EAGAIN;
    }

    TRX_LOG_INFO(qsfp, "id_stat id 0x%X rev 0x%X flat_mem 0x%X state 0x%X",
               id_stat.phys_id, id_stat.rev_spec, id_stat.flat_mem,
               id_stat.mod_state);

    /* Early setup - we need to know if this module has a page register */
    qsfp->module_flat_mem = id_stat.flat_mem;
    qsfp->module_revision = id_stat.rev_spec;

    ret = qsfp_read(qsfp, CMIS_ID, &id.cmis.base, sizeof(id.cmis.base));
    if (ret < 0) {
        if (report)
            TRX_LOG_ERR(qsfp, "Failed to read base EEPROM: %d", ret);
        return -EAGAIN;
    }

    if (id.cmis.base.phys_id != id_stat.phys_id) {
        TRX_LOG_ERR(qsfp, "QSFP phys_id mismatch: 0x%02x != 0x%02x",
                  id_stat.phys_id, id.cmis.base.phys_id);
        return -EINVAL;
    }

    /* Validate the checksum over the base structure */
    check = qsfp_check(&id.cmis.base, sizeof(id.cmis.base) - 1);
    if (check != id.cmis.base.checksum) {
        TRX_LOG_ERR(qsfp, "EEPROM base structure checksum failure: "
                  "0x%02x != 0x%02x", check, id.cmis.base.checksum);
        return -EINVAL;
    }

    if (qsfp->module_flat_mem == 0x01) {
        /* Flat memory (Page 00h supported only) */
        qsfp->id = id;
        TRX_LOG_WARN(qsfp, "EEPROM extended structure not supported");
        return 0;
    }

    ret = qsfp_read(qsfp, CMIS_ID_EXT, &id.cmis.ext, sizeof(id.cmis.ext));
    if (ret < 0) {
        if (report)
            TRX_LOG_ERR(qsfp, "Failed to read extended EEPROM: %d", ret);
        return -EAGAIN;
    }

    /* Validate the checksum over the extended structure */
    check = qsfp_check(&id.cmis.ext, sizeof(id.cmis.ext) - 1);
    if (check != id.cmis.ext.checksum) {
        TRX_LOG_ERR(qsfp, "EEPROM extended structure checksum failure: "
                  "0x%X != 0x%X", check, id.cmis.ext.checksum);
        memset(&id.cmis.ext, 0, sizeof(id.cmis.ext));
        return -EINVAL;
    }

    qsfp->id = id;

    return 0;
}

static void cmis_disable_redundant_irq(const struct qsfp *qsfp)
{
    int ret;
    u8 mod_mask[] = {0xC7, 0xFF, 0xFF, 0xFF};
    u8 page10_mask[] = {0xFF,
                        0x00, /* TX Fault/TX Failure */
                        0xFF, /* TX LOS disable */
                        0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                        0x00, /* RX LOS enable */
                        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    u8 page12_mask[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    u8 page13_mask[] = {0x80, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    u8 page17_mask[] = {0xFF};

    /* Flat memory (Page 00h supported only) */
    if (qsfp->module_flat_mem == 0x01) {
        TRX_LOG_WARN(qsfp, "LaneMask feature not supported");
        return;
    }

    ret = qsfp_write(qsfp, CMIS_MODULE_MASKS, mod_mask, sizeof(mod_mask));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to mask redundant module level "
                           "interrupts. ret %d", ret);
    }

    ret = qsfp_write(qsfp, CMIS_PAGE10_MASKS, page10_mask,
                     sizeof(page10_mask));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to mask redundant interrupts from "
                           "page10. ret %d", ret);
    }

    ret = qsfp_write(qsfp, CMIS_PAGE12_MASKS, page12_mask, sizeof(page12_mask));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to mask redundant interrupts from "
                           "page12. ret %d", ret);
    }

    ret = qsfp_write(qsfp, CMIS_PAGE13_MASKS, page13_mask, sizeof(page13_mask));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to mask redundant interrupts from "
                           "page13. ret %d", ret);
    }

    ret = qsfp_write(qsfp, CMIS_PAGE17_MASKS, page17_mask, sizeof(page17_mask));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to mask redundant interrupts from "
                           "page17. ret %d", ret);
    }
}

static u8 cmis_get_state(struct qsfp *qsfp)
{
    int ret;
    u8 state = 0;
    struct cmis_irq_flags irq_flags = {0};
    bool poll = false;
    u8 buf_0 = 0x0;
    u8 buf_ff = 0xFF;

    ret = qsfp_read(qsfp, CMIS_IRQ_FLAGS, &irq_flags, 1);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to read QSFP IRQ status. ret %d", ret);
        /* Preserve the current state */
        return qsfp->state;
    }

    /* Before cmis module state reach ready we are facing issue of LOS
     * register not giving appropriate status and masking of interrupt
     * not working
     */
    if (irq_flags.mod_state != CMIS_MODULE_STATE_READY) {
        qsfp->need_poll = true;
        return qsfp->state;
    }

    if (qsfp->need_poll) {
        u8 tx_failure = 0;
        u8 rx_los = 0;

        ret = qsfp_read(qsfp, CMIS_TX_FAILURE, &tx_failure, sizeof(tx_failure));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Failed to read TX status. ret %d", ret);
            return qsfp->state;
        }

        ret = qsfp_read(qsfp, CMIS_RX_LOS, &rx_los, sizeof(rx_los));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Failed to read RX LOS. ret %d", ret);
            return qsfp->state;
        }

        if (rx_los) {
            /* Only RX LOS considered */
            if (rx_los == 0xFF) {
                state |= QSFP_F_LOS;
            }
            poll = true;
        }

        if (tx_failure) {
            if (tx_failure == 0xFF) {
                state |= QSFP_F_TX_FAULT;
            }
            poll = true;
        }

        /* If none of flag set then unmask the interrupt */
        if (!poll) {
            ret = qsfp_write(qsfp, CMIS_RX_LOS_MASK, &buf_0, sizeof(buf_0));
            if (ret < 0) {
                TRX_LOG_ERR(qsfp, "poll Failed to unmask QSFP RX LOS. "
                                   "ret %d", ret);
            }

            ret = qsfp_write(qsfp, CMIS_TX_FAILURE_MASK, &buf_0, sizeof(buf_0));
            if (ret < 0) {
                TRX_LOG_ERR(qsfp, "poll Failed to unmask QSFP TX Failure. "
                           "ret %d", ret);
            }
        }

        qsfp->need_poll = poll;
        return state;
    }

    ret = qsfp_read(qsfp, CMIS_IRQ_FLAGS, &irq_flags, sizeof(irq_flags));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to read QSFP IRQ status. ret %d", ret);
        /* Preserve the current state */
        return qsfp->state;
    }

    TRX_LOG_INFO(qsfp, "IntL %u Module state 0x%X Bank 0: page11 %u "
    "page12 %u page 14 %u page 2c %u", irq_flags.irq_deasserted,
    irq_flags.mod_state, irq_flags.bank0_page11, irq_flags.bank0_page12,
    irq_flags.bank0_page14, irq_flags.bank0_page2c);

    if (irq_flags.bank0_page11) {
        struct cmis_bank0_page11 b0p11 = {0};
        ret = qsfp_read(qsfp, CMIS_LANE_FLAGS, &b0p11, sizeof(b0p11));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Failed to read QSFP lane flags. ret %d", ret);
            /* Preserve the current state */
            return qsfp->state;
        }

        if (b0p11.rx_los) {
            if (b0p11.rx_los == 0xFF) {
                state |= QSFP_F_LOS;
            } else {
                TRX_LOG_INFO(qsfp, "There is LOS on few lanes which"
                " is not reported RX LOS 0x%X", b0p11.rx_los);
            }
            poll = true;
        }

        if (b0p11.tx_failure) {
            if (b0p11.tx_failure == 0xFF) {
                state |= QSFP_F_TX_FAULT;
            } else if (b0p11.tx_failure != 0) {
                TRX_LOG_INFO(qsfp, "There is TX Fault on few lanes "
                "which is not reported 0x%X", b0p11.tx_failure);
            }
            poll = true;
        }

        /* If flag is set then mask the interrupt */
        if (poll) {
            ret = qsfp_write(qsfp, CMIS_RX_LOS_MASK, &buf_ff, 1);
            if (ret < 0) {
                TRX_LOG_ERR(qsfp, "Failed to mask QSFP RX LOS. ret %d", ret);
            }

            ret = qsfp_write(qsfp, CMIS_TX_FAILURE_MASK, &buf_ff, 1);
            if (ret < 0) {
                TRX_LOG_ERR(qsfp, "Failed to mask QSFP TX Failure. "
                                "ret %d", ret);
            }
        }

        TRX_LOG_INFO(qsfp, "LOS RX 0x%X TX 0x%X , TX Fault 0x%X",
                         b0p11.rx_los, b0p11.tx_los, b0p11.tx_failure);
    }

    qsfp->need_poll = poll;

    return state;
}

static void cmis_tx_enable(const struct qsfp *qsfp)
{
    u8 status;
    int ret;

    status = 0;

    ret = qsfp_write(qsfp, CMIS_TX_DISABLE, &status, sizeof(status));
    if (ret < 0)
        TRX_LOG_ERR(qsfp, "TX enable failed. ret %d", ret);

}

static void cmis_tx_disable(const struct qsfp *qsfp)
{
    u8 status;
    int ret;

    status = 0xFF;

    ret = qsfp_write(qsfp, CMIS_TX_DISABLE, &status, sizeof(status));
    if (ret < 0)
        TRX_LOG_ERR(qsfp, "TX disable failed. ret %d", ret);

}

static int cmis_check_feature_impl(struct qsfp *qsfp)
{
    /* Check for page: 0x01 support */
    if (qsfp->module_flat_mem == 0x01) {
        /* Flat memory (Page 00h supported only) */
        TRX_LOG_INFO(qsfp, "QSFP supporting features: 0x%X", qsfp->features);
        return 0;
    }

    if (qsfp->id.cmis.ext.rx_los_sup) {
        qsfp->features |= QSFP_F_LOS;
    } else {
        TRX_LOG_WARN(qsfp, "RX LOS not implemented");
    }

    if (qsfp->id.cmis.ext.tx_fault_sup) {
        qsfp->features |= QSFP_F_TX_FAULT;
    } else {
        TRX_LOG_WARN(qsfp, "TX Fault not implemented");
    }

    if (qsfp->id.cmis.ext.tx_dis_sup) {
        qsfp->features |= QSFP_F_TX_DISABLE;
    } else {
        TRX_LOG_WARN(qsfp, "TX Disable not implemented");
    }

    return 0;
}

static int cmis_module_parse_power(struct qsfp *qsfp)
{
    qsfp->module_power_class = qsfp->id.cmis.base.power_class + 1;
    qsfp->module_power_mW = 250 * qsfp->id.cmis.base.max_power;

    return 0;
}

static int cmis_handle_max_power_exceed(const struct qsfp *qsfp)
{
    return -E_MAX_POWER_EXCEED;
}

static int cmis_mod_high_power(const struct qsfp *qsfp)
{
    int ret;
    u8 mod_ctrl = 0;

    ret = qsfp_read(qsfp, CMIS_MODULE_CONTROLS, &mod_ctrl, sizeof(mod_ctrl));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to read module controls: %d", ret);
        return ret;
    }

    mod_ctrl &= ~(CMIS_LOW_POWER_ALLOW_HW | CMIS_LOW_POWER_REQ_SW);

    return qsfp_write(qsfp, CMIS_MODULE_CONTROLS, &mod_ctrl, sizeof(mod_ctrl));
}

static int cmis_mod_low_power(const struct qsfp *qsfp)
{
    int ret;
    u8 mod_ctrl = 0;

    ret = qsfp_read(qsfp, CMIS_MODULE_CONTROLS, &mod_ctrl, sizeof(mod_ctrl));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to read module controls: %d", ret);
        return ret;
    }

    mod_ctrl |= CMIS_LOW_POWER_REQ_SW;

    return qsfp_write(qsfp, CMIS_MODULE_CONTROLS, &mod_ctrl, sizeof(mod_ctrl));
}

static void cmis_eeprom_print(const struct qsfp *qsfp)
{
    const struct cmis_eeprom_id *id = &qsfp->id.cmis;
    char date[9];

    TRX_LOG_INFO(qsfp, "phys_id 0x%X Power class %u Max Power %u mW "
    "Connector 0x%X Media 0x%X Breakout config 0x%X",
    id->base.phys_id, id->base.power_class + 1, id->base.max_power * 250,
    id->base.connector, id->base.media_interface_tech,
    id->base.breakout_config);

    TRX_LOG_INFO(qsfp, "vendor name %.*s vendor pn %.*s vendor rev %.*s"
    " vendor sn %.*s", (int)sizeof(id->base.vendor_name),
    id->base.vendor_name, (int)sizeof(id->base.vendor_pn), id->base.vendor_pn,
    (int)sizeof(id->base.vendor_rev), id->base.vendor_rev,
    (int)sizeof(id->base.vendor_sn), id->base.vendor_sn);

    TRX_LOG_INFO(qsfp, "vendor_oui[]: 0x%X 0x%X 0x%X", id->base.vendor_oui[2],
                     id->base.vendor_oui[1], id->base.vendor_oui[0]);

    date[0] = id->base.datecode[4];
    date[1] = id->base.datecode[5];
    date[2] = '-';
    date[3] = id->base.datecode[2];
    date[4] = id->base.datecode[3];
    date[5] = '-';
    date[6] = id->base.datecode[0];
    date[7] = id->base.datecode[1];
    date[8] = '\0';

    TRX_LOG_INFO(qsfp, "date %s", date);

    /* Flat memory (Page 00h supported only) */
    if (qsfp->module_flat_mem == 0x01) {
        TRX_LOG_INFO(qsfp, "Supports only Flat memory");
        return;
    }

    TRX_LOG_INFO(qsfp, "banks 0x%X page3 %u page5 %u diag_page13_14 %u"
    " vdmpage20_2F %u netpathpage16_17 %u page15 %u",
    id->ext.banks, id->ext.page3, id->ext.page5, id->ext.diag_page13_14,
    id->ext.vdmpage20_2F, id->ext.netpathpage16_17, id->ext.page15);

    TRX_LOG_INFO(qsfp, "cooling_impl %u min_temp %u max_temp %u "
    "volt_min %u temp_mon_sup %u volt_mon_sup %u tx_bias_mon_sup %u "
    "tx_optical_pow_mon_sup %u rx_optical_pow_mon_sup %u",
    id->ext.cooling_impl, id->ext.min_temp, id->ext.max_temp, id->ext.volt_min,
    id->ext.temp_mon_sup, id->ext.volt_mon_sup, id->ext.tx_bias_mon_sup,
    id->ext.tx_optical_pow_mon_sup, id->ext.rx_optical_pow_mon_sup);

    TRX_LOG_INFO(qsfp, "tx_dis_mod_wide %u tx_dis_fast %u "
    "tx_inputeq_max 0x%X tx_polarity_flip %u tx_dis_sup %u "
    "tx_dis_autosquelch_sup %u tx_squelchforce_sup %u tx_squelch_method 0x%X "
    "transmitter_tunable %u tx_fault_sup %u tx_los_sup %u tx_cdr_lol_sup %u "
    "tx_adap_eq_fail_sup %u tx_cdr_sup %u tx_cdr_bypass_sup %u "
    "tx_eqfixedmanual_ctrl_sup %u tx_adapeq_sup %u tx_eqfreeze_sup %u "
    "tx_eqrecal_buf_sup %u ", id->ext.tx_dis_mod_wide,
    id->ext.tx_dis_fast, id->ext.tx_inputeq_max, id->ext.tx_polarity_flip,
    id->ext.tx_dis_sup, id->ext.tx_dis_autosquelch_sup,
    id->ext.tx_squelchforce_sup, id->ext.tx_squelch_method,
    id->ext.transmitter_tunable, id->ext.tx_fault_sup, id->ext.tx_los_sup,
    id->ext.tx_cdr_lol_sup, id->ext.tx_adap_eq_fail_sup, id->ext.tx_cdr_sup,
    id->ext.tx_cdr_bypass_sup, id->ext.tx_eqfixedmanual_ctrl_sup,
    id->ext.tx_adapeq_sup, id->ext.tx_eqfreeze_sup,
    id->ext.tx_eqrecal_buf_sup);

    TRX_LOG_INFO(qsfp, "rx_los_fast %u rx_los_type %u rx_pow_type %u "
    "rx_outeq_type 0x%X rx_polarity_flip %u rx_dis_sup %u "
    "rx_dis_autosquelch_sup %u rx_los_sup %u rx_cdr_lol_sup %u rx_cdr_sup %u "
    "rx_cdr_bypass_sup %u rx_ampl_ctrl_sup %u rx_eq_ctrl_sup 0x%X",
    id->ext.rx_los_fast, id->ext.rx_los_type, id->ext.rx_pow_type,
    id->ext.rx_outeq_type, id->ext.rx_polarity_flip, id->ext.rx_dis_sup,
    id->ext.rx_dis_autosquelch_sup, id->ext.rx_los_sup,	id->ext.rx_cdr_lol_sup,
    id->ext.rx_cdr_sup,	id->ext.rx_cdr_bypass_sup, id->ext.rx_ampl_ctrl_sup,
    id->ext.rx_eq_ctrl_sup);
}

static int cmis_module_info(struct qsfp *qsfp, struct ethtool_modinfo *modinfo)
{
    /* Eth tool not capable of detecting CMIS */
    modinfo->type = 0;
    modinfo->eeprom_len = 0;

    return 0;
}

u8 cmis_get_connector_type(const struct qsfp *qsfp)
{
    return qsfp->id.cmis.base.connector;
}

/*
 * Function to get media lane information from EEPROM page 00h byte 210.
 */
static int cmis_get_lanes_presence(const struct qsfp *qsfp,
                                   trx_lane_cfg* laneinfo)
{
    u8 channel = 0;
    int ret;

    ret = qsfp_read(qsfp, CMIS_LANE_INFO, &channel,
                     sizeof(channel));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Channel register read failed, ret %d", ret);
        return -EINVAL;
    }

    *laneinfo = ~channel;

    TRX_LOG_INFO(qsfp, "Lane info: 0x%X ", *laneinfo);

    return 0;
}

/*
 * Function to get lane supported speed based on the number of lanes'
 * present on trx.
 */
static int cmis_get_lane_speed(const struct qsfp *qsfp,
                               trx_lane_speed* lane_speed)
{
    u8 lane_cnt;
    u8 channel = 0;
    int ret;

     ret = cmis_get_lanes_presence(qsfp, &channel);
     if(ret != 0)
         return ret;

    for(lane_cnt=0;channel;++lane_cnt)
        channel &= channel-1;

    /* Need to revisit later to check the possibilities for an 8-lane
       transceiver among those only 4 implemented. */
    if (lane_cnt == 0x04) {
        *lane_speed = TRX_LANE_SPEED_100G;
    }
    else if (lane_cnt == 0x08) {
        *lane_speed = TRX_LANE_SPEED_50G;
    }
    else {
        *lane_speed = TRX_LANE_SPEED_UNKNOWN;
    }

    TRX_LOG_INFO(qsfp, "Lane speed: 0x%X ", *lane_speed);

    return 0;
}

/*
 * Function to return the QSFP identifier value.
 */
static u8 cmis_get_transceiver_type(const struct qsfp *qsfp)
{
    return qsfp->id.cmis.base.phys_id;
}

/*
 * Function to return the cable assembly information from QSFP EEPROM
 * page 00h, byte 211 BIT [4-0].
 */
static int cmis_get_breakout_config(const struct qsfp *qsfp,
                                    trx_breakout_cfg* bout_config)
{
    /*  Assign breakout information to an 8-bit variable. */
    *bout_config = (trx_breakout_cfg)qsfp->id.cmis.base.breakout_config;

    TRX_LOG_INFO(qsfp, "Breakout config: 0x%X ", *bout_config);

    return 0;
}

unsigned long cmis_irq_delay(const struct qsfp *qsfp)
{
    return 0;
}

const char* cmis_revision_to_str(u8 mod_rev_value, char *revStr)
{
    u8 major, minor;

    /* Upper nibble (bits 7-4) is the integer part (major number)
     * Lower nibble (bits 3-0) is the decimal part (minor number)
     */
    major   = (mod_rev_value >> 4 ) & 0x0F;
    minor  = mod_rev_value & 0x0F;

    scnprintf(revStr,75,"CMIS revision number: %u.%u", major,minor);
    return revStr;
}

struct qsfp_spec_ops cmis_spec_ops = {
    .mod_probe = cmis_mod_probe,
    .disable_redundant_irq = cmis_disable_redundant_irq,
    .get_state = cmis_get_state,
    .tx_enable = cmis_tx_enable,
    .tx_disable = cmis_tx_disable,
    .check_features_impl = cmis_check_feature_impl,
    .module_parse_power = cmis_module_parse_power,
    .handle_max_power_exceed = cmis_handle_max_power_exceed,
    .mod_high_power = cmis_mod_high_power,
    .mod_low_power = cmis_mod_low_power,
    .eeprom_print = cmis_eeprom_print,
    .module_info = cmis_module_info,
    .get_connector_type = cmis_get_connector_type,
    .get_lane_speed = cmis_get_lane_speed,
    .get_transceiver_type = cmis_get_transceiver_type,
    .get_lanes_presence = cmis_get_lanes_presence,
    .get_breakout_config = cmis_get_breakout_config,
    .irq_delay = cmis_irq_delay,
    .create_debugfs = cmis_create_debugfs_files,
};
