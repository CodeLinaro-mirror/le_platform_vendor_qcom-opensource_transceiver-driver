/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Code is derived from http://git.armlinux.org.uk/cgit/linux-arm.git/
 * tree/drivers/net/phy/sfp.h?h=cex7
 *
 */
#ifndef LINUX_QSFP_H
#define LINUX_QSFP_H

#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/rtnetlink.h>
#include <linux/of_platform.h>
#include <linux/ipc_logging.h>

#define QSFP_ADDR(device, page, addr) ((device) << 16 | (page) << 8 | (addr))

#include "sfp.h"
#include "sff8636.h"
#include "cmis.h"
#include "sff8472.h"
#include "transceiver_api.h"

#define QSFP_COMPATIBLE "sff,qsfp"

#define QSFP_PAGE_OFFSET (0x7F)
#define QSFP_LED1 (BIT(0))
#define QSFP_LED2 (BIT(1))
#define QSFP_LED_ON  (true)
#define QSFP_LED_OFF (false)

#define QSFP_LOS_SHIFT (8)
#define QSFP_TX_FAULT_SHIFT (16)

#define MAX_LANES 8

struct qsfp_eeprom_id {
    union {
        struct sff8636_eeprom_id sff8636;
        struct cmis_eeprom_id cmis;
        struct sfp_eeprom_id sff8472;
    };
};

struct qsfp {
    struct device *dev;
    struct fpc *fpc;
    struct i2c_adapter *i2c;
    u32 max_power_mW;
    u32 module_power_mW;
    u32 lanes_state;
    /* Stores presence LOS TX Fault TX Disable status */
    u8 status;
    u8 port_num;
    u8 i2c_address_dev0;
    u8 i2c_address_dev1;
    bool module_flat_mem;
    bool need_poll;
    struct delayed_work poll;;
    u8 features;
    u8 module_power_class;
    u8 module_revision;
    u8 sm_mod_state;
    u8 sm_dev_state;
    u8 sm_link_state;
    u8 sm_mod_tries;
    size_t i2c_block_size;

    struct delayed_work timeout;
    struct mutex sm_mutex;            /* Protects state machine */

    struct qsfp_eeprom_id id;
    const struct qsfp_spec_ops *spec_ops;

    struct lane* lane[MAX_LANES];
    u8 num_lanes;
#if IS_ENABLED(CONFIG_DEBUG_FS)
    struct dentry *debugfs_dir;
    struct dentry *module_debugfs_dir;
    u8 sim;
#endif
   struct kobject *qsfp_sysfs_dir;
};

struct qsfp_spec_ops {
    /* called during module insert to read EEPROM */
    int (*mod_probe)(struct qsfp *qsfp);
    /* Disable uninterested interrupts */
    void (*disable_redundant_irq)(const struct qsfp *qsfp);
    /* Gets module current state LOS,TX Fault */
    u32 (*get_state)(struct qsfp *qsfp);
    /* Disable TX for whole transceiver module */
    int (*mod_tx_disable)(const struct qsfp *qsfp);
    /* Enable TX lanewise */
    int (*lane_tx_enable)(const struct lane *lane);
    /* Disable TX lanewise */
    int (*lane_tx_disable)(const struct lane *lane);
    /* Check feature like LOS,TX Fault implemented or not and
     * update features field accordingly
     */
    int (*check_features_impl)(struct qsfp *qsfp);
    /* Gets power details like max power and power class */
    int (*module_parse_power)(struct qsfp *qsfp);
    /* called to handle situation of module max power is more
     * than max allowed power
     */
    int (*handle_max_power_exceed)(const struct qsfp *qsfp);
    /* configure module for high power */
    int (*mod_high_power)(const struct qsfp *qsfp);
    /* configure module for low power */
    int (*mod_low_power)(const struct qsfp *qsfp);
    /* Dumps EEPROM data */
    void (*eeprom_print)(const struct qsfp *qsfp);
    /* Ethtool callback function to get module info */
    int (*module_info)(struct qsfp *qsfp, struct ethtool_modinfo *modinfo);
    /* Gets connector type */
    u8 (*get_connector_type)(const struct qsfp *qsfp);
    /* Gets lane speed */
    int (*get_lane_speed)(const struct qsfp *qsfp, trx_lane_speed* speed);
    /* Gets transceive type */
    u8 (*get_transceiver_type)(const struct qsfp *qsfp);
    /* Gets Near-End Implementation */
    int (*get_lanes_presence)(const struct qsfp *qsfp, trx_lane_cfg* laneinfo);
    /* Gets Far-End Implementation */
    int (*get_breakout_config)(const struct qsfp *qsfp,
                          trx_breakout_cfg* bo_config);
    unsigned long (*irq_delay)(const struct qsfp *qsfp);
    int (*create_debugfs)(struct qsfp *qsfp);
};

