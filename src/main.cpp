#include <M5Unified.h>
#include <Ambient.h>
#include <Avatar.h>
#include <SensirionI2CScd4x.h>

#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "M5Core2_Avatar_VoiceText_TTS/AudioOutputI2SLipSync.h"
#include "M5Core2_Avatar_VoiceText_TTS/AudioFileSourceVoiceTextStream.h"

#include <AudioOutput.h>
//#include "AudioOutputM5Speaker.h"

using std::getenv;


const char* SSID = getenv("WIFI_SSID");
const char* PASSWORD = getenv("WIFI_PASS");


using namespace m5avatar;
Avatar avatar;

// co2
SensirionI2CScd4x scd4x;

// for ambient
WiFiClient client;
Ambient ambient;
const unsigned channelId = 0;
const char* writeKey = "0xx";

// 送信間隔
const int SEND_PEDIOD = 300;

unsigned long currentMillis;
unsigned long lastupdateMillis = 0;
 
#define RGB(r,g,b) (int16_t)(b + (g << 5) + (r << 11)) //色0～31 緑は0～63

int calculateSignalLevel(long rssi) {
    if (rssi <= -96)
        return 1;
    else if (rssi <= -65)
        return 2;
    else
        return 3;
}

 
void draw_wifi_antenna(bool force=false) {
    if (currentMillis - lastupdateMillis > 500 || force) {
        if (WiFi.status() != WL_CONNECTED) {
            M5.Lcd.setTextFont(4);
            M5.Lcd.drawString("x", 290, 0);
        } else {
            long rssi = WiFi.RSSI();
            int siglevel = calculateSignalLevel(rssi);

            M5.Lcd.fillRect(290, 5, 30, 15, RGB(0, 0, 0));
            if (siglevel >= 1)
                M5.Lcd.fillRect(290, 10, 5, 5, WHITE);
            if (siglevel >= 2)
                M5.Lcd.fillRect(300, 5, 5, 10, WHITE);
            if (siglevel >= 3)
                M5.Lcd.fillRect(310, 0, 5, 15, WHITE);
        }
        lastupdateMillis = currentMillis;
    }
}

// draw number with batterylevel
void draw_options() {
    uint8_t BatteryLevel = M5.Power.getBatteryLevel();   
    M5.Lcd.setTextFont(2);
    M5.Lcd.setCursor(245, 0);
    M5.Lcd.printf("%3d%%", BatteryLevel);
    draw_wifi_antenna(true);
}

enum State {
    HOT,
    COLD,
    NORMAL
};

// Define a function to determine the state based on temperature and humidity
State determineState(float temperature, float humidity) {
    if (temperature > 25 && humidity > 60) {
        return HOT;
    } else if (temperature < 15) {
        return COLD;
    } else {
        return NORMAL;
    }
}


void co2_setup() {
    uint16_t error;
    char errorMessage[256];

    ambient.begin(channelId, writeKey, &client);
    scd4x.begin(Wire);

    draw_options();
    //M5.Lcd.setTextFont(4);
    //M5.Lcd.drawString("Unit CO2", 110, 0);

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
    M5.update();
    static unsigned long lastSendTime = 0;
    unsigned long currentTime = millis();

    uint16_t error;
    char errorMessage[256];


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
        return;
    } else if (co2 == 0) {
        M5.Lcd.println("Invalid sample detected, skipping.");
        return;
    } else {
        // M5.Lcd.setCursor(0, 40);
        // M5.Lcd.printf("Co2:%d\n", co2);
        // M5.Lcd.printf("Temperature:%.2f\n", temperature);
        // M5.Lcd.printf("Humidity:%.2f\n", humidity);
        
        State currentState = determineState(temperature, humidity);
        M5.Lcd.setTextFont(1);
        if (co2 < 1000) {
          //avatar.setExpression(Expression::Happy);
          switch (currentState) {
              case HOT:
                  avatar.setExpression(Expression::Angry);
                  break;
              case COLD:
                  avatar.setExpression(Expression::Sad);
                  break;
              case NORMAL:
                  avatar.setExpression(Expression::Happy);
                  break;
          }
        } else {
          avatar.setExpression(Expression::Sleepy);
        }
        char c[100]; // Allocate a character array
        sprintf(c, "co2:%d\n temp:%.2f\n RH:%.0f", co2, temperature, humidity);
        avatar.setSpeechText(c);
    }

    if (currentTime - lastSendTime > SEND_PEDIOD) {
      lastSendTime = currentTime;
      ambient.set(1, temperature);
      ambient.set(2, humidity);
      ambient.set(3, co2);
      ambient.send();
      Serial.println("send to ambient");
    }

    delay(1000);
}


