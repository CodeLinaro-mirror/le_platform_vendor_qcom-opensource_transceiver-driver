// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include "fpc.h"
#include "qsfp.h"
#include "lane.h"
#include "transceiver_debugfs.h"
#include "trx_sysfs.h"

struct fpc *fpc_global[FPC_MAX_INSTANCES];
static bool sysfs_update_flag = false;
static struct timer_list fpc_probe_timer;

const u8 FPC_PORT_REG[][FPC_MAX_PORTS] = {
    /* FPC_LED_MODE_SELECT */
    {0x1A , 0x3A , 0x5A , 0x7A},
    /* FPC_INPUT_PIN_INTERRUPT_ENABLE */
    {0x20 , 0x40 , 0x60 , 0x80},
    /* FPC_INPUT_PIN_INTERRUPT_STATUS */
    {0x21 , 0x41 , 0x61 , 0x81},
};

const struct of_device_id fpc_qsfp_of_match[] = {
    { .compatible = FPC_COMPATIBLE },
    { .compatible = QSFP_COMPATIBLE },
    { .compatible = LANE_COMPATIBLE },
    { },
};
MODULE_DEVICE_TABLE(of, fpc_qsfp_of_match);

struct class *uevent_tcvr_class = NULL;
struct device *uevent_tcvr_device = NULL;

void *trx_ipc_log_buf = NULL;
/* Flag to transmit kobject uevents for logging to userspace */
u8 is_qxdm_log_en = 0;
#define EVENT_QXDM_SYSFS_CFG_CREATE  "EVENT=EVENT_QXDM_SYSFS_CFG_CREATE"

struct kobject *transceiver_kobj = NULL;
LIST_HEAD(dual_tcvr_list);

/* fpc_write_nolock needed in case of qsfp i2c bus recovery as caller has already taken i2c bus lock */
static int fpc_write_nolock(const struct fpc *fpc, u8 dev_addr, void *buf, size_t len)
{
    struct i2c_msg msgs[1];
    u8 bus_addr;
    int ret;

    if (!fpc) {
        TRX_LOG_ERR_NODEV("fpc is NULL");
        return -EINVAL;
    }

    bus_addr = fpc->i2c_address;

    msgs[0].addr = bus_addr;
    msgs[0].flags = 0;
    msgs[0].len = 1 + len;
    msgs[0].buf = kmalloc(1 + len, GFP_KERNEL);
    if (!msgs[0].buf) {
        return -ENOMEM;
    }

    msgs[0].buf[0] = dev_addr;
    memcpy(&msgs[0].buf[1], buf, len);

    ret = __i2c_transfer(fpc->i2c, msgs, ARRAY_SIZE(msgs));

    kfree(msgs[0].buf);

    if (ret < 0) {
        return ret;
    }

    return ret == ARRAY_SIZE(msgs) ? 0 : -EIO;
}

/*
 * Reads FPC402 register memory map using i2c transaction
 * returns 0 on successful read of 'len' bytes otherwise error
 */
int fpc_read(const struct fpc *fpc, u8 dev_addr, void *buf, size_t len)
{
    struct i2c_msg msgs[2];
    u8 bus_addr;
    int ret;

    if (!fpc) {
        TRX_LOG_ERR_NODEV("fpc is NULL");
        return -EINVAL;
    }

    bus_addr = fpc->i2c_address;

    msgs[0].addr = bus_addr;
    msgs[0].flags = 0;    // write
    msgs[0].len = 1;
    msgs[0].buf = &dev_addr;

    msgs[1].addr = bus_addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = len;
    msgs[1].buf = buf;

    ret = i2c_transfer(fpc->i2c, msgs, ARRAY_SIZE(msgs));
    if (ret < 0) {
        return ret;
    }

    return ret == ARRAY_SIZE(msgs) ? 0 : -EIO;
}

/*
 * Writes FPC402 register memory map using i2c transaction
 * returns 0 on successful write of 'len' bytes otherwise error
 */
int fpc_write(const struct fpc *fpc, u8 dev_addr, void *buf, size_t len)
{
    struct i2c_msg msgs[1];
    u8 bus_addr;
    int ret;

    if (!fpc) {
        TRX_LOG_ERR_NODEV("fpc is NULL");
        return -EINVAL;
    }

    bus_addr = fpc->i2c_address;

    msgs[0].addr = bus_addr;
    msgs[0].flags = 0;
    msgs[0].len = 1 + len;
    msgs[0].buf = kmalloc(1 + len, GFP_KERNEL);
    if (!msgs[0].buf) {
        return -ENOMEM;
    }

    msgs[0].buf[0] = dev_addr;
    memcpy(&msgs[0].buf[1], buf, len);

    ret = i2c_transfer(fpc->i2c, msgs, ARRAY_SIZE(msgs));

    kfree(msgs[0].buf);

    if (ret < 0) {
        return ret;
    }

    return ret == ARRAY_SIZE(msgs) ? 0 : -EIO;
}

/*
 * Checks whether transceiver module is present or not
 */
