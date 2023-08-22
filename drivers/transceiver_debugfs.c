/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include "transceiver_debugfs.h"
#include "lane.h"


#if IS_ENABLED(CONFIG_DEBUG_FS)

struct dentry *transceiver_debugfs_dir  = NULL;

const char *mod_identifier_to_str(u8 spec_id)
{
    switch (spec_id) {
    case 0x00:
    default:
        return "Unknown or unspecified";
    case 0x01:
        return "GBIC";
    case 0x02:
        return "Module/connector soldered to motherboard (using SFF-8472)";
    case 0x03:
        return "SFP/SFP+/SFP28 and later";
    case 0x04:
        return "300 pin XBI";
    case 0x05:
        return "XENPAK";
    case 0x06:
        return "XFP";
    case 0x07:
        return "XFF";
    case 0x08:
        return "XFP-E";
    case 0x09:
        return "XPAK";
    case 0x0A:
        return "X2";
    case 0x0B:
        return "DWDM-SFP/SFP+";
    case 0x0C:
        return "QSFP (INF-8438)";
    case 0x0D:
        return "QSFP+ or SFF-8636 or SFF-8436";
    case 0x0E:
        return "CXP or later";
    case 0x0F:
        return "Shielded Mini Multilane HD 4X";
    case 0x10:
        return "Shielded Mini Multilane HD 8X";
    case 0x11:
        return "QSFP28 or SFF-8636";
    case 0x12:
        return "CXP2 (aka CXP28) or later";
    case 0x13:
        return "CDFP (Style 1/Style2)";
    case 0x14:
        return "Shielded Mini Multilane HD 4X Fanout Cable";
    case 0x15:
        return "Shielded Mini Multilane HD 8X Fanout Cable";
    case 0x16:
        return "CDFP (Style 3)";
    case 0x17:
        return "micro QSFP";
    case 0x18:
        return "QSFP-DD Double Density 8X Pluggable Transceiver (INF-8628)";
    case 0x19:
        return "OSFP 8X Pluggable Transceiver";
    case 0x1A:
        return "SFP-DD Double Density 2X Pluggable Transceiver";
    case 0x1B:
        return "DSFP Dual Small Form Factor Pluggable Transceiver";
    case 0x1C:
        return "x4 MiniLink/OcuLink";
    case 0x1D:
        return "x8 MiniLink";
    case 0x1E:
        return "QSFP+ or CMIS";
    /* 0x1F to 0x7F was Reserved need to be updated in future. */
    case 0x80 ... 0xFF:
        return "Vendor specific";
    }
}

static const char *mod_connector_to_str(u8 mod_connector,char* vendor_name,
                                                char* vendor_data)
{
    switch (mod_connector) {
    case 0x00:
    default:
        return "Unknown";
    case 0x01:
        return "Subscriber Connector";
    case 0x02:
        return "Fibre Channel Style 1 copper connector";
    case 0x03:
        return "Fibre Channel Style 2 copper connector";
    case 0x04:
        return "BNC/TNC (Bayonet/Threaded Neill-Concelman)";
    case 0x05:
        return "Fibre Channel coax headers";
    case 0x06:
        return "Fiber Jack";
    case 0x07:
        return "LC (Lucent Connector)";
    case 0x08:
        return "MT-RJ (Mechanical Transfer - Registered Jack)";
    case 0x09:
        return "MU (Multiple Optical)";
    case 0x0A:
        return "SG";
    case 0x0B:
        return "Optical Pigtail";
    case 0x0C:
        return "MPO 1x12 (Multifiber Parallel Optic)";
    case 0x0D:
        return "MPO 2x16";
    case 0x20:
        return "HSSDC II (High Speed Serial Data Connector)";
    case 0x21:
        return "Copper pigtail";
    case 0x22:
        return "RJ45";
    case 0x23:
        return "No separable connector";
    case 0x24:
        return "MXC 2x16";
    case 0x25:
        return "CS optical connector";
    case 0x26:
        return "SN (previously Mini CS) optical connector";
    case 0x27:
        return "MPO 2x12";
    case 0x28:
        return "MPO 1x16";
    /* 0x29 to 0x7F was Reserved need to be update in future.*/
    case 0x80 ... 0xFF:
        scnprintf(vendor_data,75,"vendor specific {%s}",vendor_name);
        return vendor_data;
    }
}

const char *mod_link_codes_to_str(unsigned short mod_link_codes)
{
    switch (mod_link_codes) {
    case 0x00:
    default:
        return "Unspecified";
    case 0x01:
        return "100G AOC or 25GAUI C2M AOC";
    case 0x02:
        return "100GBASE-SR4 or 25GBASE-SR";
    case 0x03:
        return "100GBASE-LR4 or 25GBASE-LR";
    case 0x04:
        return "100GBASE-ER4 or 25GBASE-ER";
    case 0x05:
        return "100GBASE-SR10";
    case 0x06:
        return "100G CWDM4";
    case 0x07:
        return "100G PSM4 Parallel SMF";
    case 0x08:
        return "100G ACC or 25GAUI C2M ACC";
    case 0x09:
        return "Obsolete (assigned before 100G CWDM4 MSA required FEC)";
    case 0x0B:
        return "100GBASE-CR4, 25GBASE-CR CA-25G-L or 50GBASE-CR2 with RS FEC";
    case 0x0C:
        return "25GBASE-CR CA-25G-S or 50GBASE-CR2 with BASE-R FEC";
    case 0x0D:
        return "25GBASE-CR CA-25G-N or 50GBASE-CR2 with no FEC";
    case 0x0E:
        return "10 Mb/s Single Pair Ethernet (802.3cg, 1000 m copper)";
    case 0x10:
        return "40GBASE-ER4";
    case 0x11:
        return "4 x 10GBASE-SR";
    case 0x12:
        return "40G PSM4 Parallel SMF";
    case 0x13:
        return "G959.1 profile P1I1-2D1 (10709 MBd, 2km, 1310 nm SM)";
    case 0x14:
        return "G959.1 profile P1S1-2D2 (10709 MBd, 40km, 1550 nm SM)";
    case 0x15:
        return "G959.1 profile P1L1-2D2 (10709 MBd, 80km, 1550 nm SM)";
    case 0x16:
        return "10GBASE-T with SFI electrical interface";
    case 0x17:
        return "100G CLR4";
    case 0x18:
        return "100G AOC or 25GAUI C2M AOC";
    case 0x19:
        return "100G ACC or 25GAUI C2M ACC";
    case 0x1A:
        return "100GE-DWDM2";
    case 0x1B:
        return "100G 1550nm WDM (4 wavelengths)";
    case 0x1C:
        return "10GBASE-T Short Reach (30 meters)";
    case 0x1D:
        return "5GBASE-T";
    case 0x1E:
        return "2.5GBASE-T";
    case 0x1F:
        return "40G SWDM4";
    case 0x20:
        return "100G SWDM4";
    case 0x21:
        return "100G PAM4 BiDi";
    case 0x22:
        return "4WDM-10 MSA";
    case 0x23:
        return "4WDM-20 MSA";
    case 0x24:
        return "4WDM-40 MSA";
    case 0x25:
        return "100GBASE-DR";
    case 0x26:
        return "100G-FR or 100GBASE-FR1";
    case 0x27:
        return "100G-LR or 100GBASE-LR1";
    case 0x28:
        return "100GBASE-SR";
    case 0x29:
        return "100GBASE-SR, 200GBASE-SR2 or 400GBASE-SR4";
    case 0x2A:
        return "100GBASE-FR1";
    case 0x2B:
        return "100GBASE-LR1";
    case 0x2C:
        return "100G-LR1-20 MSA, CAUI-4";
    case 0x2D:
        return "100G-ER1-30 MSA, CAUI-4";
    case 0x2E:
        return "100G-ER1-40 MSA, CAUI-4";
    case 0x2F:
        return "100G-LR1-20 MSA";
    case 0x30:
        return "Active Copper Cable with 50GAUI,BER of 10-6 or below";
    case 0x31:
        return "Active Optical Cable with 50GAUI,BER of 10-6 or below";
    case 0x32:
        return "Active Copper Cable, BER of 2.6x10-4 for ACC, 10-5 for AUI";
    case 0x33:
        return "Active Optical Cable, BER of 2.6x10-4 for ACC, 10-5 for AUI";
    case 0x34:
        return "100G-ER1-30 MSA";
    case 0x35:
        return "100G-ER1-40 MSA";
    case 0x36:
        return "100GBASE-VR, 200GBASE-VR2 or 400GBASE-VR4";
    case 0x37:
        return "10GBASE-BR";
    case 0x38:
        return "25GBASE-BR";
    case 0x39:
        return "50GBASE-BR";
    case 0x3A:
        return "100GBASE-VR";
    /* 0x3B-0x3E Reserved */
    case 0x3F:
        return "100GBASE-CR1, 200GBASE-CR2 or 400GBASE-CR4";
    case 0x40:
        return "50GBASE-CR, 100GBASE-CR2, or 200GBASE-CR4";
    case 0x41:
        return "50GBASE-SR, 100GBASE-SR2, or 200GBASE-SR4";
    case 0x42:
        return "50GBASE-FR or 200GBASE-DR4";
    case 0x4A:
        return "50GBASE-ER";
    case 0x43:
        return "200GBASE-FR4";
    case 0x44:
        return "200G 1550 nm PSM4";
    case 0x45:
        return "50GBASE-LR";
    case 0x46:
        return "200GBASE-LR4";
    case 0x47:
        return "400GBASE-DR4";
    case 0x48:
        return "400GBASE-FR4";
    case 0x49:
        return "400GBASE-LR4-6";
    case 0x4B:
        return "400G-LR4-10";
    case 0x4C:
        return "400GBASE-ZR";
    /*0x4D-0x7E Reserved was Reserved need to be update in future.*/
    case 0x7F:
        return "256GFC-SW4";
    case 0x80:
        return "64GFC";
    case 0x81:
        return "128GFC";
    /*0x82-0xFF Reserved was Reserved need to be update in future.*/
    }
}

char* calc_external_calib_temperature(struct qsfp* qsfp,
                               int16_t tempc, char* str)
{
    int ret = 0;
    int16_t temp_ad = 0;
    int32_t temp_calib = 0;
    u16 slope = 0;
    int16_t offset = 0;

    struct sff8472_temp_diag temp_ext_cal = {0};

    /* Convert tempc info to cpu byte order */
    temp_ad = be16_to_cpu(tempc & 0XFFFF);

    ret = qsfp_read(qsfp, SFF8472_TEMP_EXT, &temp_ext_cal,
                        sizeof(temp_ext_cal));
    if (ret < 0) {
        scnprintf(str,75,"QSFP read error: %d", ret);
        return str;
    }

    slope = be16_to_cpu(temp_ext_cal.cal_t_slope);
    offset = be16_to_cpu(temp_ext_cal.cal_t_offset);

    temp_calib = (temp_ad * slope) / 256;
    temp_calib = temp_calib + offset;

    /* (temp_calib * 1000) to get the three digit precision
       while converting it to celsius. */
    temp_calib = temp_calib * 1000;

    /* Convert the temp_calib to degrees Celsius by dividing it by 256. */
    temp_calib = temp_calib/256;

    /* To avoid printing of the minus(-) symbol in decimal places.
       Multiply temp with -1 for negative values. */
    scnprintf(str,75,"Temperature: %d.%03d °C",temp_calib/1000,
                    (temp_calib < 0 ? (temp_calib * -1) : temp_calib)%1000);
    return str;
}

char* calc_common_temperature(int16_t tempc, char* str)
{
    int16_t temp_in = 0;
    int32_t temp = 0;

    /* Convert tempc info to cpu byte order */
    temp_in = be16_to_cpu(tempc & 0XFFFF);

    /* (temp_in * 1000) to get the three digit precision
       while converting it to celsius. */
    temp = temp_in * 1000;

    /* Convert the temp to degrees Celsius by dividing it by 256. */
    temp = temp/256;

    /* To avoid printing of the minus(-) symbol in decimal places.
       Multiply temp with -1 for negative values. */
    scnprintf(str,75,"Temperature: %d.%03d °C",temp/1000,
                       (temp < 0 ? (temp * -1) : temp)%1000);
    return str;
}

char* calc_external_calib_svoltage(struct qsfp* qsfp, u16 sv_ad, char* str)
{
    u16 supply_voltage_ad = 0;
    u32 supply_voltage_t = 0;
    int ret = 0;
    u16 slope = 0;
    int16_t offset = 0;

    struct sff8472_vcc_diag vcc_ext_cal = {0};

    ret = qsfp_read(qsfp, SFF8472_VCC_EXT, &vcc_ext_cal,
                        sizeof(vcc_ext_cal));
    if (ret < 0) {
        scnprintf(str,75,"QSFP read error: %d", ret);
        return str;
    }

    /* convert sv_ad info to cpu byte order */
    supply_voltage_ad = be16_to_cpu(sv_ad & 0XFFFF);

    slope = be16_to_cpu(vcc_ext_cal.cal_v_slope);
    offset = be16_to_cpu(vcc_ext_cal.cal_v_offset);

    supply_voltage_t = (supply_voltage_ad * slope) / 256;
    supply_voltage_t = supply_voltage_t + offset;

    /* supply voltage in volts (supply_voltage_t * 100 μV/ 1000000) */
    scnprintf(str,75,"Supply Voltage: %d.%03d V",
         supply_voltage_t/10000, supply_voltage_t%10000);

    return str;
}

