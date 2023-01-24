/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "fpc_led.h"
#include "qsfp.h"
#include "fpc.h"

extern struct fpc *fpc_global[FPC_MAX_INSTANCES];

/*
 * API to set fpc port LED ON
 */
int transceiver_led_on (u8 fpc_instance, u8 port_num, u8 led_num)
{
    struct fpc *fpc = NULL;
    struct qsfp *qsfp = NULL;
    int ret = SUCCESS;
    u8 buf = 0;

    /* Validation of input data from the caller */
    if (fpc_instance >= FPC_MAX_INSTANCES) {
        pr_err(" %s: Invalid FPC instance input data: %u\n", __func__,
                                                        fpc_instance);
        return ERROR_INVALID_FPC_INSTANCE_INPUT;
    }

    if (port_num >= FPC_MAX_PORTS) {
        pr_err(" %s: Invalid QSFP port number input data: %u\n", __func__,
                                                                port_num);
        return ERROR_INVALID_QSFP_PORT_NUM_INPUT;
    }

    /* For LED1(0001) LED2(0010) and for both led's(0011) */
    if ((led_num == 0) || (led_num > 3)) {
        pr_err(" %s: Invalid LED number input data: %u\n", __func__,
                                                           led_num);
        return ERROR_INVALID_LED_NUM_INPUT;
    }

    if (fpc_global[fpc_instance] == NULL) {
        pr_err(" %s: FPC probe failed for instance: %u ", __func__,
                                                     fpc_instance);
        return ERROR_FPC_PROBE;
    }

    fpc = fpc_global[fpc_instance];
    qsfp = fpc->qsfp[port_num];

    if (qsfp->sm_mod_state < QSFP_MOD_ERROR) {
        dev_notice(qsfp->dev, "%s: QSFP module is not attached\n",
                             __func__);
        return ERROR_QSFP_MODULE_PRESENCE;
    }

    ret = fpc_read(fpc,
          FPC_PORT_REG[FPC_LED_MODE_SELECT][port_num],
          &buf, sizeof(buf));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Fail to read LED mode set register\n"
                               "ret %d\n", __func__, ret);
        return ERROR_FPC_I2C_READ;
    }

    if (led_num & QSFP_LED1) {
        /* configure LED1 Mode Select Register to ON MODE (0001).*/
        buf &= ~BIT(1);
        buf |= BIT(0);
    }

    if (led_num & QSFP_LED2) {
        /* configure LED2 Mode Select Register to ON MODE (0100) */
        buf &= ~BIT(3);
        buf |= BIT(2);
    }

    ret = fpc_write(fpc,
          FPC_PORT_REG[FPC_LED_MODE_SELECT][port_num],
          &buf, sizeof(buf));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Fail to write LED mode set register"
                               " ret %d\n", __func__, ret);
        return ERROR_FPC_I2C_WRITE;
    }

    dev_notice(qsfp->dev, "%s: %s %s ON\n", __func__,
                         (led_num & QSFP_LED1) ? "LED1 ":"",
                         (led_num & QSFP_LED2) ? "LED2 ":"");
    return ret;
}

/*
 * API to set fpc port LED OFF.
 */
int transceiver_led_off(u8 fpc_instance, u8 port_num, u8 led_num)
{
    struct fpc *fpc = NULL;
    struct qsfp *qsfp = NULL;
    int ret = SUCCESS;
    u8 buf = 0;

    /* Validation of input data from the caller */
    if (fpc_instance >= FPC_MAX_INSTANCES) {
        pr_err(" %s: Invalid FPC instance input data: %u\n", __func__,
                                                        fpc_instance);
        return ERROR_INVALID_FPC_INSTANCE_INPUT;
    }

    if (port_num >= FPC_MAX_PORTS) {
        pr_err(" %s: Invalid QSFP port number input data: %u\n", __func__,
                                                                port_num);
        return ERROR_INVALID_QSFP_PORT_NUM_INPUT;
    }

    /* For LED1(0001) LED2(0010) and for both led's(0011) */
    if ((led_num == 0) || (led_num > 3)) {
        pr_err(" %s: Invalid LED number input data: %u\n", __func__,
                                                           led_num);
        return ERROR_INVALID_LED_NUM_INPUT;
    }

    if (fpc_global[fpc_instance] == NULL) {
        pr_err(" %s: FPC probe failed for instance: %u ", __func__,
                                                     fpc_instance);
        return ERROR_FPC_PROBE;
    }

    fpc = fpc_global[fpc_instance];
    qsfp = fpc->qsfp[port_num];

    if (qsfp->sm_mod_state < QSFP_MOD_ERROR) {
        dev_notice(qsfp->dev, "%s: QSFP module is not attached\n",
                             __func__);
        return ERROR_QSFP_MODULE_PRESENCE;
    }

    ret = fpc_read(fpc,
          FPC_PORT_REG[FPC_LED_MODE_SELECT][port_num],
          &buf, sizeof(buf));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Fail to read LED mode set register"
                               "ret %d\n", __func__, ret);
        return ERROR_FPC_I2C_READ;
    }

    if ( led_num & QSFP_LED1 ) {
        /* configure LED1 Mode Select Register to OFF MODE (0000) */
        buf &= ~BIT(0);
        buf &= ~BIT(1);
    }

    if (led_num & QSFP_LED2) {
        /*  configure LED2 Mode Select Register to OFF MODE (0000) */
        buf &= ~BIT(2);
        buf &= ~BIT(3);
    }

    ret = fpc_write(fpc,
          FPC_PORT_REG[FPC_LED_MODE_SELECT][port_num],
          &buf, sizeof(buf));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Fail to write LED mode set register"
                               " ret %d\n", __func__, ret);
        return ERROR_FPC_I2C_WRITE;
    }

    dev_notice(qsfp->dev, "%s: %s %s OFF\n", __func__,
                         (led_num & QSFP_LED1) ? "LED1 ":"",
                         (led_num & QSFP_LED2) ? "LED2 ":"");
    return ret;
}

