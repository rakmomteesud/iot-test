#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

//mhz19b
#include <Arduino.h>
#include "MHZ19.h"
#include <SoftwareSerial.h>

//webupdater
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

//setup wifi
#ifndef STASSID
#define STASSID "ssid" 
#define STAPSK  "password"  
#endif

//setup sensor name
String device_name = "yourdevicename"; 
String sensor_type1 = "mhz19b"; 

//setup server post data
String server_ip = "ip";
String server_port = "port";
String db_name = "dbname";
String db_username = "dbuser";
String db_password = "dbpasswd";

//setup pin rx, tx
#define RX_PIN 4
#define TX_PIN 5
#define BAUDRATE 9600 // Native to the sensor (do not change)

//hm3301
#ifdef  ARDUINO_SAMD_VARIANT_COMPLIANCE
#define SERIAL_OUTPUT SerialUSB
#else
#define SERIAL_OUTPUT Serial
#endif

//httpclient
String svdetail = "http://" + server_ip + ":" + server_port + "/write?db=" + db_name + "&u=" + db_username + "&p=" + db_password + "&precision=s";  // set host && user && pw db

//wifi
const char* ssid = STASSID;
const char* password = STAPSK;
String ipStr = "";
String macStr = "";
String rssiStr = "";

//led
const int ESP_BUILTIN_LED = 2; //led

//webupdater
const char* host = "hostname";
const char* update_path = "/firmware";
const char* update_username = "admin";
String update_password = "admin" + device_name;

//webupdater
ESP8266WebServer httpServer(8080);
ESP8266HTTPUpdateServer httpUpdater;

//mhz19b
MHZ19 myMHZ19;
SoftwareSerial mySerial(RX_PIN, TX_PIN);
unsigned long getDataTimer = 0;
void verifyRange(int range);

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += (httpServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";
  for (uint8_t i = 0; i < httpServer.args(); i++) {
    message += " " + httpServer.argName(i) + ": " + httpServer.arg(i) + "\n";
  }
  httpServer.send(404, "text/plain", message);
}

void setup() {

  // initialize serial port
  SERIAL_OUTPUT.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password); //WiFi connection

  //wifi
  SERIAL_OUTPUT.println('\n');
  SERIAL_OUTPUT.println("Attempting to connect to network...");
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(5000);
    SERIAL_OUTPUT.print("Couldn't get a WiFi connection");
    ESP.restart();
  }
  SERIAL_OUTPUT.println("Connected to network");

  IPAddress myIP = WiFi.localIP();
  ipStr = String(myIP[0]) + "." + String(myIP[1]) + "." + String(myIP[2]) + "." + String(myIP[3]);
  macStr = String(WiFi.macAddress());
  rssiStr = String(WiFi.RSSI());

  SERIAL_OUTPUT.print("IP Address: ");
  SERIAL_OUTPUT.println(ipStr) + "\n";
  SERIAL_OUTPUT.print("Mac Address: ");
  SERIAL_OUTPUT.println(macStr) + "\n";

  long rssi = WiFi.RSSI();
  SERIAL_OUTPUT.print("Signal Strength (RSSI):");
  SERIAL_OUTPUT.print(rssiStr);
  SERIAL_OUTPUT.println(" dBm\n");

  //webupdater
  MDNS.begin(host);

  httpServer.on("/info", []() {
    String message = "Device Information\n\n";
    message += "Hostname: ";
    message += device_name;
    message += "\nSSID: ";
    message += ssid;
    message += "\nIP Address: ";
    message += ipStr;
    message += "\nMac Address: ";
    message += macStr;
    message += "\nRSSI: ";
    message += rssiStr;
    message += "\n\nDatabase Config\n\n";
    message += "IP: ";
    message += server_ip.substring(12);
    message += "\nPort: ";
    message += server_port.substring(1,3);
    message += "\nDatabase Name: ";
    message += db_name.substring(0,2);
    message += "\nUsername: ";
    message += db_username.substring(1,3);
    message += "\n";
    httpServer.send(200, "text/plain", message);
  });

  httpServer.onNotFound(handleNotFound);

  httpUpdater.setup(&httpServer, update_path, update_username, update_password);
  httpServer.begin();

  MDNS.addService("http", "tcp", 8080);
  SERIAL_OUTPUT.printf("HTTPUpdateServer ready! Open http://%s:8080%s in your browser and login with username '%s' and password '*****'\n\n", host, update_path, update_username);

  //led
  pinMode(ESP_BUILTIN_LED, OUTPUT);

  //mhz19b
  mySerial.begin(BAUDRATE);
  myMHZ19.begin(mySerial); // *Important, Pass your Stream reference

  int range = 20000;
  myMHZ19.setRange(range); // advise 2000 or 5000 default 2000
  myMHZ19.calibrateZero();
  myMHZ19.setSpan(2000); //strongly recommend 2000

  /*            ###autoCalibration(false)###
     Basic:
     autoCalibration(false) - turns auto calibration OFF. (automatically sent before defined period elapses)
     autoCalibration(true)  - turns auto calibration ON.
     autoCalibration()      - turns auto calibration ON.

     Advanced:
     autoCalibration(true, 12) - turns autocalibration ON and calibration period to 12 hrs (maximum 24hrs).
  */

  myMHZ19.autoCalibration(false);
}

void loop() {
  //webupdater
  httpServer.handleClient();
  MDNS.update();

  //led
  digitalWrite(ESP_BUILTIN_LED, LOW);
  delay(1000);
  digitalWrite(ESP_BUILTIN_LED, HIGH);
  delay(1000);

  if (millis() - getDataTimer >= 10000)
  {
    int CO2;
    CO2 = myMHZ19.getCO2();
    int8_t Temp; // Buffer for temperature
    Temp = myMHZ19.getTemperature();

    if (sensor_type1 != "" && device_name != "") {
      //ppm
      if (CO2 > 0 && Temp > 0 ) {
        HTTPClient http;
        http.begin(svdetail);
        http.addHeader("Content-Type", "--data-binary");
        String postData = sensor_type1 + ",devicename=" + device_name + ",ip=" + ipStr + ",mac=" + macStr + ",rssi=" + rssiStr + " co2=" + CO2 + ",temp=" + Temp;
        int httpCode = http.POST(postData);
        SERIAL_OUTPUT.print(httpCode);
        SERIAL_OUTPUT.print(" : ");
        SERIAL_OUTPUT.println(postData);
        http.end();
      } else {
        SERIAL_OUTPUT.print("Error : mhz19b data is null !");
        SERIAL_OUTPUT.print("\n");
      }
    } else {
      SERIAL_OUTPUT.print("Error : mhz19b check config !");
      SERIAL_OUTPUT.print("\n");
    }
    getDataTimer = millis(); // Update inerval
  }
}

void verifyRange(int range)
{
  Serial.println("Requesting new range.");
  myMHZ19.setRange(range); // request new range write
  if (myMHZ19.getRange() == range) // Send command to device to return it's range value.
    Serial.println("Range successfully applied."); // Success
  else
    Serial.println("Failed to apply range."); // Failed
}