char* calc_common_svoltage(u16 svolt, char* str)
{
    u16 supply_voltage_t = 0;

    /* convert svolt info to cpu byte order */
    supply_voltage_t = be16_to_cpu(svolt & 0XFFFF);

    /* supply voltage in volts (supply_voltage_t * 100 μV/ 1000000) */
    scnprintf(str,75,"Supply Voltage: %d.%03d V",
         supply_voltage_t/10000, supply_voltage_t%10000);

    return str;
}

char* calc_external_calib_txi(struct qsfp* qsfp, u16 txi_ad, char* str)
{
    u32 txi_t = 0;
    int ret = 0;
    u16 slope = 0;
    int16_t offset = 0;

    struct sff8472_txi_diag txi_ext_cal = {0};

    ret = qsfp_read(qsfp, SFF8472_TXI_EXT, &txi_ext_cal,
                                   sizeof(txi_ext_cal));
    if (ret < 0) {
        scnprintf(str,500,"QSFP read error: %d", ret);
        return str;
    }

    slope = be16_to_cpu(txi_ext_cal.cal_txi_slope);
    offset = be16_to_cpu(txi_ext_cal.cal_txi_offset);

    txi_t = (txi_ad * slope) / 256;
    txi_t = txi_t + offset;

    /* txi_t in  Micro Amp */
    txi_t = txi_t * 2;

    scnprintf(str,500,"Tx Bias Current Lane1: %d.%03d"
                        " mA",txi_t/1000, txi_t%1000);

    return str;
}

char* calc_external_calib_txpwr(struct qsfp* qsfp, u16 txpwr_ad, char* str)
{
    u32 txpwr_t = 0;
    int ret = 0;
    u16 slope = 0;
    int16_t offset = 0;

    struct sff8472_txpwr_diag txpwr_ext_cal = {0};

    ret = qsfp_read(qsfp, SFF8472_TXPWR_EXT, &txpwr_ext_cal,
                                     sizeof(txpwr_ext_cal));
    if (ret < 0) {
        scnprintf(str,500,"QSFP read error: %d", ret);
        return str;
    }

    slope = be16_to_cpu(txpwr_ext_cal.cal_txpwr_slope);
    offset = be16_to_cpu(txpwr_ext_cal.cal_txpwr_offset);

    txpwr_t = (txpwr_ad * slope) / 256;
    txpwr_t = txpwr_t + offset;

    /* tx power in milliWatts (txpwr_t * 0.1 μW/ 1000) */
    scnprintf(str,500,"Tx Power Lane1: %d.%03d mW",
                     txpwr_t/10000, txpwr_t%10000);

    return str;
}

inline void spec_info_print(struct seq_file *s, u8 spec_id)
{
    if (spec_id == 0x00) {
        seq_printf(s,"Specification Identifier {0x%02X}\n"
                      "Unknown module\n",spec_id);
    }
    else {
        seq_printf(s,"Specification Identifier {0x%02X}\n"
                      "%s Transceiver module is not supported\n",
                      spec_id,mod_identifier_to_str(spec_id));
    }
}

void transceiver_debugfs_init(void)
{
    transceiver_debugfs_dir = debugfs_create_dir("transceiver_module", NULL);
    if (!transceiver_debugfs_dir || IS_ERR(transceiver_debugfs_dir)) {
        TRX_LOG_ERR_NODEV("debugfs_create_dir fail, error (%ld)",
                        PTR_ERR(transceiver_debugfs_dir));
       transceiver_debugfs_dir = NULL;
       return;
    }
}

void transceiver_debugfs_exit(void)
{
    if (!transceiver_debugfs_dir || IS_ERR(transceiver_debugfs_dir)) {
        TRX_LOG_ERR_NODEV("debugfs_create_dir fail, error (%ld)",
                        PTR_ERR(transceiver_debugfs_dir));
        transceiver_debugfs_dir = NULL;
        return;
    }
    debugfs_remove_recursive(transceiver_debugfs_dir);
    transceiver_debugfs_dir = NULL;
}

static int fpc_debug_i2c_address_show(struct seq_file *s, void *data)
{
    struct fpc *fpc = s->private;

    seq_printf(s, "0x%X\n",fpc->i2c_address);
    return 0;
}
DEFINE_SHOW_ATTRIBUTE(fpc_debug_i2c_address);

static int fpc_debug_i2c_adapter_show(struct seq_file *s, void *data)
{
    struct fpc *fpc = s->private;

    seq_printf(s, "i2c-%d\n",fpc->i2c->nr);
    return 0;
}
DEFINE_SHOW_ATTRIBUTE(fpc_debug_i2c_adapter);


void fpc_debugfs_init(struct fpc *fpc)
{
    struct dentry *file = NULL;
    char fpc_devname[20] = {};
    char fpc_devsubname[10] = {};
    strlcpy(fpc_devname,dev_name(fpc->dev),sizeof(fpc_devname));

    if (!transceiver_debugfs_dir || IS_ERR(transceiver_debugfs_dir)) {
        TRX_LOG_ERR(fpc, "transceiver debugfs_create_dir fail, error %ld",
                       PTR_ERR(transceiver_debugfs_dir));
        return;
    }

    if (strncmp(fpc_devname,"soc:",4) == 0 ) {
        strlcpy(fpc_devsubname,&fpc_devname[4],sizeof(fpc_devsubname));
    }
    else {
        strlcpy(fpc_devsubname, fpc_devname,sizeof(fpc_devsubname));
    }

    TRX_LOG_INFO(fpc, "dev %s , sub %s", dev_name(fpc->dev), fpc_devsubname);

    fpc->debugfs_dir = debugfs_create_dir(fpc_devsubname,
                             transceiver_debugfs_dir);
    if (!fpc->debugfs_dir || IS_ERR(fpc->debugfs_dir)) {
        TRX_LOG_ERR(fpc, "fpc debugfs_create_dir fail, error %ld",
                       PTR_ERR(fpc->debugfs_dir));
        fpc->debugfs_dir = NULL;
        return;
    }

    file = debugfs_create_file("i2c_address", 0600, fpc->debugfs_dir, fpc,
                        &fpc_debug_i2c_address_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(fpc, "fpc i2c address debugfs_create_file fail,"
                       " error %ld", PTR_ERR(file));
        debugfs_remove_recursive(fpc->debugfs_dir);
        fpc->debugfs_dir = NULL;
        return;
    }

    file = debugfs_create_file("i2c_adapter", 0600, fpc->debugfs_dir, fpc,
                        &fpc_debug_i2c_adapter_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(fpc, "fpc i2c adapter debugfs_create_file fail,"
                       " error %ld", PTR_ERR(file));
        debugfs_remove_recursive(fpc->debugfs_dir);
        fpc->debugfs_dir = NULL;
    }
}

void fpc_debugfs_exit(struct fpc *fpc)
{
    if(!fpc->debugfs_dir || !transceiver_debugfs_dir)
    {
        TRX_LOG_ERR(fpc, "FPC debugfs create dir fail\n");
        fpc->debugfs_dir = NULL;
        return;
    }

    if (IS_ERR(fpc->debugfs_dir) || IS_ERR(transceiver_debugfs_dir)) {
        TRX_LOG_ERR(fpc, "debugfs_create_dir fail, error (%ld %ld)",
                       PTR_ERR(transceiver_debugfs_dir),
                       PTR_ERR(fpc->debugfs_dir));
        fpc->debugfs_dir = NULL;
        return;
    }
    debugfs_remove_recursive(fpc->debugfs_dir);
    fpc->debugfs_dir = NULL;
}

static int qsfp_debug_qsfp_state_info_show(struct seq_file *s, void *data)
{
    u8 i;
    struct qsfp *qsfp = s->private;
    struct lane *lanei;

    /* Locking necessary to make sure all states fetched once */
    mutex_lock(&qsfp->sm_mutex);

    seq_printf(s, "State description: [Module state , Upstream Device state  ,"
                  " Link state]\n");
    seq_printf(s, "Transceiver port-%u State: [%s  %s  %s]\n", qsfp->port_num,
    mod_state_to_str(qsfp->sm_mod_state), dev_state_to_str(qsfp->sm_dev_state),
    link_state_to_str(qsfp->sm_link_state));

    if (qsfp->status.present) {
        char feature_str[QSFP_FEATURE_STR_MAX] = {0};
        qsfp_fill_features_str(qsfp, feature_str, sizeof(feature_str));

        seq_printf(s, "Module present: Yes\n");
        seq_printf(s, "Module probe attempts: %d\n",
                   PROBE_RETRY - qsfp->sm_mod_tries);
        seq_printf(s, "RX LOS: %d\n", qsfp->status.rx_los);
        seq_printf(s, "TX Fault: %d\n", qsfp->status.tx_fault);
        seq_printf(s, "Poll status: %s\n", qsfp->need_poll ? "Yes" : "No");
        seq_printf(s, "Features: %s\n", feature_str);
    } else {
        seq_printf(s, "Module present: No\n");
    }

    for (i = 0 ; i < qsfp->num_lanes; i++) {
        lanei = qsfp->lane[i];

        seq_printf(s, "\nLane%u State: [%s  %s  %s]\n",  i,
                   mod_state_to_str(lanei->sm_mod_state),
                   dev_state_to_str(lanei->sm_dev_state),
                   link_state_to_str(lanei->sm_link_state));

        if (lanei->status.present) {
            seq_printf(s, "Lane present: Yes\n");
            seq_printf(s, "RX LOS: %d\n", lanei->status.rx_los);
            seq_printf(s, "TX Fault: %d\n", lanei->status.tx_fault);
            seq_printf(s, "TX Disable: %d\n", lanei->status.tx_disable);
        } else {
            seq_printf(s, "Lane present: No\n");
        }
    }

    if (qsfp->sim.remove) {
        seq_printf(s, "\nSimulation Remove: Yes\n");
    }

    if (qsfp->sim.flags) {
        seq_printf(s, "\nSimulation Flags: Yes\n");
    }

    mutex_unlock(&qsfp->sm_mutex);

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_qsfp_state_info);

static int qsfp_debug_qsfp_i2c_address_info_show(struct seq_file *s,
                               void *data)
{
    struct qsfp *qsfp = s->private;
    seq_printf(s, "I2C device0 Address: 0x%X\n",qsfp->i2c_address_dev0);
    seq_printf(s, "I2C device1 Address: 0x%X\n",qsfp->i2c_address_dev1);

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_qsfp_i2c_address_info);

static int qsfp_debug_qsfp_port_num_info_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    seq_printf(s, "0x%X\n",qsfp->port_num);

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_qsfp_port_num_info);

static int qsfp_debug_qsfp_flags_show(struct seq_file *s,
                                      void *data)
{
    struct qsfp *qsfp = s->private;
    struct qsfp_flags flags;

    flags = qsfp->flags;

    seq_printf(s,  "Temperature High Alarm: %s\nTemperature Low Alarm: %s\n"
    "Temperature High Warning: %s\nTemperature Low Warning: %s\n\nVoltage "
    "High Alarm: %s\nVoltage Low Alarm: %s\nVoltage High Warning: %s\nVoltage "
    "Low Warning: %s\n\nBelow are Lane flags (LSB for lane0 and MSB for "
    "lane7)\n\n0x%02X RX LOS\n0x%02X TX Fault\n0x%02X TX LOS\n\n0x%02X RX CDR "
    "LOL\n0x%02X TX CDR LOL\n0x%02X TX Adaptive EQ IN Fail\n\n0x%02X RX Power "
    "High Alarm\n0x%02X RX Power Low Alarm\n0x%02X RX Power High Warning\n"
    "0x%02X RX Power Low Warning\n\n0x%02X TX Power High Alarm\n0x%02X TX "
    "Power Low Alarm\n0x%02X TX Power High Warning\n0x%02X TX Power Low "
    "Warning\n\n0x%02X TX Bias High Alarm\n0x%02X TX Bias Low Alarm\n0x%02X TX"
    " Bias High Warning\n0x%02X TX Bias Low Warning\n\n",
    (flags.temp & QSFP_TEMP_HIGH_ALARM) ? "Yes" : "No",
    (flags.temp & QSFP_TEMP_LOW_ALARM) ? "Yes" : "No",
    (flags.temp & QSFP_TEMP_HIGH_WARN) ? "Yes" : "No",
    (flags.temp & QSFP_TEMP_LOW_WARN) ? "Yes" : "No",
    (flags.volt & QSFP_VOLT_HIGH_ALARM) ? "Yes" : "No",
    (flags.volt & QSFP_VOLT_LOW_ALARM) ? "Yes" : "No",
    (flags.volt & QSFP_VOLT_HIGH_WARN) ? "Yes" : "No",
    (flags.volt & QSFP_VOLT_LOW_WARN) ? "Yes" : "No",
    flags.rx_los, flags.tx_fault, flags.tx_los, flags.rx_cdr_lol,
    flags.tx_cdr_lol, flags.tx_adap_eq_in_fail, flags.rx_power_high_alarm,
    flags.rx_power_low_alarm, flags.rx_power_high_warn,
    flags.rx_power_low_warn, flags.tx_power_high_alarm,
    flags.tx_power_low_alarm, flags.tx_power_high_warn,
    flags.tx_power_low_warn, flags.tx_bias_high_alarm,
    flags.tx_bias_low_alarm, flags.tx_bias_high_warn,
    flags.tx_bias_low_warn);

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_qsfp_flags);

