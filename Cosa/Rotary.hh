/**
 * @file Rotary.hh
 * @version 1.0
 *
 * @section License
 * Copyright (C) 2013, Mikael Patel (Cosa port and adaptation)
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
 * This file is part of the Arduino Che Cosa project.
 */

#ifndef __COSA_ROTARY_HH__
#define __COSA_ROTARY_HH__

#include "Cosa/Types.h"
#include "Cosa/InterruptPin.hh"

/**
 * Copyright 2011 Ben Buxton. Licenced under the GNU GPL Version 3.
 * Contact: bb@cactii.net
 *
 * A typical mechanical rotary encoder emits a two bit gray code
 * on 3 output pins. Every step in the output (often accompanied
 * by a physical 'click') generates a specific sequence of output
 * codes on the pins.
 *
 * There are 3 pins used for the rotary encoding - one common and
 * two 'bit' pins.
 *
 * The following is the typical sequence of code on the output when
 * moving from one step to the next:
 *
 *   Position   Bit1   Bit2
 *   ----------------------
 *     Step1     0      0
 *      1/4      1      0
 *      1/2      1      1
 *      3/4      0      1
 *     Step2     0      0
 *
 * From this table, we can see that when moving from one 'click' to
 * the next, there are 4 changes in the output code.
 *
 * - From an initial 0 - 0, Bit1 goes high, Bit0 stays low.
 * - Then both bits are high, halfway through the step.
 * - Then Bit1 goes low, but Bit2 stays high.
 * - Finally at the end of the step, both bits return to 0.
 *
 * Detecting the direction is easy - the table simply goes in the other
 * direction (read up instead of down).
 *
 * To decode this, we use a simple state machine. Every time the output
 * code changes, it follows state, until finally a full steps worth of
 * code is received (in the correct order). At the final 0-0, it returns
 * a value indicating a step in one direction or the other.
 *
 * It's also possible to use 'half-step' mode. This just emits an event
 * at both the 0-0 and 1-1 positions. This might be useful for some
 * encoders where you want to detect all positions.
 *
 * If an invalid state happens (for example we go from '0-1' straight
 * to '1-0'), the state machine resets to the start until 0-0 and the
 * next valid codes occur.
 *
 * The biggest advantage of using a state machine over other algorithms
 * is that this has inherent debounce built in. Other algorithms emit 
 * spurious output with switch bounce, but this one will simply flip 
 * between sub-states until the bounce settles, then continue along the 
 * state machine.
 *
 * A side effect of debounce is that fast rotations can cause steps to
 * be skipped. By not requiring debounce, fast rotations can be accurately
 * measured.
 *
 * Another advantage is the ability to properly handle bad state, such
 * as due to EMI, etc. It is also a lot simpler than others - a static
 * state table and less than 10 lines of logic.
 *
 * @see also
 * http://www.buxtronix.net/2011/10/rotary-encoders-done-properly.html
 */
class Rotary {
public:
  /**
   * Rotary Encoder.
   */
  class Encoder : public Event::Handler {
  public:
    /**
     * Rotary Encoder turn direction
     */
    enum Direction {
      NONE = 0x00,		// No direction change
      CW = 0x10,		// Clock-wise direction
      CCW = 0x20		// Anti-clock-wise direction
    } __attribute__((packed));
    
    /**
     * Rotary Encoder cycle mode
     */
    enum Mode {
      HALF_CYCLE,
      FULL_CYCLE
    }  __attribute__((packed));

  protected:
    /**
     * Rotary signal pin handler. Delegates to Rotary Encoder to process 
     * new state.
     */
    class SignalPin : public InterruptPin {
    private:
      Encoder* m_encoder;

      /**
       * Signal pin interrupt handler. Check possible state change and will
       * push Event::CHANGE_TYPE with direction (CW or CCW).
       */
      virtual void on_interrupt(uint16_t arg);
      
    public:
      SignalPin(Board::InterruptPin pin, Encoder* encoder) : 
	InterruptPin(pin), 
	m_encoder(encoder)
      {}
    };

    // Signal pins, previous state and stepping mode.
    SignalPin m_clk;
    SignalPin m_dt;
    uint8_t m_state;
    Mode m_mode;
    
    /**
     * Detect Rotary Encoder state change. Reads current input pin
     * values and performs a possible state change. Return turn
     * direction or none. 
     * @return direction
     */
    Direction detect();
    
  public:
    /**
     * Create Rotary Encoder with given interrupt pins. Setup must
     * call InterruptPin::begin() to initiate handling of pins.
     * @param[in] clk pin.
     * @param[in] dt pin.
     * @param[in] mode cycle.
     */
    Encoder(Board::InterruptPin clk, Board::InterruptPin dt, 
	    Mode mode = FULL_CYCLE) :
      m_clk(clk, this),
      m_dt(dt, this),
      m_state(0),
      m_mode(mode)
    {
      m_clk.enable();
      m_dt.enable();
    }

    /**
     * Get current cycle mode.
     * @return mode.
     */
    Mode get_mode()
    {
      return (m_mode);
    }
    
    /**
     * Set cycle mode.
     * @param[in] mode cycle.
     */
    void set_mode(Mode mode)
    {
      m_mode = mode;
    }

  };

  /**
   * Use Rotary Encoder as a simple dial (integer value). Allows a
   * dial within a given number(T) range (min, max) and a given
   * initial value. 
   * @param[in] T value type.
   */
  template<typename T>
  class Dial : public Encoder {
  private:
    T m_value;
    T m_min;
    T m_max;
    T m_step;

    /**
     * @override
     * Update the dial value on change. The event value is the
     * direction (CW or CCW).
     * @param[in] type the event type.
     * @param[in] value the event value.
     */
    virtual void on_event(uint8_t type, uint16_t value)
    {
      if (value == CW) {
	if (m_value == m_max) return;
	m_value += m_step;
      }
      else {
	if (m_value == m_min) return;
	m_value -= m_step;
      }
      on_change(m_value);
    }

  public:
    /**
     * Construct Rotary Dial connected to given interrupt pins with given
     * min, max and initial value.
     * @param[in] clk interrupt pin.
     * @param[in] dt interrupt pin.
     * @param[in] mode step.
     * @param[in] initial value.
     * @param[in] min value.
     * @param[in] max value.
     * @param[in] step value.
     */
    Dial(Board::InterruptPin clk, Board::InterruptPin dt, Mode mode, 
	 T initial, T min, T max, T step) :
      Encoder(clk, dt, mode),
      m_value(initial),
      m_min(min),
      m_max(max),
      m_step(step)
    {}
    
    /**
     * Return current dial value.
     * @return value.
     */
    T get_value()
    {
      return (m_value);
    }
    
    /**
     * Get current step (increment/decrement).
     * @return step.
     */
    T get_step()
    {
      return (m_step);
    }
    
    /**
     * Set step (increment/decrement).
     * @param[in] step value.
     */
    void set_cycle(T step)
    {
      m_step = step;
    }

    /**
     * @override
     * Default on change function. 
     * @param[in] value.
     */
    virtual void on_change(T value) {}
  };
};
#endif
