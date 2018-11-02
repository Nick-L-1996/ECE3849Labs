/*
 * OscilloscopeADC.h
 *
 *  Created on: Oct 25, 2018
 *      Author: Nicholas Lanotte
 *
 *  Read in ADC for Oscilloscope
 */

#ifndef OSCILLOSCOPEADC_H_
#define OSCILLOSCOPEADC_H_

#include <stdint.h>

#define ADC_SAMPLING_RATE 1000000   // [samples/sec] desired ADC sampling rate
#define CRYSTAL_FREQUENCY 25000000  // [Hz] crystal oscillator frequency used to calculate clock rates

void initADC(void);

void GetWaveform(int Direction, uint16_t Voltage);
void changeADCSampleRate(int state);

#endif /* OSCILLOSCOPEADC_H_ */