static int qsfp_debug_qsfp_module_identifier_info_show(struct seq_file *s,
                               void *data)
{
    struct qsfp *qsfp = s->private;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver inserted before processing
       showing module identifier type even module was not
       probed to handle un implemented module types       */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    seq_printf(s, "{0x%X} %s\n", *spec_id, mod_identifier_to_str(*spec_id));

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_qsfp_module_identifier_info);

static int qsfp_debug_revision_info_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    char revSpec[75];
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        seq_printf(s, "{0x%X} %s\n",qsfp->module_revision,
                      sff8472_mod_revision_to_str(qsfp->module_revision));
        break;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        seq_printf(s, "{0x%X} %s\n",qsfp->module_revision,
                      sff8636_mod_revision_to_str(qsfp->module_revision));
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        seq_printf(s, "{0x%X} %s\n",qsfp->module_revision,
                      cmis_revision_to_str(qsfp->module_revision, revSpec));
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_revision_info);

static int qsfp_debug_connector_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
    struct cmis_eeprom_id *cmis_id;
    struct sfp_eeprom_id *sff8472_id;
    u8 *spec_id = (u8*)&qsfp->id;
    char vendor_data[75] ={};

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;
        seq_printf(s, "{0x%X} %s\n",sff8472_id->base.connector,
            mod_connector_to_str(sff8472_id->base.connector,sff8472_id->base.vendor_name,
                                     vendor_data));
        break;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        seq_printf(s, "{0x%X} %s\n",id->base.connector,
            mod_connector_to_str(id->base.connector,id->base.vendor_name,
                                     vendor_data));
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;
        seq_printf(s, "{0x%X} %s\n",cmis_id->base.connector,
            mod_connector_to_str(cmis_id->base.connector,
                                 cmis_id->base.vendor_name,
                                 vendor_data));
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_connector);

static int qsfp_debug_vendor_info_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
    struct cmis_eeprom_id *cmis_id;
    struct sfp_eeprom_id *sff8472_id;
    u8 *spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;
        seq_printf(s, "vendor name: %.*s\n",
                      (int)sizeof(sff8472_id->base.vendor_name),
                      sff8472_id->base.vendor_name);
        seq_printf(s, "vendor pn: %.*s\n",
                      (int)sizeof(sff8472_id->base.vendor_pn),
                      sff8472_id->base.vendor_pn);
        seq_printf(s, "vendor rev: %.*s\n",
                      (int)sizeof(sff8472_id->base.vendor_rev),
                      sff8472_id->base.vendor_rev);
        seq_printf(s, "vendor sn: %.*s\n",
                      (int)sizeof(sff8472_id->ext.vendor_sn),
                      sff8472_id->ext.vendor_sn);
        break;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        seq_printf(s, "vendor name: %.*s\n",
                      (int)sizeof(id->base.vendor_name),
                      id->base.vendor_name);
        seq_printf(s, "vendor pn: %.*s\n",
                      (int)sizeof(id->base.vendor_pn),
                      id->base.vendor_pn);
        seq_printf(s, "vendor rev: %.*s\n",
                      (int)sizeof(id->base.vendor_rev),
                      id->base.vendor_rev);
        seq_printf(s, "vendor sn: %.*s\n",
                      (int)sizeof(id->ext.vendor_sn),
                      id->ext.vendor_sn);
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;
        seq_printf(s, "vendor name: %.*s\n",
                      (int)sizeof(cmis_id->base.vendor_name),
                      cmis_id->base.vendor_name);
        seq_printf(s, "vendor pn: %.*s\n",
                      (int)sizeof(cmis_id->base.vendor_pn),
                      cmis_id->base.vendor_pn);
        seq_printf(s, "vendor rev: %.*s\n",
                      (int)sizeof(cmis_id->base.vendor_rev),
                      cmis_id->base.vendor_rev);
        seq_printf(s, "vendor sn: %.*s\n",
                      (int)sizeof(cmis_id->base.vendor_sn),
                      cmis_id->base.vendor_sn);
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_vendor_info);

/*
* Information about the Qsfp device temperature is being exported to Sysfs.
*/
static int qsfp_debug_device_temperature_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct cmis_eeprom_id *cmis_id;
    struct sfp_eeprom_id *id;
    u8 *spec_id = (u8*)&qsfp->id;
    char temperature_data[75] = {0};
    int16_t tempc = 0;
    int ret;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        id = &qsfp->id.sff8472;
        /* Check for DDM support, Address A0h, Byte 92 Bit 6 */
        if(id->ext.diagmon & SFF8472_DIAGMON_DDM)
        {
            /* Address A2h, Bytes 96-97 */
            ret = qsfp_read(qsfp, SFF8472_TEMP, &tempc,
                                    sizeof(tempc));
            if (ret < 0) {
                seq_printf(s, "QSFP read error: %d\n", ret);
                return 0;
            }

            /* Check for Internal calibration for DDM supported SFP,
             * Address A0h, Byte 92 Bit 5.
             */
            if(id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
            {
                seq_printf(s, "%s\n", calc_common_temperature(tempc,
                                                 temperature_data));
            }
            else /* SFP supported External calibration */
            {
                seq_printf(s, "%s\n", calc_external_calib_temperature(qsfp,
                                                 tempc, temperature_data));
            }
        }
        else
        {
            seq_printf(s, "TRX temperature measurement not supported  on "
                                        "non-DDM transceiver devices.\n");
        }
        break;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h Bytes 22-23 */
        ret = qsfp_read(qsfp, SFF8636_TEMPERATURE, &tempc,
                               sizeof(tempc));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        seq_printf(s, "%s\n", calc_common_temperature(tempc,
                                         temperature_data));
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            seq_printf(s, "TRX temperature measurement not supported\n");
            return 0;
        }

        /*Support advertised in page 01h:159.0 */
        if (!cmis_id->ext.temp_mon_sup) {
            seq_printf(s, "TRX temperature measurement not supported\n");
            return 0;
        }

        /* Page 00h Bytes 14-15 */
        ret = qsfp_read(qsfp, CMIS_MOD_TEMPMON, &tempc,
                               sizeof(tempc));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        seq_printf(s, "%s\n", calc_common_temperature(tempc,
                                         temperature_data));
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_temperature);

/*
* Information about the Qsfp device supply voltage is being exported to Sysfs.
*/
static int qsfp_debug_device_Supply_Voltage_show(struct seq_file *s,
                                                    void *data)
{
    struct qsfp *qsfp = s->private;
    struct cmis_eeprom_id *cmis_id;
	struct sfp_eeprom_id *id;
    u8 *spec_id = (u8*)&qsfp->id;
    char voltage_data[75] = {};
    u16 supply_voltage_t = 0;
    int ret;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        id = &qsfp->id.sff8472;
        /* Check for DDM support, Address A0h, Byte 92 Bit 6 */
        if(id->ext.diagmon & SFF8472_DIAGMON_DDM)
        {
            /* Address A2h, Bytes 98-99 */
            ret = qsfp_read(qsfp, SFF8472_VCC, &supply_voltage_t,
                                   sizeof(supply_voltage_t));
            if (ret < 0) {
                seq_printf(s, "QSFP read error: %d\n", ret);
                return 0;
            }

            /* Check for Internal calibration for DDM supported SFP,
             * Address A0h, Byte 92 Bit 5.
             */
            if(id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
            {
                seq_printf(s, "%s\n", calc_common_svoltage(supply_voltage_t,
                                                             voltage_data));
            }
            else /* SFP supported External calibration */
            {
                seq_printf(s, "%s\n", calc_external_calib_svoltage(qsfp,
                                       supply_voltage_t, voltage_data));
            }
        }
        else
        {
            seq_printf(s, "TRX supply voltage measurement not supported on"
                                        " non-DDM transceiver devices.\n");
        }
        break;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h Bytes 26-27 */
        ret = qsfp_read(qsfp, SFF8636_SUPPLY_VOLTAGE, &supply_voltage_t,
                              sizeof(supply_voltage_t));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        seq_printf(s, "%s\n", calc_common_svoltage(supply_voltage_t,
                                                     voltage_data));
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            seq_printf(s, "TRX supply voltage measurement not supported\n");
            return 0;
        }

        /*Support advertised in page 01h:159.1 */
        if (!cmis_id->ext.volt_mon_sup) {
            seq_printf(s, "TRX supply voltage measurement not supported\n");
            return 0;
        }

        /* Page 00h Bytes 16-17 */
        ret = qsfp_read(qsfp, CMIS_MOD_VCCMON, &supply_voltage_t,
                              sizeof(supply_voltage_t));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        seq_printf(s, "%s\n", calc_common_svoltage(supply_voltage_t,
                                                     voltage_data));

        break;
    default:
        spec_info_print(s, *spec_id);
    }
    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_Supply_Voltage);

/*
* Information about the Qsfp Channel Monitor value of Rx Power
* is being exported to Sysfs.
*/
static int qsfp_debug_device_rx_power_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct cmis_eeprom_id *cmis_id;
    struct sfp_eeprom_id *sff8472_id;
    u8 *spec_id = (u8*)&qsfp->id;
    char rx_power_data[500] ={};
    u16 rx_power_t[4] = {};
    u8  rx_power[8] = {0};
    u16 cmis_rx_power_t[8] = {0};
    u8  cmis_rx_power[16] = {0};
    u8 sfp_rx_power[2] = {0};
    int ret;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;
        /* Check for DDM support, Address A0h, Byte 92 Bit 6 */
        if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_DDM)
        {
            /* Address A2h, Bytes 104-105 */
            ret = qsfp_read(qsfp, SFF8472_RX_POWER, sfp_rx_power,
                                           sizeof(sfp_rx_power));
            if (ret < 0) {
                seq_printf(s, "QSFP read error: %d\n", ret);
                return 0;
            }

            /* Check for Internal calibration for DDM supported SFP,
             * Address A0h, Byte 92 Bit 5.
             */
            if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
            {
                rx_power_t[0] = (( sfp_rx_power[0] << 8) | sfp_rx_power[1]);
                /* rx power in milliWatts (rx_power_t * 0.1 μW/ 1000) */
                scnprintf(rx_power_data,500,"Rx Power Lane1: %d.%03d mW",
                          rx_power_t[0]/10000,rx_power_t[0]%10000);
                seq_printf(s, "%s\n", rx_power_data);
            }
            else /* SFP supported External calibration */
            {
                /* The procedure to calculate Rx power in the case of
                 * external calibration was not clear, and what Rx_PWR_ADe4-1
                 * signifies was not clear. We need to revisit this later.
                 */
                seq_printf(s, "TRX optical rx power measurement not"
                              " supported for external calibration type.\n");
            }
        }
        else
        {
            seq_printf(s, "TRX optical rx power measurement not supported  on "
                                             "non-DDM transceiver devices.\n");
        }
        break;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h Bytes 34-41 */
        ret = qsfp_read(qsfp, SFF8636_RX_POWER, rx_power,
                              sizeof(rx_power));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        rx_power_t[0] = (( rx_power[0] << 8) | rx_power[1]);
        rx_power_t[1] = (( rx_power[2] << 8) | rx_power[3]);
        rx_power_t[2] = (( rx_power[4] << 8) | rx_power[5]);
        rx_power_t[3] = (( rx_power[6] << 8) | rx_power[7]);

        /* rx power in milliWatts (rx_power_t * 0.1 μW/ 1000) */
        scnprintf(rx_power_data,500,"Rx Power Lane1: %d.%03d mW\n"
                                    "Rx Power Lane2: %d.%03d mW\n"
                                    "Rx Power Lane3: %d.%03d mW\n"
                                    "Rx Power Lane4: %d.%03d mW",
                          rx_power_t[0]/10000,rx_power_t[0]%10000,
                          rx_power_t[1]/10000,rx_power_t[1]%10000,
                          rx_power_t[2]/10000,rx_power_t[2]%10000,
                          rx_power_t[3]/10000,rx_power_t[3]%10000);
        seq_printf(s, "%s\n", rx_power_data);
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            seq_printf(s, "TRX optical rx power measurement not supported\n");
            return 0;
        }

        /*Support advertised in page 01h:160.2 */
        if(!cmis_id->ext.rx_optical_pow_mon_sup ) {
            seq_printf(s, "TRX optical rx power measurement not supported\n");
            return 0;
        }

        /* Page 11h Bytes 186-201 */
        ret = qsfp_read(qsfp, CMIS_RX_POWER, cmis_rx_power,
                              sizeof(cmis_rx_power));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        cmis_rx_power_t[0] = (( cmis_rx_power[0] << 8) | cmis_rx_power[1]);
        cmis_rx_power_t[1] = (( cmis_rx_power[2] << 8) | cmis_rx_power[3]);
        cmis_rx_power_t[2] = (( cmis_rx_power[4] << 8) | cmis_rx_power[5]);
        cmis_rx_power_t[3] = (( cmis_rx_power[6] << 8) | cmis_rx_power[7]);
        cmis_rx_power_t[4] = (( cmis_rx_power[8] << 8) | cmis_rx_power[9]);
        cmis_rx_power_t[5] = (( cmis_rx_power[10] << 8) | cmis_rx_power[11]);
        cmis_rx_power_t[6] = (( cmis_rx_power[12] << 8) | cmis_rx_power[13]);
        cmis_rx_power_t[7] = (( cmis_rx_power[14] << 8) | cmis_rx_power[15]);

        /* rx power in milliWatts (rx_power_t * 0.1 μW/ 1000) */
        scnprintf(rx_power_data,500,"Rx Power Lane1: %d.%03d mW\n"
                                    "Rx Power Lane2: %d.%03d mW\n"
                                    "Rx Power Lane3: %d.%03d mW\n"
                                    "Rx Power Lane4: %d.%03d mW\n"
                                    "Rx Power Lane5: %d.%03d mW\n"
                                    "Rx Power Lane6: %d.%03d mW\n"
                                    "Rx Power Lane7: %d.%03d mW\n"
                                    "Rx Power Lane8: %d.%03d mW",
                          cmis_rx_power_t[0]/10000,cmis_rx_power_t[0]%10000,
                          cmis_rx_power_t[1]/10000,cmis_rx_power_t[1]%10000,
                          cmis_rx_power_t[2]/10000,cmis_rx_power_t[2]%10000,
                          cmis_rx_power_t[3]/10000,cmis_rx_power_t[3]%10000,
                          cmis_rx_power_t[4]/10000,cmis_rx_power_t[4]%10000,
                          cmis_rx_power_t[5]/10000,cmis_rx_power_t[5]%10000,
                          cmis_rx_power_t[6]/10000,cmis_rx_power_t[6]%10000,
                          cmis_rx_power_t[7]/10000,cmis_rx_power_t[7]%10000);
        seq_printf(s, "%s\n", rx_power_data);
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_rx_power);

