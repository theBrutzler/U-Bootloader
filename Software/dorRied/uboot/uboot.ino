#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SparkFunTMP102.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP3XX.h>
#include <vl53l8cx.h>
#include "Freenove_WS2812_Lib_for_ESP32.h"
#include "SP3SAQ2.h"
#include <block.h>

// Display Configuration
#define OLED_SDA 17
#define OLED_SCL 18
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);

// Brutzel Kurzschluss BlockIntro
int y = 0;
int x = 0;

// I2C Sensors Configuration
#define Sensor_SDA 39
#define Sensor_SCL 40

// RGB LED Configuration
#define LEDS_COUNT 1
#define LEDS_PIN 42
#define CHANNEL 0
Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL, TYPE_GRB);

// Ethanol Sensor Configuration
#define ADC_BIT_RESU 12
#define ETHANOL_PIN 4
#define ETH_HEAT 6
float sensorVal, ethanol_ppm, ethanol_old , ethanol_avg;
float ethanol[100];
SP3SAQ2 sensor(ADC_BIT_RESU, ETHANOL_PIN);

// TMP102 Temperature Sensor
TMP102 sensor_tmp;

// BMP390 Pressure Sensor
#define SEALEVELPRESSURE_HPA 1024.25  // fits best for me -- checked with https://whatismyelevation.com/ and https://www.mide.com/air-pressure-at-altitude-calculator
Adafruit_BMP3XX BMP390;

// VL53L8CX TOF Sensor Configuration - https://www.st.com/en/imaging-and-photonics-solutions/vl53l8cx.html - nice one - now I understand the price :)
#define LPN_PIN -1
#define PWREN_PIN -1
VL53L8CX sensor_vl53l8cx(&Wire, LPN_PIN);
uint8_t status;
uint8_t res = VL53L8CX_RESOLUTION_8X8;
bool EnableAmbient = false;
bool EnableSignal = false;
int distances[8][8];
int prevDistances[8][8];
int centerDistance = 0;
const int motionThreshold = 70;  // give it bit more tolerance otherwise there are too much "false" detections
bool motionDetected = false;

// WiFi AP Configuration
const char* ssid = "Titan";
const char* password = "titan123";

// Web Server and WebSocket
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Sensor Data Variables
float temperature = 0;
float temperature_bosch = 0;
float pressure = 0;
float altitude = 0;
unsigned long lastSensorUpdate = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastWebUpdate = 0;
const unsigned long SENSOR_UPDATE_INTERVAL = 500;
const unsigned long DISPLAY_UPDATE_INTERVAL = 100;
const unsigned long WEB_UPDATE_INTERVAL = 50;

