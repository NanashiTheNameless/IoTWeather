// Copyright: NamelessNanashi 2024
// License: https://oql.avris.it/license/v1.3?c=NamelessNanashi%7Chttps%3A%2F%2FNamelessNanashi.dev%2F
// LicenseID: LicenseRef-OQL-1.3

#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_SHT4x.h>
#include <Adafruit_Sensor.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <Adafruit_NeoPixel.h>

// ===================== User-Defined Timeouts and Delays =====================
// Time between sensor data refreshes (in milliseconds)
const unsigned long SENSOR_READ_INTERVAL = 10000;  // Update sensor data every 10 seconds

// Timeout for HTTP client connection (in milliseconds)
const unsigned long CLIENT_TIMEOUT = 150000;  // Disconnect HTTP client after 150 seconds of inactivity

// Delay before restarting ESP on failure (in milliseconds)
const unsigned long RESTART_DELAY = 15000;  // Wait 15 seconds before restarting

// Define the HOSTNAME for the device
const String HOSTNAME = "WeatherStation";

// Define Number of tries for reconnect
const int maxSensorInitAttempts = 10;
// ===================== End of User-Defined Timeouts and Delays =====================

// Define I2C sensor
// Adafruit_AHTX0 tempsens;
Adafruit_SHT4x tempsens;

// Debug flag
bool debug = false;  // Debug is disabled by default
bool debug_set = false;  // Used for manual override

// NeoPixel configuration
// Set POWER_NEOPIXEL to NEOPIXEL_POWER if it exists, otherwise use 1
#ifdef NEOPIXEL_POWER
  #define POWER_NEOPIXEL NEOPIXEL_POWER
#else
  #define POWER_NEOPIXEL 1
#endif

// Set NEOPIXEL_PIN to PIN_NEOPIXEL if it exists, otherwise use 2
#ifdef PIN_NEOPIXEL
  #define NEOPIXEL_PIN PIN_NEOPIXEL
#else
  #define NEOPIXEL_PIN 2
#endif

// Define the network name
const String AP_SSID = HOSTNAME + "AP";

// NeoPixel configuration
#define NUMPIXELS 1  // Usually 1 if it's just one NeoPixel

// Instantiate the NeoPixel object
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Pin for resetting Wi-Fi credentials (usually the boot button on the ESP32)
#define WIFI_RST 0 // For esp32:esp32:adafruit_feather_esp32s2
// #define WIFI_RST D9 // For esp32:esp32:XIAO_ESP32C3
// #define WIFI_RST 9 // For esp32:esp32:adafruit_qtpy_esp32c3

// Pin for handling debug mode
// #define DEBUG_PIN RX // For esp32:esp32:XIAO_ESP32C3
#define DEBUG_PIN T6 // For esp32:esp32:adafruit_feather_esp32s2

// Variables to store Wi-Fi credentials
String ssid;
String password;

// Wi-Fi server on port 80 (HTTP)
WiFiServer server(80);

// Timing variables for handling HTTP client timeouts
unsigned long currentTime;
unsigned long previousTime = 0;
unsigned long lastSensorReadTime = 0;
unsigned long lastPrintTime = 0;

// Define data variables as floats
float temperatureC = 0.0;
float humidityValue = 0.0;

// Variable for sensor initialization
bool sensorInitialized = false;

// Variable for sensor reading error handling
bool validReading = false;

// Debug macros that check the debug variable at runtime
#define DEBUG_PRINT(x)     do { if (debug || debug_set) Serial.print(x); } while (0)
#define DEBUG_PRINTLN(x)   do { if (debug || debug_set) Serial.println(x); } while (0)
#define DEBUG_PRINTF(...)  do { if (debug || debug_set) Serial.printf(__VA_ARGS__); } while (0)

// Instantiate WiFiManager object
WiFiManager wifiManager;

// Flag for saving data
bool shouldSaveConfig = false;

// Callback notifying the library of the need to save config
void saveConfigCallback () {
  DEBUG_PRINTLN("Should save config");
  shouldSaveConfig = true;
}

