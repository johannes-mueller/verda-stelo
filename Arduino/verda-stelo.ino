/*
    Copyright (C) 2018 Johannes Mueller <github@johannes-mueller.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/


const byte clock_pin = 2;
const byte latch_pin = 3;

const byte button_pin = 12;

const byte NUM_PIN = 2;
const byte data_pins[NUM_PIN] = { 4, 5 };

const unsigned int NUM_LEDS = NUM_PIN*8;

byte leds[NUM_LEDS];

const unsigned long long_press = 3000;

const float M_2PI = 6.28;

const unsigned long change_time = 5000;

class AbstractDriver
{
public:
	virtual bool adjust_leds (unsigned long elapsed) = 0;
	virtual void reset () {}
};

class DummyDriver : public AbstractDriver
{
public:
	DummyDriver() : _init (false) {}
	bool adjust_leds (unsigned long elapsed);
	void reset ();

private:
	bool _init;
};

bool DummyDriver::adjust_leds (unsigned long elapsed)
{
	if (elapsed > 5000) {
		leds[5] = 127;
		return true;
	}

	if (_init) {
		return false;
	}

	_init = true;

	leds[1] = 8;
	leds[2] = 127;
	leds[3] = 16;
	leds[4] = 1;

	leds[10] = 1;
	leds[11] = 127;
	leds[12] = 12;
	leds[15] = 127;
	leds[14] = 32;

	return true;
};

void DummyDriver::reset ()
{
	_init = false;
}

class BlinkDriver : public AbstractDriver
{
public:
	BlinkDriver () : _on (false) {}
	bool adjust_leds (unsigned long elapsed);

private:
	const static unsigned int _blink_time = 500;
	bool _on;
};

bool BlinkDriver::adjust_leds (unsigned long elapsed)
{
	if (elapsed < _blink_time) {
		return false;
	}

	_on = !_on;
	const byte v = _on ? 127 : 0;
	for (unsigned int i=0; i<NUM_LEDS; ++i) {
		leds[i] = v;
	}

	return true;
};

class WaveDriver : public AbstractDriver
{
public:
	WaveDriver (float step, unsigned long time)
		: _step (step),
		  _phi (0),
		  _time (time) {}
	bool adjust_leds (unsigned long elapsed);

private:
	const float _step;
	float _phi;
	const unsigned long _time;
};

bool WaveDriver::adjust_leds (unsigned long elapsed)
{
	if (elapsed < _time) {
		return false;
	}

	for (unsigned int i=0; i<NUM_LEDS; ++i) {
		float s = sin ((float)i*M_2PI/NUM_LEDS + _phi);
		leds[i] = 127 - (unsigned int) round (s*s * 127);
		_phi -= _step;
		if (_phi > 2*M_2PI) {
			_phi -= 2*M_2PI;
		}
	}

	return true;
}

DummyDriver dummy_driver;
WaveDriver wave_driver (0.002, 5);
BlinkDriver blink_driver;

AbstractDriver* driver;

const byte DRIVER_NUM = 2;
AbstractDriver* const driver_list[DRIVER_NUM] = {
	&dummy_driver,
	&wave_driver
};

byte driver_index = 0;

unsigned long driver_time = 0;

void reset ()
{
	for (byte i=0; i<NUM_LEDS; ++i) {
		leds[i] = 0;
	}
}

void set_high (byte pin)
{
	bitSet(PORTD, pin);
}

void set_low (byte pin)
{
	bitClear(PORTD, pin);
}

void shift_out_subframe (byte led_buf[NUM_LEDS])
{
	for (unsigned int i=0; i<8; ++i) {
		for (unsigned int s=0; s<NUM_PIN; ++s) {
			const unsigned int index = s*8 + i;
			if (led_buf[index]) {
				set_high(data_pins[s]);
				--led_buf[index];
			} else {
				set_low(data_pins[s]);
			}
		}

		set_high(clock_pin);
		set_low(clock_pin);
	}

	set_low(latch_pin);
	set_high(latch_pin);
}

void shift_out_frames ()
{
	byte led_buf[NUM_LEDS];
	for (unsigned int i=0; i<NUM_LEDS; ++i) {
		led_buf[i] = leds[i];
	}

	for (byte i=0; i<127; ++i) {
		shift_out_subframe (led_buf);
	}
}

void increment_driver ()
{
	reset ();
	if (++driver_index == DRIVER_NUM) {
		driver_index = 0;
	}
	driver = driver_list[driver_index];
	driver->reset();
}

void handle_long_press ()
{
	driver_time = change_time;
	increment_driver ();
}

bool check_button ()
{
	static bool pressed = false;
	static unsigned long last_press = 0;

	//	bool now_pressed = bitRead (PORTB, button_pin - 8);

	bool now_pressed = digitalRead (button_pin);

	if (now_pressed && !pressed) {
		last_press = millis ();
		driver_time = 0;
		pressed = true;
		return false;
	}

	if (now_pressed && pressed) {
		if (millis () - last_press > long_press) {
			driver = &blink_driver;
		}
		return false;
	}

	if (!now_pressed && pressed) {
		driver = &blink_driver;
		if (millis () - last_press > long_press) {
			handle_long_press ();
		} else {
			increment_driver ();
			driver_time = 0;
		}
		pressed = false;
		return true;
	}

	return false;
}

void setup ()
{
	pinMode (clock_pin, OUTPUT);
	pinMode (latch_pin, OUTPUT);
	pinMode (button_pin, INPUT);
	pinMode (13, OUTPUT);

	for (byte i=0; i<NUM_PIN; ++i) {
		pinMode(data_pins[i], OUTPUT);
	}

	reset();

	driver = driver_list[0];
	driver_index = 0;
}

void loop ()
{
	static unsigned long last_time = 0;
	static unsigned long last_change = 0;
	static unsigned long now = 0;

	shift_out_frames ();

	now = millis ();

	if (check_button ()) {
		last_time = now;
		last_change = now;
	}

	if (driver_time && (now-last_change > driver_time)) {
		increment_driver ();
		last_change = now;
		last_time = now;
	}

	if (driver->adjust_leds (now-last_time)) {
		last_time = now;
	}
}
