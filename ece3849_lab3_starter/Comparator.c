/*
 * Comparator.cpp
 *
 *  Created on: Dec 3, 2018
 *      Author: Nicholas Lanotte
 */
#include <stdint.h>
#include <stdbool.h>
#include <Comparator.h>
#include "driverlib/comp.h"
#include "driverlib/pin_map.h"
#include "inc/tm4c1294ncpdt.h"
#include "inc/hw_memmap.h"
#include "driverlib/fpu.h"
#include "driverlib/sysctl.h"
#include "sysctl_pll.h"
#include "driverlib/timer.h"
#include "driverlib/gpio.h"

volatile uint32_t LastTime = 0;
volatile uint32_t DiffTime = 0;
volatile uint32_t MultiPeriodInterval = 0;
volatile uint32_t NumPeriods = 0;


void initComp(){//configure the comparator
    SysCtlPeripheralEnable(SYSCTL_PERIPH_COMP0);
    ComparatorRefSet(COMP_BASE, COMP_REF_1_65V);
    ComparatorConfigure(COMP_BASE, 1, COMP_OUTPUT_NORMAL|COMP_TRIG_NONE|COMP_INT_RISE|COMP_ASRCP_REF);
//set up comp input
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
    GPIOPinTypeComparator(GPIO_PORTC_BASE, GPIO_PIN_4);
//set up comp output
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    GPIOPinTypeComparatorOutput(GPIO_PORTD_BASE, GPIO_PIN_1);
    GPIOPinConfigure(GPIO_PD1_C1O);
//set up timer capture pin
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    GPIOPinTypeTimer(GPIO_PORTD_BASE, GPIO_PIN_0);
    GPIOPinConfigure(GPIO_PD0_T0CCP0);
//configure timer 0 for capture
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    TimerDisable(TIMER0_BASE, TIMER_BOTH);
    TimerConfigure(TIMER0_BASE, TIMER_CFG_SPLIT_PAIR|TIMER_CFG_A_CAP_TIME_UP);
    TimerControlEvent(TIMER0_BASE, TIMER_A, TIMER_EVENT_POS_EDGE);
    TimerLoadSet(TIMER0_BASE, TIMER_A, 0xffff);
    TimerPrescaleSet(TIMER0_BASE, TIMER_A, 0xff);
    TimerIntEnable(TIMER0_BASE, TIMER_CAPA_EVENT);
    TimerEnable(TIMER0_BASE, TIMER_A);



}
void CompISR(void){//calculates the period of the waveform and adds it to a total
    TimerIntClear(TIMER0_BASE, TIMER_CAPA_EVENT);
    uint32_t CurrentTime = TimerValueGet(TIMER0_BASE, TIMER_A);
    uint32_t DiffTime = (CurrentTime-LastTime) &0xffffff;
    LastTime = CurrentTime;
    MultiPeriodInterval+=DiffTime;
    NumPeriods++;

}