/*
* Information about the Qsfp Channel Monitor value of Tx Bias Current
* is being exported to Sysfs.
*/
static int qsfp_debug_device_tx_bias_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct cmis_eeprom_id *cmis_id;
    struct sfp_eeprom_id *sff8472_id;
    u8 *spec_id = (u8*)&qsfp->id;
    char tx_bias_current_data[500] = {};
    u32 tx_bias_current_t[4] = {};
    u16 tx_bias_current = 0;
    u8 tx_bias[8] = {0};
    u8 cmis_tx_bias[16] = {0};
    u32 cmis_tx_bias_current_t[8] = {0};
    u8 sfp_tx_bias[2] = {0};
    u8 cmis_tx_bias_multiplier = 1;
    int ret;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;
        /* Check for DDM support, Address A0h, Byte 92 Bit 6 */
        if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_DDM)
        {
            /* Address A2h, Bytes 100-101 */
            ret = qsfp_read(qsfp, SFF8472_TX_BIAS, sfp_tx_bias,
                                      sizeof(sfp_tx_bias));
            if (ret < 0) {
                seq_printf(s, "QSFP read error: %d\n", ret);
                return 0;
            }

            tx_bias_current = (( sfp_tx_bias[0] << 8) | sfp_tx_bias[1]);

            /* Check for Internal calibration for DDM supported SFP,
             * Address A0h, Byte 92 Bit 5.
             */
            if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
            {
                /* tx_bias_current in  Micro Amp */
                tx_bias_current_t[0] = tx_bias_current * 2;

                scnprintf(tx_bias_current_data,500,"Tx Bias Current"
                                               " Lane1: %d.%03d mA",
                                          tx_bias_current_t[0]/1000,
                                         tx_bias_current_t[0]%1000);
                seq_printf(s, "%s\n", tx_bias_current_data);
            }
            else /* SFP supported External calibration */
            {
              seq_printf(s, "%s\n",calc_external_calib_txi(qsfp,
                                                tx_bias_current,
                                         tx_bias_current_data));
            }
        }
        else
        {
            seq_printf(s, "TRX tx bias current measurement not supported on "
                                           "non-DDM transceiver devices.\n");
        }
        break;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h Bytes 42-49 */
        ret = qsfp_read(qsfp, SFF8636_TX_BIAS, tx_bias,
                              sizeof(tx_bias));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }
        tx_bias_current = (( tx_bias[0] << 8) | tx_bias[1]);
        /* tx_bias_current in  Micro Amp */
        tx_bias_current_t[0] = tx_bias_current * 2;

        tx_bias_current = (( tx_bias[2] << 8) | tx_bias[3]);
        /* tx_bias_current in  Micro Amp */
        tx_bias_current_t[1] = tx_bias_current * 2;

        tx_bias_current = (( tx_bias[4] << 8) | tx_bias[5]);
        /* tx_bias_current in  Micro Amp */
        tx_bias_current_t[2] = tx_bias_current * 2;

        tx_bias_current = (( tx_bias[6] << 8) | tx_bias[7]);
        /* tx_bias_current in  Micro Amp */
        tx_bias_current_t[3] = tx_bias_current * 2;

        scnprintf(tx_bias_current_data,500,"Tx Bias Current Lane1: %d.%03d"
                                           " mA\n"
                                        "Tx Bias Current Lane2: %d.%03d mA\n"
                                        "Tx Bias Current Lane3: %d.%03d mA\n"
                                        "Tx Bias Current Lane4: %d.%03d mA",
                          tx_bias_current_t[0]/1000,tx_bias_current_t[0]%1000,
                          tx_bias_current_t[1]/1000,tx_bias_current_t[1]%1000,
                          tx_bias_current_t[2]/1000,tx_bias_current_t[2]%1000,
                          tx_bias_current_t[3]/1000,tx_bias_current_t[3]%1000);
        seq_printf(s, "%s\n", tx_bias_current_data);
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            seq_printf(s, "TRX tx bias current measurement not supported\n");
            return 0;
        }

        /*Support advertised in page 01h:160.0 */
        if(!cmis_id->ext.tx_bias_mon_sup ) {
            seq_printf(s, "TRX tx bias current measurement not supported\n");
            return 0;
        }

        switch(cmis_id->ext.tx_bias_cur_scal) {
        case 0x00:
            /* multiply x1 */
            cmis_tx_bias_multiplier = 1;
            break;
        case 0x01:
            /* multiply x2 */
            cmis_tx_bias_multiplier = 2;
            break;
        case 0x02:
            /* multiply x4 */
            cmis_tx_bias_multiplier = 4;
            break;
        /* reserved case not expected from OIF-CMIS-05.2 Rev */
        case 0x03:
            seq_printf(s, "TRX tx bias current multiplier was reserved "
                          "not expected for OIF-CMIS-05.2\n");
            return 0;
        }

        /* Page 11h Bytes 170-185 */
        ret = qsfp_read(qsfp, CMIS_TX_BIAS, cmis_tx_bias,
                              sizeof(cmis_tx_bias));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }
        tx_bias_current = (( cmis_tx_bias[0] << 8) | cmis_tx_bias[1]);
        /* tx_bias_current in  Micro Amp */
        cmis_tx_bias_current_t[0] = tx_bias_current * 2
                                     * cmis_tx_bias_multiplier;

        tx_bias_current = (( cmis_tx_bias[2] << 8) | cmis_tx_bias[3]);
        /* tx_bias_current in  Micro Amp */
        cmis_tx_bias_current_t[1] = tx_bias_current * 2
                                      * cmis_tx_bias_multiplier;

        tx_bias_current = (( cmis_tx_bias[4] << 8) | cmis_tx_bias[5]);
        /* tx_bias_current in  Micro Amp */
        cmis_tx_bias_current_t[2] = tx_bias_current * 2
                                      * cmis_tx_bias_multiplier;

        tx_bias_current = (( cmis_tx_bias[6] << 8) | cmis_tx_bias[7]);
        /* tx_bias_current in  Micro Amp */
        cmis_tx_bias_current_t[3] = tx_bias_current * 2
                                       * cmis_tx_bias_multiplier;

        tx_bias_current = (( cmis_tx_bias[8] << 8) | cmis_tx_bias[9]);
        /* tx_bias_current in  Micro Amp */
        cmis_tx_bias_current_t[4] = tx_bias_current * 2
                                       * cmis_tx_bias_multiplier;

        tx_bias_current = (( cmis_tx_bias[10] << 8) | cmis_tx_bias[11]);
        /* tx_bias_current in  Micro Amp */
        cmis_tx_bias_current_t[5] = tx_bias_current * 2
                                       * cmis_tx_bias_multiplier;

        tx_bias_current = (( cmis_tx_bias[12] << 8) | cmis_tx_bias[13]);
        /* tx_bias_current in  Micro Amp */
        cmis_tx_bias_current_t[6] = tx_bias_current * 2
                                       * cmis_tx_bias_multiplier;

        tx_bias_current = (( cmis_tx_bias[14] << 8) | cmis_tx_bias[15]);
        /* tx_bias_current in  Micro Amp */
        cmis_tx_bias_current_t[7] = tx_bias_current * 2
                                       * cmis_tx_bias_multiplier;


        scnprintf(tx_bias_current_data,500,"Tx Bias Current Lane1: %d.%03d"
                                           " mA\n"
                                        "Tx Bias Current Lane2: %d.%03d mA\n"
                                        "Tx Bias Current Lane3: %d.%03d mA\n"
                                        "Tx Bias Current Lane4: %d.%03d mA\n"
                                        "Tx Bias Current Lane5: %d.%03d mA\n"
                                        "Tx Bias Current Lane6: %d.%03d mA\n"
                                        "Tx Bias Current Lane7: %d.%03d mA\n"
                                        "Tx Bias Current Lane8: %d.%03d mA",
                cmis_tx_bias_current_t[0]/1000,cmis_tx_bias_current_t[0]%1000,
                cmis_tx_bias_current_t[1]/1000,cmis_tx_bias_current_t[1]%1000,
                cmis_tx_bias_current_t[2]/1000,cmis_tx_bias_current_t[2]%1000,
                cmis_tx_bias_current_t[3]/1000,cmis_tx_bias_current_t[3]%1000,
                cmis_tx_bias_current_t[4]/1000,cmis_tx_bias_current_t[4]%1000,
                cmis_tx_bias_current_t[5]/1000,cmis_tx_bias_current_t[5]%1000,
                cmis_tx_bias_current_t[6]/1000,cmis_tx_bias_current_t[6]%1000,
                cmis_tx_bias_current_t[7]/1000,cmis_tx_bias_current_t[7]%1000);
        seq_printf(s, "%s\n", tx_bias_current_data);
        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_tx_bias);

/*
* Information about the Qsfp Channel Monitor value of Tx Power
* is being exported to Sysfs.
*/
static int qsfp_debug_device_tx_power_show(struct seq_file *s, void *data)
{
    struct qsfp *qsfp = s->private;
    struct sff8636_eeprom_id *id;
    struct cmis_eeprom_id *cmis_id;
    struct sfp_eeprom_id *sff8472_id;
    u8 *spec_id = (u8*)&qsfp->id;
    char tx_power_data[500] = {0};
    u16 tx_power_t[4] = {0};
    u8  tx_power[8] = {0};
    u8  sfp_tx_power[2] = {0};
    u8 diagmon;
    u8  cmis_tx_power[16] = {0};
    u16 cmis_tx_power_t[8] = {0};
    int ret;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;
        /* Check for DDM support, Address A0h, Byte 92 Bit 6 */
        if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_DDM)
        {
            /* Address A2h, Bytes 102-103 */
            ret = qsfp_read(qsfp, SFF8472_TX_POWER, sfp_tx_power,
                                       sizeof(sfp_tx_power));
            if (ret < 0) {
                seq_printf(s, "QSFP read error: %d\n", ret);
                return 0;
            }

            tx_power_t[0] = ((sfp_tx_power[0] << 8) | sfp_tx_power[1]);

            /* Check for Internal calibration for DDM supported SFP,
             * Address A0h, Byte 92 Bit 5.
             */
            if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
            {
                /* tx power in milliWatts (tx_power_t * 0.1 μW/ 1000) */
                scnprintf(tx_power_data,500,"Tx Power Lane1: %d.%03d mW",
                        tx_power_t[0]/10000,tx_power_t[0]%10000);
                seq_printf(s, "%s\n", tx_power_data);
            }
            else /* SFP supported External calibration */
            {
                seq_printf(s, "%s\n",calc_external_calib_txpwr(qsfp,
                                                      tx_power_t[0],
                                                    tx_power_data));
            }
        }
        else
        {
            seq_printf(s, "Transmitter power measurement not supported on"
                                       " non-DDM transceiver devices.\n");
        }
        break;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        diagmon = id->ext.diagmon;
        if(!(diagmon & BIT(2))) {
            seq_printf(s, "Transmitter power measurement not supported\n");
            return 0;
        }
        /* Page 00h Bytes 50-57 */
        ret = qsfp_read(qsfp, SFF8636_TX_POWER, tx_power,
                              sizeof(tx_power));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        tx_power_t[0] = (( tx_power[0] << 8) | tx_power[1]);
        tx_power_t[1] = (( tx_power[2] << 8) | tx_power[3]);
        tx_power_t[2] = (( tx_power[4] << 8) | tx_power[5]);
        tx_power_t[3] = (( tx_power[6] << 8) | tx_power[7]);

        /* tx power in milliWatts (tx_power_t * 0.1 μW/ 1000) */
        scnprintf(tx_power_data,500,"Tx Power Lane1: %d.%03d mW\n"
                                    "Tx Power Lane2: %d.%03d mW\n"
                                    "Tx Power Lane3: %d.%03d mW\n"
                                    "Tx Power Lane4: %d.%03d mW",
                          tx_power_t[0]/10000,tx_power_t[0]%10000,
                          tx_power_t[1]/10000,tx_power_t[1]%10000,
                          tx_power_t[2]/10000,tx_power_t[2]%10000,
                          tx_power_t[3]/10000,tx_power_t[3]%10000);
        seq_printf(s, "%s\n", tx_power_data);
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            seq_printf(s, "Transmitter power measurement not supported\n");
            return 0;
        }

        /*Support advertised in page 01h:160.1 */
        if(!cmis_id->ext.tx_optical_pow_mon_sup) {
            seq_printf(s, "Transmitter power measurement not supported\n");
            return 0;
        }

        /* Page 11h Bytes 154-169 */
        ret = qsfp_read(qsfp, CMIS_TX_POWER, cmis_tx_power,
                              sizeof(cmis_tx_power));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        cmis_tx_power_t[0] = (( cmis_tx_power[0] << 8) | cmis_tx_power[1]);
        cmis_tx_power_t[1] = (( cmis_tx_power[2] << 8) | cmis_tx_power[3]);
        cmis_tx_power_t[2] = (( cmis_tx_power[4] << 8) | cmis_tx_power[5]);
        cmis_tx_power_t[3] = (( cmis_tx_power[6] << 8) | cmis_tx_power[7]);
        cmis_tx_power_t[4] = (( cmis_tx_power[9] << 8) | cmis_tx_power[9]);
        cmis_tx_power_t[5] = (( cmis_tx_power[10] << 8) | cmis_tx_power[11]);
        cmis_tx_power_t[6] = (( cmis_tx_power[12] << 8) | cmis_tx_power[13]);
        cmis_tx_power_t[7] = (( cmis_tx_power[14] << 8) | cmis_tx_power[15]);

        /* tx power in milliWatts (tx_power_t * 0.1 μW/ 1000) */
        scnprintf(tx_power_data,500,"Tx Power Lane1: %d.%03d mW\n"
                                    "Tx Power Lane2: %d.%03d mW\n"
                                    "Tx Power Lane3: %d.%03d mW\n"
                                    "Tx Power Lane4: %d.%03d mW\n"
                                    "Tx Power Lane5: %d.%03d mW\n"
                                    "Tx Power Lane6: %d.%03d mW\n"
                                    "Tx Power Lane7: %d.%03d mW\n"
                                    "Tx Power Lane8: %d.%03d mW",
                          cmis_tx_power_t[0]/10000,cmis_tx_power_t[0]%10000,
                          cmis_tx_power_t[1]/10000,cmis_tx_power_t[1]%10000,
                          cmis_tx_power_t[2]/10000,cmis_tx_power_t[2]%10000,
                          cmis_tx_power_t[3]/10000,cmis_tx_power_t[3]%10000,
                          cmis_tx_power_t[4]/10000,cmis_tx_power_t[4]%10000,
                          cmis_tx_power_t[5]/10000,cmis_tx_power_t[5]%10000,
                          cmis_tx_power_t[6]/10000,cmis_tx_power_t[6]%10000,
                          cmis_tx_power_t[7]/10000,cmis_tx_power_t[7]%10000);
        seq_printf(s, "%s\n", tx_power_data);

        break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_tx_power);

