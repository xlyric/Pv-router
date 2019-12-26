///////////////////
//  Pv router by Cyril Poissonnier 2019 
//  Pv routeur pour détecter l'injection par mesure de volume et envoie d'information sur domoticz 
//  pour réponse ( switch, dimmer ... ) 
//  fait pour fonctionner avec le circuit imprimé v1.1 à 1.3
/////////////////////////

///////////////////
// ----- configuration  
//  la valeur de la fréquence d échantillonage est modifiable mais déconseillé. 
//  calcul pour cos phi = 0 avec une charge résistive récupérable sur  /debug  ( int  représentant le nb de cycle avant passage par 0 ( middle ) ) 
//  puis la valeur de cos phi sur une valeur max obtenu à la lecture directe. 
//  valeur à entrer dans /get?cosphi=x ( préreglé à 12 ) en cas d'inversion de branchement de la sonde, ajouter 18 ( 1/2 cycle ) >> 30
//  le cycle de mesure est de 555ms ( /get?reatime = 555 pour 36 mesures par onde ( 50hz - 20ms ). 
//  la prise de donnée oscillo est définie par  /get?cycle=72 ( 72 mesures ) 
//  ----- envoie vers serveur domotique
//  la programmation par défaut n'envoie pas vers domoticz  ou jeedom, il faut l'activer sur l'interface et sauvegarder la configuration
//  pour le mode autonome, il est possible de spécifier directement l'ip du dimmer numérique distant ( dans le fichier de conf ). la communication doit aussi être active pour fonctionner.
//
//
// ----- sauvegarde de la configuration --- 
//  faire un /get?save pour sauver la configuration dans le fichier
//  sauvegarde du fichier de config : /Config.json
//  
/////////////////////////



//***********************************
//************* Déclaration des libraries
//***********************************

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

// Include custom images
#include "images.h"

const String VERSION = "Version 2.1" ;

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
  int cosphi;
  int readtime;
  int cycle;
  bool sending;
  bool autonome;
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
  StaticJsonDocument<512> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, configFile);
  if (error)
    Serial.println(F("Failed to read file, using default configuration"));

  // Copy values from the JsonDocument to the Config
  config.port = doc["port"] | 8080;
  strlcpy(config.hostname,                  // <- destination
          doc["hostname"] | "192.168.1.20", // <- source
          sizeof(config.hostname));         // <- destination's capacity
  
  strlcpy(config.apiKey,                  // <- destination
          doc["apiKey"] | "Myapikeystring", // <- source
          sizeof(config.apiKey));         // <- destination's capacity
		  
  config.UseDomoticz = doc["UseDomoticz"] | true; 
  config.UseJeedom = doc["UseJeedom"] | false; 
  config.IDX = doc["IDX"] | 36; 
  
  strlcpy(config.otapassword,                  // <- destination
          doc["otapassword"] | "Pvrouteur2", // <- source
          sizeof(config.otapassword));         // <- destination's capacity
  
  config.delta = doc["delta"] | 50; 
  config.cosphi = doc["cosphi"] | 11; 
  config.readtime = doc["readtime"] | 555;
  config.cycle = doc["cycle"] | 72;
  config.sending = doc["sending"] | false;
  config.autonome = doc["autonome"] | true;
  configFile.close();
      
}

//***********************************
//************* Gestion de la configuration - sauvegarde du fichier de configuration
//***********************************

