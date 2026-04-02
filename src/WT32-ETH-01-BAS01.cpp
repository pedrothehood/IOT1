// 12.3.2026
// Produktivversion Sensor DHT_ETH-02 WT32-ETH01 mit Ethernet DHT22-Wärmesensor, OTA,Telnet, SSL,
// diverse Dienste OFF, Sleep
// Upload: damit der Upload über OTA/Ethernet funktioniert, muss auf dem Server "Sleep" deaktiviert werden,
// Gerät vom Strom nehmen, dann wird die normale loop-Schlaufe mit OTA korrekt durchlaufen und
// der Upload über den Ethernet-Port funktioniert!

// Initialisiermit en OTA und Test mit Blink-Script auf LED=2
// wt32-eth01 an 5 V/5V GNd/GND Tx/RX RX/TX
// Load: 5 V/5V  Tx/RX RX/TX  , dann GND(vom TTL) an Board,dann TTL an PC einstecken,
// GND(v Board) an Ioo (grün+rote LED blinken),
// EN an GND kurzschliessen(grüne LED erlischt),
// EN an GND lösen(now ready for uploade mit Arduino IDE)
// nach Upload: GND(von Board) an GND (neben 5V-Pin) am ESP32  anschliessen
// Bei Board ESP32 Dev Module, I00 direkt an GND(TTL), dann sollte es auch funktionieren!
// ergaenzt mit Telnet-Coding -> in anderen Codings nachführen!
// ermöglicht Aufruf in putty mit IP-Adresse, "Telnet"-Modus und Port 23
// ev. Windows Defender Firewall öffnen Ein- und Ausgehend für diese App,
// d.h. Eingangs- und Ausgangsregeln definieren
//
// Boot-Probleme: Es wird empfohlen, einen Elektrolytkondensator (z.B. 100µF bis 470µF)
// zwischen GND und den EN-Pin des WT32-ETH01 zu schalten, um Boot-Probleme zu vermeiden,
// Parallelschaltung!!!
#include <ETH.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <WiFi.h>             // Erforderlich für WiFiClient (auch bei Ethernet!)
#include <esp_bt.h>           // ausschalten bluetooth
#include <ArduinoJson.h>      // für Deserialisierung
#include <WiFiClientSecure.h> // SSL Verschlüsselung
#include <ESPmDNS.h>          // für komfortablen Zugriff: http://mein-wt32.local bzw. http://<sensorid>.local

/* 29.3.2026 Webserver B */
// String webServerActive = "X";
#include <WebServer_WT32_ETH01.h>
#include <Preferences.h>
Preferences prefs;
WebServer server(80);

// Physischer Button (Zieht gegen GND)
#define CONFIG_BUTTON_PIN 32 


// Variablen zum Zwischenspeichern der Werte
String sensorid, apiKey, ssid, password, servername, mac;

/* Preferences Lesen, falls Webserver nicht gestartet wird */
void getPreferences()
{
  // Aktuelle Werte aus Preferences laden
  prefs.begin("config", true);
  sensorid = prefs.getString("sid", "");
  apiKey = prefs.getString("key", "");
  ssid = prefs.getString("ssid", "");
  password = prefs.getString("pass", "");
  servername = prefs.getString("srv", "");
  //mac = prefs.getString("mac", "");
  prefs.end();
}
void handleRoot()
{
  /* // Aktuelle Werte aus Preferences laden
    prefs.begin("config", true);
    sensorid = prefs.getString("sid", "");
    apiKey = prefs.getString("key", "");
    ssid = prefs.getString("ssid", "");
    password = prefs.getString("pass", "");
    servername = prefs.getString("srv", "");
    prefs.end();
    */
  getPreferences();

  String html = "<html><head><meta charset='UTF-8'><style>";
  html += "body { font-family: sans-serif; margin: 20px; }";
  html += "label { display: inline-block; width: 120px; margin-bottom: 10px; }";
  html += "input { margin-bottom: 10px; padding: 5px; }";
  html += "</style></head><body>";
  html += "<h1>Geräte-Konfiguration</h1>";
  html += "<form action='/save' method='POST'>";

  html += "<label>Sensor-ID:</label> <input type='text' name='sensorid' value='" + sensorid + "'><br>";
  html += "<label>API-Key:</label> <input type='text' name='apiKey' value='" + apiKey + "'><br>";
  html += "<label>SSID:</label> <input type='text' name='ssid' value='" + ssid + "'><br>";
  html += "<label>Passwort:</label> <input type='password' name='password' value='" + password + "'><br>";
  html += "<label>Servername:</label> <input type='text' name='servername' value='" + servername + "'><br>";
   html += "<label>Macadresse:</label> <input type='text' name='mac' value='" + WiFi.macAddress() + "' readonly ><br>";

  html += "<br><input type='submit' value='Speichern und Neustarten'>";
  html += "</form></body></html>";

  server.send(200, "text/html", html);
}

