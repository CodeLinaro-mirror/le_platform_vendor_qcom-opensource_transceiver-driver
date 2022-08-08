// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Code is derived from http://git.armlinux.org.uk/cgit/linux-arm.git/
 * tree/drivers/net/phy/qsfp.c?h=cex7 &
 * http://git.armlinux.org.uk/cgit/linux-arm.git/tree/drivers/
 * net/phy/sfp.c?h=cex7
 *
 */
#include "fpc.h"
#include "qsfp.h"
#include "transceiver_debugfs.h"

/* QSFP Device tree example
 *
 * qsfp_0: qsfp@0 {
 *    compatible = "sff,qsfp";
 *    fpc = <&fpc402_0>;
 *    port-num = <0>;
 *    i2c-address-device0 = <0x20>;
 *    i2c-address-device1 = <0x22>;
 *    maximum-power-milliwatt = <3500>;
 * };
 *
 */

static const char  * const mod_state_strings[] = {
    [QSFP_MOD_EMPTY] = "Empty",
    [QSFP_MOD_ERROR] = "ERROR",
    [QSFP_MOD_REJECT_SPEC] = "Reject_Spec",
    [QSFP_MOD_REJECT_PWR] = "Reject_Power",
    [QSFP_MOD_PROBE] = "Probe",
    [QSFP_MOD_WAITDEV] = "WaitDev",
    [QSFP_MOD_HPOWER] = "HPower",
    [QSFP_MOD_WAITPWR] = "WaitPwr",
    [QSFP_MOD_PRESENT] = "Present",
};

const char *mod_state_to_str(unsigned short mod_state)
{
    if (mod_state >= ARRAY_SIZE(mod_state_strings))
        return "Unknown module state";
    return mod_state_strings[mod_state];
}

static const char * const dev_state_strings[] = {
    [QSFP_DEV_DETACHED] = "Detached",
    [QSFP_DEV_DOWN] = "Down",
    [QSFP_DEV_UP] = "Up",
};

const char *dev_state_to_str(unsigned short dev_state)
{
    if (dev_state >= ARRAY_SIZE(dev_state_strings))
        return "Unknown device state";
    return dev_state_strings[dev_state];
}

static const char * const event_strings[] = {
    [QSFP_E_INSERT] = "Insert",
    [QSFP_E_REMOVE] = "Remove",
    [QSFP_E_DEV_ATTACH] = "Dev_attach",
    [QSFP_E_DEV_DETACH] = "Dev_detach",
    [QSFP_E_DEV_DOWN] = "Dev_down",
    [QSFP_E_DEV_UP] = "Dev_up",
    [QSFP_E_TX_FAULT] = "TX_Fault",
    [QSFP_E_TX_CLEAR] = "TX_Clear",
    [QSFP_E_LOS_HIGH] = "LOS_High",
    [QSFP_E_LOS_LOW] = "LOS_Low",
    [QSFP_E_TIMEOUT] = "Timeout",
};

static const char *event_to_str(unsigned short event)
{
    if (event >= ARRAY_SIZE(event_strings))
        return "Unknown event";
    return event_strings[event];
}

static const char * const sm_state_strings[] = {
    [QSFP_S_DOWN] = "Down",
    [QSFP_S_FAIL] = "FAIL",
    [QSFP_S_WAIT] = "Wait",
    [QSFP_S_INIT] = "Init",
    [QSFP_S_INIT_TX_FAULT] = "Init_TX_Fault",
    [QSFP_S_WAIT_LOS] = "Wait_LOS",
    [QSFP_S_LINK_UP] = "Link_Up",
    [QSFP_S_TX_FAULT] = "TX_Fault",
    [QSFP_S_REINIT] = "Reinit",
    [QSFP_S_TX_DISABLE] = "TX_Disable",
};

const char *sm_state_to_str(unsigned short sm_state)
{
    if (sm_state >= ARRAY_SIZE(sm_state_strings))
        return "Unknown state";
    return sm_state_strings[sm_state];
}

/*
 * dummy function not used by this driver
 */
static bool qsfp_module_supported(const struct qsfp_eeprom_id *id)
{
    return true;
}

const struct sff_data qsfp_data = {
    .module_supported = qsfp_module_supported,
};

static void qsfp_gpio_set_state(const struct qsfp *qsfp, u8 state)
{
/* Input GPIOs 'interrupt' and 'module present' cannot be set */
}

/*
 * Gets state of gpio connected from QSFP transceiver to FPC
 */
static u8 qsfp_gpio_get_state(const struct qsfp *qsfp)
{
    if (fpc_is_module_present(qsfp->fpc, qsfp->port_num)) {
        return QSFP_F_PRESENT;
    }

    return 0;
}

/*
 * Reads from QSFP memory map using i2c transaction by taking into account
 * page number as well.
 */
static int qsfp_i2c_read(const struct qsfp *qsfp, u8 page, u8 dev_addr,
                         void *buf, size_t len)
{
    struct i2c_msg msgs[2];
    size_t block_size = qsfp->i2c_block_size;
    size_t this_len;
    int ret;

    if ((dev_addr + len - 1) > QSFP_PAGE_OFFSET && !qsfp->module_flat_mem) {
        u8 page_buf[2];

        page_buf[0] = QSFP_PAGE_OFFSET;
        page_buf[1] = page;

        msgs[0].addr = qsfp->i2c_address_dev0;
        msgs[0].flags = 0;   // write
        msgs[0].len = sizeof(page_buf);
        msgs[0].buf = page_buf;

        ret = i2c_transfer(qsfp->i2c, msgs, 1);
        if (ret < 0)
            return ret;
    }

    msgs[0].addr = qsfp->i2c_address_dev0;
    msgs[0].flags = 0;     // write
    msgs[0].len = 1;
    msgs[0].buf = &dev_addr;
    msgs[1].addr = qsfp->i2c_address_dev0;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = len;
    msgs[1].buf = buf;

    while (len) {
        this_len = len;
        if (this_len > block_size)
            this_len = block_size;

        msgs[1].len = this_len;

        ret = i2c_transfer(qsfp->i2c, msgs, ARRAY_SIZE(msgs));
        if (ret < 0)
            return ret;

        if (ret != ARRAY_SIZE(msgs))
            return -EIO;

        msgs[1].buf += this_len;
        dev_addr += this_len;
        len -= this_len;
    }

    return 0;
}