int fpc_is_module_present(const struct qsfp *qsfp)
{
    int ret;
    u8 mod_present = 0;

    ret = fpc_read(qsfp->fpc, FPC_IN_B_STATUS_REGISTER, &mod_present,
                   sizeof(mod_present));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Fail to read ModulePresent GPIO status");
        return ret;
    }

    if ((mod_present >> qsfp->port_num) & 1) {
        /* active low. Bit will be 0 when active */
        return QSFP_NOT_PRESENT;
    }

    return QSFP_PRESENT;
}

static int fpc_read_qsfp_irq_status(struct fpc *fpc)
{
    u8 buf = 0;
    int ret;
    u8 i;

    for (i = 0 ; i < FPC_MAX_PORTS ; i++) {
        buf = 0;
        ret = fpc_read(fpc, FPC_PORT_REG[FPC_INPUT_PIN_INTERRUPT_STATUS][i],
                       &buf, sizeof(buf));
        if (ret < 0) {
            TRX_LOG_ERR(fpc, "Failed to read qsfp%u irq status. ret %d", i, ret);
            return ret;
        }
    }

    return 0;
}

/*
 * Process QSFP presence and QSFP module interrupts
 */
static int fpc_qsfp_irq(struct qsfp *qsfp)
{
    int ret;
    u8 buf = 0;

    if (!qsfp) {
        return 0;
    }

    ret = fpc_read(qsfp->fpc,
          FPC_PORT_REG[FPC_INPUT_PIN_INTERRUPT_STATUS][qsfp->port_num],
          &buf, sizeof(buf));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to read input pin interrupt status. "
                           "ret %d", ret);
        return ret;
    }

    if (buf & FPC_IN_B_MOD_PRESENT_RISING_EDGE_MASK) {
        TRX_LOG_INFO(qsfp, "ModulePresent Rising edge interrupt 0x%X", buf);
        qsfp_module_remove_irq(qsfp);

    } else if (buf & FPC_IN_B_MOD_PRESENT_FALLING_EDGE_MASK) {
        TRX_LOG_INFO(qsfp, "ModulePresent Falling edge interrupt 0x%X", buf);
        qsfp_module_insert_irq(qsfp);

    } else if (((buf & FPC_IN_A_INT_FALLING_EDGE_MASK) &&
               (buf & FPC_IN_A_INT_RISING_EDGE_MASK)) ||
               ((buf & FPC_IN_C_INT_FALLING_EDGE_MASK) &&
               (buf & FPC_IN_C_INT_RISING_EDGE_MASK))) {
        TRX_LOG_INFO(qsfp, "QSFP Falling and Rising edge interrupt 0x%X", buf);
        qsfp_irq(qsfp);

    } else if ((buf & FPC_IN_A_INT_FALLING_EDGE_MASK) ||
               (buf & FPC_IN_C_INT_FALLING_EDGE_MASK)) {
        TRX_LOG_INFO(qsfp, "QSFP Falling edge interrupt 0x%X", buf);
        qsfp_irq(qsfp);

    } else if ((buf & FPC_IN_A_INT_RISING_EDGE_MASK) ||
               (buf & FPC_IN_C_INT_RISING_EDGE_MASK)){
        TRX_LOG_INFO(qsfp, "QSFP Rising edge interrupt 0x%X", buf);
        qsfp_irq(qsfp);

    } else {
        TRX_LOG_ERR(qsfp, "Unknown Input Interrupt status 0x%X", buf);
    }

    return 0;
}

/*
 * Enable FPC interrupt for i2c errors SCL,SDA stuck
 */
void fpc_enable_i2c_stuck_interrupt(const struct fpc *fpc)
{
    int ret;
    u8 buf;

    buf = FPC_ENABLE_I2C_STUCK_INTERRUPT;

    ret = fpc_write(fpc, FPC_I2C_SCL_STUCK_INTERRUPT_REGISTER, &buf,
                    sizeof(buf));
    if (ret < 0) {
        TRX_LOG_WARN(fpc, "Failed to enable SCL stuck interrupt. ret %d", ret);
    }

    ret = fpc_write(fpc, FPC_I2C_SDA_STUCK_INTERRUPT_REGISTER, &buf,
                    sizeof(buf));
    if (ret < 0) {
        TRX_LOG_WARN(fpc, "Failed to enable SDA stuck interrupt. ret %d", ret);
    }
}

/*
 * Check i2c errors SCL,SDA stuck condition and logs error message
 */