enum {
    QSFP_F_PRESENT      = BIT(0),
    QSFP_F_LOS          = BIT(1),
    QSFP_F_TX_FAULT     = BIT(2),
    QSFP_F_TX_DISABLE   = BIT(3),

#if IS_ENABLED(CONFIG_DEBUG_FS)
    QSFP_F_SIM_REMOVE   = BIT(0),
    QSFP_F_SIM_FAR_END  = BIT(1),
#endif

    QSFP_F_PRESENT0      = BIT(0),
    QSFP_F_PRESENT1      = BIT(1),
    QSFP_F_PRESENT2      = BIT(2),
    QSFP_F_PRESENT3      = BIT(3),
    QSFP_F_PRESENT4      = BIT(4),
    QSFP_F_PRESENT5      = BIT(5),
    QSFP_F_PRESENT6      = BIT(6),
    QSFP_F_PRESENT7      = BIT(7),
    QSFP_F_PRESENT_0_7   = QSFP_F_PRESENT0 | QSFP_F_PRESENT1 |
                           QSFP_F_PRESENT2 | QSFP_F_PRESENT3 |
                           QSFP_F_PRESENT4 |  QSFP_F_PRESENT5 |
                           QSFP_F_PRESENT6 |  QSFP_F_PRESENT7,

    QSFP_F_LOS0          = BIT(8),
    QSFP_F_LOS1          = BIT(9),
    QSFP_F_LOS2          = BIT(10),
    QSFP_F_LOS3          = BIT(11),
    QSFP_F_LOS4          = BIT(12),
    QSFP_F_LOS5          = BIT(13),
    QSFP_F_LOS6          = BIT(14),
    QSFP_F_LOS7          = BIT(15),
    QSFP_F_LOS_0_7       = QSFP_F_LOS0 | QSFP_F_LOS1 |  QSFP_F_LOS2 |
                           QSFP_F_LOS3 |  QSFP_F_LOS4 |  QSFP_F_LOS5 |
                           QSFP_F_LOS6 |  QSFP_F_LOS7,

    QSFP_F_TX_FAULT0     = BIT(16),
    QSFP_F_TX_FAULT1     = BIT(17),
    QSFP_F_TX_FAULT2     = BIT(18),
    QSFP_F_TX_FAULT3     = BIT(19),
    QSFP_F_TX_FAULT4     = BIT(20),
    QSFP_F_TX_FAULT5     = BIT(21),
    QSFP_F_TX_FAULT6     = BIT(22),
    QSFP_F_TX_FAULT7     = BIT(23),
    QSFP_F_TX_FAULT_0_7  = QSFP_F_TX_FAULT0 | QSFP_F_TX_FAULT1 |
                           QSFP_F_TX_FAULT2 | QSFP_F_TX_FAULT3 |
                           QSFP_F_TX_FAULT4 | QSFP_F_TX_FAULT5 |
                           QSFP_F_TX_FAULT6 | QSFP_F_TX_FAULT7,