/*
 * Writes into QSFP memory map using i2c transaction by taking into account
 * page number as well.
 */
static int qsfp_i2c_write(const struct qsfp *qsfp, u8 page, u8 dev_addr,
                          void *buf, size_t len)
{
    struct i2c_msg msgs[1];
    int ret;

    if ((dev_addr + len - 1) > QSFP_PAGE_OFFSET && !qsfp->module_flat_mem) {
        u8 page_buf[2];

        page_buf[0] = QSFP_PAGE_OFFSET;
        page_buf[1] = page;

        msgs[0].addr = qsfp->i2c_address_dev0;
        msgs[0].flags = 0;   // write
        msgs[0].len = sizeof(page_buf);
        msgs[0].buf = page_buf;

        ret = i2c_transfer(qsfp->i2c, msgs, 1);
        if (ret < 0)
            return ret;
    }

    msgs[0].addr = qsfp->i2c_address_dev0;
    msgs[0].flags = 0;
    msgs[0].len = 1 + len;
    msgs[0].buf = kmalloc(1 + len, GFP_KERNEL);
    if (!msgs[0].buf)
        return -ENOMEM;

    msgs[0].buf[0] = dev_addr;
    memcpy(&msgs[0].buf[1], buf, len);

    ret = i2c_transfer(qsfp->i2c, msgs, ARRAY_SIZE(msgs));

    kfree(msgs[0].buf);

    if (ret < 0)
        return ret;

    return ret == ARRAY_SIZE(msgs) ? 0 : -EIO;
}

static int qsfp_i2c_configure(struct qsfp *qsfp)
{
    struct i2c_adapter *i2c = qsfp->fpc->i2c;

    if (!i2c) {
        dev_err(qsfp->dev, "%s: FPC I2C adapter missing\n", __func__);
        return -ENODEV;
    }

    if (!i2c_check_functionality(i2c, I2C_FUNC_I2C)) {
        dev_err(qsfp->dev, "%s: I2C expected functionality "
                           "not supported\n", __func__);
        return -EINVAL;
    }

    qsfp->i2c = i2c;
    qsfp->read = qsfp_i2c_read;
    qsfp->write = qsfp_i2c_write;

    return 0;
}

int qsfp_read(const struct qsfp *qsfp, u16 addr, void *buf, size_t len)
{
    return qsfp->read(qsfp, addr >> 8, addr, buf, len);
}

int qsfp_write(const struct qsfp *qsfp, u16 addr, void *buf, size_t len)
{
    return qsfp->write(qsfp, addr >> 8, addr, buf, len);
}

static int qsfp_set_spec_ops(struct qsfp *qsfp)
{
    int ret;
    u8* spec_id;

    if (qsfp->spec_ops)
        return 0;

    ret = qsfp_read(qsfp, 0, &qsfp->id, 1);
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Failed to read spec id. ret %d\n",
                           __func__, ret);
        return -EAGAIN;
    }

    spec_id =(u8*)&qsfp->id;

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
         qsfp->spec_ops = &sff8636_spec_ops;
         dev_notice(qsfp->dev, "%s: SFF8636 spec id %02X\n",
                               __func__, *spec_id);
         break;
    default:
         dev_warn(qsfp->dev, "%s: Unsupported spec id %02X\n",
                           __func__,*spec_id);
         return -E_UNSUPPORTED_SPEC;
    }

    return 0;
}

static u8 qsfp_soft_get_state(struct qsfp *qsfp)
{
    /* if it is not qsfp interrupt then return */
    if (!fpc_is_qsfp_interrupt(qsfp)) {
        /* Preserve the current state */
        return qsfp->state;
    }

    if (qsfp_set_spec_ops(qsfp) < 0) {
        dev_err(qsfp->dev, "%s: Unable to set the spec ops\n", __func__);
        return qsfp->state;
    }

    return qsfp->spec_ops->soft_get_state(qsfp);
}

static char* qsfp_state_to_str(u8 state, char *str, int len)
{
    bool no_state = true;

    memset(str, 0, len);
    strlcpy(str, "[ ", len);

    if (state & QSFP_F_PRESENT) {
        strlcat(str, "PRESENT ", len);
        no_state = false;
    }
    if (state & QSFP_F_LOS) {
        strlcat(str, "LOS ", len);
        no_state = false;
    }
    if (state & QSFP_F_TX_FAULT) {
        strlcat(str, "TX_FAULT ", len);
        no_state = false;
    }
    if (state & QSFP_F_TX_DISABLE) {
        strlcat(str, "TX_DISABLE ", len);
        no_state = false;
    }

    if (no_state)
        strlcat(str, "Default ", len);

    strlcat(str, "]", len);

    return str;
}

/*
 * Only TX enable/disable handled by this function
 */
static void qsfp_soft_set_state(const struct qsfp *qsfp, u8 state)
{
    if (state & QSFP_F_TX_DISABLE)
        qsfp->spec_ops->tx_disable(qsfp);
    else
        qsfp->spec_ops->tx_enable(qsfp);

}

static u8 qsfp_get_state(struct qsfp *qsfp)
{
    u8 state = qsfp->get_state(qsfp);
    u8 soft_state;
    char state_str[QSFP_STATE_STR_MAX_LEN];

    soft_state = qsfp_soft_get_state(qsfp);

    if (state & QSFP_F_PRESENT)
        state |= soft_state;

    dev_notice(qsfp->dev, "%s: state 0x%X %s\n", __func__, state,
               qsfp_state_to_str(state, state_str, sizeof(state_str)));

    return state;
}