static int fpc_read_i2c_stuck_status(const struct fpc *fpc)
{
    int ret;
    u8 buf = 0;

    ret = fpc_read(fpc, FPC_I2C_SCL_STUCK_INTERRUPT_REGISTER, &buf,
                   sizeof(buf));
    if (ret < 0) {
        TRX_LOG_ERR(fpc, "Failed to read SCL stuck status. ret %d", ret);
        return ret;
    } else if (buf & FPC_I2C_STUCK_STATUS_MASK) {
        u8 i;
        struct qsfp *qsfp;
        char *msg_scl_stuck[] = {QSFP_EVENT_I2C_SCL_STUCK, NULL};

        TRX_LOG_ERR(fpc, "SCL stuck error 0x%X", buf);

        for (i = 0 , buf >>= 4; i < FPC_MAX_PORTS ; buf >>= 1, i++) {
            if (buf & 1) {
                qsfp = fpc->qsfp[i];
                if (qsfp) {
                    kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_scl_stuck);
                    TRX_LOG_ERR(qsfp, "Port-%u: Transceiver I2C clock "
                    "line stuck. Recovery triggered", qsfp->port_num);
                    TRX_QXDM_LOG_ERROR(qsfp, "Port-%u: Transceiver I2C clock "
                    "line stuck. Recovery triggered", qsfp->port_num);
                    qsfp->i2c_stuck_counter++;
                    i2c_lock_bus(qsfp->i2c, I2C_LOCK_SEGMENT);
                    fpc_qsfp_i2c_recover(qsfp);
                    i2c_unlock_bus(qsfp->i2c, I2C_LOCK_SEGMENT);
                } else {
                    TRX_LOG_ERR(fpc, "qsfp null for port %u", i);
                }
            }
        }
    }

    buf = 0;
    ret = fpc_read(fpc, FPC_I2C_SDA_STUCK_INTERRUPT_REGISTER, &buf,
                   sizeof(buf));
    if (ret < 0) {
        TRX_LOG_ERR(fpc, "Failed to read SDA stuck status. ret %d", ret);
        return ret;
    } else if (buf & FPC_I2C_STUCK_STATUS_MASK) {
        u8 i;
        struct qsfp *qsfp;
        char *msg_sda_stuck[] = {QSFP_EVENT_I2C_SDA_STUCK, NULL};

        TRX_LOG_ERR(fpc, "SDA stuck error 0x%X", buf);

        for (i = 0 , buf >>= 4; i < FPC_MAX_PORTS ; buf >>= 1, i++) {
            if (buf & 1) {
                qsfp = fpc->qsfp[i];
                if (qsfp) {
                    kobject_uevent_env(&qsfp->dev->kobj, KOBJ_CHANGE, msg_sda_stuck);
                    TRX_LOG_ERR(qsfp, "Port-%u: Transceiver I2C data "
                    "line stuck. Recovery triggered", qsfp->port_num);
                    TRX_QXDM_LOG_ERROR(qsfp, "Port-%u: Transceiver I2C data "
                    "line stuck. Recovery triggered", qsfp->port_num);
                    qsfp->i2c_stuck_counter++;
                    i2c_lock_bus(qsfp->i2c, I2C_LOCK_SEGMENT);
                    fpc_qsfp_i2c_recover(qsfp);
                    i2c_unlock_bus(qsfp->i2c, I2C_LOCK_SEGMENT);
                } else {
                    TRX_LOG_ERR(fpc, "qsfp null for port %u", i);
                }
            }
        }
    }

    return 0;
}

/*
 * Enable FPC interrupt for changes in input A,B and C lines
 */
int fpc_enable_qsfp_interrupt(const struct qsfp *qsfp)
{
    u8 buf;

    buf = FPC_ENABLE_INPUT_A_B_C_INTERRUPT;

    return fpc_write(qsfp->fpc,
           FPC_PORT_REG[FPC_INPUT_PIN_INTERRUPT_ENABLE][qsfp->port_num],
           &buf, sizeof(buf));
}

/*
 * FPC Interrupt service routine
 * There will not be multiple instance of this function call at the same time
 * so no need of mutex lock
 */
static irqreturn_t fpc_irq(int irq, void *data)
{
    struct fpc *fpc = data;

    TRX_LOG_INFO(fpc, "");

    /* Wait for current execution to complete */
    cancel_delayed_work_sync(&fpc->irq);

    /* Schedule fpc irq task */
    mod_delayed_work(system_wq, &fpc->irq, 0);

    return IRQ_HANDLED;
}

static void fpc_irq_task(struct work_struct *work)
{
    struct fpc *fpc = container_of(work, struct fpc, irq.work);
    struct qsfp *qsfp;
    u8 port_irq = 0;
    u8 i, tmp;
    int ret;

    while (1) {
        /* Reads aggregated interrupt status */
        ret = fpc_read(fpc, FPC_INTERRUPT_STATUS_REGISTER,
                       &port_irq, sizeof(port_irq));
        if (ret < 0) {
            TRX_LOG_ERR(fpc, "Fail to read FPC interrupt status. ret %d", ret);
            mod_delayed_work(system_wq, &fpc->irq, msecs_to_jiffies(500));
            return;
        }

        TRX_LOG_INFO(fpc, "Aggregated Interrupt status 0x%X", port_irq);

        /* Return when all irq got processed */
        if ((port_irq & 0xF) == 0) {
            return;
        }

        for (i = 0, tmp = port_irq ; i < FPC_MAX_PORTS ; i++, tmp >>= 1) {
            if (!(tmp & 1)) {
                continue;
            }

            qsfp = fpc->qsfp[i];
            if (!qsfp) {
                TRX_LOG_ERR(fpc, "qsfp null for port %u", i);
                continue;
            }

            ret = fpc_qsfp_irq(qsfp);
            if (ret < 0) {
                TRX_LOG_ERR(qsfp, "Fail to read QSFP interrupt status. ret %d", ret);
                mod_delayed_work(system_wq, &fpc->irq, msecs_to_jiffies(500));
                return;
            }
        }

        ret = fpc_read_i2c_stuck_status(fpc);
        if (ret < 0) {
            TRX_LOG_ERR(fpc, "Fail to read FPC i2c stuck register. ret %d", ret);
            mod_delayed_work(system_wq, &fpc->irq, msecs_to_jiffies(500));
            return;
        }
    }
}

