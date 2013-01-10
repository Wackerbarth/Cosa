/**
 * @file CosaCanvasSweep.ino
 * @version 1.0
 *
 * @section License
 * Copyright (C) 2012, Mikael Patel
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
 * Cosa demonstration of Canvas device driver for ST7735R, 262K Color 
 * Single-Chip TFT Controller, and monitoring of an analog pin in
 * EKG style.
 *
 * @section Circuit
 * Use the analog pin(0) as the probe pin.
 *
 * This file is part of the Arduino Che Cosa project.
 */

#include "Cosa/Watchdog.hh"
#include "Cosa/SPI/ST7735R.hh"

// The display and an iostream to the device
ST7735R tft;
AnalogPin probe(0);
uint16_t CANVAS, PEN;

void setup()
{
  // Start the watchdogy for delay
  Watchdog::begin();

  // Initiate the display, setup colors and clear the screen
  tft.begin();
  PEN = tft.shade(Canvas::GREEN, 50);
  CANVAS = tft.shade(Canvas::WHITE, 10);
  tft.set_pen_color(PEN);
  tft.set_text_color(PEN);
  tft.set_canvas_color(CANVAS);
  tft.fill_screen();
  tft.set_orientation(Canvas::LANDSCAPE);
  tft.draw_rect(0, 0, tft.WIDTH - 1, tft.HEIGHT - 1);
}

void loop()
{
  const uint8_t STEP = 8;
  static uint8_t x0 = 0;
  static uint8_t y0 = 0;

  // Sample the probe and calculate the position
  uint16_t sample = probe.sample();
  uint8_t x1 = x0 + STEP;
  uint8_t y1 = tft.HEIGHT - (sample >> 3);

  // Calculate the region to erase before drawing the line
  uint8_t width = STEP * 2;
  if (x0 + 2 + width > tft.WIDTH)
    width = tft.WIDTH - x0 - 2;
  tft.set_pen_color(CANVAS);
  tft.fill_rect(x0 + 1, 1, width, tft.HEIGHT - 2);

  // Draw the line and a simple marker to follow
  tft.set_pen_color(PEN);
  tft.draw_line(x0, y0, x1, y1);
  tft.set_cursor(x1 + 1, y1 - 3);
  tft.draw_char('*');

  // Update position for the next sample
  x0 = x1;
  y0 = y1;

  // Check for wrap-around
  if (x0 >= tft.WIDTH) {
    x0 = 0;
    tft.draw_rect(0, 0, tft.WIDTH - 1, tft.HEIGHT - 1);
  }

  // Wait for the next sample
  Watchdog::delay(128);
}