AudioGeneratorMP3 *mp3;
AudioFileSourceVoiceTextStream *file;
AudioFileSourceBuffer *buff;
AudioOutputI2SLipSync *out;
const int preallocateBufferSize = 40*1024;
uint8_t *preallocateBuffer;


void text_to_speech_setup()
{
  auto cfg = M5.config();
  cfg.external_spk = true; 
  int volume = 100;
  M5.Speaker.setVolume(volume);


  Serial.begin(9600);

  Serial.println("setup Connecting to WiFi");
  preallocateBuffer = (uint8_t*)ps_malloc(preallocateBufferSize);
  if (!preallocateBuffer) {
    M5.Display.printf("FATAL ERROR:  Unable to preallocate %d bytes for app\n", preallocateBufferSize);
    for (;;) { delay(1000); }
  }
 // M5.Speaker.begin();

  //M5.begin(true, false, true);
  M5.begin(cfg);
  M5.Lcd.setBrightness(30);
  M5.Lcd.clear();
  M5.Lcd.setTextSize(2);
  delay(1000);

  Serial.println("Connecting to WiFi");
  M5.Lcd.print("Connecting to WiFi");
  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    M5.Lcd.print(".");
  }
  Serial.println("\nConnected");
  M5.Lcd.println("\nConnected");
}


// metro
const int TEMPO = 40; // BPM
const int INTERVAL = 60000 / TEMPO; // ミリ秒単位でのインターバル
const int FREQUENCY = 750; // ティック音の周波数 (Hz)
const int DURATION = 50; // ティック音の持続時間 (ms)

void metro_loop() {
  M5.Lcd.fillCircle(160, 120, 10, TFT_RED); // 赤い点を表示
  M5.Speaker.setVolume(100);
  M5.Speaker.tone(FREQUENCY, DURATION); // ティック音を出す
  delay(DURATION); // 持続時間を除いたインターバル待機

  M5.Lcd.fillCircle(160, 120, 10, TFT_BLACK); // 赤い点を消す
  delay(INTERVAL - DURATION); // 持続時間を除いたインターバル待機
}


void setup() {
    M5.begin();
    M5.Power.begin();

    WiFi.begin(SSID, PASSWORD);
    unsigned long startTime = millis();
 
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        if (millis() - startTime > 10000) {
            Serial.println("wifi not connected");
            break;
        }
    }
    draw_options();
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
    avatar.setPosition(14, 0);
    avatar.init();
    co2_setup();
    currentMode = MODE_CO2_LOOP;

  } else if (M5.BtnB.wasPressed()) {
    M5.Lcd.clear(BLACK);
    // show B in display
    M5.Lcd.setTextFont(4);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.printf("Metronome\nTempo: %d BPM", TEMPO);
    currentMode = MODE_DISPLAY_B;
  }  else if (M5.BtnC.wasPressed()) {
    avatar.init();
    text_to_speech_setup();

    currentMode = MODE_DISPLAY_C;
  } else if (M5.BtnA.wasReleaseFor(700)) {
    M5.Lcd.clear(BLACK);
    M5.Lcd.setCursor(0, 0);
  }

  switch (currentMode) {
    case MODE_CO2_LOOP:
      co2_loop();
      break;
    case MODE_DISPLAY_B:
      metro_loop();
      break;
    case MODE_DISPLAY_C:
      //text_to_speech_loop();
      break;
    default:
      break;
  }
}