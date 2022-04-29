/*
 * Copyright (C) 2021 HiHope Open Source Organization .
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 *
 * limitations under the License.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_i2c.h"
#include "iot_gpio.h"
#include "iot_pwm.h"
#include "iot_errno.h"
#include "hi_io.h"
//#include "iot_adc.h"
#include "aht20.h"
#include "oled_ssd1306.h"

#include "hi_adc.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) sizeof(a)/sizeof(a[0])
#endif

#define MS_PER_S 1000

#define BEEP_TIMES 3
#define BEEP_DURATION 100
#define BEEP_PWM_DUTY 30000
#define BEEP_PWM_FREQ 4000
#define BEEP_PIN_NAME 9
#define BEEP_PIN_FUNCTION 5
#define WIFI_IOT_PWM_PORT_PWM0 0

#define GAS_SENSOR_CHAN_NAME 5
// #define GAS_SENSOR_PIN_NAME WIFI_IOT_IO_NAME_GPIO_11

#define AHT20_BAUDRATE 400*1000
#define AHT20_I2C_IDX 0

#define ADC_RESOLUTION 2048

unsigned int g_sensorStatus = 0;

static float ConvertToVoltage(unsigned short data)
{
    return (float)data * 1.8 * 4 / 4096;
}

static void EnvironmentTask(void *arg)
{
    (void)arg;
    uint32_t retval = 0;
    float humidity = 0.0f;
    float temperature = 0.0f;
    float gasSensorResistance = 0.0f;
    static char line[32] = {0};

    OledInit();
    OledFillScreen(0);
    IoTI2cInit(AHT20_I2C_IDX, AHT20_BAUDRATE);

    // set BEEP pin as PWM function
    IoTGpioInit(BEEP_PIN_NAME);
    retval = hi_io_set_func(BEEP_PIN_NAME, BEEP_PIN_FUNCTION);
    if (retval != IOT_SUCCESS) {
        printf("IoTGpioInit(9) failed, %0X!\n", retval);
    }
    IoTGpioSetDir(9, IOT_GPIO_DIR_OUT);
    IoTPwmInit(WIFI_IOT_PWM_PORT_PWM0);

    for (int i = 0; i < BEEP_TIMES; i++) {
        snprintf(line, sizeof(line), "beep %d/%d", (i+1), BEEP_TIMES);
        OledShowString(0, 0, line, 1);

        IoTPwmStart(WIFI_IOT_PWM_PORT_PWM0, 50, BEEP_PWM_FREQ);
        usleep(BEEP_DURATION * 1000);
        IoTPwmStop(WIFI_IOT_PWM_PORT_PWM0);
        usleep((1000 - BEEP_DURATION) * 1000);
    }

    while (IOT_SUCCESS != AHT20_Calibrate()) {
        printf("AHT20 sensor init failed!\r\n");
        usleep(1000);
    }

    while(1) {
        retval = AHT20_StartMeasure();
        if (retval != IOT_SUCCESS) {
            printf("trigger measure failed!\r\n");
        }

        retval = AHT20_GetMeasureResult(&temperature, &humidity);
        if (retval != IOT_SUCCESS) {
            printf("get humidity data failed!\r\n");
        }

         unsigned short data = 0;
		 int ret;
         ret = hi_adc_read(GAS_SENSOR_CHAN_NAME, &data, HI_ADC_EQU_MODEL_4, HI_ADC_CUR_BAIS_DEFAULT, 0);
         if (ret == IOT_SUCCESS) {
             float Vx = ConvertToVoltage(data);

             // Vcc            ADC            GND
             //  |    ______   |     ______   |
             //  +---| MG-2 |---+---| 1kom |---+
             //       ------         ------
             // 查阅原理图，ADC 引脚位于 1K 电阻和燃气传感器之间，燃气传感器另一端接在 5V 电源正极上
             // 串联电路电压和阻止成正比：
             // Vx / 5 == 1kom / (1kom + Rx)
             //   => Rx + 1 == 5/Vx
             //   =>  Rx = 5/Vx - 1
             gasSensorResistance = 5 / Vx - 1;
			 printf("\r\n hi_adc_read ok, data=%d, vx=%f, gasSensorResistance=%f", data, Vx, gasSensorResistance);
        } else {
			printf("\r\n hi_adc_read fail, ret=%d", ret);
		}

        OledShowString(0, 0, "Sensor values:", 1);

        snprintf(line, sizeof(line), "temp: %.2f", temperature);
        OledShowString(0, 1, line, 1);

        snprintf(line, sizeof(line), "humi: %.2f", humidity);
        OledShowString(0, 2, line, 1);

        snprintf(line, sizeof(line), "gas: %.2f kom", gasSensorResistance);
        OledShowString(0, 3, line, 1);

		if (temperature > 35.0 || temperature < 0) {
			g_sensorStatus++;
			OledShowString(0, 5, "temp abnormal!!", 1);
		}     
		
		if (humidity < 20.0 || humidity > 50.0) {
			g_sensorStatus++;
			if (temperature > 35.0 || temperature < 0) { 
				OledShowString(0, 6, "humi abnormal!!", 1);            
			}   else {
				OledShowString(0, 5, "humi abnormal!!", 1);            
			}        
		}        

		if (g_sensorStatus > 0) { 
			IoTPwmStart(WIFI_IOT_PWM_PORT_PWM0, 50, 4000);            
			usleep(500000);            
			IoTPwmStop(WIFI_IOT_PWM_PORT_PWM0);            
			g_sensorStatus = 0;        
		} 

        usleep(500000);
    }
}

static void EnvironmentDemo(void)
{
    osThreadAttr_t attr;

    IoTGpioInit(BEEP_PIN_NAME);
    hi_io_set_func(BEEP_PIN_NAME, BEEP_PIN_FUNCTION);
    IoTPwmInit(WIFI_IOT_PWM_PORT_PWM0);

    attr.name = "EnvironmentTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 4096;
    attr.priority = osPriorityNormal;

    if (osThreadNew(EnvironmentTask, NULL, &attr) == NULL) {
        printf("[EnvironmentDemo] Falied to create EnvironmentTask!\n");
    }
}

APP_FEATURE_INIT(EnvironmentDemo);