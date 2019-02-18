/*
 * This file is part of the sigrok-firmware-fx2lafw project.
 *
 * Copyright (C) 2009 Ubixum, Inc.
 * Copyright (C) 2015 Jochen Hoenicke
 * Copyright (C) 2018 Marek Wodzinski
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <fx2macros.h>
#include <fx2ints.h>
#include <autovector.h>
#include <delay.h>
#include <setupdat.h>

#define SET_ANALOG_MODE()

#define SET_COUPLING(x) set_coupling(x)

#define SET_CALIBRATION_PULSE(x)

#define TOGGLE_CALIBRATION_PIN()

#define LED_CLEAR() NOP
#define LED_GREEN() NOP
#define LED_RED()   NOP

#define TIMER2_VAL 500

/* CTLx pin index (IFCLK, ADC clock input). */
#define CTL_BIT 0

#define OUT0 ((1 << CTL_BIT) << 4) /* OEx = 1, CTLx = 0 */

static const struct samplerate_info samplerates[] = {
	{ 48, 0x80,   0, 3, 0, 0x00, 0xea },
	{ 30, 0x80,   0, 3, 0, 0x00, 0xaa },
	{ 24,    1,   0, 2, 1, OUT0, 0xca },
	{ 16,    1,   1, 2, 0, OUT0, 0xca },
	{ 12,    2,   1, 2, 0, OUT0, 0xca },
	{  8,    3,   2, 2, 0, OUT0, 0xca },
	{  4,    6,   5, 2, 0, OUT0, 0xca },
	{  2,   12,  11, 2, 0, OUT0, 0xca },
	{  1,   24,  23, 2, 0, OUT0, 0xca },
	{ 50,   48,  47, 2, 0, OUT0, 0xca },
	{ 20,  120, 119, 2, 0, OUT0, 0xca },
	{ 10,  240, 239, 2, 0, OUT0, 0xca },
};

/*
  Update the shift register (74HC595D), connected as:
  PA4: DS (Data)
  PA5: STCP (Storage clock)
  PA6: SHCP (Shift clock)

  Q0: upper (CH 1) mux, S1
  Q1: upper (CH 1) mux, S0
  Q2: lower (CH 2) mux, S0
  Q3: lower (CH 2) mux, S1
  Q4: CH2 low voltage relay
  Q5: CH2 AC/DC relay
  Q6: CH1 low voltage relay
  Q7: CH1 AC/DC relay
 */
static void update_register(BYTE value, BYTE mask) {
  static BYTE register_value = 0;
  int i;
  register_value = (register_value & ~mask) | value;
  PA5 = 0;
  PA6 = 0;
  delay(1);
  for (i = 7; i >= 0; i--) {
    PA4 = (register_value >> i) & 0x01;
    PA6 = 1;
    delay(1);
    PA6 = 0;
    delay(1);
  }
  PA5 = 1;
}

/*
 * This sets three bits for each channel, one channel at a time.
 * For channel 0 we want to set bits 1, 2 & 3
 * For channel 1 we want to set bits 4, 5 & 6
 *
 * We convert the input values that are strange due to original
 * firmware code into the value of the three bits as follows:
 *
 * val    -> bits
 * 1      -> 010b
 * 2      -> 001b
 * 5      -> 000b
 * 10(16) -> 011b
 *
 * The third bit is always zero since there are only four outputs connected
 * from the 74HC4051 chip.
 */
static BOOL set_voltage(BYTE channel, BYTE val)
{
  /* update_register(0x01, 0x03); // reads: 1V@1V -> 1V */
  /* update_register(0x02, 0x03); // reads: 2V@1V -> 500mV */
  /* update_register(0x00, 0x03); // reads: 3V@1V -> 333mV? */
  /* update_register(0x03, 0x03); // reads: 5V@1V -> 200mV */

  static BYTE voltage_bits[][] = {
    // bits controlling channel 1 mux (swapped bits, wrt CH 2):
    { 0x01, 0x02, 0x00, 0x03 },
    // bits controlling channel 2 mux:
    { 0x02, 0x01, 0x00, 0x03 },
  };
  static BYTE channel_voltage_mask[] = { 0x03, 0x0C };
  BYTE bits;
  BYTE mask = channel_voltage_mask[channel];
  BYTE *channel_voltage_bits = voltage_bits[channel];

  switch (val) {
  case 1:
    bits = channel_voltage_bits[0];
    break;
  case 2:
    bits = channel_voltage_bits[1];
    break;
  case 5:
    bits = channel_voltage_bits[2];
    break;
  case 10: /* For backward compatibility. */
  case 16:
    bits = channel_voltage_bits[3];
    break;
  default:
    return FALSE;
  }

  update_register(bits, mask);

  return TRUE;
}

#include <scope.inc>
