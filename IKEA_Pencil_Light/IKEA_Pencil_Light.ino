// Using HomeSpan library v2.1.x
//
// Code version 2026.02.12
// Mark Hulskamp

// Include required header files
#include "HomeSpan.h"                       // Main HomeSpan library

// Define constants
#define ACCESSORYNAME "IKEA"                // Used for manufacturer name of HomeKit device
#define ACCESSORYPINCODE "03145154"         // HomeKit pairing code
#define BUTTON_PIN 14
#define LED_RED_PIN 15
#define LED_GREEN_PIN 33
#define LED_BLUE_PIN 27
#define MODE_OFF 0
#define MODE_COLOUR_CYCLE 1
#define MODE_COLOUR_LOCK 2
#define MODE_COLOUR_FADE 3
#define COLOUR_FADE_INTERVAL (40000 / 360)  // Time to show each colour in the hue colour wheel in milliseconds

// Custom class defintion for our LED
struct RGB_LED : Service::LightBulb {       // RGB LED (Command Cathode)

  LedPin *redPin, *greenPin, *bluePin;

  SpanCharacteristic *HomeKitPower;         // reference to the HomeKit On Characteristic
  SpanCharacteristic *HomeKitHue;           // reference to the HomeKit Hue Characteristic
  SpanCharacteristic *HomeKitSaturation;    // reference to the HomeKit Saturation Characteristic
  SpanCharacteristic *HomeKitBrightness;    // reference to the HomeKit Brightness Characteristic

  int currentLightMode = MODE_OFF;          // Current mode of the light
  int hue = 0;                              // Inital LED hue
  int saturation = 0;                       // Inital LED saturation 
  int brightness = 100;                     // Inital LED brightness
  float timer = millis();
  boolean power = false;
  boolean updateHomeKit = false;            // flag if we need to updated HomeKit values due to a change

  RGB_LED(int red_pin, int green_pin, int blue_pin)
    : Service::LightBulb() {
    new SpanButton(BUTTON_PIN, 15000, 2, 0, SpanButton::TRIGGER_ON_LOW);

    HomeKitPower = new Characteristic::On(false);             // instantiate the Ob Characteristic with an initial of off
    HomeKitHue = new Characteristic::Hue(0);                  // instantiate the Hue Characteristic with an initial value of 0 out of 360
    HomeKitSaturation = new Characteristic::Saturation(0);    // instantiate the Saturation Characteristic with an initial value of 0%
    HomeKitBrightness = new Characteristic::Brightness(100);  // instantiate the Brightness Characteristic with an initial value of 100%
    HomeKitBrightness->setRange(0, 100, 1);                   // sets the range of the Brightness to be from a min of 0%, to a max of 100%, in steps of 1%

    this->redPin = new LedPin(red_pin, 0, 20000, false);      // configures a PWM LED for output to the RED pin
    this->greenPin = new LedPin(green_pin, 0, 20000, false);  // configures a PWM LED for output to the GREEN pin
    this->bluePin = new LedPin(blue_pin, 0, 20000, false);    // configures a PWM LED for output to the BLUE pin

    Serial.printf("Configuring RGB LED: Pins=(%d,%d,%d)\n", redPin->getPin(), greenPin->getPin(), bluePin->getPin());
  }  // end constructor

  boolean update() {
    if (HomeKitPower->updated() == true && HomeKitPower->getVal() != HomeKitPower->getNewVal()) {
      // HomeKit power status has change from either on -> off OR off -> on
      if (HomeKitPower->getNewVal() == false) {
        // Set colour to black ie: off
        this->currentLightMode = MODE_OFF;
        this->power = false;
        Serial.print("HomeKit turned off\n");
      }
      if (HomeKitPower->getNewVal() == true) {
        // Turning on from HomeKit, so we'll use the HomeKit values for hue, saturation and brightness
        this->currentLightMode = MODE_COLOUR_LOCK;
        this->hue = HomeKitHue->getVal();
        this->saturation = HomeKitSaturation->getVal();
        this->brightness = HomeKitBrightness->getVal();
        this->power = true;
        Serial.print("HomeKit turned on, colour locked mode\n");
      }
    }

    if (HomeKitHue->updated() == true) {
      // Since hue changed from HomeKit, this means we'll set the light mode to be locked to a colour
      this->currentLightMode = MODE_COLOUR_LOCK;
      this->hue = HomeKitHue->getNewVal();
      Serial.printf("HomeKit hue changed to %d\n", HomeKitHue->getNewVal());
    }

    if (HomeKitSaturation->updated() == true) {
      // Since saturation changed from HomeKit, this means we'll set the light mode to be locked to a colour
      this->currentLightMode = MODE_COLOUR_LOCK;
      this->saturation = HomeKitSaturation->getNewVal();
      Serial.printf("HomeKit saturation changed to %d\n", HomeKitSaturation->getNewVal());
    }

    if (HomeKitBrightness->updated() == true) {
      this->brightness = HomeKitBrightness->getNewVal();
      Serial.printf("HomeKit brightness changed to %d\n", HomeKitBrightness->getNewVal());
    }

    return (true);  // return true
  }                 // end update

