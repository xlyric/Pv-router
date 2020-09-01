///////////////////
//  Pv router by Cyril Poissonnier 2019 
//  Pv routeur pour détecter l'injection par mesure de volume et envoie d'information sur domoticz 
//  pour réponse ( switch, dimmer ... ) 
//  fait pour fonctionner avec le circuit imprimé v1.1 à 1.4
/////////////////////////

///////////////////
// ----- configuration  
// la prise de valeur est de 72 échantillons par cycles
//  ----- envoie vers serveur domotique
//  la programmation par défaut n'envoie pas vers domoticz  ou jeedom, il faut l'activer sur l'interface et sauvegarder la configuration
//  pour le mode autonome, il est possible de spécifier directement l'ip du dimmer numérique distant ( dans le fichier de conf ). la communication doit aussi être active pour fonctionner.
//
//
// ----- sauvegarde de la configuration --- 
//  faire un /get?save pour sauver la configuration dans le fichier
//  sauvegarde du fichier de config : /Config.json
//  
//  /cosphi permet d'avoir des logs sur le calcul du middle et de la valeur retourné :
// valeurs lu  -- calcul du phi conseillé - calcul des sommes et de différent type de calcul de moyennes
// 
//
//  Ajout de la fonction 
// /get?reset pour redémarrer le pv routeur
//  
/////////////////////////
/**************
 *  numeric Dimmer  Information 
 *  **************
 *  
 *  ---------------------- OUTPUT & INPUT Pin table ---------------------
 *  +---------------+-------------------------+-------------------------+
 *  |   Board       | INPUT Pin               | OUTPUT Pin              |
 *  |               | Zero-Cross              |                         |
 *  +---------------+-------------------------+-------------------------+
 *  | ESP8266       | D1(IO5),    D2(IO4),    | D0(IO16),   D1(IO5),    |
 *  |               | D5(IO14),   D6(IO12),   | D2(IO4),    D5(IO14),   |
 *  |               | D7(IO13),   D8(IO15),   | D6(IO12),   D7(IO13),   |
 *  |               |                         | D8(IO15)                |
 *  +---------------+-------------------------+-------------------------+
 */


//***********************************
//************* Déclaration des libraries
//***********************************

// time librairy 
#include <NTPClient.h>
// Dimmer librairy 
#include <RBDdimmer.h>   /// the corrected librairy  in RBDDimmer-master-corrected.rar , the original has a bug
// Web services
#include <ESP8266WiFi.h>
#include <ESPAsyncWiFiManager.h>    
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266HTTPClient.h> 
// File System
#include <fs.h>
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include <ArduinoJson.h> // ArduinoJson : https://github.com/bblanchon/ArduinoJson
// Oled
#include "SSD1306Wire.h" /// Oled ( https://github.com/ThingPulse/esp8266-oled-ssd1306 ) 
// ota mise à jour sans fil
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
//mqtt
#include <PubSubClient.h>
#define mqtt_user "pvrouter"
// Dallas 18b20
//#include <OneWire.h>
//#include <DallasTemperature.h>

// Include custom images
#include "images.h"

const String VERSION = "Version 2.8" ;
String logs;

////BETA
//  int freqmesure = 85; 
  int freqmesure = 72; 
  int readtime = ( 1000000/(50*freqmesure)); 
  int goodcycle = freqmesure - 5;
  String rawdata = "";

//***********************************
//************* Gestion de la configuration
//***********************************

struct Config {
  char hostname[15];
  int port;
  char apiKey[64];
  bool UseDomoticz;
  bool UseJeedom;
  String IDX;
  char otapassword[64];
  int delta;
  int deltaneg;
  int cosphi;
  int readtime;
  int cycle;
  bool sending;
  bool autonome;
  char dimmer[15];
  bool dimmerlocal;
  float facteur;
  int num_fuse;
  bool mqtt;
  char mqttserver[15];
  String IDXdimmer;
  int tmax;
};

const char *filename_conf = "/config.json";
Config config; 

//***********************************
//************* Gestion de la configuration - Lecture du fichier de configuration
//***********************************

// Loads the configuration from a file
void loadConfiguration(const char *filename, Config &config) {
  // Open file for reading
  File configFile = SPIFFS.open(filename_conf, "r");

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/v6/assistant to compute the capacity.
  StaticJsonDocument<1024> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    Serial.println(F("Failed to read file, using default configuration in function loadConfiguration"));
    logs += "Failed to read file, using default configuration in function loadConfiguration/r/n";

  }

  
  // Copy values from the JsonDocument to the Config
  config.port = doc["port"] | 8080;
  strlcpy(config.hostname,                  // <- destination
          doc["hostname"] | "192.168.1.20", // <- source
          sizeof(config.hostname));         // <- destination's capacity
  
  strlcpy(config.apiKey,                  // <- destination
          doc["apiKey"] | "Myapikeystring", // <- source
          sizeof(config.apiKey));         // <- destination's capacity
		  
  config.UseDomoticz = doc["UseDomoticz"] | false; 
  config.UseJeedom = doc["UseJeedom"] | false; 
  config.IDX = doc["IDX"] | 62; 
  config.IDXdimmer = doc["IDXdimmer"] | 60; 
  
  strlcpy(config.otapassword,                  // <- destination
          doc["otapassword"] | "Pvrouteur2", // <- source
          sizeof(config.otapassword));         // <- destination's capacity
  
  config.facteur = doc["facteur"] | 0.86; 
  config.delta = doc["delta"] | 50; 
  config.num_fuse = doc["fuse"] | 70;
  config.deltaneg = doc["deltaneg"] | -100; 
  config.cosphi = doc["cosphi"] | 23; 
  config.readtime = doc["readtime"] | 555;
  config.cycle = doc["cycle"] | 25;
  config.tmax = doc["tmax"] | 65;
  config.sending = doc["sending"] | false;
  config.autonome = doc["autonome"] | true;
  config.mqtt = doc["mqtt"] | false;
  config.dimmerlocal = doc["dimmerlocal"] | false;
  strlcpy(config.dimmer,                  // <- destination
          doc["dimmer"] | "192.168.1.20", // <- source
          sizeof(config.dimmer));         // <- destination's capacity

   strlcpy(config.mqttserver,                  // <- destination
          doc["mqttserver"] | "192.168.1.20", // <- source
          sizeof(config.mqttserver));         // <- destination's capacity
  configFile.close();
      
}