void handleSave()
{
  if (server.method() == HTTP_POST)
  {
    prefs.begin("config", false);

    // Werte aus dem Formular in den Speicher schreiben
    prefs.putString("sid", server.arg("sensorid"));
    prefs.putString("key", server.arg("apiKey"));
    prefs.putString("ssid", server.arg("ssid"));
    prefs.putString("pass", server.arg("password"));
    prefs.putString("srv", server.arg("servername"));
    

    prefs.end();

    String msg = "<html><body><h1>Erfolgreich gespeichert!</h1>";
    msg += "<p>Das Modul startet jetzt neu...</p></body></html>";
    server.send(200, "text/html", msg);

    delay(2000);
    ESP.restart(); // Neustart, um die neuen Einstellungen zu übernehmen
  }
}

/* 29.3.2026 Webserver E */

/* Webservice -B-*/
/*
const char *serverName = "https://mediabegleitung.ch/post-data.php";
const char *serverNameFirst = "https://mediabegleitung.ch/get-sensoriddata.php?sensorid=DHT_ETH-01";
*/
const char *serverName = nullptr;
const char *serverNameFirst = nullptr;

// Keep this API Key value to be compatible with the PHP code provided in the project page.
// If you change the apiKeyValue value, the PHP file /post-esp-data.php also needs to have the same key
String apiKeyValue = "";
StaticJsonDocument<700> doc;
String strStatus;
String strSleep;
String strServerActive;
int delaymin = 60;
int delaytsec = 3600000;
/* Webservice -E-*/

/* Telnet B*/
#include "ESPTelnet.h"
ESPTelnet telnet;
void onTelnetConnect(String ip)
{
  Serial.print("- Telnet: ");
  Serial.print(ip);
  Serial.println(" verbunden");
  telnet.println("\nWillkommen beim WT32-ETH01 Debug-Monitor!");
  telnet.println("Die IP ist " + ip);
  telnet.println(ip);
}
void debugLog(String msg)
{
  // 1. Ausgabe auf dem physischen Seriellen Monitor
  Serial.println(msg);

  // 2. Ausgabe über Telnet (nur wenn ein Client verbunden ist)
  if (telnet.isConnected())
  {
    telnet.println(msg);
  }
}
/* Telnet E*/

// Ethernet Pins für WT32-ETH01 (Core 3.x) geht nicht mit Core 3.x
/*#define ETH_ADDR 1
#define ETH_POWER_PIN 16
#define ETH_MDC_PIN 23
#define ETH_MDIO_PIN 18  */

#define ETH_PHY_TYPE ETH_PHY_LAN8720 // Das ist der richtige TYP
#define ETH_PHY_ADDR 1
#define ETH_MDC_PIN 23
#define ETH_MDIO_PIN 18
#define ETH_POWER_PIN 16 // Dein Pin (bleibt ein int)
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN

// Korr. 29.3.2026: Definieren der WT32-ETH01 spezifischen Pins
// geht auch nicht:
/*#define ETH_PHY_ADDR  1
#define ETH_PHY_POWER 16 // Der Oszillator wird über GPIO 16 aktiviert
#define ETH_PHY_MDC   23
#define ETH_PHY_MDIO  18
#define ETH_PHY_TYPE  ETH_PHY_LAN8720
#define ETH_CLK_MODE  ETH_CLOCK_GPIO0_IN // GPIO0 ist der Takteingang   */

// DHT22 Konfiguration
#define DHTPIN 15 // GPIO 15 für den DHT22 Sensor
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Webservice URLs (Beispiele)
const char *get_url = "http://worldtimeapi.org";
const char *post_url = "http://dein-webserver.de";

unsigned long lastUpdate = 0;
long interval = 10000; // Alle 10 Sekunden senden

