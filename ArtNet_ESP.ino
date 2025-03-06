#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <WiFiUdp.h>
#include "ThingSpeak.h"

#define LED 32

// Area costanti
#define MAX_BUFFER_LENGTH_ARTNET 530
#define ART_NET_PORT 6454
#define ART_DMX_CODE 0x5000
#define ART_NET "Art-Net"
#define MAX_CHANNEL_LIMIT 4

// Area variabili generiche
Preferences preferences;
WebServer server(80);
IPAddress IP;
WiFiUDP udp;

// Area variabili connessione WiFi
String ssidAP = "ESP32_DMX";
String passwordAP = "password123";
String ssidSta;
String passwordSta;
bool useApMode = true;

// Area variabili Art-Net
uint8_t packetBuffer[MAX_BUFFER_LENGTH_ARTNET];
int packetSize;
uint16_t opCode;
uint16_t universe;
uint16_t length;
uint8_t* data;

// Area variabili connessione cloud ThingSpeak
unsigned long int channel_ID = 2790805;    //2862162;
const char* ApiKeyR = "OPLZ7RDRX85IUVNZ";  //"9KLK4ID4TIJZJQEL";
WiFiClient thingSpeakClient;
unsigned long currentMillis;
unsigned long previousMillis = 0;
const long interval = 17000;

// Inizializzazione
void setup() {

  Serial.begin(115200);
  Serial2.begin(250000, SERIAL_8N1, 16, 17);

  pinMode(LED, OUTPUT);

  preferences.begin("dmx_settings", false);
  loadSettings();

  setupWiFi();
  udp.begin(ART_NET_PORT);

  setupWebPage();
  server.begin();

  Serial.println("Inizializzazione ThingSpeak...");
  ThingSpeak.begin(thingSpeakClient);


  digitalWrite(LED, HIGH);

  Serial.println("Setup completato.");
}

// Esecuzione principale
void loop() {

  server.handleClient();
  handleArtNetPacket();

  currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    updateCloudInfo();
  }
}

// Gestione update su cloud
void updateCloudInfo() {

  // Calcolo dell'intensità media e numero di canali attivi
  int activeChannels = 0;
  int totalIntensity = 0;

  for (int i = 0; i < length; i++) {
    if (data[i] > 0) {
      activeChannels++;
      totalIntensity += data[i];
    }
  }

  float averageIntensity = (activeChannels > 0) ? (float)totalIntensity / activeChannels : 0;

  // Invia i dati a ThingSpeak
  ThingSpeak.setField(1, averageIntensity);
  ThingSpeak.setField(2, activeChannels);

  int res = ThingSpeak.writeFields(channel_ID, ApiKeyR);
  if (res == 200)
    Serial.println("Dati inviati al cloud!");
  else
    Serial.print("Errore nell'invio dei dati al cloud!");
}

// Invia i canali con i rispettivi valori al ricevitore (Arduino Nano) su una porta seriale secondaria
void onDmxFrame(uint16_t universe, uint16_t length, uint8_t* data) {
  for (int i = 0; i < length; i++) {
    Serial2.println(String(i + 1) + ":" + String(data[i]) + "\n");
  }
}

// Gestisce la ricezione di pacchetti Art-Net
void handleArtNetPacket() {

  packetSize = udp.parsePacket();

  if (packetSize > 0 && packetSize <= MAX_BUFFER_LENGTH_ARTNET) {

    //Serial.println("\nReceived Packet!");
    udp.read(packetBuffer, sizeof(packetBuffer));

    // Controllo header corretto
    if (memcmp(packetBuffer, ART_NET, sizeof(ART_NET)) != 0) {
      //Serial.println("Non è un pacchetto Art-Net!");
      return;
    }
    //Serial.println("Header Art-Net OK!");

    // Controllo OpCode (0x5000 che sta per ArtDMX)
    opCode = packetBuffer[8] | (packetBuffer[9] << 8);
    if (opCode != ART_DMX_CODE) {
      //Serial.println("Non è un pacchetto ArtDMX!");
      return;
    }

    // Universo DMX
    universe = packetBuffer[14] | (packetBuffer[15] << 8);

    // Lunghezza dei dati DMX
    length = packetBuffer[17] | (packetBuffer[16] << 8);

    // Dati DMX
    data = &packetBuffer[18];

    // Funzione per gestire i dati DMX e inviarli al ricevitore
    onDmxFrame(universe, length < MAX_CHANNEL_LIMIT ? length : MAX_CHANNEL_LIMIT, data);
  }
}