//***********************************
//************* Gestion de la configuration - sauvegarde du fichier de configuration
//***********************************

void saveConfiguration(const char *filename, const Config &config) {
  
  // Open file for writing
   File configFile = SPIFFS.open(filename_conf, "w");
  if (!configFile) {
    Serial.println(F("Failed to open config file for writing in function Save configuration"));
    logs += "Failed to open config file for writing in function Save configuration/r/n";
    return;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<512> doc;

  // Set the values in the document
  doc["hostname"] = config.hostname;
  doc["port"] = config.port;
  doc["apiKey"] = config.apiKey;
  doc["UseDomoticz"] = config.UseDomoticz;
  doc["UseJeedom"] = config.UseJeedom;
  doc["IDX"] = config.IDX;
  doc["IDXdimmer"] = config.IDXdimmer;
  doc["otapassword"] = config.otapassword;
  doc["delta"] = config.delta;
  doc["deltaneg"] = config.deltaneg;
  doc["cosphi"] = config.cosphi;
  doc["readtime"] = config.readtime;
  doc["cycle"] = config.cycle;
  doc["sending"] = config.sending;
  doc["autonome"] = config.autonome;
  doc["dimmer"] = config.dimmer;
  doc["dimmerlocal"] = config.dimmerlocal;
  doc["facteur"] = config.facteur;
  doc["fuse"] = config.num_fuse;
  doc["mqtt"] = config.mqtt;
  doc["mqttserver"] = config.mqttserver;  
  doc["tmax"] = config.tmax;

  // Serialize JSON to file
  if (serializeJson(doc, configFile) == 0) {
    Serial.println(F("Failed to write to file in function Save configuration "));
    logs += "Failed to write to file in function Save configuration:r/n";
  }

  // Close the file
  configFile.close();
}

//***********************************
//************* dimmer
//***********************************

#define outputPin  D7 
#define zerocross  D6 // for boards with CHANGEBLE input pins
dimmerLamp dimmer_hard(outputPin, zerocross); //initialase port for dimmer for ESP8266, ESP32, Arduino due boards
int dimmer_security = 60;  // coupe le dimmer toute les X minutes en cas de probleme externe. 
int dimmer_security_count = 0; 


//***********************************
//************* Dallas 18b20
//***********************************
#define ONE_WIRE_BUS 7
#define TEMPERATURE_PRECISION 10

//***********************************
//************* Time 
//***********************************
const long utcOffsetInSeconds = 3600;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", utcOffsetInSeconds);
int timesync = 0; 
int timesync_refresh = 120; 

void call_time() {
  if ( timesync_refresh >= timesync ) {  timeClient.update(); timesync = 0; }
  else {timesync++;} 
} 
 
String getTime() {
  String state; 
  state = timeClient.getHours() + ":" + timeClient.getMinutes() ; 
  return String(state);
}

//***********************************
//************* Afficheur Oled
//***********************************
const int I2C_DISPLAY_ADDRESS = 0x3c;
SSD1306Wire  display(0x3c, D1, D2);

//***********************************
//************* Constantes
//***********************************
//constantes de mesure

#define OLED 1  // activation de l'afficheur oled
#define attente 2 /// nombre de 1/2 secondes entre chaque cycle de mesure.

int middle ;


//constantes de fonctionnement
#define USE_SERIAL  Serial
#define linky  A0
#define lcd_scd D1  // LCD et OLED
#define lcd_scl D2  // LCD et OLED 
#define sens D5   /// niveau de la diode 
int dimmer_power = 0;
int security=0; // envoie régulier de 0 vers le dimmer dans certains cas 

/// constantes de debug 
#define modeserial 0
//int inversion = 0 ; 
int phi;
int somme, sigma ;
int signe = 0;
int temp = 0; 
int timer = 0 ; 
String retour; 
int nbtent=0;
int moyenne, laststartvalue; 
String message; 
String configweb , memory, inputMessage;
int inj_mode = 0 ; 
//int RMS = 0 ;
bool change;
//String serialmessage; 
bool middlevalue=0;
int bypassfalse=0;


int bestcosphi, bestpuissance; 
//***********************************
//************* Gestion du serveur WEB
//***********************************
// Create AsyncWebServer object on port 80
WiFiClient domotic_client;
// mqtt
PubSubClient client(domotic_client);

AsyncWebServer server(80);
DNSServer dns;
HTTPClient http;


// creation des fonctions de collect 

//***********************************
//************* retour des pages
//***********************************

String getState() {
  String state ; 
  if ( inj_mode == 0 ) {   state = "Linky"; }
  if ( inj_mode == 1 ) {   state = "Injection"; }
  if ( inj_mode == 2 ) {   state = "Stabilise"; }
  //int sigma = somme ;
  state = state + ";" + sigma + ";" + dimmer_power ;
  return String(state);
}

const char* PARAM_INPUT_1 = "send"; /// paramettre de retour sendmode
const char* PARAM_INPUT_2 = "cycle"; /// paramettre de retour cycle
const char* PARAM_INPUT_3 = "readtime"; /// paramettre de retour readtime
const char* PARAM_INPUT_4 = "cosphi"; /// paramettre de retour cosphi
const char* PARAM_INPUT_save = "save"; /// paramettre de retour cosphi
const char* PARAM_INPUT_dimmer = "dimmer"; /// paramettre de retour cosphi
const char* PARAM_INPUT_server = "server"; /// paramettre de retour server domotique
const char* PARAM_INPUT_IDX = "idx"; /// paramettre de retour idx
const char* PARAM_INPUT_IDXdimmer = "idxdimmer"; /// paramettre de retour idx
const char* PARAM_INPUT_port = "port"; /// paramettre de retour port server domotique
const char* PARAM_INPUT_delta = "delta"; /// paramettre retour delta
const char* PARAM_INPUT_deltaneg = "deltaneg"; /// paramettre retour deltaneg
const char* PARAM_INPUT_fuse = "fuse"; /// paramettre retour fusible numérique
const char* PARAM_INPUT_API = "apiKey"; /// paramettre de retour apiKey
const char* PARAM_INPUT_servermode = "servermode"; /// paramettre de retour activation de mode server
const char* PARAM_INPUT_dimmer_power = "POWER"; /// paramettre de retour activation de mode server
const char* PARAM_INPUT_facteur = "facteur"; /// paramettre retour delta
const char* PARAM_INPUT_tmax = "tmax"; /// paramettre retour delta
const char* PARAM_INPUT_mqttserver = "mqttserver"; /// paramettre retour mqttserver
const char* PARAM_INPUT_reset = "reset"; /// paramettre retour mqttserver

String stringbool(bool mybool){
  String truefalse = "true";
  if (mybool == false ) {truefalse = "";}
  return String(truefalse);
  }

String getSendmode() {
  String sendmode;
  if ( config.sending == 0 ) {   sendmode = "Off"; }
  else {   sendmode = "On"; }
  return String(sendmode);
}



String getServermode(String Servermode) {
  if ( Servermode == "Domoticz" ) {   config.UseDomoticz = !config.UseDomoticz; }
  if ( Servermode == "Jeedom" ) {   config.UseJeedom = !config.UseJeedom;}
  if ( Servermode == "autonome" ) {   config.autonome = !config.autonome; }
  if ( Servermode == "dimmer local" ) {   config.dimmerlocal = !config.dimmerlocal; }
  if ( Servermode == "mqtt" ) {   config.mqtt = !config.mqtt; }

return String(Servermode);
}

String getSigma() {
  
  return String(sigma);
}


String getcosphi() {
  
  return String(rawdata) ;
}


String getpuissance() {
  int tempvalue=15;
  
  return String(bestpuissance*config.facteur) ;
}



String getconfig() {
    
  configweb = String(config.hostname) + ";" +  config.port + ";"  + config.IDX + ";"  +  VERSION +";" + middle +";"+ config.delta +";"+config.cycle+";"+config.dimmer+";"+config.cosphi+";"+config.readtime +";"+stringbool(config.UseDomoticz)+";"+stringbool(config.UseJeedom)+";"+stringbool(config.autonome)+";"+config.apiKey+";"+stringbool(config.dimmerlocal)+";"+config.facteur+";"+stringbool(config.mqtt)+";"+config.mqttserver+";"+config.deltaneg;
  return String(configweb);
}

String getchart() {
	  oscilloscope() ;
      return String(retour);
}

String getdebug() {
  configweb = String(ESP.getFreeHeap())+ ";" + String(laststartvalue)  + ";" +  String(middle) + ";" +  phi +";"+ dimmer_power ;
    return String(configweb);
}

String getmemory() {
  
    memory = String(ESP.getFreeHeap()) + ";" + String(ESP.getHeapFragmentation()) + ";" + String(ESP.getMaxFreeBlockSize());
      return String(memory);
}

String getlogs() {
    
      return String(logs);
}


String processor(const String& var){
   Serial.println(var);
   if (var == "SIGMA"){
    return getSigma();
  }
  else if (var == "SENDMODE"){
  
    return getSendmode();
  }
  else if (var == "STATE"){
    
    return getState();
  }  
  
}


					//***********************************
					//************* Setup 
					//***********************************

void setup() {
// init port
  pinMode(linky, INPUT);
  
  Serial.begin(115200);
  Serial.println();

  // Initialising OLED
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.clear();

  //démarrage file system
  SPIFFS.begin();
  Serial.println("Demarrage file System");

  // vérification de la présence d'index.html
  if(!SPIFFS.exists("/index.html")){
    Serial.println("Attention fichiers SPIFFS non chargé sur l'ESP, ça ne fonctionnera pas.");  
    logs += "Attention fichiers SPIFFS non chargé sur l'ESP, ça ne fonctionnera pas.";
    }
  
		//***********************************
		//************* Setup -  récupération du fichier de configuration
		//***********************************
  
  // Should load default config if run for the first time
  Serial.println(F("Loading configuration..."));
   logs += "Loading configuration.../r/n";
  loadConfiguration(filename_conf, config);

  // Create configuration file
  Serial.println(F("Saving configuration..."));
  logs += "Saving configuration... /r/n";
  saveConfiguration(filename_conf, config);
  

  // configuration dimmer
  dimmer_hard.begin(NORMAL_MODE, ON); //dimmer initialisation: name.begin(MODE, STATE) 
  dimmer_hard.setPower(0); 
  USE_SERIAL.println("Dimmer started...");
   
   // configuration Wifi
  AsyncWiFiManager wifiManager(&server,&dns);
  wifiManager.autoConnect("pvrouter", "pvrouter");

  
		//***********************************
		//************* Setup - Connexion Wifi
		//***********************************
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  //Si connexion affichage info dans console
  Serial.println("");
  Serial.print("Connection ok sur le reseau :  ");
 
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); 
  Serial.println(ESP.getResetReason());
  // mqtt
  if (config.mqtt==1) { client.connect("pvrouter"); }
		//***********************************
		//************* Setup - OTA 
		//***********************************
		
  ArduinoOTA.setHostname("PV Routeur");
  //ArduinoOTA.setPassword(otapassword);
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  

		//***********************************
		//************* Setup - Web pages
		//***********************************

  server.on("/",HTTP_ANY, [](AsyncWebServerRequest *request){
      if(SPIFFS.exists("/index.html")){
     request->send(SPIFFS, "/index.html", String(), false, processor);
    }
    else {request->send_P(200, "text/plain", "fichiers SPIFFS non présent sur l ESP. " ); }
  });

    server.on("/config.html",HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/config.html", String(), false, processor);
  });

  server.on("/all.min.css", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/all.min.css", "text/css");
  });

    server.on("/favicon.ico", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.ico", "image/png");
  });

  server.on("/fa-solid-900.woff2", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/fa-solid-900.woff2", "text/css");
  });
  
    server.on("/sb-admin-2.js", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/sb-admin-2.js", "text/javascript");
  });

  server.on("/sb-admin-2.min.css", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/sb-admin-2.min.css", "text/css");
  });

  server.on("/chart.json", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send_P(200, "application/json", getchart().c_str());
  }); 
  
  server.on("/sendmode", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getSendmode().c_str());
  });
  
  server.on("/state", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getState().c_str());
  });

  server.on("/serial", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getState().c_str());
  });
  
  server.on("/config", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getconfig().c_str());
  });

  server.on("/memory", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getmemory().c_str());
  });

  server.on("/debug", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getdebug().c_str());
  });

  server.on("/log", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getlogs().c_str());
  });