static void qsfp_set_state(const struct qsfp *qsfp, u8 state)
{
    char state_str[QSFP_STATE_STR_MAX_LEN];

    dev_notice(qsfp->dev, "%s: state 0x%X %s\n", __func__, state,
               qsfp_state_to_str(state, state_str, sizeof(state_str)));

    qsfp->set_state(qsfp, state);

    if (state & QSFP_F_PRESENT)
        qsfp_soft_set_state(qsfp, state);
}

u32 qsfp_check(void *buf, size_t len)
{
    u8 *p, check;

    for (p = buf, check = 0; len; p++, len--)
        check += *p;

    return check;
}

/* Helpers */
static void qsfp_module_tx_disable(struct qsfp *qsfp)
{
    dev_notice(qsfp->dev, "%s: TX Disable %u -> %u\n", __func__,
               qsfp->state & QSFP_F_TX_DISABLE ? 1 : 0, 1);
    qsfp->state |= QSFP_F_TX_DISABLE;
    qsfp_set_state(qsfp, qsfp->state);
}

static void qsfp_module_tx_enable(struct qsfp *qsfp)
{
    dev_notice(qsfp->dev, "%s: TX Enable %u -> %u\n", __func__,
               qsfp->state & QSFP_F_TX_DISABLE ? 1 : 0, 0);
    qsfp->state &= ~QSFP_F_TX_DISABLE;
    qsfp_set_state(qsfp, qsfp->state);
}

static void qsfp_module_tx_fault_reset(struct qsfp *qsfp)
{
    u8 state = qsfp->state;

    if (state & QSFP_F_TX_DISABLE)
        return;

    qsfp_set_state(qsfp, state | QSFP_F_TX_DISABLE);

    udelay(T_RESET_US);

    qsfp_set_state(qsfp, state);
}

/* QSFP state machine */
static void qsfp_sm_set_timer(struct qsfp *qsfp, u32 timeout)
{
    if (timeout)
        mod_delayed_work(system_power_efficient_wq, &qsfp->timeout,
                 timeout);
    else
        cancel_delayed_work(&qsfp->timeout);
}

static void qsfp_sm_next(struct qsfp *qsfp, u32 state,
            u32 timeout)
{
    qsfp->sm_state = state;
    qsfp_sm_set_timer(qsfp, timeout);
}

static void qsfp_sm_mod_next(struct qsfp *qsfp, u32 state,
                u32 timeout)
{
    qsfp->sm_mod_state = state;
    qsfp_sm_set_timer(qsfp, timeout);
}

static void qsfp_sm_link_up(struct qsfp *qsfp)
{
    sfp_link_up(qsfp->sfp_bus);
    dev_notice(qsfp->dev, "%s: sfp_link_up upstream ops called\n", __func__);
    fpc_qsfp_set_led(qsfp, QSFP_LED2, QSFP_LED_ON);
    qsfp_sm_next(qsfp, QSFP_S_LINK_UP, 0);
}

static void qsfp_sm_link_down(const struct qsfp *qsfp)
{
    sfp_link_down(qsfp->sfp_bus);
    dev_notice(qsfp->dev, "%s: sfp_link_down upstream ops called\n",
                           __func__);
    fpc_qsfp_set_led(qsfp, QSFP_LED2, QSFP_LED_OFF);
}

static void qsfp_sm_link_check_los(struct qsfp *qsfp)
{
    if (qsfp->state & QSFP_F_LOS)
        qsfp_sm_next(qsfp, QSFP_S_WAIT_LOS, 0);
    else
        qsfp_sm_link_up(qsfp);
}

static bool qsfp_los_event_active(const struct qsfp *qsfp, u32 event)
{
    if (event == QSFP_E_LOS_LOW)
        return false;

    return true;
}

static bool qsfp_los_event_inactive(const struct qsfp *qsfp, u32 event)
{
    if (event == QSFP_E_LOS_LOW)
        return true;

    return false;
}

static void qsfp_sm_fault(struct qsfp *qsfp, u32 next_state,
                          bool warn)
{
    if (qsfp->sm_fault_retries && !--qsfp->sm_fault_retries) {
        dev_err(qsfp->dev,"%s: Module persistently indicates Fault,"
                           "Disabling\n", __func__);
        qsfp_sm_next(qsfp, QSFP_S_TX_DISABLE, 0);
    } else {
        if (warn)
            dev_err(qsfp->dev, "%s: Module TX fault indicated\n",
                                __func__);

        qsfp_sm_next(qsfp, next_state, T_FAULT_RECOVER);
    }
}

/*
 * If 'enable' is true push the QSFP to its high power class
 * otherwise push the QSFP to low power class 1
 */
static int qsfp_sm_mod_hpower(const struct qsfp *qsfp, bool enable)
{
    int ret;

    /* Module high power is greater than allowed so do not allow
     * switching high power class
     */
    if (enable && qsfp->module_power_mW > qsfp->max_power_mW)
        ret = qsfp->spec_ops->handle_max_power_exceed(qsfp);
    else if (enable)
        ret = qsfp->spec_ops->mod_high_power(qsfp);
    else
        ret = qsfp->spec_ops->mod_low_power(qsfp);

    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Failed to set Power class: enable %u "
                           "ret %d\n",__func__, enable, ret);
        return -EAGAIN;
    }

    if (enable && qsfp->module_power_mW > qsfp->max_power_mW)
        dev_notice(qsfp->dev, "%s: Module high power is greater than max "
        "allowed so handled by moving to allowed lower power class\n",
        __func__);
    else if (enable)
        dev_notice(qsfp->dev, "%s: Module switched to High Power class\n",
                              __func__);
    else
        dev_notice(qsfp->dev, "%s: Module switched to Low Power class\n",
                              __func__);

    return 0;
}