/*
 * Api to set fpc port LED Brightness based on user input.
 */
int transceiver_led_brightness_set(u8 fpc_instance, u8 port_num,
                                   u8 led_num, u8 percent)
{
    struct fpc *fpc = NULL;
    struct qsfp *qsfp = NULL;
    int ret = SUCCESS;
    u8 buf = 0;
    u8 pwm_reg = 0;
    u16 pwm_cal_val = 0;

    /* Validation of input data from the caller */
    if (fpc_instance >= FPC_MAX_INSTANCES) {
        pr_err(" %s: Invalid FPC instance input data: %u\n", __func__,
                                                        fpc_instance);
        return ERROR_INVALID_FPC_INSTANCE_INPUT;
    }

    if (port_num >= FPC_MAX_PORTS) {
        pr_err(" %s: Invalid QSFP port number input data: %u\n", __func__,
                                                                port_num);
        return ERROR_INVALID_QSFP_PORT_NUM_INPUT;
    }

    /* For LED1(0001) LED2(0010) and for both led's(0011) */
    if ((led_num == 0) || (led_num > 3)) {
        pr_err(" %s: Invalid LED number input data: %u\n", __func__,
                                                           led_num);
        return ERROR_INVALID_LED_NUM_INPUT;
    }

    if (percent > 100) {
        pr_err(" %s: Invalid PWM register input data: %u\n", __func__,
                                                             percent);
        return ERROR_INVALID_PWM_REG_INPUT;
    }

    if (fpc_global[fpc_instance] == NULL) {
        pr_err(" %s: FPC probe failed for instance: %u ", __func__,
                                                     fpc_instance);
        return ERROR_FPC_PROBE;
    }

    fpc = fpc_global[fpc_instance];
    qsfp = fpc->qsfp[port_num];

    if (qsfp->sm_mod_state < QSFP_MOD_ERROR) {
        dev_notice(qsfp->dev, "%s: QSFP module is not attached\n",
                             __func__);
        return ERROR_QSFP_MODULE_PRESENCE;
    }

    ret = fpc_read(fpc,
          FPC_PORT_REG[FPC_LED_MODE_SELECT][port_num],
          &buf, sizeof(buf));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Fail to read LED mode set register "
                               "ret %d\n", __func__, ret);

        return ERROR_FPC_I2C_READ;
    }

    if (percent == 0) {
        /* User not passed brightness value need to calculate based on
           QSFP maximum data speed supported */
        led_calculate_pwm_value(qsfp, &pwm_reg, true);
    }
    else {
        /* In PWM mode,setting pwm ctrl register to FFh will turn off the LED
           instead of setting max brightness */
        if (percent == 100)
            percent = percent - 1;

        pwm_cal_val = (percent * 255) / 100;
        pwm_reg = (u8)pwm_cal_val;
    }

    if ( led_num & QSFP_LED1) {
        /* configure LED1 Mode Select Register to PWM MODE (0010) */
        buf |= BIT(1);
        buf &= ~BIT(0);

        ret = fpc_write(fpc,
              FPC_PORT_REG[FPC_LED_MODE_SELECT][port_num],
              &buf, sizeof(buf));
        if (ret < 0) {
            dev_err(qsfp->dev, "%s: Fail to write LED mode set register "
                                  "ret %d\n", __func__, ret);
            return ERROR_FPC_I2C_WRITE;
        }

        /* configure LED1 PWM Control Register with user passed value or
           calculated value */
        ret = fpc_write(fpc,
              FPC_PORT_LED_REG[FPC_LED1_PWM_CTRL_REG][port_num],
              &pwm_reg, sizeof(pwm_reg));
        if (ret < 0) {
            dev_err(qsfp->dev, "%s: Fail to write LED1 pwm ctrl reg "
                              "ret %d\n", __func__, ret);
            return ERROR_FPC_I2C_WRITE;
        }
    }

    if ( led_num & QSFP_LED2 ) {
        /* configure LED2 Mode Select Register to PWM MODE (1000) */
        buf |= BIT(3);
        buf &= ~BIT(2);

        ret = fpc_write(fpc,
              FPC_PORT_REG[FPC_LED_MODE_SELECT][port_num],
              &buf, sizeof(buf));
        if (ret < 0) {
            dev_err(qsfp->dev, "%s: Fail to write LED mode "
                                  "ret %d\n", __func__, ret);
            return ERROR_FPC_I2C_WRITE;
        }

        /* configure LED2 PWM Control Register with user passed value or
           calculated value */
        ret = fpc_write(fpc,
              FPC_PORT_LED_REG[FPC_LED2_PWM_CTRL_REG][port_num],
              &pwm_reg, sizeof(pwm_reg));
        if (ret < 0) {
            dev_err(qsfp->dev, "%s: Fail to write LED2 pwm ctrl reg "
                              "ret %d\n", __func__, ret);
            return ERROR_FPC_I2C_WRITE;
        }
    }

    dev_notice(qsfp->dev, "%s: %s %s PWM with Percent: %u\n", __func__,
                          (led_num & QSFP_LED1) ? "LED1 ":"",
                          (led_num & QSFP_LED2) ? "LED2 ":"",
                          percent);
     return ret;
}