// Function to set NeoPixel color
void setNeoPixelColor(uint8_t red, uint8_t green, uint8_t blue) {
  pixels.setPixelColor(0, pixels.Color(red, green, blue));
  pixels.show();
}

void setup() {
  // Set the Wi-Fi reset pin as input
  pinMode(WIFI_RST, INPUT_PULLUP);

  // Set the debug enable pins
  pinMode(DEBUG_PIN, INPUT_PULLUP);

  // Read the state the pin
  int pinState = digitalRead(DEBUG_PIN);

  // Small delay
  delay(10);
  
  // Start the serial communication for debugging
  Serial.begin(115200);

  delay(1500);  // Wait for serial initialization

  // Handlde Debug state
  if (pinState == LOW) {
    debug_set = true;
    Serial.println("Debug Pin is LOW.");
  } else {
    debug_set = false;
    Serial.println("Debug Pin is HIGH.");
  }

  // Initialize NeoPixel pins and set initial color
  pinMode(POWER_NEOPIXEL, OUTPUT);
  pinMode(NEOPIXEL_PIN, OUTPUT);
  digitalWrite(POWER_NEOPIXEL, (debug || debug_set) ? HIGH : LOW);
  pixels.begin();
  setNeoPixelColor(0, 0, 0);  // Turn off NeoPixel at the start

  DEBUG_PRINTLN("Debug mode enabled.");

  if (debug || debug_set) {
    setNeoPixelColor(0, 0, 1);  // Set NeoPixel to blue for startup and Wi-Fi connection
  }

  // Set precision for temperature sensor
  tempsens.setPrecision(SHT4X_HIGH_PRECISION); // ("SHT4X_LOW_PRECISION", "SHT4X_MED_PRECISION", "SHT4X_HIGH_PRECISION")
  tempsens.setHeater(SHT4X_NO_HEATER);

  // WiFiManager setup
  wifiManager.setClass("invert");
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setConfigPortalTimeout(360);  // 360 seconds for config portal
  wifiManager.setAPStaticIPConfig(IPAddress(10, 0, 1, 1), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));

  // Attempt to auto-connect Wi-Fi using WiFiManager
  if (!wifiManager.autoConnect(AP_SSID.c_str(), HOSTNAME.c_str())) {
    DEBUG_PRINTLN("Failed to connect to Wi-Fi.");
    if (debug || debug_set) {
      setNeoPixelColor(1, 0, 0);  // Set NeoPixel to red for failure
    }
    delay(RESTART_DELAY);
    ESP.restart();
  }

  DEBUG_PRINTLN("Wi-Fi Connected");
  DEBUG_PRINT("IP Address: ");
  DEBUG_PRINTLN(WiFi.localIP());
  
  if (debug || debug_set) {
    setNeoPixelColor(0, 1, 0);  // Set NeoPixel to green for successful Wi-Fi connection
  }

  // Initialize mDNS
  if (!MDNS.begin(HOSTNAME.c_str())) {
    DEBUG_PRINTLN("Error setting up mDNS responder!");
    if (debug || debug_set) {
      setNeoPixelColor(1, 0, 0);  // Set NeoPixel to red for failure
    }
  } else {
    DEBUG_PRINTLN("mDNS responder started.");
  }

  // Initialize the Temp/Humidity sensor
  int sensorInitAttempts = 0;
  
  while (!sensorInitialized && sensorInitAttempts < maxSensorInitAttempts) {
    if (tempsens.begin()) {
      sensorInitialized = true;
    } else {
      sensorInitAttempts++;
      DEBUG_PRINTLN("Failed to initialize Temp/Humidity sensor. Retrying...");
      delay(2000);  // Wait 2 seconds before retrying
    }
  }

  if (!sensorInitialized) {
    DEBUG_PRINTLN("Could not initialize Temp/Humidity sensor after multiple attempts.");
    if (debug || debug_set) {
      setNeoPixelColor(1, 0, 0);  // Set NeoPixel to red for failure
    }
  }

  // Start the web server
  server.begin();
}


