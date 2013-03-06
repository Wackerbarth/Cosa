/**
 * @file Cosa/IOStream/Driver/UART.cpp
 * @version 1.0
 *
 * @section License
 * Copyright (C) 2012-2013, Mikael Patel
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA  02111-1307  USA
 *
 * @section Description
 * Basic UART device handler with internal buffering.
 *
 * This file is part of the Arduino Che Cosa project.
 */

#if defined(__AVR_ATtiny25__) \
 || defined(__AVR_ATtiny45__) \
 || defined(__AVR_ATtiny85__)

#include "Cosa/IOStream/Driver/UART.hh"

bool
UART::begin(uint32_t baudrate, uint8_t format)
{
  m_period = 1000000L / baudrate;
  m_format = format;
  return (1);
}

int 
UART::putchar(char c)
{
  int res = (c & 0xff);
  uint8_t bits = m_format & DATA_MASK;
  synchronized {
    m_pin.write(0);
    DELAY(m_period);
    for (uint8_t i = 0; i < bits; i++) {
      m_pin.write(c & 1);
      DELAY(m_period);
      c >>= 1;
    }
    m_pin.write(1);
  }
  DELAY(m_period * 32);
  return (res);
}

UART uart(Board::D0) __attribute__ ((weak));

#else

#include "Cosa/Bits.h"
#include "Cosa/IOStream/Driver/UART.hh"
#include <avr/sleep.h>

static char ibuffer[UART::BUFFER_MAX];
static IOBuffer ibuf(sizeof(ibuffer), ibuffer);

static char obuffer[UART::BUFFER_MAX];
static IOBuffer obuf(sizeof(obuffer), obuffer);

UART uart(0, &ibuf, &obuf);

bool
UART::begin(uint32_t baudrate, uint8_t format)
{
  uint16_t setting = (F_CPU / (baudrate * 8L)) - 1;

  // Check if double rate is not possible
  if (setting > 4095) {
    setting = (F_CPU / (baudrate * 16L)) - 1;
  } else {
    *UCSRnA() = _BV(U2X0);
  }

  // Set baudrate
  *UBRRn() = setting;

  // Enable transmitter interrupt
  *UCSRnB() = (_BV(RXCIE0) | _BV(RXEN0) | _BV(TXEN0));
    
  // Set frame format: asynchronous, 8data, 2stop bit
  *UCSRnC() = format;
  return (1);
}

bool 
UART::end()
{
  // Disable transmitter interrupt
  *UCSRnB() &= ~(_BV(RXCIE0) | _BV(RXEN0) | _BV(TXEN0));
  m_obuf->flush();
  m_ibuf->flush();
  return (1);
}

int 
UART::putchar(char c)
{
  uint8_t mode = SLEEP_MODE_IDLE;
  while (m_obuf->putchar(c) == -1) {
    cli();
    set_sleep_mode(mode);
    sleep_enable();
    sei();
    sleep_cpu();
    sleep_disable();
  }
  *UCSRnB() |= _BV(UDRIE0);
  return (c & 0xff);
}

ISR(USART_UDRE_vect)
{
  int c = uart.m_obuf->getchar();
  if (c != -1) {
    UDR0 = c;
  }
  else {
    bit_clear(UCSR0B, UDRIE0);
  }
}

ISR(USART_RX_vect)
{
  uart.m_ibuf->putchar(UDR0);
}

#if defined(__AVR_ATmega1284P__)

UART* uart1 = 0;

ISR(USART1_UDRE_vect)
{
  if (uart1 == 0) return;
  int c = uart1->m_obuf->getchar();
  if (c != -1) {
    UDR1 = c;
  }
  else {
    bit_clear(UCSR1B, UDRIE0);
  }
}

ISR(USART1_RX_vect)
{
  if (uart1 == 0) return;
  uart1->m_ibuf->putchar(UDR1);
}
#endif

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)

// Fix: Add Arduino Mega UART interrupt handlers

#endif
#endif
