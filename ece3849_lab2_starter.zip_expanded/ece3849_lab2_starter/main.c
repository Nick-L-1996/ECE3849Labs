/*
 * ECE 3849 Lab2 starter project
 *
 * Gene Bogdanov    9/13/2017
 */
/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>
#include <xdc/cfg/global.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include "inc/tm4c1294ncpdt.h"
#include <stdint.h>
#include <stdbool.h>
#include "driverlib/interrupt.h"

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

#include <math.h>
#include "kiss_fft.h"
#include "_kiss_fft_guts.h"

#define PI 3.14159265358979f
#define NFFT 1024
#define KISS_FFT_CFG_SIZE (sizeof(struct kiss_fft_state)+sizeof(kiss_fft_cpx)*(NFFT-1))

#define SYSTEM_CLOCK_MHZ 120
uint32_t gSystemClock = 120000000; // [Hz] system clock frequency
volatile uint32_t gTime = 8345; // time in hundredths of a second
uint16_t ADCPrintBuffer[128];
uint16_t ADCScaledPrintBuffer[128];
uint16_t RawSpectrumBuffer[1024];
uint16_t ProcessedSpectrumBuffer[1024];
extern volatile uint32_t gButtons;
volatile uint32_t VoltageScale = 2;
volatile int ADCSampleState = 9;
volatile int OscilloscopeMode = 0;
int prevADCSampleState = 9;
char str[50];   // string buffer
const char * const TimeScaleStr[]= {
   "100ms", "50ms", "20ms", "10ms", "2ms", "800us", "400us", "200us", "50us", "20us"
};
const char * const VoltageScaleStr[] = {
  "100mV", "200mV", "500mV", "1V"
};
//Variables for determining trigger
volatile uint16_t tDirection = 0;
volatile uint16_t tVoltage = 2048;
uint32_t gpresses = 0;
uint32_t ticksAtInterrupt = 0;

uint32_t ButtonIntTime = 0;
uint32_t Button_Latency = 0;
uint32_t Button_Response_Time = 0;

#pragma FUNC_CANNOT_INLINE(cpu_load_count)
uint32_t cpu_load_count(void);
uint32_t count_unloaded = 0;
uint32_t count_loaded = 0;
float cpu_load = 0.0;
//Variables for plotting oscilloscope readings
    int VIN_RANGE = 3.3;
    int PIXELS_PER_DIV = 20;
    int ADC_BITS = 12;
    int ADC_OFFSET = 2048;
    float fVoltsPerDiv = 0.5;
    float fScale = 0;
#define BUTTONPERIOD = 5000
uint32_t TIMER0_PERIOD = 120 * 5000;
int buttonMissedDeadlines = 0;
uint32_t WaveformResponseTime=0;
uint32_t WaveformTimerValue = 0;
int waveformErrors = 0;
/*
 *  ======== main ========
 */
int main(void)
{
    IntMasterDisable();
    // Enable the Floating Point Unit, and permit ISRs to use it
    FPUEnable();
    FPULazyStackingEnable();

    //Initialize the system clock to 120 MHz
    gSystemClock = SysCtlClockFreqSet(SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480, 120000000);
    Crystalfontz128x128_Init(); // Initialize the LCD display driver
    Crystalfontz128x128_SetOrientation(LCD_ORIENTATION_UP); // set screen orientation
    count_unloaded = cpu_load_count();//measure cpu load with ints disabled
    // initialize buttons
    ButtonInit();
    initADC();//enable last so that the ADC doesnt have an error

    IntMasterEnable();//enable interrupts
    fScale = (VIN_RANGE * PIXELS_PER_DIV)/((1 << ADC_BITS) * fVoltsPerDiv);

    /* Start BIOS */
    BIOS_start();
    return (0);
}