void handleButton() {
  // check for button press
  if ( digitalRead(WIFI_RST) == LOW ) {
    // poor mans debounce/press-hold, code not ideal for production
    delay(5);
    if( digitalRead(WIFI_RST) == LOW ){
      Serial.println("Button Pressed");
      // still holding button for 3000 ms, reset settings, code not ideal for production
      delay(3000); // reset delay hold
      if( digitalRead(WIFI_RST) == LOW ){
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        wifiManager.resetSettings();
        ESP.restart();
      }
    }
  }
}

void loop() {
  currentTime = millis();  // Update currentTime in each loop iteration

  // Periodically refresh sensor data every SENSOR_READ_INTERVAL
  if (sensorInitialized && currentTime - lastSensorReadTime >= SENSOR_READ_INTERVAL) {
    lastSensorReadTime = currentTime;

    // Query the sensor for temperature and humidity
    sensors_event_t temp, humidity;
    tempsens.getEvent(&humidity, &temp);  // Get temperature and humidity readings

    temperatureC = temp.temperature;             // Store latest temperature
    humidityValue = humidity.relative_humidity;  // Store latest humidity

// Ensure data is valid before serving
    if (
        (!isnan(temperatureC) && temperatureC >= -40 && temperatureC <= 85) &&
        (!isnan(humidityValue) && humidityValue >= 0 && humidityValue <= 100)
    ) {
        // Both readings are valid
        validReading = true;

        // Print values for debugging
        DEBUG_PRINT("Temperature: ");
        DEBUG_PRINT(temperatureC);
        DEBUG_PRINTLN(" °C");
        DEBUG_PRINT("Humidity: ");
        DEBUG_PRINT(humidityValue);
        DEBUG_PRINTLN(" % rH");
    } else {
        // One or both readings are invalid
        if (isnan(temperatureC) || temperatureC < -40 || temperatureC > 85) {
            DEBUG_PRINTLN("Invalid temperature reading.");
        }
        if (isnan(humidityValue) || humidityValue < 0 || humidityValue > 100) {
            DEBUG_PRINTLN("Invalid humidity reading.");
        }
        validReading = false;
        DEBUG_PRINTLN("Sensor read error.");
    }

  // Handle any incoming web clients (HTTP requests)
  handleClient();
  // Handle Button
  handleButton();

}

void handleClient() {
  // Check if a client is connected to the server
  WiFiClient client = server.available();

  if (client) {
    currentTime = millis();
    previousTime = currentTime;
    DEBUG_PRINTLN("New Client connected.");

    // Variables to store the current line from the HTTP request and the full request
    String currentLine = "";
    String request = "";

    // Loop while the client is connected
    while (client.connected() && currentTime - previousTime <= CLIENT_TIMEOUT) {
      currentTime = millis();

      // If the client has sent data
      if (client.available()) {
        char c = client.read();  // Read one byte from the client
        DEBUG_PRINT(c);          // Output the byte to the serial monitor
        request += c;            // Append the byte to the request string

        // If the byte is a newline character, the end of the HTTP request is reached
        if (c == '\n') {
          // Check if the current line is empty (i.e., two newlines in a row)
          if (currentLine.length() == 0) {

            // Prepare sensor data for display in the HTML/JSON response
            String tempDisplay;
            String humidityDisplay;
            String tempFDisplay;

            // Check if the request is for the /data endpoint
            if (request.indexOf("GET /data") >= 0) {
              // Data variable handling
              if (!sensorInitialized) {
                tempDisplay = "Sensor Not Initialized";
                humidityDisplay = "Sensor Not Initialized";
                tempFDisplay = "Sensor Not Initialized";
              } else if (!validReading) {
                tempDisplay = "Read-Failure";
                humidityDisplay = "Read-Failure";
                tempFDisplay = "Read-Failure";
              } else {
                tempDisplay = String(temperatureC);
                tempFDisplay = String(1.8 * temperatureC + 32);
                humidityDisplay = String(humidityValue);
              }
              // Prepare and send JSON response
              String jsonResponse = "{";
              jsonResponse += "\"tempC\": " + String(tempDisplay) + ",";
              jsonResponse += "\"tempF\": " + String(tempFDisplay) + ",";
              jsonResponse += "\"humidityValue\": " + String(humidityDisplay);
              jsonResponse += "}";

              // Send the HTTP headers
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: application/json");
              client.println("Connection: close");
              client.println();

              // Send the JSON response
              client.println(jsonResponse);
              break;
            } else {
              // Data variable handling
              if (!sensorInitialized) {
                tempDisplay = "Sensor Not Initialized";
                humidityDisplay = "Sensor Not Initialized";
                tempFDisplay = "Sensor Not Initialized";
              } else if (!validReading) {
                tempDisplay = "Read-Failure";
                humidityDisplay = "Read-Failure";
                tempFDisplay = "Read-Failure";
              } else {
                tempDisplay = String(temperatureC);
                tempFDisplay = String(1.8 * temperatureC + 32);
                humidityDisplay = String(humidityValue);
              }
              // Send an HTTP response back to the client for the main page
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println("Connection: close");

              // Blank line to end the header
              client.println();

              // Send the beginning of the HTML structure
              client.println("<!DOCTYPE html>");
              client.println("<html><head>");
              client.println("<meta charset=\"UTF-8\">");
              client.println("<title>" + HOSTNAME + "</title>");
              client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
              client.println("<link rel='icon' type='image/png' href='data:image/png;base64,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bS94YXAvMS4wL3NUeXBlL1Jlc291cmNlRXZlbnQjIgogICAgeG1sbnM6ZGM9Imh0dHA6Ly9wdXJsLm9yZy9kYy9lbGVtZW50cy8xLjEvIgogICAgeG1sbnM6R0lNUD0iaHR0cDovL3d3dy5naW1wLm9yZy94bXAvIgogICAgeG1sbnM6dGlmZj0iaHR0cDovL25zLmFkb2JlLmNvbS90aWZmLzEuMC8iCiAgICB4bWxuczp4bXA9Imh0dHA6Ly9ucy5hZG9iZS5jb20veGFwLzEuMC8iCiAgIHhtcE1NOkRvY3VtZW50SUQ9ImdpbXA6ZG9jaWQ6Z2ltcDpmM2MwMDYyOC1lYjA1LTQ3ZTUtOTAzMS1jYmM5YmI1ZDRkZmMiCiAgIHhtcE1NOkluc3RhbmNlSUQ9InhtcC5paWQ6NjdhZjZiMTEtN2NmMy00NTExLWEzNzEtMzBlNzA4ZTYxM2ViIgogICB4bXBNTTpPcmlnaW5hbERvY3VtZW50SUQ9InhtcC5kaWQ6NjhhZDJjMmYtYTZjYS00YjlhLTk5MzctNDliYjc4NjIwNWRlIgogICBkYzpGb3JtYXQ9ImltYWdlL3BuZyIKICAgR0lNUDpBUEk9IjIuMCIKICAgR0lNUDpQbGF0Zm9ybT0iTGludXgiCiAgIEdJTVA6VGltZVN0YW1wPSIxNzI3NTY4Mzg2MDcwNzQ3IgogICBHSU1QOlZlcnNpb249IjIuMTAuMzAiCiAgIHRpZmY6T3JpZW50YXRpb249IjEiCiAgIHhtcDpDcmVhdG9yVG9vbD0iR0lNUCAyLjEwIj4KICAgPHhtcE1NOkhpc3Rvcnk+CiAgICA8cmRmOlNlcT4KICAgICA8cmRmOmxpCiAgICAgIHN0RXZ0OmFjdGlvbj0ic2F2ZWQiCiAgICAgIHN0RXZ0OmNoYW5nZWQ9Ii8iCiAgICAgIHN0RXZ0Omluc3RhbmNlSUQ9InhtcC5paWQ6ODA1OTVjMGQtMWExMS00ZjFlLTk3ZDctN2E4YzI1YzIzNGRlIgogICAgICBzdEV2dDpzb2Z0d2FyZUFnZW50PSJHaW1wIDIuMTAgKExpbnV4KSIKICAgICAgc3RFdnQ6d2hlbj0iMjAyNC0wOS0yOFQxOTowNjoyNi0wNTowMCIvPgogICAgPC9yZGY6U2VxPgogICA8L3htcE1NOkhpc3Rvcnk+CiAgPC9yZGY6RGVzY3JpcHRpb24+CiA8L3JkZjpSREY+CjwveDp4bXBtZXRhPgogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIAogICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgCiAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAKICAgICAgICAgICAgICAgICAgICAgICAgICAgCjw/eHBhY2tldCBlbmQ9InciPz5fJZNUAAAABmJLR0QA/wD/AP+gvaeTAAAACXBIWXMAARCQAAEQkAGJrNK4AAAAB3RJTUUH6AkdAAYaMpiAgAAAAM1JREFUWMPtlkEOgDAIBAvxrX0Un9WTiWloF6TgxT0jTFlobc2o3vvZEsSe4AwInhWyFNsBpAKICFV1gyNFLKBI9Cw2JvQA3LFeKJ5572lxxA6yJNNOussOQieaJUaWWYGWANYkkWGl1ccIwOP9LBdFfEUtH/NqMa4ZeAOKIE0zELEJQRwoMOsVVG9CjfDNanm6EBqyyE155yPvBxFpW3FU/PWs5oqzn1vUEdrtt9cKbh/rUwARoXKAcdCpcgtM90AFBLwJMyGyN+zXL7cuCL2yds8ZL5UAAAAASUVORK5CYII=' sizes='32x32'>");

              // Optimized CSS style for the page
              client.println("<style>"
                              "body{font-size:2vw;text-align:center;font-family:'Trebuchet MS',Arial;background:#2b2b2b;color:white;}"
                              "table{font-size:6.5vw;border-collapse:collapse;width:auto;margin:auto;color:white;}"
                              "th{padding:0.2vw;background:#0043af;color:white;}"
                              "tr{border:5px solid #ddd;padding:1vw;}"
                              "tr:hover{background:#2b2b2b;}"
                              "td{border:none;padding:1vw;}"
                              ".sensor{color:white;font-weight:bold;background:#2b2b2b;padding:1vw;}"
                              "</style>");

              // Add the JavaScript for periodic content refresh
              client.println("<script>"
                              "function refreshData(){"
                              "var xhr=new XMLHttpRequest();"
                              "xhr.onreadystatechange=function(){"
                              "if(xhr.readyState==4&&xhr.status==200){"
                              "var data=JSON.parse(xhr.responseText);"
                              "document.getElementById('tempC').innerHTML=data.tempC+' &deg;C';"
                              "document.getElementById('tempF').innerHTML=data.tempF+' &deg;F';"
                              "document.getElementById('humidityValue').innerHTML=data.humidityValue+' % rH';"
                              "}};"
                              "xhr.open('GET','/data',true);"
                              "xhr.send();"
                              "}"
                              "setInterval(refreshData," + String(SENSOR_READ_INTERVAL) + ");"
                              "setTimeout(function(){"
                              "location.reload();"
                              "}, " + String(CLIENT_TIMEOUT) + ");"
                              "</script></head>");

              // Start the Page Content
              client.println("<body><h1>" + HOSTNAME + "</h1>");
              client.println("<table><tr><th>MEASUREMENT</th><th>VALUE</th></tr>");

              // Display temperature in Celsius
              client.println("<tr><td>Temp. Celsius</td><td><span id='tempC' class='sensor'>" + String(tempDisplay) + " &deg;C</span></td></tr>");

              // Display temperature in Fahrenheit
              client.println("<tr><td>Temp. Fahrenheit</td><td><span id='tempF' class='sensor'>" + String(tempFDisplay) + " &deg;F</span></td></tr>");

              // Display humidity
              client.println("<tr><td>Humidity</td><td><span id='humidityValue' class='sensor'>" + String(humidityDisplay) + " % rH</span></td></tr>");

              // Close the table and the HTML tags
              client.println("</table></body></html>");

              // Blank line to indicate the end of the HTTP response
              client.println();
              break;
            }
          } else {
            currentLine = "";  // Clear the current line for the next iteration
          }
        } else if (c != '\r') {
          currentLine += c;  // Append characters that are not carriage returns
        }
      }

      // Handle Button while waiting
      handleButton();
    }

    // Close the connection
    client.stop();
    DEBUG_PRINTLN("Client disconnected.");
  }
}
