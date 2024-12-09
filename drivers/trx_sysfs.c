/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "trx_sysfs.h"
#include "fpc_led.h"
#include "lane.h"

#define MAX_SYSFS_TRX_FILE_LENGTH (1024)

enum {
    TX_POWER = 0,
    RX_POWER,
    TX_BIAS,
    TEMP_HALRM,
    TEMP_LALRM,
    TEMP_HWARN,
    TEMP_LWARN,
    VOLT_HALRM,
    VOLT_LALRM,
    VOLT_HWARN,
    VOLT_LWARN,
    TXPWR_HALRM,
    TXPWR_LALRM,
    TXPWR_HWARN,
    TXPWR_LWARN,
    RXPWR_HALRM,
    RXPWR_LALRM,
    RXPWR_HWARN,
    RXPWR_LWARN,
    TXI_HALRM,
    TXI_LALRM,
    TXI_HWARN,
    TXI_LWARN,
    LTEMP_HALRM,
    LTEMP_LALRM,
    LTEMP_HWARN,
    LTEMP_LWARN,
};

/* Function to export device state information like
 * port number, link status, and module presence.
 */
static ssize_t trx_state_info_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp = dev_get_drvdata(dev);
    struct lane *lanei;
    ssize_t ret;
    u8 i;

    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    /* Locking necessary to make sure all states fetched once */
    mutex_lock(&qsfp->sm_mutex);

    ret = scnprintf(buf, PAGE_SIZE, "State description: [Module state , "
    "Upstream Device state  , Link state]\nTransceiver port-%u State: [%s  %s"
    "  %s]\n", qsfp->port_num,  mod_state_to_str(qsfp->sm_mod_state),
    dev_state_to_str(qsfp->sm_dev_state),
    link_state_to_str(qsfp->sm_link_state));

    if (qsfp->status.present) {
        char feature_str[QSFP_FEATURE_STR_MAX] = {0};
        qsfp_fill_features_str(qsfp, feature_str, sizeof(feature_str));
        ret += scnprintf(buf + ret, PAGE_SIZE - ret, "Module present: Yes\n"
               "Module probe attempts: %d\nRX LOS: %d\nTX Fault: %d\nETH "
               "Linkup: %d\nPoll status: %s\nFeatures: %s\n",
               PROBE_RETRY - qsfp->sm_mod_tries, qsfp->status.rx_los,
               qsfp->status.tx_fault, qsfp->status.eth_linkup,
               qsfp->need_poll ? "Yes" : "No", feature_str);

    } else {
        ret += scnprintf(buf + ret, PAGE_SIZE - ret, "Module present: No\n");
    }

    for (i = 0 ; i < qsfp->num_lanes; i++) {
        lanei = qsfp->lane[i];

        ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\nLane%u State: [%s  %s"
               "  %s]\n", i, mod_state_to_str(lanei->sm_mod_state),
               dev_state_to_str(lanei->sm_dev_state),
               link_state_to_str(lanei->sm_link_state));

        if (lanei->status.present) {
            ret += scnprintf(buf + ret, PAGE_SIZE - ret, "Lane present: Yes\n"
                   "RX LOS: %d\nTX Fault: %d\nTX Disable: %d\nETH Linkup: %d\n",
                   lanei->status.rx_los, lanei->status.tx_fault,
                   lanei->status.tx_disable, lanei->status.eth_linkup);

        } else {
            ret += scnprintf(buf + ret, PAGE_SIZE - ret, "Lane present: No\n");
        }
    }

#if IS_ENABLED(CONFIG_DEBUG_FS)
    if (qsfp->sim.remove) {
        ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\nSimulation Remove: "
                                                     "Yes\n");
    }

    if (qsfp->sim.flags) {
        ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\nSimulation Flags: "
                                                     "Yes\n");
    }
#endif

    mutex_unlock(&qsfp->sm_mutex);

    return ret;
}

/* Function to show LED ON/OFF status from the FPC402 mode select register
 * through SysFS.
 */
static ssize_t trx_led_on_off_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    unsigned int led1_state = 0;
    unsigned int led2_state = 0;
    u8 lbuff = 0;
    int ret = 0;
    u8 modesel = 0;

    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    ret = fpc_read(qsfp->fpc,
                   FPC_PORT_REG[FPC_LED_MODE_SELECT][qsfp->port_num],
                   &lbuff, sizeof(lbuff));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Fail to read LED mode set register\n");
        return -EINVAL;
    }

    modesel = lbuff & 0x03;
    /* To check LED1 state */
    switch(modesel) {
    case 0x00:
    default:
        led1_state = LED_MODE_OFF;
        break;
    case 0x01:
    case 0x02:
    case 0x03:
        led1_state = LED_MODE_ON;
        break;
    }

    modesel = (lbuff & 0x0C) >> 2;
    /* To check LED2 state */
    switch(modesel) {
    case 0x00:
    default:
        led2_state = LED_MODE_OFF;
        break;
    case 0x01:
    case 0x02:
    case 0x03:
        led2_state = LED_MODE_ON;
        break;
    }

    return scnprintf(buf, MAX_SYSFS_TRX_FILE_LENGTH,"#\n# Specifies port leds on,"
        "off status\n# Led number : LED1:1 LED2:2 both LED1&LED2:3 Any other"
        " value is invalid\n# LED state  : off:0 on:1 Default:0 Any other value"
        " is invalid\n#\nLED1 state: %d\nLED2 state: %d\n",
        led1_state , led2_state);
}

/* Wrapper function to validate the data size from the SysFS file.
 */
int trx_sysfs_validate_and_copy_buf(char *dest_buf,
                      size_t dest_buf_size, char const *source_buf,
                      size_t source_buf_size)
{
    if (source_buf_size > (dest_buf_size - 1)) {
        return -EINVAL;
    }

    strlcpy(dest_buf, source_buf, dest_buf_size);
    if (dest_buf[source_buf_size - 1] == '\n')
        dest_buf[source_buf_size - 1] = '\0';
    return 0;
}

/* Function to store the LED number and state information through
 * SysFS for LED ON/OFF.
 */
static ssize_t trx_led_on_off_store(struct device *dev,
                                   struct device_attribute *attr,
                                   char const *buf, size_t count)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    char buf_local[MAX_SYSFS_TRX_FILE_LENGTH] = {0};
    char led_num_buff[10] = {0};
    char led_state_buff[10] = {0};
    int led_num = 0;
    int led_state = 0;
    int ret = 0;

    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    ret = trx_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
                     buf, count);
    if (ret != 0) {
        TRX_LOG_ERR(qsfp, "SysFS file length is larger than %zu bytes\n",
                                                    sizeof(buf_local));
        return ret;
    }

    sscanf(buf_local,"%s %d %s %d", led_num_buff, &led_num,
                               led_state_buff, &led_state);

    if ((strncmp(led_num_buff,"led_num:",8) != 0) ||
         (strncmp(led_state_buff,"led_state:",10) != 0))
    {
        TRX_LOG_ERR(qsfp, "Invalid data read from file\n");
        return -EINVAL;
    }

    /* Validate input values from the user. */
    if(((led_num < 1) || (led_num > 3)) ||
        ((led_state < LED_MODE_OFF) ||
        (led_state > LED_MODE_ON)))
    {
        TRX_LOG_ERR(qsfp, "Invalid input data led_num: %d,"
                    "led_state: %d\n", led_num, led_state);
        return -EINVAL;
    }

    if(led_state == LED_MODE_ON)
        ret = transceiver_led_on(qsfp, (u8)led_num);
    else
        ret  = transceiver_led_off(qsfp, (u8)led_num);

    if(ret != 0)
        return -EINVAL;
    else
        return count;
}

/* Function to show LED brightness value from the FPC402 register
 * through SysFS.
 */
static ssize_t trx_led_blink_set_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    u8 led1_state = 0;
    u8 led2_state = 0;
    u8 led1_on_time = 0;
    u8 led1_off_time = 0;
    u8 led2_on_time = 0;
    u8 led2_off_time = 0;

    char led1_str[100] = {0};
    char led2_str[100] = {0};

    u8 lbuff = 0;
    int ret = 0;
    u8 modesel = 0;

    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    ret = fpc_read(qsfp->fpc,
                   FPC_PORT_REG[FPC_LED_MODE_SELECT][qsfp->port_num],
                   &lbuff, sizeof(lbuff));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Fail to read LED mode set register\n");
        return -EINVAL;
    }

    modesel = lbuff & 0x03;
    /* To check LED1 state */
    switch(modesel) {
    case 0x00:
    default:
        led1_state = LED_MODE_OFF;
        scnprintf(led1_str, 100, "OFF");
        break;
    case 0x01:
    case 0x02:
        led1_state = LED_MODE_ON;
        scnprintf(led1_str, 100, "ON with no blink");
        break;
    case 0x03:
        led1_state = LED_MODE_BLINK;
        break;
    }

    if(led1_state == LED_MODE_BLINK) {
        ret = fpc_read(qsfp->fpc,
                 FPC_PORT_LED_REG[FPC_LED1_BLINK_ON_TIME_REG][qsfp->port_num],
                 &led1_on_time, sizeof(led1_on_time));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Fail to read LED1 blink on time reg. ret %d\n",
                              ret);
            return -EINVAL;
        }

        ret = fpc_read(qsfp->fpc,
              FPC_PORT_LED_REG[FPC_LED1_BLINK_OFF_TIME_REG][qsfp->port_num],
              &led1_off_time, sizeof(led1_off_time));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Fail to read LED1 blink off time reg. ret %d\n",
                               ret);
            return -EINVAL;
        }

    scnprintf(led1_str, 100, "BLINK with ON_VAL: %d, OFF_VAL: %d",
                             led1_on_time, led1_off_time);
    }

    modesel = (lbuff & 0x0C) >> 2;
    /* To check LED2 state */
    switch(modesel) {
    case 0x00:
    default:
        led2_state = LED_MODE_OFF;
        scnprintf(led2_str, 100, "OFF");
        break;
    case 0x01:
    case 0x02:
        led2_state = LED_MODE_ON;
        scnprintf(led2_str, 100, "ON with no blink");
        break;
     case 0x03:
         led2_state = LED_MODE_BLINK;
         break;
     }

    if(led2_state == LED_MODE_BLINK) {
        ret = fpc_read(qsfp->fpc,
                 FPC_PORT_LED_REG[FPC_LED2_BLINK_ON_TIME_REG][qsfp->port_num],
                 &led2_on_time, sizeof(led2_on_time));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Fail to read LED2 blink on time reg. ret %d\n",
                              ret);
            return -EINVAL;
        }

        ret = fpc_read(qsfp->fpc,
               FPC_PORT_LED_REG[FPC_LED2_BLINK_OFF_TIME_REG][qsfp->port_num],
               &led2_off_time, sizeof(led2_off_time));
        if (ret < 0) {
           TRX_LOG_ERR(qsfp, "Fail to read LED2 blink off time reg. ret %d\n",
                             ret);
           return -EINVAL;
        }

    scnprintf(led2_str, 100, "BLINK with ON_VAL: %d, OFF_VAL: %d",
                                  led2_on_time, led2_off_time);
    }

    return scnprintf(buf, MAX_SYSFS_TRX_FILE_LENGTH,"#\n# Specifies port leds "
           "Blink mode on and off value information\n# LED blink on value: "
           "Minimum:1 Maximum:255 Any other value is invalid\n# LED blink "
           "off value: Minimum:1 Maximum:255 Any other value is "
           "invalid\n#\nLED1 %s\nLED2 %s\n", led1_str , led2_str);
 }

/* Function to store the LED number and on, off, and brightness information
 * through SysFS to set the LED to blink mode.
 */
static ssize_t trx_led_blink_set_store(struct device *dev,
                                   struct device_attribute *attr,
                                   char const *buf, size_t count)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    char buf_local[MAX_SYSFS_TRX_FILE_LENGTH] = {0};

    int led_num = 0;
    char led_num_buff[10] = {0};
    int led_on_val = 0;
    char led_on_val_buff[10] = {0};

    int led_off_val = 0;
    char led_off_val_buff[10] = {0};

    int led_pwm_val = 0;
    char led_pwm_val_buff[10] = {0};

    int ret = 0;

    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    ret = trx_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
                     buf, count);
    if (ret != 0) {
        TRX_LOG_ERR(qsfp, "SysFS file length is larger than %zu bytes\n",
                                                    sizeof(buf_local));
        return ret;
    }

    sscanf(buf_local,"%s %d %s %d %s %d %s %d", led_num_buff, &led_num,
                        led_on_val_buff, &led_on_val, led_off_val_buff,
                         &led_off_val, led_pwm_val_buff, &led_pwm_val);

    if ((strncmp(led_num_buff,"led_num:",8) != 0) ||
         (strncmp(led_on_val_buff,"led_on_val:",11) != 0) ||
         (strncmp(led_off_val_buff,"led_off_val:",12) != 0) ||
         (strncmp(led_pwm_val_buff,"led_pwm_val:",12) != 0))
    {
        TRX_LOG_ERR(qsfp, "Invalid data read from file\n");
        return -EINVAL;
    }

    /* Validate input values from the user. */
    if(((led_num < 1) || (led_num > 3)) ||
        ((led_pwm_val < 0) || (led_pwm_val > 255)) ||
        ((led_off_val < 1) || (led_off_val > 255)) ||
        ((led_on_val < 1) || (led_on_val > 255)))
    {
        TRX_LOG_ERR(qsfp, "Invalid input data led_num: %d,"
                    "led_pwm_val: %d, led_off_val: %d, led_on_val: %d\n",
                    led_num, led_pwm_val, led_off_val, led_on_val);
        return -EINVAL;
    }

    ret = transceiver_led_blink_set(qsfp, (u8)led_num, (u8)led_on_val,
                                    (u8)led_off_val, (u8)led_pwm_val);
    if(ret != 0)
        return -EINVAL;
    else
        return count;
}

/* Function to show LED blink on and blink off value from the FPC402 register
 * through SysFS.
 */
static ssize_t trx_led_brightness_set_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    unsigned int led1_state = 0;
    u8 led1_pwm_val = 0;
    unsigned int led2_state = 0;
    u8 led2_pwm_val = 0;
    char led1_str[50] = {0};
    char led2_str[50] = {0};

    u8 lbuff = 0;
    int ret = 0;
    u8 modesel = 0;

    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    ret = fpc_read(qsfp->fpc,
                   FPC_PORT_REG[FPC_LED_MODE_SELECT][qsfp->port_num],
                   &lbuff, sizeof(lbuff));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "Fail to read LED mode set register\n");
        return -EINVAL;
    }

    modesel = lbuff & 0x03;
    /* To check LED1 state */
    switch(modesel) {
    case 0x00:
    default:
        led1_state = LED_MODE_OFF;
        scnprintf(led1_str, 50, "OFF");
        break;
    case 0x01:
        led1_state = LED_MODE_ON;
        scnprintf(led1_str, 50, "ON with constant, no PWM");
        break;
    case 0x02:
        led1_state = LED_MODE_PWM;
        break;
    case 0x03:
        led1_state = LED_MODE_BLINK;
        break;
    }

    if((led1_state == LED_MODE_PWM) ||
        (led1_state == LED_MODE_BLINK)) {
        ret = fpc_read(qsfp->fpc,
                FPC_PORT_LED_REG[FPC_LED1_PWM_CTRL_REG][qsfp->port_num],
                &led1_pwm_val, sizeof(led1_pwm_val));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Fail to read LED1 pwm ctrl reg. ret %d\n", ret);
            return -EINVAL;
        }

        if(led1_state == LED_MODE_PWM)
            scnprintf(led1_str, 50, "ON with PWM: %d", led1_pwm_val);
        else
            scnprintf(led1_str, 50, "BLINK with PWM: %d", led1_pwm_val);
    }

    modesel = (lbuff & 0x0C) >> 2;
    /* To check LED2 state */
    switch(modesel) {
    case 0x00:
    default:
        led2_state = LED_MODE_OFF;
        scnprintf(led2_str, 50, "OFF");
        break;
    case 0x01:
        led2_state = LED_MODE_ON;
        scnprintf(led2_str, 50, "ON with constant, no PWM");
        break;
    case 0x02:
        led2_state = LED_MODE_PWM;
        break;
    case 0x03:
        led2_state = LED_MODE_BLINK;
        break;
    }

    if( (led2_state == LED_MODE_PWM) ||
        (led2_state == LED_MODE_BLINK) ) {
        ret = fpc_read(qsfp->fpc,
                FPC_PORT_LED_REG[FPC_LED2_PWM_CTRL_REG][qsfp->port_num],
                &led2_pwm_val, sizeof(led2_pwm_val));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "Fail to read LED2 pwm ctrl reg. ret %d\n", ret);
            return -EINVAL;
        }

        if(led2_state == LED_MODE_PWM)
            scnprintf(led2_str, 50, "ON with PWM: %d", led2_pwm_val);
        else
            scnprintf(led2_str, 50, "BLINK with PWM: %d", led2_pwm_val);
    }

    return scnprintf(buf, MAX_SYSFS_TRX_FILE_LENGTH,"#\n# Specifies port leds"
           " PWM and Blink mode brightness value information\n# PWM mode "
           "brightness value: Minimum:0 Maximum:254 Any other value "
           "is invalid\n# Blink mode brightness value: Minimum:0 Maximum:255"
           " Any other value is invalid\n#   If brightness value = 0, the "
           "transceiver driver will internally calculate brightness\n#   "
           "value based on the trx maximum supported speed\n#\nLED1 %s\n"
           "LED2 %s\n", led1_str , led2_str);
}

/* Function to store the LED number and brightness information
 * through SysFS to set the LED to PWM mode.
 */
static ssize_t trx_led_brightness_set_store(struct device *dev,
                                   struct device_attribute *attr,
                                   char const *buf, size_t count)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    char buf_local[MAX_SYSFS_TRX_FILE_LENGTH] = {0};
    int led_num = 0;
    char led_num_buff[10] = {0};

    int led_pwm_val = 0;
    char led_pwm_val_buff[10] = {0};

    int ret = 0;

    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    ret = trx_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
                     buf, count);
    if (ret != 0) {
        TRX_LOG_ERR(qsfp, "SysFS file length is larger than %zu bytes\n",
                                                    sizeof(buf_local));
        return ret;
    }

    sscanf(buf_local,"%s %d %s %d", led_num_buff, &led_num,
                           led_pwm_val_buff, &led_pwm_val);

    if ((strncmp(led_num_buff,"led_num:",8) != 0) ||
         (strncmp(led_pwm_val_buff,"led_pwm_val:",12) != 0))
    {
        TRX_LOG_ERR(qsfp, "Invalid data read from file\n");
        return -EINVAL;
    }

    /* Validate input values from the user. */
    if(((led_num < 1) || (led_num > 3)) ||
       ((led_pwm_val < 0)|| (led_pwm_val > 254)))
    {
        TRX_LOG_ERR(qsfp, "Invalid input data led_num: %d,"
                    "led_pwm_val: %d\n", led_num, led_pwm_val);
        return -EINVAL;
    }

    ret = transceiver_led_brightness_set(qsfp, (u8)led_num, (u8)led_pwm_val);
    if(ret != 0)
        return -EINVAL;
    else
        return count;
}

