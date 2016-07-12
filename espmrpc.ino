
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <mrpc.h>

using namespace MRPC;
Node *mrpc;
ESP8266WebServer server(80);
StaticJsonBuffer<2048> jsonBuffer;

JsonObject& loadSettings() {
  EEPROM.begin(1024);
  char json[1024];
  EEPROM.get(0, json);
  JsonObject& object = jsonBuffer.parseObject(json);
  if(object.success()) {
    return object;
  }
  return jsonBuffer.createObject();
}

JsonObject& settings = loadSettings();

void saveSettings() {
  char json[1024];
  settings.printTo(json, sizeof(json));
  Serial.println(json);
  EEPROM.put(0, json);
  EEPROM.commit();
}

void setupWiFiAP(char *password)
{
  WiFi.mode(WIFI_AP);

  // Do a little work to get a unique-ish name. Append the
  // last two bytes of the MAC (HEX'd) to "Thing-":
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  String macID = String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();
  String AP_NameString = "MRPC " + macID;

  char AP_NameChar[AP_NameString.length() + 1];
  memset(AP_NameChar, 0, AP_NameString.length() + 1);

  for (int i=0; i<AP_NameString.length(); i++)
    AP_NameChar[i] = AP_NameString.charAt(i);
  WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_NameChar, password);
}

void handleRoot() {
  Serial.println("Scanning networks");
  int wifi_count = WiFi.scanNetworks();
  Serial.println("Scan done");
  String response =
"<form action=\"/connect\" method=\"get\">\
    <label>SSID:</label>\
    <select name=\"ssid\">";

  for(int i = 0; i < wifi_count; i++) {
    response += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + "</option>\n";
  }
  response +=
"    </select>\
    <label>Password:</label>\
    <input type=\"password\" name=\"password\">\
  <input type=\"submit\" value=\"Connect\">\
</form>";
  server.send(200, "text/html", response);
}

void handleConnect() {
  if(server.hasArg("ssid") && server.hasArg("password")) {
    JsonObject& wifi_settings = settings.get("wifi").as<JsonObject>();
    wifi_settings["ssid"] = server.arg("ssid");
    wifi_settings["password"] = server.arg("password");
    saveSettings();
    server.send(200, "text/html", "Successfully saved settings");
  }
  else
    server.send(500, "text/html", "Missing ssid or password");
}

bool validWifiSettings() {
  if(settings["wifi"] == NULL) return false;
  if(!settings.get("wifi").is<JsonObject>()) return false;
  JsonObject& wifi_settings = settings.get("wifi").as<JsonObject>();
  if(wifi_settings.get<const char*>("ssid") == NULL || wifi_settings.get<const char*>("password") == NULL) return false;
  return true;
}

JsonVariant temperature(Service *self, StaticJsonBuffer<1024> *jsonBuffer) {
  Serial.println("Sending temperature");
    return 75;
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  char json[1024];
  EEPROM.get(0, json);
  Serial.print("EEPROM Contents: ");
  Serial.println(json);
  WiFi.mode(WIFI_STA);
  bool createAP = true;
  if(validWifiSettings()) {
    Serial.println("Found wifi settings:");
    JsonObject& wifi_settings = settings.get("wifi").as<JsonObject>();
    wifi_settings.printTo(Serial);
    Serial.println();
    
    WiFi.begin(wifi_settings.get<const char*>("ssid"), wifi_settings.get<const char*>("password"));
    Serial.println("Connecting to WiFi");
    for(int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
    if(WiFi.status() == WL_CONNECTED) {
      Serial.println("Connection successful");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      createAP = false;
    }
    else
      Serial.println("Connection failed");
  }
  else {
    settings.createNestedObject("wifi");
    Serial.println("Couldn't find WiFi settings");
  }

  if(createAP) {
    Serial.println("Creating AP");
    setupWiFiAP("password");
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/connect", HTTP_GET, handleConnect);
  server.begin();
  mrpc = new Node();
    Transport *trans = new UDPTransport(50123);
    Service simpleService = Service();
    //simpleService.add_publisher("temperature", &temperature, "/LivingRoom", 1000);
    mrpc->register_service("/SimpleService", &simpleService);
    mrpc->use_transport(trans);
  
}

void loop() {
  server.handleClient();
  mrpc->poll();
  delay(50);
}