void setup()
{
  Serial.begin(115200);

  // WLAN komplett ausschalten
 // WiFi.mode(WIFI_OFF);
 // WiFi.disconnect(true);

  // Bluetooth-Radio stoppen
  btStop();
  // Optional: Bluetooth-Speicher im RAM freigeben
  esp_bt_controller_disable();
  esp_bt_controller_deinit();
  setCpuFrequencyMhz(40); // Takt auf 40 MHz reduzieren
  dht.begin();

  // Ethernet initialisieren
  /* if (!ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_POWER_PIN, ETH_CLK_MODE)) {
    Serial.println("ETH Fehler!");
  } */

  if (!ETH.begin(1, 16, 23, 18, ETH_PHY_LAN8720, ETH_CLOCK_GPIO0_IN))
  {
    Serial.println("ETH Fehler!");
  }

  // Neuer Funktionsaufruf für Core 3.x
  /*if (!ETH.begin(
    ETH_PHY_LAN8720,  // Der Enum-Typ (KEINE Zahl!)
    1,                // PHY_ADDR
    23,               // MDC_PIN
    18,               // MDIO_PIN
    16,               // POWER_PIN (Das ist deine "16")
    ETH_CLOCK_GPIO0_IN // CLK_MODE
  )) {
    Serial.println("ETH Initialisierung fehlgeschlagen!");
  } */

  while (ETH.localIP().toString() == "0.0.0.0")
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nVerbunden! IP: " + ETH.localIP().toString());

  /* Telnet B*/
  // Telnet Setup
  telnet.onConnect(onTelnetConnect);
  if (telnet.begin(23, false))
  { // Port 23, WiFi-Check deaktiviert
    // telnet.begin(23, false);
    // if (telnet.isConnected()) {
    // dieser Code wird nicht ausgeführt!?
    Serial.println("Telnet-Server gestartet");
    debugLog("Telnet-Server gestartet");
    IPAddress myIP = ETH.localIP();
    String ipAsString = myIP.toString();
    debugLog("Meine IP ist: " + ipAsString);
  }
  /* Telnet E*/
  debugLog("Mac-Adresse:");   // Mac-Adresse ausgeben
  debugLog(ETH.macAddress()); // Mac-Adresse ausgeben
  // OTA Setup
  ArduinoOTA.setHostname("WT32-ETH01-Knoten");
  ArduinoOTA.begin();
}