// HTML Web Page with Twitch-themed colors
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Titan Sensor Dashboard</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #0e0e10;
            color: #efeff1;
            padding: 20px;
            line-height: 1.6;
        }
        
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        
        h1 {
            color: #9147ff;
            text-align: center;
            margin-bottom: 30px;
            font-size: 2.5em;
            text-shadow: 0 0 20px rgba(145, 71, 255, 0.5);
        }
        
        .dashboard {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        
        .sensor-card {
            background: #18181b;
            border: 1px solid #2d2d30;
            border-radius: 8px;
            padding: 20px;
            transition: all 0.3s ease;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.3);
        }
        
        .sensor-card:hover {
            transform: translateY(-5px);
            border-color: #9147ff;
            box-shadow: 0 6px 20px rgba(145, 71, 255, 0.2);
        }
        
        .sensor-title {
            color: #9147ff;
            font-size: 1.2em;
            margin-bottom: 15px;
            font-weight: 600;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        
        .sensor-value {
            font-size: 2em;
            font-weight: bold;
            color: #efeff1;
            margin: 10px 0;
            text-align: center;
            padding: 15px;
            background: #0e0e10;
            border-radius: 6px;
            border: 1px solid #2d2d30;
        }
        
        .unit {
            font-size: 0.5em;
            color: #adadb8;
            margin-left: 5px;
        }
        
        .tof-container {
            grid-column: span 2;
            background: #18181b;
            border: 1px solid #2d2d30;
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.3);
            margin-top: 20px;
        }
        
        #tofCanvas {
            width: 100%;
            max-width: 600px;
            margin: 20px auto;
            display: block;
            border: 2px solid #9147ff;
            border-radius: 8px;
            background: #0e0e10;
            box-shadow: 0 0 30px rgba(145, 71, 255, 0.3);
        }
        
        .status {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background: #00f593;
            animation: pulse 2s infinite;
        }
        
        .status.offline {
            background: #ff4444;
            animation: none;
        }
        
        @keyframes pulse {
            0% { box-shadow: 0 0 0 0 rgba(0, 245, 147, 0.7); }
            70% { box-shadow: 0 0 0 10px rgba(0, 245, 147, 0); }
            100% { box-shadow: 0 0 0 0 rgba(0, 245, 147, 0); }
        }
        
        .motion-indicator {
            text-align: center;
            padding: 10px;
            margin-top: 10px;
            background: #0e0e10;
            border-radius: 6px;
            border: 1px solid #2d2d30;
            transition: all 0.3s ease;
        }
        
        .motion-indicator.active {
            background: #9147ff;
            color: white;
            border-color: #9147ff;
            animation: flash 0.5s;
        }
        
        @keyframes flash {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        
        .legend {
            display: flex;
            justify-content: center;
            gap: 30px;
            margin-top: 15px;
            flex-wrap: wrap;
        }
        
        .legend-item {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 0.9em;
            color: #adadb8;
        }
        
        .legend-color {
            width: 20px;
            height: 20px;
            border-radius: 4px;
            border: 1px solid #2d2d30;
        }
        
        .connection-status {
            text-align: center;
            margin-bottom: 20px;
            padding: 10px;
            background: #18181b;
            border-radius: 8px;
            border: 1px solid #2d2d30;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Titan Sensor Dashboard</h1>
        
        <div class="connection-status">
            <span class="status" id="connectionStatus"></span>
            <span id="connectionText">Connected</span>
        </div>
        
        <div class="dashboard">
            <div class="sensor-card">
                <div class="sensor-title">
                    🌡️ Temperature Texas (TI)
                </div>
                <div class="sensor-value" id="temperature">
                    --<span class="unit">°C</span>
                </div>
            </div>
            
            <div class="sensor-card">
                <div class="sensor-title">
                    🌡️ Temperature Bosch
                </div>
                <div class="sensor-value" id="temperature_bosch">
                    --<span class="unit">°C</span>
                </div>
            </div>

            <div class="sensor-card">
                <div class="sensor-title">
                    ⛰️ Altitude
                </div>
                <div class="sensor-value" id="altitude">
                    --<span class="unit">m</span>
                </div>
            </div>
            
            <div class="sensor-card">
                <div class="sensor-title">
                    🌪️ Pressure
                </div>
                <div class="sensor-value" id="pressure">
                    --<span class="unit">bar</span>
                </div>
            </div>

            <div class="sensor-card">
                <div class="sensor-title">
                    🔬 Ethanol
                </div>
                <div class="sensor-value" id="ethanol">
                    --<span class="unit">ppm</span>
                </div>
            </div>
        </div>
        
        <div class="tof-container">
            <div class="sensor-title">
                VL53L8CX Sensor
            </div>
            <canvas id="tofCanvas" width="400" height="400"></canvas>
            <div class="motion-indicator" id="motionIndicator">
                No Motion Detected
            </div>
            <div class="legend">
                <div class="legend-item">
                    <div class="legend-color" style="background: #0000ff;"></div>
                    <span>Near (< 500mm)</span>
                </div>
                <div class="legend-item">
                    <div class="legend-color" style="background: #00ff00;"></div>
                    <span>Medium (500-1500mm)</span>
                </div>
                <div class="legend-item">
                    <div class="legend-color" style="background: #ff0000;"></div>
                    <span>Far (> 1500mm)</span>
                </div>
            </div>
        </div>
    </div>
    
    <script>
        let ws;
        const canvas = document.getElementById('tofCanvas');
        const ctx = canvas.getContext('2d');
        const cellSize = canvas.width / 8;
        
        function connectWebSocket() {
            ws = new WebSocket('ws://' + window.location.hostname + ':81/');
            
            ws.onopen = function() {
                console.log('WebSocket connected');
                document.getElementById('connectionStatus').classList.remove('offline');
                document.getElementById('connectionText').textContent = 'Connected';
            };
            
            ws.onclose = function() {
                console.log('WebSocket disconnected');
                document.getElementById('connectionStatus').classList.add('offline');
                document.getElementById('connectionText').textContent = 'Disconnected - Reconnecting...';
                setTimeout(connectWebSocket, 3000);
            };
            
            ws.onerror = function(error) {
                console.error('WebSocket error:', error);
            };
            
            ws.onmessage = function(event) {
                try {
                    const data = JSON.parse(event.data);
                    updateSensorData(data);
                } catch(e) {
                    console.error('Failed to parse data:', e);
                }
            };
        }
        
        function updateSensorData(data) {
            if (data.temperature !== undefined) {
                document.getElementById('temperature').innerHTML = 
                    data.temperature.toFixed(1) + '<span class="unit">°C</span>';
            }
            if (data.temperature_bosch !== undefined) {
                document.getElementById('temperature_bosch').innerHTML = 
                    data.temperature_bosch.toFixed(1) + '<span class="unit">°C</span>';
            }
            if (data.ethanol !== undefined) {
                document.getElementById('ethanol').innerHTML = 
                    data.ethanol.toFixed(2) + '<span class="unit">ppm</span>';
            }
            if (data.bac !== undefined) {
                document.getElementById('bac').innerHTML = 
                    data.bac.toFixed(2) + '<span class="unit">‰</span>';
            }
            if (data.pressure !== undefined) {
                document.getElementById('pressure').innerHTML = 
                    data.pressure.toFixed(1) + '<span class="unit">bar</span>';
            }
            if (data.altitude !== undefined) {
                document.getElementById('altitude').innerHTML = 
                    data.altitude.toFixed(1) + '<span class="unit">m</span>';
            }
            if (data.tof !== undefined) {
                drawTOFGrid(data.tof);
            }
            if (data.motion !== undefined) {
                const indicator = document.getElementById('motionIndicator');
                if (data.motion) {
                    indicator.textContent = 'Motion Detected!';
                    indicator.classList.add('active');
                } else {
                    indicator.textContent = 'No Motion Detected';
                    indicator.classList.remove('active');
                }
            }
        }
        
        function drawTOFGrid(distances) {
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            
            for (let y = 0; y < 8; y++) {
                for (let x = 0; x < 8; x++) {
                    const distance = distances[y][x];
                    const color = getColorForDistance(distance);
                    
                    ctx.fillStyle = color;
                    ctx.fillRect(x * cellSize, y * cellSize, cellSize - 2, cellSize - 2);
                    
                    ctx.fillStyle = '#ffffff';
                    ctx.font = '12px Arial';
                    ctx.textAlign = 'center';
                    ctx.fillText(distance + 'mm', x * cellSize + cellSize/2, y * cellSize + cellSize/2);
                }
            }
        }
        
        function getColorForDistance(distance) {
            if (distance < 500) {
                return `rgba(0, 0, 255, ${1 - (distance / 500) * 0.5})`;
            } else if (distance < 1500) {
                return `rgba(0, 255, 0, ${1 - ((distance - 500) / 1000) * 0.5})`;
            } else {
                return `rgba(255, 0, 0, 0.5)`;
            }
        }
        
        connectWebSocket();
    </script>
</body>
</html>
)rawliteral";



void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("Starting Titan System...");

  // Initialize Ethanol heater pin
  pinMode(ETH_HEAT, OUTPUT);
  digitalWrite(ETH_HEAT, HIGH);

  // Initialize Display I2C (Wire1)
  Wire1.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false)) {
    Serial.println("SSD1306 allocation failed");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Titan System");
    display.println("Initializing...");
    display.display();
    Serial.println("Display done");
  }

  // Initialize RGB LED
  strip.begin();
  strip.setBrightness(10);
  setStatusLED(10, 0, 0);  // Red during initialization
  strip.show();
  Serial.println("RGB done");

  // Initialize Ethanol Sensor
  sensor.begin();
  Serial.println("ETH done");

  // Initialize I2C for sensors
  Wire.begin(Sensor_SDA, Sensor_SCL);
  Serial.println("I2C Sensors bus initialized");

  // Initialize TMP102
  sensor_tmp.begin(0x48, Wire);
  Serial.println("TMP102 initialized at 0x48");

  // Initialize VL53L8CX
  Serial.println("Initializing VL53L8CX...");
  sensor_vl53l8cx.set_i2c_address(0x29);
  sensor_vl53l8cx.begin();
  status = sensor_vl53l8cx.init();
  if (status) {
    Serial.println("VL53L8CX init failed!");
    while (1)
      ;
  }
  sensor_vl53l8cx.set_resolution(VL53L8CX_RESOLUTION_8X8);
  sensor_vl53l8cx.set_ranging_frequency_hz(15);
  sensor_vl53l8cx.set_ranging_mode(VL53L8CX_RANGING_MODE_CONTINUOUS);  // @Brutzi: basically thats the main line you missed ;)
  sensor_vl53l8cx.start_ranging();
  Serial.println("VL53L8CX initialized successfully");

  // Initialize BMP390
  BMP390.begin_I2C(0x76, &Wire);

  // Setup WiFi Access Point
  Serial.print("Setting up Access Point...");
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Setup Web Server Route
  server.on("/", []() {
    server.send(200, "text/html", index_html);
  });

  server.begin();
  Serial.println("HTTP server started");

  // Start WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket server started on port 81");

  while (y < 64) {
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

  // Set LED to green when ready
  setStatusLED(0, 10, 0);

  Serial.println("Setup done");
}