server.on("/config.json", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/config.json", "application/json");
  });

/// beta
server.on("/cosphi", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getcosphi().c_str());
  });
  
server.on("/puissance", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain",  getpuissance().c_str());
  });

/////////////////////////
////// mise à jour parametre d'envoie vers domoticz et récupération des modifications de configurations
/////////////////////////

server.on("/get", HTTP_ANY, [] (AsyncWebServerRequest *request) {
      ///   /get?send=on
    if (request->hasParam(PARAM_INPUT_1)) 		  { inputMessage = request->getParam(PARAM_INPUT_1)->value();
													config.sending = 0; 
													if ( inputMessage != "On" ) { config.sending = 1; }
													request->send(200, "text/html", getSendmode().c_str()); 	}
	   // /get?cycle=x
    if (request->hasParam(PARAM_INPUT_save)) { Serial.println(F("Saving configuration..."));
                          saveConfiguration(filename_conf, config);   
                            }
                           
	 if (request->hasParam(PARAM_INPUT_2)) { config.cycle = request->getParam(PARAM_INPUT_2)->value().toInt(); }
	 if (request->hasParam(PARAM_INPUT_3)) { config.readtime = request->getParam(PARAM_INPUT_3)->value().toInt();}
	 if (request->hasParam(PARAM_INPUT_4)) { config.cosphi = request->getParam(PARAM_INPUT_4)->value().toInt();  }
   if (request->hasParam(PARAM_INPUT_dimmer)) { request->getParam(PARAM_INPUT_dimmer)->value().toCharArray(config.dimmer,15);  }
   if (request->hasParam(PARAM_INPUT_server)) { request->getParam(PARAM_INPUT_server)->value().toCharArray(config.hostname,15);  }
   if (request->hasParam(PARAM_INPUT_mqttserver)) { request->getParam(PARAM_INPUT_mqttserver)->value().toCharArray(config.mqttserver,15);  }
   if (request->hasParam(PARAM_INPUT_delta)) { config.delta = request->getParam(PARAM_INPUT_delta)->value().toInt(); }
   if (request->hasParam(PARAM_INPUT_deltaneg)) { config.deltaneg = request->getParam(PARAM_INPUT_deltaneg)->value().toInt(); }
   if (request->hasParam(PARAM_INPUT_fuse)) { config.num_fuse = request->getParam(PARAM_INPUT_fuse)->value().toInt(); }
   if (request->hasParam(PARAM_INPUT_port)) { config.port = request->getParam(PARAM_INPUT_port)->value().toInt(); }
   if (request->hasParam(PARAM_INPUT_IDX)) { config.IDX = request->getParam(PARAM_INPUT_IDX)->value().toInt();}
   if (request->hasParam(PARAM_INPUT_IDXdimmer)) { config.IDXdimmer = request->getParam(PARAM_INPUT_IDXdimmer)->value().toInt();}
   if (request->hasParam(PARAM_INPUT_API)) { request->getParam(PARAM_INPUT_API)->value().toCharArray(config.apiKey,64);}
   if (request->hasParam(PARAM_INPUT_dimmer_power)) {dimmer_power = request->getParam( PARAM_INPUT_dimmer_power)->value().toInt(); change = 1 ;  } 
   if (request->hasParam(PARAM_INPUT_facteur)) { config.facteur = request->getParam(PARAM_INPUT_facteur)->value().toFloat();}
   if (request->hasParam(PARAM_INPUT_tmax)) { config.tmax = request->getParam(PARAM_INPUT_tmax)->value().toInt();}
   //reset
   if (request->hasParam(PARAM_INPUT_reset)) {Serial.println("Resetting ESP");  ESP.restart();}
   
   
   if (request->hasParam(PARAM_INPUT_servermode)) { inputMessage = request->getParam( PARAM_INPUT_servermode)->value();
                                            getServermode(inputMessage);
                                            request->send(200, "text/html", getconfig().c_str());
                                        }

  
    request->send(200, "text/html", getconfig().c_str());

	});


    //***********************************
    //************* mise en route de Time
    //***********************************


  timeClient.begin();
  timeClient.update();
		
		//***********************************
		//************* Setup -  demarrage du webserver et affichage de l'oled
		//***********************************



  server.begin(); 
  affiche_info_main();
  
  if (config.mqtt==1) { client.setServer(config.mqttserver, 1883);
  mqtt(config.IDXdimmer,"0");  }

  /// reset dimmer
  int httpCode;
  String baseurl; 
   dimmer_power=0;
   baseurl = "/?POWER=" + String(dimmer_power) ; http.begin(config.dimmer,80,baseurl);    httpCode = http.GET();  http.end();
   delay(5000); 
  

 
}

						//***********************************
						//************* loop
						//***********************************


