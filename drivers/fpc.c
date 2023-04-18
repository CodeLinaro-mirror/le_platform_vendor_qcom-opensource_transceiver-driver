// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include "fpc.h"
#include "qsfp.h"
#include "transceiver_debugfs.h"
#include "trx_sysfs.h"

struct fpc *fpc_global[FPC_MAX_INSTANCES];

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
    { },
};
MODULE_DEVICE_TABLE(of, fpc_qsfp_of_match);

void *trx_ipc_log_buf = NULL;

/*
 * Reads FPC402 register memory map using i2c transaction
 * returns 0 on successful read of 'len' bytes otherwise error
 */
int fpc_read(const struct fpc *fpc, u8 dev_addr, void *buf, size_t len)
{
    struct i2c_msg msgs[2];
    u8 bus_addr = fpc->i2c_address;
    int ret;

    msgs[0].addr = bus_addr;
    msgs[0].flags = 0;
    msgs[0].len = 1;
    msgs[0].buf = &dev_addr;
    msgs[1].addr = bus_addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = len;
    msgs[1].buf = buf;

    ret = i2c_transfer(fpc->i2c, msgs, ARRAY_SIZE(msgs));
    if (ret < 0)
        return ret;

    return ret == ARRAY_SIZE(msgs) ? 0 : -EIO;
}

/*
 * Writes FPC402 register memory map using i2c transaction
 * returns 0 on successful write of 'len' bytes otherwise error
 */
int fpc_write(const struct fpc *fpc, u8 dev_addr, void *buf, size_t len)
{
    struct i2c_msg msgs[1];
    u8 bus_addr = fpc->i2c_address;
    int ret;

    msgs[0].addr = bus_addr;
    msgs[0].flags = 0;
    msgs[0].len = 1 + len;
    msgs[0].buf = kmalloc(1 + len, GFP_KERNEL);
    if (!msgs[0].buf)
        return -ENOMEM;

    msgs[0].buf[0] = dev_addr;
    memcpy(&msgs[0].buf[1], buf, len);

    ret = i2c_transfer(fpc->i2c, msgs, ARRAY_SIZE(msgs));

    kfree(msgs[0].buf);

    if (ret < 0)
        return ret;

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

/*
 * Process QSFP presence and QSFP module interrupts
 */
static int fpc_qsfp_irq(struct qsfp *qsfp)
{
    int ret;
    u8 buf = 0;

    if (!qsfp)
        return 0;

    ret = fpc_read(qsfp->fpc,
          FPC_PORT_REG[FPC_INPUT_PIN_INTERRUPT_STATUS][qsfp->port_num],
          &buf, sizeof(buf));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Failed to read input pin interrupt status. "
                           "ret %d", ret);
        return ret;
    }

    TRX_LOG_INFO(qsfp, "Input Interrupt status 0x%X", buf);

    if (buf & FPC_IN_B_MOD_PRESENT_RISING_EDGE_MASK) {
        TRX_LOG_INFO(qsfp, "ModulePresent Rising edge interrupt 0x%X", buf);
        qsfp_module_remove_irq(qsfp);
        return 0;
    } else if (buf & FPC_IN_B_MOD_PRESENT_FALLING_EDGE_MASK) {
        TRX_LOG_INFO(qsfp, "ModulePresent Falling edge interrupt 0x%X", buf);
        qsfp_module_insert_irq(qsfp);
    }

    if (buf & FPC_IN_A_INT_FALLING_EDGE_MASK) {
        TRX_LOG_INFO(qsfp, "QSFP Falling edge interrupt 0x%X", buf);
        qsfp_irq(qsfp);
    }

    if (buf & FPC_IN_A_INT_RISING_EDGE_MASK) {
        TRX_LOG_INFO(qsfp, "QSFP Rising edge interrupt 0x%X", buf);
        qsfp_irq(qsfp);
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
    if (ret < 0)
        TRX_LOG_WARN(fpc, "Failed to enable SCL stuck interrupt. ret %d", ret);

    ret = fpc_write(fpc, FPC_I2C_SDA_STUCK_INTERRUPT_REGISTER, &buf,
                    sizeof(buf));
    if (ret < 0)
        TRX_LOG_WARN(fpc, "Failed to enable SDA stuck interrupt. ret %d", ret);

}

/*
 * Check i2c errors SCL,SDA stuck condition and logs error message
 */
static void fpc_read_i2c_stuck_status(const struct fpc *fpc)
{
    int ret;
    u8 buf = 0;

    ret = fpc_read(fpc, FPC_I2C_SCL_STUCK_INTERRUPT_REGISTER, &buf,
                   sizeof(buf));
    if (ret < 0) {
        TRX_LOG_ERR(fpc, "Failed to read SCL stuck status. ret %d", ret);
    } else if (buf & FPC_I2C_STUCK_STATUS_MASK) {
        TRX_LOG_ERR(fpc, "SCL stuck error 0x%X", buf);
    }

    buf = 0;
    ret = fpc_read(fpc, FPC_I2C_SDA_STUCK_INTERRUPT_REGISTER, &buf,
                   sizeof(buf));
    if (ret < 0) {
        TRX_LOG_ERR(fpc, "Failed to read SDA stuck status. ret %d", ret);
    } else if (buf & FPC_I2C_STUCK_STATUS_MASK) {
        TRX_LOG_ERR(fpc, "SDA stuck error 0x%X", buf);
    }
}

