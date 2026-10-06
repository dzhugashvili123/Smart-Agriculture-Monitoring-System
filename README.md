# Smart Agriculture Monitoring System

This is a small IoT project I made with a NodeMCU (ESP8266). It watches the conditions around a plant, turns a water pump and an LED on or off by itself, and sends the readings to Adafruit IO so I can see them from my phone.

The idea is simple: if the soil gets too dry, water it. If it gets dark, turn the light on. Everything else is just measuring and uploading those values.

This follows the same idea as the [CircuitDigest smart agriculture project](https://circuitdigest.com/microcontroller-projects/iot-based-smart-agriculture-moniotring-system). The sketch in this repo is my own version of that program.

## What it measures

- Air temperature and humidity, using a DHT11
- Soil temperature, using a DS18B20
- Soil moisture, using a moisture sensor on the analog pin
- Light level, using an LDR
- A short weather label from OpenWeatherMap (sunny, rain, few clouds, or broken clouds)

## Parts

- NodeMCU ESP8266
- DHT11
- DS18B20 waterproof temperature probe (with a 4.7k pull-up on the data pin)
- Soil moisture sensor
- LDR
- Small water pump
- LED strip
- TIP122 transistors so the NodeMCU can switch the pump and the LED (they need more current than a GPIO pin can give)
- 12V adapter and a 7805 regulator for the 5V side

## Wiring

These are the pins used in `src/Smart_Agriculture_Monitoring_System.ino`.

| Part | NodeMCU pin | What the pin does |
| --- | --- | --- |
| DHT11 data | D4 | Reads air temperature and humidity |
| DS18B20 data | D2 (GPIO4) | Reads soil temperature (1-Wire) |
| Soil moisture analog out | A0 | Only analog pin on the NodeMCU |
| LDR | D1 | Read with `analogRead`, dark if the value is 200 or less |
| Water pump | D8 | HIGH turns the pump on |
| LED strip | D0 | HIGH turns the LED on |

The pump and LED are not powered directly from the NodeMCU. Each one goes through a transistor.

## How the program works

`setup()` starts the serial monitor at 9600 baud, starts the DHT11 and the soil probe, subscribes to the LED and pump feeds, and connects to Wi-Fi.

After that, `loop()` keeps doing the same steps:

1. Make sure the MQTT connection to Adafruit IO is still up. If it drops, it tries 3 times. If all 3 fail, it sits in a loop until the board resets.
2. About every 10 minutes, ask OpenWeatherMap for the Hyderabad forecast and turn the description into a short label.
3. Read the LDR. At 200 or below the LED turns on. Above that it turns off.
4. Read soil moisture and convert it to a percentage. Below 35% the pump turns on. Above 38% it turns off. Between 35% and 38% it stays as it is, so the pump does not flicker on and off around one value.
5. Read air temperature, humidity, and soil temperature.
6. Every 50 seconds, publish moisture, temperature, humidity, soil temperature, and the weather label.
7. Check the dashboard for new LED or pump messages.
8. Wait 9 seconds, then repeat.

The LED feed works the normal way: `ON` turns the pin high, `OFF` turns it low. The pump feed in this sketch is the other way around: `OFF` drives the motor pin high and `ON` drives it low. Local moisture control runs first, then a new dashboard message can override it until the next moisture check.

## Adafruit IO

I used the free Adafruit IO account. The sketch publishes to these feeds (the names have to match):

- `Moisture`
- `Temperature`
- `Humidity`
- `SoilTemp`
- `WeatherData`

It also listens to:

- `LED`
- `Pump`

On the dashboard I added gauges for the sensor feeds, a text block for the weather label, and two toggle buttons for the LED and the pump.

## Weather

The board calls the OpenWeatherMap 5-day forecast API for Hyderabad. It only looks at the later forecast description and stores one of these strings:

- `Few Clouds`
- `Rain`
- `Broken Clouds`
- `Sunny` (used for anything else)

That string is what gets sent to the `WeatherData` feed.

## Libraries

Install these from the Arduino Library Manager:

- ESP8266 board package (this brings `ESP8266WiFi`)
- Adafruit MQTT Library
- DHT sensor library
- DallasTemperature
- OneWire
- ArduinoJson **version 5** (the sketch uses `DynamicJsonBuffer`, which was removed in version 6)

Board setting: **NodeMCU 1.0 (ESP-12E Module)**. Serial monitor: **9600 baud**.

## Before you upload

Open the sketch and put in your own values:

- Wi-Fi name and password
- Adafruit IO username and AIO key
- OpenWeatherMap API key
- City name, set to `Hyderabad,IN`

If this repo is public, do not leave real passwords or API keys in the file. Use your own keys only on the board.

## Upload

1. Connect the NodeMCU with a USB cable.
2. Pick the NodeMCU board and the right port.
3. Upload `src/Smart_Agriculture_Monitoring_System.ino`.
4. Open the serial monitor. You should see it connect to Wi-Fi, then print the light reading and the soil moisture percentage.