void loop() {

/// preparation des mesures
  temp = 0; 
  timer = 0 ; 

  somme = 0; 
  int i = 0;
  int validation = 0;

// lecture de la valeur moyenne 
   
  valeur_moyenne();

/// recherche d'un cycle stable
  if ( middlevalue == 1 || bypassfalse >= 5 ) {
 
sigma = somme * config.facteur ;

//**************************
////  détection traitement des conso
//**************************

// production
  if ( sigma > config.delta ) {
 
    if ( modeserial == 1 )  {Serial.println("prod"); }
  
    message = "Mode Linky        "; 
    inj_mode=0;

    if ( config.sending == 1 ) {
       SendToDomotic(String(sigma));  
    
      
       delay(2*attente*1000); 
    }
  }
  
//**************************
// injection 
//**************************
  else if ( sigma < config.deltaneg ) {
    
    if ( modeserial == 1 )  {Serial.println("inj"); }
    
    message = "Correc injection    ";
    inj_mode=1;
    
    if ( config.sending == 1) {
            SendToDomotic(String(sigma));  
            
            delay(2*attente*1000); 
    
    } 
    
  }
  else {
		message = "Mode stabilise    " ;
		inj_mode=2;
    if ( config.sending == 1 ) {	 security ++; if ( security >=6 ) { SendToDomotic(String(sigma)); delay(5*attente*1000); security =0; }  }  // envoie de l'info de temps en temps pour les logs
  }
  affiche_info_injection(inj_mode);
  affiche_info_volume(sigma);
  if (config.autonome == 1 ) {dimmer(inj_mode); }

  }
  else 
  {
    bypassfalse++;
  }



//pause de X s secondes avant prochaine mesure

affiche_info_transmission(config.sending);
 delay (500*attente);
 ArduinoOTA.handle();

 dimmer_security_count ++; 
 if ( dimmer_security_count >= dimmer_security *120 )  {
      dimmer_security_count = 0; 
      /// dimmer security cut every %dimmer_security% minutes
        dimmer_hard.setPower(0);        
                                                  }

  if ( config.dimmerlocal == 1 && change == 1   ) {  dimmer_hard.setPower(5) ; delay ( 250 ) ; dimmer_hard.setPower(dimmer_power) ; change = 0 ; }      
            
  delay (1000);                       
 }
 
