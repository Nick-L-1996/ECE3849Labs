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
volatile int32_t ADCLocalBufferIndex;
int32_t LastIndex=0;


void initADC(void){
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_0 ); // GPIO setup for analog input AIN3
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC1);

    //set ADC Timer trigger clock
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);
    TimerDisable(TIMER2_BASE, TIMER_BOTH);
    TimerConfigure(TIMER2_BASE, TIMER_CFG_PERIODIC);
    TimerLoadSet(TIMER2_BASE, TIMER_A, (float)gSystemClock/480 );
    TimerEnable(TIMER2_BASE, TIMER_BOTH);
    TimerControlTrigger(TIMER2_BASE, TIMER_A, 0);

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


void GetWaveform(int Direction, uint16_t Voltage){//gets 128 samples from the circular buffer to be plotted on the display
    ADCLocalBufferIndex =  ADC_BUFFER_WRAP(gADCBufferIndex -64); //index begins at half a screen behind the most recent sample
    prevVoltage = gADCBuffer[ADCLocalBufferIndex];
    int searching = 1;
    loopCount = 0;//used to count loops
    ADCLocalBufferIndex = ADC_BUFFER_WRAP(ADCLocalBufferIndex -1);
    while(searching){
        loopCount = loopCount +1;//keeps track of the loops so that it stops once half the circular buffer is searched
        if(loopCount>=(ADC_BUFFER_SIZE/2)){
            ADCLocalBufferIndex = ADC_BUFFER_WRAP(ADCLocalBufferIndex+(int32_t) (ADC_BUFFER_SIZE/2-1));
            break;
        }
        if(Direction == 0){ // 0 is falling. Checks if voltage crosses the set threshold
            if(prevVoltage<Voltage && gADCBuffer[ADCLocalBufferIndex]>=Voltage){
                searching = 0;
            }
            else{//if threshold not crossed, prepare to check next point
                prevVoltage = gADCBuffer[ADCLocalBufferIndex];
                LastIndex = ADCLocalBufferIndex;
                ADCLocalBufferIndex = ADC_BUFFER_WRAP(ADCLocalBufferIndex - 1); //for some reason the adc buffer wrap increments the count by 64 instead of 1
            }
        }
        else{//rising edge. Check is voltage has crosses the set threshold
            if(prevVoltage>Voltage && gADCBuffer[ADCLocalBufferIndex]<=Voltage){
                searching = 0;
            }//if threshold not crossed, prepare to check next point
            else{
                prevVoltage = gADCBuffer[ADCLocalBufferIndex];
                LastIndex = ADCLocalBufferIndex;
                ADCLocalBufferIndex = ADC_BUFFER_WRAP(ADCLocalBufferIndex - 1);
            }
        }
    }
    //once trigger is found, pull 128 samples from buffer about that point
    int copyCount = 0;
    ADCLocalBufferIndex = ADC_BUFFER_WRAP(ADCLocalBufferIndex-63);
    while(copyCount<128){
        ADCPrintBuffer[copyCount] = gADCBuffer[ADCLocalBufferIndex];
        copyCount = copyCount + 1;
        ADCLocalBufferIndex = ADC_BUFFER_WRAP(ADCLocalBufferIndex + 1);
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
               ] = (ADC1_SSFIFO0_R); // read sample from the ADC1 sequence 0 FIFO
}

void changeADCSampleRate(int state){
        ADCSequenceDisable(ADC1_BASE, 0); // choose ADC1 sequence 0; disable before configuring
        TimerDisable(TIMER2_BASE, TIMER_BOTH);
        switch(state){
        case 0:
            ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_TIMER, 1); // set time scale to 100ms
            TimerLoadSet(TIMER2_BASE, TIMER_A, 600000 );
            TimerControlTrigger(TIMER2_BASE, TIMER_A, 1);
            break;

        case 1:
           ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_TIMER, 1); // set time scale to 50ms
           TimerLoadSet(TIMER2_BASE, TIMER_A, 300000 );
           TimerControlTrigger(TIMER2_BASE, TIMER_A, 1);
           break;

        case 2:
           ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_TIMER, 1); // set time scale to 20ms
           TimerLoadSet(TIMER2_BASE, TIMER_A, 120000 );
           TimerControlTrigger(TIMER2_BASE, TIMER_A, 1);
           break;

        case 3:
            ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_TIMER, 1); // set time scale to 10ms
            TimerLoadSet(TIMER2_BASE, TIMER_A, 60000 );
            TimerControlTrigger(TIMER2_BASE, TIMER_A, 1);
            break;

        case 4:
            ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_TIMER, 1); // set time scale to 2ms
            TimerLoadSet(TIMER2_BASE, TIMER_A, 12000 );
            TimerControlTrigger(TIMER2_BASE, TIMER_A, 1);
            break;

        case 5:
            ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_TIMER, 1); // set time scale to 800us
            TimerLoadSet(TIMER2_BASE, TIMER_A, 4800 );
            TimerControlTrigger(TIMER2_BASE, TIMER_A, 1);
            break;

        case 6:
            ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_TIMER, 1); // set time scale to 400us
            TimerLoadSet(TIMER2_BASE, TIMER_A, 2400 );
            TimerControlTrigger(TIMER2_BASE, TIMER_A, 1);
            break;

        case 7:
            ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_TIMER, 1); // set time scale to 200us
            TimerLoadSet(TIMER2_BASE, TIMER_A, 1200 );
            TimerControlTrigger(TIMER2_BASE, TIMER_A, 1);
            break;

        case 8:
            ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_TIMER, 1); // set time scale to 50us
            TimerLoadSet(TIMER2_BASE, TIMER_A, 300 );
            TimerControlTrigger(TIMER2_BASE, TIMER_A, 1);
            break;

        case 9:
            ADCSequenceConfigure(ADC1_BASE, 0, ADC_TRIGGER_ALWAYS, 0); // set time scale to 20us
            TimerControlTrigger(TIMER2_BASE, TIMER_A, 0);
            break;

        }
        ADCSequenceEnable(ADC1_BASE, 0); // enable the sequence. it is now sampling
        TimerEnable(TIMER2_BASE, TIMER_BOTH);
}