static int qsfp_module_parse_power(struct qsfp *qsfp)
{
    int ret;

    ret = qsfp->spec_ops->module_parse_power(qsfp);
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: spec parse power failed\n", __func__);
        return ret;
    }

    dev_notice(qsfp->dev, "%s: Module Power class %u Power %u.%uW\n",
    __func__, qsfp->module_power_class, qsfp->module_power_mW / 1000,
    (qsfp->module_power_mW / 100) % 10);

    return 0;
}

static int qsfp_sm_mod_probe(struct qsfp *qsfp, bool report)
{
    int ret;

    ret = qsfp_set_spec_ops(qsfp);
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Unable to set spec ops. ret %d\n",
                           __func__, ret);
        return ret;
    }

    ret = qsfp->spec_ops->mod_probe(qsfp, report);
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: spec mod probe failed. ret %d\n",
                           __func__, ret);
        return ret;
    }

    qsfp->spec_ops->disable_redundant_irq(qsfp);

    /* TX disable when module inserted */
    qsfp_module_tx_disable(qsfp);

    qsfp->module_t_start_up = T_START_UP;

    /* Parse the module power requirement */
    ret = qsfp_module_parse_power(qsfp);
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: spec module parse power failed. "
                           "ret %d\n", __func__, ret);
        return ret;
    }

    if (qsfp->module_power_mW > qsfp->max_power_mW) {
        ret = qsfp->spec_ops->handle_max_power_exceed(qsfp);
        if (ret < 0) {
           dev_err(qsfp->dev, "%s: handle max power exceed failed. "
                              "ret %d\n", __func__, ret);
           return ret;
        }
    }

    ret = qsfp->spec_ops->check_features_impl(qsfp);
    if (ret < 0) {
        dev_warn(qsfp->dev, "%s: required features not implemented so rejecting"
                           " module. ret %d\n", __func__, ret);
        return ret;
    }

    qsfp->spec_ops->eeprom_print(qsfp);

    return 0;
}

static void qsfp_sm_mod_remove(struct qsfp *qsfp)
{
    if (qsfp->sm_mod_state > QSFP_MOD_WAITDEV) {
        sfp_module_remove(qsfp->sfp_bus);
        dev_notice(qsfp->dev, "%s: sfp_module_remove upstream ops called\n",
                               __func__);
    }

    fpc_qsfp_set_led(qsfp, QSFP_LED1 | QSFP_LED2, QSFP_LED_OFF);

    memset(&qsfp->id, 0, sizeof(qsfp->id));
    qsfp->module_power_mW = 0;
    qsfp->spec_ops = NULL;

    dev_notice(qsfp->dev, "%s: Module removed\n", __func__);
}

/* This state machine tracks the upstream's state */
static void qsfp_sm_device(struct qsfp *qsfp, u32 event)
{
    switch (qsfp->sm_dev_state) {
    default:
        if (event == QSFP_E_DEV_ATTACH)
            qsfp->sm_dev_state = QSFP_DEV_DOWN;
        break;

    case QSFP_DEV_DOWN:
        if (event == QSFP_E_DEV_DETACH)
            qsfp->sm_dev_state = QSFP_DEV_DETACHED;
        else if (event == QSFP_E_DEV_UP)
            qsfp->sm_dev_state = QSFP_DEV_UP;
        break;

    case QSFP_DEV_UP:
        if (event == QSFP_E_DEV_DETACH)
            qsfp->sm_dev_state = QSFP_DEV_DETACHED;
        else if (event == QSFP_E_DEV_DOWN)
            qsfp->sm_dev_state = QSFP_DEV_DOWN;
        break;
    }
}

/* This state machine tracks the insert/remove state of the module, probes
 * the on-board EEPROM, and sets up the power level.
 */