// *************** FIN DE LOOP ********************


// ***********************************
// ************* Fonction interne
// ***********************************

// ***********************************
// ** recherche de changement sur la diode.
// ***********************************
void front(int variation) {
  int Watchdog=0 ;  int Watchdog_alert=0; 
	while ( digitalRead(sens) == variation ) {
	delayMicroseconds (25);
  Watchdog++;
  if ( Watchdog > 10000 && Watchdog_alert == 0 ) {  Serial.print("Attention pas de porteuse, alimentation 12v AC ou pont redresseur débranché ? "); Watchdog_alert =1 ; } 

	}

 
}



// ***********************************
// ************* Fonction  d'affichage
// ***********************************

// ***********************************
// ************* main 
// ***********************************

void affiche_info_main() {
  display.clear();
  
  display.drawXbm(0,0, 16, 16, antenne_logo);
  
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(63, 0, String(WiFi.localIP().toString()));
  
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  ////// affichage de l'état conf
  display.drawString(0,54, VERSION );
  affiche_info_transmission(config.sending);
  
  display.display();
}

// ***********************************
// ************* affichage transmission
// ***********************************

void affiche_info_transmission (int transmission ){
  display.setColor(BLACK);
  display.fillRect(0, 16, 50, 10);
  
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
	if ( transmission == 0 ) {   display.drawString(0,16, "Trans Off"); }
	else {   display.drawString(0,16, "Trans On "); }
  
  display.display();
}