void loop() {
  server.handleClient();
  webSocket.loop();

  unsigned long currentMillis = millis();

  // Update sensors
  if (currentMillis - lastSensorUpdate >= SENSOR_UPDATE_INTERVAL) {
    lastSensorUpdate = currentMillis;
    updateSensors();
  }

  // Update display
  if (currentMillis - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    lastDisplayUpdate = currentMillis;
    updateOLED();
  }

  // Send data to web clients
  if (currentMillis - lastWebUpdate >= WEB_UPDATE_INTERVAL) {
    lastWebUpdate = currentMillis;
    sendSensorData();
  }
}

void updateSensors() {
  static bool tmpAvailable = true;
  if (tmpAvailable) {
    sensor_tmp.oneShot(1);
    float newTemp = sensor_tmp.readTempC();
    if (newTemp > -40 && newTemp < 125) {
      temperature = newTemp;
    } else {
      tmpAvailable = false;
    }
  }
  if(x<100){
    sensorVal = sensor.read();
    ethanol[x] = sensorVal;
  }
  else{
    x = 0;
  }
  for(int i=0;i<100;i++){
    ethanol_avg+=ethanol[i];
  }
  ethanol_avg=ethanol_avg/100;
 

  // Ethanol calculation - must be tested - not linear - https://www.saiyasensor.com/res/soft/2024/22e2933ca620c575.pdf

  ethanol_ppm = ((sensorVal - (ethanol_avg))+0.15) / 1.5;
  ethanol_avg = sensorVal;
  if (ethanol_ppm < 0) ethanol_ppm = 0;
  if (ethanol_ppm>ethanol_old) ethanol_old = ethanol_ppm;

  // Read BMP390
  BMP390.performReading();
  float pressure_hPa = BMP390.pressure / 100.0;  // hPa;
  pressure = (pressure_hPa/1000),2;            //  hPa to bar
  altitude = BMP390.readAltitude(SEALEVELPRESSURE_HPA);
  temperature_bosch = BMP390.temperature;     // when I would divide by 1.7 it roughly fits my room temperature - but yes the board is "warm" thats for sure so measured temps should fit - TI and Bosch - same story

  // Read VL53L8CX
  updateVL53L8CXSensor();
}

