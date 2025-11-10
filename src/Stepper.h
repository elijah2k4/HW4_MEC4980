
// Stepper.h
// Header-only Stepper motor helper for Arduino/PlatformIO
// Put this file at: src/Stepper.h



#ifndef STEPPER_H
#define STEPPER_H

#include <Arduino.h>

class Stepper {
public:
    // Constructor
    Stepper(uint8_t stepPin, uint8_t dirPin,
            uint8_t minSwitchPin, uint8_t maxSwitchPin = 0, uint8_t enablePin = 0,
            bool switchesActiveLow = true)
        : _enablePin(enablePin), _stepPin(stepPin), _dirPin(dirPin),
          _minPin(minSwitchPin), _maxPin(maxSwitchPin),
          _switchActiveLow(switchesActiveLow)
    {
        _position = 0;
        _target = 0;
        _enabled = false;
        _direction = false; // true = forward / increasing position
        _velocity = 0.0f;
        _moveSpeed = 900.0f; // default steps/sec for moveTo/home
        _lastStepMicros = 0;
        _stepPulseMicros = 1;
        _autoMove = false;
    }

    // Call in setup()
    void begin() {
        if (_enablePin > 0) {pinMode(_enablePin, OUTPUT);}
        if (_maxPin > 0) {pinMode(_maxPin, INPUT_PULLUP);}
        pinMode(_stepPin, OUTPUT);
        pinMode(_dirPin, OUTPUT);
        pinMode(_minPin, INPUT_PULLUP);
        disable();
        digitalWrite(_stepPin, LOW);
    }

    // Enable / disable driver (active LOW assumed)
    void enable() {
        _enabled = true;
        digitalWrite(_enablePin, LOW);
    }
    void disable() {
        _enabled = false;
        digitalWrite(_enablePin, HIGH);
    }
    bool isEnabled() const { return _enabled; }

    // Single step (blocking pulse). Respects direction and limit switches.
    // Returns true if a step was taken, false if prevented by limit or disabled.
    bool stepOnce() {
        if (!_enabled) return false;
        if (!_canStepInDirection(_direction)) return false;
        digitalWrite(_dirPin, _direction ? HIGH : LOW);
        _pulseStep();
        _position += _direction ? 1 : -1;
        return true;
    }

    // Set direction: true = forward/increasing, false = reverse/decreasing
    void setDirection(bool dir) { _direction = dir; digitalWrite(_dirPin, dir ? HIGH : LOW); }
    bool getDirection() const { return _direction; }

    // Continuous velocity in steps/sec (signed). Call update() frequently.
    // Positive = forward, Negative = reverse. 0 stops continuous motion.
    void setVelocity(float stepsPerSec) {
        _velocity = stepsPerSec;
        _autoMove = false; // manual velocity overrides automatic moveTo state
    }
    float getVelocity() const { return _velocity; }

    // Set speed (positive steps/sec) used by moveTo/home operations
    void setMoveSpeed(float stepsPerSec) { _moveSpeed = fabs(stepsPerSec); }
    float getMoveSpeed() const { return _moveSpeed; }

    // Set step pulse width (microseconds)
    void setStepPulseWidth(unsigned int microsPulse) { _stepPulseMicros = microsPulse; }

    // Non-blocking moveTo: sets a target position and update() will drive towards it.
    // Requires setMoveSpeed() (or keep default).
    void moveTo(long position) {
        _target = position;
        if (_target == _position) { _autoMove = false; _velocity = 0.0f; return; }
        _autoMove = true;
        _velocity = (_target > _position) ? fabs(_moveSpeed) : -fabs(_moveSpeed);
    }
    long getTarget() const { return _target; }

    // Blocking convenience: move to position and wait (with optional timeout ms).
    // Returns true if reached, false if timed out or prevented by limit switch.
    bool moveToBlocking(long position, unsigned long timeoutMs = 0) {
        moveTo(position);
        unsigned long start = millis();
        while (_autoMove) {
            update();
            if (timeoutMs && (millis() - start) >= timeoutMs) { stop(); return false; }
        }
        return (_position == position);
    }