/*
 * Configure i2c address of FPC402
 */
static int fpc_configure_i2c_address(struct fpc *fpc, u8 i2c_address)
{
    int ret;
    u8 buf;

    buf = i2c_address;
    ret = fpc_write(fpc, FPC_I2C_DEVICE_ID_REGISTER, &buf, sizeof(buf));
    if (ret < 0) {
        /* if it fails try with i2c address that you are about to configure
         * this logic is needed to make sure driver works even after
         * rmmod and insmod as fpc reset wont reset configured i2c address
         */
        fpc->i2c_address = i2c_address >> 1;
        ret = fpc_write(fpc, FPC_I2C_DEVICE_ID_REGISTER, &buf, sizeof(buf));
        if (ret < 0) {
            TRX_LOG_INFO(fpc, "Unable to write i2c address");
            return ret;
        }
    } else {
    /* Convert 8-bit to 7-bit i2c address
     * In HW spec they mention i2c address in 8-bit form with last bit 0
     * but in actual i2c_transfer we have to use 7-bit i2c address so
     * it need right shift by one position.
     */
        fpc->i2c_address = i2c_address >> 1;
    }

    return 0;
}

static void fpc_cleanup(void *data)
{
    struct fpc *fpc = data;

    TRX_LOG_INFO(fpc, "");

    cancel_delayed_work_sync(&fpc->irq);

    if (fpc->instance_num < FPC_MAX_INSTANCES) {
        fpc_global[fpc->instance_num] = NULL;
    }

    kfree(fpc);
}

static struct fpc *fpc_alloc(struct device *dev)
{
    struct fpc *fpc;
    int i;

    fpc = kzalloc(sizeof(*fpc), GFP_KERNEL);
    if (!fpc) {
        return ERR_PTR(-ENOMEM);
    }

    fpc->dev = dev;
    INIT_DELAYED_WORK(&fpc->irq, fpc_irq_task);

    for (i = 0 ; i < FPC_MAX_PORTS ; i++)
           fpc->qsfp[i] = NULL;

    /* Assigning default i2c address
     * Convert 8-bit to 7-bit i2c address
     * In HW spec they mention i2c address in 8-bit form with last bit 0
     * but in actual i2c_transfer we have to use 7-bit i2c address so
     * it need right shift by one position.
     */
    fpc->i2c_address = FPC_DEFAULT_I2C_ADDRESS >> 1;

    fpc->debugfs_dir = NULL;

    return fpc;
}

static int fpc_reset(const struct fpc *fpc)
{
    int ret;
    u8 buf;

    /* Reset the FPC402 */
    buf = FPC_RESET_SEQUENCE;

    ret = fpc_write(fpc, FPC_RESET_REGISTER, &buf, sizeof(buf));
    if (ret < 0) {
        TRX_LOG_ERR(fpc, "Fail to write reset register. ret %d", ret);
        return ret;
    }

    buf = 0;
    ret = fpc_write(fpc, FPC_RESET_REGISTER, &buf, sizeof(buf));
    if (ret < 0) {
        TRX_LOG_ERR(fpc, "Fail to revert port reset sequence. ret %d", ret);
    }

    return ret;
}

int fpc_qsfp_i2c_recover(struct qsfp *qsfp)
{
    int ret;
    u8 buf;

    TRX_LOG_INFO(qsfp, "");

    buf = 1 << qsfp->port_num;

    ret = fpc_write_nolock(qsfp->fpc, FPC_RESET_REGISTER, &buf, sizeof(buf));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Fail to write reset register. ret %d", ret);
        return ret;
    }

    buf = 0;

    ret = fpc_write_nolock(qsfp->fpc, FPC_RESET_REGISTER, &buf, sizeof(buf));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Fail to revert port reset sequence. ret %d", ret);
        return ret;
    }

    qsfp->reset_counter++;

    if (qsfp->fpc_qsfp_i2c_recover_delay > 0) {
        msleep(qsfp->fpc_qsfp_i2c_recover_delay);
    }

    return 0;
}

/* Reset all 4 ports using reset gpio line */
static void fpc_reset_qsfp_ports(const struct fpc *fpc)
{
    int ret;
    u8 buf;

    buf = FPC_QSFP_RESET_SEQUENCE;
    ret = fpc_write(fpc, FPC_OUT_A_B_VALUE, &buf, sizeof(buf));
    if (ret < 0) {
        TRX_LOG_WARN(fpc, "Fail to write Reset sequence. ret %d", ret);
        return;
    }

    buf = FPC_OUT_A_ENABLE;
    ret = fpc_write(fpc, FPC_OUT_A_B_ENABLE_REGISTER, &buf, sizeof(buf));
    if (ret < 0) {
        TRX_LOG_WARN(fpc, "Fail to enable Reset gpio. ret %d", ret);
        return;
    }

    buf = FPC_OUT_A_DISABLE;
    ret = fpc_write(fpc, FPC_OUT_A_B_ENABLE_REGISTER, &buf, sizeof(buf));
    if (ret < 0) {
        TRX_LOG_WARN(fpc, "Fail to disable Reset gpio. ret %d", ret);
    }
}