static long trx_calibrate_temp(__be16 tmp_val)
{
    long value = 0;

    value = be16_to_cpu(tmp_val);
    if (value >= 0x8000)
        value -= 0x10000;

    value = DIV_ROUND_CLOSEST(value , 256);

    return value;
}

static long trx_calibrate_vcc(__be16 vcc_val)
{
    long value = 0;

    value = be16_to_cpu(vcc_val);
    value = DIV_ROUND_CLOSEST(value, 10);

    return value;
}

static long trx_calibrate_power(__be16 power_val)
{
    long value = 0;

    value = be16_to_cpu(power_val);
    value = DIV_ROUND_CLOSEST(value, 10);

    return value;
}

static long trx_calibrate_txbias(__be16 bias_val)
{
    long value = 0;

    value = be16_to_cpu(bias_val);
    value = DIV_ROUND_CLOSEST(value, 500);

    return value;
}

static long trx_ext_temp_ddm(__be16 tmp_val, struct sff8472_temp_diag* temp_const)
{
    long temp_val = 0;
    int16_t temp_ad = 0;
    u16 slope = 0;
    int16_t offset = 0;

    temp_ad = be16_to_cpu(tmp_val & 0XFFFF);
    slope = be16_to_cpu(temp_const->cal_t_slope);
    offset = be16_to_cpu(temp_const->cal_t_offset);

    temp_val = DIV_ROUND_CLOSEST(temp_ad * slope, 256) + offset;

    if(temp_val >= 0x8000)
        temp_val -= 0x10000;

    /* Need to confirm this value */
    temp_val = DIV_ROUND_CLOSEST(temp_val, 256);
    return temp_val;
}

static long trx_ext_vcc_ddm(__be16 svcc_val, struct sff8472_vcc_diag* vcc_const)
{
    long vcc_val = 0;
    int16_t vcc_ad = 0;
    u16 slope = 0;
    int16_t offset = 0;

    vcc_ad = be16_to_cpu(svcc_val & 0XFFFF);
    slope = be16_to_cpu(vcc_const->cal_v_slope);
    offset = be16_to_cpu(vcc_const->cal_v_offset);

    vcc_val = DIV_ROUND_CLOSEST(vcc_ad * slope, 256) + offset;

    /* Need to confirm this value */
    vcc_val = DIV_ROUND_CLOSEST(vcc_val, 10);
    return vcc_val;
}

static long trx_ext_ddm_power(__be16 power_val, struct sff8472_txpwr_diag* txpwr_const)
{
    long txpwr_val = 0;
    int16_t txpwr_ad = 0;
    u16 slope = 0;
    int16_t offset = 0;

    txpwr_ad = be16_to_cpu(txpwr_val & 0XFFFF);
    slope = be16_to_cpu(txpwr_const->cal_txpwr_slope);
    offset = be16_to_cpu(txpwr_const->cal_txpwr_offset);

    txpwr_val = DIV_ROUND_CLOSEST(txpwr_ad * slope, 256) + offset;

    /* Need to confirm this value */
    txpwr_val = DIV_ROUND_CLOSEST(txpwr_val, 10);
    return txpwr_val;
}

static long trx_ext_ddm_txbias(__be16 tx_val, struct sff8472_txi_diag* txi_const)
{
    long txi_val = 0;
    int16_t txi_ad = 0;
    u16 slope = 0;
    int16_t offset = 0;

    txi_ad = be16_to_cpu(tx_val & 0XFFFF);
    slope = be16_to_cpu(txi_const->cal_txi_slope);
    offset = be16_to_cpu(txi_const->cal_txi_offset);

    txi_val = DIV_ROUND_CLOSEST(txi_ad * slope, 256) + offset;

    /* Need to confirm this value */
    txi_val = DIV_ROUND_CLOSEST(txi_val, 500);
    return txi_val;
}

static int calc_external_calib_ddm(struct seq_file *s,
                 struct sff8472_ddm_thresholds* ddm_limits)
{
    struct qsfp *qsfp = s->private;
    struct sff8472_temp_diag temp_ext_cal = {0};
    struct sff8472_vcc_diag vcc_ext_cal = {0};
    struct sff8472_txi_diag txi_ext_cal = {0};
    struct sff8472_txpwr_diag txpwr_ext_cal = {0};
    int ret = 0;

    ret = qsfp_read(qsfp, SFF8472_TEMP_EXT, &temp_ext_cal,
                        sizeof(temp_ext_cal));
    if (ret < 0) {
        seq_printf(s,"QSFP read error for temperature external calibration"
                                                " constants: %d\n\n", ret);
        return 0;
    }

    seq_printf(s, "************ temperature threshold limits ****"
                                                    "********\n");
    seq_printf(s, "temp_high_alarm: %d °C \ntemp_low_alarm : %d °C "
                "\ntemp_high_warn : %d °C \ntemp_low_warn  : %d °C \n\n",
            trx_ext_temp_ddm(ddm_limits->temp_high_alarm, &temp_ext_cal),
            trx_ext_temp_ddm(ddm_limits->temp_low_alarm, &temp_ext_cal),
            trx_ext_temp_ddm(ddm_limits->temp_high_warn, &temp_ext_cal),
            trx_ext_temp_ddm(ddm_limits->temp_low_warn, &temp_ext_cal));

    ret = qsfp_read(qsfp, SFF8472_VCC_EXT, &vcc_ext_cal,
                                   sizeof(vcc_ext_cal));
    if (ret < 0) {
        seq_printf(s,"QSFP read error for supply voltage external"
                          " calibration constants: %d\n\n", ret);
        return 0;
    }

    seq_printf(s, "********** supply voltage threshold limits ****"
                                                       "*******\n");
    seq_printf(s, "volt_high_alarm: %d mV \nvolt_low_alarm : %d mV \n"
               "volt_high_warn : %d mV \nvolt_low_warn  : %d mV \n\n",
              trx_ext_vcc_ddm(ddm_limits->volt_high_alarm, &vcc_ext_cal),
               trx_ext_vcc_ddm(ddm_limits->volt_low_alarm, &vcc_ext_cal),
               trx_ext_vcc_ddm(ddm_limits->volt_high_warn, &vcc_ext_cal),
               trx_ext_vcc_ddm(ddm_limits->volt_low_warn, &vcc_ext_cal));

    ret = qsfp_read(qsfp, SFF8472_TXPWR_EXT, &txpwr_ext_cal,
                                     sizeof(txpwr_ext_cal));
    if (ret < 0) {
        seq_printf(s,"QSFP read error for tx power external calibration"
                                             " constants: %d\n\n", ret);
        return 0;
    }

   seq_printf(s, "************* tx power threshold limits ********"
                                                       "******\n");
   seq_printf(s, "txpwr_high_alarm: %d µW \ntxpwr_low_alarm : %d µW \n"
                "txpwr_high_warn : %d µW \ntxpwr_low_warn: %d µW \n\n",
               trx_ext_ddm_power(ddm_limits->txpwr_high_alarm, &txpwr_ext_cal),
                trx_ext_ddm_power(ddm_limits->txpwr_low_alarm, &txpwr_ext_cal),
                trx_ext_ddm_power(ddm_limits->txpwr_high_warn, &txpwr_ext_cal),
                trx_ext_ddm_power(ddm_limits->txpwr_low_warn, &txpwr_ext_cal));

    ret = qsfp_read(qsfp, SFF8472_TXI_EXT, &txi_ext_cal,
                                   sizeof(txi_ext_cal));
    if (ret < 0) {
        seq_printf(s,"QSFP read error for tx bias current of external"
                               " calibration constants: %d\n\n", ret);
        return 0;
    }

    seq_printf(s, "************** tx bias threshold limits ********"
                                                        "******\n");
    seq_printf(s, "bias_high_alarm: %d mA \nbias_low_alarm : %d mA \n"
               "bias_high_warn : %d mA \nbias_low_warn  : %d mA \n\n",
                 trx_ext_ddm_txbias(ddm_limits->bias_high_alarm, &txi_ext_cal),
                  trx_ext_ddm_txbias(ddm_limits->bias_low_alarm, &txi_ext_cal),
                  trx_ext_ddm_txbias(ddm_limits->bias_high_warn, &txi_ext_cal),
                  trx_ext_ddm_txbias(ddm_limits->bias_low_warn, &txi_ext_cal));

    return 0;
}

