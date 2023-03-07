/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Code is derived from http://git.armlinux.org.uk/cgit/linux-arm.git/
 * tree/drivers/net/phy/qsfp.c?h=cex7
 */
#include "transceiver_debugfs.h"


static int sff8636_mod_probe(struct qsfp *qsfp, bool report)
{
    /* QSFP module inserted - read I2C data */
    struct sff8636_id_stat id_stat = {0};
    struct qsfp_eeprom_id id = {0};
    u8 check;
    int ret;

    ret = qsfp_read(qsfp, 0, &id_stat, sizeof(id_stat));
    if (ret < 0) {
        if (report)
            TRX_LOG_ERR(qsfp, "Failed to read EEPROM: %d", ret);
        return -EAGAIN;
    }

    TRX_LOG_INFO(qsfp, "id_stat id 0x%X rev 0x%X flat_mem 0x%X",
               id_stat.phys_id, id_stat.rev_spec, id_stat.flat_mem);

    /* Early setup - we need to know if this module has a page register */
    qsfp->module_flat_mem = id_stat.flat_mem;
    qsfp->module_revision = id_stat.rev_spec;

    ret = qsfp_read(qsfp, SFF8636_ID, &id, sizeof(id.sff8636));
    if (ret < 0) {
        if (report)
            TRX_LOG_ERR(qsfp, "Failed to read EEPROM: %d", ret);
        return -EAGAIN;
    }

    if (id.sff8636.base.phys_id != id_stat.phys_id) {
        TRX_LOG_ERR(qsfp, "QSFP phys_id mismatch: 0x%02x != 0x%02x",
                  id_stat.phys_id, id.sff8636.base.phys_id);
        return -EINVAL;
    }

    /* Validate the checksum over the base structure */
    check = qsfp_check(&id.sff8636.base, sizeof(id.sff8636.base) - 1);
    if (check != id.sff8636.base.cc_base) {
        TRX_LOG_ERR(qsfp, "EEPROM base structure checksum failure: "
                  "0x%02x != 0x%02x", check, id.sff8636.base.cc_base);
        return -EINVAL;
    }

    /* Validate the checksum over the extended structure */
    check = qsfp_check(&id.sff8636.ext, sizeof(id.sff8636.ext) - 1);
    if (check != id.sff8636.ext.cc_ext) {
        TRX_LOG_ERR(qsfp, "EEPROM extended structure checksum failure: "
                  "0x%02x != 0x%02x", check, id.sff8636.ext.cc_ext);
        memset(&id.sff8636.ext, 0, sizeof(id.sff8636.ext));
        return -EINVAL;
    }

    qsfp->id = id;

    return 0;
}

static int sff8636_check_feature_impl(struct qsfp *qsfp)
{
    if (!qsfp->id.sff8636.ext.tx_los_impl)
        TRX_LOG_WARN(qsfp, "TX LOS not implemented");
    else
        qsfp->features |= QSFP_F_LOS;

    if (!qsfp->id.sff8636.ext.tx_fault_impl)
        TRX_LOG_WARN(qsfp, "TX Fault not implemented");
    else
        qsfp->features |= QSFP_F_TX_FAULT;

    if (!qsfp->id.sff8636.ext.tx_dis_impl)
        TRX_LOG_WARN(qsfp, "TX Disable not implemented");
    else
        qsfp->features |= QSFP_F_TX_DISABLE;

    return 0;
}

static int sff8636_module_parse_power(struct qsfp *qsfp)
{
    u32 power_mW, power_class;
    u8 pwr = 0, mask;
    int ret;

    if (qsfp->module_revision >= SFF8636_REV_8636_2_8 &&
        qsfp->id.sff8636.base.phys_ext_id & BIT(5)) {

        ret = qsfp_read(qsfp, SFF8636_CLS8_MAX_POWER, &pwr,
                        sizeof(pwr));
        if (ret < 0)
            return ret;

        power_class = 8;
        power_mW = pwr * 100;
    } else {
        if (qsfp->id.sff8636.base.phys_id == SFF8024_ID_QSFP_8438)
            mask = 0xc0;
        else
            mask = 0xc3;

        switch (qsfp->id.sff8636.base.phys_ext_id & mask) {
        default:
            power_mW = 1500;
            power_class = 1;
            break;
        case 0x40:
            power_mW = 2000;
            power_class = 2;
            break;
        case 0x80:
            power_mW = 2500;
            power_class = 3;
            break;
        case 0xc0:
            power_mW = 3500;
            power_class = 4;
            break;
        case 0xc1:
            power_mW = 4000;
            power_class = 5;
            break;
        case 0xc2:
            power_mW = 4500;
            power_class = 6;
            break;
        case 0xc3:
            power_mW = 5000;
            power_class = 7;
            break;
        }
    }

    qsfp->module_power_mW = power_mW;
    qsfp->module_power_class = power_class;

    return 0;
}