/*
 * Enable FPC interrupt for changes in input A and B lines
 */
int fpc_enable_qsfp_interrupt(const struct qsfp *qsfp)
{
    u8 buf;

    buf = FPC_ENABLE_INPUT_A_B_INTERRUPT;

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
    u8 port_interrupt = 0, port_num;
    int ret;

    fpc_read_i2c_stuck_status(fpc);

    /* Reads aggregated interrupt status */
    ret = fpc_read(fpc, FPC_INTERRUPT_STATUS_REGISTER,
                   &port_interrupt, sizeof(port_interrupt));
    if (ret < 0) {
        TRX_LOG_ERR(fpc, "Fail to read FPC interrupt status. ret %d", ret);
        return IRQ_HANDLED;
    }

    TRX_LOG_INFO(fpc, "Aggregated Interrupt status 0x%X", port_interrupt);

    for (port_num = 0 ; port_num < FPC_MAX_PORTS ; port_num++) {
        if (port_interrupt & 1) {
            fpc_qsfp_irq(fpc->qsfp[port_num]);
        }
        port_interrupt >>= 1;
    }

    return IRQ_HANDLED;
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
    if (!fpc)
        return ERR_PTR(-ENOMEM);

    fpc->dev = dev;

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
    if (ret < 0)
        TRX_LOG_ERR(fpc, "Fail to revert port reset sequence. ret %d", ret);

    return ret;
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
    if (ret < 0)
        TRX_LOG_WARN(fpc, "Fail to disable Reset gpio. ret %d", ret);
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

    fpc = fpc_alloc(&pdev->dev);
    if (IS_ERR(fpc)) {
        TRX_LOG_ERR(&pdev, "fpc_alloc failed");
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

    TRX_LOG_INFO(fpc, "Success");

    return 0;
}

/*
 * Checks whether platform device is for FPC
 */
static bool is_fpc_device(const struct platform_device *pdev)
{
    struct device_node *node = pdev->dev.of_node;
    const struct of_device_id *id;

    if (!node) {
        TRX_LOG_ERR(&pdev, "No dev of_node");
        return -EINVAL;
    }

    id = of_match_node(fpc_qsfp_of_match, node);
    if (WARN_ON(!id)) {
        TRX_LOG_ERR(&pdev, "No of_match_node");
        return -EINVAL;
    }

    if (strcmp(id->compatible, FPC_COMPATIBLE))
        return false;
    else
        return true;
}

/*
 * Unified probe function gets called when device tree node matches with
 * table fpc_qsfp_of_match for either FPC or QSFP
 */
static int fpc_qsfp_probe(struct platform_device *pdev)
{
    if (is_fpc_device(pdev)) {
        return fpc_probe(pdev);
    } else {
        return qsfp_probe(pdev);
    }
}

static int fpc_remove(struct platform_device *pdev)
{
    struct fpc *fpc = platform_get_drvdata(pdev);

    fpc_reset(fpc);

    fpc_debugfs_exit(fpc);

    return 0;
}

static int fpc_qsfp_remove(struct platform_device *pdev)
{
    if (is_fpc_device(pdev)) {
        return fpc_remove(pdev);
    } else {
        return qsfp_remove(pdev);
    }
}

static void fpc_shutdown(struct platform_device *pdev)
{
    struct fpc *fpc = platform_get_drvdata(pdev);

    if (fpc->gpio_irq)
        devm_free_irq(fpc->dev, fpc->gpio_irq, fpc);

}

static void fpc_qsfp_shutdown(struct platform_device *pdev)
{
    if (is_fpc_device(pdev)) {
        fpc_shutdown(pdev);
    } else {
        qsfp_shutdown(pdev);
    }
}

static struct platform_driver fpc_qsfp_driver = {
    .probe = fpc_qsfp_probe,
    .remove = fpc_qsfp_remove,
    .shutdown = fpc_qsfp_shutdown,
    .driver = {
        .name = DRV_NAME,
        .of_match_table = fpc_qsfp_of_match,
    },
};

/*
 * Init function gets called during insmod/modprobe
 */
static int fpc_qsfp_init(void)
{
    trx_ipc_log_buf = ipc_log_context_create(TRX_IPC_LOG_PAGES, DRV_NAME, 0);
    if (trx_ipc_log_buf == NULL) {
        TRX_LOG_ERR_NODEV("IPC log creation failed");
    } else {
        TRX_LOG_INFO_NODEV("IPC log creation successful");
    }

    transceiver_debugfs_init();
    return platform_driver_register(&fpc_qsfp_driver);
}
module_init(fpc_qsfp_init);

/*
 * Exit function gets called during rmmod
 */
static void fpc_qsfp_exit(void)
{
    platform_driver_unregister(&fpc_qsfp_driver);
    transceiver_debugfs_exit();

    if (trx_ipc_log_buf) {
        ipc_log_context_destroy(trx_ipc_log_buf);
    }
}
module_exit(fpc_qsfp_exit);

MODULE_ALIAS("platform:qsfp");
MODULE_LICENSE("GPL v2");