void saveConfiguration(const char *filename, const Config &config) {
  
  // Open file for writing
   File configFile = SPIFFS.open(filename_conf, "w");
  if (!configFile) {
    Serial.println(F("Failed to open config file for writing"));
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
  doc["otapassword"] = config.otapassword;
  doc["delta"] = config.delta;
  doc["cosphi"] = config.cosphi;
  doc["readtime"] = config.readtime;
  doc["cycle"] = config.cycle;
  doc["sending"] = config.sending;
  doc["autonome"] = config.autonome;
  

  // Serialize JSON to file
  if (serializeJson(doc, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  // Close the file
  configFile.close();
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

//int middle = 541;  /// recalculé automatiquement à chaque cycle
int middle ;
int front_size=19 ; // ( n +1 )  cycle d'onde de 36 mesures ( 360°/10 soit 10° d'onde par mesure ) 

//constantes de fonctionnement
#define USE_SERIAL  Serial
#define linky  A0
#define lcd_scd D1  // LCD et OLED
#define lcd_scl D2  // LCD et OLED 
#define sens D5   /// niveau de la diode 
#define pinreset 1 /// si D0 et reset connecté >> 1
int num_fuse = 50 ; /// limitation de la puissance du dimmer numérique à 50% 
int dimmer_power = 0;

/// constantes de debug 
// #define debug 1  // recherche la valeur milieu >> middle   
#define modeserial 0
// #define oscillo 1   /// affichage de l'oscillo sur le server web. 
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

//***********************************
//************* Gestion du serveur WEB
//***********************************
// Create AsyncWebServer object on port 80
WiFiClient domotic_client;

AsyncWebServer server(80);
DNSServer dns;
HTTPClient http;

//String state = "Stabilise";
//String sendmode = "off";


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
const char* PARAM_INPUT_5 = "save"; /// paramettre de retour cosphi

String getSendmode() {
  String sendmode;
  if ( config.sending == 0 ) {   sendmode = "Off"; }
  else {   sendmode = "On"; }
  return String(sendmode);
}

String getSigma() {
  
  return String(sigma);
}

String getconfig() {
  configweb = String(config.hostname) + ";" +  config.port + ";"  + config.IDX + ";"  +  VERSION +";" + middle +";"+ config.delta +";"+config.cycle ;
  return String(configweb);
}

String getchart() {
	  oscilloscope() ;
      return String(retour);
}

String getdebug() {
  configweb = String(ESP.getFreeHeap())+ ";" + String(laststartvalue)  + ";" +  String(middle) + ";" +  String(front_size) +";"+ phi +";"+ dimmer_power;
    return String(configweb);
}

String getmemory() {
  
    memory = String(ESP.getFreeHeap()) + ";" + String(ESP.getHeapFragmentation()) + ";" + String(ESP.getMaxFreeBlockSize());
      return String(memory);
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
 /*  else if (var == "Domoticz"){
    
    return String(config.hostname);
  }
  else if (var == "config.IDX"){
    
    return config.IDX;
  }
  else if (var == "config.port"){
    
    return config.port;
  } */
  
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
  
		//***********************************
		//************* Setup -  récupération du fichier de configuration
		//***********************************
  
  // Should load default config if run for the first time
  Serial.println(F("Loading configuration..."));
  loadConfiguration(filename_conf, config);

  // Create configuration file
  Serial.println(F("Saving configuration..."));
  saveConfiguration(filename_conf, config);
  
   
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
    request->send(SPIFFS, "/index.html", String(), false, processor);
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

  server.on("/config", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getconfig().c_str());
  });

  server.on("/memory", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getmemory().c_str());
  });

  server.on("/debug", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", getdebug().c_str());
  });

server.on("/config.json", HTTP_ANY, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/config.json", "application/json");
  });



////// mise à jour parametre d'envoie vers domoticz et récupération des modifications de configurations
server.on("/get", HTTP_ANY, [] (AsyncWebServerRequest *request) {
      ///   /get?send=on
    if (request->hasParam(PARAM_INPUT_1)) 		  { inputMessage = request->getParam(PARAM_INPUT_1)->value();
													config.sending = 0; 
													if ( inputMessage != "On" ) { config.sending = 1; }
													request->send(200, "text/html", getSendmode().c_str()); 	}
	   // /get?cycle=x
	else	if (request->hasParam(PARAM_INPUT_2)) { config.cycle = request->getParam(PARAM_INPUT_2)->value().toInt(); request->send(200, "text/html", getconfig().c_str()); }
		// /get?readtime=x
	else 	if (request->hasParam(PARAM_INPUT_3)) { config.readtime = request->getParam(PARAM_INPUT_3)->value().toInt(); request->send(200, "text/html", getconfig().c_str());}
		// /get?cosphi=x
	else	if (request->hasParam(PARAM_INPUT_4)) { config.cosphi = request->getParam(PARAM_INPUT_4)->value().toInt(); request->send(200, "text/html", getconfig().c_str()); }
		// /get?save
	else	if (request->hasParam(PARAM_INPUT_5)) { Serial.println(F("Saving configuration..."));
													saveConfiguration(filename_conf, config);   
													request->send(200, "text/html", getconfig().c_str()); }

	});

		//***********************************
		//************* Setup -  demarrage du webserver et affichage de l'oled
		//***********************************

  server.begin(); 
  affiche_info_main();
}






						//***********************************
						//************* loop
						//***********************************


