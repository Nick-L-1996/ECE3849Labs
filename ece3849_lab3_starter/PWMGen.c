/*
 * Comparator.cpp
 *
 *  Created on: Dec 3, 2018
 *      Author: Nicholas Lanotte
 */
#include <stdint.h>
#include <stdbool.h>
#include "PWMGen.h"
#include "driverlib/pwm.h"
#include "driverlib/pin_map.h"
#include "inc/tm4c1294ncpdt.h"
#include "inc/hw_memmap.h"
#include "driverlib/fpu.h"
#include "driverlib/sysctl.h"
#include "sysctl_pll.h"
#include "driverlib/timer.h"
#include "driverlib/gpio.h"
#include <math.h>

uint32_t gPhase = 0;              // phase accumulator
uint32_t gPhaseIncrement = 147746875;     // phase increment for 16 kHz

#define PWM_WAVEFORM_INDEX_BITS 10
#define PWM_WAVEFORM_TABLE_SIZE (1 << PWM_WAVEFORM_INDEX_BITS)
uint8_t gPWMWaveformTable[PWM_WAVEFORM_TABLE_SIZE] = {0};
#define PWM_PERIOD 258
#define PI 3.14159265358979f





void initPWM(void){
    //initialize the PWM output
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    GPIOPinTypePWM(GPIO_PORTF_BASE, GPIO_PIN_1);//PF1 = M0PWM1
    GPIOPinConfigure(GPIO_PF1_M0PWM1);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_STRENGTH_8MA, GPIO_PIN_TYPE_STD);

    // configure the PWM0 peripheral
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
    PWMClockSet(PWM0_BASE, PWM_SYSCLK_DIV_1);             // use system clock
    PWMGenConfigure(PWM0_BASE, PWM_GEN_0, PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC);
    PWMGenPeriodSet(PWM0_BASE, PWM_GEN_0, PWM_PERIOD);
    PWMPulseWidthSet(PWM0_BASE, PWM_OUT_1, PWM_PERIOD/2); // initial 50% duty cycle
    PWMOutputInvert(PWM0_BASE, PWM_OUT_1_BIT, true);      // invert PWM output
    PWMOutputState(PWM0_BASE, PWM_OUT_1_BIT, true);       // enable PWM output
    PWMGenEnable(PWM0_BASE, PWM_GEN_0);                   // enable PWM generator

    PWMGenIntTrigEnable(PWM0_BASE, PWM_GEN_0, PWM_INT_CNT_ZERO);
    PWMIntEnable(PWM0_BASE, PWM_INT_GEN_0);
    int i;
    //Generate lookup table
    for(i=0; i<PWM_WAVEFORM_TABLE_SIZE; i++){
        gPWMWaveformTable[i] = roundf(sinf(2.f*PI*i/PWM_WAVEFORM_TABLE_SIZE)*127)+128;
    }
}

void PWM_ISR(void)
{
    PWMGenIntClear(PWM0_BASE, PWM_GEN_0, PWM_INT_CNT_ZERO ); // clear PWM interrupt flag
    gPhase += gPhaseIncrement;
    // write directly to the Compare B register that determines the duty cycle
    PWM0_0_CMPB_R = 1 + gPWMWaveformTable[gPhase >> (32 - PWM_WAVEFORM_INDEX_BITS)];
}

