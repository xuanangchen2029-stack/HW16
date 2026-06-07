# HW16 - Servo Control with ADC Feedback and INA219 Current Sensing

## Overview
This project implements a closed-loop test setup for a modified RC servo using the STM32 NUCLEO-C092RC board.  
The system includes:

- ADC reading from the servo potentiometer
- PWM motor driving through a DRV8833 H-bridge
- Current sensing with an INA219 over I2C
- A 1 kHz timer interrupt for periodic control timing
- Safety limits based on potentiometer ADC range

The motor is driven in a simple repeating sequence:

1. Forward
2. Off
3. Reverse
4. Off

During operation, the STM32 continuously prints the potentiometer reading, INA219 data, and current estimate through the serial port.

---

## Hardware Used

- STM32 NUCLEO-C092RC
- Modified RC servo
- DRV8833 motor driver
- INA219 current sensor
- External battery supply for motor power
- Breadboard and jumper wires

---

## Wiring Summary

### Servo potentiometer
- Potentiometer output -> ADC input `PA0` (`A0` on board)
- Potentiometer power -> `3.3V`
- Potentiometer ground -> `GND`

### DRV8833
- PWM output 1 -> `PA8`
- PWM output 2 -> `PA1`
- Driver logic ground -> common `GND`
- Driver sleep/enable -> `3.3V`
- Motor power -> external battery supply

### INA219
- `SDA` -> `PA6` (board location used as `D12`)
- `SCL` -> `PA7` (board location used as `D11`)
- `VCC` -> `3.3V`
- `GND` -> `GND`

### Motor current path
The INA219 was inserted in series with the motor current path so current could be measured during forward and reverse motion.

---

## Software Features

- `ADC1` reads the servo potentiometer position
- `TIM1` generates PWM for motor drive
- `TIM2` runs at 1 kHz and provides timing ticks
- `I2C2` communicates with the INA219
- Serial output is used for debugging and demonstration

The code includes:
- Motor state machine
- ADC-based safety stop
- INA219 shunt voltage and current estimation
- Periodic serial printing for monitoring

---

## Safety Limits

The system uses ADC limits to stop the motor if the potentiometer reading moves outside the allowed range.

Example limits used in testing:
- `ADC_MIN_LIMIT = 380`
- `ADC_MAX_LIMIT = 2600`

These limits were tuned experimentally based on the actual motion range of the modified servo.

---

## Example Serial Output

```text
[STATE=0] ADC=2154 | CFG=0x399F | SHUNT raw=1545 | BUS raw=0x00A2 | Shunt=15.45 mV | Current=154.50 mA
[STATE=1] ADC=1691 | CFG=0x399F | SHUNT raw=0 | BUS raw=0x05DA | Shunt=0.00 mV | Current=0.00 mA
[STATE=2] ADC=1530 | CFG=0x399F | SHUNT raw=-4657 | BUS raw=0x086A | Shunt=-46.57 mV | Current=-465.70 mA