// ***********************************
// ************* affichage du sigma et heure
// ***********************************

void affiche_info_volume(int volume ) {
  affiche_heure(); // Affichage de l'heure
  display.setColor(BLACK);
  display.fillRect(55, 30, 128, 64);
  
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(85,30, String(volume) + " W"); 
  display.drawString(85,47, String(dimmer_power) + " %"); 
  
  
  display.display();
}

// ***********************************
// ************* affichage mode de conso
// ***********************************

void affiche_info_injection(int injection_mode ) {
  display.setColor(BLACK);
  display.fillRect(75, 16, 128, 26);
  
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(100,16, "              ");
  
  if ( injection_mode == 0 ) {   display.drawString(100,16, "Linky"); }
  else if ( injection_mode == 1 ) {   display.drawString(100,16, "Injection"); }
  else {   display.drawString(100,16, "Stabilise"); }
  
  display.display();
}

// ***********************************
// ************* affichage des delta
// ***********************************

void affiche_info_delat() {

display.setColor(BLACK);
display.fillRect(0, 27, 55, 28);

display.setColor(WHITE);
display.setTextAlignment(TEXT_ALIGN_LEFT);
display.setFont(ArialMT_Plain_10);
display.drawString(0,29, "Delta : " + String(config.delta));
display.drawString(0,41, "Neg : " + String(config.deltaneg));

display.display();
}

// ***********************************
// ************* affichage Heure
// ***********************************
void affiche_heure()
{
String ActualTime = timeClient.getFormattedTime();
//String ActualTime = timeClient.getHours()+":"+timeClient.getMinutes() ;
display.setColor(BLACK);
display.fillRect(0, 27, 55, 28);

display.setColor(WHITE);
display.setTextAlignment(TEXT_ALIGN_LEFT);
display.setFont(ArialMT_Plain_10);
display.drawString(0,29, ActualTime);
//display.drawString(0,41, "Neg : " + String(config.deltaneg));

display.display();
}


// ***********************************
// ************* affichage logo transmission
// ***********************************
void affiche_transmission(int actif) {
 if ( actif == 1 ) { 
 display.drawXbm(111,0 , 16, 16, antenne_logo2); 
 }
 else {
  display.setColor(BLACK);
  display.fillRect(111, 0, 127, 16);
  display.setColor(WHITE); 
  }
  display.display();
}



//***********************************
//** Oscillo mode creation du tableau de mesure pour le graph
//***********************************

void oscilloscope() {

  int starttime,endtime; 
  timer = 0; 

  retour = "[[";
  front (0);
  front(1);
  delayMicroseconds (config.cosphi*config.readtime); // correction décalage
  while ( timer < ( freqmesure ) )
  {

  
  temp =  analogRead(linky); signe = digitalRead(sens);
  moyenne = middle - 25 + signe*50; 
  //moyenne = moyenne + abs(temp - middle) ;
  /// mode oscillo graph 

  
  retour += String(timer) + "," + String(moyenne) + "," + String(temp) + "],[" ; 
  timer ++ ;
  delayMicroseconds (config.readtime);
  } 
  
  temp =  analogRead(linky); signe = digitalRead(sens);
  moyenne = middle - 25 + signe*50; 
  retour += String(timer) + "," + String(moyenne) + "," + String(temp) + "]]" ;
 

  
}

//***********************************
//************* Fonction mesure
//***********************************
/*
int mesure () {

  temp = 0,
  timer = 0; 
  //RMS = 0; 
  int startvalue = 0; 
  somme = 0; 
   Serial.print("middle : ");
   Serial.println(middle);
  
  front(0);
  front(1);
  
  //delayMicroseconds (config.cosphi*config.readtime); // correction décalage
 // temp =  analogRead(linky);
  // somme = abs(temp - middle); 
  //somme = abs(somme ); 
  //if ( somme < 5 ) { somme = temp - middle ; timer ++; startvalue = temp; }
  //else {somme = 0 ; } 
  //startvalue = temp; 
  
// analyse par volumes
 // delayMicroseconds (config.readtime);

//// lecture dans la période de la demi alternance fait par la diode  pour calcul de l'air
  while ( timer < 24 )
  {
    temp =  analogRead(linky); 
    //signe = digitalRead(sens);
    //if ( signe == 0 ) {
  //   RMS = RMS + abs(temp - middle) ; 
      somme = somme + temp - middle ;
     // if (timer ==0) {startvalue = temp; }
    // }
  timer ++ ;
  delayMicroseconds (readtime); // frequence de 100Hz environ
  } 
   Serial.print("valeur depart : ");
   Serial.println(startvalue);
   
   Serial.print("cycle :");
   Serial.println(timer);
   Serial.print("somme :");
   Serial.println(somme);
      laststartvalue = startvalue;
  return (somme);
}

*/


