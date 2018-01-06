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

const byte NUM_PIN = 2;
const byte data_pins[NUM_PIN] = { 4, 5 };

const unsigned int NUM_LEDS = NUM_PIN*8;

byte leds[NUM_LEDS];

const float M_2PI = 6.28;

class AbstractDriver
{
public:
	virtual bool adjust_leds (unsigned int elapsed) = 0;
};

class DummyDriver : public AbstractDriver
{
public:
	DummyDriver() : _init (false) {}
	bool adjust_leds (unsigned int elapsed);

private:
	bool _init;
};

bool DummyDriver::adjust_leds (unsigned int elapsed)
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


class WaveDriver : public AbstractDriver
{
public:
	WaveDriver (float step, unsigned int time)
		: _step (step),
		  _phi (0),
		  _time (time) {}
	bool adjust_leds (unsigned int elapsed);

private:
	const float _step;
	float _phi;
	const unsigned int _time;
};

bool WaveDriver::adjust_leds (unsigned int elapsed)
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
AbstractDriver* driver;



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

void setup ()
{
	pinMode (clock_pin, OUTPUT);
	pinMode (latch_pin, OUTPUT);

	for (byte i=0; i<NUM_PIN; ++i) {
		pinMode(data_pins[i], OUTPUT);
	}

	reset();

	driver = &wave_driver;

}

void loop ()
{
	static unsigned int last_time = 0;
	static unsigned int now = 0;

	shift_out_frames ();

	now = millis ();

	if (driver->adjust_leds (now-last_time)) {
		last_time = now;
	}
}