/*
 * Disable the interrupts (Temperature, Voltage alarms, vendor specific)
 * which are not handled by this driver
 */
static void sff8636_disable_redundant_irq(const struct qsfp *qsfp)
{
    int ret;
    /* Enable only TX/RX LOS and TX Fault intterupts */
    u8 buf1[] = {0x00, /* TX LOS , RX LOS enable */
                 0xF0, /* TX Fault enable */
                 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    u8 buf2[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    ret = qsfp_write(qsfp, SFF8636_INTERRUPT_MASK,
                     buf1, sizeof(buf1));
    if (ret < 0)
        TRX_LOG_ERR(qsfp, "Failed to mask redundant interrupts. ret %d", ret);

    ret = qsfp_write(qsfp, SFF8636_CHANNEL_INTERRUPT_MASK,
                     buf2, sizeof(buf2));
    if (ret < 0)
        TRX_LOG_ERR(qsfp, "Failed to mask redundant channel interrupts."
                        " ret %d", ret);

}

static int sff8636_handle_max_power_exceed(const struct qsfp *qsfp)
{
    u8 val = SFF8636_POWER_CLASS_1TO4;

    return qsfp_write(qsfp, SFF8636_POWER_ENABLE, &val, sizeof(val));
}

static int sff8636_mod_high_power(const struct qsfp *qsfp)
{
    u8 val = SFF8636_POWER_CLASS_HIGH;

    /* As power class 1 is the lowest we can not push module for
     * further high power class
     */
    if (qsfp->module_power_class == 1)
        return 0;

    return qsfp_write(qsfp, SFF8636_POWER_ENABLE, &val, sizeof(val));
}

static int sff8636_mod_low_power(const struct qsfp *qsfp)
{
    u8 val = SFF8636_POWER_CLASS_LOW;

    /* As power class 1 is the lowest we can not push module for
     * further low power class
     */
    if (qsfp->module_power_class == 1)
        return 0;

    return qsfp_write(qsfp, SFF8636_POWER_ENABLE, &val, sizeof(val));
}

static void sff8636_eeprom_print(const struct qsfp *qsfp)
{
    const struct sff8636_eeprom_id *id = &qsfp->id.sff8636;
    char date[9];

    TRX_LOG_INFO(qsfp, "phys_id 0x%X  phys_ext_id 0x%X  connector 0x%X",
    id->base.phys_id, id->base.phys_ext_id, id->base.connector);

    TRX_LOG_INFO(qsfp, "ecom_extended 0x%X   e10g_base_lrm 0x%X   "
    "e10g_base_lr 0x%X  e10g_base_sr 0x%X  e40g_base_cr4 0x%X  e40g_base_sr4 0x%X   "
    "e40g_base_lr4 0x%X   e40g_active 0x%X", id->base.ecom_extended,
    id->base.e10g_base_lrm, id->base.e10g_base_lr, id->base.e10g_base_sr,
    id->base.e40g_base_cr4, id->base.e40g_base_sr4, id->base.e40g_base_lr4,
    id->base.e40g_active);

    TRX_LOG_INFO(qsfp, "reserved_2 0x%X  "
    "sonet_oc48_smf_long_reach 0x%X  sonet_oc48_smf_intermediate_reach 0x%X"
    "   sonet_oc48_short_reach 0x%X", id->base.reserved_2,
    id->base.sonet_oc48_smf_long_reach,
    id->base.sonet_oc48_smf_intermediate_reach,
    id->base.sonet_oc48_short_reach);

    TRX_LOG_INFO(qsfp, "sas_24gbps 0x%X  sas_12gbps 0x%X  "
    "sas_6gbps 0x%X  sas_3gbps 0x%X   reserved_3 0x%X",
    id->base.sas_24gbps, id->base.sas_12gbps,id->base.sas_6gbps,
    id->base.sas_3gbps, id->base.reserved_3);

    TRX_LOG_INFO(qsfp, "reserved_4 0x%X   e1000_base_t 0x%X   "
    "e1000_base_cx 0x%X   e1000_base_lx 0x%X   e1000_base_sx 0x%X",
    id->base.reserved_4, id->base.e1000_base_t, id->base.e1000_base_cx,
    id->base.e1000_base_lx, id->base.e1000_base_sx);

    TRX_LOG_INFO(qsfp, "fc_ll_v 0x%X  fc_ll_s 0x%X  fc_ll_i 0x%X"
    "  fc_ll_l 0x%X  fc_ll_m 0x%X   reserved_5 0x%X   fc_tech_lc 0x%X   "
    "fc_tech_electrical_inter_enclosure 0x%X", id->base.fc_ll_v,
    id->base.fc_ll_s, id->base.fc_ll_i, id->base.fc_ll_l, id->base.fc_ll_m,
    id->base.reserved_5, id->base.fc_tech_lc,
    id->base.fc_tech_electrical_inter_enclosure);

    TRX_LOG_INFO(qsfp, "electrical_intra_enclosure 0x%X   "
    "longwave_laser_wo_ofc 0x%X   longwave_laser_w_ofc 0x%X   "
    "longwave_laser 0x%X  reserved_6 %X",
    id->base.electrical_intra_enclosure,
    id->base.longwave_laser_wo_ofc, id->base.longwave_laser_w_ofc,
    id->base.longwave_laser, id->base.reserved_6);

    TRX_LOG_INFO(qsfp, "fc_media_tw 0x%X   fc_media_tp 0x%X   "
    "fc_media_mi 0x%X   fc_media_tv 0x%X   fc_media_m6 0x%X   "
    "fc_media_m5 0x%X   fc_media_om3 0x%X   fc_media_sm 0x%X",
    id->base.fc_media_tw, id->base.fc_media_tp, id->base.fc_media_mi,
    id->base.fc_media_tv, id->base.fc_media_m6, id->base.fc_media_m5,
    id->base.fc_media_om3, id->base.fc_media_sm);

    TRX_LOG_INFO(qsfp, "fc_speed_1200 0x%X   fc_speed_800 0x%X   "
    "fc_speed_1600 0x%X   fc_speed_400 0x%X   fc_speed_3200 0x%X   "
    "fc_speed_200 0x%X   fc_extended 0x%X   fc_speed_100 0x%X",
    id->base.fc_speed_1200, id->base.fc_speed_800, id->base.fc_speed_1600,
    id->base.fc_speed_400, id->base.fc_speed_3200, id->base.fc_speed_200,
    id->base.fc_extended, id->base.fc_speed_100);

    TRX_LOG_INFO(qsfp, "encoding 0x%X   br_nominal 0x%X   "
    "ext_ratesel_spec 0x%X", id->base.encoding,
    id->base.br_nominal, id->base.ext_ratesel_spec);

    TRX_LOG_INFO(qsfp, "length[]: 0x%X 0x%X 0x%X 0x%X 0x%X",
    id->base.length[4], id->base.length[3], id->base.length[2],
    id->base.length[1], id->base.length[0]);

    TRX_LOG_INFO(qsfp, "device_tech 0x%X",
                           id->base.device_tech);

    TRX_LOG_INFO(qsfp, "vendor name %.*s",
                           (int)sizeof(id->base.vendor_name),
                           id->base.vendor_name);

    TRX_LOG_INFO(qsfp, "ext_module 0x%X",
                           id->base.ext_module);

    TRX_LOG_INFO(qsfp, "vendor_oui[]: 0x%X 0x%X 0x%X",
    id->base.vendor_oui[2], id->base.vendor_oui[1], id->base.vendor_oui[0]);

    TRX_LOG_INFO(qsfp, "vendor pn %.*s",
               (int)sizeof(id->base.vendor_pn), id->base.vendor_pn);

    TRX_LOG_INFO(qsfp, "vendor rev %.*s",
               (int)sizeof(id->base.vendor_rev), id->base.vendor_rev);

    TRX_LOG_INFO(qsfp, "wavelength 0x%X   wavelength_tolerance 0x%X   "
    "max_case_temp 0x%X   cc_base 0x%X", id->base.wavelength,
    id->base.wavelength_tolerance, id->base.max_case_temp,
    id->base.cc_base);

    TRX_LOG_INFO(qsfp, "link_codes 0x%X",
                           id->ext.link_codes);

    TRX_LOG_INFO(qsfp, "lpmode_gpio 0x%X  intl_gpio 0x%X "
    "tx_adap_eq_freeze 0x%X  tx_eq_auto_adap 0x%X  tx_eq_prg 0x%X  "
    "rx_emp_prg 0x%X  rx_amp_prg 0x%X", id->ext.lpmode_gpio,
    id->ext.intl_gpio, id->ext.tx_adap_eq_freeze, id->ext.tx_eq_auto_adap,
    id->ext.tx_eq_prg, id->ext.rx_emp_prg, id->ext.rx_amp_prg);

    TRX_LOG_INFO(qsfp, "tx_cdr_ctrl_impl 0x%X  rx_cdr_ctrl_impl 0x%X  "
    "tx_cdr_lol_impl 0x%X  rx_cdr_lol_impl 0x%X  rx_squelch_dis_impl 0x%X  "
    "rx_output_dis_impl 0x%X  tx_squelch_dis_impl 0x%X  "
    "tx_squelch_impl 0x%X", id->ext.tx_cdr_ctrl_impl,
    id->ext.rx_cdr_ctrl_impl, id->ext.tx_cdr_lol_impl, id->ext.rx_cdr_lol_impl,
    id->ext.rx_squelch_dis_impl, id->ext.rx_output_dis_impl,
    id->ext.tx_squelch_dis_impl, id->ext.tx_squelch_impl);

    TRX_LOG_INFO(qsfp, "page2 0x%X  page1 0x%X  rate_select_impl 0x%X  "
    "tx_dis_impl 0x%X  tx_fault_impl 0x%X  tx_squelch_oma_impl 0x%X  "
    "tx_los_impl 0x%X  page20_21 0x%X", id->ext.page2,
    id->ext.page1, id->ext.rate_select_impl, id->ext.tx_dis_impl,
    id->ext.tx_fault_impl, id->ext.tx_squelch_oma_impl, id->ext.tx_los_impl,
    id->ext.page20_21);

    TRX_LOG_INFO(qsfp, "vendor sn %.*s",
               (int)sizeof(id->ext.vendor_sn), id->ext.vendor_sn);

    date[0] = id->ext.datecode[4];
    date[1] = id->ext.datecode[5];
    date[2] = '-';
    date[3] = id->ext.datecode[2];
    date[4] = id->ext.datecode[3];
    date[5] = '-';
    date[6] = id->ext.datecode[0];
    date[7] = id->ext.datecode[1];
    date[8] = '\0';

    TRX_LOG_INFO(qsfp, "date %s", date);
    TRX_LOG_INFO(qsfp, "diagmon 0x%X   enh_options 0x%X   "
    "baud_rate_nominal 0x%X    cc_ext 0x%X", id->ext.diagmon,
    id->ext.enh_options, id->ext.baud_rate_nominal, id->ext.cc_ext);

}

static u8 sff8636_get_state(struct qsfp *qsfp)
{
    int ret;
    u8 state = 0;
    bool poll = false;
    struct sff8636_irq_flags irq_flags = {0};

    if (qsfp->need_poll) {
        /* In case of poll just read LOS TX Fault s*/
        ret = qsfp_read(qsfp, SFF8636_IRQ_FLAGS, &irq_flags, 2);
    } else {
        ret = qsfp_read(qsfp, SFF8636_IRQ_FLAGS, &irq_flags,
                        sizeof(irq_flags));
    }

    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to read QSFP IRQ status. ret %d", ret);
        /* Preserve the current state */
        return qsfp->state;
    }

    if (irq_flags.los_rx) {
        /* Dont report LOS even if one lane works fine
         * consider only RX lanes
         */
        if (irq_flags.los_rx == 0xF) {
            state |= QSFP_F_LOS;
        } else if (!qsfp->need_poll) {
            TRX_LOG_INFO(qsfp, "There is LOS on few lanes which is not"
                       " reported 0x%X", irq_flags.los_rx);
        }
        /* In case RX LOS we have to enable poll */
        poll = true;
    }

    if (irq_flags.los_tx) {
        /* In case TX LOS we have to enable poll */
        poll = true;
    }

    if (irq_flags.tx_fault) {
        /* Dont report TX Fault even if one lane works fine
         * Nibble represent TX Fault for 4 TX lanes
         */
        if (irq_flags.tx_fault == 0xF) {
            state |= QSFP_F_TX_FAULT;
        } else if (!qsfp->need_poll) {
            TRX_LOG_INFO(qsfp, "There is TX Fault on few lanes which "
                       "is not reported 0x%X", irq_flags.tx_fault);
        }
        /* In case TX Fault we have to enable poll */
        poll = true;
    }

    if (!qsfp->need_poll) {
        TRX_LOG_INFO(qsfp, "IRQ status dump: LOS RX 0x%X LOS TX 0x%X TX Fault"
        " 0x%X eq 0x%X LOL 0x%X Init 0x%X ready 0x%X Temp 0x%X VCC 0x%X Vendor"
        " 0x%X RX12_Power 0x%X RX34_Power 0x%X TX12_bias 0x%X TX34_bias 0x%X "
        "TX12_pow 0x%X TX34_pow 0x%X Vendor 0x%X 0x%X 0x%X",
        irq_flags.los_rx, irq_flags.los_tx, irq_flags.tx_fault,
        irq_flags.tx_adap_eq_fault, irq_flags.lol, irq_flags.init_complete,
        irq_flags.tc_ready, irq_flags.temp_alarm, irq_flags.volt_alarm,
        irq_flags.vendor_specific1, irq_flags.rx12_pow_alarm,
        irq_flags.rx34_pow_alarm, irq_flags.tx12_bias_alarm,
        irq_flags.tx34_bias_alarm, irq_flags.tx12_pow_alarm,
        irq_flags.tx34_pow_alarm, irq_flags.vendor_specific2[0],
        irq_flags.vendor_specific2[1], irq_flags.vendor_specific2[2]);
    }

    qsfp->need_poll = poll;

    return state;
}

static void sff8636_tx_disable(const struct qsfp *qsfp)
{
    u8 status;
    int ret;

    status = 0xF;

    ret = qsfp_write(qsfp, SFF8636_TX_DISABLE, &status,
                     sizeof(status));
    if (ret < 0)
        TRX_LOG_ERR(qsfp, "TX disable failed. ret %d", ret);

}

static void sff8636_tx_enable(const struct qsfp *qsfp)
{
    u8 status;
    int ret;

    status = 0;

    ret = qsfp_write(qsfp, SFF8636_TX_DISABLE, &status,
                     sizeof(status));
    if (ret < 0)
        TRX_LOG_ERR(qsfp, "TX enable failed. ret %d", ret);

}

static int sff8636_module_info(struct qsfp *qsfp, struct ethtool_modinfo *modinfo)
{
    switch (qsfp->id.sff8636.base.phys_id) {
    case ETH_MODULE_SFF_8636:
        modinfo->type = ETH_MODULE_SFF_8636;
        modinfo->eeprom_len = ETH_MODULE_SFF_8636_LEN;
        break;

    case ETH_MODULE_SFF_8436:
        modinfo->type = ETH_MODULE_SFF_8436;
        modinfo->eeprom_len = ETH_MODULE_SFF_8436_LEN;
        break;

    default:
        modinfo->type = 0;
        modinfo->eeprom_len = 0;
        break;
    }

    return 0;
}

const char *sff8636_mod_revision_to_str(u8 mod_rev_value)
{
    switch (mod_rev_value) {
    case 0x00:
    default:
        return "Revision not specified";
    case 0x01:
        return "SFF-8436 Rev 4.8 or earlier";
    case 0x02:
        return "revision 4.8 or earlier of SFF-8436";
    case 0x03:
        return "SFF-8636 Rev 1.3 or earlier";
    case 0x04:
        return "SFF-8636 Rev 1.4";
    case 0x05:
        return "SFF-8636 Rev 1.5";
    case 0x06:
        return "SFF-8636 Rev 2.0";
    case 0x07:
        return "SFF-8636 Rev 2.5, 2.6 and 2.7";
    case 0x08:
        return "SFF-8636 Rev 2.8, 2.9 and 2.10";
    case 0x09 ... 0xFF:
        return "Reserved need to be update in future";
    }
}

const char *sff8636_mod_encoding_to_str(u8 mod_encoding)
{
    switch (mod_encoding) {
    case 0x00:
    default:
        return "Unspecified";
    case 0x01:
        return "8B/10B";
    case 0x02:
        return "4B/5B";
    case 0x03:
        return "NRZ";
    case 0x04:
        return "SONET Scrambled";
    case 0x05:
        return "64B/66B";
    case 0x06:
        return "Manchester";
    case 0x07:
        return "256B/257B";
    case 0x08:
        return "PAM4";
    case 0x09 ... 0xFF:
        return "Reserved need to be update in future";
    }
}

/*
 * Function to get link speed from the linkcodes.
 */
static u8 sff8024_link_codes_to_speed(unsigned short mod_link_codes)
{
    switch (mod_link_codes) {
    /* Speed unknown */
    case 0x00:
    /*  400GBPS not supported by sff8636 */
    case 0x3F:
    case 0x47 ... 0x49:
    case 0x4B ... 0x4C:

    /* Assume that SFF-8636 does not support 50GBPS and that this
       feature should be revisited if such a transceiver is discovered. */
    case 0x0C ... 0x0D:
    case 0x4A:
    case 0x45:
    case 0x39:

    /* Assume that SFF-8636 does not support 25GBPS and that this feature
       should be revisited if such a transceiver is discovered. */
    case 0x38:

    /*  Assume that SFF-8636 does not support 5GBPS and that this feature
       should be revisited if such a transceiver is discovered. */
    case 0x1D:

    /*  Assume that SFF-8636 does not support 2.5GBPS and that this feature
       should be revisited if such a transceiver is discovered. */
    case 0x1E:
    default:
        return TRX_LANE_SPEED_UNKNOWN;

    /* Total transceiver speed supported was 200 GBPS. As per SFF-8636,
       the supported number of lanes was 4, so each lane supports 50 GBPS.*/
    case 0x40 ... 0x44:
    case 0x46:
        return TRX_LANE_SPEED_50G;

    /* Total transceiver speed supported was 100 GBPS. As per SFF-8636,
       the supported number of lanes was 4, so each lane supports 25 GBPS. */
    case 0x01 ... 0x08:
    case 0x0B:
    case 0x16 ... 0x1B:
    case 0x20 ... 0x21:
    case 0x25 ... 0x2F:
    case 0x34 ... 0x36:
    case 0x3A:
        return TRX_LANE_SPEED_25G;

    /* Total transceiver speed supported was 40 GBPS. As per SFF-8636,
       the supported number of lanes was 4, so each lane supports 10 GBPS. */
    case 0x10 ... 0x12:
    case 0x1F:
        return TRX_LANE_SPEED_10G;
    /* 0x13 to 0x15 G959.1 profiles need to confirm the speed */
    /* Total transceiver speed supported was 10 GBPS. As per SFF-8636,
       the supported number of lanes was 4, so each lane supports 2.5 GBPS. */
    case 0x1C:
    case 0x37:
        return TRX_LANE_SPEED_2_5G;

    /* 0x22 to 0x24 are of type 4WDM need to check the speed capability*/
    /* 0x30 to 0x33 are of type active Copper or Optical need to check
        the speed capability */
    /* 0x3B-0x3E was Reserved need to be update in future.*/
    /* 0x4D-0x7E was Reserved need to be update in future.*/
    /* 0x82-0xFF Reserved was Reserved need to be update in future.*/
    /* case 0x7F 0x80 0x81 speeds are not clear need to be update in future.*/
    }

}

u8 sff8636_get_connector_type(const struct qsfp *qsfp)
{
    return qsfp->id.sff8636.base.connector;
}

/*
 * Function to get the lane supported speed using linkcodes page 00h, byte 192,
 * or ethernet compliance codes page 00h, byte 131.
 */
static int sff8636_get_lane_speed(const struct qsfp *qsfp,
                                  trx_lane_speed* lane_speed)
{
    const struct sff8636_eeprom_id *id = &qsfp->id.sff8636;

    /* Check ethernet compliance codes page 00h byte 131 */
    if (id->base.ecom_extended == 0x1) {
        *lane_speed = sff8024_link_codes_to_speed(id->ext.link_codes);
    }
    else if ((id->base.e10g_base_lrm == 0x1) ||
             (id->base.e10g_base_lr == 0x1)  ||
             (id->base.e10g_base_sr == 0x1)) {
         *lane_speed = TRX_LANE_SPEED_2_5G;
    }
    else if ((id->base.e40g_base_cr4 == 0x1) ||
          (id->base.e40g_base_sr4 == 0x1) ||
          (id->base.e40g_base_lr4 == 0x1) ||
          (id->base.e40g_active == 0x1)) {
        *lane_speed = TRX_LANE_SPEED_10G;
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
static u8 sff8636_get_transceiver_type(const struct qsfp *qsfp)
{
    return qsfp->id.sff8636.base.phys_id;
}

/*
 * Function to get channel information from EEPROM page 00h byte 113.
 */
static int sff8636_get_lanes_presence(const struct qsfp *qsfp,
                                      trx_lane_cfg* laneinfo)
{
    u8 channel = 0;
    int ret;

    ret = qsfp_read(qsfp, SFF8636_CHANNEL_INFO, &channel,
                     sizeof(channel));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Channel register read failed. ret %d", ret);
        return -EINVAL;
    }

    *laneinfo = ~channel;
    /* SFF-8636 supports a maximum of four lanes, the first
       four bytes are required to check for lane presence. */
    *laneinfo &= 0x0F;

    TRX_LOG_INFO(qsfp, "Lane info: 0x%X", *laneinfo);

    return 0;
}


static int sff8636_get_breakout_config(const struct qsfp *qsfp,
                                       trx_breakout_cfg* bout_config)
{
    u8 buf = 0;
    int ret;

    ret = qsfp_read(qsfp, SFF8636_FREE_SIDE_PROP, &buf,
                     sizeof(buf));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Far-end support register read failed, ret %d", ret);
        return -EINVAL;
    }

    /* BIT(3): A value of 1 indicates that the far end is managed and
       complies with SFF-8636.*/
    buf >>= 3;
    if (~(buf & 1)) {
        /* Using 0XFF to indicate the far-end configuration did not
           support transceivers with detachable connectors.*/
        *bout_config = TRX_FAR_END_NOT_MANAGED;
        TRX_LOG_INFO(qsfp, "Breakout config: 0x%X ",
                              *bout_config);
        return 0;
    }

    buf = 0;
    ret = qsfp_read(qsfp, SFF8636_CHANNEL_INFO, &buf,
                     sizeof(buf));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Breakout config register read failed, ret %d", ret);
        return -EINVAL;
    }

    /* BIT[6:4]: indicates Far-end implementation */
    buf = buf >> 4;
    buf &= 0x07;

    *bout_config = buf;

    TRX_LOG_INFO(qsfp, "Breakout config: 0x%X ",
                          *bout_config);

    return 0;
}

unsigned long sff8636_irq_delay(const struct qsfp *qsfp)
{
    return 0;
}

struct qsfp_spec_ops sff8636_spec_ops = {
    .mod_probe = sff8636_mod_probe,
    .disable_redundant_irq = sff8636_disable_redundant_irq,
    .get_state = sff8636_get_state,
    .tx_enable = sff8636_tx_enable,
    .tx_disable = sff8636_tx_disable,
    .check_features_impl = sff8636_check_feature_impl,
    .module_parse_power = sff8636_module_parse_power,
    .handle_max_power_exceed = sff8636_handle_max_power_exceed,
    .mod_high_power = sff8636_mod_high_power,
    .mod_low_power = sff8636_mod_low_power,
    .eeprom_print = sff8636_eeprom_print,
    .module_info = sff8636_module_info,
    .get_connector_type = sff8636_get_connector_type,
    .get_lane_speed = sff8636_get_lane_speed,
    .get_transceiver_type = sff8636_get_transceiver_type,
    .get_lanes_presence = sff8636_get_lanes_presence,
    .get_breakout_config = sff8636_get_breakout_config,
    .irq_delay = sff8636_irq_delay,
    .create_debugfs = sff8636_create_debugfs_files,
};
