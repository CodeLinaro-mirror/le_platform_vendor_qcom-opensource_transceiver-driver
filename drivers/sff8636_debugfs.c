/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include "transceiver_debugfs.h"

static int qsfp_debug_power_info_show(struct seq_file *s, void *data)
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
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_power_info);

static int qsfp_debug_module_power_class_info_show(struct seq_file *s,
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
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_module_power_class_info);

static int qsfp_debug_phys_ext_id_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
    u8 *spec_id = (u8*)&qsfp->id;
    u8 powerclassl,powerclassh;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        powerclassl = id->base.phys_ext_id >> 6;
        powerclassh = id->base.phys_ext_id & 0x03;
        seq_printf(s, "Extended Identifier: 0x%X\n",id->base.phys_ext_id);
        seq_printf(s, "BIT[7-6]: ");
        switch (powerclassl) {
        case 0x00:
            seq_printf(s, "Power Class 1 (1.5 W max)\n");
            break;
        case 0x01:
            seq_printf(s, "Power Class 2 (2.0 W max)\n");
            break;
        case 0x02:
            seq_printf(s, "Power Class 3 (2.5 W max)\n");
            break;
        case 0x03:
            seq_printf(s, "Power Class 4 (3.5 W max) and Power Classes 5,"
                          " 6 or 7\n");
            break;
        }
        id->base.phys_ext_id&BIT(5)?seq_printf(s, "  BIT[5]: Power Class 8 "
                                    "implemented\n"):
                  seq_printf(s, "  BIT[5]: Power Class 8 not implemented\n");
        id->base.phys_ext_id&BIT(4)?seq_printf(s, "  BIT[4]: CLEI code present"
                                " in Page 02h\n"):
                  seq_printf(s, "  BIT[4]: No CLEI code present in"
                                " Page 02h\n");
        id->base.phys_ext_id&BIT(3)?seq_printf(s, "  BIT[3]: CDR present"
                                " in Tx\n"):
                  seq_printf(s, "  BIT[3]: No CDR in Tx\n");
        id->base.phys_ext_id&BIT(2)?seq_printf(s, "  BIT[2]: CDR present in"
                                " Rx\n"):
                  seq_printf(s, "  BIT[2]: No CDR in Rx\n");
        seq_printf(s, "BIT[1-0]: ");
        switch (powerclassh) {
        case 0x00:
            seq_printf(s, "Power Classes 1 to 4\n");
            break;
        case 0x01:
            seq_printf(s, "Power Class 5 (4.0 W max)\n");
            break;
        case 0x02:
            seq_printf(s, "Power Class 6 (4.5 W max)\n");
            break;
        case 0x03:
            seq_printf(s, "Power Class 7 (5.0 W max)\n");
            break;
        }
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_phys_ext_id);

static int qsfp_debug_encoding_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        seq_printf(s, "{0x%X} %s\n",id->base.encoding,
                     sff8636_mod_encoding_to_str(id->base.encoding));
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_encoding);

static int qsfp_debug_link_codes_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        seq_printf(s, "{0x%X} %s\n",id->ext.link_codes,
                     mod_link_codes_to_str(id->ext.link_codes));
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_link_codes);

static int qsfp_debug_max_case_temp_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        seq_printf(s, "0x%X\n",id->base.max_case_temp);
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_max_case_temp);

static int qsfp_debug_device_tech_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
    u8 *spec_id = (u8*)&qsfp->id;
    u8 dtechl,dtechr;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
          seq_printf(s, "QSFP transceiver not inserted\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        dtechl = id->base.device_tech >> 4;
        dtechr = id->base.device_tech & 0x0F;
        seq_printf(s, "Device Technology: 0x%X\n",id->base.device_tech);
        seq_printf(s, "Transmitter technology BIT[7-4]: ");
        switch (dtechl) {
        case 0x00:
            seq_printf(s, "850 nm VCSEL\n");
            break;
        case 0x01:
            seq_printf(s, "1310 nm VCSEL\n");
            break;
        case 0x02:
            seq_printf(s, "1550 nm VCSEL\n");
            break;
        case 0x03:
            seq_printf(s, "1310 nm FP\n");
            break;
        case 0x04:
            seq_printf(s, "1310 nm DFB\n");
            break;
        case 0x05:
            seq_printf(s, "1550 nm DFB\n");
            break;
        case 0x06:
            seq_printf(s, "1310 nm EML\n");
            break;
        case 0x07:
            seq_printf(s, "1550 nm EML\n");
            break;
        case 0x08:
            seq_printf(s, "Other / Undefined\n");
            break;
        case 0x09:
            seq_printf(s, "1490 nm DFB\n");
            break;
        case 0x0A:
            seq_printf(s, "Copper cable unequalized\n");
            break;
        case 0x0B:
            seq_printf(s, "Copper cable passive equalized\n");
            break;
        case 0x0C:
            seq_printf(s, "Copper cable, near and far end limiting active"
                           "equalizers\n");
            break;
        case 0x0D:
            seq_printf(s, "Copper cable, far end limiting active"
                          " equalizers\n");
            break;
        case 0x0E:
            seq_printf(s, "Copper cable, near end limiting active"
                          " equalizers\n");
            break;
        case 0x0F:
            seq_printf(s, "Copper cable, linear active equalizers\n");
            break;
        }
        dtechr&BIT(3)?seq_printf(s, "BIT[3]: Active wavelength control\n"):
                       seq_printf(s, "BIT[3]: No wavelength control\n");
        dtechr&BIT(2)?seq_printf(s, "BIT[2]: Cooled transmitter\n"):
                       seq_printf(s, "BIT[2]: Uncooled transmitter device\n");
        dtechr&BIT(1)?seq_printf(s, "BIT[1]: APD detector\n"):
                       seq_printf(s, "BIT[1]: Pin detector\n");
        dtechr&BIT(0)?seq_printf(s, "BIT[0]: Transmitter tunable\n"):
                         seq_printf(s, "BIT[0]: Transmitter not tunable\n");
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_tech);