// Decide se connettersi a un WiFi esistente oppure creare il suo in base alle impostazioni
void setupWiFi() {

  if (useApMode) {
    WiFi.softAP(ssidAP.c_str(), passwordAP.c_str());
    IP = WiFi.softAPIP();
    Serial.println("Rete generata! IP: " + IP.toString());
  } else {
    WiFi.begin(ssidSta.c_str(), passwordSta.c_str());
    Serial.println("Connessione al WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      digitalWrite(LED, !digitalRead(LED));
      Serial.print(".");
    }
    Serial.println("Connesso! IP: " + WiFi.localIP().toString());
  }
}

// Gestisce la pagina web di configurazione
void setupWebPage() {

  // Generazione pagina HTML
  server.on("/", HTTP_GET, []() {
    String page = "<html><head><title>ESP32 DMX Controller</title>";
    page += "<style>";
    page += "body { font-family: Arial, sans-serif; text-align: center; background-color: #f4f4f4; margin: 20px; }";
    page += "h1 { color: #333; }";
    page += "form { background: white; padding: 20px; border-radius: 10px; display: inline-block; text-align: left; box-shadow: 0px 0px 10px rgba(0,0,0,0.1); }";
    page += "input[type='text'], input[type='password'], input[type='submit'] { width: 100%; padding: 8px; margin: 5px 0; border: 1px solid #ccc; border-radius: 5px; }";
    page += "input[type='radio'] { margin-right: 5px; }";
    page += "input[type='submit'] { background: #007bff; color: white; border: none; cursor: pointer; }";
    page += "input[type='submit']:hover { background: #0056b3; }";
    page += "fieldset { border: 1px solid #ddd; padding: 10px; border-radius: 5px; margin-bottom: 15px; }";
    page += "legend { font-weight: bold; }";
    page += "</style></head><body>";

    page += "<h1>ESP32 DMX Controller</h1>";
    page += "<form method='POST' action='/save'>";

    page += "<fieldset><legend>WiFi proprio</legend>";
    page += "SSID: <input type='text' name='ssid_ap' value='" + ssidAP + "'><br>";
    page += "Password: <input type='password' name='password_ap' value='" + passwordAP + "'>";
    page += "</fieldset>";

    page += "<fieldset><legend>WiFi esterno</legend>";
    page += "SSID: <input type='text' name='ssid_sta' value='" + ssidSta + "'><br>";
    page += "Password: <input type='password' name='password_sta' value='" + passwordSta + "'>";
    page += "</fieldset>";

    page += "<fieldset><legend>Modalita' connessione</legend>";
    page += "<label><input type='radio' name='mode' value='ap' " + String(useApMode ? "checked" : "") + "> WiFi proprio</label><br>";
    page += "<label><input type='radio' name='mode' value='sta' " + String(!useApMode ? "checked" : "") + "> Rete esistente</label>";
    page += "</fieldset>";

    page += "<input type='submit' value='Salva'>";
    page += "</form></body></html>";

    server.send(200, "text/html", page);
  });


  // Se viene cliccato il pulsante "Salva"
  server.on("/save", HTTP_POST, []() {
    ssidAP = server.arg("ssid_ap");
    passwordAP = server.arg("password_ap");
    ssidSta = server.arg("ssid_sta");
    passwordSta = server.arg("password_sta");
    useApMode = (server.arg("mode") == "ap");
    saveSettings();
    server.send(200, "text/html", "<html><body><h1>Settings Saved</h1><a href='/'>Back</a></body></html>");
    delay(1000);
    ESP.restart();
  });
}

// Salva le impostazioni su memoria non volatile
void saveSettings() {
  preferences.putString("ssidAP", ssidAP);
  preferences.putString("passwordAP", passwordAP);
  preferences.putString("ssidSta", ssidSta);
  preferences.putString("passwordSta", passwordSta);
  preferences.putBool("useApMode", useApMode);
}

// Carica le impostazione da memoria non volatile
void loadSettings() {
  ssidAP = preferences.getString("ssidAP", ssidAP);
  passwordAP = preferences.getString("passwordAP", passwordAP);
  ssidSta = preferences.getString("ssidSta", "");
  passwordSta = preferences.getString("passwordSta", "");
  useApMode = preferences.getBool("useApMode", true);
}
