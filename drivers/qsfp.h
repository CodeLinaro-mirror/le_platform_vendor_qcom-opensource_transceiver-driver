/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "sfp.h"
#include "sff8636.h"

#define QSFP_COMPATIBLE "sff,qsfp"

#define QSFP_PAGE_OFFSET (0x7F)
#define QSFP_LED1 (BIT(0))
#define QSFP_LED2 (BIT(1))
#define QSFP_LED_ON  (true)
#define QSFP_LED_OFF (false)

struct qsfp_eeprom_id {
    union {
        struct sff8636_eeprom_id sff8636;
        //struct cmis_eeprom_id cmis;
    };
};

struct qsfp {
    struct device *dev;
    struct fpc *fpc;
    struct sfp_bus *sfp_bus;
    struct i2c_adapter *i2c;
    u32 max_power_mW;
    u32 module_power_mW;
    u32 module_t_start_up;
    u8 state;
    u8 port_num;
    u8 i2c_address_dev0;
    u8 i2c_address_dev1;
    bool module_flat_mem;
    bool prefetch;
    u8 features;
    u8 module_power_class;
    u8 module_revision;
    u8 sm_mod_state;
    u8 sm_mod_tries_init;
    u8 sm_mod_tries;
    u8 sm_dev_state;
    u8 sm_fault_retries;
    unsigned short sm_state;
    size_t i2c_block_size;

    int (*read)(const struct qsfp *, u8, u8, void *, size_t);
    int (*write)(const struct qsfp *, u8, u8, void *, size_t);

    struct delayed_work timeout;
    struct mutex sm_mutex;            /* Protects state machine */

    struct qsfp_eeprom_id id;
    struct qsfp_spec_ops *spec_ops;

#if IS_ENABLED(CONFIG_DEBUG_FS)
    struct dentry *debugfs_dir;
#endif

};

struct qsfp_spec_ops {
    /* called during module insert to read EEPROM */
    int (*mod_probe)(struct qsfp *qsfp, bool report);
    /* Disable uninterested interrupts */
    void (*disable_redundant_irq)(const struct qsfp *qsfp);
    /* Gets module current state LOS,TX Fault */
    u8 (*get_state)(struct qsfp *qsfp);
    /* Enable TX */
    void (*tx_enable)(const struct qsfp *qsfp);
    /* Disable TX */
    void (*tx_disable)(const struct qsfp *qsfp);
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
    /* Starts FPC402 prefetch from irq status offset */
    void (*irq_status_prefetch_start)(const struct qsfp *qsfp);
    /* Gets connector type */
    u8 (*get_connector_type)(const struct qsfp *qsfp);
};

enum {
    QSFP_F_PRESENT      = BIT(0),
    QSFP_F_LOS          = BIT(1),
    QSFP_F_TX_FAULT     = BIT(2),
    QSFP_F_TX_DISABLE   = BIT(3),

    QSFP_E_INSERT = 0,
    QSFP_E_REMOVE,
    QSFP_E_DEV_ATTACH,
    QSFP_E_DEV_DETACH,
    QSFP_E_DEV_DOWN,
    QSFP_E_DEV_UP,
    QSFP_E_TX_FAULT,
    QSFP_E_TX_CLEAR,
    QSFP_E_LOS_HIGH,
    QSFP_E_LOS_LOW,
    QSFP_E_TIMEOUT,

    QSFP_MOD_EMPTY = 0,
    QSFP_MOD_ERROR,
    QSFP_MOD_REJECT_SPEC,
    QSFP_MOD_REJECT_PWR,
    QSFP_MOD_PROBE,
    QSFP_MOD_WAITDEV,
    QSFP_MOD_HPOWER,
    QSFP_MOD_WAITPWR,
    QSFP_MOD_PRESENT,

    QSFP_DEV_DETACHED = 0,
    QSFP_DEV_DOWN,
    QSFP_DEV_UP,

    QSFP_S_DOWN = 0,
    QSFP_S_FAIL,
    QSFP_S_WAIT,
    QSFP_S_INIT,
    QSFP_S_INIT_TX_FAULT,
    QSFP_S_WAIT_LOS,
    QSFP_S_LINK_UP,
    QSFP_S_TX_FAULT,
    QSFP_S_REINIT,
    QSFP_S_TX_DISABLE,
};

enum {
    E_UNSUPPORTED_SPEC = 1000,
    E_MAX_POWER_EXCEED,
};

/* t_start_up (SFF-8431) or t_init (SFF-8472) is the time required for a
 * non-cooled module to initialise its laser safety circuitry. We wait
 * an initial T_WAIT period before we check the tx fault to give any PHY
 * on board (for a copper SFP) time to initialise.
 */
#define T_WAIT            msecs_to_jiffies(50)
#define T_START_UP        msecs_to_jiffies(300)

/* t_reset is the time required to assert the TX_DISABLE signal to reset
 * an indicated TX_FAULT.
 */
#define T_RESET_US        10
#define T_FAULT_RECOVER        msecs_to_jiffies(1000)

/* N_FAULT_INIT is the number of recovery attempts at module initialisation
 * time. If the TX_FAULT signal is not deasserted after this number of
 * attempts at clearing it, we decide that the module is faulty.
 * N_FAULT is the same but after the module has initialised.
 */
#define N_FAULT_INIT        5
#define N_FAULT             5

/* T_PHY_RETRY is the time interval between attempts to probe the PHY.
 */
#define T_PHY_RETRY        msecs_to_jiffies(50)

/* SFP module presence detection is poor: the three MOD DEF signals are
 * the same length on the PCB, which means it's possible for MOD DEF 0 to
 * connect before the I2C bus on MOD DEF 1/2.
 *
 * The SFF-8472 specifies t_serial ("Time from power on until module is
 * ready for data transmission over the two wire serial bus.") as 300ms.
 */
#define T_SERIAL              msecs_to_jiffies(300)
#define T_HPOWER_LEVEL        msecs_to_jiffies(300)
#define T_PROBE_RETRY_INIT    msecs_to_jiffies(100)
#define R_PROBE_RETRY_INIT    10
#define T_PROBE_RETRY_SLOW    msecs_to_jiffies(5000)
#define R_PROBE_RETRY_SLOW    12

#define QSFP_STATE_STR_MAX_LEN (50)

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

extern const struct of_device_id fpc_qsfp_of_match[];
extern struct qsfp_spec_ops sff8636_spec_ops;

extern int fpc_is_module_present(const struct qsfp *qsfp);
extern int fpc_enable_qsfp_interrupt(const struct qsfp *qsfp);
extern void fpc_qsfp_set_led(const struct qsfp *qsfp, u8 led, bool state);
extern int fpc_data_prefetch_start(const struct qsfp *qsfp, u8 device,
                                   u8 offset, u8 len, u8 period);
extern int fpc_data_prefetch_stop(const struct qsfp *qsfp);

extern int qsfp_read(const struct qsfp *qsfp, u16 addr, void *buf, size_t len);
extern int qsfp_write(const struct qsfp *qsfp, u16 addr, void *buf, size_t len);
extern u32 qsfp_check(void *buf, size_t len);

#endif
