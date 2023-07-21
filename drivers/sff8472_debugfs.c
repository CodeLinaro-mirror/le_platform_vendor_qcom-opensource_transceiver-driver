/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include "transceiver_debugfs.h"

const char *sff8472_mod_revision_to_str(u8 mod_rev_value)
{
    switch (mod_rev_value) {
    case 0x00:
    default:
        return "Revision not specified, unexpected value for SFF-8472";
    case 0x01:
        return "Includes functionality as per Rev 9.3 of SFF-8472.";
    case 0x02:
        return "Includes functionality as per Rev 9.5 of SFF-8472.";
    case 0x03:
        return "Includes functionality as per Rev 10.2 of SFF-8472.";
    case 0x04:
        return "Includes functionality as per Rev 10.4 of SFF-8472.";
    case 0x05:
        return "Includes functionality as per Rev 11.0 of SFF-8472.";
    case 0x06:
        return "Includes functionality as per Rev 11.3 of SFF-8472.";
    case 0x07:
        return "Includes functionality as per Rev 11.4 of SFF-8472.";
    case 0x08:
        return "Includes functionality as per Rev 12.3 of SFF-8472.";
    case 0x09:
        return "Includes functionality as per Rev 12.4 of SFF-8472.";
    case 0x0A ... 0xFF:
        return "Reserved needs to be updated in the future.";
    }
}

const char *sff8472_mod_encoding_to_str(u8 mod_encoding)
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
        return "Manchester";
    case 0x05:
        return "SONET Scrambled";
    case 0x06:
        return "64B/66B";
    case 0x07:
        return "256B/257B";
    case 0x08:
        return "PAM4";
    case 0x09 ... 0xFF:
        return "Reserved needs to be updated in the future.";
    }
}

