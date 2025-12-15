/*
 * Smart Greenhouse Module Firmware
 * Wemos D1 Mini (ESP8266)
 * 
 * Ports configuration:
 * - Port 0: Digital Output (D5)
 * - Port 1: Digital Output (D6)
 * - Port 2: Analog Input (A0)
 * - Port 3: Digital Input (D2)
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// WiFi Configuration - CHANGE THESE
const char* WIFI_SSID = "WIFI_SSID";
const char* WIFI_PASSWORD = "WIFI_PASSWORD";

// Module Configuration
const int MODULE_ID = 1;  // Change for each module

// Pin Definitions
#define OUTPUT_PIN_0 D5  // Digital output 0
#define OUTPUT_PIN_1 D6  // Digital output 1
#define ANALOG_PIN A0    // Analog input
#define DIGITAL_INPUT_PIN D2  // Digital input (OneWire for DS18B20)

// Port Types
#define PORT_TYPE_ANALOG_INPUT 0
#define PORT_TYPE_DIGITAL_INPUT 1
#define PORT_TYPE_OUTPUT 2

// Driver Types
#define DRIVER_TYPE_ANALOG_INPUT 0
#define DRIVER_TYPE_DIGITAL_INPUT 1
#define DRIVER_TYPE_OUTPUT 2

// Driver IDs
#define DRIVER_ID_PHOTORESISTOR 1
#define DRIVER_ID_SOIL_MOISTURE 2
#define DRIVER_ID_DS18B20 3
#define DRIVER_ID_DIGITAL_OUTPUT 4

// Port structure
struct Port {
  int id;
  int type;
  int pin;
  int boundDriverId;
  bool isBound;
};

// Driver structure
struct Driver {
  int id;
  String name;
  int type;
};

// Initialize ports
Port ports[] = {
  {0, PORT_TYPE_OUTPUT, OUTPUT_PIN_0, -1, false},
  {1, PORT_TYPE_OUTPUT, OUTPUT_PIN_1, -1, false},
  {2, PORT_TYPE_ANALOG_INPUT, ANALOG_PIN, -1, false},
  {3, PORT_TYPE_DIGITAL_INPUT, DIGITAL_INPUT_PIN, -1, false}
};

// Define available drivers
Driver drivers[] = {
  {DRIVER_ID_PHOTORESISTOR, "Photoresistor", DRIVER_TYPE_ANALOG_INPUT},
  {DRIVER_ID_SOIL_MOISTURE, "Soil Moisture Sensor", DRIVER_TYPE_ANALOG_INPUT},
  {DRIVER_ID_DS18B20, "DS18B20 Temperature", DRIVER_TYPE_DIGITAL_INPUT},
  {DRIVER_ID_DIGITAL_OUTPUT, "Digital Output", DRIVER_TYPE_OUTPUT}
};

const int NUM_PORTS = sizeof(ports) / sizeof(Port);
const int NUM_DRIVERS = sizeof(drivers) / sizeof(Driver);

// OneWire and DallasTemperature for DS18B20
OneWire oneWire(DIGITAL_INPUT_PIN);
DallasTemperature sensors(&oneWire);

// Web server
ESP8266WebServer server(80);

// Function prototypes
void setupWiFi();
void setupPins();
void handleRequest();
void handleInfo();
void handleDrivers();
void handlePorts();
void handlePortBind(int portId);
void handlePortBindGet(int portId);
void handlePortControl(int portId);
void handlePortControlPost(int portId);
void handleNotFound();
int extractPortId(String uri, String endpoint);
Port* getPortById(int portId);
Driver* getDriverById(int driverId);
bool isDriverCompatibleWithPort(int driverId, int portType);
float readAnalogSensor(int driverId, int pin);
float readDigitalSensor(int driverId, int pin);
void writeOutput(int pin, int level);

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("\n\nSmart Greenhouse Module Starting...");
  
  setupPins();
  setupWiFi();
  
  // Initialize DS18B20
  sensors.begin();
  
  // Setup REST API endpoints
  server.on("/info", HTTP_GET, handleInfo);
  server.on("/drivers", HTTP_GET, handleDrivers);
  server.on("/ports", HTTP_GET, handlePorts);
  
  // Use onNotFound to handle all requests and route manually
  server.onNotFound(handleRequest);
  
  server.begin();
  Serial.println("HTTP server started");
  Serial.print("Module ID: ");
  Serial.println(MODULE_ID);
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
}

void setupWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed!");
  }
}

void setupPins() {
  pinMode(OUTPUT_PIN_0, OUTPUT);
  pinMode(OUTPUT_PIN_1, OUTPUT);
  pinMode(DIGITAL_INPUT_PIN, INPUT);
  
  // Set outputs to LOW initially
  digitalWrite(OUTPUT_PIN_0, LOW);
  digitalWrite(OUTPUT_PIN_1, LOW);
  
  Serial.println("Pins configured");
}

// GET /info
void handleInfo() {
  StaticJsonDocument<128> doc;
  doc["module-id"] = MODULE_ID;
  
  String response;
  serializeJson(doc, response);
  
  server.send(200, "application/json", response);
}

// GET /drivers
void handleDrivers() {
  DynamicJsonDocument doc(1024);
  JsonArray driversArray = doc.createNestedArray("drivers");
  
  for (int i = 0; i < NUM_DRIVERS; i++) {
    JsonObject driver = driversArray.createNestedObject();
    driver["id"] = drivers[i].id;
    driver["name"] = drivers[i].name;
    driver["type"] = drivers[i].type;
  }
  
  String response;
  serializeJson(doc, response);
  
  server.send(200, "application/json", response);
}

// GET /ports
void handlePorts() {
  DynamicJsonDocument doc(512);
  JsonArray portsArray = doc.createNestedArray("ports");
  
  for (int i = 0; i < NUM_PORTS; i++) {
    JsonObject port = portsArray.createNestedObject();
    port["id"] = ports[i].id;
    port["type"] = ports[i].type;
  }
  
  String response;
  serializeJson(doc, response);
  
  server.send(200, "application/json", response);
}

// Main request handler - routes all dynamic endpoints
void handleRequest() {
  String uri = server.uri();
  HTTPMethod method = server.method();
  
  // Route to appropriate handler based on URI pattern
  if (uri == "/info" && method == HTTP_GET) {
    handleInfo();
  } else if (uri == "/drivers" && method == HTTP_GET) {
    handleDrivers();
  } else if (uri == "/ports" && method == HTTP_GET) {
    handlePorts();
  } else if (uri.startsWith("/ports/") && uri.endsWith("/bind")) {
    int portId = extractPortId(uri, "/bind");
    if (portId >= 0) {
      if (method == HTTP_PUT) {
        handlePortBind(portId);
      } else if (method == HTTP_GET) {
        handlePortBindGet(portId);
      } else {
        handleNotFound();
      }
    } else {
      handleNotFound();
    }
  } else if (uri.startsWith("/ports/") && uri.endsWith("/control")) {
    int portId = extractPortId(uri, "/control");
    if (portId >= 0) {
      if (method == HTTP_GET) {
        handlePortControl(portId);
      } else if (method == HTTP_POST) {
        handlePortControlPost(portId);
      } else {
        handleNotFound();
      }
    } else {
      handleNotFound();
    }
  } else {
    handleNotFound();
  }
}

// Extract port ID from URI like "/ports/3/bind" or "/ports/2/control"
int extractPortId(String uri, String endpoint) {
  // Find the position of "/ports/"
  int portsPos = uri.indexOf("/ports/");
  if (portsPos == -1) return -1;
  
  // Find the position of the endpoint
  int endpointPos = uri.indexOf(endpoint);
  if (endpointPos == -1) return -1;
  
  // Extract the number between "/ports/" and endpoint
  String portIdStr = uri.substring(portsPos + 7, endpointPos);
  
  // Validate that it's a number
  for (unsigned int i = 0; i < portIdStr.length(); i++) {
    if (!isDigit(portIdStr.charAt(i))) {
      return -1;
    }
  }
  
  return portIdStr.toInt();
}

// PUT /ports/{port}/bind
void handlePortBind(int portId) {
  Port* port = getPortById(portId);
  if (port == nullptr) {
    server.send(422, "application/json", "{\"error\":\"Port not found\"}");
    return;
  }

  // Parse request body
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }
  
  if (!doc.containsKey("driverId")) {
    server.send(400, "application/json", "{\"error\":\"Missing driverId\"}");
    return;
  }
  
  int driverId = doc["driverId"];
  Driver* driver = getDriverById(driverId);
  
  if (driver == nullptr) {
    server.send(422, "application/json", "{\"error\":\"Driver not found\"}");
    return;
  }
  
  // Check compatibility
  if (!isDriverCompatibleWithPort(driverId, port->type)) {
    server.send(409, "application/json", "{\"error\":\"Driver incompatible with port type\"}");
    return;
  }
  
  // Bind driver to port
  port->boundDriverId = driverId;
  port->isBound = true;
  
  Serial.print("Port ");
  Serial.print(portId);
  Serial.print(" bound to driver ");
  Serial.println(driverId);
  
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// GET /ports/{port}/bind
void handlePortBindGet(int portId) {
  Port* port = getPortById(portId);
  if (port == nullptr) {
    server.send(422, "application/json", "{\"error\":\"Port not found\"}");
    return;
  }
  
  if (!port->isBound) {
    server.send(400, "application/json", "{\"error\":\"Port not bound to driver\"}");
    return;
  }
  
  StaticJsonDocument<128> doc;
  doc["driverId"] = port->boundDriverId;
  
  String response;
  serializeJson(doc, response);
  
  server.send(200, "application/json", response);
}

// GET /ports/{port}/control
void handlePortControl(int portId) {
  Port* port = getPortById(portId);
  if (port == nullptr) {
    server.send(422, "application/json", "{\"error\":\"Port not found\"}");
    return;
  }
  
  if (!port->isBound) {
    server.send(400, "application/json", "{\"error\":\"Port not bound to driver\"}");
    return;
  }
  
  // Check if port supports reading (inputs only)
  if (port->type == PORT_TYPE_OUTPUT) {
    server.send(405, "application/json", "{\"error\":\"Method not supported\"}");
    return;
  }
  
  float value = 0.0;
  
  // Read sensor based on type
  if (port->type == PORT_TYPE_ANALOG_INPUT) {
    value = readAnalogSensor(port->boundDriverId, port->pin);
  } else if (port->type == PORT_TYPE_DIGITAL_INPUT) {
    value = readDigitalSensor(port->boundDriverId, port->pin);
  }
  
  StaticJsonDocument<128> doc;
  doc["value"] = value;
  
  String response;
  serializeJson(doc, response);
  
  server.send(200, "application/json", response);
}

// POST /ports/{port}/control
void handlePortControlPost(int portId) {
  Port* port = getPortById(portId);
  if (port == nullptr) {
    server.send(422, "application/json", "{\"error\":\"Port not found\"}");
    return;
  }
  
  if (!port->isBound) {
    server.send(400, "application/json", "{\"error\":\"Port not bound to driver\"}");
    return;
  }
  
  // Check if port supports writing (outputs only)
  if (port->type != PORT_TYPE_OUTPUT) {
    server.send(405, "application/json", "{\"error\":\"Method not supported\"}");
    return;
  }
  
  // Parse request body
  DynamicJsonDocument doc(256);
  DeserializationError error = deserializeJson(doc, server.arg("plain"));
  
  if (error || !doc.containsKey("level")) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON or missing level\"}");
    return;
  }
  
  int level = doc["level"];
  
  if (level < 0 || level > 255) {
    server.send(400, "application/json", "{\"error\":\"Level must be between 0 and 255\"}");
    return;
  }
  
  // Write to output (digital - convert level to HIGH/LOW)
  writeOutput(port->pin, level);
  
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void handleNotFound() {
  server.send(404, "application/json", "{\"error\":\"Not found\"}");
}

// Helper functions

Port* getPortById(int portId) {
  for (int i = 0; i < NUM_PORTS; i++) {
    if (ports[i].id == portId) {
      return &ports[i];
    }
  }
  return nullptr;
}

Driver* getDriverById(int driverId) {
  for (int i = 0; i < NUM_DRIVERS; i++) {
    if (drivers[i].id == driverId) {
      return &drivers[i];
    }
  }
  return nullptr;
}

bool isDriverCompatibleWithPort(int driverId, int portType) {
  Driver* driver = getDriverById(driverId);
  if (driver == nullptr) return false;
  
  return driver->type == portType;
}

float readAnalogSensor(int driverId, int pin) {
  int rawValue = analogRead(pin);
  float result = 0.0;
  
  switch (driverId) {
    case DRIVER_ID_PHOTORESISTOR:
      result = map(rawValue, 0, 1023, 100, 0);
      break;
      
    case DRIVER_ID_SOIL_MOISTURE:
      result = map(rawValue, 0, 1023, 100, 0);
      break;
      
    default:
      result = rawValue;
  }
  
  Serial.print("Analog read (driver ");
  Serial.print(driverId);
  Serial.print("): ");
  Serial.println(result);
  
  return result;
}

float readDigitalSensor(int driverId, int pin) {
  float result = 0.0;
  
  switch (driverId) {
    case DRIVER_ID_DS18B20:
      sensors.requestTemperatures();
      result = sensors.getTempCByIndex(0);
      
      // Check for sensor error
      if (result == DEVICE_DISCONNECTED_C) {
        Serial.println("DS18B20 sensor error!");
        result = -127.0;  // Error value
      }
      break;
      
    default:
      result = digitalRead(pin);
  }
  
  Serial.print("Digital read (driver ");
  Serial.print(driverId);
  Serial.print("): ");
  Serial.println(result);
  
  return result;
}

void writeOutput(int pin, int level) {  
  analogWrite(pin, level);
  
  Serial.print("Output pin ");
  Serial.print(pin);
  Serial.print(" set to ");
  Serial.println(level);
}


