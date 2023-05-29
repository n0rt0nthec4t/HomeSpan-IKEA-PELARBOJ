//
#include "HomeSpan.h"       // HomeSpan library
#include "extras/PwmPin.h"  // library of various PWM functions

// Define constants
#define ACCESSORYNAME "IKEA"         // Used for manufacturer name of HomeKit device
#define ACCESSORYPINCODE "03145154"  // HomeKit pairing code
#define BUTTON_PIN 14
#define LED_RED_PIN 15
#define LED_GREEN_PIN 33
#define LED_BLUE_PIN 27
#define MODE_OFF 0
#define MODE_COLOUR_CYCLE 1
#define MODE_COLOUR_LOCK 2
#define MODE_COLOUR_FADE 3
#define MAX_TIME_IN_MODE 10000
#define COLOUR_CYCLE_INTERVAL 2000
#define COLOUR_FADE_INTERVAL (40000 / 360)  // Time to show each colour in the hue colour wheel in milliseconds
#define NO_PRESS 0
#define SINGLE_PRESS 1
#define LONG_PRESS 2

// Definbe our global variables
LedPin *redPin;
LedPin *greenPin;
LedPin *bluePin;
int mode = MODE_OFF;
int hue = 0;
int saturation = 0;
int brightness = 100;
float colourCycleTimer;
float timeInMode;
boolean power = false;

// Custom class defintion for our LED
struct HomeKitLED : Service::LightBulb {
  SpanCharacteristic *HomeKitPower;       // reference to the On Characteristic
  SpanCharacteristic *HomeKitHue;         // reference to the Hue Characteristic
  SpanCharacteristic *HomeKitSaturation;  // reference to the Saturation Characteristic
  SpanCharacteristic *HomeKitBrightness;  // reference to the Brightness Characteristic

  HomeKitLED() : Service::LightBulb() {
    HomeKitPower = new Characteristic::On(power);
    HomeKitHue = new Characteristic::Hue(hue);  // 0 - 360
    HomeKitSaturation = new Characteristic::Saturation(saturation); // 0 - 100%
    HomeKitBrightness = new Characteristic::Brightness(brightness); // 0 - 100 %
  }  // end constructor

  boolean update() {
    if (HomeKitPower->updated() == true && HomeKitPower->getVal() != HomeKitPower->getNewVal()) {
      // HomeKit power status has change from either on -> off OR off -> on
      if (HomeKitPower->getNewVal() == false) {
        // Set colour to black ie: off
        mode = MODE_OFF;
        hue = 0;
        saturation = 0;
        power = false;
        Serial.print("HomeKit turned off\n");
      }
      if (HomeKitPower->getNewVal() == true) {
        // Set colour we start with, in this case, white
        mode = MODE_COLOUR_LOCK;
        hue = 0;
        saturation = 0;
        power = true;
        Serial.print("HomeKit turned on, colour locked mode\n");
      }
    }

    if (HomeKitHue->updated() == true) {
      // Since hue changed from HomeKit, this means we'll set the light mode to be locked to a colour
      mode = MODE_COLOUR_LOCK;
      hue = HomeKitHue->getNewVal();
      Serial.printf("HomeKit hue changed to %d\n", HomeKitHue->getNewVal());
    }

    if (HomeKitSaturation->updated() == true) {
      // Since saturation changed from HomeKit, this means we'll set the light mode to be locked to a colour
      mode = MODE_COLOUR_LOCK;
      saturation = HomeKitSaturation->getNewVal();
      Serial.printf("HomeKit saturation changed to %d\n", HomeKitSaturation->getNewVal());
    }

    if (HomeKitBrightness->updated() == true) {
      brightness = HomeKitBrightness->getNewVal();
      Serial.printf("HomeKit brightness changed to %d\n", HomeKitBrightness->getNewVal());
    }

    return (true);  // return true
  } // end update

  void loop() {
    // We'll use the HomeSpan update loop to update HomeKit seperately
    // Only update the characteristics we use if they have changed
    if (HomeKitPower->getVal() != power) {
      HomeKitPower->setVal(power);
    }
    if (HomeKitHue->getVal() != hue) {
      HomeKitHue->setVal(hue);
    }
    if (HomeKitSaturation->getVal() != saturation) {
      HomeKitSaturation->setVal(saturation);
    }
    if (HomeKitBrightness->getVal() != brightness) {
      HomeKitBrightness->setVal(brightness);
    }
  } // end loop
};

int buttonCheck(int pin) {
  int lastButtonState = digitalRead(pin);
  delay(50); // debounce time
  int currentButtonState = digitalRead(pin);
  int pressType = NO_PRESS;
  if (lastButtonState != currentButtonState) { // state changed
    if (currentButtonState == HIGH) {
      pressType = SINGLE_PRESS;
    }
  }
  return pressType;
} // end buttonCheck