static void qsfp_sm_module(struct qsfp *qsfp, u32 event)
{
    int err;

    /* Handle remove event globally, it resets this state machine */
    if (event == QSFP_E_REMOVE) {
        if (qsfp->sm_mod_state > QSFP_MOD_PROBE)
            qsfp_sm_mod_remove(qsfp);
        qsfp_sm_mod_next(qsfp, QSFP_MOD_EMPTY, 0);
        return;
    }

    /* Handle device detach globally */
    if (qsfp->sm_dev_state < QSFP_DEV_DOWN &&
        qsfp->sm_mod_state > QSFP_MOD_WAITDEV) {
        if (qsfp->module_power_mW > 1000 &&
            qsfp->sm_mod_state > QSFP_MOD_HPOWER)
            qsfp_sm_mod_hpower(qsfp, false);

        qsfp_sm_mod_next(qsfp, QSFP_MOD_WAITDEV, 0);
        return;
    }

    switch (qsfp->sm_mod_state) {
    default:
        if (event == QSFP_E_INSERT) {
            qsfp_sm_mod_next(qsfp, QSFP_MOD_PROBE, T_SERIAL);
            qsfp->sm_mod_tries_init = R_PROBE_RETRY_INIT;
            qsfp->sm_mod_tries = R_PROBE_RETRY_SLOW;
        }
        break;

    case QSFP_MOD_PROBE:
        /* Wait for T_PROBE_INIT to time out */
        if (event != QSFP_E_TIMEOUT)
            break;

        err = qsfp_sm_mod_probe(qsfp, qsfp->sm_mod_tries == 1);
        if (err == -EAGAIN) {
            if (qsfp->sm_mod_tries_init &&
               --qsfp->sm_mod_tries_init) {
                qsfp_sm_set_timer(qsfp, T_PROBE_RETRY_INIT);
                break;
            } else if (qsfp->sm_mod_tries && --qsfp->sm_mod_tries) {
                if (qsfp->sm_mod_tries == R_PROBE_RETRY_SLOW - 1)
                    dev_warn(qsfp->dev,
                    "please wait, module slow to respond\n");
                qsfp_sm_set_timer(qsfp, T_PROBE_RETRY_SLOW);
                break;
            }
        }

        if (err == -E_UNSUPPORTED_SPEC) {
            qsfp_sm_mod_next(qsfp, QSFP_MOD_REJECT_SPEC, 0);
            fpc_qsfp_set_led(qsfp, QSFP_LED1 | QSFP_LED2, QSFP_LED_OFF);
            break;
        } else if (err == -E_MAX_POWER_EXCEED) {
            qsfp_sm_mod_next(qsfp, QSFP_MOD_REJECT_PWR, 0);
            fpc_qsfp_set_led(qsfp, QSFP_LED1 | QSFP_LED2, QSFP_LED_OFF);
            break;
        } else if (err < 0) {
            qsfp_sm_mod_next(qsfp, QSFP_MOD_ERROR, 0);
            fpc_qsfp_set_led(qsfp, QSFP_LED1 | QSFP_LED2, QSFP_LED_OFF);
            break;
        }

        qsfp_sm_mod_next(qsfp, QSFP_MOD_WAITDEV, 0);
        fallthrough;
    case QSFP_MOD_WAITDEV:
        /* Ensure that the device is attached before proceeding */
        if (qsfp->sm_dev_state < QSFP_DEV_DOWN)
            break;

        /* Report the module insertion to the upstream device */
        err = sfp_module_insert(qsfp->sfp_bus,
                       (const struct sfp_eeprom_id*)&qsfp->id);
        if (err < 0) {
            qsfp_sm_mod_next(qsfp, QSFP_MOD_ERROR, 0);
            break;
        } else {
            dev_notice(qsfp->dev, "%s: sfp_module_insert upstream ops "
                                  "called\n", __func__);
        }

        /* If this is a power level 1 module, we are done */
        if (qsfp->module_power_mW <= 1000)
            goto insert;

        qsfp_sm_mod_next(qsfp, QSFP_MOD_HPOWER, 0);
        fallthrough;
    case QSFP_MOD_HPOWER:
        /* Enable high power mode */
        err = qsfp_sm_mod_hpower(qsfp, true);
        if (err < 0) {
            if (err != -EAGAIN) {
                sfp_module_remove(qsfp->sfp_bus);
                dev_notice(qsfp->dev, "%s: sfp_module_remove upstream ops"
                                      " called\n", __func__);
                qsfp_sm_mod_next(qsfp, QSFP_MOD_ERROR, 0);
            } else {
                qsfp_sm_set_timer(qsfp, T_PROBE_RETRY_INIT);
            }
            break;
        }

        qsfp_sm_mod_next(qsfp, QSFP_MOD_WAITPWR, T_HPOWER_LEVEL);
        break;

    case QSFP_MOD_WAITPWR:
        /* Wait for T_HPOWER_LEVEL to time out */
        if (event != QSFP_E_TIMEOUT)
            break;

    insert:
        qsfp_sm_mod_next(qsfp, QSFP_MOD_PRESENT, 0);
        fpc_qsfp_set_led(qsfp, QSFP_LED1, QSFP_LED_ON);
        break;

    case QSFP_MOD_PRESENT:
    case QSFP_MOD_REJECT_SPEC:
    case QSFP_MOD_REJECT_PWR:
    case QSFP_MOD_ERROR:
         break;
    }
}

static void qsfp_sm_main(struct qsfp *qsfp, u32 event)
{
    unsigned long timeout;

    /* Some events are global */
    if (qsfp->sm_state != QSFP_S_DOWN &&
        (qsfp->sm_mod_state != QSFP_MOD_PRESENT ||
         qsfp->sm_dev_state != QSFP_DEV_UP)) {
        if (qsfp->sm_state == QSFP_S_LINK_UP &&
            qsfp->sm_dev_state == QSFP_DEV_UP)
            qsfp_sm_link_down(qsfp);
        /* TX disable is not needed as it is not done through gpio */
        //qsfp_module_tx_disable(qsfp);
        qsfp_sm_next(qsfp, QSFP_S_DOWN, 0);
        return;
    }

    /* The main state machine */
    switch (qsfp->sm_state) {
    case QSFP_S_DOWN:
        if (qsfp->sm_mod_state != QSFP_MOD_PRESENT ||
            qsfp->sm_dev_state != QSFP_DEV_UP)
            break;

        qsfp_module_tx_enable(qsfp);

        /* Initialise the fault clearance retries */
        qsfp->sm_fault_retries = N_FAULT_INIT;

        /* We need to check the TX_FAULT state, which is not defined
         * while TX_DISABLE is asserted. The earliest we want to do
         * anything (such as probe for a PHY) is 50ms.
         */
        qsfp_sm_next(qsfp, QSFP_S_WAIT, T_WAIT);
        break;

    case QSFP_S_WAIT:
        if (event != QSFP_E_TIMEOUT)
            break;

        if (qsfp->state & QSFP_F_TX_FAULT) {
            /* Wait up to t_init (SFF-8472) or t_start_up (SFF-8431)
             * from the TX_DISABLE deassertion for the module to
             * initialise, which is indicated by TX_FAULT
             * deasserting.
             */
            timeout = qsfp->module_t_start_up;
            if (timeout > T_WAIT)
                timeout -= T_WAIT;
            else
                timeout = 1;

            qsfp_sm_next(qsfp, QSFP_S_INIT, timeout);
        } else {
            /* TX_FAULT is not asserted, assume the module has
             * finished initialising.
             */
            goto init_done;
        }
        break;

    case QSFP_S_INIT:
        if (event == QSFP_E_TIMEOUT && qsfp->state & QSFP_F_TX_FAULT) {
            /* TX_FAULT is still asserted after t_init
             * or t_start_up, so assume there is a fault.
             */
            qsfp_sm_fault(qsfp, QSFP_S_INIT_TX_FAULT,
                     qsfp->sm_fault_retries == N_FAULT_INIT);
        } else if (event == QSFP_E_TIMEOUT || event == QSFP_E_TX_CLEAR) {
    init_done:
            goto phy_probe;
        }
        break;

    phy_probe:
        /* TX_FAULT deasserted or we timed out with TX_FAULT
         * clear. Check the LOS state.
         */
        qsfp_sm_link_check_los(qsfp);

        /* Reset the fault retry count */
        qsfp->sm_fault_retries = N_FAULT;
        break;

    case QSFP_S_INIT_TX_FAULT:
        if (event == QSFP_E_TIMEOUT) {
            qsfp_module_tx_fault_reset(qsfp);
            qsfp_sm_next(qsfp, QSFP_S_INIT, qsfp->module_t_start_up);
        }
        break;

    case QSFP_S_WAIT_LOS:
        if (event == QSFP_E_TX_FAULT)
            qsfp_sm_fault(qsfp, QSFP_S_TX_FAULT, true);
        else if (qsfp_los_event_inactive(qsfp, event))
            qsfp_sm_link_up(qsfp);
        break;

    case QSFP_S_LINK_UP:
        if (event == QSFP_E_TX_FAULT) {
            qsfp_sm_link_down(qsfp);
            qsfp_sm_fault(qsfp, QSFP_S_TX_FAULT, true);
        } else if (qsfp_los_event_active(qsfp, event)) {
            qsfp_sm_link_down(qsfp);
            qsfp_sm_next(qsfp, QSFP_S_WAIT_LOS, 0);
        }
        break;

    case QSFP_S_TX_FAULT:
        if (event == QSFP_E_TIMEOUT) {
            qsfp_module_tx_fault_reset(qsfp);
            qsfp_sm_next(qsfp, QSFP_S_REINIT, qsfp->module_t_start_up);
        }
        break;

    case QSFP_S_REINIT:
        if (event == QSFP_E_TIMEOUT && qsfp->state & QSFP_F_TX_FAULT) {
            qsfp_sm_fault(qsfp, QSFP_S_TX_FAULT, false);
        } else if (event == QSFP_E_TIMEOUT || event == QSFP_E_TX_CLEAR) {
            dev_notice(qsfp->dev, "%s: Module TX Fault recovered\n",
                           __func__);
            qsfp_sm_link_check_los(qsfp);
        }
        break;

    case QSFP_S_TX_DISABLE:
        break;
    }
}