//***********************************
//************* Fonction valeur moyenne et calcul 
//***********************************
void valeur_moyenne() {

  String log="";
  float moyenne = 0; 
  int temp = 0 ;
  int j = 0;
  int i = 0; 
  int up = 0; 
  middlevalue = 0;
  int sommetemp = 0;
  int sommeinverse = 0;  
  int sommetotale = 0; 
  int demicycle = freqmesure/2; 
  int derive = 0 ; 

  int min=1024; 
  int max=0; 
  int startvalue=0; 
  unsigned long startMillis;

  
  //// attente du prochain cycle 

 while (digitalRead(sens) != 1 ) {
        delayMicroseconds (100);
   }
   
  while (digitalRead(sens) != 0 ) {
        delayMicroseconds (10);
   }

  startMillis = micros();

  ////  calcul de la 1/2 onde 
  //// rajout de la condition && j < (demicycle-20)  pour éviter les rebonds 
  while (digitalRead(sens) != 1 && j < (demicycle) ) {
  temp =  analogRead(linky);
  
    if ( ( j >= config.cosphi  && j < (demicycle+config.cosphi) )) {  
        sommetemp += temp;  i++; 
        log += String(temp) + "-" ; 
        }
    else  {
      log += String(temp) + " " ;
      sommeinverse += temp; 
      }
    if ( j == config.cosphi ) { startvalue = temp; }
    if ( temp > max )  { max =temp ; up = j; } 
    if ( temp < min )  { min = temp ; }
    j ++;  
    rt_loop( startMillis, readtime*j ) ; 
    
  }
  log += + "++" ;
  /// calcul de la 2em 1/2 onde 
  while (digitalRead(sens) != 0 && j != freqmesure ) {
  temp =  analogRead(linky);
    if ( j < (demicycle+config.cosphi) ) { 
      sommetemp += temp; i++;  
       log += String(temp) + "-" ;
      }
    else  {
       log += String(temp) + " " ;
      sommeinverse += temp; 
      }
    if ( temp > max )  { max =temp ; up = j;  } 
    if ( temp < min )  { min = temp ; }
    j ++; 
    rt_loop( startMillis, readtime*j ) ; 
  
  }
  // affichage du temps pris
  startMillis =  micros() - startMillis ;
  Serial.print("temp: ");
  Serial.print(startMillis);  


  sommetotale = ( sommetemp + sommeinverse )  ; 
  
  moyenne = float(sommetotale) / float(freqmesure) ;
  float moyenne2 =  ((float(min) + float(max)) / 2.0) ;
  float moyenne3 = config.cycle; 
  

  somme = (sommetemp - (moyenne*demicycle)) ; 
  float somme2 = (sommetemp - (moyenne2*demicycle)) ; 
  float somme3 = (sommetemp - (moyenne3*demicycle)) ; 

    
  //sommeinverse = (sommeinverse - (moyenne*demicycle) ); 
  //somme =  (somme + sommeinverse)/2 ; 
  float inverse1 =  (sommeinverse - (moyenne*demicycle)); 
  float inverse2 =  (sommeinverse - (moyenne2*demicycle)) ; 
  float inverse3 =  (sommeinverse - (moyenne3*demicycle)) ; 

/*

sommetotale = ( sommetemp + sommeinverse )  ; 
moyenne = float(sommetotale) / float(freqmesure);  
somme = (sommetemp - (moyenne*demicycle)) - (sommeinverse - (moyenne*demicycle)) ; 
somme = sommetemp - sommeinverse ; 
*/
  
  
   if ( j >=goodcycle || bypassfalse >= 5  ) {      
    Serial.println("-------------");
     Serial.print("valeur moyenne");
     Serial.println(moyenne);
     Serial.print("cycle : ");
     Serial.println(j);
     Serial.print("demi : ");
     Serial.println(i);
    
   
      Serial.print("cosphi :");
     Serial.println(config.cosphi);
     Serial.print("sommetemp :");
     Serial.println(sommetemp);
     
     Serial.print("somme :");
     Serial.println(somme);  

     Serial.print("min max :");
     Serial.print(min );
     Serial.print(" ");
     Serial.println(max);

     Serial.print("START :");
     Serial.println(startvalue );
     laststartvalue = startvalue ;
     Serial.println(log);
     
      middle = moyenne ;
      middlevalue = 1; 

      long rssi = WiFi.RSSI();
      Serial.print("RSSI:");
      Serial.println(rssi);
                }

   else { 
      Serial.println("erreur cycle :");
      Serial.println(j);
        }
  /// retrait de rowdata de la condition pour debug    
rawdata = log + "  \r\n -- phi : " + ( up - (j/4) ) +  " somme : " + sommetemp  + " -- somme inverse : " + sommeinverse + "\r\n -- middle : " + moyenne  +  "-- somme :" + somme +  "-- inv :" + inverse1 + "  \r\n -- moyenne 2 : " +  moyenne2 + " : " + somme2 + "-- inv :" + inverse2 + " total " + (somme2 - inverse2 )/2 + " \r\n -- moyenne 3 : " +  moyenne3 + " : " + somme3 + "-- inv :" + inverse3 + " total " + (somme3 - inverse3 )/2 +" temps" + startMillis;



}

