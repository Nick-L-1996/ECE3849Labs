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



uint32_t gSystemClock; // [Hz] system clock frequency
volatile uint32_t gTime = 8345; // time in hundredths of a second
uint16_t ADCPrintBuffer[128];
extern volatile uint32_t gButtons;

int binary_conversion(int num){
    if(num==0){
        return 0;
    }
    else
        return(num%2+10*binary_conversion(num/2));
}

int main(void)
{
    IntMasterDisable();

    // Enable the Floating Point Unit, and permit ISRs to use it
    FPUEnable();
    FPULazyStackingEnable();

    // Initialize the system clock to 120 MHz
    gSystemClock = SysCtlClockFreqSet(SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480, 120000000);

    Crystalfontz128x128_Init(); // Initialize the LCD display driver
    Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP); // set screen orientation

    ButtonInit();
    initADC();
    IntMasterEnable();

    tContext sContext;
    GrContextInit(&sContext, &g_sCrystalfontz128x128); // Initialize the grlib graphics context


int VIN_RANGE = 3.3;
int PIXELS_PER_DIV = 16;
int ADC_BITS = 12;
int ADC_OFFSET = 2200;
float fVoltsPerDiv = 0.5;
    uint16_t Voltage = 1000;
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


        GrContextForegroundSet(&sContext, ClrYellow); // yellow text
        GetWaveform(0, Voltage);

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