    QSFP_F_TX_DISABLE0    = BIT(24),
    QSFP_F_TX_DISABLE1    = BIT(25),
    QSFP_F_TX_DISABLE2    = BIT(26),
    QSFP_F_TX_DISABLE3    = BIT(27),
    QSFP_F_TX_DISABLE4    = BIT(28),
    QSFP_F_TX_DISABLE5    = BIT(29),
    QSFP_F_TX_DISABLE6    = BIT(30),
    QSFP_F_TX_DISABLE7    = BIT(31),
    QSFP_F_TX_DISABLE_0_7 = QSFP_F_TX_DISABLE0 | QSFP_F_TX_DISABLE1 |
                            QSFP_F_TX_DISABLE2 | QSFP_F_TX_DISABLE3 |
                            QSFP_F_TX_DISABLE4 | QSFP_F_TX_DISABLE5 |
                            QSFP_F_TX_DISABLE6 | QSFP_F_TX_DISABLE7,

    QSFP_E_INSERT = 0,
    QSFP_E_REMOVE,
    QSFP_E_DEV_ATTACH,
    QSFP_E_DEV_DETACH,
    QSFP_E_LANE_DOWN,
    QSFP_E_DEV_DOWN,
    QSFP_E_DEV_UP,
    QSFP_E_TX_FAULT,
    QSFP_E_TX_FAULT_RECOVERY,
    QSFP_E_LOS,
    QSFP_E_LOS_RECOVERY,
    QSFP_E_REVISIT,

    QSFP_MOD_EMPTY = 0,
    QSFP_MOD_ERROR,
    QSFP_MOD_REJECT_SPEC,
    QSFP_MOD_REJECT_PWR,
    QSFP_MOD_PROBE,
    QSFP_MOD_WAITDEV,
    QSFP_MOD_PRESENT,

    QSFP_DEV_DETACHED = 0,
    QSFP_DEV_DOWN,
    QSFP_DEV_UP,

    QSFP_S_DOWN = 0,
    QSFP_S_LOS,
    QSFP_S_TX_FAULT,
    QSFP_S_LINK_UP,
};

enum {
    E_UNSUPPORTED_SPEC = 1000,
    E_MAX_POWER_EXCEED,
};

#define PROBE_RETRY             50
#define PROBE_RETRY_TIME_GAP    msecs_to_jiffies(100)
#define MOD_READY_TIME          msecs_to_jiffies(300)

#define QSFP_STATE_STR_MAX_LEN (40)

enum {
    SFF8024_CONNECTOR_FC1_COPPER = 0x02,
    SFF8024_CONNECTOR_FC2_COPPER = 0x03,
    SFF8024_CONNECTOR_BNC_TNC    = 0x04,
    SFF8024_CONNECTOR_FC_COAX    = 0x05,
    SFF8024_CONNECTOR_CS_OPTICAL = 0x25,
    SFF8024_CONNECTOR_SN_OPTICAL = 0x26,
    SFF8024_CONNECTOR_MPO_2X12   = 0x27,
    SFF8024_CONNECTOR_MPO_1X16   = 0x28,
};

enum {
    SFF8024_ID_GBIC              = 0x01,
    SFF8024_ID_300_XBI           = 0x04,
    SFF8024_ID_XENPAK            = 0x05,
    SFF8024_ID_XFP               = 0x06,
    SFF8024_ID_XFF               = 0x07,
    SFF8024_ID_XFP_E             = 0x08,
    SFF8024_ID_XPAK              = 0x09,
    SFF8024_ID_X2                = 0x0A,
    SFF8024_ID_CXP               = 0x0E,
    SFF8024_ID_SH_ML_HD_4X       = 0x0F,
    SFF8024_ID_H_ML_HD_8X        = 0x10,
    SFF8024_ID_CXP2              = 0x12,
    SFF8024_ID_CPFP_S1_S2        = 0x13,
    SFF8024_ID_SH_ML_HD_4X_FO    = 0x14,
    SFF8024_ID_SH_ML_HD_8X_FO    = 0x15,
    SFF8024_ID_CPFP_S3           = 0x16,
    SFF8024_ID_MICRO_QSFP        = 0x17,
    SFF8024_ID_QSFPDD_CMIS       = 0x18,
    SFF8024_ID_QSFP_8X           = 0x19,
    SFF8024_ID_SFP_DD_2X         = 0x1A,
    SFF8024_ID_DSFP              = 0x1B,
    SFF8024_ID_MLNK_X4           = 0x1C,
    SFF8024_ID_MLNK_X8           = 0x1D,
    SFF8024_ID_QSFP_P_CMIS       = 0x1E,
};