//////////////////////// RT loop

void rt_loop (unsigned long startmicro, unsigned long timer )
{

while ( micros() <=  ( startmicro + timer ) )
    {
    delayMicroseconds(1);
    }

 
}



//***********************************
//************* Fonction domotique 
//***********************************

boolean SendToDomotic(String Svalue){
  String baseurl; 
  Serial.print("connecting to mqtt & dimmer");
  Serial.println(config.hostname);
  //Serial.print("Requesting URL: ");

  //if ( config.UseDomoticz == 1 ) { baseurl = "/json.htm?type=command&param=udevice&idx="+config.IDX+"&nvalue=0&svalue="+Svalue;  http.begin(config.hostname,config.port,baseurl); }
  //if ( config.UseJeedom == 1 ) {  baseurl = "/core/api/jeeApi.php?apikey=" + String(config.apiKey) + "&type=virtual&id="+ config.IDX + "&value=" + Svalue; http.begin(config.hostname,config.port,baseurl);  }
  if ( config.mqtt == 1 ) {     mqtt(config.IDX,Svalue);  }
  Serial.println(baseurl);

  // http.begin(config.hostname,config.port,baseurl);
  //int httpCode = http.GET();
  //Serial.println("closing connection");
  //http.end();

  if ( config.autonome == 1 && change == 1   ) {  baseurl = "/?POWER=" + String(dimmer_power) ; http.begin(config.dimmer,80,baseurl);   int httpCode = http.GET();
    http.end(); 
    if ( config.mqtt == 1 ) { mqtt(config.IDXdimmer, String(dimmer_power));  }

      
    
    }

valeur_moyenne();  
 
}


//***********************************
//************* Fonction aservissement autonome
//***********************************

void dimmer(int commande){
  change = 0; 
  
  
	if ( sigma >= 350 && dimmer_power != 0 ) { dimmer_power = 0 ;  change = 1 ;} // si grosse puissance instantanée sur le réseau, coupure du dimmer. ( ici 350w environ ) 
	else if (commande == 0 && dimmer_power != 0 && sigma >= (config.delta+20) ) { dimmer_power += -2*((sigma-config.delta)/50) ; change = 1; }  /// si mode linky  on reduit la puissance 
	else if (commande == 0 && dimmer_power != 0 ) { dimmer_power += -1 ; change = 1; }  /// si mode linky  on reduit la puissance 
  else if (commande == 1 && sigma <= (config.deltaneg-20) ) {   dimmer_power += 2*abs(sigma/50) ; change = 1 ; } /// si injection on augmente la puissance  ( charge de 1 KW ) 
  else if (commande == 1  ) { dimmer_power += 1 ; change = 1 ; } /// si injection on augmente la puissance
  //else if (commande == 1  ) { dimmer_power += round(sigma/(bestpuissance * config.facteur)*100)  ; change = 1 ; } /// si injection on augmente la puissance
  
  
if ( dimmer_power >= config.num_fuse ) {dimmer_power = config.num_fuse; change = 1 ; }
if ( dimmer_power <= 0 && dimmer_power != 0 ) {dimmer_power = 0; change = 1 ; }

if (commande == 2  ) { change = 1 ; } /// mode stabilisé, envoie de sigma de temps en temps

security ++ ;
if ( security >= 5 ) { if ( dimmer_power <= 0 ) {dimmer_power = 0; change = 1 ; security = 0;  }} 


}


void mqtt(String idx, String value)
{
  String nvalue = "0" ; 
  if ( value != "0" ) { nvalue = "2" ; }
String message = "  { \"idx\" : " + idx +" ,   \"svalue\" : \"" + value + "\",  \"nvalue\" : " + nvalue + "  } ";

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  client.publish("domoticz/in", String(message).c_str(), true);
  
}




void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world it's me !! PvRouter ");
      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
/*
int getBarsSignal(long rssi){
  // 5. High quality: 90% ~= -55db
  // 4. Good quality: 75% ~= -65db
  // 3. Medium quality: 50% ~= -75db
  // 2. Low quality: 30% ~= -85db
  // 1. Unusable quality: 8% ~= -96db
  // 0. No signal
  int bars;
  
  if (rssi > -55) { 
    bars = 5;
  } else if (rssi < -55 & rssi > -65) {
    bars = 4;
  } else if (rssi < -65 & rssi > -75) {
    bars = 3;
  } else if (rssi < -75 & rssi > -85) {
    bars = 2;
  } else if (rssi < -85 & rssi > -96) {
    bars = 1;
  } else {
    bars = 0;
  }
  return bars;
}


void affiche_heure() 
{
String ActualTime = timeClient.getFormattedTime();
//String ActualTime = timeClient.getHours()+":"+timeClient.getMinutes() ;
  display.setColor(BLACK);
  display.fillRect(28, 28, 38, 38);
  
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0,28,ActualTime);
 
  display.display();

  
}
*/