void loop()
{
  /* Telnet B*/
  telnet.loop(); // Wichtig: Hält die Verbindung aufrecht
  server.handleClient();
  /* Telnet E*/
  /* Webserver 29.3.2029 -B- */
  /* if (webServerActive == "X")
   {
     server.handleClient();
   }
   else
   {
     // getPreferences();
   }  */
  getPreferences(); // auf jeden Fall lesen
  static String strServerName = servername + "/" + "post-data.php";
  static String strServerNameFirst = servername + "/" + "get-sensoriddata.php?sensorid=" + sensorid;
  serverName = strServerName.c_str();           // Dem Pointer jetzt die Adresse zuweisen
  serverNameFirst = strServerNameFirst.c_str(); // Dem Pointer jetzt die Adresse zuweisen
  apiKeyValue = apiKey;                         // aus den Preferences
  /* Webserver 29.3.2029 -E- */

  ArduinoOTA.handle();

  if (millis() - lastUpdate > interval)
  {
    lastUpdate = millis();

    if (MDNS.begin(sensorid)) // Sensorid als kurzurl: http.<sensorid>.local
    {
      Serial.println("mDNS Responder gestartet");
    }

    // 1. Sensordaten einlesen
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    int ih = h;
    int it = t;
    if (isnan(h) || isnan(t))
    {
      Serial.println("Fehler beim Lesen des DHT22!");
      debugLog("Fehler beim Lesen des DHT22!");
      return;
    }

    Serial.printf("Temp: %.1f°C, Feuchte: %.1f%%\n", t, h);
    String tempAusgabe = "Temp: " + String(t, 1) + "°C, Feuchte: " + String(h, 1) + "%\n";
    debugLog(tempAusgabe);

    // 2. Daten per GET von einem Webservice abrufen
    // WiFiClient client; // Benötigt für HTTPClient
    // HTTPClient http;

    WiFiClientSecure *client = new WiFiClientSecure; // SSL
                                                     // Original-Code alt:  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);

    // Ignore SSL certificate validation
    client->setInsecure();

    // create an HTTPClient instance
    HTTPClient https;

    Serial.println("GET Request...");
    debugLog("GET Request...");
    debugLog("strServerNameFirst:");
    debugLog(strServerNameFirst);
    // http.begin(client, get_url);
    // http.begin(client, serverNameFirst);
    https.begin(*client, serverNameFirst); // SSL
    int httpCodeGet = https.GET();
    if (httpCodeGet > 0)
    {
      String payload = https.getString();
      Serial.println("GET Response: " + payload.substring(0, 50) + "...");
      debugLog("GET Response: " + payload.substring(0, 50) + "...");
      debugLog(payload);

      // Deserialize the JSON document
      DeserializationError error = deserializeJson(doc, payload);
      // Test if parsing succeeds.
      if (error)
      {
        Serial.print(F("First: deserializeJson() failed: "));
        debugLog(F("First: deserializeJson() failed: "));
        Serial.println(error.f_str());
        debugLog(error.f_str());
        return;
      }
      delaymin = doc["sensorinfo"][0]["delaymin"];
      delaytsec = delaymin * 60 * 1000;
      //      status = doc["sensorinfo"]["status"];
      const char *status = doc["sensorinfo"][0]["status"];
      strStatus = status;
      const char *sleep = doc["sensorinfo"][0]["sleep"];
      strSleep = sleep;
      const char *serveractive = doc["sensorinfo"][0]["serveractive"];
      strServerActive = serveractive;
      /* Webserver 29.3.2029 -B- */
      debugLog("strServerActive=");
      debugLog(strServerActive);
      if (strServerActive == "X")
      {
        // nach vorne genommen  server.handleClient();
        server.on("/", HTTP_GET, handleRoot);
        server.on("/save", HTTP_POST, handleSave);
        server.begin();
        Serial.println("HTTP Server läuft.");
        debugLog("HTTP Server läuft.");
      }
      /* Webserver 29.3.2029 -E- */
      Serial.print("status=");
      Serial.println(strStatus);
      Serial.print("delaytsec=");
      Serial.println(delaytsec);

      debugLog("status=");
      debugLog(strStatus);
      debugLog("sleep=");
      debugLog(strSleep);
      debugLog("delaytsec=");
      debugLog(String(delaytsec));

      // temporär auskommentiert!!!! interval = delaytsec;    // Wichtig: setzt das aktuelle Intervall vom Server
      interval = delaytsec; // Wichtig: setzt das aktuelle Intervall vom Server
    }
    https.end();

    // 3. Sensordaten per POST senden

    if (strStatus == "Aktiv")
    {
      https.begin(*client, serverName);

      Serial.println("POST Request...");
      debugLog("POST Request...");
      https.addHeader("Content-Type", "application/x-www-form-urlencoded"); //"application/json");
                                                                            // in esp8266-Code: https.addHeader("Content-Type", "application/x-www-form-urlencoded");
                                                                            // DHT_ETH-01
                                                                            // JSON String erstellen
                                                                            // String httpRequestData = "{\"temp\":" + String(t) + ",\"hum\":" + String(h) + "}";
                                                                            //  String httpRequestData = "api_key=" + apiKeyValue + "&sensorid=DHT_ETH-01" + "&value1=" + String(it) + "&value2=" + String(ih) + "&value3=" + " " + "";
      String httpRequestData = "api_key=" + apiKeyValue + "&sensorid=" + sensorid + "&value1=" + String(it) + "&value2=" + String(ih) + "&value3=" + " " + "";

      Serial.print("httpRequestData: ");
      Serial.println(httpRequestData);
      debugLog("httpRequestData: ");
      debugLog(httpRequestData);

      int httpResponseCode = https.POST(httpRequestData);
      if (httpResponseCode > 0)
      {
        Serial.printf("POST Status: %d\n", httpResponseCode);
        // debugLog("POST Status: %d\n", httpCodePost);
        debugLog("POST Status:  " + String(httpResponseCode) + "\n");
      }
      else
      {
        String fehlermeldung = String(https.errorToString(httpResponseCode).c_str());
        Serial.printf("POST Fehler: %s\n", https.errorToString(httpResponseCode).c_str());
        // debugLog("POST Fehler: %s\n", http.errorToString(httpCodePost).c_str());
        debugLog("POST Fehler: " + fehlermeldung + "\n");
      }
      https.end();
    }
    else
    {
      Serial.println("Status ist nicht aktiv, keine Daten gesendet");
    }
    if (strSleep == "X" && strServerActive != "X") // nicht bei aktivem Webserver beenden
    {
      // 3. Ethernet sauber beenden
      debugLog("Beende Ethernet-Verbindung...");
      // client.stop();   // ETH.stop() gibt es nicht!
      //  -> geht nichtETH.end();  // ???
      //  Kurze Pause (optional), damit der Chip Zeit zum Abschalten hat
      // delay(100);
      // Ab in den Schlaf -> Deep Sleep
      esp_sleep_enable_timer_wakeup((uint64_t)delaymin * 60 * 1000000ULL);
      debugLog("geht in den Schlafmode");
      esp_deep_sleep_start();
    }
  }
}