//signals button task every 5ms
void Clock_Task(UArg arg1, UArg arg2){
    ButtonIntTime =  (TIMER0_PERIOD-TIMER0_TAR_R)/120;//used to time button task latency and response time
    Semaphore_post(SignalButtonTask);//signal button task
}
void task_GetButtons(UArg arg1, UArg arg2)
{
    IntMasterEnable();
    while (true) {
        Semaphore_pend(SignalButtonTask, BIOS_WAIT_FOREVER);//waits to be signalled
        uint32_t readTime = (TIMER0_PERIOD-TIMER0_TAR_R)/120;//stores current time
        uint32_t temptime = (readTime - ButtonIntTime)/120;//calculates latency

        if(temptime>Button_Latency){//stores highest latency value
            Button_Latency = temptime;
        }
        gpresses = getButtonPresses();//retrieve button press data
        if(gpresses !=0){//if at least 1 button is pressed...
            Mailbox_post(ButtonIDs, &gpresses, BIOS_WAIT_FOREVER);//add data to mailbox , signal user input task
        }
        uint32_t temptime2=0;
        if(Semaphore_getCount(SignalButtonTask)){//checks to see if deadline missed
                   buttonMissedDeadlines++;//if so increment missed deadlines by 1
                   temptime2 = 2 * TIMER0_PERIOD;//calculate response time
               }
        else{
            temptime2 = (TIMER0_PERIOD-TIMER0_TAR_R)/120;//otherwise calculate response time
        }
        if(temptime2>Button_Response_Time){//if response time greater than previous high, store it
            Button_Response_Time = temptime2;
        }
    }
}
//called by user input task
void task_UserInput(UArg arg1, UArg arg2){
    IntMasterEnable();
       while (true) {
           uint32_t presses;
           Mailbox_pend(ButtonIDs, &presses, BIOS_WAIT_FOREVER);//store buttons into local variable
           HandleButtonPress(presses);//calls helper function to process button events
           int lADCSampleState = ADCSampleState;//This is state variable for time scale. Atomic read in to prevent shared data issues
           if(lADCSampleState != prevADCSampleState){
                changeADCSampleRate(lADCSampleState);//changes ADC sample trigger depending on time scale selected
            }
           prevADCSampleState = lADCSampleState;//remembers the last state value for comparison next loop
           uint32_t localV = VoltageScale;
                     //Sets voltage per division text depending on the current setting
                     switch(localV){
                         case 0:
                             fVoltsPerDiv = .1;
                             break;
                         case 1:
                             fVoltsPerDiv = .2;
                             break;
                         case 2:
                             fVoltsPerDiv = .5;
                             break;
                         case 3:
                             fVoltsPerDiv = 1;
                     }
            fScale = (VIN_RANGE * PIXELS_PER_DIV)/((1 << ADC_BITS) * fVoltsPerDiv);//adjusts voltage scale
           Semaphore_post(SignalDisplay);//Signal display to refresh
       }
}
//writes to the LCD display
void Display_Task(UArg arg1, UArg arg2){

    IntMasterEnable();
    tContext sContext;//Context for Display
    GrContextInit(&sContext, &g_sCrystalfontz128x128); // Initialize the grlib graphics context
    GrContextFontSet(&sContext, &g_sFontFixed6x8); // select font
    // full-screen rectangle
    tRectangle rectFullScreen = {0, 0, GrContextDpyWidthGet(&sContext)-1, GrContextDpyHeightGet(&sContext)-1};
          while (true) {
            Semaphore_pend(SignalDisplay, BIOS_WAIT_FOREVER);//waits to be signaled
            //////////////////////////////////////////////////////Waveform Mode//////////////////////////////////////
            if(OscilloscopeMode == 0){//if in waveform mode...
                GrContextForegroundSet(&sContext, ClrBlack);
                GrRectFill(&sContext, &rectFullScreen);



                ///////////////////draw division lines for LCD display//////////////////////
                GrContextForegroundSet(&sContext, ClrDarkBlue);//make LCD Lines blue

                //Vertical lines
                GrLineDrawV(&sContext, 3, 0, 127);
                GrLineDrawV(&sContext, 23, 0, 127);
                GrLineDrawV(&sContext, 43, 0, 127);
                GrLineDrawV(&sContext, 83, 0, 127);
                GrLineDrawV(&sContext, 103, 0, 127);
                GrLineDrawV(&sContext, 123, 0, 127);

                //Horizontal lines
                GrLineDrawH(&sContext, 0, 127, 3);
                GrLineDrawH(&sContext, 0, 127, 23);
                GrLineDrawH(&sContext, 0, 127, 43);
                GrLineDrawH(&sContext, 0, 127, 63);
                GrLineDrawH(&sContext, 0, 127, 83);
                GrLineDrawH(&sContext, 0, 127, 103);
                GrLineDrawH(&sContext, 0, 127, 123);

                GrContextForegroundSet(&sContext, ClrBlue);//make LCD Lines blue
                GrLineDrawV(&sContext, 63, 0, 127);
                /////////////////////////////////////Draws Trigger Voltage level in blue///////////////////
                uint16_t tVoltageL = tVoltage;//atomic read in to prevent shared data issues
                float tVLevel = ((float)tVoltageL - ADC_OFFSET)/4096*3.3;
                GrLineDrawH(&sContext, 0, 127, LCD_VERTICAL_MAX/2 - (int)roundf(fScale * (tVoltageL - ADC_OFFSET)));
                //////////////////////////////////////Set text color to white and write time/voltage scales and trigger icon //////////////////
                GrContextForegroundSet(&sContext, ClrWhite); // white text
                uint32_t localV = VoltageScale;
                GrStringDraw(&sContext, VoltageScaleStr[localV], /*length*/ -1, /*x*/ 40, /*y*/ 1, /*opaque*/ false);//writes V Scale text

                int lADCSampleState = ADCSampleState;
                //prints appropriate time scale value
                GrStringDraw(&sContext, TimeScaleStr[lADCSampleState], /*length*/ -1, /*x*/ 5, /*y*/ 1, /*opaque*/ false);

                snprintf(str, sizeof(str), "%.2fV", tVLevel); // convert time to string
                GrStringDraw(&sContext, str, /*length*/ -1, /*x*/ 90, /*y*/ 1, /*opaque*/ false);
                /////////////////////////Draws trigger icon//////////////////////
                GrLineDrawV(&sContext, 80, 11, 1);
                if(tDirection == 0){//if falling edge
                   GrLineDrawH(&sContext, 80, 85, 11);
                   GrLineDrawH(&sContext, 75, 80, 1);
                   GrLineDraw(&sContext, 78, 5, 79, 6);
                   GrLineDraw(&sContext, 82, 5, 81,  6);
                }
                else{//if rising edge
                   GrLineDrawH(&sContext, 75, 80, 11);
                   GrLineDrawH(&sContext, 80, 85, 1);
                   GrLineDraw(&sContext, 78, 6, 79, 5);
                   GrLineDraw(&sContext, 82, 6, 81,  5);
                }
               //////////////////Draw Waveform////////////////////////////////////////

                GrContextForegroundSet(&sContext, ClrYellow); // yellow waveform
                int i;
                for(i = 0; i<127; i++){
                    int sample = ADCScaledPrintBuffer[i];
                    int nsample = ADCScaledPrintBuffer[i+1];
                    GrLineDraw(&sContext, i, sample, i+1, nsample);
                }
                GrFlush(&sContext); // flush the frame buffer to the LCD
            }
       ////////////////////////////////////////////////////Sprectrum Mode/////////////////////////////////////////
            else{//if in Spectrum mode
                GrContextForegroundSet(&sContext, ClrBlack);
                GrRectFill(&sContext, &rectFullScreen);
                GrContextForegroundSet(&sContext, ClrBlue);//make LCD Lines blue
                GrLineDrawH(&sContext, 0, 127, 23);

                GrContextForegroundSet(&sContext, ClrDarkBlue);//make LCD Lines blue

                //Vertical lines
                GrLineDrawV(&sContext, 3, 0, 127);
                GrLineDrawV(&sContext, 23, 0, 127);
                GrLineDrawV(&sContext, 43, 0, 127);
                GrLineDrawV(&sContext, 63, 0, 127);
                GrLineDrawV(&sContext, 83, 0, 127);
                GrLineDrawV(&sContext, 103, 0, 127);
                GrLineDrawV(&sContext, 123, 0, 127);

                //Horizontal lines
                GrLineDrawH(&sContext, 0, 127, 3);
                GrLineDrawH(&sContext, 0, 127, 43);
                GrLineDrawH(&sContext, 0, 127, 63);
                GrLineDrawH(&sContext, 0, 127, 83);
                GrLineDrawH(&sContext, 0, 127, 103);
                GrLineDrawH(&sContext, 0, 127, 123);
                GrContextForegroundSet(&sContext, ClrWhite); // white text
                GrStringDraw(&sContext, "20 kHz", /*length*/ -1, /*x*/ 5, /*y*/ 1, /*opaque*/ false);
                GrStringDraw(&sContext, "20 dB", /*length*/ -1, /*x*/ 50, /*y*/ 1, /*opaque*/ false);//writes V Scale text

                GrContextForegroundSet(&sContext, ClrYellow); // yellow waveform
                int i;
                //draw lines between points in processed Spectrum buffer
                for(i = 0; i<127; i++){
                   int sample = ProcessedSpectrumBuffer[i];
                   int nsample = ProcessedSpectrumBuffer[i+1];
                   GrLineDraw(&sContext, i, sample, i+1, nsample);
                }
            GrFlush(&sContext); // flush the frame buffer to the LCD
            }
        }
   }