/*
 * Api to set fpc port LED Blink on/off/on brightness based on user input.
 */
int transceiver_led_blink_set(u8 fpc_instance, u8 port_num, u8 led_num,
                              u8 on_val, u8 off_val, u8 percent)
{
    struct fpc *fpc = NULL;
    struct qsfp *qsfp = NULL;
    int ret = SUCCESS;
    u8 buf = 0;
    u8 blink_on_time_value  = on_val;
    u8 blink_off_time_value = off_val;
    u8 pwm_reg = 0;
    u16 pwm_cal_val = 0;

    /* Validation of input data from the caller */
    if (fpc_instance >= FPC_MAX_INSTANCES) {
        pr_err(" %s: Invalid FPC instance input data: %u\n", __func__,
                                                        fpc_instance);
        return ERROR_INVALID_FPC_INSTANCE_INPUT;
    }

    if (port_num >= FPC_MAX_PORTS) {
        pr_err(" %s: Invalid QSFP port number input data: %u\n", __func__,
                                                                port_num);
        return ERROR_INVALID_QSFP_PORT_NUM_INPUT;
    }

    /* For LED1(0001) LED2(0010) and for both led's(0011) */
    if ((led_num == 0) || (led_num > 3)) {
        pr_err(" %s: Invalid LED number input data: %u\n", __func__,
                                                           led_num);
        return ERROR_INVALID_LED_NUM_INPUT;
    }

    if (percent > 100) {
        pr_err(" %s: Invalid PWM register input data: %u\n", __func__,
                                                             percent);
        return ERROR_INVALID_PWM_REG_INPUT;
    }

    if (fpc_global[fpc_instance] == NULL) {
        pr_err(" %s: FPC probe failed for instance: %u ", __func__,
                                                     fpc_instance);
        return ERROR_FPC_PROBE;
    }

    fpc = fpc_global[fpc_instance];
    qsfp = fpc->qsfp[port_num];

    if (qsfp->sm_mod_state < QSFP_MOD_ERROR) {
        dev_notice(qsfp->dev, "%s: QSFP module is not attached\n",
                          __func__);
        return ERROR_QSFP_MODULE_PRESENCE;
    }

    ret = fpc_read(fpc,
          FPC_PORT_REG[FPC_LED_MODE_SELECT][port_num],
          &buf, sizeof(buf));
    if (ret < 0) {
        dev_err(qsfp->dev, "%s: Fail to read LED mode "
                               "ret %d\n", __func__, ret);
        return ERROR_FPC_I2C_READ;
    }

    if (percent == 0) {
        /* User not passed brightness value need to calculate based on
           Qsfp maximum data speed supported */
        led_calculate_pwm_value(qsfp,&pwm_reg,false);
    }
    else {
        pwm_cal_val = (percent * 255) / 100;
        pwm_reg = (u8)pwm_cal_val;
    }

    if ( led_num & QSFP_LED1 ) {
        /* configure LED1 Mode Select Register to BLINK MODE (0011)*/
        buf |= FPC_PORT_LED1_SEL_BLINK;

        ret = fpc_write(fpc,
              FPC_PORT_REG[FPC_LED_MODE_SELECT][port_num],
              &buf, sizeof(buf));
        if (ret < 0) {
            dev_err(qsfp->dev, "%s: Fail to write LED1 mode "
                              "ret %d\n", __func__, ret);
            return ERROR_FPC_I2C_WRITE;
        }

        /* configure LED1 PWM Control Register with user passed value or
           calculated value */
        ret = fpc_write(fpc,
              FPC_PORT_LED_REG[FPC_LED1_PWM_CTRL_REG][port_num],
              &pwm_reg, sizeof(pwm_reg));
        if (ret < 0) {
            dev_err(qsfp->dev, "%s: Fail to write LED1 pwm ctrl reg "
                              "ret %d\n", __func__, ret);
            return ERROR_FPC_I2C_WRITE;
        }

        /* configure LED1 Blink Off Time Register with user
           passed off_val */
        ret = fpc_write(fpc,
              FPC_PORT_LED_REG[FPC_LED1_BLINK_OFF_TIME_REG][port_num],
              &blink_off_time_value, sizeof(blink_off_time_value));
        if (ret < 0) {
            dev_err(qsfp->dev, "%s: Fail to write LED1 blink off reg "
                              "ret %d\n", __func__, ret);
            return ERROR_FPC_I2C_WRITE;
        }

        /* configure LED1 Blink On Time Register with user passed on_val. */
        ret = fpc_write(fpc,
              FPC_PORT_LED_REG[FPC_LED1_BLINK_ON_TIME_REG][port_num],
              &blink_on_time_value, sizeof(blink_on_time_value));
        if (ret < 0) {
            dev_err(qsfp->dev, "%s: Fail to write LED1 blink on reg "
                              "ret %d\n", __func__, ret);
            return ERROR_FPC_I2C_WRITE;
        }
    }

    if ( led_num & QSFP_LED2 ) {
        /* configure LED2 Mode Select Register to BLINK MODE (1100)*/
        buf |= FPC_PORT_LED2_SEL_BLINK;

        ret = fpc_write(fpc,
              FPC_PORT_REG[FPC_LED_MODE_SELECT][port_num],
              &buf, sizeof(buf));
        if (ret < 0) {
            dev_err(qsfp->dev, "%s: Fail to write LED2 mode reg "
                              "ret %d\n", __func__, ret);
            return ERROR_FPC_I2C_WRITE;
        }

        /* configure LED2 PWM Control Register with user passed value or
           calculated value */
        ret = fpc_write(fpc,
              FPC_PORT_LED_REG[FPC_LED2_PWM_CTRL_REG][port_num],
              &pwm_reg , sizeof(pwm_reg));
        if (ret < 0) {
            dev_err(qsfp->dev, "%s: Fail to write LED2 pwm ctrl reg "
                              "ret %d\n", __func__, ret);
            return ERROR_FPC_I2C_WRITE;
        }

        /* configure LED2 Blink Off Time Register with
           user passed off_val */
        ret = fpc_write(fpc,
              FPC_PORT_LED_REG[FPC_LED2_BLINK_OFF_TIME_REG][port_num],
              &blink_off_time_value, sizeof(blink_off_time_value));
        if (ret < 0) {
            dev_err(qsfp->dev, "%s: Fail to write LED2 blink off reg "
                              "ret %d\n", __func__, ret);
            return ERROR_FPC_I2C_WRITE;
        }

        /* configure LED2 Blink On Time Register with user passed on_val. */
        ret = fpc_write(fpc,
              FPC_PORT_LED_REG[FPC_LED2_BLINK_ON_TIME_REG][port_num],
              &blink_on_time_value, sizeof(blink_on_time_value));
        if (ret < 0) {
            dev_err(qsfp->dev, "%s: Fail to write LED2 blink on reg "
                              "ret %d\n", __func__, ret);
            return ERROR_FPC_I2C_WRITE;
        }
    }

    dev_notice(qsfp->dev, "%s: %s %s BLINK with ON:OFF:Percent "
                         "%u:%u:%u\n",
                         __func__,
                         (led_num & QSFP_LED1) ? "LED1 ":"",
                         (led_num & QSFP_LED2) ? "LED2 ":"",
                         on_val,off_val,percent);
    return ret;
}