static int qsfp_debug_device_ddm_thresholds_show(struct seq_file *s,
                                                         void *data)
{
    struct qsfp *qsfp = s->private;
    struct sfp_eeprom_id *sff8472_id;

    u8 *spec_id = (u8*)&qsfp->id;
    struct sff8472_ddm_thresholds ddm_limits = {0};
    struct sff8636_ddm_thresholds sff8636_ddm_limits = {0};
    struct cmis_thresholds cmis_ddm_limits = {0};
    int ret = 0;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }
    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;

        if(!(sff8472_id->ext.enhopts & SFP_ENHOPTS_ALARMWARN))
        {
            seq_printf(s, "TRX does not support alarm and warning "
                                              "threshold limits\n");
            return 0;
        }

        /* Address A2h, Bytes 0-39 */
        ret = qsfp_read(qsfp, SFF8472_DDM_TH, &ddm_limits,
                                     sizeof(ddm_limits));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_EXT_CAL)
        {
            return calc_external_calib_ddm(s,
                                   &ddm_limits);
        }
        else /* SFP supported Internal calibration */
        {
            seq_printf(s, "************ temperature threshold limits ****"
                                                            "********\n");
            seq_printf(s, "temp_high_alarm: %d °C \ntemp_low_alarm : %d °C "
                   "\ntemp_high_warn : %d °C \ntemp_low_warn  : %d °C \n\n",
                               trx_calibrate_temp(ddm_limits.temp_high_alarm),
                                trx_calibrate_temp(ddm_limits.temp_low_alarm),
                                trx_calibrate_temp(ddm_limits.temp_high_warn),
                                trx_calibrate_temp(ddm_limits.temp_low_warn));


            seq_printf(s, "********** supply voltage threshold limits ****"
                                                              "*******\n");
            seq_printf(s, "volt_high_alarm: %d mV \nvolt_low_alarm : %d mV \n"
                       "volt_high_warn : %d mV \nvolt_low_warn  : %d mV \n\n",
                                trx_calibrate_vcc(ddm_limits.volt_high_alarm),
                                 trx_calibrate_vcc(ddm_limits.volt_low_alarm),
                                 trx_calibrate_vcc(ddm_limits.volt_high_warn),
                                 trx_calibrate_vcc(ddm_limits.volt_low_warn));

            seq_printf(s, "************* tx power threshold limits ********"
                                                                "******\n");
            seq_printf(s, "txpwr_high_alarm: %d µW \ntxpwr_low_alarm :"
                          " %d µW \ntxpwr_high_warn : %d µW \n"
                          "txpwr_low_warn  : %d µW \n\n",
                            trx_calibrate_power(ddm_limits.txpwr_high_alarm),
                             trx_calibrate_power(ddm_limits.txpwr_low_alarm),
                             trx_calibrate_power(ddm_limits.txpwr_high_warn),
                             trx_calibrate_power(ddm_limits.txpwr_low_warn));

            seq_printf(s, "************* rx power threshold limits ********"
                                                                "******\n");
            seq_printf(s, "rxpwr_high_alarm: %d µW \nrxpwr_low_alarm :"
                          " %d µW \nrxpwr_high_warn : %d µW \n"
                          "rxpwr_low_warn  : %d µW \n\n",
                            trx_calibrate_power(ddm_limits.rxpwr_high_alarm),
                             trx_calibrate_power(ddm_limits.rxpwr_low_alarm),
                             trx_calibrate_power(ddm_limits.rxpwr_high_warn),
                             trx_calibrate_power(ddm_limits.rxpwr_low_warn));

            seq_printf(s, "************** tx bias threshold limits ********"
                                                                "******\n");
            seq_printf(s, "bias_high_alarm: %d mA \nbias_low_alarm : %d mA \n"
                       "bias_high_warn : %d mA \nbias_low_warn  : %d mA \n\n",
                             trx_calibrate_txbias(ddm_limits.bias_high_alarm),
                              trx_calibrate_txbias(ddm_limits.bias_low_alarm),
                              trx_calibrate_txbias(ddm_limits.bias_high_warn),
                              trx_calibrate_txbias(ddm_limits.bias_low_warn));
        }
        break;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h, Byte-2 Bit-2 */
        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            seq_printf(s, "TRX does not support alarm and warning "
                                              "threshold limits\n");
            return 0;
        }

        /* Page 03h Bytes 128-199 */
        ret = qsfp_read(qsfp, SFF8636_DDM_TH, &sff8636_ddm_limits,
                                     sizeof(sff8636_ddm_limits));
        if (ret < 0) {
            seq_printf(s, "QSFP read error: %d\n", ret);
            return 0;
        }

        seq_printf(s, "************ temperature threshold limits ****"
                                                         "********\n");
        seq_printf(s, "temp_high_alarm: %d °C \ntemp_low_alarm : %d °C "
                "\ntemp_high_warn : %d °C \ntemp_low_warn  : %d °C \n\n",
                      trx_calibrate_temp(sff8636_ddm_limits.temp_high_alarm),
                       trx_calibrate_temp(sff8636_ddm_limits.temp_low_alarm),
                       trx_calibrate_temp(sff8636_ddm_limits.temp_high_warn),
                       trx_calibrate_temp(sff8636_ddm_limits.temp_low_warn));

        seq_printf(s, "********** supply voltage threshold limits ****"
                                                          "*******\n");
        seq_printf(s, "volt_high_alarm: %d mV \nvolt_low_alarm : %d mV \n"
                      "volt_high_warn : %d mV \nvolt_low_warn  : %d mV \n\n",
                         trx_calibrate_vcc(sff8636_ddm_limits.volt_high_alarm),
                          trx_calibrate_vcc(sff8636_ddm_limits.volt_low_alarm),
                          trx_calibrate_vcc(sff8636_ddm_limits.volt_high_warn),
                          trx_calibrate_vcc(sff8636_ddm_limits.volt_low_warn));

        seq_printf(s, "************* tx power threshold limits ********"
                                                            "******\n");
        seq_printf(s, "txpwr_high_alarm: %d µW \ntxpwr_low_alarm : %d µW \n"
                    "txpwr_high_warn : %d µW \ntxpwr_low_warn  : %d µW \n\n",
                      trx_calibrate_power(sff8636_ddm_limits.txpwr_high_alarm),
                       trx_calibrate_power(sff8636_ddm_limits.txpwr_low_alarm),
                       trx_calibrate_power(sff8636_ddm_limits.txpwr_high_warn),
                       trx_calibrate_power(sff8636_ddm_limits.txpwr_low_warn));

        seq_printf(s, "************* rx power threshold limits ********"
                                                            "******\n");
        seq_printf(s, "rxpwr_high_alarm: %d µW \nrxpwr_low_alarm : %d µW \n"
                    "rxpwr_high_warn : %d µW \nrxpwr_low_warn  : %d µW \n\n",
                      trx_calibrate_power(sff8636_ddm_limits.rxpwr_high_alarm),
                       trx_calibrate_power(sff8636_ddm_limits.rxpwr_low_alarm),
                       trx_calibrate_power(sff8636_ddm_limits.rxpwr_high_warn),
                       trx_calibrate_power(sff8636_ddm_limits.rxpwr_low_warn));

        seq_printf(s, "************** tx bias threshold limits ********"
                                                            "******\n");
        seq_printf(s, "bias_high_alarm: %d mA \nbias_low_alarm : %d mA \n"
                    "bias_high_warn : %d mA \nbias_low_warn  : %d mA \n\n",
                      trx_calibrate_txbias(sff8636_ddm_limits.bias_high_alarm),
                       trx_calibrate_txbias(sff8636_ddm_limits.bias_low_alarm),
                       trx_calibrate_txbias(sff8636_ddm_limits.bias_high_warn),
                       trx_calibrate_txbias(sff8636_ddm_limits.bias_low_warn));
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        /* Page 00h, Byte-2 Bit-7 */
        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            seq_printf(s, "TRX does not support alarm and warning "
                                              "threshold limits\n");
            return 0;
        }

        /* Page 02h Bytes 128-199 */
        ret = qsfp_read(qsfp, CMIS_DDM_TH, &cmis_ddm_limits,
                                   sizeof(cmis_ddm_limits));
        if (ret < 0) {
        seq_printf(s, "QSFP read error: %d\n", ret);
        return 0;
    }

    seq_printf(s, "************ temperature threshold limits ****"
                                                    "********\n");
    seq_printf(s, "temp_high_alarm: %d °C \ntemp_low_alarm : %d °C "
           "\ntemp_high_warn : %d °C \ntemp_low_warn  : %d °C \n\n",
                   trx_calibrate_temp(cmis_ddm_limits.temp_high_alarm),
                    trx_calibrate_temp(cmis_ddm_limits.temp_low_alarm),
                    trx_calibrate_temp(cmis_ddm_limits.temp_high_warn),
                    trx_calibrate_temp(cmis_ddm_limits.temp_low_warn));

    seq_printf(s, "********** supply voltage threshold limits ****"
                                                      "*******\n");
    seq_printf(s, "volt_high_alarm: %d mV \nvolt_low_alarm : %d mV \n"
                "volt_high_warn : %d mV \nvolt_low_warn	: %d mV \n\n",
                   trx_calibrate_vcc(cmis_ddm_limits.volt_high_alarm),
                    trx_calibrate_vcc(cmis_ddm_limits.volt_low_alarm),
                    trx_calibrate_vcc(cmis_ddm_limits.volt_high_warn),
                    trx_calibrate_vcc(cmis_ddm_limits.volt_low_warn));

    seq_printf(s, "************* tx power threshold limits ********"
                                                        "******\n");
    seq_printf(s, "txpwr_high_alarm: %d µW \ntxpwr_low_alarm : %d µW \n"
                "txpwr_high_warn : %d µW \ntxpwr_low_warn  : %d µW \n\n",
                   trx_calibrate_power(cmis_ddm_limits.txpwr_high_alarm),
                    trx_calibrate_power(cmis_ddm_limits.txpwr_low_alarm),
                    trx_calibrate_power(cmis_ddm_limits.txpwr_high_warn),
                    trx_calibrate_power(cmis_ddm_limits.txpwr_low_warn));

    seq_printf(s, "************* rx power threshold limits ********"
                                                        "******\n");
    seq_printf(s, "rxpwr_high_alarm: %d µW \nrxpwr_low_alarm : %d µW \n"
                "rxpwr_high_warn : %d µW \nrxpwr_low_warn  : %d µW \n\n",
                   trx_calibrate_power(cmis_ddm_limits.rxpwr_high_alarm),
                    trx_calibrate_power(cmis_ddm_limits.rxpwr_low_alarm),
                    trx_calibrate_power(cmis_ddm_limits.rxpwr_high_warn),
                    trx_calibrate_power(cmis_ddm_limits.rxpwr_low_warn));

    seq_printf(s, "************** tx bias threshold limits ********"
                                                        "******\n");
    seq_printf(s, "bias_high_alarm: %d mA \nbias_low_alarm : %d mA \n"
                "bias_high_warn : %d mA \nbias_low_warn  : %d mA \n\n",
                 trx_calibrate_txbias(cmis_ddm_limits.bias_high_alarm),
                  trx_calibrate_txbias(cmis_ddm_limits.bias_low_alarm),
                  trx_calibrate_txbias(cmis_ddm_limits.bias_high_warn),
                  trx_calibrate_txbias(cmis_ddm_limits.bias_low_warn));
       break;
    default:
        spec_info_print(s, *spec_id);
    }

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_ddm_thresholds);

static const char* link_length_range_to_str(trx_link_length_range link_length_range)
{
    switch (link_length_range) {
    case TRX_SR:
        return "SR";
    case TRX_LR:
        return "LR";
    case TRX_ER:
        return "ER";
    case TRX_ZR:
        return "ZR";
    case TRX_CR:
        return "CR";
    case TRX_CLR:
        return "CLR";
    case TRX_DR:
        return "DR";
    case TRX_BR:
        return "BR";
    case TRX_FR:
        return "FR";
    case TRX_VR:
        return "VR";
    default:
        return "Unknown";
    }
}

static int qsfp_debug_device_link_length_range_show(struct seq_file *s,
                                                    void *data)
{
    struct qsfp *qsfp = s->private;
    trx_link_length_range link_length_range;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        seq_printf(s, "QSFP transceiver not inserted\n");
        return 0;
    }

    if (!qsfp->spec_ops) {
        seq_printf(s, "Spec ops not initiazed\n");
        return 0;
    }

    link_length_range = qsfp->spec_ops->get_link_length_range(qsfp);

    seq_printf(s, "%s\n", link_length_range_to_str(link_length_range));

    return 0;
}
DEFINE_SHOW_ATTRIBUTE(qsfp_debug_device_link_length_range);

int create_common_debugfs_files(struct qsfp *qsfp)
{
    struct dentry *file = NULL;

    file = debugfs_create_file("module_revision", 0600,
                                   qsfp->module_debugfs_dir,
                                   qsfp, &qsfp_debug_revision_info_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp module_revision debugfs_create_file fail,"
                        "error %ld", PTR_ERR(file));
        goto failed_module_dir;
     }

    file = debugfs_create_file("connector", 0600, qsfp->module_debugfs_dir,
                               qsfp, &qsfp_debug_connector_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp phys_ext_id debugfs_create_file fail,"
                        " error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("vendor_info", 0600, qsfp->module_debugfs_dir,
                        qsfp, &qsfp_debug_vendor_info_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp vendor_info debugfs_create_file fail,"
                        " error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("temperature", 0600, qsfp->module_debugfs_dir,
                        qsfp, &qsfp_debug_device_temperature_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp temperature debugfs_create_file fail,"
                        " error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("supply_voltage", 0600,
                        qsfp->module_debugfs_dir, qsfp,
                        &qsfp_debug_device_Supply_Voltage_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp supply_voltage debugfs_create_file fail,"
                        " error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("rx_power", 0600, qsfp->module_debugfs_dir,
                        qsfp, &qsfp_debug_device_rx_power_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp rx_power debugfs_create_file fail,"
                        " error %ld", PTR_ERR(file));
        goto failed_module_dir;;
    }

    file = debugfs_create_file("tx_bias_current", 0600,
                        qsfp->module_debugfs_dir,
                        qsfp, &qsfp_debug_device_tx_bias_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp tx_bias debugfs_create_file fail,"
                        " error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("tx_power", 0600, qsfp->module_debugfs_dir,
                        qsfp, &qsfp_debug_device_tx_power_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp tx_power debugfs_create_file fail,"
                        " error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("ddm_thresholds", 0600, qsfp->module_debugfs_dir,
                        qsfp, &qsfp_debug_device_ddm_thresholds_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp ddm_thresholds debugfs_create_file fail,"
                        " error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    file = debugfs_create_file("link_length_range", 0600, qsfp->module_debugfs_dir,
                               qsfp, &qsfp_debug_device_link_length_range_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp link_length_range debugfs_create_file fail,"
                          " error %ld", PTR_ERR(file));
        goto failed_module_dir;
    }

    return 0;

failed_module_dir:
    debugfs_remove_recursive(qsfp->module_debugfs_dir);
    qsfp->module_debugfs_dir = NULL;
    return -1;
}

