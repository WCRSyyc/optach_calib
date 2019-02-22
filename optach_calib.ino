/*
  Optical Tachometer Calibration

  Turn on and off a light emitting diode (LED) connected to a digital pin.
  The on time (high pulse width) and frequency are controllable using commands
  from the serial port.  This is intended to be used to verify the functionality,
  and possible limitations of the optical tachometer circuit and code.

  The LED being driven here is (a substitute for) the IR LED in the  QRD1114
  Reflective Object Sensor used in the tachometer. Instead of attempting to
  provide known frequency reflections for calibration, this is to be setup to
  always reflect, and control when the LED is on instead.  LED on is equivalent
  to reflecting the light, LED off is equivalent to not reflecting.

  The circuit:
  - The basic LED blink circuit: An LED connected from the specified digital
    pin to a current limiting resistor, which connects to ground.  This may
    work with a normal LED, but using the LED from the QRD1114 is more
    appropriate for what is being tested.

  Created 2019
  by H. Phil Duby

  This code is copyright with the MIT license.

  Pulse frequency hZ = 1 sec / interval (sec) = 1000*1000 µS/Sec / interval µS
  RPM (with single pulse per revolution) = frequency hZ * 60 Sec/min
  RPM = frequency hZ * 60 Sec/min / pulses_per_revolution)

  QRD1114 datasheet shows most measurements for IR emitter diode at 20ma.  That
  is the maximum current that can be supplied by a single digital pin on most
  Arduino boards.  If powering from VCC, not a problem.  However, the tachometer
  circuit was designed to use digital pin for power and ground sources, to
  simplify the physical connection.  Pin 2 is required on an UNO for interrupt
  inputs, so adjecnt pins 3 and 4 were used for power and ground, to allow
  connecting the sensor with a simple 3 pin header.
*/

// 300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 57600, 115200
unsigned const long SERIAL_SPEED = 115200;
const uint8_t ledPin = 7;  // best NOT to use LED_BUILTIN
// const uint8_t ledPin = LED_BUILTIN;  // for visual DEBUG and testing
unsigned const long syncDelay = 1000; // µSec to delay first transition after change
unsigned const int CMD_BUF_SIZE = 60; // bytes in buffer

unsigned long nextHigh;  // will store next time LED should be turned on (millis)
unsigned long nextLow;  // will store next time LED should be turned off (millis)
unsigned long pulseWidth = 6000;  // Initial time to keep the LED on
unsigned long pulseInterval = 60000;  // Initial interval between pulse starts
// 60000 & 6000 is 1000 RPM@10% duty cycle
unsigned char commandBuf[CMD_BUF_SIZE];  // Buffer to collect command from serial port
unsigned int bufIndex = 0;  // Position to store the next input character

void setup() {
    Serial.begin(SERIAL_SPEED, SERIAL_8N1);
    while (!Serial) {
        ; // wait for serial port to connect. Needed for native USB port only
    }
    Serial.println(F("Tachometer Calibration"));
    pinMode(ledPin, OUTPUT);
    pinMode(2, INPUT);
    pinMode(3, OUTPUT);
    pinMode(4, OUTPUT);
    digitalWrite(ledPin, LOW);
    digitalWrite(3, LOW);
    digitalWrite(4, HIGH);
    setPulsePattern(0, 0); // Setup to use hardcoded timings
    // setPulsePattern(100, 50);  // DEBUG for scope check
}

void loop() {
    unsigned long now = micros();
    if (now >= nextHigh) {
        digitalWrite(ledPin, HIGH);
        nextHigh += pulseInterval;
        return;
    }
    if (now < nextLow) {
        return;
    }
    digitalWrite(ledPin, LOW);
    nextLow += pulseInterval;
    checkSerialCommand();
}

void checkSerialCommand() {
    bool have_serial, have_cmd, have_overflow;
    // unsigned static int chars = 0; // DEBUG
    // unsigned long start_serial, end_serial; // DEBUG

    // To increase timing stability, only check the serial port *just* after a
    // high to low transition.  Normally that will be the start of the longest
    // time where the code is just waiting.
    have_serial = false;
    have_overflow = false;
    have_cmd = false;
    // start_serial = micros(); // DEBUG
    while (Serial.available()) {
        have_serial = true;
        commandBuf[bufIndex++] = Serial.read();
        if (commandBuf[bufIndex-1] == '\n') {
            have_cmd = true; // a complete command line is in the buffer
        }
        if (bufIndex >= CMD_BUF_SIZE) {
            have_overflow = true; // buffer overflow: throw it all away
            break;
        }
    }
    // end_serial = micros(); // DEBUG
    if (have_serial) {
        // report_serial(end_serial - start_serial, chars, have_cmd, have_overflow); // DEBUG
        if (have_cmd) {
            processCommand(bufIndex);
        }
        if (have_cmd or have_overflow) {
            bufIndex = 0;
        }
        // chars = bufIndex; // DEBUG
    }
} // ./void checkSerialCommand()