/*
 * Helper function to calculate pwm register value in case of user
 * not provided.
 */
int led_calculate_pwm_value(struct qsfp *qsfp, u8* pwm_reg, bool pwm_mode)
{
    int ret = SUCCESS;
    struct sff8636_eeprom_id *id;
    u8 *spec_id;
    u16 max_data_speed;
    u16 pwm_cal_val;
    spec_id = (u8*)&qsfp->id;

    switch (*spec_id) {
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        /* check Ethernet Compliance Codes page 00h byte 131 */
        if (id->base.ecom_extended == 0x1) {
            mod_link_codes_to_speed(id->ext.link_codes, &max_data_speed);
        }
        else if ((id->base.e10g_base_lrm == 0x1) ||
                 (id->base.e10g_base_lr == 0x1)  ||
                 (id->base.e10g_base_sr == 0x1)) {
            max_data_speed = 10;
        }
        else if ((id->base.e40g_base_cr4 == 0x1) ||
              (id->base.e40g_base_sr4 == 0x1) ||
              (id->base.e40g_base_lr4 == 0x1) ||
              (id->base.e40g_active == 0x1)) {
            max_data_speed = 40;
        }
        else {
            max_data_speed = 400;
        }
        break;
    default:
        /* setting as maximum brightness */
        max_data_speed = 400;
    }

    /* consider QSFP-DD has Maximum data speed ie 400GBPS,
       so make the max value in scale of 0-100 */
    max_data_speed = max_data_speed / 4;

    /* setting the PWM register as 0xFF in PWM Mode will turn off
       the LED insteaded of maximum brightness */
    if ((pwm_mode == true) && (max_data_speed == 100))
        max_data_speed = max_data_speed - 1;

    pwm_cal_val = (max_data_speed * 255) / 100;
    *pwm_reg = (u8)pwm_cal_val;

    dev_notice(qsfp->dev," %s: pwm reg: 0x%02X specid: 0x%02X", __func__,
                      *pwm_reg, *spec_id);
    return ret;
}