/* Wraper function to write specification ID information to the SysFS buffer.
 */
inline ssize_t sysfs_spec_info_print(char *buf, u8 spec_id)
{
    if (spec_id == 0x00) {
        return scnprintf(buf, PAGE_SIZE, "Specification Identifier {0x%02X}\n"
                      "Unknown module\n",spec_id);
    }
    else {
        return scnprintf(buf, PAGE_SIZE, "Specification Identifier {0x%02X}\n"
                      "%s Transceiver module is not supported\n",
                      spec_id,mod_identifier_to_str(spec_id));
    }
}

/* Function to export transceiver device temperature information to Sysfs.
 */
static ssize_t trx_temperature_show(struct device *dev,
                       struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id;
    struct sfp_eeprom_id *id;
    char temperature_data[75] = {0};
    int16_t tempc = 0;
    int ret = 0;

    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing. */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return scnprintf(buf, PAGE_SIZE,"QSFP transceiver not inserted\n");
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
                return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
            }

            /* Check for Internal calibration for DDM supported SFP,
             * Address A0h, Byte 92 Bit 5.
             */
            if(id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
            {
                return scnprintf(buf, PAGE_SIZE, "%s\n",
                          calc_common_temperature(tempc,
                          temperature_data));
            }
            else /* SFP supported External calibration */
            {
                return scnprintf(buf, PAGE_SIZE, "%s\n",
                       calc_external_calib_temperature(qsfp, tempc,
                       temperature_data));
            }
        }
        else
        {
            return scnprintf(buf, PAGE_SIZE, "TRX temperature measurement not "
                                "supported on non-DDM transceiver devices.\n");
        }
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        if (!qsfp->support.temp_flags) {
            return scnprintf(buf, PAGE_SIZE, "TRX temperature measurement not "
                             "supported on non-DDM transceiver devices.\n");
        }
        /* Page 00h Bytes 22-23 */
        ret = qsfp_read(qsfp, SFF8636_TEMPERATURE, &tempc,
                                           sizeof(tempc));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE,"QSFP read error: %d\n", ret);
        }

        return scnprintf(buf, PAGE_SIZE, "%s\n", calc_common_temperature(tempc,
                                         temperature_data));
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            return scnprintf(buf, PAGE_SIZE,"TRX temperature measurement"
                                            " not supported\n");
        }

        /*Support advertised in page 01h:159.0 */
        if (!cmis_id->ext.temp_mon_sup) {
            return scnprintf(buf, PAGE_SIZE, "TRX temperature measurement"
                                             " not supported\n");
        }

        /* Page 00h Bytes 14-15 */
        ret = qsfp_read(qsfp, CMIS_MOD_TEMPMON, &tempc,
                               sizeof(tempc));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
        }

        return scnprintf(buf, PAGE_SIZE, "%s\n", calc_common_temperature(tempc,
                                         temperature_data));
    default:
        return sysfs_spec_info_print(buf,*spec_id);
    }
}

/* Function to export transceiver device supply voltage information to Sysfs.
 */
static ssize_t trx_supply_voltage_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    struct cmis_eeprom_id *cmis_id;
    struct sfp_eeprom_id *id;
    u8 *spec_id;
    char voltage_data[75] = {0};
    u16 supply_voltage_t = 0;
    int ret = 0;

    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing. */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return scnprintf(buf, PAGE_SIZE, "QSFP transceiver not inserted\n");
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
                return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
            }

            /* Check for Internal calibration for DDM supported SFP,
             * Address A0h, Byte 92 Bit 5.
             */
            if(id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
            {
                return scnprintf(buf, PAGE_SIZE, "%s\n",
                       calc_common_svoltage(supply_voltage_t,
                       voltage_data));
            }
            else /* SFP supported External calibration */
            {
                return scnprintf(buf, PAGE_SIZE, "%s\n",
                                 calc_external_calib_svoltage(qsfp,
                                 supply_voltage_t,
                                 voltage_data));
            }
        }
        else
        {
            return scnprintf(buf, PAGE_SIZE,"TRX supply voltage measurement "
                          "not supported on non-DDM transceiver devices.\n");
        }
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        if (!qsfp->support.volt_flags) {
            return scnprintf(buf, PAGE_SIZE,"TRX supply voltage measurement "
                             "not supported on non-DDM transceiver devices.\n");
        }
        /* Page 00h Bytes 26-27 */
        ret = qsfp_read(qsfp, SFF8636_SUPPLY_VOLTAGE, &supply_voltage_t,
                              sizeof(supply_voltage_t));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
        }

        return scnprintf(buf, PAGE_SIZE, "%s\n",
                              calc_common_svoltage(supply_voltage_t,
                              voltage_data));
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            return scnprintf(buf, PAGE_SIZE,"TRX supply voltage measurement"
                                            " not supported\n");
        }

        /*Support advertised in page 01h:159.1 */
        if (!cmis_id->ext.volt_mon_sup) {
            return scnprintf(buf, PAGE_SIZE,"TRX supply voltage measurement"
                                            " not supported\n");
        }

        /* Page 00h Bytes 16-17 */
        ret = qsfp_read(qsfp, CMIS_MOD_VCCMON, &supply_voltage_t,
                              sizeof(supply_voltage_t));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
        }

        return scnprintf(buf, PAGE_SIZE,"%s\n",
                              calc_common_svoltage(supply_voltage_t,
                              voltage_data));
    default:
        return sysfs_spec_info_print(buf,*spec_id);
    }
}

/* Function to export transceiver device Channel Monitor value of Rx
 * Power information to Sysfs.
 */
static ssize_t trx_rx_power_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    struct cmis_eeprom_id *cmis_id;
    struct sfp_eeprom_id *sff8472_id;
    u8 *spec_id;
    char rx_power_data[500] ={0};
    u16 rx_power_t[4] = {0};
    u8  rx_power[8] = {0};
    u8  sfp_rx_power[2] = {0};
    u16 cmis_rx_power_t[8] = {0};
    u8  cmis_rx_power[16] = {0};
    int ret = 0;

    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing. */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return scnprintf(buf, PAGE_SIZE, "QSFP transceiver not inserted\n");
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
                return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
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
                return scnprintf(buf, PAGE_SIZE, "%s\n", rx_power_data);
            }
            else /* SFP supported External calibration */
            {
                /* The procedure to calculate Rx power in the case of
                 * external calibration was not clear, and what Rx_PWR_ADe4-1
                 * signifies was not clear. We need to revisit this later.
                 */
                return scnprintf(buf, PAGE_SIZE, "TRX optical rx power"
                       " measurement not supported for external calibration"
                       " type.\n");
            }
        }
        else
        {
            return scnprintf(buf, PAGE_SIZE, "TRX optical rx power measurement"
                           " not supported on non-DDM transceiver devices.\n");
        }
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        if (!qsfp->support.rx_power_flags) {
            return scnprintf(buf, PAGE_SIZE, "TRX optical rx power measurement"
                           " not supported on non-DDM transceiver devices.\n");
        }
        /* Page 00h Bytes 34-41 */
        ret = qsfp_read(qsfp, SFF8636_RX_POWER, rx_power,
                              sizeof(rx_power));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE,"QSFP read error: %d\n", ret);
        }

        rx_power_t[0] = (( rx_power[0] << 8) | rx_power[1]);
        rx_power_t[1] = (( rx_power[2] << 8) | rx_power[3]);
        rx_power_t[2] = (( rx_power[4] << 8) | rx_power[5]);
        rx_power_t[3] = (( rx_power[6] << 8) | rx_power[7]);

        /* rx power in milliwatts (rx_power_t * 0.1 μW/ 1000) */
        scnprintf(rx_power_data,500,"Rx Power Lane1: %d.%03d mW\n"
                                    "Rx Power Lane2: %d.%03d mW\n"
                                    "Rx Power Lane3: %d.%03d mW\n"
                                    "Rx Power Lane4: %d.%03d mW",
                          rx_power_t[0]/10000,rx_power_t[0]%10000,
                          rx_power_t[1]/10000,rx_power_t[1]%10000,
                          rx_power_t[2]/10000,rx_power_t[2]%10000,
                          rx_power_t[3]/10000,rx_power_t[3]%10000);
        return scnprintf(buf, PAGE_SIZE, "%s\n", rx_power_data);
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            return scnprintf(buf, PAGE_SIZE,"TRX optical rx power "
                                     "measurement not supported\n");
        }

        /*Support advertised in page 01h:160.2 */
        if(!cmis_id->ext.rx_optical_pow_mon_sup ) {
            return scnprintf(buf, PAGE_SIZE, "TRX optical rx power "
                                     "measurement not supported\n");
        }

        /* Page 11h Bytes 186-201 */
        ret = qsfp_read(qsfp, CMIS_RX_POWER, cmis_rx_power,
                              sizeof(cmis_rx_power));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
        }

        cmis_rx_power_t[0] = (( cmis_rx_power[0] << 8) | cmis_rx_power[1]);
        cmis_rx_power_t[1] = (( cmis_rx_power[2] << 8) | cmis_rx_power[3]);
        cmis_rx_power_t[2] = (( cmis_rx_power[4] << 8) | cmis_rx_power[5]);
        cmis_rx_power_t[3] = (( cmis_rx_power[6] << 8) | cmis_rx_power[7]);
        cmis_rx_power_t[4] = (( cmis_rx_power[8] << 8) | cmis_rx_power[9]);
        cmis_rx_power_t[5] = (( cmis_rx_power[10] << 8) | cmis_rx_power[11]);
        cmis_rx_power_t[6] = (( cmis_rx_power[12] << 8) | cmis_rx_power[13]);
        cmis_rx_power_t[7] = (( cmis_rx_power[14] << 8) | cmis_rx_power[15]);

        /* rx power in milliwatts (rx_power_t * 0.1 μW/ 1000) */
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
        return scnprintf(buf, PAGE_SIZE, "%s\n", rx_power_data);
    default:
        return sysfs_spec_info_print(buf,*spec_id);
    }
}

/* Function to export transceiver device Channel Monitor value of Tx
 * Bias Current information to Sysfs.
 */
static ssize_t trx_tx_bias_current_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    struct cmis_eeprom_id *cmis_id;
    struct sfp_eeprom_id *sff8472_id;
    u8 *spec_id;
    char tx_bias_current_data[500] = {0};
    u32 tx_bias_current_t[4] = {0};
    u16 tx_bias_current = 0;
    u8 tx_bias[8] = {0};
    u8 cmis_tx_bias[16] = {0};
    u8 sfp_tx_bias[2] = {0};
    u32 cmis_tx_bias_current_t[8] = {0};
    u8 cmis_tx_bias_multiplier = 1;
    int ret = 0;

    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing. */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return scnprintf(buf, PAGE_SIZE, "QSFP transceiver not inserted\n");
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
                return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
            }

            tx_bias_current = (( sfp_tx_bias[0] << 8) | sfp_tx_bias[1]);

            /* Check for Internal calibration for DDM supported SFP,
             * Address A0h, Byte 92 Bit 5.
             */
            if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
            {
                /* tx_bias_current in  Micro Amp */
                tx_bias_current_t[0] = tx_bias_current * 2;
                scnprintf(tx_bias_current_data,500,"Tx Bias Current "
                          "Lane1: %d.%03d mA",tx_bias_current_t[0]/1000,
                          tx_bias_current_t[0]%1000);
                return scnprintf(buf, PAGE_SIZE, "%s\n", tx_bias_current_data);
            }
            else /* SFP supported External calibration */
            {
                return scnprintf(buf, PAGE_SIZE, "%s\n",
                                 calc_external_calib_txi(qsfp, tx_bias_current,
                                 tx_bias_current_data));
            }
        }
        else
        {
            return scnprintf(buf, PAGE_SIZE,"TRX tx bias current measurement"
                         " not supported on non-DDM transceiver devices.\n");
        }
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        if (!qsfp->support.tx_bias_flags) {
            return scnprintf(buf, PAGE_SIZE,"TRX tx bias current measurement"
                         " not supported on non-DDM transceiver devices.\n");
        }
        /* Page 00h Bytes 42-49 */
        ret = qsfp_read(qsfp, SFF8636_TX_BIAS, tx_bias,
                              sizeof(tx_bias));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
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
        return scnprintf(buf, PAGE_SIZE, "%s\n", tx_bias_current_data);
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            return scnprintf(buf, PAGE_SIZE, "TRX tx bias current measurement "
                                                            "not supported\n");
        }

        /*Support advertised in page 01h:160.0 */
        if(!cmis_id->ext.tx_bias_mon_sup ) {
            return scnprintf(buf, PAGE_SIZE, "TRX tx bias current measurement "
                                                            "not supported\n");
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
        /* reserved case is not expected from OIF-CMIS-05.2 Rev */
        case 0x03:
            return scnprintf(buf, PAGE_SIZE, "TRX tx bias current multiplier "
                             "was reserved not expected for OIF-CMIS-05.2\n");
        }

        /* Page 11h Bytes 170-185 */
        ret = qsfp_read(qsfp, CMIS_TX_BIAS, cmis_tx_bias,
                              sizeof(cmis_tx_bias));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
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
        return scnprintf(buf, PAGE_SIZE, "%s\n", tx_bias_current_data);
    default:
        return sysfs_spec_info_print(buf,*spec_id);
    }
}

/* Function to export transceiver device Channel Monitor value of Tx
 * Power information to Sysfs.
 */
static ssize_t trx_tx_power_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    struct sff8636_eeprom_id *id;
    struct cmis_eeprom_id *cmis_id;
    struct sfp_eeprom_id *sff8472_id;
    u8 *spec_id;
    char tx_power_data[500] = {0};
    u16 tx_power_t[4] = {0};
    u8  tx_power[8] = {0};
    u8  sfp_tx_power[2] = {0};
    u8  diagmon;
    u8  cmis_tx_power[16] = {0};
    u16 cmis_tx_power_t[8] = {0};
    int ret = 0;

    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return scnprintf(buf, PAGE_SIZE,"QSFP transceiver not inserted\n");
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
                return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
            }

            tx_power_t[0] = (( sfp_tx_power[0] << 8) | sfp_tx_power[1]);

            /* Check for Internal calibration for DDM supported SFP,
             * Address A0h, Byte 92 Bit 5.
             */
            if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
            {
                /* tx power in milliWatts (tx_power_t * 0.1 μW/ 1000) */
                scnprintf(tx_power_data,500,"Tx Power Lane1: %d.%03d mW",
                                tx_power_t[0]/10000,tx_power_t[0]%10000);
                return scnprintf(buf, PAGE_SIZE, "%s\n", tx_power_data);
            }
            else /* SFP supported External calibration */
            {
                return scnprintf(buf, PAGE_SIZE, "%s\n",
                                 calc_external_calib_txpwr(qsfp, tx_power_t[0],
                                 tx_power_data));
            }
        }
        else
        {
            return scnprintf(buf, PAGE_SIZE, "Transmitter power measurement"
                        " not supported on non-DDM transceiver devices.\n");
        }
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        id = &qsfp->id.sff8636;
        diagmon = id->ext.diagmon;
        if(!(diagmon & BIT(2))) {
            return scnprintf(buf, PAGE_SIZE, "Transmitter power measurement"
                                             " not supported\n");
        }
        /* Page 00h Bytes 50-57 */
        ret = qsfp_read(qsfp, SFF8636_TX_POWER, tx_power,
                              sizeof(tx_power));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
        }

        tx_power_t[0] = (( tx_power[0] << 8) | tx_power[1]);
        tx_power_t[1] = (( tx_power[2] << 8) | tx_power[3]);
        tx_power_t[2] = (( tx_power[4] << 8) | tx_power[5]);
        tx_power_t[3] = (( tx_power[6] << 8) | tx_power[7]);

        /* tx power in milliwatts (tx_power_t * 0.1 μW/ 1000) */
        scnprintf(tx_power_data,500,"Tx Power Lane1: %d.%03d mW\n"
                                    "Tx Power Lane2: %d.%03d mW\n"
                                    "Tx Power Lane3: %d.%03d mW\n"
                                    "Tx Power Lane4: %d.%03d mW",
                          tx_power_t[0]/10000,tx_power_t[0]%10000,
                          tx_power_t[1]/10000,tx_power_t[1]%10000,
                          tx_power_t[2]/10000,tx_power_t[2]%10000,
                          tx_power_t[3]/10000,tx_power_t[3]%10000);
        return scnprintf(buf, PAGE_SIZE, "%s\n", tx_power_data);
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            return scnprintf(buf, PAGE_SIZE, "Transmitter power measurement"
                                             " not supported\n");
        }

        /*Support advertised in page 01h:160.1 */
        if(!cmis_id->ext.tx_optical_pow_mon_sup) {
            return scnprintf(buf, PAGE_SIZE, "Transmitter power measurement "
                                             "not supported\n");
        }

        /* Page 11h Bytes 154-169 */
        ret = qsfp_read(qsfp, CMIS_TX_POWER, cmis_tx_power,
                              sizeof(cmis_tx_power));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
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
        return scnprintf(buf, PAGE_SIZE, "%s\n", tx_power_data);
    default:
        return sysfs_spec_info_print(buf,*spec_id);
    }
}


ssize_t calc_external_calib_ddm_sys(struct qsfp *qsfp, char *buf,
                 struct sff8472_ddm_thresholds* ddm_limits)
{
    struct sff8472_temp_diag temp_ext_cal = {0};
    struct sff8472_vcc_diag vcc_ext_cal = {0};
    struct sff8472_txi_diag txi_ext_cal = {0};
    struct sff8472_txpwr_diag txpwr_ext_cal = {0};
    int ret = 0;

    ret = qsfp_read(qsfp, SFF8472_TEMP_EXT, &temp_ext_cal,
                        sizeof(temp_ext_cal));
    if (ret < 0) {
        return scnprintf(buf, PAGE_SIZE,"QSFP read error for temperature "
                            "external calibration constants: %d\n\n", ret);
    }

    ret = qsfp_read(qsfp, SFF8472_VCC_EXT, &vcc_ext_cal,
                                   sizeof(vcc_ext_cal));
    if (ret < 0) {
        return scnprintf(buf, PAGE_SIZE,"QSFP read error for supply voltage"
                            " external calibration constants: %d\n\n", ret);
    }

    ret = qsfp_read(qsfp, SFF8472_TXPWR_EXT, &txpwr_ext_cal,
                                     sizeof(txpwr_ext_cal));
    if (ret < 0) {
        return scnprintf(buf, PAGE_SIZE,"QSFP read error for tx power external"
                                        " calibration constants: %d\n\n", ret);
    }

    ret = qsfp_read(qsfp, SFF8472_TXI_EXT, &txi_ext_cal,
                                   sizeof(txi_ext_cal));
    if (ret < 0) {
        return scnprintf(buf, PAGE_SIZE,"QSFP read error for tx bias current "
                           "of external calibration constants: %d\n\n", ret);
    }

