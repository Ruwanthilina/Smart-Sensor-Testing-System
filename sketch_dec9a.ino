#include <WiFi.h>
#include <WebSocketsServer.h>
#include <DHT.h>
#include <Wire.h>
#include <VL53L0X.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "****";
const char* password = "****";

// WebSocket server
WebSocketsServer webSocket = WebSocketsServer(81);

// DHT Sensor setup
#define DHTPIN 18
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// VL53L0X ToF sensor setup
VL53L0X lox;

// Pin Definitions
#define LDR_PIN 34
#define ULTRASONIC_TRIG_PIN 27
#define ULTRASONIC_ECHO_PIN 14
#define RED_LED_PIN 32
#define GREEN_LED_PIN 33
#define BLUE_LED_PIN 26
#define IR_PIN 4
#define BUZZER_PIN 12

// I2C Pins (ESP32 default)
#define SDA_PIN 21
#define SCL_PIN 22

// Variables for sensor readings
float temperature = 0.0; 
float humidity = 0.0;
int lightLevel = 0;
int proximity = 0;
unsigned long lastReadTime = 0;
const unsigned long READ_INTERVAL = 2000; // 2 seconds

// Sensor activation states
bool activateTemperature = false;
bool activateHumidity = false;
bool activateTof = false;
bool activateLdr = false;
bool activateUltrasonic = false;
bool activateBuzzer = false;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // Initialize pin modes
  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(IR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Initialize WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // Initialize DHT sensor
  dht.begin();

  // Initialize VL53L0X sensor
  if (!lox.init()) {
    Serial.println("Failed to initialize VL53L0X sensor!");
    while (1);
  }
  lox.setTimeout(500);
  lox.startContinuous(); // Start continuous ranging
  Serial.println("VL53L0X initialized.");
}

void loop() {
  // Handle WebSocket connections
  webSocket.loop();

  // Read sensors at specified interval
  unsigned long currentTime = millis();
  if (currentTime - lastReadTime >= READ_INTERVAL) {
    lastReadTime = currentTime;

    // Read active sensors
    if (activateTemperature) {
      temperature = dht.readTemperature();
      sendSensorData("temperature", String(temperature));
    }
    
    if (activateHumidity) {
      humidity = dht.readHumidity();
      sendSensorData("humidity", String(humidity));
    }
    
    if (activateTof) {
      proximity = lox.readRangeContinuousMillimeters();
      sendSensorData("tof", String(proximity));
    }
    
    if (activateLdr) {
      lightLevel = analogRead(LDR_PIN);
      sendSensorData("ldr", String(lightLevel));
    }
    
    if (activateUltrasonic) {
      long distance = readUltrasonic();
      sendSensorData("ultrasonic", String(distance));
    }
  }

  // Handle buzzer activation
  digitalWrite(BUZZER_PIN, activateBuzzer ? HIGH : LOW);
}

// Toggle LED state
void toggleLED(int pin) {
  digitalWrite(pin, !digitalRead(pin));
}

// WebSocket event handler
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    // Parse incoming JSON message
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
      return;
    }

    String sensor = doc["sensor"].as<String>();
    String action = doc["action"] | "toggle";

    // Handle LED toggles
    if (sensor == "red") {
      toggleLED(RED_LED_PIN);
    } else if (sensor == "green") {
      toggleLED(GREEN_LED_PIN);
    } else if (sensor == "blue") {
      toggleLED(BLUE_LED_PIN);
    } 
    // Handle buzzer
    else if (sensor == "buzzer") {
      activateBuzzer = (action == "on");
    }

    // Handle sensor activation/deactivation
    if (action == "on") {
      if (sensor == "temperature") activateTemperature = true;
      else if (sensor == "humidity") activateHumidity = true;
      else if (sensor == "tof") activateTof = true;
      else if (sensor == "ldr") activateLdr = true;
      else if (sensor == "ultrasonic") activateUltrasonic = true;
    } else if (action == "off") {
      if (sensor == "temperature") activateTemperature = false;
      else if (sensor == "humidity") activateHumidity = false;
      else if (sensor == "tof") activateTof = false;
      else if (sensor == "ldr") activateLdr = false;
      else if (sensor == "ultrasonic") activateUltrasonic = false;
    }
  }
}

// Send sensor data via WebSocket
void sendSensorData(const String& sensor, const String& value) {
  DynamicJsonDocument doc(512);
  doc["sensor"] = sensor;
  doc["value"] = value;

  String json;
  serializeJson(doc, json);
  webSocket.broadcastTXT(json);
}

// Read distance using Ultrasonic sensor
long readUltrasonic() {
  // Trigger ultrasonic pulse
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

  // Measure pulse duration and calculate distance
  long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH);
  long distance = duration * 0.034 / 2; // Distance in cm
  return distance;
}