void updateVL53L8CXSensor() {
  VL53L8CX_ResultsData results;
  uint8_t isReady = 0;

  sensor_vl53l8cx.check_data_ready(&isReady);

  if (isReady) {
    sensor_vl53l8cx.get_ranging_data(&results);

    // Store previous distances for motion detection
    memcpy(prevDistances, distances, sizeof(distances));

    // Update current distances
    int index = 0;
    motionDetected = false;

    for (int y = 0; y < 8; y++) {
      for (int x = 0; x < 8; x++) {
        distances[y][x] = results.distance_mm[index];

        // Check for motion
        if (abs(distances[y][x] - prevDistances[y][x]) > motionThreshold) {
          motionDetected = true;
        }
        index++;
      }
    }

    // Get center distance (average of center 4 zones)
    centerDistance = (distances[3][3] + distances[3][4] + distances[4][3] + distances[4][4]) / 4;

    // Update LED based on center distance
    updateLEDByDistance(centerDistance);
  }
}

void updateLEDByDistance(int distance) {
  if (distance < 500) {
    setStatusLED(0, 0, 255);  // Blue for close
  } else if (distance < 1000) {
    setStatusLED(0, 255, 0);  // Green for medium
  } else if (distance < 1500) {
    setStatusLED(255, 255, 0);  // Yellow for far
  } else {
    setStatusLED(255, 0, 0);  // Red for very far
  }
}

