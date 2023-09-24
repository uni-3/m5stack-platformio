#include <M5Stack.h>
#include <SensirionI2CScd4x.h>

SensirionI2CScd4x scd4x;

void co2_setup() {
    M5.Lcd.setTextFont(4);
    M5.Lcd.drawString("Unit CO2", 110, 0);
    uint16_t error;
    char errorMessage[256];

    scd4x.begin(Wire);

    // stop potentially previously started measurement
    error = scd4x.stopPeriodicMeasurement();
    if (error) {
        Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }

    // Start Measurement
    error = scd4x.startPeriodicMeasurement();
    if (error) {
        Serial.print("Error trying to execute startPeriodicMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }

    Serial.println("Waiting for first measurement... (5 sec)");
}

void co2_loop() {
    uint16_t error;
    char errorMessage[256];

    delay(1000);

    // Read Measurement
    uint16_t co2      = 0;
    float temperature = 0.0f;
    float humidity    = 0.0f;
    bool isDataReady  = false;
    error             = scd4x.getDataReadyFlag(isDataReady);
    if (error) {
        M5.Lcd.print("Error trying to execute readMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
        return;
    }
    if (!isDataReady) {
        return;
    }
    error = scd4x.readMeasurement(co2, temperature, humidity);
    if (error) {
        M5.Lcd.print("Error trying to execute readMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else if (co2 == 0) {
        M5.Lcd.println("Invalid sample detected, skipping.");
    } else {
        M5.Lcd.setCursor(0, 40);
        M5.Lcd.printf("Co2:%d\n", co2);
        M5.Lcd.printf("Temperature:%.2f\n", temperature);
        M5.Lcd.printf("Humidity:%.2f\n", humidity);
    }
}

void setup() {
    M5.begin();
    M5.Power.begin();
}

//bool isPeriodicMeasurement = false;
enum OperationMode {
  MODE_NONE,
  MODE_CO2_LOOP,
  MODE_DISPLAY_B,
  MODE_DISPLAY_C
};
OperationMode currentMode = MODE_NONE;


void loop() {
  M5.update();

  // pressed bitton a
  if (M5.BtnA.wasPressed()) {
    // ボタン押したあとにloopが実行されるようにする
    co2_setup();
    currentMode = MODE_CO2_LOOP;

  } else if (M5.BtnB.wasPressed()) {
    // show B in display
    currentMode = MODE_DISPLAY_B;
    M5.Lcd.setTextFont(4);
    M5.Lcd.print("B");
  }  else if (M5.BtnC.wasPressed()) {
    currentMode = MODE_DISPLAY_C;
    // show C in display
    M5.Lcd.print("C");
  } else if (M5.BtnB.wasReleasefor(700)) {
    M5.Lcd.clear(BLACK);
    M5.Lcd.setCursor(0, 0);
  }

  switch (currentMode) {
    case MODE_CO2_LOOP:
      co2_loop();
      break;
    case MODE_DISPLAY_B:
      M5.Lcd.setTextFont(4);
      M5.Lcd.print("B");
      break;
    case MODE_DISPLAY_C:
      M5.Lcd.print("C");
      break;
    default:
      break;
  }
}