static void qsfp_sm_event(struct qsfp *qsfp, u32 event)
{
    mutex_lock(&qsfp->sm_mutex);

    dev_notice(qsfp->dev, "%s: Enter [%7s:%8s:%8s]   Event: %s\n", __func__,
                          mod_state_to_str(qsfp->sm_mod_state),
                          dev_state_to_str(qsfp->sm_dev_state),
                          sm_state_to_str(qsfp->sm_state),
                          event_to_str(event));

    qsfp_sm_device(qsfp, event);
    qsfp_sm_module(qsfp, event);
    qsfp_sm_main(qsfp, event);

    dev_notice(qsfp->dev, "%s: Exit  [%7s:%8s:%8s]\n", __func__,
                          mod_state_to_str(qsfp->sm_mod_state),
                          dev_state_to_str(qsfp->sm_dev_state),
                          sm_state_to_str(qsfp->sm_state));

    mutex_unlock(&qsfp->sm_mutex);
}

static void qsfp_attach(struct sfp *sfp)
{
    struct qsfp *qsfp = (struct qsfp*)sfp;

    qsfp_sm_event(qsfp, QSFP_E_DEV_ATTACH);
}

static void qsfp_detach(struct sfp *sfp)
{
    struct qsfp *qsfp = (struct qsfp*)sfp;

    qsfp_sm_event(qsfp, QSFP_E_DEV_DETACH);
}

static void qsfp_start(struct sfp *sfp)
{
    struct qsfp *qsfp = (struct qsfp*)sfp;

    qsfp_sm_event(qsfp, QSFP_E_DEV_UP);
}

static void qsfp_stop(struct sfp *sfp)
{
    struct qsfp *qsfp = (struct qsfp*)sfp;

    qsfp_sm_event(qsfp, QSFP_E_DEV_DOWN);
}

static int qsfp_module_info(struct sfp *sfp, struct ethtool_modinfo *modinfo)
{
    struct qsfp *qsfp = (struct qsfp*)sfp;

    dev_notice(qsfp->dev, "%s:", __func__);

    return qsfp->spec_ops->module_info(qsfp, modinfo);
}

static int qsfp_module_eeprom(struct sfp *sfp, struct ethtool_eeprom *ee,
                 u8 *data)
{
    int ret;
    struct qsfp *qsfp = (struct qsfp*)sfp;

    dev_notice(qsfp->dev, "%s: offset %u length %u\n", __func__,
                          ee->offset, ee->len);

    if (ee->len == 0)
        return -EINVAL;

    ret = qsfp_read(qsfp, ee->offset, data, ee->len);
    if (ret < 0)
        dev_err(qsfp->dev, "%s: Fail to read EEPROM from offset %u "
                "length %u. ret %d\n", __func__, ee->offset, ee->len, ret);

    return ret;
}

static int qsfp_module_eeprom_by_page(struct sfp *sfp,
                     const struct ethtool_module_eeprom *page,
                     struct netlink_ext_ack *extack)
{
    int ret;
    struct qsfp *qsfp = (struct qsfp*)sfp;

    dev_notice(qsfp->dev, "%s: bank %u page %u offset %u length %u\n",
               __func__, page->bank, page->page, page->offset, page->length);

    if (page->bank) {
        dev_err(qsfp->dev, "%s: Banks not supported", __func__);
        NL_SET_ERR_MSG(extack, "Banks not supported");
        return -EOPNOTSUPP;
    }

