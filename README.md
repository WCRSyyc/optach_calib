# Optical Tachometer Calibration

Calibration and testing of optical tachometer

This logic for this sketch is variation on the BlinkWithoutDelay example.  It uses the micros() timer values to control the on and off time intervals for an LED.  It is setup to counter drift from the amount of time that the code takes to loop.  If the code delays a scheduled transition, the following events are still scheduled relative to when the transition should have occurred.  This means that the time between transitions can be either less or greater than configured, but that overall the number of transitions should match what is expected.  This should be as accurate as the µController clock frequency.

The direct way to test and calibrate the tachometer, would be to use a shaft with a known speed.  That is not convenient to access.  Instead, this code is used to `blink` the IR LED in the QRD1114 Reflective Object Sensor, at a known frequency.  As long as a stationary reflective surface is provided, that will produce the same pulse train as a reflective spot on a spinning shaft.  With much easier control over the frequency and width of the pulses.

```tx
Pulse frequency hZ = 1 sec / interval (sec) = 1000*1000 µS/Sec / interval µS
RPM (with single pulse per revolution) = frequency hZ * 60 Sec/min
RPM = frequency hZ * 60 Sec/min / pulses_per_revolution)
```

An interval of 60000 µSec, and 6000 µSec pulse width simulates a 1000 RPM shaft with a single reflective point covering 10% of its circumference.   Testing (on an oscilloscope) shows that pulse width as low as 10 µSec trigger the detector.  The optical sensor side does not turn on as fast as the LED.  There is a distinct ramp up time.  10µS seems to be enough to get most of the way to full current for the amount of reflected light.

The circuit:

- The basic LED blink circuit: An LED connected from the specified digital pin to a current limiting resistor, which connects to ground.   This may work with a normal LED, but using the LED from the QRD1114 is more appropriate for what is being tested.

The QRD1114 datasheet shows most measurements for the IR emitter diode at 20ma.  That is the maximum current that can be supplied by a single digital pin on most Arduino boards.  If powering from VCC, that is not a problem.  However, the tachometer circuit was designed to use digital pin for power and ground sources to simplify the physical connection.  Pin 2 is required on an UNO for interrupt inputs, so adjacent pins 3 and 4 were used for power and ground, to allow
connecting the sensor with a simple 3 pin header.  The same limitation applies to the test and calibration circuit.  An arduino pin is being used to drive the IR LED.  With a 5V Arduino board, and the [datasheet specifications](https://www.sparkfun.com/datasheets/BOT/QRD1114.pdf), a 165 Ohm resistor would be needed to get 20ma.  That is pushing the Arduino pin to its limit.  Using 560 Ohm cuts the current to a much safer 6ma.  That is sufficient, at least for the easier to control environment used for the testing.  Higher current does make the LED brighter, which means reflections can be detected from further away.  The datasheet information is based on a good reflective white surface 0.05" (1.27mm) from the sensor face.  Fairly tight for something that is going to need to be manually positioned when in use.  When reading information from the datasheet, remember the the V<sub>CE</sub> is shown as 5 volts, but in the Arduino circuit, it is going to be less than that.  Even with a VCC of 5V, the needed sensor/current limiting resistor is going to reduce that significantly.

IDEA: Instead of trying to read the reflection pulses directly from the QRD1114 sensor, connect the sensor C and E between VCC and the base of an NPN transistor.  That will get about 4.4 V<sub>CE</sub>, and I<sub>BE</sub> of the NPN BJT into the range of about .3 to 1ma.  That should make the amplified signal at the collector much easier to read.