/* Reset particular QSFP port using reset gpio line */
void fpc_reset_qsfp(const struct qsfp *qsfp)
{
    int ret;
    u8 buf;
    struct fpc *fpc = qsfp->fpc;

    buf = (1 << qsfp->port_num) ^ 0xF;
    ret = fpc_write(fpc, FPC_OUT_A_B_VALUE, &buf, sizeof(buf));
    if (ret < 0) {
        TRX_LOG_WARN(fpc, "Fail to write Reset sequence. ret %d", ret);
        return;
    }

    buf = 1 << qsfp->port_num;
    ret = fpc_write(fpc, FPC_OUT_A_B_ENABLE_REGISTER, &buf, sizeof(buf));
    if (ret < 0) {
        TRX_LOG_WARN(fpc, "Fail to enable Reset gpio. ret %d", ret);
        return;
    }

    buf = (1 << qsfp->port_num) ^ 0xF;
    ret = fpc_write(fpc, FPC_OUT_A_B_ENABLE_REGISTER, &buf, sizeof(buf));
    if (ret < 0) {
        TRX_LOG_WARN(fpc, "Fail to disable Reset gpio. ret %d", ret);
    }
}

/*
 * Probe function which processes FPC device node
 * it reads i2c adapter , i2c address and register ISR
 */