    return scnprintf(buf, PAGE_SIZE, "************ temperature threshold "
    "limits ************\ntemp_high_alarm: %d °C \ntemp_low_alarm : %d °C "
    "\ntemp_high_warn : %d °C \ntemp_low_warn  : %d °C \n\n"
    "********** supply voltage threshold limits ***********\n"
    "volt_high_alarm: %d mV \nvolt_low_alarm : %d mV \n"
    "volt_high_warn : %d mV \nvolt_low_warn  : %d mV \n\n"
    "************* tx power threshold limits **************\n"
    "txpwr_high_alarm: %d µW \ntxpwr_low_alarm : %d µW \n"
    "txpwr_high_warn : %d µW \ntxpwr_low_warn: %d µW \n\n"
    "************** tx bias threshold limits **************\n"
    "bias_high_alarm: %d mA \nbias_low_alarm : %d mA \n"
    "bias_high_warn : %d mA \nbias_low_warn  : %d mA \n\n",
    trx_ext_temp_ddm(ddm_limits->temp_high_alarm, &temp_ext_cal),
    trx_ext_temp_ddm(ddm_limits->temp_low_alarm, &temp_ext_cal),
    trx_ext_temp_ddm(ddm_limits->temp_high_warn, &temp_ext_cal),
    trx_ext_temp_ddm(ddm_limits->temp_low_warn, &temp_ext_cal),
    trx_ext_vcc_ddm(ddm_limits->volt_high_alarm, &vcc_ext_cal),
    trx_ext_vcc_ddm(ddm_limits->volt_low_alarm, &vcc_ext_cal),
    trx_ext_vcc_ddm(ddm_limits->volt_high_warn, &vcc_ext_cal),
    trx_ext_vcc_ddm(ddm_limits->volt_low_warn, &vcc_ext_cal),
    trx_ext_ddm_power(ddm_limits->txpwr_high_alarm, &txpwr_ext_cal),
    trx_ext_ddm_power(ddm_limits->txpwr_low_alarm, &txpwr_ext_cal),
    trx_ext_ddm_power(ddm_limits->txpwr_high_warn, &txpwr_ext_cal),
    trx_ext_ddm_power(ddm_limits->txpwr_low_warn, &txpwr_ext_cal),
    trx_ext_ddm_txbias(ddm_limits->bias_high_alarm, &txi_ext_cal),
    trx_ext_ddm_txbias(ddm_limits->bias_low_alarm, &txi_ext_cal),
    trx_ext_ddm_txbias(ddm_limits->bias_high_warn, &txi_ext_cal),
    trx_ext_ddm_txbias(ddm_limits->bias_low_warn, &txi_ext_cal));
}

static inline ssize_t spec_info_print(char *buf, u8 spec_id)
{
    if (spec_id == 0x00) {
        return scnprintf(buf, PAGE_SIZE, "Specification Identifier {0x%02X}\n"
                                         "Unknown module\n",spec_id);
    } else {
        return scnprintf(buf, PAGE_SIZE, "Specification Identifier {0x%02X}\n"
                        "%s Transceiver module is not supported\n",
                        spec_id, mod_identifier_to_str(spec_id));
    }
}

static ssize_t trx_vendor_info_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    struct sfp_eeprom_id *sff8472_id;
    struct sff8636_eeprom_id *sff8636_id;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id;

    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return scnprintf(buf, PAGE_SIZE, "QSFP transceiver not inserted\n");
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;

        return scnprintf(buf, PAGE_SIZE, "vendor name: %.*s\nvendor pn: %.*s\n"
        "vendor rev: %.*s\nvendor sn: %.*s\n",
        (int)sizeof(sff8472_id->base.vendor_name), sff8472_id->base.vendor_name,
        (int)sizeof(sff8472_id->base.vendor_pn), sff8472_id->base.vendor_pn,
        (int)sizeof(sff8472_id->base.vendor_rev), sff8472_id->base.vendor_rev,
        (int)sizeof(sff8472_id->ext.vendor_sn), sff8472_id->ext.vendor_sn);

    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        sff8636_id = &qsfp->id.sff8636;

        return scnprintf(buf, PAGE_SIZE, "vendor name: %.*s\nvendor pn: %.*s\n"
        "vendor rev: %.*s\nvendor sn: %.*s\n",
        (int)sizeof(sff8636_id->base.vendor_name), sff8636_id->base.vendor_name,
        (int)sizeof(sff8636_id->base.vendor_pn), sff8636_id->base.vendor_pn,
        (int)sizeof(sff8636_id->base.vendor_rev), sff8636_id->base.vendor_rev,
        (int)sizeof(sff8636_id->ext.vendor_sn), sff8636_id->ext.vendor_sn);

    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        return scnprintf(buf, PAGE_SIZE, "vendor name: %.*s\nvendor pn: %.*s\n"
        "vendor rev: %.*s\nvendor sn: %.*s\n",
        (int)sizeof(cmis_id->base.vendor_name), cmis_id->base.vendor_name,
        (int)sizeof(cmis_id->base.vendor_pn), cmis_id->base.vendor_pn,
        (int)sizeof(cmis_id->base.vendor_rev), cmis_id->base.vendor_rev,
        (int)sizeof(cmis_id->base.vendor_sn), cmis_id->base.vendor_sn);

    default:
        return spec_info_print(buf, *spec_id);
    }
}

/* Function to export transceiver ddm threshold values information to Sysfs.
 */
static ssize_t trx_ddm_thresholds_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct qsfp *qsfp= dev_get_drvdata(dev);
    struct sfp_eeprom_id *sff8472_id;
    u8 *spec_id;
    struct sff8472_ddm_thresholds ddm_limits = {0};
    struct sff8636_ddm_thresholds sff8636_ddm_limits = {0};
    struct cmis_thresholds cmis_ddm_limits = {0};
    int ret = 0;

    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing  */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return scnprintf(buf, PAGE_SIZE,"QSFP transceiver not inserted\n");
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;

        if(!(sff8472_id->ext.enhopts & SFP_ENHOPTS_ALARMWARN))
        {
            return scnprintf(buf, PAGE_SIZE, "TRX does not support alarm and "
                                                "warning threshold limits\n");
        }

        /* Address A2h, Bytes 0-39 */
        ret = qsfp_read(qsfp, SFF8472_DDM_TH, &ddm_limits,
                                     sizeof(ddm_limits));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
        }

        if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_EXT_CAL)
        {
           return calc_external_calib_ddm_sys(qsfp, buf, &ddm_limits);
        }
        else /* SFP supported Internal calibration */
        {
            return scnprintf(buf, PAGE_SIZE, "************ temperature "
                    "threshold limits ************\n"
                    "temp_high_alarm: %d °C \ntemp_low_alarm : %d °C "
                    "\ntemp_high_warn : %d °C \ntemp_low_warn  : %d °C \n\n"
                    "********** supply voltage threshold limits ***********\n"
                    "volt_high_alarm: %d mV \nvolt_low_alarm : %d mV \n"
                    "volt_high_warn : %d mV \nvolt_low_warn : %d mV \n\n"
                    "************* tx power threshold limits **************\n"
                    "txpwr_high_alarm: %d µW \ntxpwr_low_alarm : %d µW \n"
                    "txpwr_high_warn : %d µW \ntxpwr_low_warn  : %d µW \n\n"
                    "************* rx power threshold limits **************\n"
                    "rxpwr_high_alarm: %d µW \nrxpwr_low_alarm : %d µW \n"
                    "rxpwr_high_warn : %d µW \nrxpwr_low_warn  : %d µW \n\n"
                    "************** tx bias threshold limits **************\n"
                    "bias_high_alarm: %d mA \nbias_low_alarm : %d mA \n"
                    "bias_high_warn : %d mA \nbias_low_warn : %d mA \n\n",
                    trx_calibrate_temp(ddm_limits.temp_high_alarm),
                    trx_calibrate_temp(ddm_limits.temp_low_alarm),
                    trx_calibrate_temp(ddm_limits.temp_high_warn),
                    trx_calibrate_temp(ddm_limits.temp_low_warn),
                    trx_calibrate_vcc(ddm_limits.volt_high_alarm),
                    trx_calibrate_vcc(ddm_limits.volt_low_alarm),
                    trx_calibrate_vcc(ddm_limits.volt_high_warn),
                    trx_calibrate_vcc(ddm_limits.volt_low_warn),
                    trx_calibrate_power(ddm_limits.txpwr_high_alarm),
                    trx_calibrate_power(ddm_limits.txpwr_low_alarm),
                    trx_calibrate_power(ddm_limits.txpwr_high_warn),
                    trx_calibrate_power(ddm_limits.txpwr_low_warn),
                    trx_calibrate_power(ddm_limits.rxpwr_high_alarm),
                    trx_calibrate_power(ddm_limits.rxpwr_low_alarm),
                    trx_calibrate_power(ddm_limits.rxpwr_high_warn),
                    trx_calibrate_power(ddm_limits.rxpwr_low_warn),
                    trx_calibrate_txbias(ddm_limits.bias_high_alarm),
                    trx_calibrate_txbias(ddm_limits.bias_low_alarm),
                    trx_calibrate_txbias(ddm_limits.bias_high_warn),
                    trx_calibrate_txbias(ddm_limits.bias_low_warn));
        }
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:

        /* Page 00h, Byte-2 Bit-2 */
        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            return scnprintf(buf, PAGE_SIZE, "TRX does not support alarm and "
                                                "warning threshold limits\n");
        }

        /* Page 03h Bytes 128-199 */
        ret = qsfp_read(qsfp, SFF8636_DDM_TH, &sff8636_ddm_limits,
                                     sizeof(sff8636_ddm_limits));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
        }

        return scnprintf(buf, PAGE_SIZE, "************ temperature "
                 "threshold limits ************\n"
                "temp_high_alarm: %d °C \ntemp_low_alarm : %d °C "
                "\ntemp_high_warn : %d °C \ntemp_low_warn  : %d °C \n\n"
                "********** supply voltage threshold limits ***********\n"
                "volt_high_alarm: %d mV \nvolt_low_alarm : %d mV \n"
                "volt_high_warn : %d mV \nvolt_low_warn : %d mV \n\n"
                "************* tx power threshold limits **************\n"
                "txpwr_high_alarm: %d µW \ntxpwr_low_alarm : %d µW \n"
                "txpwr_high_warn : %d µW \ntxpwr_low_warn  : %d µW \n\n"
                "************* rx power threshold limits **************\n"
                "rxpwr_high_alarm: %d µW \nrxpwr_low_alarm : %d µW \n"
                "rxpwr_high_warn : %d µW \nrxpwr_low_warn  : %d µW \n\n"
                "************** tx bias threshold limits **************\n"
                "bias_high_alarm: %d mA \nbias_low_alarm : %d mA \n"
                "bias_high_warn : %d mA \nbias_low_warn : %d mA \n\n",
                trx_calibrate_temp(sff8636_ddm_limits.temp_high_alarm),
                trx_calibrate_temp(sff8636_ddm_limits.temp_low_alarm),
                trx_calibrate_temp(sff8636_ddm_limits.temp_high_warn),
                trx_calibrate_temp(sff8636_ddm_limits.temp_low_warn),
                trx_calibrate_vcc(sff8636_ddm_limits.volt_high_alarm),
                trx_calibrate_vcc(sff8636_ddm_limits.volt_low_alarm),
                trx_calibrate_vcc(sff8636_ddm_limits.volt_high_warn),
                trx_calibrate_vcc(sff8636_ddm_limits.volt_low_warn),
                trx_calibrate_power(sff8636_ddm_limits.txpwr_high_alarm),
                trx_calibrate_power(sff8636_ddm_limits.txpwr_low_alarm),
                trx_calibrate_power(sff8636_ddm_limits.txpwr_high_warn),
                trx_calibrate_power(sff8636_ddm_limits.txpwr_low_warn),
                trx_calibrate_power(sff8636_ddm_limits.rxpwr_high_alarm),
                trx_calibrate_power(sff8636_ddm_limits.rxpwr_low_alarm),
                trx_calibrate_power(sff8636_ddm_limits.rxpwr_high_warn),
                trx_calibrate_power(sff8636_ddm_limits.rxpwr_low_warn),
                trx_calibrate_txbias(sff8636_ddm_limits.bias_high_alarm),
                trx_calibrate_txbias(sff8636_ddm_limits.bias_low_alarm),
                trx_calibrate_txbias(sff8636_ddm_limits.bias_high_warn),
                trx_calibrate_txbias(sff8636_ddm_limits.bias_low_warn));
    case SFF8024_ID_QSFPDD_CMIS:
        /* Page 00h, Byte-2 Bit-7 */
        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            return scnprintf(buf, PAGE_SIZE,"TRX does not support alarm and "
                                                "warning threshold limits\n");
        }

        /* Page 02h Bytes 128-199 */
        ret = qsfp_read(qsfp, CMIS_DDM_TH, &cmis_ddm_limits,
                                   sizeof(cmis_ddm_limits));
        if (ret < 0) {
            return scnprintf(buf, PAGE_SIZE, "QSFP read error: %d\n", ret);
        }

        return scnprintf(buf, PAGE_SIZE, "************ temperature "
                 "threshold limits ************\n"
                "temp_high_alarm: %d °C \ntemp_low_alarm : %d °C "
                "\ntemp_high_warn : %d °C \ntemp_low_warn  : %d °C \n\n"
                "********** supply voltage threshold limits ***********\n"
                "volt_high_alarm: %d mV \nvolt_low_alarm : %d mV \n"
                "volt_high_warn : %d mV \nvolt_low_warn : %d mV \n\n"
                "************* tx power threshold limits **************\n"
                "txpwr_high_alarm: %d µW \ntxpwr_low_alarm : %d µW \n"
                "txpwr_high_warn : %d µW \ntxpwr_low_warn  : %d µW \n\n"
                "************* rx power threshold limits **************\n"
                "rxpwr_high_alarm: %d µW \nrxpwr_low_alarm : %d µW \n"
                "rxpwr_high_warn : %d µW \nrxpwr_low_warn  : %d µW \n\n"
                "************** tx bias threshold limits **************\n"
                "bias_high_alarm: %d mA \nbias_low_alarm : %d mA \n"
                "bias_high_warn : %d mA \nbias_low_warn : %d mA \n\n",
                trx_calibrate_temp(cmis_ddm_limits.temp_high_alarm),
                trx_calibrate_temp(cmis_ddm_limits.temp_low_alarm),
                trx_calibrate_temp(cmis_ddm_limits.temp_high_warn),
                trx_calibrate_temp(cmis_ddm_limits.temp_low_warn),
                trx_calibrate_vcc(cmis_ddm_limits.volt_high_alarm),
                trx_calibrate_vcc(cmis_ddm_limits.volt_low_alarm),
                trx_calibrate_vcc(cmis_ddm_limits.volt_high_warn),
                trx_calibrate_vcc(cmis_ddm_limits.volt_low_warn),
                trx_calibrate_power(cmis_ddm_limits.txpwr_high_alarm),
                trx_calibrate_power(cmis_ddm_limits.txpwr_low_alarm),
                trx_calibrate_power(cmis_ddm_limits.txpwr_high_warn),
                trx_calibrate_power(cmis_ddm_limits.txpwr_low_warn),
                trx_calibrate_power(cmis_ddm_limits.rxpwr_high_alarm),
                trx_calibrate_power(cmis_ddm_limits.rxpwr_low_alarm),
                trx_calibrate_power(cmis_ddm_limits.rxpwr_high_warn),
                trx_calibrate_power(cmis_ddm_limits.rxpwr_low_warn),
                trx_calibrate_txbias(cmis_ddm_limits.bias_high_alarm),
                trx_calibrate_txbias(cmis_ddm_limits.bias_low_alarm),
                trx_calibrate_txbias(cmis_ddm_limits.bias_high_warn),
                trx_calibrate_txbias(cmis_ddm_limits.bias_low_warn));
    default:
        return sysfs_spec_info_print(buf,*spec_id);
    }

}
struct qsfp* get_qsfp_kobj(struct kobject *kobj)
{
    struct device *dev = kobj_to_dev(kobj->parent);
    struct qsfp *qsfp = NULL;

    if(!dev) {
        TRX_LOG_ERR_NODEV("Device structure is NULL\n");
        return NULL;
    }

    qsfp= dev_get_drvdata(dev);
    if(!qsfp) {
        TRX_LOG_ERR_NODEV("QSFP is NULL\n");
        return NULL;
    }

    return qsfp;
}
static ssize_t temp_show(struct kobject *kobj, struct kobj_attribute *attr,
                                                   char *buf)
{
    struct sff8472_temp_diag temp_ext_cal = {0};
    struct cmis_eeprom_id *cmis_id;
    struct qsfp *qsfp = NULL;
    struct sfp_eeprom_id *id;
    int16_t tempc = 0;
    u8 *spec_id;
    int ret = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    spec_id = (u8*)&qsfp->id;

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
                TRX_LOG_ERR(qsfp, "QSFP read error: %d\n", ret);;
                return -EINVAL;
            }

            /* Check for Internal calibration for DDM supported SFP,
             * Address A0h, Byte 92 Bit 5.
             */
            if(id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
            {
                return sysfs_emit(buf, "%ld\n",
                          trx_calibrate_temp(tempc));
            }
            else /* SFP supported External calibration */
            {
                ret = qsfp_read(qsfp, SFF8472_TEMP_EXT, &temp_ext_cal,
                                      sizeof(temp_ext_cal));
                if (ret < 0) {
                    TRX_LOG_ERR(qsfp, "QSFP read error for temperature "
                            "external calibration constants: %d\n\n", ret);
                    return -EINVAL;
                }
                return sysfs_emit(buf, "%ld\n", trx_ext_temp_ddm(tempc,
                                               &temp_ext_cal));
            }
        }
        else
        {
            TRX_LOG_ERR(qsfp, "TRX temperature measurement not "
                                "supported on non-DDM transceiver devices.\n");
            return -EINVAL;
        }
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        if (!qsfp->support.temp_flags) {
            TRX_LOG_ERR(qsfp, "TRX temperature measurement not "
                               "supported on non-DDM transceiver devices.\n");
            return -EINVAL;
        }
        /* Page 00h Bytes 22-23 */
        ret = qsfp_read(qsfp, SFF8636_TEMPERATURE, &tempc,
                                           sizeof(tempc));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp,"QSFP read error: %d\n", ret);
            return -EINVAL;
        }

        return sysfs_emit(buf,"%ld\n", trx_calibrate_temp(tempc));
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            TRX_LOG_ERR(qsfp,"TRX temperature measurement"
                                            " not supported\n");
            return -EINVAL;
        }

        /*Support advertised in page 01h:159.0 */
        if (!cmis_id->ext.temp_mon_sup) {
            TRX_LOG_ERR(qsfp, "TRX temperature measurement"
                              " not supported\n");
            return -EINVAL;
        }

        /* Page 00h Bytes 14-15 */
        ret = qsfp_read(qsfp, CMIS_MOD_TEMPMON, &tempc,
                              sizeof(tempc));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "QSFP read error: %d\n", ret);
            return -EINVAL;
        }

        return sysfs_emit(buf, "%ld\n", trx_calibrate_temp(tempc));
    default:
        TRX_LOG_ERR(qsfp, "Invalid Specification Identifier "
                          "{0x%02X}\n", *spec_id);
        return -EINVAL;
    }
}