int module_debugfs_init(struct qsfp *qsfp)
{
    if(!qsfp->debugfs_dir || !qsfp->fpc->debugfs_dir ||
       !transceiver_debugfs_dir) {
        TRX_LOG_ERR(qsfp, "Module debugfs parent dir failed\n");
        qsfp->module_debugfs_dir = NULL;
        return -1;
    }

    if (IS_ERR(qsfp->debugfs_dir) || IS_ERR(qsfp->fpc->debugfs_dir) ||
        IS_ERR(transceiver_debugfs_dir)) {
        TRX_LOG_ERR(qsfp, "debugfs_create_dir fail, "
                        "error (%ld %ld %ld)",
                        PTR_ERR(transceiver_debugfs_dir),
                        PTR_ERR(qsfp->fpc->debugfs_dir),
                        PTR_ERR(qsfp->debugfs_dir));
        qsfp->module_debugfs_dir = NULL;
        return -1;
    }

    qsfp->module_debugfs_dir = debugfs_create_dir("module_spec_info",
                                                  qsfp->debugfs_dir);
    if (!qsfp->module_debugfs_dir || IS_ERR(qsfp->module_debugfs_dir)) {
        TRX_LOG_ERR(qsfp, "qsfp module_spec_info debugfs_create_dir"
                        "fail, error %ld",
                        PTR_ERR(qsfp->module_debugfs_dir));
        qsfp->module_debugfs_dir = NULL;
        return -1;
    }
    return 0;
}

void module_debugfs_exit(struct qsfp *qsfp)
{
    if (!qsfp->fpc) {
        TRX_LOG_ERR(qsfp, "fpc is NULL");
        return;
    }

    if(!qsfp->module_debugfs_dir || !qsfp->debugfs_dir ||
       !qsfp->fpc->debugfs_dir || !transceiver_debugfs_dir)
    {
        TRX_LOG_ERR(qsfp, "Module debugfs create dir failed\n");
        qsfp->module_debugfs_dir = NULL;
        return;
    }

    if (IS_ERR(qsfp->module_debugfs_dir) || IS_ERR(qsfp->debugfs_dir) ||
        IS_ERR(qsfp->fpc->debugfs_dir) ||
        IS_ERR(transceiver_debugfs_dir)) {
        TRX_LOG_ERR(qsfp, "debugfs_create_dir fail, error "
                        "(%ld %ld %ld %ld)",
                        PTR_ERR(transceiver_debugfs_dir),
                        PTR_ERR(qsfp->fpc->debugfs_dir),
                        PTR_ERR(qsfp->debugfs_dir),
                        PTR_ERR(qsfp->module_debugfs_dir));
        qsfp->module_debugfs_dir = NULL;
        return;
    }
    else
    {
        debugfs_remove_recursive(qsfp->module_debugfs_dir);
        qsfp->module_debugfs_dir = NULL;
        TRX_LOG_INFO(qsfp, "Removed module debugfs dir\n");
    }
}

static ssize_t qsfp_simulation_read(struct file *file, char __user *ubuf,
                                    size_t count, loff_t *ppos)
{
    struct qsfp *qsfp = file->f_inode->i_private;
    struct qsfp_flags sf;
    char buf[SIM_READ_BUF_MAX] = "";

    sf = qsfp->sim_flags;

    scnprintf(buf, SIM_READ_BUF_MAX, "Simulation Remove: %s\nSimulation Flags:"
    " %s\n\nTemperature High Alarm: %s\nTemperature Low Alarm: %s\nTemperature"
    " High Warning: %s\nTemperature Low Warning: %s\n\nVoltage High Alarm: %s"
    "\nVoltage Low Alarm: %s\nVoltage High Warning: %s\nVoltage Low Warning: "
    "%s\n\nBelow are Lane flags (LSB for lane0 and MSB for lane7)\n\n0x%02X RX"
    " LOS\n0x%02X TX Fault\n0x%02X TX LOS\n\n0x%02X RX CDR LOL\n0x%02X TX CDR "
    "LOL\n0x%02X TX Adaptive EQ IN Fail\n\n0x%02X RX Power High Alarm\n0x%02X RX"
    " Power Low Alarm\n0x%02X RX Power High Warning\n0x%02X RX Power Low "
    "Warning\n\n0x%02X TX Power High Alarm\n0x%02X TX Power Low Alarm\n0x%02X "
    "TX Power High Warning\n0x%02X TX Power Low Warning\n\n0x%02X TX Bias High"
    " Alarm\n0x%02X TX Bias Low Alarm\n0x%02X TX Bias High Warning\n0x%02X TX "
    "Bias Low Warning\n\n", qsfp->sim.remove ? "Yes" : "No",
    qsfp->sim.flags ? "Yes" : "No",
    (sf.temp & QSFP_TEMP_HIGH_ALARM) ? "Yes" : "No",
    (sf.temp & QSFP_TEMP_LOW_ALARM) ? "Yes" : "No",
    (sf.temp & QSFP_TEMP_HIGH_WARN) ? "Yes" : "No",
    (sf.temp & QSFP_TEMP_LOW_WARN) ? "Yes" : "No",
    (sf.volt & QSFP_VOLT_HIGH_ALARM) ? "Yes" : "No",
    (sf.volt & QSFP_VOLT_LOW_ALARM) ? "Yes" : "No",
    (sf.volt & QSFP_VOLT_HIGH_WARN) ? "Yes" : "No",
    (sf.volt & QSFP_VOLT_LOW_WARN) ? "Yes" : "No",
    sf.rx_los, sf.tx_fault, sf.tx_los, sf.rx_cdr_lol, sf.tx_cdr_lol,
    sf.tx_adap_eq_in_fail, sf.rx_power_high_alarm, sf.rx_power_low_alarm,
    sf.rx_power_high_warn, sf.rx_power_low_warn, sf.tx_power_high_alarm,
    sf.tx_power_low_alarm, sf.tx_power_high_warn, sf.tx_power_low_warn,
    sf.tx_bias_high_alarm, sf.tx_bias_low_alarm, sf.tx_bias_high_warn,
    sf.tx_bias_low_warn);

    return simple_read_from_buffer(ubuf, count, ppos, buf, strlen(buf));;
}

static void qsfp_sim_insert(struct qsfp *qsfp)
{
    int ret;

    /* Check whether Transceiver module state present or not */
    if (qsfp->status.present) {
        TRX_LOG_ERR(qsfp, "Simulated insert rejected as module "
                          "already present");
        return;
    }

    /* Check whether Transceiver module physically present or not */
    ret = fpc_is_module_present(qsfp);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "fpc_is_module_present failed. ret %d", ret);
        return;
    } else if (ret != QSFP_PRESENT) {
        TRX_LOG_ERR(qsfp, "Simulated insert rejected as module physically"
                          " not present in the port");
        return;
    }

    qsfp->sim.remove = 0;
    qsfp_module_insert_irq(qsfp);

    fpc_reset_qsfp(qsfp);

    TRX_LOG_INFO(qsfp, "------ SIMULATED INSERT ------");
}

static void qsfp_sim_remove(struct qsfp *qsfp)
{
    if (!qsfp->status.present) {
        TRX_LOG_ERR(qsfp, "Simulated remove rejected as module not present");
        return;
    }

    qsfp->sim.remove = 1;
    qsfp_module_remove_irq(qsfp);

    TRX_LOG_INFO(qsfp, "------ SIMULATED REMOVE ------");
}

static void qsfp_sim_flags_change(struct qsfp *qsfp)
{
    if (!qsfp->status.present) {
        TRX_LOG_ERR(qsfp, "Simulated flags change rejected as module not present");
        return;
    }

    qsfp->sim.flags = 1;

    qsfp_stop_poll(qsfp);

    qsfp_check_state(qsfp);

    TRX_LOG_INFO(qsfp, "------ SIMULATED FLAGS CHANGE ------");
}

static void qsfp_sim_clear(struct qsfp *qsfp)
{
    memset(&qsfp->sim_flags, 0, sizeof(qsfp->sim_flags));

    if (qsfp->sim.remove) {
        qsfp->sim.remove = 0;
        qsfp_module_insert_irq(qsfp);

        fpc_reset_qsfp(qsfp);

        TRX_LOG_INFO(qsfp, "------ SIMULATED INSERT ------");
    }

    if (qsfp->sim.flags) {

        qsfp->sim.flags = 0;

        memset(&qsfp->flags, 0, sizeof(qsfp->flags));

        if (qsfp_atleast_one_flag_supported(qsfp)) {
        /* start polling to see actual hardware state which clears any
         * simulated flags
         */
            qsfp->need_poll = true;
            mod_delayed_work(system_wq, &qsfp->poll, 0);
        }
    }

    TRX_LOG_INFO(qsfp, "------ SIMULATION CLEAR ------");
}