static int fpc_probe(struct platform_device *pdev)
{
    struct fpc *fpc;
    char *fpc_irq_name;
    u32 i2c_address = 0;
    u32 fpc_instance_no = 0;
    int ret;
    struct device_node *node = pdev->dev.of_node;
    struct device_node *i2c_np;
    u8 buff = 0;

    if(timer_pending(&fpc_probe_timer) &&
       (sysfs_update_flag == false)) {
        TRX_LOG_PDEV_ERR(&pdev, "Defer as dual transceiver "
                            "sysfs file not yet updated\n");
        return -EPROBE_DEFER;
    }

    fpc = fpc_alloc(&pdev->dev);
    if (IS_ERR(fpc)) {
        TRX_LOG_PDEV_ERR(&pdev, "fpc_alloc failed");
        return PTR_ERR(fpc);
    }

    ret = devm_add_action(fpc->dev, fpc_cleanup, fpc);
    if (ret < 0) {
        TRX_LOG_ERR(fpc, "devm_add_action failed. ret %d", ret);
        fpc_cleanup(fpc);
        return ret;
    }

    if (!node) {
        TRX_LOG_ERR(fpc, "dev node not found");
        return -EINVAL;
    }

    ret = device_property_read_u32(fpc->dev, "instance-num", &fpc_instance_no);
    if (ret < 0) {
        TRX_LOG_INFO(fpc, "Fail to get instance-num attribute. ret %d", ret);
        return ret;
    }

    if ((fpc_instance_no & 0xFF) >= FPC_MAX_INSTANCES) {
        TRX_LOG_INFO(fpc, "Invalid instance-num attribute");
        return -EINVAL;
    }
    fpc->instance_num = fpc_instance_no & 0xFF;

    /* Not applicable for 1st FPC402 constroller */
    if (fpc->instance_num > 0)  {
        /* Check previous FPC402 instance is NULL */
        if (!fpc_global[fpc->instance_num-1]) {
            TRX_LOG_INFO(fpc, "Defer as previous FPC402 instance %u not yet "
                              "initilised", fpc->instance_num);
            return -EPROBE_DEFER;
        }
    }

    ret = device_property_read_u32(fpc->dev, "i2c-address", &i2c_address);
    if (ret < 0) {
        TRX_LOG_ERR(fpc, "Fail to get i2c-address attribute. ret %d", ret);
        return ret;
    }

    TRX_LOG_INFO(fpc, "i2c_address 0x%02X (0x%02X)",
                    i2c_address, i2c_address >> 1);

    i2c_np = of_parse_phandle(node, "i2c-bus", 0);
    if (!i2c_np) {
        TRX_LOG_ERR(fpc, "Missing 'i2c-bus' property");
        return -ENODEV;
    }

    fpc->i2c = of_find_i2c_adapter_by_node(i2c_np);
    of_node_put(i2c_np);

    if (!fpc->i2c) {
        TRX_LOG_ERR(fpc, "Not able to find i2c adapter from node %s",
                       i2c_np->full_name);
        return -EPROBE_DEFER;
    }

    TRX_LOG_INFO(fpc, "i2c adapter i2c-%d", fpc->i2c->nr);

    ret = fpc_configure_i2c_address(fpc, i2c_address & 0xFF);
    if (ret < 0) {
        TRX_LOG_INFO(fpc, "Not able to configure i2c address. ret %d", ret);
        return -EPROBE_DEFER;
    }

    /* The FPC Reserved register at offset 0x03 is used to determine the type
     * of reset (hard or soft) for the FPC.
     *    0xFF indicates soft reset is performed.
     *    0x11 indicates hard reset is performed.
     */
    ret = fpc_read(fpc, FPC_RESERVED_REGISTER, &buff, sizeof(buff));
    if (ret < 0)
    {
        TRX_LOG_ERR(fpc, "Failed to read FPC reserved register. ret %d", ret);
    } else {
        if(buff == FPC_RESERVED_REG_DEFAULT_VAL)
            TRX_LOG_INFO(fpc, "FPC power reset completed by triggering a"
            " hard reset: 0x%X", buff);
        else if(buff == FPC_RESERVED_REG_UPDATE_VAL)
            TRX_LOG_INFO(fpc, "FPC power reset not done: 0x%X\n", buff);
    }

    ret = fpc_reset(fpc);
    if (ret < 0) {
        TRX_LOG_ERR(fpc, "Unable to reset FPC402. ret %d", ret);
        return -EPROBE_DEFER;
    }

    fpc->interrupt_gpio = devm_gpiod_get_optional(fpc->dev, "interrupt",
                                                  GPIOD_IN);

    fpc->gpio_irq = gpiod_to_irq(fpc->interrupt_gpio);

    if (fpc->gpio_irq < 0) {
        TRX_LOG_ERR(fpc, "Unable to get gpio for interrupt %d", fpc->gpio_irq);
        return -EPROBE_DEFER;
    }

    fpc_irq_name = devm_kasprintf(fpc->dev, GFP_KERNEL,
                                  "%s-%s", dev_name(fpc->dev),
                                  "Interrupt");

    if (!fpc_irq_name) {
        TRX_LOG_ERR(fpc, "Unable to get interrupt name");
        return -EPROBE_DEFER;
    }

    TRX_LOG_INFO(fpc, "gpio_irq 0x%X fpc_irq_name %s",
                    fpc->gpio_irq, fpc_irq_name);

    /* Need to read QSFP irq status register to clear any previous interrupts */
    ret = fpc_read_qsfp_irq_status(fpc);
    if (ret < 0) {
        TRX_LOG_ERR(fpc, "QSFP irq status read failed. ret %d", ret);
        return -EPROBE_DEFER;
    }

    ret = devm_request_threaded_irq(fpc->dev, fpc->gpio_irq,
                                    NULL, fpc_irq,
                                    IRQF_ONESHOT |
                                    IRQF_TRIGGER_FALLING,
                                    fpc_irq_name, fpc);
    if (ret < 0) {
        TRX_LOG_ERR(fpc, "Interrupt register failed. ret %d", ret);
        return -EPROBE_DEFER;
    }

    fpc_enable_i2c_stuck_interrupt(fpc);

    /* Reset all 4 ports using reset gpio line */
    fpc_reset_qsfp_ports(fpc);

    fpc_debugfs_init(fpc);

    /* set driver data once everything is successful */
    platform_set_drvdata(pdev, fpc);

    if (fpc_global[fpc->instance_num] == NULL) {
        fpc_global[fpc->instance_num] = fpc;
    } else {
        TRX_LOG_ERR(fpc, "Instance-num %u already exists", fpc->instance_num);
        return -EINVAL;
    }

    /* Set FPC Byte offset 0x03 with value 0xFF */
    buff = FPC_RESERVED_REG_UPDATE_VAL;

    ret = fpc_write(fpc, FPC_RESERVED_REGISTER, &buff, sizeof(buff));
    if (ret < 0) {
        TRX_LOG_ERR(fpc, "Fail to write reserved register .", ret);
    }

    TRX_LOG_INFO(fpc, "Success");
    TRX_QXDM_LOG_INFO(fpc, "FPC402 instance %u init successful", fpc->instance_num);

    return 0;
}

/*
 * Checks whether platform device is for FPC
 */
static u8 get_node_type(struct platform_device *pdev)
{
    struct device_node *node = pdev->dev.of_node;
    const struct of_device_id *id;

    if (!node) {
        TRX_LOG_PDEV_ERR(&pdev, "No dev of_node");
        return -EINVAL;
    }

    id = of_match_node(fpc_qsfp_of_match, node);
    if (WARN_ON(!id)) {
        TRX_LOG_PDEV_ERR(&pdev, "No of_match_node");
        return -EINVAL;
    }

    if (!strcmp(id->compatible, FPC_COMPATIBLE)) {
        return FPC402;
    } else if (!strcmp(id->compatible, QSFP_COMPATIBLE)) {
        return QSFP_PORT;
    } else if (!strcmp(id->compatible, LANE_COMPATIBLE)) {
        return QSFP_LANE;
    } else {
        TRX_LOG_PDEV_ERR(&pdev, "%s no match", id->compatible);
        return QSFP_NONE;
    }
}

/*
 * Unified probe function gets called when device tree node matches with
 * table fpc_qsfp_of_match for either FPC or QSFP
 */
