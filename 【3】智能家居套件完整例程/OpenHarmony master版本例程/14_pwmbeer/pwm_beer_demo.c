/*
 * Copyright (c) 2020, HiHope Community.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "iot_errno.h"
#include "hi_pwm.h"
#include "hi_io.h"

static int g_beepState = 0;
#define     CLK_160M                160000000
#define     IOT_GPIO_IDX_9          9
#define     IOT_GPIO_IDX_8          8
#define     IOT_GPIO_PWM_FUNCTION   5
#define     IOT_PWM_PORT_PWM0       0
#define     IO_FUNC_GPIO_8_GPIO     0
#define     IOT_IO_PULL_UP          1

static void *PWMBeerTask(const char *arg)
{
    (void)arg;

    printf("PWMBeerTask start!\r\n");
    
    while (1) {
        if (g_beepState) {
            IoTPwmStart(IOT_PWM_PORT_PWM0, 50, 4000);
        } else {
            IoTPwmStop(IOT_PWM_PORT_PWM0);
        }
    }

    return NULL;
}

static void OnButtonPressed(char *arg)
{
    (void) arg;
    g_beepState = !g_beepState;
}

static void StartPWMBeerTask(void)
{
    osThreadAttr_t attr;


    IoTGpioInit(IOT_GPIO_IDX_9);
    hi_io_set_func(IOT_GPIO_IDX_9, IOT_GPIO_PWM_FUNCTION);
    IoTGpioSetDir(IOT_GPIO_IDX_9, IOT_GPIO_DIR_OUT);
    IoTPwmInit(IOT_PWM_PORT_PWM0);

    hi_io_set_func(IOT_GPIO_IDX_8, IO_FUNC_GPIO_8_GPIO);
    IoTGpioSetDir(IOT_GPIO_IDX_8, IOT_GPIO_DIR_IN);
    hi_io_set_pull(IOT_GPIO_IDX_8, IOT_IO_PULL_UP);
    IoTGpioRegisterIsrFunc(IOT_GPIO_IDX_8, IOT_INT_TYPE_EDGE, IOT_GPIO_EDGE_FALL_LEVEL_LOW,
        OnButtonPressed, NULL);

    IoTWatchDogDisable();

    attr.name = "PWMBeerTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024;
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)PWMBeerTask, NULL, &attr) == NULL) {
        printf("[StartPWMBeerTask] Falied to create PWMBeerTask!\n");
    }
}

APP_FEATURE_INIT(StartPWMBeerTask);