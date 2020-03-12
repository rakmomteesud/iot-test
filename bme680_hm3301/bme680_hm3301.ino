#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

//webupdater
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

//bme680
#include "seeed_bme680.h"

//hm3301
#include <Seeed_HM330X.h>

//setup wifi
#ifndef STASSID
#define STASSID "ssid"
#define STAPSK  "password"  
#endif

//setup sensor name
String device_name = "yourdevicename"; 
String sensor_type1 = "bme680";
String sensor_type2 = "hm3301"; 

//setup server post data
String server_ip = "ip";
String server_port = "port";
String db_name = "dbname";
String db_username = "dbuser";
String db_password = "dbpasswd";

//bme680
#define IIC_ADDR  uint8_t(0x76)

//bme680
Seeed_BME680 bme680(IIC_ADDR); /* IIC PROTOCOL */

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

//start hm3301
HM330X sensor;
uint8_t buf[30];
float data_hm[7];

const char *str[] = {
  "sensor num: ",
  "PM1.0 concentration(CF=1,Standard particulate matter,unit:ug/m3): ",
  "PM2.5 concentration(CF=1,Standard particulate matter,unit:ug/m3): ",
  "PM10 concentration(CF=1,Standard particulate matter,unit:ug/m3): ",
  "PM1.0 concentration(Atmospheric environment,unit:ug/m3): ",
  "PM2.5 concentration(Atmospheric environment,unit:ug/m3): ",
  "PM10 concentration(Atmospheric environment,unit:ug/m3): ",
};

HM330XErrorCode print_result(const char *str, uint16_t value) {
  if (NULL == str) {
    return ERROR_PARAM;
  }
  //SERIAL_OUTPUT.print(str);
  //SERIAL_OUTPUT.println(value);
  return NO_ERROR;
}

/*parse buf with 29 uint8_t-data*/
HM330XErrorCode parse_result(uint8_t *data) {
  uint16_t value = 0;
  if (NULL == data)
    return ERROR_PARAM;
  for (int i = 1; i < 8; i++) {
    value = (uint16_t) data[i * 2] << 8 | data[i * 2 + 1];
    data_hm[i] = value;
  }
  return NO_ERROR;
}

HM330XErrorCode parse_result_value(uint8_t *data) {
  if (NULL == data) {
    return ERROR_PARAM;
  }
  for (int i = 0; i < 28; i++) {
    //SERIAL_OUTPUT.print(data[i], HEX);
    //SERIAL_OUTPUT.print("  ");
    if ((0 == (i) % 5) || (0 == i)) {
      //SERIAL_OUTPUT.println("");
    }
  }
  uint8_t sum = 0;
  for (int i = 0; i < 28; i++) {
    sum += data[i];
  }
  if (sum != data[28]) {
    //SERIAL_OUTPUT.println("wrong checkSum!!!!");
  }
  //SERIAL_OUTPUT.println("");
  return NO_ERROR;
}
//end hm3301

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

  //led
  digitalWrite(ESP_BUILTIN_LED, LOW);
  delay(1000);
  digitalWrite(ESP_BUILTIN_LED, HIGH);
  delay(1000);

  if (bme680.init() && !bme680.read_sensor_data()) {
    //bme680
    float t =  bme680.sensor_result_value.temperature;//c
    float p =  bme680.sensor_result_value.pressure / 1000.0; //KPa
    float h =  bme680.sensor_result_value.humidity;//%
    float g =  bme680.sensor_result_value.gas / 1000.0; //Kohms

    if (sensor_type1 != "" && device_name != "" && t > 0 && p > 0 && h > 0) {
      HTTPClient http;
      http.begin(svdetail);
      http.addHeader("Content-Type", "--data-binary");
      String postData = sensor_type1 + ",devicename=" + device_name + ",ip=" + ipStr + ",mac=" + macStr + ",rssi=" + rssiStr + " temperature=" + t + ",pressure=" + p + ",humidity=" + h + ",gas=" + g;
      int httpCode = http.POST(postData);
      SERIAL_OUTPUT.print(httpCode);
      SERIAL_OUTPUT.print(" : ");
      SERIAL_OUTPUT.println(postData);
      http.end();
    } else {
      SERIAL_OUTPUT.print("Error : bme680 data is null ! ");
      SERIAL_OUTPUT.print("\n");
    }
  } else {
    SERIAL_OUTPUT.print("Error : bme680 can't find device and read data !");
    SERIAL_OUTPUT.print("\n");
  }

  if (!sensor.init() && !sensor.read_sensor_value(buf, 29)) {
    //hm3301
    parse_result(buf);
    if (sensor_type2 != "" && device_name != "" && data_hm[2] > 0 && data_hm[3] > 0 && data_hm[4] > 0 && data_hm[5] > 0 && data_hm[6] > 0 && data_hm[7] > 0) {
      HTTPClient http;
      http.begin(svdetail);
      http.addHeader("Content-Type", "--data-binary");
      String postData2 = sensor_type2 + ",devicename=" + device_name + ",ip=" + ipStr + ",mac=" + macStr + ",rssi=" + rssiStr + " pm1std=" + data_hm[2] + ",pm25std=" + data_hm[3] + ",pm10std=" + data_hm[4] + ",pm1atm=" + data_hm[5] + ",pm25atm=" + data_hm[6] + ",pm10atm=" + data_hm[7];
      int httpCode2 = http.POST(postData2);
      SERIAL_OUTPUT.print(httpCode2);
      SERIAL_OUTPUT.print(" : ");
      SERIAL_OUTPUT.println(postData2);
      http.end();
    } else {
      SERIAL_OUTPUT.print("Error : hm3301 data is null !");
      SERIAL_OUTPUT.print("\n");
    }
  } else {
    SERIAL_OUTPUT.print("Error : hm3301 can't find  device and read data !");
    SERIAL_OUTPUT.print("\n");
  }

  delay(6000);

}