static int fpc_qsfp_lane_probe(struct platform_device *pdev)
{
    u8 node_type;

    node_type = get_node_type(pdev);

    switch (node_type) {
    case FPC402:
        return fpc_probe(pdev);
    case QSFP_PORT:
        return qsfp_probe(pdev);
    case QSFP_LANE:
        return lane_probe(pdev);
    default:
       return -EINVAL;
    }
}

static int fpc_remove(struct platform_device *pdev)
{
    struct fpc *fpc = platform_get_drvdata(pdev);
    struct qsfp *qsfpi;
    u8 i, buff;
    int ret;

    TRX_LOG_INFO(fpc, "");

    /* Free the interrupt to stop further interrupts */
    devm_free_irq(fpc->dev, fpc->gpio_irq, fpc);

    for (i = 0 ; i < FPC_MAX_PORTS ; i++) {
        qsfpi = fpc->qsfp[i];
        if (qsfpi) {
            mutex_lock(&qsfpi->sm_mutex);
            qsfpi->fpc = NULL;
            mutex_unlock(&qsfpi->sm_mutex);
        }
    }

    fpc_reset(fpc);

    /* Set FPC Byte offset 0x03 with value 0xFF */
    buff = FPC_RESERVED_REG_UPDATE_VAL;

    ret = fpc_write(fpc, FPC_RESERVED_REGISTER, &buff, sizeof(buff));
    if (ret < 0) {
        TRX_LOG_ERR(fpc, "Fail to write reserved register .", ret);
    }

    fpc_debugfs_exit(fpc);

    return 0;
}

static int fpc_qsfp_remove(struct platform_device *pdev)
{
    u8 node_type;

    node_type = get_node_type(pdev);

    switch (node_type) {
    case FPC402:
        return fpc_remove(pdev);
    case QSFP_PORT:
        return qsfp_remove(pdev);
    case QSFP_LANE:
        return lane_remove(pdev);
    default:
        return -EINVAL;
    }
}

static struct platform_driver fpc_qsfp_driver = {
    .probe = fpc_qsfp_lane_probe,
    .remove = fpc_qsfp_remove,
    .driver = {
        .name = DRV_NAME,
        .of_match_table = fpc_qsfp_of_match,
    },
};

static ssize_t enable_qxdm_show(struct kobject *kobj,
                        struct kobj_attribute *attr, char *buf) {
    return snprintf(buf, PAGE_SIZE, "%d\n", is_qxdm_log_en);
}

static ssize_t enable_qxdm_store(struct kobject *kobj,
             struct kobj_attribute *attr, const char *buf, size_t count) {
    sscanf(buf, "%u", &is_qxdm_log_en);
    printk(KERN_INFO "QXDM Logging %s\n", is_qxdm_log_en ? "enabled" :
                                                          "disabled");
    return count;
}

static struct kobj_attribute qxdm_attribute = __ATTR(enable_qxdm,
                       0664, enable_qxdm_show, enable_qxdm_store);

static ssize_t dual_transceivers_show(struct kobject *kobj,
                           struct kobj_attribute *attr, char *buf) {
    struct dual_tcvr_entry *entry;
    size_t count = 0;

    list_for_each_entry(entry, &dual_tcvr_list, list) {
        count += scnprintf(buf + count, PAGE_SIZE - count, "%s\n",
                                                     entry->name);
    }

    return count;
}

void update_runtime_dual_tcvr(void)
{
    struct fpc *fpc = NULL;
    struct qsfp *qsfp = NULL;
    int i =0, j=0;

    for (i=0; i<FPC_MAX_INSTANCES; i++)
    {
       if(fpc_global[i] != NULL)
       {
           fpc = fpc_global[i];
           for (j=0; j<FPC_MAX_PORTS; j++)
           {
              if(fpc->qsfp[j] != NULL)
              {
                  qsfp = fpc->qsfp[j];
                  update_runtime_dual_cfg(qsfp);
              }
           }
       }
    }
}