static ssize_t volt_show(struct kobject *kobj, struct kobj_attribute *attr,
                                                   char *buf)
{
    struct qsfp *qsfp = NULL;
    struct cmis_eeprom_id *cmis_id;
    struct sfp_eeprom_id *id;
    u8 *spec_id;
    u16 supply_voltage_t = 0;
    struct sff8472_vcc_diag vcc_ext_cal = {0};
    int ret = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    spec_id = (u8*)&qsfp->id;

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
                TRX_LOG_ERR(qsfp, "QSFP read error: %d\n", ret);
                return -EINVAL;
            }

            /* Check for Internal calibration for DDM supported SFP,
             * Address A0h, Byte 92 Bit 5.
             */
            if(id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
            {
                return sysfs_emit(buf, "%ld\n",
                       trx_calibrate_vcc(supply_voltage_t));
            }
            else /* SFP supported External calibration */
            {
                ret = qsfp_read(qsfp, SFF8472_VCC_EXT, &vcc_ext_cal,
                        sizeof(vcc_ext_cal));
                if (ret < 0) {
                    TRX_LOG_ERR(qsfp,"QSFP read error: %d", ret);
                    return -EINVAL;
                }
                return sysfs_emit(buf, "%ld\n",
                                 trx_ext_vcc_ddm(supply_voltage_t,
                                 &vcc_ext_cal));
            }
        }
        else
        {
            TRX_LOG_ERR(qsfp,"TRX supply voltage measurement "
                          "not supported on non-DDM transceiver devices.\n");
            return -EINVAL;
        }
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        if (!qsfp->support.volt_flags) {
            TRX_LOG_ERR(qsfp,"TRX supply voltage measurement "
                           "not supported on non-DDM transceiver devices.\n");
            return -EINVAL;
        }
        /* Page 00h Bytes 26-27 */
        ret = qsfp_read(qsfp, SFF8636_SUPPLY_VOLTAGE, &supply_voltage_t,
                              sizeof(supply_voltage_t));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "QSFP read error: %d\n", ret);
            return -EINVAL;
        }

        return sysfs_emit(buf, "%ld\n",
                              trx_calibrate_vcc(supply_voltage_t));
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            TRX_LOG_ERR(qsfp,"TRX supply voltage measurement"
                                            " not supported\n");
            return -EINVAL;
        }

        /*Support advertised in page 01h:159.1 */
        if (!cmis_id->ext.volt_mon_sup) {
            TRX_LOG_ERR(qsfp, "TRX supply voltage measurement"
                                            " not supported\n");
            return -EINVAL;
        }

        /* Page 00h Bytes 16-17 */
        ret = qsfp_read(qsfp, CMIS_MOD_VCCMON, &supply_voltage_t,
                              sizeof(supply_voltage_t));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "QSFP read error: %d\n", ret);
            return -EINVAL;
        }

        return sysfs_emit(buf,"%ld\n",
                              trx_calibrate_vcc(supply_voltage_t));
    default:
        TRX_LOG_ERR(qsfp, "Invalid Specification Identifier"
                                   " {0x%02X}\n", *spec_id);
        return -EINVAL;
    }
}

static ssize_t vendor_name_show(struct kobject *kobj,
                                      struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    struct sfp_eeprom_id *sff8472_id;
    struct sff8636_eeprom_id *sff8636_id;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    spec_id = (u8*)&qsfp->id;

    switch (*spec_id) {
     case SFF8024_ID_SFP:
     case SFF8024_ID_SFF_8472:
         sff8472_id = &qsfp->id.sff8472;
         return sysfs_emit(buf, "%.*s\n",
                                (int)sizeof(sff8472_id->base.vendor_name),
                                sff8472_id->base.vendor_name);
     case SFF8024_ID_QSFP28_8636:
     case SFF8024_ID_QSFP_8436_8636:
         sff8636_id = &qsfp->id.sff8636;
         return sysfs_emit(buf, "%.*s\n",
                                (int)sizeof(sff8636_id->base.vendor_name),
                                sff8636_id->base.vendor_name);
     case SFF8024_ID_QSFPDD_CMIS:
         cmis_id = &qsfp->id.cmis;
         return sysfs_emit(buf, "%.*s\n",
                                (int)sizeof(cmis_id->base.vendor_name),
                                cmis_id->base.vendor_name);

     default:
         TRX_LOG_ERR(qsfp, "Invalid Specification Identifier"
                           " {0x%02X}\n", *spec_id);
         return -EINVAL;
     }
}

static ssize_t vendor_pn_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    struct sfp_eeprom_id *sff8472_id;
    struct sff8636_eeprom_id *sff8636_id;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    spec_id = (u8*)&qsfp->id;

    switch (*spec_id) {
     case SFF8024_ID_SFP:
     case SFF8024_ID_SFF_8472:
         sff8472_id = &qsfp->id.sff8472;
         return sysfs_emit(buf, "%.*s\n",
                                (int)sizeof(sff8472_id->base.vendor_pn),
                                sff8472_id->base.vendor_pn);
     case SFF8024_ID_QSFP28_8636:
     case SFF8024_ID_QSFP_8436_8636:
         sff8636_id = &qsfp->id.sff8636;
         return sysfs_emit(buf, "%.*s\n",
                                (int)sizeof(sff8636_id->base.vendor_pn),
                                sff8636_id->base.vendor_pn);
     case SFF8024_ID_QSFPDD_CMIS:
         cmis_id = &qsfp->id.cmis;
         return sysfs_emit(buf, "%.*s\n", (int)sizeof(cmis_id->base.vendor_pn),
                                cmis_id->base.vendor_pn);

     default:
         TRX_LOG_ERR(qsfp, "Invalid Specification Identifier"
                           " {0x%02X}\n", *spec_id);
         return -EINVAL;
     }
}

static ssize_t vendor_sn_show(struct kobject *kobj,
                                   struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    struct sfp_eeprom_id *sff8472_id;
    struct sff8636_eeprom_id *sff8636_id;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    spec_id = (u8*)&qsfp->id;

    switch (*spec_id) {
     case SFF8024_ID_SFP:
     case SFF8024_ID_SFF_8472:
         sff8472_id = &qsfp->id.sff8472;

         return sysfs_emit(buf, "%.*s\n",
         (int)sizeof(sff8472_id->ext.vendor_sn), sff8472_id->ext.vendor_sn);

     case SFF8024_ID_QSFP28_8636:
     case SFF8024_ID_QSFP_8436_8636:
         sff8636_id = &qsfp->id.sff8636;

         return sysfs_emit(buf, "%.*s\n",
         (int)sizeof(sff8636_id->ext.vendor_sn), sff8636_id->ext.vendor_sn);
 
     case SFF8024_ID_QSFPDD_CMIS:
         cmis_id = &qsfp->id.cmis;

         return sysfs_emit(buf, "%.*s\n",
         (int)sizeof(cmis_id->base.vendor_sn), cmis_id->base.vendor_sn);

     default:
         TRX_LOG_ERR(qsfp, "Invalid Specification Identifier"
                           " {0x%02X}\n", *spec_id);
         return -EINVAL;
     }
}

static ssize_t wavelength_show(struct kobject *kobj,
                                     struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    struct sfp_eeprom_id *sff8472_id;
    struct sff8636_eeprom_id *sff8636_id;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    spec_id = (u8*)&qsfp->id;

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;
        if ((!sff8472_id->base.sfp_ct_passive) &&
            (!sff8472_id->base.sfp_ct_active))
        {
            return sysfs_emit(buf, "%ld\n",
                   be16_to_cpu(sff8472_id->base.optical_wavelength));
        }
        else
        {
            TRX_LOG_ERR(qsfp, "Unknown Wavelength\n");
            return -EINVAL;
        }
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        sff8636_id = &qsfp->id.sff8636;
        if ((sff8636_id->base.device_tech & 0xF0) >=
            SFF8636_TRANS_COPPER_UNEQUAL)
        {
            /* Wavelength not defined for copper cable */
            TRX_LOG_ERR(qsfp, "Unknown Wavelength\n");
            return -EINVAL;
        }
        else
        {
            /* Laser wavelength in nm */
            return sysfs_emit(buf, "%ld\n",
                be16_to_cpu(sff8636_id->base.wavelength)/20);
        }
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;
        if(cmis_id->base.media_interface_tech >= 0x0A)
        {
            /* Wavelength not defined for copper cable */
            TRX_LOG_ERR(qsfp, "Unknown Wavelength\n");
            return -EINVAL;
        }
        else if (!qsfp->module_flat_mem)
        {
            /* Laser wavelength in nm */
            return sysfs_emit(buf, "%ld\n",
                   be16_to_cpu(cmis_id->ext.nominal_wavelength)/20);
        }
        else
        {
            TRX_LOG_ERR(qsfp, "Unknown Wavelength\n");
            return -EINVAL;
        }
    default:
        TRX_LOG_ERR(qsfp, "Invalid Specification Identifier"
                          " {0x%02X}\n", *spec_id);
        return -EINVAL;
    }
}

static ssize_t mod_info_show(struct kobject *kobj,
                                  struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    u8 *spec_id;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    spec_id = (u8*)&qsfp->id;
    return sysfs_emit(buf, "%s\n", mod_identifier_to_str(*spec_id));
}

static ssize_t laser_temp_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id;
    struct sfp_eeprom_id *id;
    struct sff8636_eeprom_ext *ext;
    __be16 l_tempc = 0;
    int ret = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    spec_id = (u8*)&qsfp->id;

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        id = &qsfp->id.sff8472;
        /* Check for DDM support, Address A0h, Byte 92 Bit 6 and
         * Internalcalibration */
        if((id->ext.diagmon & SFF8472_DIAGMON_DDM) &&
           (id->ext.diagmon & SFF8472_DIAGMON_INT_CAL))
        {
            /* Adress A2h, Bytes 106-107 */
            ret = qsfp_read(qsfp, SFF8472_LASER_TEMP_WL, &l_tempc,
                                    sizeof(l_tempc));
            if (ret < 0) {
                TRX_LOG_ERR(qsfp, "QSFP read error: %d\n", ret);;
                return -EINVAL;
            }

            if(l_tempc == 0x00) {
                TRX_LOG_ERR(qsfp, "Laser temperature unknown\n");
                return -EINVAL;
            }

            return sysfs_emit(buf, "%ld\n", trx_calibrate_temp(l_tempc));
        }
        else
        {
            TRX_LOG_ERR(qsfp, "Laser temperature not supported\n");
            return -EINVAL;
        }
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        ext = &qsfp->id.sff8636.ext;
        /* Check page 20, 21 support from page 00h byte 195 bit 0 */
        if(!ext->page20_21)
        {
            TRX_LOG_ERR(qsfp, "Laser temperature not supported\n");
             return -EINVAL;
        }
        else
        {
            if((qsfp->param_info.laser_temp_sup_flag == 0x1) &&
                (qsfp->param_info.ltemp_reg_addr != 0x00))
            {
                ret = qsfp_read(qsfp, qsfp->param_info.ltemp_reg_addr,
                                           &l_tempc, sizeof(l_tempc));
                if (ret < 0) {
                    TRX_LOG_ERR(qsfp, "Page 20H laser temp read failed,"
                                      " ret %d", ret);
                    return -EINVAL;
                }
                return sysfs_emit(buf, "%ld\n", trx_calibrate_temp(l_tempc));
            }
            return -EINVAL;
        }
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;

        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
            memory modules*/
            TRX_LOG_ERR(qsfp,"TRX Laser temperature measurement"
                                            " not supported\n");
            return -EINVAL;
        }

        /* AUX2 denotes Laser temperature */
        if(!(cmis_id->ext.aux_mon_obs & BIT(1)) &&
            cmis_id->ext.aux2_mon_sup)
        {
            ret = qsfp_read(qsfp, CMIS_MOD_AUX2_MON, &l_tempc,
                                    sizeof(l_tempc));
        }
        else if(!(cmis_id->ext.aux_mon_obs & BIT(2)) &&
                 cmis_id->ext.aux3_mon_sup)
        {
          /* AUX3 denotes Laser temperature */
            ret = qsfp_read(qsfp, CMIS_MOD_AUX3_MON, &l_tempc,
                                    sizeof(l_tempc));
        }
        else
        {
            TRX_LOG_ERR(qsfp, "Laser temperature unknown\n");
            return -EINVAL;
        }

        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "QSFP read error: %d\n", ret);;
            return -EINVAL;
        }

        if(l_tempc == 0x00) {
            TRX_LOG_ERR(qsfp, "Laser temperature unknown\n");
            return -EINVAL;
        }
        return sysfs_emit(buf, "%ld\n", trx_calibrate_temp(l_tempc));
    default:
        TRX_LOG_ERR(qsfp, "Invalid Specification Identifier {0x%02X}\n", *spec_id);
        return -EINVAL;
    }
}

int common_temp_threshold(struct qsfp *qsfp, int attr, long* value)
{
    struct sff8472_ddm_thresholds *sff8472_ddm_limits;
    struct sff8636_ddm_thresholds *sff8636_ddm_limits;
    struct cmis_thresholds *cmis_ddm_limits;
    struct sfp_eeprom_id *sff8472_id;
    u8 *spec_id;
    struct sff8472_temp_diag temp_ext_cal = {0};
    int ret = 0;
    __be16 temp_threshold = 0;

    spec_id = (u8*)&qsfp->id;

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;
        if(!(sff8472_id->ext.enhopts & SFP_ENHOPTS_ALARMWARN))
        {
           TRX_LOG_ERR(qsfp, "TRX does not support alarm and "
                             "warning threshold limits\n");
           return -EINVAL;
        }

        sff8472_ddm_limits = &qsfp->diag.sff8472_ddm_limits;

        if(attr == TEMP_HALRM)
            temp_threshold = sff8472_ddm_limits->temp_high_alarm;
        else if(attr == TEMP_LALRM)
            temp_threshold = sff8472_ddm_limits->temp_low_alarm;
        else if(attr == TEMP_HWARN)
            temp_threshold = sff8472_ddm_limits->temp_high_warn;
        else if(attr == TEMP_LWARN)
            temp_threshold = sff8472_ddm_limits->temp_low_warn;
        else
            return -EINVAL;

       if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_EXT_CAL)
       {
           ret = qsfp_read(qsfp, SFF8472_TEMP_EXT, &temp_ext_cal,
                                 sizeof(temp_ext_cal));
           if (ret < 0) {
                TRX_LOG_ERR(qsfp, "QSFP read error for temperature "
                             "external calibration constants: %d\n\n", ret);
                return -EINVAL;
           }
           *value = trx_ext_temp_ddm(temp_threshold, &temp_ext_cal);
       }
       else /* SFP supported Internal calibration */
       {
          *value = trx_calibrate_temp(temp_threshold);
       }
       return 0;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h, Byte-2 Bit-2 */
        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            TRX_LOG_ERR(qsfp, "TRX does not support alarm and "
                               "warning threshold limits\n");
            return -EINVAL;
        }

        sff8636_ddm_limits = &qsfp->diag.sff8636_ddm_limits;

        if(attr == TEMP_HALRM)
            temp_threshold = sff8636_ddm_limits->temp_high_alarm;
        else if(attr == TEMP_LALRM)
            temp_threshold = sff8636_ddm_limits->temp_low_alarm;
        else if(attr == TEMP_HWARN)
            temp_threshold = sff8636_ddm_limits->temp_high_warn;
        else if(attr == TEMP_LWARN)
            temp_threshold = sff8636_ddm_limits->temp_low_warn;
        else
            return -EINVAL;

        *value = trx_calibrate_temp(temp_threshold);
        return 0;
    case SFF8024_ID_QSFPDD_CMIS:
        /* Page 00h, Byte-2 Bit-2 */
        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            TRX_LOG_ERR(qsfp, "TRX does not support alarm and "
                               "warning threshold limits\n");
            return -EINVAL;
        }

        cmis_ddm_limits = &qsfp->diag.cmis_ddm_limits;

        if(attr == TEMP_HALRM)
            temp_threshold = cmis_ddm_limits->temp_high_alarm;
        else if(attr == TEMP_LALRM)
            temp_threshold = cmis_ddm_limits->temp_low_alarm;
        else if(attr == TEMP_HWARN)
            temp_threshold = cmis_ddm_limits->temp_high_warn;
        else if(attr == TEMP_LWARN)
            temp_threshold = cmis_ddm_limits->temp_low_warn;
        else
            return -EINVAL;

        *value = trx_calibrate_temp(temp_threshold);
        return 0;
    }

    return 0;
}

static ssize_t temp_halrm_show(struct kobject *kobj,
                                     struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long temp_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_temp_threshold(qsfp,TEMP_HALRM,&temp_threshold);
    if(ret < 0)
         return -EINVAL;

    return sysfs_emit(buf, "%ld\n",temp_threshold);
}

static ssize_t temp_lalrm_show(struct kobject *kobj,
                                     struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long temp_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_temp_threshold(qsfp,TEMP_LALRM,&temp_threshold);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",temp_threshold);
}

static ssize_t temp_hwarn_show(struct kobject *kobj,
                                     struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long temp_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_temp_threshold(qsfp, TEMP_HWARN,&temp_threshold);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",temp_threshold);

}

static ssize_t temp_lwarn_show(struct kobject *kobj,
                                     struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long temp_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_temp_threshold(qsfp, TEMP_LWARN,&temp_threshold);
    if(ret < 0)
       return -EINVAL;

    return sysfs_emit(buf, "%ld\n",temp_threshold);
}