static int qsfp_debug_device_diagmon_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
          seq_printf(s, "QSFP transceiver not inserted\n");
          return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        seq_printf(s, "Diagnostic Monitoring Type: 0x%X\n",id->ext.diagmon);
        id->ext.diagmon&BIT(5)?seq_printf(s, "BIT[5]: Temperature monitoring"
                                             " implemented\n"):
            seq_printf(s, "BIT[5]: Temperature monitoring Not implemented"
                              " or pre-Rev 2.8\n");
        id->ext.diagmon&BIT(4)?seq_printf(s, "BIT[4]: Supply voltage"
                                             " monitoring implemented\n"):
            seq_printf(s, "BIT[4]: Supply voltage monitoring Not implemented"
                          " or pre-Rev 2.8\n");
        id->ext.diagmon&BIT(3)?seq_printf(s, "BIT[3]: Received power "
                               "measurements type is Average Power\n"):
            seq_printf(s, "BIT[3]: Received power measurements type is OMA\n");
        id->ext.diagmon&BIT(2)?seq_printf(s, "BIT[2]: Transmitter power "
                                             "measurement Supported\n"):
            seq_printf(s, "BIT[2]: Transmitter power measurement "
                                               "Not supported\n");
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_diagmon);

/*
* Information about the Qsfp status indicators (Page 00h, Byte 2) is being
* exported to Sysfs.
*/
static int qsfp_debug_status_indicators_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    u8 *spec_id = (u8*)&qsfp->id;
    u8 status = 0;
    int ret;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        ret = qsfp_read(qsfp, SFF8636_IRQ_FLAGS, &status,
                         sizeof(status));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        seq_printf(s, "Status Indicators: 0x%X\n",status);
        status&BIT(2)?seq_printf(s, "BIT[2]: Lower and upper pages 00h"
                                    " only\n"):
                      seq_printf(s, "BIT[2]: Upper page 03h implemented\n");
        status&BIT(1)?seq_printf(s, "BIT[1]: IntL not asserted\n"):
                      seq_printf(s, "BIT[1]: IntL asserted\n");
        status&BIT(0)?seq_printf(s, "BIT[0]: Free side does not yet have"
                                    " valid monitor data due to device reset\n"
                                    "or power up reset or prior to a valid "
                                    "suite of monitor readings\n"):
                      seq_printf(s, "BIT[0]: Free side have valid "
                                    "monitor data like temperature "
                                    "& supply voltage\n");
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_status_indicators);

int sff8636_create_debugfs_files (struct qsfp *qsfp) {
    struct dentry *file = NULL;
    int ret = 0;

    ret = module_debugfs_init(qsfp);
    if (ret != 0)
    {
        TRX_LOG_ERR(qsfp, "qsfp module_spec_info debugfs dir fail");
        return 0;
    }

    ret = create_common_debugfs_files(qsfp);
    if(ret != 0)
    {
        TRX_LOG_ERR(qsfp, "common debugfs files create fail ");
        return 0;
    }

    file = debugfs_create_file("power_info", 0600, qsfp->module_debugfs_dir,
                    qsfp, &qsfp_debug_power_info_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp power_info debugfs_create_file fail,"
                        "error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("module_power_class", 0600,
                    qsfp->module_debugfs_dir,
                    qsfp, &qsfp_debug_module_power_class_info_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp module_power_class debugfs_create_file"
                        "fail, error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("phys_ext_id", 0600, qsfp->module_debugfs_dir,
                qsfp, &qsfp_debug_phys_ext_id_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp phys_ext_id debugfs_create_file fail,"
                        "error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("encoding", 0600, qsfp->module_debugfs_dir,
                    qsfp, &qsfp_debug_encoding_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp encoding debugfs_create_file fail,"
                        "error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("link_codes", 0600, qsfp->module_debugfs_dir,
                    qsfp, &qsfp_debug_link_codes_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp link_codes debugfs_create_file fail,"
                        " error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("max_case_temp", 0600, qsfp->module_debugfs_dir,
                    qsfp, &qsfp_debug_max_case_temp_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp max_case_temp debugfs_create_file fail,"
                        " error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("device_tech", 0600, qsfp->module_debugfs_dir,
                    qsfp, &qsfp_debug_device_tech_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp device_tech debugfs_create_file fail,"
                        " error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("diagmon", 0600, qsfp->module_debugfs_dir,
                    qsfp, &qsfp_debug_device_diagmon_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp diagmon debugfs_create_file fail,"
                        " error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("status_indicators", 0600,
                               qsfp->module_debugfs_dir, qsfp,
                               &qsfp_debug_status_indicators_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp status_indicators debugfs_create_file "
                        "fail, error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    return 0;

failed_module_dir:
    debugfs_remove_recursive(qsfp->module_debugfs_dir);
    qsfp->module_debugfs_dir = NULL;
    return 0;
}