void LEDFixedColourCycle(void) {
  // We are fixed colour cycling, so get the current values for hue, saturation and brightness
  // From this, we'll work out the next colour to switch too
  // White -> red -> orange -> yellow -> green -> blue -> purple

  if (hue == 0 && saturation == 0) {
    // Current colour is white, so move to red
    hue = 0;
    saturation = 100;
  } else if (hue == 0 && saturation == 100) {
    // Current colour is red, so move to orange
    hue = 35;
    saturation = 100;
  } else if (hue == 35 && saturation == 100) {
    // Current colour is orange, so move to yellow
    hue = 60;
    saturation = 100;
  } else if (hue == 60 && saturation == 100) {
    // Current colour is yellow, so move to green
    hue = 120;
    saturation = 100;
  } else if (hue == 120 && saturation == 100) {
    // Current colour is green, so move to blue
    hue = 240;
    saturation = 100;
  } else if (hue == 240 && saturation == 100) {
    // Current colour is blue, so move to purple
    hue = 280;
    saturation = 100;
  } else if (hue == 280 && saturation == 100) {
    // Current colour is purple, so move to white
    // Ends our colour cycle and restarts
    hue = 0;
    saturation = 0;
  }
}  // end LEDFixedColourCycle

void LEDFadeColourCycle(void) {
  // We are fade colour cycling, so use the current values for hue, saturation and brightness
  // to step around the hue colour wheel, starting from the current hue and adjusting the hue value in steps
  hue++;
  if (hue > 360) {
    hue = 0;
  }
}  // end LEDFadeColourCycle

void setup() {
  Serial.begin(115200);

  Serial.printf("Configured RGB LED: Pins=(%d,%d,%d)\n", LED_RED_PIN, LED_GREEN_PIN, LED_BLUE_PIN);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  redPin = new LedPin(LED_RED_PIN, 0, 20000, false);      // configures a PWM LED for output to the RED pin
  greenPin = new LedPin(LED_GREEN_PIN, 0, 20000, false);  // configures a PWM LED for output to the GREEN pin
  bluePin = new LedPin(LED_BLUE_PIN, 0, 20000, false);    // configures a PWM LED for output to the BLUE pin

  homeSpan.setPairingCode(ACCESSORYPINCODE);
  homeSpan.begin(Category::Lighting, "IKEA PELARBOJ", "IKEAPELARBOJ", "IKEA PELARBOJ");

  // Configure HomeKit accessory service
  new SpanAccessory();
  new Service::AccessoryInformation();
  new Characteristic::Identify();
  new Characteristic::FirmwareRevision("1.0.0");
  new Characteristic::Manufacturer(ACCESSORYNAME);
  new Characteristic::Model("PELARBOJ");
  new Characteristic::Name("IKEA PELARBOJ Light");
  new Characteristic::SerialNumber("304.230.17");  // IKEA product code, 68:EC:8A:xx:xx:xx??

  new HomeKitLED();  // Configure HomeKit lightbulb

  colourCycleTimer = millis(); // Get current time
  timeInMode = millis(); // Get current time

  homeSpan.autoPoll();
}  // end setup

void loop() {
  int buttonPress = buttonCheck(BUTTON_PIN);  // Perform a check of the push button first

  // Determined the button presss type, so now action it
  // If the current light mode has been active for more than 10secs, the next press of the button will turn off, rather than cycle modes
  if (buttonPress == SINGLE_PRESS) {
    // We need to cycle the lights operating modes
    mode++;
    if (mode > MODE_COLOUR_FADE) {
      mode = MODE_OFF;
    }

    if (mode != MODE_OFF && (millis() - timeInMode) > MAX_TIME_IN_MODE) {
      // Current mode has been active for more than 10 seconds, so mode will be OFF
      mode = MODE_OFF;
    }

    colourCycleTimer = millis(); // Update colour cycle time
    timeInMode = millis(); // Updated time in this mode

    if (mode == MODE_COLOUR_CYCLE) {
      // Set colour we start with, in this case, white
      hue = 0;
      saturation = 0;
      power = true;  // Should be on
      Serial.print("Button turned to colour cycle mode\n");
    }

    if (mode == MODE_COLOUR_LOCK) {
      // We know last mode was colour cycling, so use the current values for hue, saturation and brightness
      power = true;  // Should be on
      Serial.print("Button turned to colour locked mode\n");
    }

    if (mode == MODE_COLOUR_FADE) {
      // Set colour we start with will be the current hue, except if white, in which case, we'll start from red
      if (hue == 0 && saturation == 0) {
        hue = 0;
        saturation = 100;
      }
      power = true;  // Should be on
      Serial.print("Button turned to colour fade mode\n");
    }

    if (mode == MODE_OFF) {
      // Set colour to black ie: off
      hue = 0;
      saturation = 0;
      power = false;  // Should be off
      Serial.print("Button turned off\n");
    }
  }

  // Output the current HSV values to the RGB LEDs
  if (power == true) {
    float red, green, blue;
    LedPin::HSVtoRGB((float)hue, (saturation / (float)100), (brightness / (float)100), &red, &green, &blue);
    int r = red * 100;
    int g = green * 100;
    int b = blue * 100;
    redPin->set(r);
    greenPin->set(g);
    bluePin->set(b);
  }
  if (power == false) {
    // Power off, so no LED output
    redPin->set(0);
    greenPin->set(0);
    bluePin->set(0);
  }

  if (mode == MODE_COLOUR_CYCLE && (millis() - colourCycleTimer) > COLOUR_CYCLE_INTERVAL) {
    LEDFixedColourCycle();
    colourCycleTimer = millis();
  }
  if (mode == MODE_COLOUR_FADE && (millis() - colourCycleTimer) > COLOUR_FADE_INTERVAL) {
    LEDFadeColourCycle();
    colourCycleTimer = millis();
  }
}  // end loop