void processCommand(const unsigned int len) {
    unsigned int wIdx, cIdx;
    unsigned long cmdParam, interval, pulse;

    if (badLen(len)) { return; }
    interval = 0;
    pulse = 0;
    cIdx = indexAfterLeading(commandBuf, 0);
    while (cIdx < len - 1 and commandBuf[cIdx] != '\0') {
        wIdx = parseULong(commandBuf, cIdx + 1, len, &cmdParam);

        switch(commandBuf[cIdx]) {
            case 'i':  // interval (microseconds) between pulses (LOW to HIGH transitions)
                interval = cmdParam;
                break;
            case 'p': // pulse width (microseconds) (HIGH interval)
                pulse = cmdParam;
                break;
            default:
                Serial.print(F("Invalid command \""));
                Serial.print((char)commandBuf[cIdx]);
                Serial.println('"');
                return;
        }
        cIdx = indexAfterLeading(commandBuf, wIdx);
    }
    setPulsePattern(interval, pulse);
}

void setPulsePattern(const unsigned long pInterval, const unsigned long pWidth) {
    unsigned long interval = pulseInterval;
    unsigned long width = pulseWidth;
    if (pInterval > 0) {
        interval = pInterval;
    }
    if (pWidth > 0) {
        width = pWidth;
    }
    if (interval < width) {
        Serial.print(F("Pulse width "));
        Serial.print(width);
        Serial.print(F(" is too long for an interval of "));
        Serial.print(interval);
        Serial.print(F(" microseconds"));
        return;
    }
    pulseWidth = width;  // microseconds to keep LED on
    pulseInterval = interval;  // new interval between pulse starts
    // Show the pulse interval and width settings
    Serial.print(F("Blink timing currently: Interval = "));
    Serial.print(pulseInterval);
    Serial.print(F(", pulse width = "));
    Serial.println(pulseWidth);

    // force pulse cycle [re]start a bit in the future
    nextHigh = syncDelay + micros();
    nextLow = nextHigh + pulseWidth;
} // ./void setPulsePattern()

bool badLen(const unsigned int len) {
    // Sanity check error trap
    if (len <= CMD_BUF_SIZE) {
        return false;
    }
    Serial.print(F("Can not process "));
    Serial.print(len, DEC);
    Serial.print(F(" bytes from a "));
    Serial.print(CMD_BUF_SIZE, DEC);
    Serial.println(F(" byte buffer"));
    return true; // Error was detected and reported
} // ./ bool badLen()

unsigned int cloneBuf(const unsigned char * src, unsigned char * dest, const unsigned int len) {
    unsigned int wLen;
    unsigned char endChar;
    for (unsigned int i = 0; i < len; i++) {
        dest[i] = src[i];
    }
    dest[len] = '\0'; // null terminate

    // remove any trailing delimiters
    wLen = len - 1;
    endChar = dest[wLen];
    while (endChar == '\n' or endChar == '\r' or endChar == ' ' or endChar == ',') {
        dest[wLen] = '\0';
        if (wLen == 0) {
            Serial.println(F("Nothing in the buffer to process"));
            return 0;
        }
        endChar = dest[--wLen];
    }
    return ++wLen;
} // ./ unsigned int cloneBuf()

unsigned int indexAfterLeading(const unsigned char * buf, const unsigned int start) {
    unsigned long idx = start;
    // Possible buffer overflow: This code does not know how long the input buffer
    // is, so if the buffer is completely full of leading markers, this will attempt
    // to access memory beyond the end … and return an index beyond the end too.
    while (buf[idx] == ',' or buf[idx] == ' ' or buf[idx] == '\t') {
        idx++;
    }
    return idx;
} // ./unsigned int indexAfterLeading()

unsigned int parseULong(const unsigned char *buf, const unsigned int sIdx, const unsigned int eIdx, unsigned long *dest) {
    unsigned int idx = sIdx;

    idx = sIdx;
    while (idx < eIdx and isdigit(buf[idx])) {
        idx++; // locate terminating delimiter
    }
    *dest = atol((char *)&buf[sIdx]);
    return idx; // index to the first unprocessed character of the buffer
} // ./unsigned int parseULong()

void report_serial(const unsigned long delta_micros, const unsigned int prv_idx,
        const bool eol, const bool overflow) {
    Serial.print(F("State: chars = "));
    Serial.print(prv_idx);
    Serial.print(F(", idx = "));
    Serial.print(bufIndex);
    Serial.print(F(", have_cmd = "));
    Serial.print(eol);
    Serial.print(F(", overflow = "));
    Serial.print(overflow);

    Serial.print(F(": reading "));
    Serial.print(bufIndex - prv_idx);
    Serial.print(F(" bytes took "));
    Serial.print(delta_micros);
    Serial.println(F("µSec"));

    if (eol) {
        Serial.println(F("have command"));
    }
    if (overflow) {
        Serial.println(F("overflow"));
    }
}
