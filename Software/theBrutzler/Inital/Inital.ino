#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Display
#include <block.h>
#define OLED_SDA 17
#define OLED_SCL 18
Adafruit_SSD1306 display(128, 64, &Wire1); //, OLED_RESET);
int y = 0;
int x = 0;

// RGB LED
#include "Freenove_WS2812_Lib_for_ESP32.h"
#define LEDS_COUNT  1
#define LEDS_PIN	42
#define CHANNEL		0
Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL, TYPE_GRB);
uint8_t m_color[5][3] = { {255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}, {0, 0, 0} };
int delayval = 100;

//ETHANOL
#include "SP3SAQ2.h"
#define ADC_BIT_RESU (12) // for ESP32
#define pin          (4)  // D4 (ADC1)
float sensorVal, Ethanol;
SP3SAQ2 sensor(ADC_BIT_RESU, pin);

//I2C_Sensors
#define Sensor_SDA 39
#define Sensor_SCL 40


//TMP102
#include <SparkFunTMP102.h> // Used to send and recieve specific information from our sensor
TMP102 sensor0;

//BMP390
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP3XX.h>
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BMP3XX BMP390;

//DHT bzw SY-TH-36BP5(LCSC C19727297)
#include <DFRobot_DHT20.h>
DFRobot_DHT20 DHT(&Wire,0xB8);

//VL53L8
#include <vl53l8cx.h>
#define LPN_PIN -1
#define PWREN_PIN -1
VL53L8CX sensor_vl53l8cx_top(&Wire, LPN_PIN);
uint8_t status;
bool EnableAmbient = false;
bool EnableSignal = false;
char report[256];
uint8_t res = VL53L8CX_RESOLUTION_4X4;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  //Display
  Wire1.begin(OLED_SDA, OLED_SCL);

  //RGB
	strip.begin();
	strip.setBrightness(10);

  //ETHANOL
  sensor.begin(); 

  //I2C Sensors
  Wire.begin(Sensor_SDA,Sensor_SCL);
  sensor0.begin();  

  //DHT
  DHT.begin();

  //VL53L8
  sensor_vl53l8cx_top.begin();
  status = sensor_vl53l8cx_top.init();
  status = sensor_vl53l8cx_top.start_ranging();







  while (y < 8)
  {
		strip.setLedColorData(0, m_color[y][0], m_color[y][1], m_color[y][2]);
		strip.show();

    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print(F(" Junkies Easteregg "));
    display.drawBitmap(34, 10, block[x], 54, 54, WHITE);
    display.display();
    delay(100);
    if (x == 13)
      x = 0;
    else
      x++;
    y++;
  }
  display.clearDisplay();

}

void loop() {

    //ETHANOL
    sensorVal = sensor.read();
    Ethanol = sensor.calculateppm(sensorVal, 3);
    display.setCursor(0, 0);
    display.print(F(" Ethanol: "));
    display.println(Ethanol);
    
    //TMP102
    sensor0.oneShot(1); // Set One-Shot bit
    while(sensor0.oneShot() == 0); // Wait for conversion to be ready
      
    display.print(F(" TempT: "));
    display.println(sensor0.readTempC());  // Print temperature reading

    //BMP390
    BMP390.performReading();
    float temp = BMP390.temperature;         // °C
    float pressure_hPa = BMP390.pressure / 100.0;  // hPa
    float altitude_m = BMP390.readAltitude(SEALEVELPRESSURE_HPA);  // Höhe über NN
    display.print("TempB: ");
    display.print(temp); // Print with 2 decimal places
    display.println(" °C");
    display.print("Pressure: ");
    display.print((pressure_hPa*1000),2); // Convert Pa to hPa (hectopascals)
    display.println(" Bar");

    //DHT
    display.print("TempD: ");
    display.print(DHT.getTemperature());
    display.println(" °C");
    display.print("HumiD: ");
    display.print(DHT.getHumidity()*100);
    display.println(" H");

    //VL53L8C
    VL53L8CX_ResultsData Results;
    uint8_t NewDataReady = 0;
    do {
      status = sensor_vl53l8cx_top.check_data_ready(&NewDataReady);
    } while (!NewDataReady);
    if ((!status) && (NewDataReady != 0)) {
      status = sensor_vl53l8cx_top.get_ranging_data(&Results);
      print_result(&Results);
    }

    delay(1000);


}

void print_result(VL53L8CX_ResultsData *Result)
{
  int8_t i, j, k;
  uint8_t l, zones_per_line;
  uint8_t number_of_zones = res;

  zones_per_line = (number_of_zones == 16) ? 4 : 8;

  Serial.print("Cell Format :\n\n");

  for (l = 0; l < VL53L8CX_NB_TARGET_PER_ZONE; l++) {
    snprintf(report, sizeof(report), " \033[38;5;10m%20s\033[0m : %20s\n", "Distance [mm]", "Status");
    Serial.print(report);

    if (EnableAmbient || EnableSignal) {
      snprintf(report, sizeof(report), " %20s : %20s\n", "Signal [kcps/spad]", "Ambient [kcps/spad]");
      Serial.print(report);
    }
  }

  Serial.print("\n\n");

  for (j = 0; j < number_of_zones; j += zones_per_line) {
    for (i = 0; i < zones_per_line; i++) {
      Serial.print(" -----------------");
    }
    Serial.print("\n");

    for (i = 0; i < zones_per_line; i++) {
      Serial.print("|                 ");
    }
    Serial.print("|\n");

    for (l = 0; l < VL53L8CX_NB_TARGET_PER_ZONE; l++) {
      // Print distance and status
      for (k = (zones_per_line - 1); k >= 0; k--) {
        if (Result->nb_target_detected[j + k] > 0) {
          snprintf(report, sizeof(report), "| \033[38;5;10m%5ld\033[0m  :  %5ld ",
                   (long)Result->distance_mm[(VL53L8CX_NB_TARGET_PER_ZONE * (j + k)) + l],
                   (long)Result->target_status[(VL53L8CX_NB_TARGET_PER_ZONE * (j + k)) + l]);
          Serial.print(report);
        } else {
          snprintf(report, sizeof(report), "| %5s  :  %5s ", "X", "X");
          Serial.print(report);
        }
      }
      Serial.print("|\n");

      if (EnableAmbient || EnableSignal) {
        // Print Signal and Ambient
        for (k = (zones_per_line - 1); k >= 0; k--) {
          if (Result->nb_target_detected[j + k] > 0) {
            if (EnableSignal) {
              snprintf(report, sizeof(report), "| %5ld  :  ", (long)Result->signal_per_spad[(VL53L8CX_NB_TARGET_PER_ZONE * (j + k)) + l]);
              Serial.print(report);
            } else {
              snprintf(report, sizeof(report), "| %5s  :  ", "X");
              Serial.print(report);
            }
            if (EnableAmbient) {
              snprintf(report, sizeof(report), "%5ld ", (long)Result->ambient_per_spad[j + k]);
              Serial.print(report);
            } else {
              snprintf(report, sizeof(report), "%5s ", "X");
              Serial.print(report);
            }
          } else {
            snprintf(report, sizeof(report), "| %5s  :  %5s ", "X", "X");
            Serial.print(report);
          }
        }
        Serial.print("|\n");
      }
    }
  }
  for (i = 0; i < zones_per_line; i++) {
    Serial.print(" -----------------");
  }
  Serial.print("\n");
}