    if (page->i2c_address != 0x50) {
        dev_err(qsfp->dev, "%s: I2C address 0x%X not supported\
                Only address 0x50 supported", __func__, page->i2c_address);
        NL_SET_ERR_MSG(extack, "Only address 0x50 supported");
        return -EOPNOTSUPP;
    }

    ret = qsfp_read(qsfp, (page->page << 8) | page->offset,
            page->data, page->length);
    if (ret < 0)
        dev_err(qsfp->dev, "%s: Fail to read EEPROM from page %u offset %u "
                "length %u. ret %d\n", __func__, page->page, page->offset,
                page->length, ret);

    return ret;

};

static const struct sfp_socket_ops qsfp_module_ops = {
    .attach = qsfp_attach,
    .detach = qsfp_detach,
    .start = qsfp_start,
    .stop = qsfp_stop,
    .module_info = qsfp_module_info,
    .module_eeprom = qsfp_module_eeprom,
    .module_eeprom_by_page = qsfp_module_eeprom_by_page,
};

static void qsfp_timeout(struct work_struct *work)
{
    struct qsfp *qsfp = container_of(work, struct qsfp, timeout.work);

    rtnl_lock();
    qsfp_sm_event(qsfp, QSFP_E_TIMEOUT);
    rtnl_unlock();
}

void qsfp_check_state(struct qsfp *qsfp)
{
    u8 state, changed;
    char cur_state_str[QSFP_STATE_STR_MAX_LEN];
    char next_state_str[QSFP_STATE_STR_MAX_LEN];
    char changed_state_str[QSFP_STATE_STR_MAX_LEN];
    u32 presence_event = QSFP_E_INSERT;

    if (!qsfp)
        return;

    mutex_lock(&qsfp->st_mutex);

    state = qsfp_get_state(qsfp);
    changed = state ^ qsfp->state;
    changed &= QSFP_F_PRESENT | QSFP_F_LOS | QSFP_F_TX_FAULT;

    dev_notice(qsfp->dev, "%s: Current state %s 0x%X, Next state %s 0x%X, "
    "Changed state to be processed %s 0x%X\n",__func__,
    qsfp_state_to_str(qsfp->state, cur_state_str, sizeof(cur_state_str)),
    qsfp->state,
    qsfp_state_to_str(state, next_state_str, sizeof(next_state_str)),
    state,
    qsfp_state_to_str(changed, changed_state_str, sizeof(changed_state_str)),
    changed);

    state |= qsfp->state & QSFP_F_TX_DISABLE;
    qsfp->state = state;

    rtnl_lock();

    if (changed & QSFP_F_PRESENT) {
        presence_event = state & QSFP_F_PRESENT ? QSFP_E_INSERT :
                         QSFP_E_REMOVE;
        qsfp_sm_event(qsfp, presence_event);
    }

    /* if it is remove module event then no need to process LOS
     * and TX Fault events
     */
    if (presence_event != QSFP_E_REMOVE) {
        if (changed & QSFP_F_TX_FAULT)
            qsfp_sm_event(qsfp, state & QSFP_F_TX_FAULT ?
                          QSFP_E_TX_FAULT : QSFP_E_TX_CLEAR);

        if (changed & QSFP_F_LOS)
            qsfp_sm_event(qsfp, state & QSFP_F_LOS ?
                          QSFP_E_LOS_HIGH : QSFP_E_LOS_LOW);
    }

    rtnl_unlock();

    mutex_unlock(&qsfp->st_mutex);
}

static struct qsfp *qsfp_alloc(struct device *dev)
{
    struct qsfp *qsfp;

    qsfp = kzalloc(sizeof(*qsfp), GFP_KERNEL);
    if (!qsfp)
        return ERR_PTR(-ENOMEM);

    qsfp->dev = dev;

    mutex_init(&qsfp->sm_mutex);
    mutex_init(&qsfp->st_mutex);
    INIT_DELAYED_WORK(&qsfp->timeout, qsfp_timeout);

    /* valid port numbers are 0,1,2,3.
     * FPC_MAX_PORTS signifies invalid port number
     * */
    qsfp->port_num = FPC_MAX_PORTS;

    /* Some SFP modules and also some Linux I2C drivers do not like reads
     * longer than 16 bytes, so read the EEPROM in chunks of 16 bytes at
     * a time.
     */
    qsfp->i2c_block_size = 16;

    return qsfp;
}

static void qsfp_cleanup(void *data)
{
    struct qsfp *qsfp = data;

    cancel_delayed_work_sync(&qsfp->timeout);

    kfree(qsfp);
}

/*
 * Probe function which processes QSFP device node
 * it reads i2c address of device0 and device1 to be configured
 * handle to parent FPC ,port number and max power
 */
