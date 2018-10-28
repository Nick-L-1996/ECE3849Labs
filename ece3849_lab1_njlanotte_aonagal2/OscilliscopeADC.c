/*
 * OscilliscopeADC.c
 *
 *  Created on: Oct 25, 2018
 *      Author: Nicholas Lanotte
 */


#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "driverlib/adc.h"
#include "sysctl_pll.h"
#include "OscilloscopeADC.h"
#include "inc/tm4c1294ncpdt.h"

extern uint32_t gSystemClock;   // [Hz] system clock frequency
extern volatile uint32_t gTime; // time in hundredths of a second
#define ADC_BUFFER_SIZE 2048 // size must be a power of 2
#define ADC_BUFFER_WRAP(i) ((i) & (ADC_BUFFER_SIZE - 1)) // index wrapping macro
volatile int32_t gADCBufferIndex = ADC_BUFFER_SIZE - 1; // latest sample index
volatile uint16_t gADCBuffer[ADC_BUFFER_SIZE]; // circular buffer
volatile uint32_t gADCErrors; // number of missed ADC deadlines
uint16_t prevVoltage = 0;
int loopCount = 0;
extern uint16_t ADCPrintBuffer[128];
int32_t ADCLocalBufferIndex;
int32_t LastIndex=0;


void initADC(void){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_0 ); // GPIO setup for analog input AIN3
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1);

    // ADC clock
    uint32_t pll_frequency = SysCtlFrequencyGet(CRYSTAL_FREQUENCY);
    uint32_t pll_divisor = (pll_frequency - 1) / (16 * ADC_SAMPLING_RATE) + 1; //round up
    ADCClockConfigSet(ADC1_BASE, ADC_CLOCK_SRC_PLL | ADC_CLOCK_RATE_FULL, pll_divisor);
    ADCSequenceDisable(ADC1_BASE, 0); // choose ADC1 sequence 0; disable before configuring
    ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_ALWAYS, 0); // specify the "Always" trigger
    ADCSequenceStepConfigure(ADC1_BASE, 0, 0, ADC_CTL_CH3| ADC_CTL_IE | ADC_CTL_END);// in the 0th step, sample channel 3 (AIN3) **Might need changing

    // enable interrupt, and make it the end of sequence
    ADCSequenceEnable(ADC1_BASE, 0); // enable the sequence. it is now sampling
    ADCIntEnable(ADC1_BASE, 0); // enable sequence 0 interrupt in the ADC1 peripheral
    IntPrioritySet(INT_ADC1SS0, 0); // set ADC1 sequence 0 interrupt priority
    IntEnable(INT_ADC1SS0); // enable ADC1 sequence 0 interrupt in int. controller

}
void GetWaveform(int Direction, uint16_t Voltage){
    ADCLocalBufferIndex =  ADC_BUFFER_WRAP(gADCBufferIndex-64); //index begins at half a screen behind the most recent sample
    prevVoltage = gADCBuffer[ADCLocalBufferIndex];
    int searching = 1;
    loopCount = 0;
    ADCLocalBufferIndex = ADC_BUFFER_WRAP(ADCLocalBufferIndex-1);
    while(searching){
        loopCount = loopCount +1;
        if(loopCount>=(ADC_BUFFER_SIZE/2)){
            ADCLocalBufferIndex = ADC_BUFFER_WRAP(ADCLocalBufferIndex+ADC_BUFFER_SIZE/2-1);
            break;
        }
        if(Direction == 0){ // 0 is falling
            if(prevVoltage>Voltage && gADCBuffer[ADCLocalBufferIndex]<=Voltage){
                searching = 0;
            }
            else{
                prevVoltage = gADCBuffer[ADCLocalBufferIndex];
                LastIndex = ADCLocalBufferIndex;
                ADCLocalBufferIndex = ADC_BUFFER_WRAP(ADCLocalBufferIndex-1)-63; //for some reason the adc buffer wrap increments the count by 64 instead of 1

            }
        }
        else{
            if(prevVoltage<Voltage && gADCBuffer[ADCLocalBufferIndex]>=Voltage){
                          searching = 0;
                      }
                      else{
                          prevVoltage = gADCBuffer[ADCLocalBufferIndex];
                          LastIndex = ADCLocalBufferIndex;
                          ADCLocalBufferIndex = ADC_BUFFER_WRAP(ADCLocalBufferIndex - 1);

                      }
        }
        int copyCount = 0;
        ADCLocalBufferIndex = ADC_BUFFER_WRAP(ADCLocalBufferIndex-63);
        while(copyCount<128){
            ADCPrintBuffer[copyCount] = gADCBuffer[ADCLocalBufferIndex];
            copyCount = copyCount + 1;
            ADCLocalBufferIndex = ADC_BUFFER_WRAP(ADCLocalBufferIndex + 1);
        }
    }


}

void ADC_ISR(void){
    ADC1_ISC_R = ADC1_ISC_R & (0xFFFEFFF); // clear ADC1 sequence0 interrupt flag in the ADCISC register
    if (ADC1_OSTAT_R & ADC_OSTAT_OV0) { // check for ADC FIFO overflow
        gADCErrors++; // count errors
        ADC1_OSTAT_R = ADC_OSTAT_OV0; // clear overflow condition
    }
    gADCBuffer[
               gADCBufferIndex = ADC_BUFFER_WRAP(gADCBufferIndex + 1)
               ] = ADC1_SSFIFO0_R & 0x00000FFF; // read sample from the ADC1 sequence 0 FIFO
}