/*
 * Function to calculate the maximum supported linkspeed from the linkcodes.
 */
int mod_link_codes_to_speed(unsigned short mod_link_codes, u16* max_data_speed)
{
    switch (mod_link_codes) {
    case 0x00:
    case 0x3F:
    case 0x47 ... 0x49:
    case 0x4B ... 0x4C:
    default:
        /* assign to max dataspeed 400GBPS */
        *max_data_speed = 400;
        break;
    case 0x40 ... 0x44:
    case 0x46:
        /* assign max dataspeed as 200GBPS */
        *max_data_speed = 200;
        break;
    case 0x01 ... 0x08:
    case 0x0B:
    case 0x16 ... 0x1B:
    case 0x20 ... 0x21:
    case 0x25 ... 0x2F:
    case 0x34 ... 0x36:
    case 0x3A:
        /* assign max dataspeed as 100GBPS */
        *max_data_speed = 100;
        break;
    case 0x0C ... 0x0D:
    case 0x4A:
    case 0x45:
        /* assign max dataspeed as 50GBPS */
        *max_data_speed = 50;
        break;
    case 0x10 ... 0x12:
    case 0x1F:
        /* assign max dataspeed as 40GBPS */
        *max_data_speed = 40;
        break;
    /* 0x13 to 0x15 G959.1 profiles need to confirm the speed */
    case 0x39:
        /* assign max dataspeed as 50GBPS */
        *max_data_speed = 50;
        break;
    case 0x38:
        /* assign max dataspeed as 25GBPS */
        *max_data_speed = 25;
        break;
    case 0x1C:
    case 0x37:
        /* assign max dataspeed as 10GBPS */
        *max_data_speed = 10;
        break;
    case 0x1D:
        /* assign max dataspeed as 5GBPS */
        *max_data_speed = 5;
        break;
    case 0x1E:
        /* it is 2.5Gbps rounded to upper value assigning as 3GBPS */
        *max_data_speed = 3;
        break;
    /* 0x22 to 0x24 are of type 4WDM need to check the speed capability*/
    /* 0x30 to 0x33 are of type active Copper or Optical need to check
       the speed capability */
    /* 0x3B-0x3E was Reserved need to be update in future.*/
    /* 0x4D-0x7E was Reserved need to be update in future.*/
    /* 0x82-0xFF Reserved was Reserved need to be update in future.*/
    /* case 0x7F 0x80 0x81 speeds are not clear need to be update in future.*/
    }

    return SUCCESS;
}