void loop() {
const int deltaneg = 50 - config.delta ; // décalage de la sensibilité pour l'injection
/// preparation des mesures
  temp = 0; 
  timer = 0 ; 
  //int dimmer = 0;
  somme = 0; 
  int i = 0;
  int validation = 0;
// lecture de la valeur moyenne 
   middle = valeur_moyenne() ;

/// recherche d'un cycle stable
 
  int cycle_stable = 2 ;  
  int cycle_total = 0 ; 
  int cycle_last = 0; 
  int cycle_moyen = 0 ; 
  while ( i < cycle_stable )
  {
  cycle_last = mesure();
  cycle_total = cycle_total + cycle_last; 
  i++;
  }
  cycle_moyen = ( cycle_total / cycle_stable );
  cycle_total = cycle_moyen - cycle_last; 
   Serial.println("-----------" );
   Serial.print("cycle total et last : " );
   Serial.println(cycle_total);
   Serial.println(cycle_last);

    // vérification de la cohérence
  if ( cycle_total > 10 or cycle_total < -10 ) {somme = 0;}
  else {somme = cycle_moyen ;  validation = 1; }

 //// vérification du middle 
 //if ( middle != valeur_moyenne() ) {somme = 0; }
 
  if ( modeserial == 1 )  {
   Serial.println("-----------" );
   Serial.print("somme mesuré : " );
    Serial.println(somme);
  }
   
 phi = cycle_phi();  
  
 

  //// facteur de correction 
   // int correction  =    ; 
   // somme = somme - correction  ; 

   
   if ( modeserial == 1 )  {
  // Serial.print("facteur de correction : ");
  // Serial.println(correction);
   
   Serial.print("somme corrigé : ");
   Serial.println(somme); }

   //if ( inversion == 1 ) { somme = 0 - somme ; }
   /// si la valeur est validée, sigma est raffraichi 
   if (validation == 1 ) { sigma = somme * 1.5 ; }

//**************************
////  détection traitement des conso
//**************************



// production
  if ( somme > config.delta ) {
    //dimmer = 10 ;
    if ( modeserial == 1 )  {Serial.println("prod"); }
  
    message = "Mode Linky        "; 
    inj_mode=0;

    if ( config.sending == 1) {
       SendToDomotic(String(somme));  
      
      
      delay(5*attente*1000); 
    }
  }
  
//**************************
// injection 
//**************************
  else if ( somme < deltaneg ) {
    //dimmer = -10;
    if ( modeserial == 1 )  {Serial.println("inj"); }
    
    message = "Correc injection    ";
    inj_mode=1;
    
    if ( config.sending == 1) {
            SendToDomotic(String(somme));  
            
            delay(5*attente*1000); 
    
    } 
    
  }
  else {
		message = "Mode stabilise    " ;
		inj_mode=2;
		
  }
  affiche_info_injection(inj_mode);
  affiche_info_volume(somme);
  if (config.autonome == 1 ) {dimmer(inj_mode); }

  if (modeserial == 1 ){
    //////////// affichage des variables sur port série pour regler le delta.

    Serial.print("somme: ");
    Serial.println(somme);
  }






//pause de X s secondes avant prochaine mesure

affiche_info_transmission(config.sending);
 delay (500*attente);
 ArduinoOTA.handle();
 
}



// ***********************************
// ************* Fonction interne
// ***********************************

