/**
 * main.c
 *
 * ECE 3849 Lab 1 Nicholas Lanotte Andrew Nagal
 * Built off of starter project made by Gene Bogdanov    10/18/2017
 *
 *
 * This version is using the new hardware for B2017: the EK-TM4C1294XL LaunchPad with BOOSTXL-EDUMKII BoosterPack.
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include "driverlib/fpu.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "Crystalfontz128x128_ST7735.h"
#include <stdio.h>
#include "buttons.h"
#include "OscilloscopeADC.h"
#include <math.h>
#include "driverlib/timer.h"
#include "inc/hw_memmap.h"



uint32_t gSystemClock; // [Hz] system clock frequency
volatile uint32_t gTime = 8345; // time in hundredths of a second
uint16_t ADCPrintBuffer[128];
extern volatile uint32_t gButtons;
volatile uint32_t VoltageScale = 2;
int gVoltageScaleStr[] = {
  100, 200, 500, 1
};
char str[50];   // string buffer

int binary_conversion(int num){
    if(num==0){
        return 0;
    }
    else
        return(num%2+10*binary_conversion(num/2));
}
#pragma FUNC_CANNOT_INLINE(cpu_load_count)
uint32_t cpu_load_count(void);
uint32_t count_unloaded = 0;
uint32_t count_loaded = 0;
float cpu_load = 0.0;

volatile uint16_t tDirection = 0;
volatile uint16_t tVoltage = 2048;

int main(void)
{
    FPUEnable();
    IntMasterDisable();

    // Enable the Floating Point Unit, and permit ISRs to use it
    FPUEnable();
    FPULazyStackingEnable();

    // Initialize the system clock to 120 MHz
    gSystemClock = SysCtlClockFreqSet(SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480, 120000000);

    Crystalfontz128x128_Init(); // Initialize the LCD display driver
    Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP); // set screen orientation

    ButtonInit();


    // initialize timer 3 in one-shot mode for polled timing
       SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER3);
       TimerDisable(TIMER3_BASE, TIMER_BOTH);
       TimerConfigure(TIMER3_BASE, TIMER_CFG_A_PERIODIC);
       TimerLoadSet(TIMER3_BASE, TIMER_A, gSystemClock/100-1); // .01 sec interval

       count_unloaded = cpu_load_count();
       initADC();
    IntMasterEnable();

    tContext sContext;
    GrContextInit(&sContext, &g_sCrystalfontz128x128); // Initialize the grlib graphics context


int VIN_RANGE = 3.3;
int PIXELS_PER_DIV = 16;
int ADC_BITS = 12;
int ADC_OFFSET = 2048;
float fVoltsPerDiv = 0.5;

    float fScale = (VIN_RANGE * PIXELS_PER_DIV)/((1 << ADC_BITS) * fVoltsPerDiv);
    // full-screen rectangle
    tRectangle rectFullScreen = {0, 0, GrContextDpyWidthGet(&sContext)-1, GrContextDpyHeightGet(&sContext)-1};

    while (true) {

        GrContextForegroundSet(&sContext, ClrBlack);
        GrRectFill(&sContext, &rectFullScreen); // fill screen with black
        GrContextForegroundSet(&sContext, ClrBlue);
        GrLineDrawV(&sContext, 15, 0, 127);
        GrLineDrawV(&sContext, 31, 0, 127);
        GrLineDrawV(&sContext, 47, 0, 127);
        GrLineDrawV(&sContext, 63, 0, 127);
        GrLineDrawV(&sContext, 79, 0, 127);
        GrLineDrawV(&sContext, 95, 0, 127);
        GrLineDrawV(&sContext, 111, 0, 127);


        GrLineDrawH(&sContext, 0, 127, 15);
        GrLineDrawH(&sContext, 0, 127, 31);
        GrLineDrawH(&sContext, 0, 127, 47);
        GrLineDrawH(&sContext, 0, 127, 63);
        GrLineDrawH(&sContext, 0, 127, 79);
        GrLineDrawH(&sContext, 0, 127, 95);
        GrLineDrawH(&sContext, 0, 127, 111);

        GrContextForegroundSet(&sContext, ClrWhite); // yellow text
        GrContextFontSet(&sContext, &g_sFontFixed6x8); // select font
        uint32_t localV = VoltageScale;


        if(localV == 0){
            fVoltsPerDiv = .1;
              GrStringDraw(&sContext, "100mV", /*length*/ -1, /*x*/ 40, /*y*/ 1, /*opaque*/ false);
        }
        else if(localV == 1){
            fVoltsPerDiv = .2;
            GrStringDraw(&sContext, "200mV", /*length*/ -1, /*x*/ 40, /*y*/ 1, /*opaque*/ false);
        }
        else if(localV == 2){
            fVoltsPerDiv = .5;
            GrStringDraw(&sContext, "500mV", /*length*/ -1, /*x*/ 40, /*y*/ 1, /*opaque*/ false);
                }
        else{
            fVoltsPerDiv = 1;
            GrStringDraw(&sContext, " 1V", /*length*/ -1, /*x*/ 40, /*y*/ 1, /*opaque*/ false);
        }
        fScale = (VIN_RANGE * PIXELS_PER_DIV)/((1 << ADC_BITS) * fVoltsPerDiv);
        GrStringDraw(&sContext, "16us", /*length*/ -1, /*x*/ 5, /*y*/ 1, /*opaque*/ false);
        count_loaded = cpu_load_count();
              cpu_load = 1.0f - (float)count_loaded/count_unloaded; // compute CPU load
              char str[50];   // string buffer
              snprintf(str, sizeof(str), "CPU load = %.1f %%", cpu_load*100); // convert time to string
              GrStringDraw(&sContext, str, /*length*/ -1, /*x*/ 5, /*y*/ 120, /*opaque*/ false);

       GrLineDrawV(&sContext, 80, 11, 1);
       GrLineDrawH(&sContext, 75, 80, 11);
       GrLineDrawH(&sContext, 80, 85, 1);

       if(tDirection == 0){
           GrLineDraw(&sContext, 78, 5, 79, 6);
           GrLineDraw(&sContext, 82, 5, 81,  6);
       }
       else{
           GrLineDraw(&sContext, 78, 6, 79, 5);
           GrLineDraw(&sContext, 82, 6, 81,  5);
       }
       uint16_t tVoltageL = tVoltage;
       float tVLevel = ((float)tVoltageL - ADC_OFFSET)/4096*3.3;
       snprintf(str, sizeof(str), "%.2f V", tVLevel); // convert time to string
       GrStringDraw(&sContext, str, /*length*/ -1, /*x*/ 90, /*y*/ 1, /*opaque*/ false);
       GrContextForegroundSet(&sContext, ClrPurple); // yellow text
       GrLineDrawH(&sContext, 0, 127, LCD_VERTICAL_MAX/2 - (int)roundf(fScale * (tVoltageL - ADC_OFFSET)));




        GrContextForegroundSet(&sContext, ClrYellow); // yellow text
        GetWaveform(tDirection, tVoltageL);

        int i;
       for(i = 0; i<127; i++){

            int sample = ADCPrintBuffer[i];
            int nsample = ADCPrintBuffer[i+1];
            int CurY = LCD_VERTICAL_MAX/2 - (int)roundf(fScale * (sample - ADC_OFFSET));
            int NextY = LCD_VERTICAL_MAX/2 - (int)roundf(fScale * (nsample - ADC_OFFSET));
            GrLineDraw(&sContext, i, CurY, i+1, NextY);

        }


        GrFlush(&sContext); // flush the frame buffer to the LCD
    }
}

uint32_t cpu_load_count(void)
{
    uint32_t i = 0;
    TimerIntClear(TIMER3_BASE, TIMER_TIMA_TIMEOUT);
    TimerEnable(TIMER3_BASE, TIMER_A); // start one-shot timer
    while (!(TimerIntStatus(TIMER3_BASE, false) & TIMER_TIMA_TIMEOUT))
        i++;
    return i;
}

