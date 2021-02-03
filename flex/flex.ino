#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <Arduino.h>

//webupdater
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

#ifndef STASSID
#define STASSID "ssid"
#define STAPSK  "password"
#endif

//check serial
#ifdef  ARDUINO_SAMD_VARIANT_COMPLIANCE
#define SERIAL_OUTPUT SerialUSB
#else
#define SERIAL_OUTPUT Serial
#endif

//esp-12f fix pin
#define sensorPin A0

//setup sensor name
String device_name = "yourdevicename";
String sensor_type1 = "flex";

//setup server post data
String server_ip = "ip";
String server_port = "port";
String db_name = "dbname";
String db_username = "dbuser";
String db_password = "dbpasswd";

//httpclient
String svdetail = "http://" + server_ip + ":" + server_port + "/write?db=" + db_name + "&u=" + db_username + "&p=" + db_password + "&precision=s";  // set host && user && pw db

const char* ssid     = STASSID;
const char* password = STAPSK;
String ipStr = "";
String macStr = "";
String rssiStr = "";

//led
const int ESP_BUILTIN_LED = 500; //led

//webupdater
const char* host = "hostname";
const char* update_path = "/firmware";
const char* update_username = "admin";
String update_password = "admin" + device_name;

unsigned long getDataTimer = 0;

//webupdater
ESP8266WebServer httpServer(8080);
ESP8266HTTPUpdateServer httpUpdater;

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
  SERIAL_OUTPUT.begin(115200);

 
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      SERIAL_OUTPUT.print(".");
    }
  
    SERIAL_OUTPUT.println("");
    SERIAL_OUTPUT.println("WiFi connected");

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
    message += server_port.substring(1, 3);
    message += "\nDatabase Name: ";
    message += db_name.substring(0, 2);
    message += "\nUsername: ";
    message += db_username.substring(1, 3);
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

}

void loop() {
  //webupdater
  httpServer.handleClient();
  MDNS.update();

  if (millis() - getDataTimer >= 0)
  {

    int x = analogRead(sensorPin);    
    SERIAL_OUTPUT.println(x);

    if (x != 0) {

        HTTPClient http;
        http.begin(svdetail);
        http.addHeader("Content-Type", "--data-binary");
        String postData = sensor_type1 + ",devicename=" + device_name + ",ip=" + ipStr + ",mac=" + macStr + ",rssi=" + rssiStr + " pressure=" + x;
        int httpCode = http.POST(postData);
        SERIAL_OUTPUT.print(httpCode);
        SERIAL_OUTPUT.print(" : ");
        SERIAL_OUTPUT.println(postData);
        http.end();

    } else {
      SERIAL_OUTPUT.print("Error : flex20 data is null !");
      SERIAL_OUTPUT.print("\n");
    }
    getDataTimer = millis(); // Update inerval
  }

  //led
  digitalWrite(ESP_BUILTIN_LED, LOW);
  //delay(1000);
  digitalWrite(ESP_BUILTIN_LED, HIGH);
  delay(1000);

}