int common_volt_threshold(struct qsfp *qsfp, int attr, long* value)
{
    struct sff8472_ddm_thresholds *sff8472_ddm_limits;
    struct sff8636_ddm_thresholds *sff8636_ddm_limits;
    struct cmis_thresholds *cmis_ddm_limits;
    struct sfp_eeprom_id *sff8472_id;
    u8 *spec_id;
    struct sff8472_vcc_diag vcc_ext_cal = {0};
    int ret = 0;
    __be16 volt_threshold = 0;

    spec_id = (u8*)&qsfp->id;

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;
        if(!(sff8472_id->ext.enhopts & SFP_ENHOPTS_ALARMWARN))
        {
            TRX_LOG_ERR(qsfp, "TRX does not support alarm and "
                             "warning threshold limits\n");
            return -EINVAL;
        }

        sff8472_ddm_limits = &qsfp->diag.sff8472_ddm_limits;

        if(attr == VOLT_HALRM)
            volt_threshold = sff8472_ddm_limits->volt_high_alarm;
        else if(attr == VOLT_LALRM)
            volt_threshold = sff8472_ddm_limits->volt_low_alarm;
        else if(attr == VOLT_HWARN)
            volt_threshold = sff8472_ddm_limits->volt_high_warn;
        else if(attr == VOLT_LWARN)
            volt_threshold = sff8472_ddm_limits->volt_low_warn;
        else
            return -EINVAL;

       if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_EXT_CAL)
       {
           ret = qsfp_read(qsfp, SFF8472_VCC_EXT, &vcc_ext_cal,
                                 sizeof(vcc_ext_cal));
           if (ret < 0) {
                TRX_LOG_ERR(qsfp, "QSFP read error for voltage "
                             "external calibration constants: %d\n\n", ret);
                return -EINVAL;
           }
           *value = trx_ext_vcc_ddm(volt_threshold, &vcc_ext_cal);
       }
       else /* SFP supported Internal calibration */
       {
          *value = trx_calibrate_vcc(volt_threshold);
       }
       return 0;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h, Byte-2 Bit-2 */
        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            TRX_LOG_ERR(qsfp, "TRX does not support alarm and "
                               "warning threshold limits\n");
            return -EINVAL;
        }

        sff8636_ddm_limits = &qsfp->diag.sff8636_ddm_limits;

        if(attr == VOLT_HALRM)
            volt_threshold = sff8636_ddm_limits->volt_high_alarm;
        else if(attr == VOLT_LALRM)
            volt_threshold = sff8636_ddm_limits->volt_low_alarm;
        else if(attr == VOLT_HWARN)
            volt_threshold = sff8636_ddm_limits->volt_high_warn;
        else if(attr == VOLT_LWARN)
            volt_threshold = sff8636_ddm_limits->volt_low_warn;
        else
            return -EINVAL;

        *value = trx_calibrate_vcc(volt_threshold);
        return 0;
    case SFF8024_ID_QSFPDD_CMIS:
        /* Page 00h, Byte-2 Bit-2 */
        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            TRX_LOG_ERR(qsfp, "TRX does not support alarm and "
                               "warning threshold limits\n");
            return -EINVAL;
        }

        cmis_ddm_limits = &qsfp->diag.cmis_ddm_limits;

        if(attr == VOLT_HALRM)
            volt_threshold = cmis_ddm_limits->volt_high_alarm;
        else if(attr == VOLT_LALRM)
            volt_threshold = cmis_ddm_limits->volt_low_alarm;
        else if(attr == VOLT_HWARN)
            volt_threshold = cmis_ddm_limits->volt_high_warn;
        else if(attr == VOLT_LWARN)
            volt_threshold = cmis_ddm_limits->volt_low_warn;
        else
            return -EINVAL;

        *value = trx_calibrate_vcc(volt_threshold);
        return 0;
    }

    return 0;
}

static ssize_t volt_halrm_show(struct kobject *kobj,
                                     struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long volt_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_volt_threshold(qsfp, VOLT_HALRM, &volt_threshold);
    if(ret < 0)
       return -EINVAL;

    return sysfs_emit(buf, "%ld\n",volt_threshold);
}

static ssize_t volt_lalrm_show(struct kobject *kobj,
                                     struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long volt_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_volt_threshold(qsfp, VOLT_LALRM, &volt_threshold);
    if(ret < 0)
       return -EINVAL;

    return sysfs_emit(buf, "%ld\n",volt_threshold);
}

static ssize_t volt_hwarn_show(struct kobject *kobj,
                                     struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long volt_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_volt_threshold(qsfp, VOLT_HWARN, &volt_threshold);
    if(ret < 0)
       return -EINVAL;

    return sysfs_emit(buf, "%ld\n",volt_threshold);
}

static ssize_t volt_lwarn_show(struct kobject *kobj,
                                     struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long volt_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_volt_threshold(qsfp, VOLT_LWARN, &volt_threshold);
    if(ret < 0)
       return -EINVAL;

    return sysfs_emit(buf, "%ld\n",volt_threshold);

}

int common_txi_threshold(struct qsfp *qsfp, int attr, long* value)
{
    struct sff8472_ddm_thresholds *sff8472_ddm_limits;
    struct sff8636_ddm_thresholds *sff8636_ddm_limits;
    struct cmis_thresholds *cmis_ddm_limits;
    struct sfp_eeprom_id *sff8472_id;
    u8 *spec_id;
    struct sff8472_txi_diag txi_ext_cal = {0};
    int ret = 0;
    __be16 txi_threshold = 0;
    struct cmis_eeprom_id *cmis_id;
    u8 cmis_tx_bias_multiplier = 1;

    spec_id = (u8*)&qsfp->id;

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;
        if(!(sff8472_id->ext.enhopts & SFP_ENHOPTS_ALARMWARN))
        {
           TRX_LOG_ERR(qsfp, "TRX does not support alarm and "
                             "warning threshold limits\n");
           return -EINVAL;
        }

        sff8472_ddm_limits = &qsfp->diag.sff8472_ddm_limits;

        if(attr == TXI_HALRM)
            txi_threshold = sff8472_ddm_limits->bias_high_alarm;
        else if(attr == TXI_LALRM)
            txi_threshold = sff8472_ddm_limits->bias_low_alarm;
        else if(attr == TXI_HWARN)
            txi_threshold = sff8472_ddm_limits->bias_high_warn;
        else if(attr == TXI_LWARN)
            txi_threshold = sff8472_ddm_limits->bias_low_warn;
        else
            return -EINVAL;

       if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_EXT_CAL)
       {
           ret = qsfp_read(qsfp, SFF8472_TXI_EXT, &txi_ext_cal,
                                 sizeof(txi_ext_cal));
           if (ret < 0) {
                TRX_LOG_ERR(qsfp, "QSFP read error for tx bias current "
                            "external calibration constants: %d\n\n", ret);
                return -EINVAL;
           }
           *value = trx_ext_ddm_txbias(txi_threshold, &txi_ext_cal);
       }
       else /* SFP supported Internal calibration */
       {
          *value = trx_calibrate_txbias(txi_threshold);
       }
       return 0;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h, Byte-2 Bit-2 */
        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            TRX_LOG_ERR(qsfp, "TRX does not support alarm and "
                               "warning threshold limits\n");
            return -EINVAL;
        }

        sff8636_ddm_limits = &qsfp->diag.sff8636_ddm_limits;

        if(attr == TXI_HALRM)
            txi_threshold = sff8636_ddm_limits->bias_high_alarm;
        else if(attr == TXI_LALRM)
            txi_threshold = sff8636_ddm_limits->bias_low_alarm;
        else if(attr == TXI_HWARN)
            txi_threshold = sff8636_ddm_limits->bias_high_warn;
        else if(attr == TXI_LWARN)
            txi_threshold = sff8636_ddm_limits->bias_low_warn;
        else
            return -EINVAL;

        *value = trx_calibrate_txbias(txi_threshold);
        return 0;
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;
        /* Page 00h, Byte-2 Bit-2 */
        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            TRX_LOG_ERR(qsfp, "TRX does not support alarm and "
                               "warning threshold limits\n");
            return -EINVAL;
        }

        /*Support advertised in page 01h:160.0 */
        if(!cmis_id->ext.tx_bias_mon_sup ) {
            TRX_LOG_ERR(qsfp, "TRX tx bias current measurement "
                               "not supported\n");
            return -EINVAL;
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
        /* reserved case is not expected from OIF-CMIS-05.2 Rev */
        case 0x03:
            TRX_LOG_ERR(qsfp, "TRX tx bias current multiplier "
                       "was reserved not expected for OIF-CMIS-05.2\n");
            return -EINVAL;
        }

        cmis_ddm_limits = &qsfp->diag.cmis_ddm_limits;

        if(attr == TXI_HALRM)
            txi_threshold = cmis_ddm_limits->bias_high_alarm;
        else if(attr == TXI_LALRM)
            txi_threshold = cmis_ddm_limits->bias_low_alarm;
        else if(attr == TXI_HWARN)
            txi_threshold = cmis_ddm_limits->bias_high_warn;
        else if(attr == TXI_LWARN)
            txi_threshold = cmis_ddm_limits->bias_low_warn;
        else
            return -EINVAL;

        txi_threshold *= cmis_tx_bias_multiplier;

        *value = trx_calibrate_txbias(txi_threshold);
        return 0;
    }

    return 0;
}

static ssize_t tx_bias_halrm_show(struct kobject *kobj,
                                  struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long txi_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_txi_threshold(qsfp, TXI_HALRM,  &txi_threshold);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",txi_threshold);
}

static ssize_t tx_bias_lalrm_show(struct kobject *kobj,
                                     struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long txi_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_txi_threshold(qsfp, TXI_LALRM,  &txi_threshold);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",txi_threshold);
}

static ssize_t tx_bias_hwarn_show(struct kobject *kobj,
                                     struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long txi_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_txi_threshold(qsfp, TXI_HWARN, &txi_threshold);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",txi_threshold);
}

static ssize_t tx_bias_lwarn_show(struct kobject *kobj,
                                     struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long txi_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_txi_threshold(qsfp, TXI_LWARN, &txi_threshold);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",txi_threshold);
}

int common_power_threshold(struct qsfp *qsfp, int attr, long* value)
{
    struct sff8472_ddm_thresholds *sff8472_ddm_limits;
    struct sff8636_ddm_thresholds *sff8636_ddm_limits;
    struct cmis_thresholds *cmis_ddm_limits;
    struct sfp_eeprom_id *sff8472_id;
    u8 *spec_id;
    struct sff8472_txpwr_diag txpwr_ext_cal = {0};

    int ret = 0;
    __be16 power_threshold = 0;

    spec_id = (u8*)&qsfp->id;

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;
        if(!(sff8472_id->ext.enhopts & SFP_ENHOPTS_ALARMWARN))
        {
            TRX_LOG_ERR(qsfp, "TRX does not support alarm and "
                              "warning threshold limits\n");
            return -EINVAL;
        }

        sff8472_ddm_limits = &qsfp->diag.sff8472_ddm_limits;

        if(attr == TXPWR_HALRM)
            power_threshold = sff8472_ddm_limits->txpwr_high_alarm;
        else if(attr == TXPWR_LALRM)
            power_threshold = sff8472_ddm_limits->txpwr_low_alarm;
        else if(attr == TXPWR_HWARN)
            power_threshold = sff8472_ddm_limits->txpwr_high_warn;
        else if(attr == TXPWR_LWARN)
            power_threshold = sff8472_ddm_limits->txpwr_low_warn;
        else if(attr == RXPWR_HALRM)
            power_threshold = sff8472_ddm_limits->rxpwr_high_alarm;
        else if(attr == RXPWR_LALRM)
            power_threshold = sff8472_ddm_limits->rxpwr_low_alarm;
        else if(attr == RXPWR_HWARN)
            power_threshold = sff8472_ddm_limits->rxpwr_high_warn;
        else if(attr == RXPWR_LWARN)
            power_threshold = sff8472_ddm_limits->rxpwr_low_warn;
        else
            return -EINVAL;

       if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_EXT_CAL)
       {
           ret = qsfp_read(qsfp, SFF8472_TXPWR_EXT, &txpwr_ext_cal,
                                 sizeof(txpwr_ext_cal));
           if (ret < 0) {
                TRX_LOG_ERR(qsfp, "QSFP read error for txpwr "
                          "external calibration constants: %d\n\n", ret);
                return -EINVAL;
           }
           *value =  trx_ext_ddm_power(power_threshold, &txpwr_ext_cal);
       }
       else /* SFP supported Internal calibration */
       {
          *value = trx_calibrate_power(power_threshold);
       }
       return 0;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h, Byte-2 Bit-2 */
        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            TRX_LOG_ERR(qsfp, "TRX does not support alarm and "
                               "warning threshold limits\n");
            return -EINVAL;
        }

        sff8636_ddm_limits = &qsfp->diag.sff8636_ddm_limits;

        if(attr == TXPWR_HALRM)
            power_threshold = sff8636_ddm_limits->txpwr_high_alarm;
        else if(attr == TXPWR_LALRM)
            power_threshold = sff8636_ddm_limits->txpwr_low_alarm;
        else if(attr == TXPWR_HWARN)
            power_threshold = sff8636_ddm_limits->txpwr_high_warn;
        else if(attr == TXPWR_LWARN)
            power_threshold = sff8636_ddm_limits->txpwr_low_warn;
        else if(attr == RXPWR_HALRM)
            power_threshold = sff8636_ddm_limits->rxpwr_high_alarm;
        else if(attr == RXPWR_LALRM)
            power_threshold = sff8636_ddm_limits->rxpwr_low_alarm;
        else if(attr == RXPWR_HWARN)
            power_threshold = sff8636_ddm_limits->rxpwr_high_warn;
        else if(attr == RXPWR_LWARN)
            power_threshold = sff8636_ddm_limits->rxpwr_low_warn;
        else
            return -EINVAL;

        *value = trx_calibrate_power(power_threshold);
        return 0;
    case SFF8024_ID_QSFPDD_CMIS:
        /* Page 00h, Byte-2 Bit-2 */
        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            TRX_LOG_ERR(qsfp, "TRX does not support alarm and "
                               "warning threshold limits\n");
            return -EINVAL;
        }

        cmis_ddm_limits = &qsfp->diag.cmis_ddm_limits;
        if(attr == TXPWR_HALRM)
            power_threshold = cmis_ddm_limits->txpwr_high_alarm;
        else if(attr == TXPWR_LALRM)
            power_threshold = cmis_ddm_limits->txpwr_low_alarm;
        else if(attr == TXPWR_HWARN)
            power_threshold = cmis_ddm_limits->txpwr_high_warn;
        else if(attr == TXPWR_LWARN)
            power_threshold = cmis_ddm_limits->txpwr_low_warn;
        else if(attr == RXPWR_HALRM)
            power_threshold = cmis_ddm_limits->rxpwr_high_alarm;
        else if(attr == RXPWR_LALRM)
            power_threshold = cmis_ddm_limits->rxpwr_low_alarm;
        else if(attr == RXPWR_HWARN)
            power_threshold = cmis_ddm_limits->rxpwr_high_warn;
        else if(attr == RXPWR_LWARN)
            power_threshold = cmis_ddm_limits->rxpwr_low_warn;
        else
            return -EINVAL;

        *value = trx_calibrate_power(power_threshold);
        return 0;
    }

    return 0;
}

static ssize_t tx_power_halrm_show(struct kobject *kobj,
                                   struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long power_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power_threshold(qsfp, TXPWR_HALRM, &power_threshold);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",power_threshold);
}

static ssize_t tx_power_lalrm_show(struct kobject *kobj,
                                  struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long power_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power_threshold(qsfp, TXPWR_LALRM, &power_threshold);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",power_threshold);

}

static ssize_t tx_power_hwarn_show(struct kobject *kobj,
                                  struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long power_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power_threshold(qsfp, TXPWR_HWARN, &power_threshold);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",power_threshold);
}

static ssize_t tx_power_lwarn_show(struct kobject *kobj,
                                   struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long power_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power_threshold(qsfp, TXPWR_LWARN, &power_threshold);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",power_threshold);
}

static ssize_t rx_power_halrm_show(struct kobject *kobj,
                                  struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long power_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power_threshold(qsfp, RXPWR_HALRM, &power_threshold);
    if(ret < 0)
       return -EINVAL;

    return sysfs_emit(buf, "%ld\n",power_threshold);
}

static ssize_t rx_power_lalrm_show(struct kobject *kobj,
                                     struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long power_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power_threshold(qsfp, RXPWR_LALRM, &power_threshold);
    if(ret < 0)
       return -EINVAL;

    return sysfs_emit(buf, "%ld\n",power_threshold);

}

static ssize_t rx_power_hwarn_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long power_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power_threshold(qsfp, RXPWR_HWARN, &power_threshold);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",power_threshold);

}

static ssize_t rx_power_lwarn_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long power_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power_threshold(qsfp, RXPWR_LWARN, &power_threshold);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",power_threshold);
}

int common_laser_temp_threshold(struct qsfp *qsfp, int attr, long* value)
{
    struct sff8472_ddm_thresholds *sff8472_ddm_limits;
    struct cmis_thresholds *cmis_ddm_limits;
    struct sfp_eeprom_id *sff8472_id;
    struct cmis_eeprom_id *cmis_id;
    struct sff8636_eeprom_ext *ext;
    long laser_temp_threshold;
    u8 *spec_id;

    spec_id = (u8*)&qsfp->id;

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;
        sff8472_ddm_limits = &qsfp->diag.sff8472_ddm_limits;

        if(!(sff8472_id->ext.enhopts & SFP_ENHOPTS_ALARMWARN))
        {
            TRX_LOG_ERR(qsfp,"TRX Laser temperature Alrm/Warn"
                              " not supported\n");
            return -EINVAL;
        }

        if(attr == LTEMP_HALRM)
            laser_temp_threshold = sff8472_ddm_limits->laser_temp_high_alarm;
        else if(attr == LTEMP_LALRM)
            laser_temp_threshold = sff8472_ddm_limits->laser_temp_low_alarm;
        else if(attr == LTEMP_HWARN)
            laser_temp_threshold = sff8472_ddm_limits->laser_temp_high_warn;
        else if(attr == LTEMP_LWARN)
            laser_temp_threshold = sff8472_ddm_limits->laser_temp_low_warn;
        else
            return -EINVAL;

        if(laser_temp_threshold == 0x00) {
            TRX_LOG_ERR(qsfp, "Laser temperature threshold unknown\n");
            return -EINVAL;
        }
        *value = trx_calibrate_temp(laser_temp_threshold);
        return 0;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        ext = &qsfp->id.sff8636.ext;
        /* Check page 20, 21 support from page 00h byte 195 bit 0 */
        if(!ext->page20_21)
        {
            TRX_LOG_ERR(qsfp, "Laser temperature thresholds not supported\n");
            return -EINVAL;
        }
        else
        {
            if(qsfp->param_info.laser_temp_thsup_flag == 0x1)
            {
                if(attr == LTEMP_HALRM)
                    laser_temp_threshold = qsfp->param_info.param_th.param_high_alarm;
                else if(attr == LTEMP_LALRM)
                    laser_temp_threshold = qsfp->param_info.param_th.param_low_alarm;
                else if(attr == LTEMP_HWARN)
                    laser_temp_threshold = qsfp->param_info.param_th.param_high_warn;
                else if(attr == LTEMP_LWARN)
                    laser_temp_threshold = qsfp->param_info.param_th.param_low_warn;
                else
                    return -EINVAL;

                *value = trx_calibrate_temp(laser_temp_threshold);
                return 0;
            }
            else
            {
                TRX_LOG_ERR(qsfp, "Laser temperature thresholds not supported\n");
                return -EINVAL;
            }
        }
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;
        /* Page 00h, Byte-2 Bit-7 */
        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            TRX_LOG_ERR(qsfp,"TRX Laser temperature Alrm/Warn"
                             " not supported\n");
            return -EINVAL;
        }

        cmis_ddm_limits = &qsfp->diag.cmis_ddm_limits;
        /* AUX2 denotes Laser temperature */
        if(cmis_id->ext.aux2_mon_sup  &&
           !(cmis_id->ext.aux_mon_obs & BIT(1)))
        {
            if(attr == LTEMP_HALRM)
                laser_temp_threshold = cmis_ddm_limits->aux2_high_alarm;
            else if(attr == LTEMP_LALRM)
                laser_temp_threshold = cmis_ddm_limits->aux2_low_alarm;
            else if(attr == LTEMP_HWARN)
                laser_temp_threshold = cmis_ddm_limits->aux2_high_warn;
            else if(attr == LTEMP_LWARN)
                laser_temp_threshold = cmis_ddm_limits->aux2_low_warn;
            else
                return -EINVAL;
        }
        else if(cmis_id->ext.aux3_mon_sup &&
                !(cmis_id->ext.aux_mon_obs & BIT(2)))
        {
            /* AUX3 denotes Laser temperature */
            if(attr == LTEMP_HALRM)
                laser_temp_threshold = cmis_ddm_limits->aux3_high_alarm;
            else if(attr == LTEMP_LALRM)
                laser_temp_threshold = cmis_ddm_limits->aux3_low_alarm;
            else if(attr == LTEMP_HWARN)
                laser_temp_threshold = cmis_ddm_limits->aux3_high_warn;
            else if(attr == LTEMP_LWARN)
                laser_temp_threshold = cmis_ddm_limits->aux3_low_warn;
            else
                return -EINVAL;
        }
        else
        {
            TRX_LOG_ERR(qsfp,"TRX Laser temperature Alrm/Warn"
                             " not supported\n");
            return -EINVAL;
        }

        if(laser_temp_threshold == 0x00) {
            TRX_LOG_ERR(qsfp, "Laser temperature threshold unknown\n");
            return -EINVAL;
        }

        *value = trx_calibrate_temp(laser_temp_threshold);
        return 0;
    default:
        TRX_LOG_ERR(qsfp, "Invalid Specification Identifier"
                          " {0x%02X}\n", *spec_id);
        return -EINVAL;
    }
}