int qsfp_probe(struct platform_device *pdev)
{
    struct device_node *node = pdev->dev.of_node;
    struct device_node *fpc_node;
    struct platform_device *fpc_pdev;
    const struct of_device_id *id;
    struct qsfp *qsfp;
    u32 fpc_handle, temp;
    int ret;

    qsfp = qsfp_alloc(&pdev->dev);
    if (IS_ERR(qsfp)) {
        dev_err(&pdev->dev, "%s: qsfp_alloc failed\n", __func__);
        return PTR_ERR(qsfp);
    }

    platform_set_drvdata(pdev, qsfp);

    ret = devm_add_action(qsfp->dev, qsfp_cleanup, qsfp);
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: devm_add_action failed. ret %d\n",
                           __func__, ret);
        qsfp_cleanup(qsfp);
        return ret;
    }

    if (!pdev->dev.of_node) {
        dev_err(qsfp->dev, "%s: dev node not found\n", __func__);
        return -EINVAL;
    }

    id = of_match_node(fpc_qsfp_of_match, node);
    if (WARN_ON(!id)) {
        dev_err(qsfp->dev, "%s: Node match id not found\n", __func__);
        return -EINVAL;
    }

    qsfp->type = &qsfp_data;

    ret = of_property_read_u32(node, "fpc", &fpc_handle);
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Unable to read property 'fpc'. ret %d\n",
                    __func__, ret);
        return ret;
    }

    fpc_node = of_find_node_by_phandle(fpc_handle);
    if (!fpc_node) {
        dev_err(qsfp->dev, "%s: Unable to find FPC node\n", __func__);
        return -EPROBE_DEFER;
    }

    fpc_pdev = of_find_device_by_node(fpc_node);
    if (!fpc_pdev) {
        dev_err(qsfp->dev, "%s: Unable to get FPC pdev\n", __func__);
        return -EPROBE_DEFER;
    }

    qsfp->fpc = platform_get_drvdata(fpc_pdev);
    if (!qsfp->fpc) {
        dev_info(qsfp->dev, "%s: Unable to get FPC handler\n", __func__);
        return -EPROBE_DEFER;
    }

    ret = qsfp_i2c_configure(qsfp);
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: I2C configuration failed. ret %d\n",
                            __func__, ret);
        return ret;
    }

    qsfp->get_state = qsfp_gpio_get_state;
    qsfp->set_state = qsfp_gpio_set_state;

    ret = device_property_read_u32(&pdev->dev, "port-num", &temp);
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Fail to get 'port-num' property. "
                           "ret %d\n", __func__, ret);
        return ret;
    }
    qsfp->port_num = temp & 0xFF;

    dev_notice(qsfp->dev, "%s: port_num %u\n", __func__, qsfp->port_num);

    if (qsfp->port_num < 0 || qsfp->port_num >= FPC_MAX_PORTS) {
        dev_err(qsfp->dev, "%s: port_num %0X is Invalid\n", __func__,
                           qsfp->port_num);
        return -EINVAL;
    }

    ret = device_property_read_u32(&pdev->dev, "maximum-power-milliwatt",
                                   &qsfp->max_power_mW);
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Fail to get 'maximum-power-milliwatt' "
                           "property. ret %d\n", __func__, ret);
        return ret;
    }
    dev_notice(qsfp->dev, "%s: Maximum-power-milliwatt %u\n", __func__,
                          qsfp->max_power_mW);

    dev_notice(qsfp->dev, "%s: Host maximum power %u.%uW\n", __func__,
               qsfp->max_power_mW / 1000, (qsfp->max_power_mW / 100) % 10);

    ret = device_property_read_u32(&pdev->dev, "i2c-address-device0",
                                   &temp);
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Fail to get 'i2c-address-device0' property. "
                           "ret %d\n", __func__, ret);
        return ret;
    }
    qsfp->i2c_address_dev0 = temp & 0xFF;

    dev_notice(qsfp->dev, "%s: I2C-address-device0  0x%02X (0x%02X)\n",
     __func__, qsfp->i2c_address_dev0, qsfp->i2c_address_dev0 >> 1);

    ret = device_property_read_u32(&pdev->dev, "i2c-address-device1",
                                   &temp);
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Fail to get 'i2c-address-device1' property. "
                           "ret %d\n", __func__, ret);
        return ret;
    }
    qsfp->i2c_address_dev1 = temp & 0xFF;
    dev_notice(qsfp->dev, "%s: I2C-address-device1  0x%02X (0x%02X)\n",
     __func__, qsfp->i2c_address_dev1, qsfp->i2c_address_dev1 >> 1);

    /* Convert 8-bit to 7-bit i2c address
     * In HW spec they mention i2c address in 8-bit form with last bit 0
     * but in actual i2c_transfer we have to use 7-bit i2c address so
     * it need right shift by one position.
     */
    qsfp->i2c_address_dev0 >>= 1;
    qsfp->i2c_address_dev1 >>= 1;

    if (qsfp->fpc->qsfp[qsfp->port_num]) {
        dev_err(qsfp->dev, "%s: QSFP port number %u already used",
                           __func__, qsfp->port_num);
        return -EINVAL;
    } else {
        qsfp->fpc->qsfp[qsfp->port_num] = qsfp;
    }

    ret = fpc_read_agr_interrupt_input_status(qsfp->fpc);
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Fail to read fpc interrupt input status. "
                           "ret %d\n", __func__, ret);
        return -EPROBE_DEFER;
    } else {
        qsfp->state = qsfp_get_state(qsfp);

        if (qsfp->state & QSFP_F_PRESENT) {
            dev_notice(qsfp->dev, "%s: QSFP present during probe\n", __func__);
            rtnl_lock();
            qsfp_sm_event(qsfp, QSFP_E_INSERT);
            rtnl_unlock();
        }
    }

    ret = fpc_enable_qsfp_interrupt(qsfp);
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Enable QSFP interrupt failed. ret %d\n",
                           __func__, ret);
        return -EPROBE_DEFER;
    }

    qsfp->sfp_bus = sfp_register_socket(qsfp->dev, (struct sfp*)qsfp,
                                        &qsfp_module_ops);
    if (!qsfp->sfp_bus) {
        dev_err(qsfp->dev, "%s: Socket register failed\n", __func__);
        return -ENOMEM;
    }

    qsfp_debugfs_init(qsfp);

    fpc_qsfp_set_led(qsfp, QSFP_LED1 | QSFP_LED2, QSFP_LED_OFF);

    return 0;
}

int qsfp_remove(struct platform_device *pdev)
{
    struct qsfp *qsfp = platform_get_drvdata(pdev);

    sfp_unregister_socket(qsfp->sfp_bus);

    rtnl_lock();
    qsfp_sm_event(qsfp, QSFP_E_REMOVE);
    rtnl_unlock();

    qsfp_debugfs_exit(qsfp);

    return 0;
}

void qsfp_shutdown(struct platform_device *pdev)
{
    struct qsfp *qsfp = platform_get_drvdata(pdev);

    cancel_delayed_work_sync(&qsfp->timeout);
}