static int sfp_debug_enhopts_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sfp_eeprom_id *id;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
          seq_printf(s, "QSFP transceiver not inserted\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        id = &qsfp->id.sff8472;

        seq_printf(s, "Enhanced Options: 0x%X\n",id->ext.enhopts);
        id->ext.enhopts&BIT(7)?seq_printf(s, "BIT[7]: Optional Alarm/warning"
                     " flags implemented for all monitored quantities.\n"):
           seq_printf(s, "BIT[7]: Optional Alarm/warning flags are not "
                                                      "implemented.\n");
        id->ext.enhopts&BIT(6)?seq_printf(s, "BIT[6]: Optional soft TX_DISABLE"
            " control and monitoring is implemented.\n"):seq_printf(s,
            "BIT[6]: Optional soft TX_DISABLE control and monitoring are not "
            "implemented.\n");
        id->ext.enhopts&BIT(5)?seq_printf(s, "BIT[5]: Optional soft TX_FAULT "
            "monitoring is implemented.\n"):seq_printf(s, "BIT[5]: Optional "
            "soft TX_FAULT monitoring is not implemented.\n");
        id->ext.enhopts&BIT(4)?seq_printf(s, "BIT[4]: Optional soft RX_LOS "
            "monitoring is implemented.\n"):seq_printf(s, "BIT[4]: Optional "
            "soft RX_LOS monitoring is not implemented.\n");
        id->ext.enhopts&BIT(3)?seq_printf(s, "BIT[3]: Optional soft "
            "RATE_SELECT control and monitoring are implemented.\n"):
            seq_printf(s, "BIT[3]: Optional soft RATE_SELECT control "
                             "and monitoring are not implemented.\n");
        id->ext.enhopts&BIT(2)?seq_printf(s, "BIT[2]: Optional Application "
            "Select Control is implemented as per SFF-8079.\n"):seq_printf(s,
            "BIT[2]: Optional Application Select Control is not "
            "implemented.\n");
        id->ext.enhopts&BIT(1)?seq_printf(s, "BIT[1]: Optional soft Rate "
             "Select control is implemented as per SFF-843.\n"):seq_printf(s,
            "BIT[1]: Optional Rate Select control is not implemented.\n");
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(sfp_debug_enhopts);

static int sfp_debug_diagmon_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sfp_eeprom_id *id;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
          seq_printf(s, "QSFP transceiver not inserted\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        id = &qsfp->id.sff8472;
        seq_printf(s, "Diagnostic Monitoring Type: 0x%X\n",id->ext.diagmon);
        id->ext.diagmon&BIT(6)?seq_printf(s, "BIT[6]: Digital diagnostic"
            " monitoring is implemented.\n"):seq_printf(s, "BIT[6]: Digital "
            "diagnostic monitoring is not implemented.\n");
        id->ext.diagmon&BIT(5)?seq_printf(s, "BIT[5]: Internally calibrated,"
            " means the transceiver directly reports calibrated values in "
            "units of current, power etc.\n"):seq_printf(s, "BIT[5]: "
            "Internal calibration is not set.\n");
        id->ext.diagmon&BIT(4)?seq_printf(s, "BIT[4]: Externally calibrated, "
            "means the reported DDM values are A/D counts which must be "
            "converted to real world units.\n"):seq_printf(s, "BIT[4]: "
            "External calibration is not set.\n");
        id->ext.diagmon&BIT(3)?seq_printf(s, "BIT[3]: Received power "
            "measurement type is average power.\n"):seq_printf(s,
            "BIT[3]: Received power measurement type is OMA.\n");
        id->ext.diagmon&BIT(2)?seq_printf(s, "BIT[2]: Address change sequence "
            "required.\n"):seq_printf(s, "BIT[2]: Address change sequence is "
            "not required.\n");
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(sfp_debug_diagmon);

static int sfp_debug_power_info_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    seq_printf(s, "Maximum Power (mW) : %u\n",qsfp->max_power_mW);
    seq_printf(s, "Module Power (mW) : %u\n",qsfp->module_power_mW);

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(sfp_debug_power_info);

static int sfp_debug_module_power_class_info_show(struct seq_file *s,
                               void *data)
{
    struct qsfp *qsfp = s->private;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    seq_printf(s, "0x%X\n",qsfp->module_power_class);

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(sfp_debug_module_power_class_info);

static int sfp_debug_phys_ext_id_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sfp_eeprom_id *id;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        id = &qsfp->id.sff8472;
        seq_printf(s, "Extended Identifier: 0x%X\n",id->base.phys_ext_id);
        switch (id->base.phys_ext_id) {
        case 0x00:
            seq_printf(s, "GBIC definition is not specified or \nGBIC "
               "definition is not compliant with a defined MOD_DEF.\n");
            break;
        case 0x01:
            seq_printf(s, "GBIC is compliant with MOD_DEF 1.\n");
            break;
        case 0x02:
            seq_printf(s, "GBIC is compliant with MOD_DEF 2.\n");
            break;
        case 0x03:
            seq_printf(s, "GBIC is compliant with MOD_DEF 3.\n");
            break;
        case 0x04:
            seq_printf(s, "GBIC/SFP function is defined by 2-wire "
                          "interface ID only.\n");
            break;
        case 0x05:
            seq_printf(s, "GBIC is compliant with MOD_DEF 5.\n");
            break;
        case 0x06:
            seq_printf(s, "GBIC is compliant with MOD_DEF 6.\n");
            break;
        case 0x07:
            seq_printf(s, "GBIC is compliant with MOD_DEF 7.\n");
            break;
        case 0x08 ... 0xFF:
            seq_printf(s, "Reserved needs to be updated in the future.\n");
            break;
        default:
            seq_printf(s, "Invalid physical device extended identifier"
                                    ": 0x%X \n", id->base.phys_ext_id);
        }
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(sfp_debug_phys_ext_id);

static int sfp_debug_encoding_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sfp_eeprom_id *id;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        id = &qsfp->id.sff8472;
        seq_printf(s, "{0x%X} %s\n",id->base.encoding,
                     sff8472_mod_encoding_to_str(id->base.encoding));
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(sfp_debug_encoding);

int sff8472_create_debugfs_files(struct qsfp *qsfp)
{
    struct dentry *file = NULL;
    int ret = 0;

    ret = module_debugfs_init(qsfp);
    if (ret != 0)
    {
        TRX_LOG_ERR(qsfp, "SFP module_spec_info debugfs dir create fail.\n");
        return 0;
    }

    ret = create_common_debugfs_files(qsfp);
    if(ret != 0)
    {
        TRX_LOG_ERR(qsfp, "Common debugfs files create fail ");
        return 0;
    }

    file = debugfs_create_file("enhopts", 0600, qsfp->module_debugfs_dir,
                    qsfp, &sfp_debug_enhopts_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "QSFP enhopts debugfs_create_file fail,"
                        "error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("diagmon", 0600, qsfp->module_debugfs_dir,
                    qsfp, &sfp_debug_diagmon_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "QSFP diagmon debugfs_create_file fail,"
                        "error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("power_info", 0600, qsfp->module_debugfs_dir,
                    qsfp, &sfp_debug_power_info_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "QSFP power_info debugfs_create_file fail,"
                        "error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("module_power_class", 0600,
                    qsfp->module_debugfs_dir,
                    qsfp, &sfp_debug_module_power_class_info_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "QSFP module_power_class debugfs_create_file"
                        "fail, error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("phys_ext_id", 0600, qsfp->module_debugfs_dir,
                qsfp, &sfp_debug_phys_ext_id_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "QSFP phys_ext_id debugfs_create_file fail,"
                        "error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("encoding", 0600, qsfp->module_debugfs_dir,
                    qsfp, &sfp_debug_encoding_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "QSFP encoding debugfs_create_file fail,"
                        "error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    return 0;

failed_module_dir:
    debugfs_remove_recursive(qsfp->module_debugfs_dir);
    qsfp->module_debugfs_dir = NULL;
    return 0;
}

