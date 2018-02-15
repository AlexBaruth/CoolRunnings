CoolRunnings Fan Controller for Arduino 


This sketch is for the CoolRunnings v3 PC fan controller board.  It has been tested on the Atmel328p, but should also work on other AVRs.

The sketch includes support for the following:

Two OneWire DS18B20 temperature sensors.

RPM reading for common three wire PC fans which induce a Hall Effect sensor.

Three speed automatic and manual fan control as well as continuously variable fan speed control. using PWM.

Serial Port communication using a USB to Serial converter(CH340)

Serial Commands for fan control and the setting of relevant parameters.

IC support for an external 16x2 LCD display.

Three fan speed indicator lights.

Serial communication at 9600 baud.
Type ? for commands

Needed Libraries:

These libraries should also be available through the Arduino IDE
Menu: Sketch -> Include Library -> Manage Libraries

Arduino library (included with IDE)
OneWire - https://github.com/PaulStoffregen/OneWire
DallasTemperature - https://github.com/milesburton/Arduino-Temperature-Control-Library
LiquidCrystal_I2C - https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library