    // Home using minimum switch (blocking). When pressed, position is set to 0.
    // Moves slowly using moveSpeed. Returns true if homed, false if timed out.
    bool homeBlocking(unsigned long timeoutMs = 0) {
        if (!_enabled) enable();
        unsigned long start = millis();
        // Step toward min switch (assume min is reverse direction)
        _velocity = -fabs(_moveSpeed);
        _autoMove = false; // use continuous velocity mode for blocking homing
        while (!minPressed()) {
            update();
            if (timeoutMs && (millis() - start) >= timeoutMs) { stop(); return false; }
        }
        stop();
        _position = 0;
        _target = _position;
        return true;
    }

    // Go to maximum switch (blocking). When pressed, position is set to provided pos (optional).
    bool gotoMaxBlocking(long setPositionWhenMax = 0, unsigned long timeoutMs = 0) {
        if (!_enabled) enable();
        unsigned long start = millis();
        _velocity = fabs(_moveSpeed);
        _autoMove = false;
        while (!maxPressed()) {
            update();
            if (timeoutMs && (millis() - start) >= timeoutMs) { stop(); return false; }
        }
        stop();
        _position = setPositionWhenMax;
        _target = _position;
        return true;
    }

    // Stop any motion
    void stop() {
        _velocity = 0.0f;
        _autoMove = false;
    }

    // Call frequently from loop(). Pass micros() if available to reduce calls.
    void update(unsigned long nowMicros = 0) {
        if (!_enabled) return;
        if (nowMicros == 0) nowMicros = micros();

        // If automatic moveTo active, ensure velocity points toward target
        if (_autoMove) {
            if (_position == _target) { stop(); return; }
            _velocity = (_target > _position) ? fabs(_moveSpeed) : -fabs(_moveSpeed);
        }

        if (fabs(_velocity) < 1e-6) return; // no motion

        bool movingForward = _velocity > 0;
        if (!_canStepInDirection(movingForward)) {
            // if blocked, zero velocity and cancel auto move
            stop();
            return;
        }

        unsigned long interval = (unsigned long)(1000000.0f / fabs(_velocity));
        if (_lastStepMicros == 0) _lastStepMicros = nowMicros;
        if ((nowMicros - _lastStepMicros) >= interval) {
            // perform one step
            digitalWrite(_dirPin, movingForward ? HIGH : LOW);
            _pulseStep();
            _position += movingForward ? 1 : -1;
            _lastStepMicros = nowMicros;
            // if autoMove and reached target, stop
            if (_autoMove && _position == _target) {
                stop();
            }
            // if hit limit after stepping, stop and adjust
            if (minPressed() && _position < 0) _position = 0;
            // (user may want different behavior upon hitting limit - adjust as needed)
        }
    }

    // Position access
    long getPosition() const { return _position; }
    void setPosition(long pos) { _position = pos; }

    // Switch states
    bool minPressed() const {
        bool val = digitalRead(_minPin);
        return _switchActiveLow ? (val == LOW) : (val == HIGH);
    }
    bool maxPressed() const {
        bool val = digitalRead(_maxPin);
        return _switchActiveLow ? (val == LOW) : (val == HIGH);
    }

private:
    uint8_t _enablePin, _stepPin, _dirPin, _minPin, _maxPin;
    bool _switchActiveLow;
    volatile long _position;
    volatile long _target;
    bool _enabled;
    bool _direction;
    float _velocity;     // signed steps/sec for continuous movement
    float _moveSpeed;    // speed used for moveTo/home (positive)
    bool _autoMove;      // true if moveTo is driving motion
    unsigned long _lastStepMicros;
    unsigned int _stepPulseMicros;

    // Pulse the step pin (blocking small delay)
    void _pulseStep() {
        digitalWrite(_stepPin, HIGH);
        delayMicroseconds(_stepPulseMicros);
        digitalWrite(_stepPin, LOW);
    }

    // Check if it's allowed to step in requested direction given limit switches
    bool _canStepInDirection(bool forward) const {
        if (forward && maxPressed()) return false;
        if (!forward && minPressed()) return false;
        return true;
    }
};

#endif // STEPper_H