  void loop() {
    // Update HomeKit to show the current HSV values
    if (this->updateHomeKit == true) {
      HomeKitHue->setVal(this->hue);
      HomeKitSaturation->setVal(this->saturation);
      HomeKitBrightness->setVal(this->brightness);
      HomeKitPower->setVal(this->power);
      this->updateHomeKit = false;
    }

    // Output the current HSV values to the RGB LEDs
    if (this->power == true) {
      float red, green, blue;
      LedPin::HSVtoRGB((float)this->hue, (this->saturation / (float)100), (this->brightness / (float)100), &red, &green, &blue);

      // Apply gamma correction and channel calibration
      float gamma = 2.2;
      float rFactor = 1.2; // boost red
      float gFactor = 1.0;
      float bFactor = 0.9; // reduce blue
      int r = constrain(pow(red, gamma) * 100 * rFactor, 0, 100);
      int g = constrain(pow(green, gamma) * 100 * gFactor, 0, 100);
      int b = constrain(pow(blue, gamma) * 100 * bFactor, 0, 100);
      
      redPin->set(r);
      greenPin->set(g);
      bluePin->set(b);
    }
    
    if (this->power == false) {
      // Power off, so no LED output
      redPin->set(0);
      greenPin->set(0);
      bluePin->set(0);
    }

    //Serial.printf("hue %d saturation %d brightness %d power %d timer %f (%f)\n", this->hue, this->saturation, this->brightness, this->power, this->timer, (millis() - this->timer));

    if (this->currentLightMode == MODE_COLOUR_CYCLE && (millis() - this->timer) > 2000) {
      this->timer = millis(); // Update timer
      this->updateHomeKit = true;
      LEDFixedColourCycle();
    }

    if (this->currentLightMode == MODE_COLOUR_FADE && (millis() - this->timer) > COLOUR_FADE_INTERVAL) {
      this->timer = millis(); // Update timer
      this->updateHomeKit = true;
      LEDFadeColourCycle();
    }
  }  // end loop

  void button(int pin, int pType) override {
    if (pType == SpanButton::SINGLE) {
      // We need to cycle the lights operating modes

      this->updateHomeKit = true;
      this->timer = millis(); // Update timer
      this->currentLightMode++;
      if (this->currentLightMode > MODE_COLOUR_FADE) {
        this->currentLightMode = MODE_OFF;
      }

      if (this->currentLightMode == MODE_COLOUR_CYCLE) {
        // Set colour we start with, in this case, white
        this->hue = 0;
        this->saturation = 0;
        this->brightness = 100; // Reset brightness to 100% if using physical button
        this->power = true;  // Should be on
        Serial.print("Button turned to colour cycle mode\n");
      }

      if (this->currentLightMode == MODE_COLOUR_LOCK) {
        // We know last mode was colour cycling, so use the current values for hue, saturation and brightness
        this->power = true;  // Should be on
        Serial.print("Button turned to colour locked mode\n");
      }

      if (this->currentLightMode == MODE_COLOUR_FADE) {
        // Set colour we start with, in this case, red
        this->hue = 0;
        this->saturation = 100;
        this->power = true;  // Should be on
        Serial.print("Button turned to colour fade mode\n");
      }

      if (this->currentLightMode == MODE_OFF) {
        // Set colour to black ie: off
        this->hue = 0;
        this->saturation = 0;
        this->power = false;  // Should be off
        Serial.print("Button turned off\n");
      }

    }

    if (pType == SpanButton::LONG) {
        // We'll use this for resetting HomeKit paring Default for this is 15seonds
        Serial.print("HomeKit pairing reset requested. To implement\n");
    }
  }  // end button

  void LEDFixedColourCycle(void) {
    // We are fixed colour cycling, so get the current values for hue, saturation and brightness
    // From this, we'll work out the next colour to switch too
    // White -> red -> orange -> yellow -> green -> blue -> purple

    if (this->hue == 0 && this->saturation == 0) {
      // Current colour is white, so move to red
      this->hue = 0;
      this->saturation = 100;
    } else if (this->hue == 0 && this->saturation == 100) {
      // Current colour is red, so move to orange
      this->hue = 35;  // 39??
      this->saturation = 100;
    } else if (this->hue == 35 && this->saturation == 100) {
      // Current colour is orange, so move to yellow
      this->hue = 60;
      this->saturation = 100;
    } else if (this->hue == 60 && this->saturation == 100) {
      // Current colour is yellow, so move to green
      this->hue = 120;
      this->saturation = 100;
    } else if (this->hue == 120 && this->saturation == 100) {
      // Current colour is green, so move to blue
      this->hue = 240;
      this->saturation = 100;
    } else if (this->hue == 240 && this->saturation == 100) {
      // Current colour is blue, so move to purple
      this->hue = 280;  // 287?
      this->saturation = 100;
    } else if (this->hue == 280 && this->saturation == 100) {
      // Current colour is purple, so move to white
      // Ends our colour cycle and restarts
      this->hue = 0;
      this->saturation = 0;
    }
  }  // end LEDFixedColourCycle

  void LEDFadeColourCycle(void) {
    // We are fade colour cycling, so use the current values for hue, saturation and brightness
    // to step around the hue colour wheel, starting from the current hue and adjusting the hue value in steps
    this->hue = (this->hue + 1) % 360;
  }  // end LEDFadeColourCycle
};


void setup() {
  Serial.begin(115200);

  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);

  homeSpan.setPairingCode(ACCESSORYPINCODE);
  homeSpan.begin(Category::Lighting, "IKEA PELARBOJ", "IKEAPELARBOJ", "IKEA PELARBOJ");

  new SpanAccessory();

  new Service::AccessoryInformation();
  new Characteristic::Identify();
  new Characteristic::FirmwareRevision("1.0.0");
  new Characteristic::Manufacturer(ACCESSORYNAME);
  new Characteristic::Model("PELARBOJ");
  new Characteristic::Name("IKEA PELARBOJ Light");
  new Characteristic::SerialNumber("304.230.17");  // IKEA product code, 68:EC:8A:xx:xx:xx??

  new RGB_LED(LED_RED_PIN, LED_GREEN_PIN, LED_BLUE_PIN);  // Create an RGB LED attached to pins

  homeSpan.autoPoll();
}  // end setup

void loop() {

}  // end loop