static ssize_t ltemp_halrm_show(struct kobject *kobj,
                                   struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long ltemp_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_laser_temp_threshold(qsfp, LTEMP_HALRM, &ltemp_threshold);
    if(ret < 0)
         return -EINVAL;

    return sysfs_emit(buf, "%ld\n",ltemp_threshold);
}

static ssize_t ltemp_lalrm_show(struct kobject *kobj,
                                   struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long ltemp_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_laser_temp_threshold(qsfp, LTEMP_LALRM, &ltemp_threshold);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",ltemp_threshold);
}

static ssize_t ltemp_hwarn_show(struct kobject *kobj,
                                   struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long ltemp_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_laser_temp_threshold(qsfp, LTEMP_HWARN, &ltemp_threshold);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",ltemp_threshold);
}

static ssize_t ltemp_lwarn_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long ltemp_threshold = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_laser_temp_threshold(qsfp, LTEMP_LWARN, &ltemp_threshold);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",ltemp_threshold);
}

int get_sfp_power(struct qsfp *qsfp, int lane_number, long* value_in, int attr)
{
   struct sfp_eeprom_id *sff8472_id;
   struct sff8472_txpwr_diag txpwr_ext_cal = {0};
   int ret = 0;
   __be16 sfp_tx_power = 0;
   sff8472_id = &qsfp->id.sff8472;

   if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_DDM)
   {
       if(attr == TX_POWER)
           ret = qsfp_read(qsfp, SFF8472_TX_POWER, &sfp_tx_power,
                                 sizeof(sfp_tx_power));
        else if(attr == RX_POWER)
         ret = qsfp_read(qsfp, SFF8472_RX_POWER, &sfp_tx_power,
                                      sizeof(sfp_tx_power));
        else
             return -EINVAL;

       if (ret < 0) {
           TRX_LOG_ERR(qsfp,"QSFP read error: %d\n", ret);
           return -EINVAL;
       }

       /* Check for Internal calibration for DDM supported SFP,
        * Address A0h, Byte 92 Bit 5.
        */
       if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
       {
          *value_in = trx_calibrate_power(sfp_tx_power);
          return 0;
       }
       else /* SFP supported External calibration */
       {
           ret = qsfp_read(qsfp, SFF8472_TXPWR_EXT, &txpwr_ext_cal,
                                sizeof(txpwr_ext_cal));
           if (ret < 0) {
               TRX_LOG_ERR(qsfp,"QSFP read error for tx power external"
                                " calibration constants: %d\n\n", ret);
               return -EINVAL;
           }
          *value_in = trx_ext_ddm_power(sfp_tx_power,&txpwr_ext_cal);
          return 0;
       }
    }
    else
    {
        TRX_LOG_ERR(qsfp, "Transmitter power measurement"
                    " not supported on non-DDM transceiver devices.\n");
        return -EINVAL;
    }
}

int get_qsfp_reg(int lane_number, u32* addr, int attr)
{
    if(attr == TX_POWER || attr == RX_POWER)
    {
        if(lane_number == 1) {
            *addr  = (attr == RX_POWER) ? SFF8636_RX_POWER :
                                          SFF8636_TX_POWER;
        }
        else if (lane_number == 2) {
            *addr  = (attr == RX_POWER) ? SFF8636_RX_POWER_LANE2 :
                                          SFF8636_TX_POWER_LANE2;
        }
        else if (lane_number == 3) {
            *addr  = (attr == RX_POWER) ? SFF8636_RX_POWER_LANE3 :
                                          SFF8636_TX_POWER_LANE3;
        }
        else if (lane_number == 4) {
            *addr  =  (attr == RX_POWER) ? SFF8636_RX_POWER_LANE4 :
                                           SFF8636_TX_POWER_LANE4;
        }
        else
            return -1;
    }
    else if(attr == TX_BIAS)
    {
        if(lane_number == 1)
            *addr  = SFF8636_TX_BIAS;
        else if (lane_number == 2)
            *addr  = SFF8636_TX_BIAS_LANE2;
        else if (lane_number == 3)
            *addr  = SFF8636_TX_BIAS_LANE3;
        else if (lane_number == 4)
            *addr  = SFF8636_TX_BIAS_LANE4;
        else
            return -1;
    }
    else
        return -1;

    return 0;
}

int get_cmis_reg(int lane_number, u32* addr, int attr)
{
    if(attr == TX_POWER || attr == RX_POWER)
    {
        if(lane_number == 1) {
            *addr  = (attr == RX_POWER) ? CMIS_RX_POWER :
                                          CMIS_TX_POWER;
        }
        else if (lane_number == 2) {
            *addr  = (attr == RX_POWER) ? CMIS_RX_POWER_LANE2 :
                                          CMIS_TX_POWER_LANE2;
        }
        else if (lane_number == 3) {
            *addr  = (attr == RX_POWER) ? CMIS_RX_POWER_LANE3 :
                                          CMIS_TX_POWER_LANE3;
        }
        else if (lane_number == 4) {
            *addr  = (attr == RX_POWER) ? CMIS_RX_POWER_LANE4 :
                                          CMIS_TX_POWER_LANE4;
        }
        else if (lane_number == 5) {
            *addr  = (attr == RX_POWER) ? CMIS_RX_POWER_LANE5 :
                                          CMIS_TX_POWER_LANE5;
        }
        else if (lane_number == 6) {
            *addr  = (attr == RX_POWER) ? CMIS_RX_POWER_LANE6 :
                                          CMIS_TX_POWER_LANE6;
        }
        else if (lane_number == 7) {
            *addr  = (attr == RX_POWER) ? CMIS_RX_POWER_LANE7 :
                                          CMIS_TX_POWER_LANE7;
        }
        else if (lane_number == 8) {
            *addr  = (attr == RX_POWER) ? CMIS_RX_POWER_LANE8 :
                                          CMIS_TX_POWER_LANE8;
        }
        else
            return -1;
    }
    else if(attr == TX_BIAS)
    {
        if(lane_number == 1)
            *addr  = CMIS_TX_BIAS;
        else if (lane_number == 2)
            *addr  = CMIS_TX_BIAS_LANE2;
        else if (lane_number == 3)
            *addr  = CMIS_TX_BIAS_LANE3;
        else if (lane_number == 4)
            *addr  = CMIS_TX_BIAS_LANE4;
        else if (lane_number == 5)
            *addr  = CMIS_TX_BIAS_LANE5;
        else if (lane_number == 6)
            *addr  = CMIS_TX_BIAS_LANE6;
        else if (lane_number == 7)
            *addr  = CMIS_TX_BIAS_LANE7;
        else if (lane_number == 8)
            *addr  = CMIS_TX_BIAS_LANE8;
        else
            return -1;
    }
    else
        return -1;

    return 0;
}

int get_qsfp_power(struct qsfp *qsfp, int lane_number,
                        long* value_in, int attr)
{
    int ret = 0;
    __be16 qsfp_tx_power = 0;
    struct sff8636_eeprom_id *id;
    u8  diagmon;
    u32 addr;
    id = &qsfp->id.sff8636;
    diagmon = id->ext.diagmon;

    if(attr == TX_POWER) {
        if(!(diagmon & BIT(2))) {
            TRX_LOG_ERR(qsfp,  "Transmitter tx power measurement"
                           " not supported\n");
            return -EINVAL;
        }
    }
    else if(attr == RX_POWER) {
        if (!qsfp->support.rx_power_flags) {
            TRX_LOG_ERR(qsfp, "TRX optical rx power measurement"
                           " not supported on non-DDM transceiver devices.\n");
            return -EINVAL;
        }
    }
    else
        return -EINVAL;

    if(attr == TX_POWER)
        ret = get_qsfp_reg(lane_number, &addr, TX_POWER);
    else
        ret = get_qsfp_reg(lane_number, &addr, RX_POWER);

    if (ret < 0)
        return -EINVAL;

    ret = qsfp_read(qsfp, addr, &qsfp_tx_power,
                          sizeof(qsfp_tx_power));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "QSFP read error: %d\n", ret);
        return -EINVAL;
    }

    *value_in = trx_calibrate_power(qsfp_tx_power);

    return 0;
}

int get_cmis_power(struct qsfp *qsfp, int lane_number,
                        long* value_in, int attr)
{
    struct cmis_eeprom_id *cmis_id;
    u32 addr = 0;
    __be16 cmis_tx_power = 0;
    int ret = 0;
    cmis_id = &qsfp->id.cmis;

    if (qsfp->module_flat_mem == 0x01) {
        /* Module level monitor values supports only for paged
           memory modules*/
        TRX_LOG_ERR(qsfp, "Transmitter power measurement"
                          " not supported\n");
        return -EINVAL;
    }

    if(attr == TX_POWER) {
        /*Support advertised in page 01h:160.1 */
        if(!cmis_id->ext.tx_optical_pow_mon_sup) {
            TRX_LOG_ERR(qsfp, "Transmitter power measurement "
                           "not supported\n");
            return -EINVAL;
        }
    }
    else if(attr == RX_POWER) {
        if (!qsfp->support.rx_power_flags) {
            TRX_LOG_ERR(qsfp, "TRX optical rx power measurement"
                           " not supported on non-DDM transceiver devices.\n");
            return -EINVAL;
        }
    }
    else
        return -EINVAL;

    if(attr == TX_POWER)
        ret = get_cmis_reg(lane_number, &addr, TX_POWER);
    else
        ret = get_cmis_reg(lane_number, &addr, RX_POWER);

    if (ret < 0)
        return -EINVAL;

    ret = qsfp_read(qsfp, addr, &cmis_tx_power,
                          sizeof(cmis_tx_power));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp,, "QSFP read error: %d\n", ret);
        return -EINVAL;
    }

    *value_in = trx_calibrate_power(cmis_tx_power);
    return 0;
}

int common_power(struct qsfp *qsfp, int lane_number,
                      long* value_in, int attr)
{
    int ret = 0;
    long value = 0;
    u8 *spec_id;
    spec_id = (u8*)&qsfp->id;

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        ret = get_sfp_power(qsfp, lane_number, &value, attr);
        if(ret == 0)
            *value_in = value;
        return ret;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        ret = get_qsfp_power(qsfp, lane_number, &value, attr);
        if(ret == 0)
            *value_in = value;
        return ret;
    case SFF8024_ID_QSFPDD_CMIS:
        ret = get_cmis_power(qsfp, lane_number, &value, attr);
        if(ret == 0)
            *value_in = value;
        return ret;
    default:
        TRX_LOG_ERR(qsfp, "Invalid Specification Identifier"
                          " {0x%02X}\n", *spec_id);
        return -EINVAL;
    }
}

static ssize_t tx_power_lane1_show(struct kobject *kobj,
                                  struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long tx_power = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power(qsfp,1,&tx_power,TX_POWER);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",tx_power);
}

static ssize_t tx_power_lane2_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long tx_power = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power(qsfp,2,&tx_power, TX_POWER);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",tx_power);
}

static ssize_t tx_power_lane3_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long tx_power = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power(qsfp,3,&tx_power, TX_POWER);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",tx_power);

}

static ssize_t tx_power_lane4_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long tx_power = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power(qsfp,4,&tx_power, TX_POWER);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",tx_power);
}

static ssize_t tx_power_lane5_show(struct kobject *kobj,
                                     struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long tx_power = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power(qsfp,5,&tx_power, TX_POWER);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",tx_power);

}

static ssize_t tx_power_lane6_show(struct kobject *kobj,
                                      struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long tx_power = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power(qsfp,6,&tx_power, TX_POWER);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",tx_power);

}

static ssize_t tx_power_lane7_show(struct kobject *kobj,
                                       struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long tx_power = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power(qsfp,7,&tx_power, TX_POWER);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",tx_power);

}

static ssize_t tx_power_lane8_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long tx_power = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power(qsfp,8,&tx_power, TX_POWER);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",tx_power);
}

static ssize_t rx_power_lane1_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long rx_power = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power(qsfp,1,&rx_power, RX_POWER);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",rx_power);
}

static ssize_t rx_power_lane2_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long rx_power = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power(qsfp,2,&rx_power, RX_POWER);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",rx_power);
}

static ssize_t rx_power_lane3_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long rx_power = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power(qsfp,3,&rx_power, RX_POWER);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",rx_power);
}

static ssize_t rx_power_lane4_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long rx_power = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power(qsfp,4,&rx_power, RX_POWER);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",rx_power);
}

static ssize_t rx_power_lane5_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long rx_power = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power(qsfp,5,&rx_power, RX_POWER);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",rx_power);
}

static ssize_t rx_power_lane6_show(struct kobject *kobj,
                                  struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long rx_power = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power(qsfp,6,&rx_power, RX_POWER);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",rx_power);
}

static ssize_t rx_power_lane7_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long rx_power = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power(qsfp,7,&rx_power, RX_POWER);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",rx_power);
}

static ssize_t rx_power_lane8_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long rx_power = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_power(qsfp,8,&rx_power, RX_POWER);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",rx_power);
}

int get_sfp_tx_bias(struct qsfp* qsfp, int lane_number,long* value)
{
    struct sfp_eeprom_id *sff8472_id;
    u16 tx_bias_current = 0;
    long bias_value = 0;
    struct sff8472_txi_diag txi_ext_cal = {0};
    int ret = 0;

    sff8472_id = &qsfp->id.sff8472;

    /* Check for DDM support, Address A0h, Byte 92 Bit 6 */
    if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_DDM)
    {
        /* Address A2h, Bytes 100-101 */
        ret = qsfp_read(qsfp, SFF8472_TX_BIAS, &tx_bias_current,
                                      sizeof(tx_bias_current));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "QSFP read error: %d\n", ret);
            return -EINVAL;
        }

        /* Check for Internal calibration for DDM supported SFP,
         * Address A0h, Byte 92 Bit 5.
         */
        if(sff8472_id->ext.diagmon & SFF8472_DIAGMON_INT_CAL)
        {
            bias_value =  trx_calibrate_txbias(tx_bias_current);
            *value = bias_value;
            return 0;
        }
        else /* SFP supported External calibration */
        {
            ret = qsfp_read(qsfp, SFF8472_TXI_EXT, &txi_ext_cal,
                                  sizeof(txi_ext_cal));
            if (ret < 0) {
                TRX_LOG_ERR(qsfp,"QSFP read error for tx bias current "
                           "of external calibration constants: %d\n\n", ret);
                return -EINVAL;
            }

            bias_value =   trx_ext_ddm_txbias(tx_bias_current, &txi_ext_cal);
            *value = bias_value;
            return 0;
        }
    }
    else
    {
        TRX_LOG_ERR(qsfp, "TRX tx bias current measurement"
                         " not supported on non-DDM transceiver devices.\n");
        return -EINVAL;
    }

    return 0;
}

int get_qsfp_tx_bias(struct qsfp* qsfp, int lane_number,long* value)
{
    int ret = 0;
    u32 addr;
    u16 tx_bias = 0;

    if (!qsfp->support.tx_bias_flags) {
        TRX_LOG_ERR(qsfp,"TRX tx bias current measurement"
                     " not supported on non-DDM transceiver devices.\n");
        return -EINVAL;
    }

    ret = get_qsfp_reg(lane_number, &addr, TX_BIAS);
    if (ret < 0)
        return -EINVAL;

     ret = qsfp_read(qsfp, addr, &tx_bias,
                                 sizeof(tx_bias));
     if (ret < 0) {
        TRX_LOG_ERR(qsfp, "QSFP read error: %d\n", ret);
        return -EINVAL;
     }

    *value = trx_calibrate_txbias(tx_bias);
    return 0;
}

int get_cmis_tx_bias(struct qsfp* qsfp, int lane_number,long* value)
{
    u8 cmis_tx_bias_multiplier = 1;
    int ret = 0;
    struct cmis_eeprom_id *cmis_id;
    u16 tx_bias_current = 0;
    u32 addr;

    cmis_id = &qsfp->id.cmis;

    if (qsfp->module_flat_mem == 0x01) {
        /* Module level monitor values supports only for paged
           memory modules*/
        TRX_LOG_ERR(qsfp,  "TRX tx bias current measurement "
                           "not supported\n");
        return -EINVAL;
    }

    /*Support advertised in page 01h:160.0 */
    if(!cmis_id->ext.tx_bias_mon_sup ) {
        TRX_LOG_ERR(qsfp, "TRX tx bias current measurement "
                          "not supported\n");
        return -EINVAL;
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
    /* reserved case is not expected from OIF-CMIS-05.2 Rev */
    case 0x03:
        TRX_LOG_ERR(qsfp, "TRX tx bias current multiplier "
                          "was reserved not expected for OIF-CMIS-05.2\n");
        return -EINVAL;
    }

     ret = get_cmis_reg(lane_number, &addr, TX_BIAS);
     if (ret < 0)
             return -EINVAL;

    /* Page 11h Bytes 170-185 */
    ret = qsfp_read(qsfp, addr, &tx_bias_current,
                          sizeof(tx_bias_current));
    if (ret < 0) {
        TRX_LOG_ERR(qsfp, "QSFP read error: %d\n", ret);
        return -EINVAL;
    }

    tx_bias_current *= cmis_tx_bias_multiplier;

     *value = trx_calibrate_txbias(tx_bias_current);
     return 0;
}