// ***********************************
// ** recherche de changement sur la diode.
// ***********************************
void front(int variation) {
	while ( digitalRead(sens) == variation ) {
	delayMicroseconds (25);
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

	//if ( debug == 1 ) {   display.drawString(0,27, "Debug");  }
	if ( modeserial == 1 ) {   display.drawString(0,38, "Serial"); }

  display.drawString(0,54, VERSION );
  affiche_info_transmission(config.sending);
  
  display.display();
}

// ***********************************
// ************* affichage transmission
// ***********************************

void affiche_info_transmission (int transmission ){
  display.setColor(BLACK);
  display.fillRect(0, 16, 50, 26);
  
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
	if ( transmission == 0 ) {   display.drawString(0,16, "Trans Off"); }
	else {   display.drawString(0,16, "Trans On "); }
  
  display.display();
}

// ***********************************
// ************* affichage du sigma
// ***********************************

void affiche_info_volume(int volume ) {
  display.setColor(BLACK);
  display.fillRect(55, 30, 128, 64);
  
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(80,30, String(volume) + " W"); 
  display.drawString(80,47, String(dimmer_power) + " %"); 
  
  
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
  while ( timer < config.cycle )
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

int mesure () {
  temp = 0,
  timer = 0; 
  int startvalue = 0; 
  somme = 0; 
  front(0);
  front(1);
  
  delayMicroseconds (config.cosphi*config.readtime); // correction décalage
  temp =  analogRead(linky);
  somme = temp - middle ; 
  somme = abs(somme ); 
  if ( somme < 5 ) { somme = temp - middle ; timer ++; startvalue = temp; }
  else {somme = 0 ; }
  
// analyse par volumes
 // delayMicroseconds (config.readtime);

//// lecture dans la période de la demi alternance fait par la diode  pour calcul de l'air
  while ( timer < front_size )
  {
    temp =  analogRead(linky); signe = digitalRead(sens);
    //if ( signe == 0 ) {
      somme = somme + temp - middle ;
      if (timer ==0) {startvalue = temp; }
    // }
  timer ++ ;
  delayMicroseconds (config.readtime); // frequence de 100Hz environ
  } 
   Serial.print("valeur depart : ");
   Serial.println(startvalue);
   laststartvalue = startvalue;
  return (somme);
}


int valeur_moyenne() {

int temp, moyenne, count = 0; 
  front(1);
  front(0);
    
  // lecture d'une ondulation pour valeur moyenne
while ( digitalRead(sens) == 1 )
  {
    temp =  analogRead(linky); 
    moyenne = moyenne + temp ;   
    delayMicroseconds (config.readtime);
    count ++; 
  }
while ( digitalRead(sens) == 0 )
  {
    temp =  analogRead(linky); 
    moyenne = moyenne + temp ;   
    delayMicroseconds (config.readtime);
    count ++ ;
  }
  
  moyenne = moyenne / count ;
   
   if ( modeserial == 1 )  {
   Serial.print("valeur_moyenne : ");
   Serial.println(moyenne);
   }
 return (moyenne);
}

int cycle_phi() {

int count = 0; 

  front(1);
  front(0);

while ( digitalRead(sens) == 1 )
  {
    temp =  analogRead(linky); 
    if ( temp < moyenne ) {
    delayMicroseconds (config.readtime);
    count ++;  }
  } 

return (count);
}


//***********************************
//************* Fonction domotique 
//***********************************

boolean SendToDomotic(String Svalue){
  String baseurl; 
  Serial.print("connecting to ");
  Serial.println(config.hostname);
  Serial.print("Requesting URL: ");

  if ( config.UseDomoticz == 1 ) { baseurl = "/json.htm?type=command&param=udevice&idx="+config.IDX+"&nvalue=0&svalue="+Svalue;  }
  if ( config.UseJeedom == 1 ) {  baseurl = "/core/api/jeeApi.php?apikey=" + String(config.apiKey) + "&type=virtual&id="+ config.IDX + "&value=" + Svalue;  }
  if ( config.autonome == 1 ) {  baseurl = "/?POWER=" + String(dimmer_power) ; }
  Serial.println(baseurl);

  http.begin(config.hostname,config.port,baseurl);
  int httpCode = http.GET();
  
  Serial.println("closing connection");
  http.end();
  
}


//***********************************
//************* Fonction aservicement autonome
//***********************************

void dimmer(int commande){
	if ( sigma >= 350 ) { dimmer_power = 0 ; } // si grosse puissance instantanée sur le réseau, coupure du dimmer. ( ici 350w environ ) 
	else if (commande == 0  ) { dimmer_power += -2 ;}  /// si mode linky  on reduit la puissance 
	else if (commande == 1  ) { dimmer_power += 5 ;} /// si injection on augmente la puissance
		
if ( dimmer_power >= num_fuse ) {dimmer_power = num_fuse; }
if ( dimmer_power <= 0 ) {dimmer_power = 0; }
	
}


