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
#define ETH_HEAT 6

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
  pinMode(ETH_HEAT, OUTPUT);
  digitalWrite(ETH_HEAT, HIGH);
  delay(2000);
  Serial.print("Starting\n");

  //Display
  Wire1.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false);
  Serial.print("Display done\n");

  //RGB
	strip.begin();
	strip.setLedColorData(0, 10, 0, 0);
	strip.setBrightness(10);
	strip.show();
  Serial.print("RGB done\n");

  //ETHANOL
  sensor.begin(); 
  Serial.print("ETH done\n");

  //I2C Sensors
  Wire.begin(Sensor_SDA,Sensor_SCL);
  sensor0.begin(0x48, Wire);  
  Serial.print("I2C done\n");

  //VL53L8
  sensor_vl53l8cx_top.set_i2c_address(0x29);
  sensor_vl53l8cx_top.begin();
  status = sensor_vl53l8cx_top.init();
  Serial.print("VL53 done\n");

  //BMP390
  BMP390.begin_I2C(0x76,&Wire);


  while (y < 64)
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print(F(" U-BOOTLOADER "));
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
  display.display();

  
  Serial.print("Setup done\n");

}

void loop() {
    display.clearDisplay();
    Serial.print("Loop Start\n");

    //ETHANOL
    sensorVal = sensor.read();
    Serial.print(F(" Ethanol raw: "));
    Serial.println(sensorVal);
    Ethanol = (sensorVal-0.55)/1.5;
    if(Ethanol<=0)
      Ethanol = 0;
    display.setCursor(0, 0);
    display.print(F(" Ethanol: "));
    display.println(Ethanol);

    Serial.print(F(" Ethanol: "));
    Serial.println(Ethanol);
    Serial.print("Ethanol END\n");
    
    //TMP102
    sensor0.oneShot(1); // Set One-Shot bit
    //while(sensor0.oneShot() == 0); // Wait for conversion to be ready

    Serial.print(F(" TempT: "));
    Serial.println(sensor0.readTempC());  // Print temperature reading
      
    display.print(F(" TempT: "));
    display.println(sensor0.readTempC());  // Print temperature reading

    //BMP390
    BMP390.performReading();
    float temp = BMP390.temperature;         // °C
    float pressure_hPa = BMP390.pressure / 100.0;  // hPa
    float altitude_m = BMP390.readAltitude(SEALEVELPRESSURE_HPA);  // Höhe über NN
    display.print(" TempB: ");
    display.print(temp); // Print with 2 decimal places
    display.println(" C");
    display.print(" PressB: ");
    display.print((pressure_hPa/1000),2); // Convert Pa to hPa (hectopascals)
    display.println(" Bar");
    display.print(" AltB: ");
    display.println(altitude_m);
    display.display();

    Serial.print(" TempB: ");
    Serial.print(temp); // Print with 2 decimal places
    Serial.println(" °C");
    Serial.print(" Pressure: ");
    Serial.print((pressure_hPa/1000),2); // Convert Pa to hPa (hectopascals)
    Serial.println(" Bar");

    //VL53L8C
    VL53L8CX_ResultsData Results;
    uint8_t NewDataReady = 0;
    
    Serial.print("Sensor Start\n");

      sensor_vl53l8cx_top.get_ranging_data(&Results);
      print_result(&Results);
    Serial.print("Sensor END\n");
    display.display();

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