void Waveform_Task(UArg arg1, UArg arg2){
    IntMasterEnable();
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER3);
          TimerDisable(TIMER3_BASE, TIMER_BOTH);
          TimerConfigure(TIMER3_BASE, TIMER_CFG_A_PERIODIC);
          TimerLoadSet(TIMER3_BASE, TIMER_A, (gSystemClock-1)/100); // .01 sec interval

          while (true) {
             Semaphore_pend(SignalWaveform, BIOS_WAIT_FOREVER);//wait to be signaled

             //This timer is used to measure the response time of this task. Gets reset every loop
             TimerDisable(TIMER3_BASE, TIMER_BOTH);
             TimerLoadSet(TIMER3_BASE, TIMER_A, (gSystemClock-1)/100); // .01 sec interval
             TimerEnable(TIMER3_BASE, TIMER_A); // start one-shot timer from

             WaveformTimerValue = TIMER3_TAR_R;//take time value after timer reset
              if(OscilloscopeMode == 0){
                  uint16_t tDirectionL = tDirection;//local variable to prevent shared data issues
                  uint32_t tVoltageL = tVoltage;//atomic read in to prevent shared data issues
                  GetWaveform(tDirectionL, tVoltageL);//puts 128 samples into global frame buffer
              }
              else{
                  GetSpectrum();//store 1024 samples from adc buffer
              }
              uint32_t tempTime = (1200000-TIMER3_TAR_R)/120;//measure response time
              if(Semaphore_getCount(SignalWaveform)){//make sure the deadline for the task met (not possible to be missed in this implementation)
                  waveformErrors++;
              }
              if(tempTime>WaveformResponseTime){//if new response time is higher than the last store it
                  WaveformResponseTime = tempTime;
              }
              Semaphore_post(SignalProcessing);//signal Processing task
          }
}
void Processing_Task(UArg arg1, UArg arg2){
    IntMasterEnable();

    //initializes kiss library
    static char kiss_fft_cfg_buffer[KISS_FFT_CFG_SIZE];
    size_t buffer_size = KISS_FFT_CFG_SIZE;
    kiss_fft_cfg cfg;
    static kiss_fft_cpx in[NFFT], out[NFFT];
    static float w[NFFT];
    int i;
    cfg = kiss_fft_alloc(NFFT, 0, kiss_fft_cfg_buffer, &buffer_size);
    int offset = 85;
    for (i = 0; i<NFFT; i++){//W is used to smooth out the spectrum waveform so the peaks are rounded rather than pointed
        w[i] = 0.42f - 0.5f * cosf(2 * PI *i/(NFFT-1))+ 0.08f * cosf(4 * PI * i/(NFFT-1));

    }
    while (true) {
        Semaphore_pend(SignalProcessing, BIOS_WAIT_FOREVER);//waits to be signaled
        if(OscilloscopeMode == 0){//if in waveform mode
            int i;
            for(i = 0; i<128; i++){//calculate y coordinate of each point in the buffer
            ADCScaledPrintBuffer[i] = LCD_VERTICAL_MAX/2 - (int)roundf(fScale * (ADCPrintBuffer[i] - ADC_OFFSET));
            }

        }
        else{//if in spectrum mode
           for(i = 0; i < NFFT; i++){//enter waveform values
               in[i].r = RawSpectrumBuffer[i] * 3.3f/4096.f * w[i];
               in[i].i = 0;
           }
           kiss_fft(cfg, in, out);//conduct Discrete fourier transform

           for(i = 0; i <NFFT; i++){
               float signalMagnitude = log10f(sqrtf(out[i].r * out[i].r + out[i].i * out[i].i));//calculate magnitude of frequency domain output. convert to log scale
               ProcessedSpectrumBuffer[i] = offset - (int)roundf(20.f*signalMagnitude);//round values, and apply an offset so they will accurately display on screen. convert to decibel
           }
        }
        Semaphore_post(SignalDisplay);//signal display task
        Semaphore_post(SignalWaveform);//signal waveform task after display task is done
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