extern const struct of_device_id fpc_qsfp_of_match[];
extern const struct qsfp_spec_ops sff8636_spec_ops;
extern const struct qsfp_spec_ops cmis_spec_ops;
extern const struct qsfp_spec_ops sff8472_spec_ops;
extern const struct sfp_socket_ops lane_ops;

extern int fpc_is_module_present(const struct qsfp *qsfp);
extern int fpc_enable_qsfp_interrupt(const struct qsfp *qsfp);

extern const char *mod_identifier_to_str(u8 spec_id);
extern const char *mod_link_codes_to_str(unsigned short mod_link_codes);

extern int qsfp_read(const struct qsfp *qsfp, u32 addr, void *buf, size_t len);
extern int qsfp_write(const struct qsfp *qsfp, u32 addr, void *buf, size_t len);
extern u8 qsfp_check(void *buf, size_t len);
extern int qsfp_get_link_type(struct qsfp *qsfp, u8* link_info);
extern void qsfp_sm_event(struct qsfp *qsfp, u8 event);
extern void qsfp_check_state(struct qsfp *qsfp);

extern int sff8636_create_debugfs_files (struct qsfp *qsfp);
extern int cmis_create_debugfs_files(struct qsfp *qsfp);
extern int sff8472_create_debugfs_files(struct qsfp *qsfp);

extern void lane_sm_event(struct lane *lane, u32 event);
extern void lane_sm_mod_remove(struct lane *lane);
extern void lane_sm_mod_next(struct lane *lane, u8 state);
extern void lane_sm_link_next(struct lane *lane, u8 state);
extern void lane_sm_link_upstream_linkdown(const struct lane *lane);

extern void *trx_ipc_log_buf;

#define TRX_IPC_LOG_PAGES 50

#define TRX_IPC_Log(buf, fmt, args...) \
do {\
    ipc_log_string((buf), fmt, ## args); \
} while (0)

#define TRX_LOG_INFO(p, fmt, args...) \
do {\
    dev_notice(p->dev, " %s: " fmt, __func__, ## args);\
    if (trx_ipc_log_buf) { \
        TRX_IPC_Log(trx_ipc_log_buf , " %s:%s: " fmt, dev_name(p->dev), __func__\
                                  , ## args); \
    } \
} while (0)

#define TRX_LOG_WARN(p, fmt, args...) \
do {\
    dev_warn(p->dev, " %s: " fmt, __func__, ## args);\
    if (trx_ipc_log_buf) { \
        TRX_IPC_Log(trx_ipc_log_buf , " %s:%s: " fmt, dev_name(p->dev), __func__\
                                  , ## args); \
    } \
} while (0)

#define TRX_LOG_ERR(p, fmt, args...) \
do {\
    dev_err(p->dev, " %s: " fmt, __func__, ## args);\
    if (trx_ipc_log_buf) { \
        TRX_IPC_Log(trx_ipc_log_buf , " ERR:%s:%s: " fmt, dev_name(p->dev), __func__\
                                  , ## args); \
    } \
} while (0)

#define TRX_LOG_INFO_NODEV(fmt, args...) \
do {\
    pr_notice("fpc-qsfp %s: " fmt, __func__, ## args);\
    if (trx_ipc_log_buf) { \
        TRX_IPC_Log(trx_ipc_log_buf , " %s: " fmt, __func__, ## args); \
    } \
} while (0)

#define TRX_LOG_ERR_NODEV(fmt, args...) \
do {\
    pr_err("fpc-qsfp %s: " fmt, __func__, ## args);\
    if (trx_ipc_log_buf) { \
        TRX_IPC_Log(trx_ipc_log_buf , " ERR:%s: " fmt, __func__, ## args); \
    } \
} while (0)

#endif