int common_tx_bias(struct qsfp *qsfp, int lane_number, long* tx_bias)
{
    int ret = 0;
    long value = 0;
    u8 *spec_id;
    spec_id = (u8*)&qsfp->id;

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        ret = get_sfp_tx_bias(qsfp, lane_number, &value);
        if(ret == 0)
            *tx_bias = value;
        return ret;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        ret = get_qsfp_tx_bias(qsfp, lane_number, &value);
        if(ret == 0)
            *tx_bias = value;
        return ret;
    case SFF8024_ID_QSFPDD_CMIS:
        ret = get_cmis_tx_bias(qsfp, lane_number, &value);
        if(ret == 0)
            *tx_bias = value;
        return ret;
    default:
        TRX_LOG_ERR(qsfp, "Invalid Specification Identifier"
                          " {0x%02X}\n", *spec_id);
        return -EINVAL;
    }
}

static ssize_t tx_bias_lane1_show(struct kobject *kobj,
                                    struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long tx_bias = 0;

     qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_tx_bias(qsfp,1,&tx_bias);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",tx_bias);
}

static ssize_t tx_bias_lane2_show(struct kobject *kobj,
                                 struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long tx_bias = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_tx_bias(qsfp,2,&tx_bias);
    if(ret < 0)
       return -EINVAL;

    return sysfs_emit(buf, "%ld\n",tx_bias);
}

static ssize_t tx_bias_lane3_show(struct kobject *kobj,
                                 struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long tx_bias = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_tx_bias(qsfp,3,&tx_bias);
    if(ret < 0)
       return -EINVAL;

    return sysfs_emit(buf, "%ld\n",tx_bias);
}

static ssize_t tx_bias_lane4_show(struct kobject *kobj,
                                 struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long tx_bias = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_tx_bias(qsfp,4,&tx_bias);
    if(ret < 0)
       return -EINVAL;

    return sysfs_emit(buf, "%ld\n",tx_bias);
}

static ssize_t tx_bias_lane5_show(struct kobject *kobj,
                                 struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long tx_bias = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_tx_bias(qsfp,5,&tx_bias);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",tx_bias);
}

static ssize_t tx_bias_lane6_show(struct kobject *kobj,
                                 struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long tx_bias = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_tx_bias(qsfp,6,&tx_bias);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",tx_bias);
}

static ssize_t tx_bias_lane7_show(struct kobject *kobj,
                                 struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long tx_bias = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_tx_bias(qsfp,7,&tx_bias);
    if(ret < 0)
       return -EINVAL;

    return sysfs_emit(buf, "%ld\n",tx_bias);
}

static ssize_t tx_bias_lane8_show(struct kobject *kobj,
                                 struct kobj_attribute *attr, char *buf)
{
    struct qsfp *qsfp = NULL;
    int ret = 0;
    long tx_bias = 0;

    qsfp = get_qsfp_kobj(kobj);
    if(!qsfp)
        return -EINVAL;

    ret = common_tx_bias(qsfp,8,&tx_bias);
    if(ret < 0)
        return -EINVAL;

    return sysfs_emit(buf, "%ld\n",tx_bias);
}

static ssize_t tcvr_compliance_code_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    u8 *spec_id;
    /* Variable to hold compliance code type o-ran-spec enum index */
    u8 compliance_code;

    struct qsfp *qsfp = dev_get_drvdata(dev);
    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing. */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return -EINVAL;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        if(qsfp->module_revision == SFF8472_REV_9_3) {
            compliance_code = 1;
        } else if (qsfp->module_revision == SFF8472_REV_9_5){
            compliance_code = 2;
        } else if (qsfp->module_revision == SFF8472_REV_10_2){
            compliance_code = 3;
        }  else if (qsfp->module_revision == SFF8472_REV_10_4){
            compliance_code = 8;
        } else if (qsfp->module_revision == SFF8472_REV_11_0){
            compliance_code = 4;
        } else if (qsfp->module_revision == SFF8472_REV_11_3){
            compliance_code = 5;
        } else if (qsfp->module_revision == SFF8472_REV_11_4){
            compliance_code = 6;
        } else if (qsfp->module_revision == SFF8472_REV_12_3){
            compliance_code = 9;
        } else if (qsfp->module_revision == SFF8472_REV_12_4){
            compliance_code = 10;
        } else if(qsfp->module_revision == 0x00){
            compliance_code = 0;
        } else {
            return -EINVAL;
        }
        break;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
    case SFF8024_ID_QSFPDD_CMIS:
    default:
         return -EINVAL;
    }

    return sysfs_emit(buf, "%u\n",compliance_code);
}

static ssize_t trx_connector_type_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    u8 *spec_id;

    u8 connector_type = 0;
    u8 oranConnTypeIdx = 0;

    struct qsfp *qsfp = dev_get_drvdata(dev);
    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing. */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return -EINVAL;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        connector_type = qsfp->id.sff8472.base.connector;
        break;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        connector_type = qsfp->id.sff8636.base.connector;
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        connector_type = qsfp->id.cmis.base.connector;
        break;
    default:
         return -EINVAL;
    }

    switch(connector_type) {
    case SFF8024_CONNECTOR_UNSPEC:
    default:
        /* [0] unknown */
        oranConnTypeIdx = 0;
        break;
    case SFF8024_CONNECTOR_SC:
        /* [1] subscrber-connector */
        oranConnTypeIdx = 1;
        break;
    case SFF8024_CONNECTOR_FIBERJACK:
        /* [2] fiber-jack */
        oranConnTypeIdx = 2;
        break;
    case SFF8024_CONNECTOR_LC:
        /* [3] lucent-connector */
        oranConnTypeIdx = 3;
        break;
    case SFF8024_CONNECTOR_MT_RJ:
        /* [4] mt-rj */
        oranConnTypeIdx = 4;
        break;
    case SFF8024_CONNECTOR_MU:
        /* [5] multiple-optical */
        oranConnTypeIdx = 5;
        break;
    case SFF8024_CONNECTOR_SG:
        /* [6] sg */
        oranConnTypeIdx = 6;
        break;
    case SFF8024_CONNECTOR_OPTICAL_PIGTAIL:
        /* [7] optical-pigtail */
        oranConnTypeIdx = 7;
        break;
    case SFF8024_CONNECTOR_MPO_1X12:
        /*[8] multi-fiber-parralel-optic-1x12 */
        oranConnTypeIdx = 8;
        break;
    case SFF8024_CONNECTOR_MPO_2X16:
        /* [9] multi-fiber-parralel-optic-2x16 */
        oranConnTypeIdx = 9;
        break;
    case SFF8024_CONNECTOR_HSSDC_II:
        /* [10] hssdc_2 */
        oranConnTypeIdx = 10;
        break;
    case SFF8024_CONNECTOR_COPPER_PIGTAIL:
        /* [11] copper-pigtail */
        oranConnTypeIdx = 11;
        break;
    case SFF8024_CONNECTOR_RJ45:
        /* [12] rj45 */
        oranConnTypeIdx = 12;
        break;
    case SFF8024_CONNECTOR_NOSEPARATE:
        /* [13] no-separable-connector */
        oranConnTypeIdx = 13;
        break;
    case SFF8024_CONNECTOR_MXC_2X16:
        /* [14] mxc-2x16 */
        oranConnTypeIdx = 14;
        break;
    }
    return sysfs_emit(buf, "%u\n",oranConnTypeIdx);
}

static ssize_t trx_bitrate_margin_low_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    u8 *spec_id;
    unsigned int br_min = 0, br_nominal = 0;

    struct qsfp *qsfp = dev_get_drvdata(dev);
    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing. */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return -EINVAL;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        br_nominal = qsfp->id.sff8472.base.br_nominal;
        br_min = qsfp->id.sff8472.ext.br_min;
        break;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
    case SFF8024_ID_QSFPDD_CMIS:
    default:
         return -EINVAL;
    }

    if(br_nominal == 0) {
        br_min = 0;
    }

    return sysfs_emit(buf, "%u\n",br_min);
}

static ssize_t trx_bitrate_margin_high_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    u8 *spec_id;
    unsigned int br_max = 0,br_nominal = 0, br_min = 0;

    struct qsfp *qsfp = dev_get_drvdata(dev);
    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing. */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return -EINVAL;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        br_nominal = qsfp->id.sff8472.base.br_nominal;
        br_max = qsfp->id.sff8472.ext.br_max;
        br_min = qsfp->id.sff8472.ext.br_min;
        break;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
    case SFF8024_ID_QSFPDD_CMIS:
    default:
         return -EINVAL;
    }

    if(br_nominal == 0) {
        br_max = 0;
    } else if(br_nominal == 255) {
        br_max = br_min;
    }

    return sysfs_emit(buf, "%u\n",br_max);
}

static ssize_t trx_bitrate_nominal_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    u8 *spec_id;
    u8 br_nominal = 0, br_max = 0;
    unsigned int br_nom = 0;

    struct qsfp *qsfp = dev_get_drvdata(dev);
    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing. */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return -EINVAL;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        br_nominal = qsfp->id.sff8472.base.br_nominal;
        br_max = qsfp->id.sff8472.ext.br_max;
        break;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        br_nominal = qsfp->id.sff8636.base.br_nominal;
        br_max = qsfp->id.sff8636.ext.baud_rate_nominal;
        break;
    case SFF8024_ID_QSFPDD_CMIS:
    default:
         return -EINVAL;
    }

    if(br_nominal == 0) {
        br_nom = 0;
    } else if(br_nominal == 255) {
        br_nom = br_max * 250;
    } else {
        br_nom = br_nominal * 100;
    }

    return sysfs_emit(buf, "%u\n",br_nom);
}

static ssize_t trx_rx_power_type_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{
    struct sfp_eeprom_id *sfp_id;
    struct sff8636_eeprom_id *qsfp_id;
    struct cmis_eeprom_id *cmis_id;
    u8 *spec_id;
    /*
     * Variable to hold receiver power measurement o-ran-spec enum index,
     *   [0] oma, optical modulation amplitude.
     *   [1] avp, average power.
     */
    u8 rx_power_type = 0;

    struct qsfp *qsfp = dev_get_drvdata(dev);
    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing. */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return -EINVAL;
    }

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sfp_id = &qsfp->id.sff8472;
        if (sfp_id->ext.diagmon & BIT(3)) {
            // [1] avp, average power.
            rx_power_type = 1;
        } else {
            //  [0] oma, optical modulation amplitude.
            rx_power_type = 0;
        }
        break;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        qsfp_id =  &qsfp->id.sff8636;
        if (qsfp_id->ext.rx_mon_impl) {
            // [1] avp, average power.
            rx_power_type = 1;
        } else {
            //  [0] oma, optical modulation amplitude.
            rx_power_type = 0;
        }
        break;
    case SFF8024_ID_QSFPDD_CMIS:
        cmis_id = &qsfp->id.cmis;
        if (cmis_id->ext.rx_pow_type) {
            // [1] avp, average power.
            rx_power_type = 1;
        } else {
            //  [0] oma, optical modulation amplitude.
            rx_power_type = 0;
        }

        break;
    default:
         return -EINVAL;
    }

    return sysfs_emit(buf, "%u\n",rx_power_type);
}

static ssize_t mod_identifier_oran_eidx_show(struct device *dev,
                    struct device_attribute *attr, char *buf)
{

    u8 *spec_id;
    u8 ornModuleTyp = 0;

    struct qsfp *qsfp = dev_get_drvdata(dev);
    if (!qsfp) {
        TRX_LOG_ERR_NODEV("qsfp is NULL");
        return -EINVAL;
    }

    spec_id = (u8*)&qsfp->id;

    /* Ensure that the transceiver is inserted before processing. */
    if (qsfp->sm_mod_state == QSFP_MOD_EMPTY) {
        return -EINVAL;
    }

    if(*spec_id == SFF8024_ID_SFF_8472) {
        ornModuleTyp = 2;
    }
    else if(*spec_id == SFF8024_ID_SFP) {
        ornModuleTyp = 3;
    }
    else if(*spec_id == SFF8024_ID_QSFP_8436_8636) {
        ornModuleTyp = 13;
    }
    else if(*spec_id == 0x00){
        ornModuleTyp = 0;
    }
    else {
        return -EINVAL;
    }

    return sysfs_emit(buf, "%u\n", ornModuleTyp);
}

static DEVICE_ATTR(state_info, S_IRUGO, trx_state_info_show, NULL);
static DEVICE_ATTR(led_on_off, 0644, trx_led_on_off_show,
                                   trx_led_on_off_store);
static DEVICE_ATTR(led_blink_set, 0644, trx_led_blink_set_show,
                                      trx_led_blink_set_store);
static DEVICE_ATTR(led_brightness_set, 0644, trx_led_brightness_set_show,
                                           trx_led_brightness_set_store);

static DEVICE_ATTR(temperature, S_IRUGO, trx_temperature_show, NULL);
static DEVICE_ATTR(supply_voltage, S_IRUGO, trx_supply_voltage_show, NULL);
static DEVICE_ATTR(rx_power, S_IRUGO, trx_rx_power_show, NULL);
static DEVICE_ATTR(tx_bias_current, S_IRUGO, trx_tx_bias_current_show, NULL);
static DEVICE_ATTR(tx_power, S_IRUGO, trx_tx_power_show, NULL);
static DEVICE_ATTR(ddm_thresholds, S_IRUGO, trx_ddm_thresholds_show, NULL);
static DEVICE_ATTR(vendor_info, S_IRUGO, trx_vendor_info_show, NULL);
static DEVICE_ATTR(tcvr_compliance_code, S_IRUGO, tcvr_compliance_code_show, NULL);
static DEVICE_ATTR(connector_type, S_IRUGO, trx_connector_type_show, NULL);
static DEVICE_ATTR(bitrate_margin_low, S_IRUGO, trx_bitrate_margin_low_show, NULL);
static DEVICE_ATTR(bitrate_margin_high, S_IRUGO, trx_bitrate_margin_high_show, NULL);
static DEVICE_ATTR(bitrate_nominal, S_IRUGO, trx_bitrate_nominal_show, NULL);
static DEVICE_ATTR(rx_power_type, S_IRUGO, trx_rx_power_type_show, NULL);
static DEVICE_ATTR(mod_identifier_oran_eidx, S_IRUGO, mod_identifier_oran_eidx_show, NULL);


/* Transceiver port static attributes */
static struct attribute *trx_attrs[] = {
    &dev_attr_state_info.attr,
    &dev_attr_led_on_off.attr,
    &dev_attr_led_blink_set.attr,
    &dev_attr_led_brightness_set.attr,
    NULL
};

/* Transceiver module runtime attributes */
static struct attribute *trx_module_attrs[] = {
    &dev_attr_temperature.attr,
    &dev_attr_supply_voltage.attr,
    &dev_attr_rx_power.attr,
    &dev_attr_tx_bias_current.attr,
    &dev_attr_tx_power.attr,
    &dev_attr_ddm_thresholds.attr,
    &dev_attr_vendor_info.attr,
    &dev_attr_tcvr_compliance_code.attr,
    &dev_attr_connector_type.attr,
    &dev_attr_bitrate_margin_low.attr,
    &dev_attr_bitrate_margin_high.attr,
    &dev_attr_bitrate_nominal.attr,
    &dev_attr_rx_power_type.attr,
    &dev_attr_mod_identifier_oran_eidx.attr,
    NULL
};

static struct kobj_attribute temperature_attribute = __ATTR(temperature, 0664,
                                                     temp_show, NULL);
static struct kobj_attribute voltage_attribute = __ATTR(supply_voltage, 0664,
                                                 volt_show, NULL);
static struct kobj_attribute vendor_name_attribute = __ATTR(vendor_name, 0664,
                                                     vendor_name_show, NULL);
static struct kobj_attribute vendor_pn_attribute = __ATTR(vendor_pn, 0664,
                                                   vendor_pn_show, NULL);
static struct kobj_attribute vendor_sn_attribute = __ATTR(vendor_sn, 0664,
                                                   vendor_sn_show, NULL);
static struct kobj_attribute wavelength_attribute = __ATTR(wavelength, 0664,
                                                    wavelength_show, NULL);
static struct kobj_attribute mod_info_attribute = __ATTR(mod_info, 0664,
                                                  mod_info_show, NULL);
static struct kobj_attribute laser_temp_attribute = __ATTR(laser_temp, 0664,
                                                    laser_temp_show, NULL);

static struct kobj_attribute temp_halrm_attribute = __ATTR(temp_halrm, 0664,
                                                    temp_halrm_show, NULL);
static struct kobj_attribute temp_lalrm_attribute = __ATTR(temp_lalrm, 0664,
                                                    temp_lalrm_show, NULL);
static struct kobj_attribute temp_hwarn_attribute = __ATTR(temp_hwarn, 0664,
                                                    temp_hwarn_show, NULL);
static struct kobj_attribute temp_lwarn_attribute = __ATTR(temp_lwarn, 0664,
                                                    temp_lwarn_show, NULL);

static struct kobj_attribute volt_halrm_attribute = __ATTR(volt_halrm, 0664,
                                                    volt_halrm_show, NULL);
static struct kobj_attribute volt_lalrm_attribute = __ATTR(volt_lalrm, 0664,
                                                    volt_lalrm_show, NULL);
static struct kobj_attribute volt_hwarn_attribute = __ATTR(volt_hwarn, 0664,
                                                    volt_hwarn_show, NULL);
static struct kobj_attribute volt_lwarn_attribute = __ATTR(volt_lwarn, 0664,
                                                    volt_lwarn_show, NULL);

static struct kobj_attribute tx_bias_halrm_attribute = __ATTR(tx_bias_halrm,
                                            0664, tx_bias_halrm_show, NULL);
static struct kobj_attribute tx_bias_lalrm_attribute = __ATTR(tx_bias_lalrm,
                                            0664, tx_bias_lalrm_show, NULL);
static struct kobj_attribute tx_bias_hwarn_attribute = __ATTR(tx_bias_hwarn,
                                             0664,tx_bias_hwarn_show, NULL);
static struct kobj_attribute tx_bias_lwarn_attribute = __ATTR(tx_bias_lwarn,
                                            0664, tx_bias_lwarn_show, NULL);

static struct kobj_attribute tx_power_halrm_attribute = __ATTR(tx_power_halrm,
                                             0664, tx_power_halrm_show, NULL);
static struct kobj_attribute tx_power_lalrm_attribute = __ATTR(tx_power_lalrm,
                                             0664, tx_power_lalrm_show, NULL);
static struct kobj_attribute tx_power_hwarn_attribute = __ATTR(tx_power_hwarn,
                                             0664, tx_power_hwarn_show, NULL);
static struct kobj_attribute tx_power_lwarn_attribute = __ATTR(tx_power_lwarn,
                                             0664, tx_power_lwarn_show, NULL);

static struct kobj_attribute rx_power_halrm_attribute = __ATTR(rx_power_halrm,
                                             0664, rx_power_halrm_show, NULL);
