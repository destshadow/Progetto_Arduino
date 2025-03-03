#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <WiFiUdp.h>
//#include "thingProperties.h"


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

// Inizializzazione
void setup() {

  Serial.begin(115200);
  Serial2.begin(250000,SERIAL_8N1,16,17);

  preferences.begin("dmx_settings", false);
  loadSettings();

  setupWiFi();
  udp.begin(ART_NET_PORT);

  setupWebPage();
  server.begin();

  // Inizializza la connessione al cloud
  //initProperties();
  //ArduinoCloud.begin(ArduinoIoTPreferredConnection);

  Serial.println("Setup completato.");

}

// Esecuzione principale
void loop() {
  //ArduinoCloud.update();
  server.handleClient();
  handleArtNetPacket();
}

void onArtnetChange()  {
  // Add your code here to act upon Artnet change
  Serial.println("OK");
}

// Invia i canali con i rispettivi valori al ricevitore (Arduino Nano) su una porta seriale secondaria
void onDmxFrame(uint16_t universe, uint16_t length, uint8_t* data) {
  for (int i = 0; i < length; i++) {
    Serial2.println(String(i + 1) + ":" + String(data[i]) + "\n");
  }
}

// Funzione - Gestisce la ricezione di pacchetti Art-Net
void handleArtNetPacket() {
  
  packetSize = udp.parsePacket();

  if (packetSize > 0 && packetSize <= MAX_BUFFER_LENGTH_ARTNET) {

    //Serial.println("\nReceived Packet!");
    udp.read(packetBuffer, sizeof(packetBuffer));

    // Controllo heaer corretto
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

    // Versione protocollo
    //uint16_t protocolVersion = (packetBuffer[10] << 8) | packetBuffer[11];
    //if (protocolVersion < 14) {  // Versione minima accettata (0x0E)
      //Serial.println("Versione protocollo non supportata!");
      //return;
    //}
    //Serial.println("Versione protocollo OK!");

    // Parametri "Sequence" e "Physical"
    //uint8_t sequence = packetBuffer[12];
    //uint8_t physical = packetBuffer[13];
    //Serial.print("Sequence: ");
    //Serial.println(sequence);
    //Serial.print("Physical: ");
    //Serial.println(physical);

    // Universo DMX
    universe = packetBuffer[14] | (packetBuffer[15] << 8); 
    //Serial.print("Universe: ");
    //Serial.println(universe);

    // Lunghezza dei dati DMX
    length = packetBuffer[17] | (packetBuffer[16] << 8);
    //if (length > 512) {
      //Serial.println("Errore: lunghezza DMX non valida!");
      //return;
    //}
    //Serial.print("Lunghezza dati DMX: ");
    //Serial.println(length);

    // Inizio dati DMX
    uint8_t* data = &packetBuffer[18];

    // Funzione per gestire i dati DMX e inviarli al ricevitore
    onDmxFrame(universe, length < MAX_CHANNEL_LIMIT ? length : MAX_CHANNEL_LIMIT, data);

  }
}

// Decide se connettersi a un WiFi esistente oppure creare il suo
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
            Serial.print(".");
        }
        Serial.println("Connesso! IP: " + WiFi.localIP().toString());
    }
}

// Gestisce la pagina web di configurazione
void setupWebPage() {
    server.on("/", HTTP_GET, []() {
        String page = "<html><body><h1>ESP32 DMX Controller</h1>";
        page += "<form method='POST' action='/save'>";
        page += "<h3>AP Mode</h3>SSID: <input type='text' name='ssid_ap' value='" + ssidAP + "'><br>";
        page += "Password: <input type='text' name='password_ap' value='" + passwordAP + "'><br><br>";
        page += "<h3>Station Mode</h3>SSID: <input type='text' name='ssid_sta' value='" + ssidSta + "'><br>";
        page += "Password: <input type='text' name='password_sta' value='" + passwordSta + "'><br><br>";
        page += "<input type='radio' name='mode' value='ap' " + String(useApMode ? "checked" : "") + "> AP Mode<br>";
        page += "<input type='radio' name='mode' value='sta' " + String(!useApMode ? "checked" : "") + "> Station Mode<br><br>";
        page += "<input type='submit' value='Save'>";
        page += "</form></body></html>";
        server.send(200, "text/html", page);
    });

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