static ssize_t qsfp_simulation_write(struct file *file, const char __user *buf,
                                     size_t count, loff_t *ppos)
{
    ssize_t ret;
    char request[SIM_REQ_MAX] = {0};
    struct qsfp *qsfp = file->f_inode->i_private;
    struct lane *lanei;
    bool recovery = false;
    char *p;
    u8 *flag;
    u8 start, end, i;

    ret = simple_write_to_buffer(request, SIM_REQ_MAX - 1, ppos, buf, count);
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "simple_write_to_buffer fails. ret %d", ret);

    } else if (!strncmp(request, SIM_INSERT, sizeof(SIM_INSERT))) {
        qsfp_sim_insert(qsfp);
        return ret;

    } else if (!strncmp(request, SIM_REMOVE, sizeof(SIM_REMOVE))) {
        qsfp_sim_remove(qsfp);
        return ret;

    } else if (!strncmp(request, SIM_COPY_FLAGS, sizeof(SIM_COPY_FLAGS))) {
        qsfp->sim_flags = qsfp->flags;
        TRX_LOG_INFO(qsfp, "Copied current flags into simulation flags");
        return ret;

    } else if (!strncmp(request, SIM_FLAGS_CHANGE, sizeof(SIM_FLAGS_CHANGE))) {
        qsfp_sim_flags_change(qsfp);
        return ret;

    } else if (!strncmp(request, SIM_CLEAR, sizeof(SIM_CLEAR))) {
        qsfp_sim_clear(qsfp);
        return ret;

    } else if (!strncmp(request, SIM_TEMP_HIGH_ALARM,
                        sizeof(SIM_TEMP_HIGH_ALARM))) {
        qsfp->sim_flags.temp_high_alarm = 1;
        return ret;

    } else if (!strncmp(request, SIM_TEMP_HIGH_ALARM_RECOVERY,
                        sizeof(SIM_TEMP_HIGH_ALARM_RECOVERY))) {
        qsfp->sim_flags.temp_high_alarm = 0;
        return ret;

    } else if (!strncmp(request, SIM_TEMP_LOW_ALARM,
                        sizeof(SIM_TEMP_LOW_ALARM))) {
        qsfp->sim_flags.temp_low_alarm = 1;
        return ret;

    } else if (!strncmp(request, SIM_TEMP_LOW_ALARM_RECOVERY,
                        sizeof(SIM_TEMP_LOW_ALARM_RECOVERY))) {
        qsfp->sim_flags.temp_low_alarm = 0;
        return ret;

    } else if (!strncmp(request, SIM_TEMP_HIGH_WARN,
                        sizeof(SIM_TEMP_HIGH_WARN))) {
        qsfp->sim_flags.temp_high_warn = 1;
        return ret;

    } else if (!strncmp(request, SIM_TEMP_HIGH_WARN_RECOVERY,
                        sizeof(SIM_TEMP_HIGH_WARN_RECOVERY))) {
        qsfp->sim_flags.temp_high_warn = 0;
        return ret;

    } else if (!strncmp(request, SIM_TEMP_LOW_WARN,
                        sizeof(SIM_TEMP_LOW_WARN))) {
        qsfp->sim_flags.temp_low_warn = 1;
        return ret;

    } else if (!strncmp(request, SIM_TEMP_LOW_WARN_RECOVERY,
                        sizeof(SIM_TEMP_LOW_WARN_RECOVERY))) {
        qsfp->sim_flags.temp_low_warn = 0;
        return ret;

    } else if (!strncmp(request, SIM_VOLT_HIGH_ALARM,
                        sizeof(SIM_VOLT_HIGH_ALARM))) {
        qsfp->sim_flags.volt_high_alarm = 1;
        return ret;

    } else if (!strncmp(request, SIM_VOLT_HIGH_ALARM_RECOVERY,
                        sizeof(SIM_VOLT_HIGH_ALARM_RECOVERY))) {
        qsfp->sim_flags.volt_high_alarm = 0;
        return ret;

    } else if (!strncmp(request, SIM_VOLT_LOW_ALARM,
                        sizeof(SIM_VOLT_LOW_ALARM))) {
        qsfp->sim_flags.volt_low_alarm = 1;
        return ret;

    } else if (!strncmp(request, SIM_VOLT_LOW_ALARM_RECOVERY,
                        sizeof(SIM_VOLT_LOW_ALARM_RECOVERY))) {
        qsfp->sim_flags.volt_low_alarm = 0;
        return ret;

    } else if (!strncmp(request, SIM_VOLT_HIGH_WARN,
                        sizeof(SIM_VOLT_HIGH_WARN))) {
        qsfp->sim_flags.volt_high_warn = 1;
        return ret;

    } else if (!strncmp(request, SIM_VOLT_HIGH_WARN_RECOVERY,
                        sizeof(SIM_VOLT_HIGH_WARN_RECOVERY))) {
        qsfp->sim_flags.volt_high_warn = 0;
        return ret;

    } else if (!strncmp(request, SIM_VOLT_LOW_WARN,
                        sizeof(SIM_VOLT_LOW_WARN))) {
        qsfp->sim_flags.volt_low_warn = 1;
        return ret;

    } else if (!strncmp(request, SIM_VOLT_LOW_WARN_RECOVERY,
                        sizeof(SIM_VOLT_LOW_WARN_RECOVERY))) {
        qsfp->sim_flags.volt_low_warn = 0;
        return ret;
    }

    p = request;
    /* Lane flags */
    if (!strncmp(request, SIM_RX_LOS, sizeof(SIM_RX_LOS) - 1)) {
        flag = &qsfp->sim_flags.rx_los;
        p += sizeof(SIM_RX_LOS) - 1;

    } else if (!strncmp(request, SIM_TX_FAULT, sizeof(SIM_TX_FAULT) - 1)) {
        flag = &qsfp->sim_flags.tx_fault;
        p += sizeof(SIM_TX_FAULT) - 1;

    } else if (!strncmp(request, SIM_TX_LOS, sizeof(SIM_TX_LOS) - 1)) {
        flag = &qsfp->sim_flags.tx_los;
        p += sizeof(SIM_TX_LOS) - 1;

    } else if (!strncmp(request, SIM_RX_CDR_LOL, sizeof(SIM_RX_CDR_LOL) - 1)) {
        flag = &qsfp->sim_flags.rx_cdr_lol;
        p += sizeof(SIM_RX_CDR_LOL) - 1;

    } else if (!strncmp(request, SIM_TX_CDR_LOL, sizeof(SIM_TX_CDR_LOL) - 1)) {
        flag = &qsfp->sim_flags.tx_cdr_lol;
        p += sizeof(SIM_TX_CDR_LOL) - 1;

    } else if (!strncmp(request, SIM_TX_ADAP_EQ_IN_FAIL,
                        sizeof(SIM_TX_ADAP_EQ_IN_FAIL) - 1)) {
        flag = &qsfp->sim_flags.tx_adap_eq_in_fail;
        p += sizeof(SIM_TX_ADAP_EQ_IN_FAIL) - 1;

    } else if (!strncmp(request, SIM_RX_POWER_HIGH_ALARM,
                        sizeof(SIM_RX_POWER_HIGH_ALARM) - 1)) {
        flag = &qsfp->sim_flags.rx_power_high_alarm;
        p += sizeof(SIM_RX_POWER_HIGH_ALARM) - 1;

    } else if (!strncmp(request, SIM_RX_POWER_LOW_ALARM,
                        sizeof(SIM_RX_POWER_LOW_ALARM) - 1)) {
        flag = &qsfp->sim_flags.rx_power_low_alarm;
        p += sizeof(SIM_RX_POWER_LOW_ALARM) - 1;

    } else if (!strncmp(request, SIM_RX_POWER_HIGH_WARN,
                        sizeof(SIM_RX_POWER_HIGH_WARN) - 1)) {
        flag = &qsfp->sim_flags.rx_power_high_warn;
        p += sizeof(SIM_RX_POWER_HIGH_WARN) - 1;

    } else if (!strncmp(request, SIM_RX_POWER_LOW_WARN,
                        sizeof(SIM_RX_POWER_LOW_WARN) - 1)) {
        flag = &qsfp->sim_flags.rx_power_low_warn;
        p += sizeof(SIM_RX_POWER_LOW_WARN) - 1;

    } else if (!strncmp(request, SIM_TX_POWER_HIGH_ALARM,
                        sizeof(SIM_TX_POWER_HIGH_ALARM) - 1)) {
        flag = &qsfp->sim_flags.tx_power_high_alarm;
        p += sizeof(SIM_TX_POWER_HIGH_ALARM) - 1;

    } else if (!strncmp(request, SIM_TX_POWER_LOW_ALARM,
                        sizeof(SIM_TX_POWER_LOW_ALARM) - 1)) {
        flag = &qsfp->sim_flags.tx_power_low_alarm;
        p += sizeof(SIM_TX_POWER_LOW_ALARM) - 1;

    } else if (!strncmp(request, SIM_TX_POWER_HIGH_WARN,
                        sizeof(SIM_TX_POWER_HIGH_WARN) - 1)) {
        flag = &qsfp->sim_flags.tx_power_high_warn;
        p += sizeof(SIM_TX_POWER_HIGH_WARN) - 1;

    } else if (!strncmp(request, SIM_TX_POWER_LOW_WARN,
                        sizeof(SIM_TX_POWER_LOW_WARN) - 1)) {
        flag = &qsfp->sim_flags.tx_power_low_warn;
        p += sizeof(SIM_TX_POWER_LOW_WARN) - 1;

    } else if (!strncmp(request, SIM_TX_BIAS_HIGH_ALARM,
                        sizeof(SIM_TX_BIAS_HIGH_ALARM) - 1)) {
        flag = &qsfp->sim_flags.tx_bias_high_alarm;
        p += sizeof(SIM_TX_BIAS_HIGH_ALARM) - 1;

    } else if (!strncmp(request, SIM_TX_BIAS_LOW_ALARM,
                        sizeof(SIM_TX_BIAS_LOW_ALARM) - 1)) {
        flag = &qsfp->sim_flags.tx_bias_low_alarm;
        p += sizeof(SIM_TX_BIAS_LOW_ALARM) - 1;

    } else if (!strncmp(request, SIM_TX_BIAS_HIGH_WARN,
                        sizeof(SIM_TX_BIAS_HIGH_WARN) - 1)) {
        flag = &qsfp->sim_flags.tx_bias_high_warn;
        p += sizeof(SIM_TX_BIAS_HIGH_WARN) - 1;

    } else if (!strncmp(request, SIM_TX_BIAS_LOW_WARN,
                        sizeof(SIM_TX_BIAS_LOW_WARN) - 1)) {
        flag = &qsfp->sim_flags.tx_bias_low_warn;
        p += sizeof(SIM_TX_BIAS_LOW_WARN) - 1;

    } else {
        goto error;
    }

    /* Lane flag recovery case */
    if (!strncmp(p, SIM_RECOVERY, strlen(SIM_RECOVERY))) {
        recovery = true;
        p += sizeof(SIM_RECOVERY) - 1;
    }

    if (*p == '\n') {
        /* ex: rx_los. In this case all lanes considered */
        start = 0;
        end = qsfp->num_lanes - 1;
        goto success;
    } else if (*p != '-') {
        /* ex: rx_los-* */
        goto error;
    }

    p++;
    /* Check whether integer or not */
    if ((*p >= '0') && (*p <= '9')) {
        start = *p - '0';
    } else {
        goto error;
    }

    p++;
    if (*p == '\n') {
        /* ex: rx_los-2. In this case end is same as start
         * only one lane considered
         */
        end = start;
        goto success;
    } else if (*p != '-') {
        goto error;
    }

    p++;
    if ((*p >= '0') && (*p <= '9')) {
        /* ex: rx_los-0-2 */
        end = *p - '0';
    } else {
        goto error;
    }

    p++;
    if (*p != '\n') {
        goto error;
    }

success:
    /* Added to remove static analysis error */
    if ((start >= MAX_LANES) || (end >= MAX_LANES)) {
        TRX_LOG_ERR(qsfp, "Invalid lane. start %u end %u", start, end);
        goto error;
    }

    if ((start >= qsfp->num_lanes) || (end >= qsfp->num_lanes) ||
        (start > end)) {
        TRX_LOG_ERR(qsfp, "Invalid lane. start %u end %u", start, end);
        goto error;
    }

    for (i = start ; i <= end ; i++) {
        lanei = qsfp->lane[i];
        if (!lanei) {
            TRX_LOG_ERR(qsfp, "Lane %u NULL", i);
            continue;
        }

        if (lanei->status.present) {
            if (recovery) {
                *flag &= (~(1 << i));
            } else {
                *flag |= (1 << i);
            }
        } else {
            TRX_LOG_ERR(lanei, "Lane not present");
        }
    }

    return ret;

error:
    TRX_LOG_ERR(qsfp, "Invalid input %s", request);
    return -EINVAL;
}

static const struct file_operations qsfp_debug_qsfp_simulation_fops = {
    .read = qsfp_simulation_read,
    .write = qsfp_simulation_write,
};

void qsfp_debugfs_init(struct qsfp *qsfp)
{
    struct dentry *file = NULL;
    char qsfp_devname[20] = {};
    char qsfp_devsubname[10] = {};
    strlcpy(qsfp_devname,dev_name(qsfp->dev),sizeof(qsfp_devname));

    if(!qsfp->fpc->debugfs_dir || !transceiver_debugfs_dir)
    {
        TRX_LOG_ERR(qsfp, "Trx debugfs create dir fail\n");
        qsfp->debugfs_dir = NULL;
        return;
    }

    if (IS_ERR(qsfp->fpc->debugfs_dir) || IS_ERR(transceiver_debugfs_dir)) {
        TRX_LOG_ERR(qsfp, "debugfs_create_dir fail, error (%ld %ld)",
                        PTR_ERR(transceiver_debugfs_dir),
                        PTR_ERR(qsfp->fpc->debugfs_dir));
        qsfp->debugfs_dir = NULL;
        return;
    }

    if (strncmp(qsfp_devname,"soc:",4) == 0) {
        strlcpy(qsfp_devsubname,&qsfp_devname[4],sizeof(qsfp_devsubname));
    }
    else {
        strlcpy(qsfp_devsubname,qsfp_devname,sizeof(qsfp_devsubname));
    }

    TRX_LOG_INFO(qsfp, "dev %s , sub %s", dev_name(qsfp->dev),
                                          qsfp_devsubname);

    qsfp->debugfs_dir = debugfs_create_dir(qsfp_devsubname,
                         qsfp->fpc->debugfs_dir);
    if (!qsfp->debugfs_dir || IS_ERR(qsfp->debugfs_dir)) {
        TRX_LOG_ERR(qsfp, "qsfp debugfs_create_dir fail, error %ld",
                        PTR_ERR(qsfp->debugfs_dir));
        qsfp->debugfs_dir = NULL;
        return;
    }

    file = debugfs_create_file("state_info", 0600, qsfp->debugfs_dir, qsfp,
                        &qsfp_debug_qsfp_state_info_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp state_info debugfs_create_file fail,"
                        " error %ld", PTR_ERR(file));
        goto failed_trx_debugfs_dir;
    }

    file = debugfs_create_file("i2c_address_info", 0600, qsfp->debugfs_dir,
                        qsfp, &qsfp_debug_qsfp_i2c_address_info_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp i2c_address_info debugfs_create_file "
                        "fail, error %ld", PTR_ERR(file));
        goto failed_trx_debugfs_dir;
    }

    file = debugfs_create_file("port_num_info", 0600, qsfp->debugfs_dir, qsfp,
                        &qsfp_debug_qsfp_port_num_info_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp port_num_info debugfs_create_file fail,"
                        "error %ld", PTR_ERR(file));
        goto failed_trx_debugfs_dir;
    }

    file = debugfs_create_file("module_identifier", 0600, qsfp->debugfs_dir,
                   qsfp, &qsfp_debug_qsfp_module_identifier_info_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp module_identifier debugfs_create_file"
                        " fail, error %ld", PTR_ERR(file));
        goto failed_trx_debugfs_dir;
    }

    file = debugfs_create_file("flags", 0600, qsfp->debugfs_dir,
                   qsfp, &qsfp_debug_qsfp_flags_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp flags debugfs_create_file"
                        " fail, error %ld", PTR_ERR(file));
        goto failed_trx_debugfs_dir;
    }

    file = debugfs_create_file("simulation", 0600, qsfp->debugfs_dir, qsfp,
                               &qsfp_debug_qsfp_simulation_fops);
    if (!file || IS_ERR(file)) {
        TRX_LOG_ERR(qsfp, "qsfp simulation debugfs_create_file fail,"
                        " error %ld", PTR_ERR(file));
        goto failed_trx_debugfs_dir;
    }

    return;

failed_trx_debugfs_dir:
    debugfs_remove_recursive(qsfp->debugfs_dir);
    qsfp->debugfs_dir = NULL;

}

void qsfp_debugfs_exit(struct qsfp *qsfp)
{
    if (!qsfp->fpc) {
        TRX_LOG_ERR(qsfp, "fpc is NULL");
        return;
    }

    if(!qsfp->debugfs_dir || !qsfp->fpc->debugfs_dir ||
       !transceiver_debugfs_dir)
    {
        TRX_LOG_ERR(qsfp, "Trx debugfs create dir fail\n");
        qsfp->debugfs_dir = NULL;
        return;
    }

    if (IS_ERR(qsfp->debugfs_dir) || IS_ERR(qsfp->fpc->debugfs_dir) ||
        IS_ERR(transceiver_debugfs_dir)) {
        TRX_LOG_ERR(qsfp, "debugfs_create_dir fail, error (%ld %ld %ld)",
                        PTR_ERR(transceiver_debugfs_dir),
                        PTR_ERR(qsfp->fpc->debugfs_dir),
                        PTR_ERR(qsfp->debugfs_dir));
        qsfp->debugfs_dir = NULL;
        return;
    }
    else
    {
        debugfs_remove_recursive(qsfp->debugfs_dir);
        qsfp->debugfs_dir = NULL;
        TRX_LOG_INFO(qsfp, "Removed Trx debugfs dir\n");
    }
}
#else
void transceiver_debugfs_init(void)
{
}

void transceiver_debugfs_exit(void)
{
}

void fpc_debugfs_init(struct fpc *fpc)
{
}

void fpc_debugfs_exit(struct fpc *fpc)
{
}

void qsfp_debugfs_init(struct qsfp *qsfp)
{
}

void qsfp_debugfs_exit(struct qsfp *qsfp)
{
}
#endif