static struct kobj_attribute rx_power_lalrm_attribute = __ATTR(rx_power_lalrm,
                                             0664, rx_power_lalrm_show, NULL);
static struct kobj_attribute rx_power_hwarn_attribute = __ATTR(rx_power_hwarn,
                                             0664, rx_power_hwarn_show, NULL);
static struct kobj_attribute rx_power_lwarn_attribute = __ATTR(rx_power_lwarn,
                                             0664, rx_power_lwarn_show, NULL);

static struct kobj_attribute ltemp_halrm_attribute = __ATTR(ltemp_halrm, 0664,
                                                     ltemp_halrm_show, NULL);
static struct kobj_attribute ltemp_lalrm_attribute = __ATTR(ltemp_lalrm, 0664,
                                                     ltemp_lalrm_show, NULL);
static struct kobj_attribute ltemp_hwarn_attribute = __ATTR(ltemp_hwarn, 0664,
                                                     ltemp_hwarn_show, NULL);
static struct kobj_attribute ltemp_lwarn_attribute = __ATTR(ltemp_lwarn, 0664,
                                                     ltemp_lwarn_show, NULL);

static struct kobj_attribute tx_power_lane1_attribute = __ATTR(tx_power_lane1,
                                             0664, tx_power_lane1_show, NULL);
static struct kobj_attribute tx_power_lane2_attribute = __ATTR(tx_power_lane2,
                                             0664, tx_power_lane2_show, NULL);
static struct kobj_attribute tx_power_lane3_attribute = __ATTR(tx_power_lane3,
                                             0664, tx_power_lane3_show, NULL);
static struct kobj_attribute tx_power_lane4_attribute = __ATTR(tx_power_lane4,
                                             0664, tx_power_lane4_show, NULL);
static struct kobj_attribute tx_power_lane5_attribute = __ATTR(tx_power_lane5,
                                             0664, tx_power_lane5_show, NULL);
static struct kobj_attribute tx_power_lane6_attribute = __ATTR(tx_power_lane6,
                                             0664, tx_power_lane6_show, NULL);
static struct kobj_attribute tx_power_lane7_attribute = __ATTR(tx_power_lane7,
                                             0664, tx_power_lane7_show, NULL);
static struct kobj_attribute tx_power_lane8_attribute = __ATTR(tx_power_lane8,
                                             0664, tx_power_lane8_show, NULL);

static struct kobj_attribute rx_power_lane1_attribute = __ATTR(rx_power_lane1,
                                             0664, rx_power_lane1_show, NULL);
static struct kobj_attribute rx_power_lane2_attribute = __ATTR(rx_power_lane2,
                                            0664, rx_power_lane2_show, NULL);
static struct kobj_attribute rx_power_lane3_attribute = __ATTR(rx_power_lane3,
                                             0664, rx_power_lane3_show, NULL);
static struct kobj_attribute rx_power_lane4_attribute = __ATTR(rx_power_lane4,
                                             0664, rx_power_lane4_show, NULL);
static struct kobj_attribute rx_power_lane5_attribute = __ATTR(rx_power_lane5,
                                             0664, rx_power_lane5_show, NULL);
static struct kobj_attribute rx_power_lane6_attribute = __ATTR(rx_power_lane6,
                                             0664, rx_power_lane6_show, NULL);
static struct kobj_attribute rx_power_lane7_attribute = __ATTR(rx_power_lane7,
                                             0664, rx_power_lane7_show, NULL);
static struct kobj_attribute rx_power_lane8_attribute = __ATTR(rx_power_lane8,
                                             0664, rx_power_lane8_show, NULL);

static struct kobj_attribute tx_bias_lane1_attribute = __ATTR(tx_bias_lane1,
                                            0664, tx_bias_lane1_show, NULL);
static struct kobj_attribute tx_bias_lane2_attribute = __ATTR(tx_bias_lane2,
                                            0664, tx_bias_lane2_show, NULL);
static struct kobj_attribute tx_bias_lane3_attribute = __ATTR(tx_bias_lane3,
                                            0664, tx_bias_lane3_show, NULL);
static struct kobj_attribute tx_bias_lane4_attribute = __ATTR(tx_bias_lane4,
                                            0664, tx_bias_lane4_show, NULL);
static struct kobj_attribute tx_bias_lane5_attribute = __ATTR(tx_bias_lane5,
                                            0664, tx_bias_lane5_show, NULL);
static struct kobj_attribute tx_bias_lane6_attribute = __ATTR(tx_bias_lane6,
                                            0664, tx_bias_lane6_show, NULL);
static struct kobj_attribute tx_bias_lane7_attribute = __ATTR(tx_bias_lane7,
                                            0664, tx_bias_lane7_show, NULL);
static struct kobj_attribute tx_bias_lane8_attribute = __ATTR(tx_bias_lane8,
                                            0664, tx_bias_lane8_show, NULL);

/* sensor info runtime attributes */
static struct attribute *sensor_attrs[] = {
    &temperature_attribute.attr,
    &voltage_attribute.attr,
    &vendor_name_attribute.attr,
    &vendor_pn_attribute.attr,
    &vendor_sn_attribute.attr,
    &wavelength_attribute.attr,
    &mod_info_attribute.attr,
    &laser_temp_attribute.attr,

    &temp_halrm_attribute.attr,
    &temp_lalrm_attribute.attr,
    &temp_hwarn_attribute.attr,
    &temp_lwarn_attribute.attr,

    &volt_halrm_attribute.attr,
    &volt_lalrm_attribute.attr,
    &volt_hwarn_attribute.attr,
    &volt_lwarn_attribute.attr,

    &tx_bias_halrm_attribute.attr,
    &tx_bias_lalrm_attribute.attr,
    &tx_bias_hwarn_attribute.attr,
    &tx_bias_lwarn_attribute.attr,

    &tx_power_halrm_attribute.attr,
    &tx_power_lalrm_attribute.attr,
    &tx_power_hwarn_attribute.attr,
    &tx_power_lwarn_attribute.attr,

    &rx_power_halrm_attribute.attr,
    &rx_power_lalrm_attribute.attr,
    &rx_power_hwarn_attribute.attr,
    &rx_power_lwarn_attribute.attr,

    &ltemp_halrm_attribute.attr,
    &ltemp_lalrm_attribute.attr,
    &ltemp_hwarn_attribute.attr,
    &ltemp_lwarn_attribute.attr,
    NULL
};

/* SFF-8472 sensor info runtime attributes */
static struct attribute *sff8472_sysfs_attrs[] = {
    &tx_power_lane1_attribute.attr,
    &rx_power_lane1_attribute.attr,
    &tx_bias_lane1_attribute.attr,
    NULL
};

/* SFF-8636 sensor info runtime attributes */
static struct attribute *sff8636_sysfs_attrs[] = {
    &tx_power_lane1_attribute.attr,
    &tx_power_lane2_attribute.attr,
    &tx_power_lane3_attribute.attr,
    &tx_power_lane4_attribute.attr,

    &rx_power_lane1_attribute.attr,
    &rx_power_lane2_attribute.attr,
    &rx_power_lane3_attribute.attr,
    &rx_power_lane4_attribute.attr,

    &tx_bias_lane1_attribute.attr,
    &tx_bias_lane2_attribute.attr,
    &tx_bias_lane3_attribute.attr,
    &tx_bias_lane4_attribute.attr,
    NULL
};

/* cmis sensor info runtime attributes */
static struct attribute *cmis_sysfs_attrs[] = {
    &tx_power_lane1_attribute.attr,
    &tx_power_lane2_attribute.attr,
    &tx_power_lane3_attribute.attr,
    &tx_power_lane4_attribute.attr,
    &tx_power_lane5_attribute.attr,
    &tx_power_lane6_attribute.attr,
    &tx_power_lane7_attribute.attr,
    &tx_power_lane8_attribute.attr,

    &rx_power_lane1_attribute.attr,
    &rx_power_lane2_attribute.attr,
    &rx_power_lane3_attribute.attr,
    &rx_power_lane4_attribute.attr,
    &rx_power_lane5_attribute.attr,
    &rx_power_lane6_attribute.attr,
    &rx_power_lane7_attribute.attr,
    &rx_power_lane8_attribute.attr,

    &tx_bias_lane1_attribute.attr,
    &tx_bias_lane2_attribute.attr,
    &tx_bias_lane3_attribute.attr,
    &tx_bias_lane4_attribute.attr,
    &tx_bias_lane5_attribute.attr,
    &tx_bias_lane6_attribute.attr,
    &tx_bias_lane7_attribute.attr,
    &tx_bias_lane8_attribute.attr,
    NULL
};

static const struct attribute_group trx_attr_group = {
    .attrs = trx_attrs,
};

static const struct attribute_group trx_module_attr_group = {
    .attrs = trx_module_attrs,
};

static const struct attribute_group sensor_attr_group = {
    .attrs = sensor_attrs,
};

static const struct attribute_group sff8472_attr_group = {
    .attrs = sff8472_sysfs_attrs,
};

static const struct attribute_group sff8636_attr_group = {
    .attrs = sff8636_sysfs_attrs,
};

static const struct attribute_group cmis_attr_group = {
    .attrs = cmis_sysfs_attrs,
};

/* Initialization function for creating SysFS attribute files
 * for transceiver.
 */
int qsfp_sysfs_init(struct qsfp *qsfp)
{
    int ret = 0;

    if(!qsfp->qsfp_sysfs_dir)
    {
        TRX_LOG_ERR(qsfp, "qsfp_sysfs_dir is NULL\n");
        return -EINVAL;
    }

    ret = sysfs_create_group(qsfp->qsfp_sysfs_dir, &trx_attr_group);
    if(ret != 0) {
        TRX_LOG_ERR(qsfp, "SysFS trx attr group create failure\n");
        sysfs_remove_group(qsfp->qsfp_sysfs_dir, &trx_attr_group);
        return ret;
    }
    return 0;
}

int qsfp_sensor_sysfs_init(struct qsfp *qsfp)
{
    int ret = 0;
    if(!qsfp->qsfp_sysfs_dir)
    {
        TRX_LOG_ERR(qsfp, "qsfp_sysfs_dir is NULL\n");
        return -EINVAL;
    }

    qsfp->sensor_sysfs_dir = kobject_create_and_add("sensor_info", qsfp->qsfp_sysfs_dir);
    if(!qsfp->sensor_sysfs_dir)
        return -ENOMEM;

    ret = sysfs_create_group(qsfp->sensor_sysfs_dir, &sensor_attr_group);
    if(ret != 0) {
        TRX_LOG_ERR(qsfp, "SysFS sensor attr group create failure\n");
        sysfs_remove_group(qsfp->sensor_sysfs_dir, &sensor_attr_group);
        return ret;
    }

    /* create spec specific files */
    qsfp->spec_ops->spec_sensor_sysfs_init(qsfp);

    return 0;
}

int qsfp_sensor_sysfs_exit(struct qsfp *qsfp)
{

    if(!qsfp->sensor_sysfs_dir)
    {
        TRX_LOG_ERR(qsfp, "sensor_sysfs_dir is NULL\n");
        return -EINVAL;
    }

    sysfs_remove_group(qsfp->sensor_sysfs_dir, &sensor_attr_group);

    /* delete spec specific files */
    qsfp->spec_ops->spec_sensor_sysfs_exit(qsfp);

    kobject_del(qsfp->sensor_sysfs_dir);
    qsfp->sensor_sysfs_dir = NULL;
    return 0;
}

/* Deinitilization function to delete SysFS attribute files for
 * transceiver.
 */
int qsfp_sysfs_exit(struct qsfp *qsfp)
{
    if(!qsfp->qsfp_sysfs_dir)
    {
        TRX_LOG_ERR(qsfp, "qsfp_sysfs_dir is NULL\n");
        return -EINVAL;
    }

    sysfs_remove_group(qsfp->qsfp_sysfs_dir, &trx_attr_group);
    TRX_LOG_INFO(qsfp, "SysFS trx_attr cleanup\n");
    return 0;
}

/* Initialization function for creating SysFS runtime attribute files
 * for transceiver.
 */
int module_sysfs_init(struct qsfp *qsfp)
{
    int ret = 0;

    if(!qsfp->qsfp_sysfs_dir)
    {
        TRX_LOG_ERR(qsfp, "qsfp_sysfs_dir is NULL\n");
        return -EINVAL;
    }

    ret = sysfs_create_group(qsfp->qsfp_sysfs_dir, &trx_module_attr_group);
    if(ret != 0) {
        TRX_LOG_ERR(qsfp, "SysFS trx module attr group create failure\n");
        sysfs_remove_group(qsfp->qsfp_sysfs_dir, &trx_module_attr_group);
        return ret;
    }
    return 0;
}

/* Deinitilization function to delete SysFS runtime attribute files for
 * transceiver.
 */
int module_sysfs_exit(struct qsfp *qsfp)
{
    if(!qsfp->qsfp_sysfs_dir)
    {
        TRX_LOG_ERR(qsfp, "qsfp_sysfs_dir is NULL\n");
        return -EINVAL;
    }

    sysfs_remove_group(qsfp->qsfp_sysfs_dir, &trx_module_attr_group);
    TRX_LOG_INFO(qsfp, "SysFS module_attr cleanup\n");
    return 0;
}

int sff8472_create_sysfs_files(struct qsfp *qsfp)
{
    int ret = 0;
    if(!qsfp->sensor_sysfs_dir)
    {
        TRX_LOG_ERR(qsfp, "sensor_sysfs_dir is NULL\n");
        return -EINVAL;
    }

    ret = sysfs_create_group(qsfp->sensor_sysfs_dir, &sff8472_attr_group);
    if(ret != 0) {
        TRX_LOG_ERR(qsfp, "SysFS 8472 attr group create failure\n");
        sysfs_remove_group(qsfp->sensor_sysfs_dir, &sff8472_attr_group);
        return ret;
    }

    return 0;
}

int sff8472_remove_sysfs_files(struct qsfp *qsfp)
{
    if(!qsfp->sensor_sysfs_dir)
    {
        TRX_LOG_ERR(qsfp, "sensor_sysfs_dir is NULL\n");
        return -EINVAL;
    }

    sysfs_remove_group(qsfp->sensor_sysfs_dir, &sff8472_attr_group);

    return 0;
}

int sff8636_create_sysfs_files(struct qsfp *qsfp)
{
    int ret = 0;
    if(!qsfp->sensor_sysfs_dir)
    {
        TRX_LOG_ERR(qsfp, "sensor_sysfs_dir is NULL\n");
        return -EINVAL;
    }

    ret = sysfs_create_group(qsfp->sensor_sysfs_dir, &sff8636_attr_group);
    if(ret != 0) {
        TRX_LOG_ERR(qsfp, "SysFS 8636 attr group create failure\n");
        sysfs_remove_group(qsfp->sensor_sysfs_dir, &sff8636_attr_group);
        return ret;
    }

    return 0;
}

int sff8636_remove_sysfs_files(struct qsfp *qsfp)
{
    if(!qsfp->sensor_sysfs_dir)
    {
        TRX_LOG_ERR(qsfp, "sensor_sysfs_dir is NULL\n");
        return -EINVAL;
    }

    sysfs_remove_group(qsfp->sensor_sysfs_dir, &sff8636_attr_group);

    return 0;
}

int cmis_create_sysfs_files(struct qsfp *qsfp)
{
    int ret = 0;
    if(!qsfp->sensor_sysfs_dir)
    {
        TRX_LOG_ERR(qsfp, "sensor_sysfs_dir is NULL\n");
        return -EINVAL;
    }

    ret = sysfs_create_group(qsfp->sensor_sysfs_dir, &cmis_attr_group);
    if(ret != 0) {
        TRX_LOG_ERR(qsfp, "SysFS cmis attr group create failure\n");
        sysfs_remove_group(qsfp->sensor_sysfs_dir, &cmis_attr_group);
        return ret;
    }

    return 0;
}

int cmis_remove_sysfs_files(struct qsfp *qsfp)
{
    if(!qsfp->sensor_sysfs_dir)
    {
        TRX_LOG_ERR(qsfp, "sensor_sysfs_dir is NULL\n");
        return -EINVAL;
    }

    sysfs_remove_group(qsfp->sensor_sysfs_dir, &cmis_attr_group);

    return 0;
}

int qsfp_module_parse_ddm_thresholds(struct qsfp* qsfp)
{
    int ret = 0;
    struct qsfp_diag diag_l;
    u8 *spec_id;
    struct sfp_eeprom_id *sff8472_id;
    spec_id = (u8*)&qsfp->id;

    switch (*spec_id) {
    case SFF8024_ID_SFP:
    case SFF8024_ID_SFF_8472:
        sff8472_id = &qsfp->id.sff8472;
        if(!(sff8472_id->ext.enhopts & SFP_ENHOPTS_ALARMWARN))
        {
            TRX_LOG_ERR(qsfp, "TRX does not support alarm and "
                              "warning threshold limits\n");
            return 0;
        }

        /* Address A2h, Bytes 0-39 */
        ret = qsfp_read(qsfp, SFF8472_DDM_TH, &diag_l.sff8472_ddm_limits,
                                      sizeof(diag_l.sff8472_ddm_limits));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "QSFP read error: %d\n", ret);
            return -1;
        }
        qsfp->diag = diag_l;
        return 0;
    case SFF8024_ID_QSFP28_8636:
    case SFF8024_ID_QSFP_8436_8636:
        /* Page 00h, Byte-2 Bit-2 */
        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            TRX_LOG_ERR(qsfp, "TRX does not support alarm and "
                               "warning threshold limits\n");
            return 0;
        }

        /* Page 03h Bytes 128-199 */
        ret = qsfp_read(qsfp, SFF8636_DDM_TH, &diag_l.sff8636_ddm_limits,
                              sizeof(diag_l.sff8636_ddm_limits));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "QSFP read error: %d\n", ret);
            return -1;
        }
        qsfp->diag = diag_l;
        return 0;
    case SFF8024_ID_QSFPDD_CMIS:
        /* Page 00h, Byte-2 Bit-7 */
        if (qsfp->module_flat_mem == 0x01) {
            /* Module level monitor values supports only for paged
               memory modules*/
            TRX_LOG_ERR(qsfp,"TRX does not support alarm and "
                             "warning threshold limits\n");
            return 0;
        }

        /* Page 02h Bytes 128-199 */
        ret = qsfp_read(qsfp, CMIS_DDM_TH, &diag_l.cmis_ddm_limits,
                              sizeof(diag_l.cmis_ddm_limits));
        if (ret < 0) {
            TRX_LOG_ERR(qsfp, "QSFP read error: %d\n", ret);
            return -1;
        }
        qsfp->diag = diag_l;
        return 0;
    default:
        return -1;
    }
}

int qsfp_module_parse_laser_temp(struct qsfp* qsfp)
{
    u8 *spec_id;
    spec_id = (u8*)&qsfp->id;

    if((*spec_id == SFF8024_ID_QSFP28_8636) ||
       (*spec_id == SFF8024_ID_QSFP_8436_8636))
    {
        return parse_sff8636_page20_21(qsfp);
    }
    return 0;
}