void setStatusLED(uint8_t r, uint8_t g, uint8_t b) {
  strip.setLedColorData(0, r, g, b);
  strip.show();
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("== TITAN SENSORS ==");

  display.setCursor(0, 16);
  display.print("Temp: ");
  display.print(temperature, 1);
  display.println(" C");

  display.setCursor(0, 26);
  display.print("Ethanol: ");
  display.print(sensorVal, 2);
  display.println(" ppm");

  display.print("Ethanol: ");
  display.print(ethanol_old, 2);
  display.println(" max");
  

  //display.setCursor(0, 36);
  //display.print("Pressure: ");
  //display.print(pressure, 2);
  //display.println(" bar");

  display.setCursor(0, 46);
  display.print("Alt: ");
  display.print(altitude, 1);
  display.println(" m");

  display.setCursor(0, 56);
  display.print("Dist: ");
  display.print(centerDistance);
  display.print("mm ");

  if (motionDetected) {
    display.print("[MOTION]");
  }

  display.display();
}

void updateDisplay(String line1, String line2, String line3) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println(line1);
  display.setCursor(0, 20);
  display.println(line2);
  display.setCursor(0, 40);
  display.println(line3);

  display.display();
}

void sendSensorData() {
  if (webSocket.connectedClients() > 0) {
    StaticJsonDocument<1024> doc;

    doc["temperature"] = temperature;
    doc["temperature_bosch"] = temperature_bosch;
    doc["ethanol"] = sensorVal;
    doc["pressure"] = pressure;
    doc["altitude"] = altitude;
    doc["motion"] = motionDetected;

    // Add VL53L8CX data
    JsonArray tofArray = doc.createNestedArray("tof");
    for (int y = 0; y < 8; y++) {
      JsonArray row = tofArray.createNestedArray();
      for (int x = 0; x < 8; x++) {
        row.add(distances[y][x]);
      }
    }

    String jsonString;
    serializeJson(doc, jsonString);
    webSocket.broadcastTXT(jsonString);
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
      }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] Received text: %s\n", num, payload);
      break;
    case WStype_BIN:
      Serial.printf("[%u] Received binary length: %u\n", num, length);
      break;
  }
}