static ssize_t dual_transceivers_store(struct kobject *kobj,
              struct kobj_attribute *attr, const char *buf, size_t count) {
    char *kbuf, *token, *rest;
    struct dual_tcvr_entry *new_transceiver, *tmp;

    kbuf = kmalloc(count + 1, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    strncpy(kbuf, buf, count);
    kbuf[count] = '\0';

    /* Delete the existing list */
    list_for_each_entry_safe(new_transceiver, tmp, &dual_tcvr_list, list) {
        list_del(&new_transceiver->list);
        kfree(new_transceiver->name);
        kfree(new_transceiver);
    }

    new_transceiver = NULL;

    rest = kbuf;
    while ((token = strsep(&rest, "\n")) != NULL) {
        new_transceiver = kmalloc(sizeof(*new_transceiver), GFP_KERNEL);
        if (!new_transceiver) {
            kfree(kbuf);
            return -ENOMEM;
        }

        new_transceiver->name = kstrdup(token, GFP_KERNEL);
        if (!new_transceiver->name) {
            kfree(new_transceiver);
            kfree(kbuf);
            return -ENOMEM;
        }

        if(strlen(new_transceiver->name) == 0)
        {
            kfree(new_transceiver->name);
            kfree(new_transceiver);
            continue;
        }

        INIT_LIST_HEAD(&new_transceiver->list);
        list_add_tail(&new_transceiver->list, &dual_tcvr_list);
    }

    kfree(kbuf);

    if(sysfs_update_flag == true)
        update_runtime_dual_tcvr();

    sysfs_update_flag = true;

    return count;
}

static struct kobj_attribute transceiver_attribute = __ATTR(dual_transceivers,
                       0664, dual_transceivers_show, dual_transceivers_store);

void fpc_probe_timer_callback(struct timer_list *timer) {
    TRX_LOG_INFO_NODEV("fpc_probe_timer expired\n");
}

/*
 * Init function gets called during insmod/modprobe
 */
static int fpc_qsfp_init(void)
{
    int ret = 0;

    // Create a class specifically for sending uevents to user application.
    uevent_tcvr_class = class_create(THIS_MODULE, "uevent_tcvr_class");
    if (IS_ERR(uevent_tcvr_class)) {
        printk(KERN_ERR "Failed to create uevent_tcvr_class: %ld \n",
                                        PTR_ERR(uevent_tcvr_class));
        uevent_tcvr_class = NULL;
    }
    else {
        /* Create a device specifically for sending uevents to user
         * Application via its kobject */
        uevent_tcvr_device = device_create(uevent_tcvr_class, NULL,
                             MKDEV(0, 0), NULL, "uevent_tcvr_device");
        if (IS_ERR(uevent_tcvr_device)) {
            printk(KERN_ERR "Failed to create uevent_tcvr_device: %ld \n",
                                            PTR_ERR(uevent_tcvr_device));
            class_destroy(uevent_tcvr_class);
            uevent_tcvr_class = NULL;
            uevent_tcvr_device = NULL;
        }
    }

    transceiver_kobj = kobject_create_and_add("transceiver", kernel_kobj);
    if (!transceiver_kobj) {
        TRX_LOG_ERR_NODEV("transceiver kobj creation failed");
        return -ENOMEM;
    }

    ret = sysfs_create_file(transceiver_kobj, &qxdm_attribute.attr);
    if (ret) {
        TRX_LOG_ERR_NODEV("transceiver enable_qxdm sysfs file creation failed");
        kobject_put(transceiver_kobj);
        transceiver_kobj = NULL;
        return ret;
    } else {
        char *msg_qxdm_cfg[] = {EVENT_QXDM_SYSFS_CFG_CREATE, NULL};
        if(uevent_tcvr_device) {
            kobject_uevent_env(&uevent_tcvr_device->kobj, KOBJ_CHANGE, msg_qxdm_cfg);
        }
    }

    ret = sysfs_create_file(transceiver_kobj, &transceiver_attribute.attr);
    if (ret) {
        TRX_LOG_ERR_NODEV("transceiver dual speed sysfs file creation failed");
        kobject_put(transceiver_kobj);
        transceiver_kobj = NULL;
        return ret;
    }

    trx_ipc_log_buf = ipc_log_context_create(TRX_IPC_LOG_PAGES, DRV_NAME, 0);
    if (trx_ipc_log_buf == NULL) {
        TRX_LOG_ERR_NODEV("IPC log creation failed");
    } else {
        TRX_LOG_INFO_NODEV("IPC log creation successful");
    }

    // Initialize the timer
    timer_setup(&fpc_probe_timer, fpc_probe_timer_callback, 0);

    // Set the timer to expire after 1 second (1 * HZ jiffies)
    mod_timer(&fpc_probe_timer, jiffies + 1 * HZ);

    transceiver_debugfs_init();
    return platform_driver_register(&fpc_qsfp_driver);
}
module_init(fpc_qsfp_init);

/*
 * Exit function gets called during rmmod
 */
static void fpc_qsfp_exit(void)
{
    struct dual_tcvr_entry *entry, *tmp;
    transceiver_debugfs_exit();
    platform_driver_unregister(&fpc_qsfp_driver);

    if (trx_ipc_log_buf) {
        ipc_log_context_destroy(trx_ipc_log_buf);
    }

    if (uevent_tcvr_device)
        device_destroy(uevent_tcvr_class, MKDEV(0, 0));

    if(uevent_tcvr_class)
        class_destroy(uevent_tcvr_class);

    uevent_tcvr_class = NULL;
    uevent_tcvr_device = NULL;

    // Delete the timer if it is still active
    del_timer(&fpc_probe_timer);

    list_for_each_entry_safe(entry, tmp, &dual_tcvr_list, list) {
        list_del(&entry->list);
        kfree(entry->name);
        kfree(entry);
        entry = NULL;
    }

    if(transceiver_kobj != NULL) {
        sysfs_remove_file(transceiver_kobj, &qxdm_attribute.attr);
        sysfs_remove_file(transceiver_kobj, &transceiver_attribute.attr);
        kobject_put(transceiver_kobj);
        transceiver_kobj = NULL;
    }

    sysfs_update_flag = false;
}
module_exit(fpc_qsfp_exit);

MODULE_ALIAS("platform:qsfp");
MODULE_LICENSE("GPL v2");
