///////////////////
//  Pv router by Cyril Poissonnier 2019 
//  Pv routeur pour détecter l'injection par mesure de volume et envoie d'information sur domoticz 
//  pour réponse ( switch, dimmer ... ) 
//  fait pour fonctionner avec le circuit imprimé v1.1
/////////////////////////
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" /// Oled ( https://github.com/ThingPulse/esp8266-oled-ssd1306 ) 
#include <WiFiManager.h>  /// librairy : >> search  'wifimanager'  by tzapu

// Include custom images
#include "images.h"

String VERSION = "Version 1.7" ;

// WIFI Configuration 
// At first launch, create wifi network 'pvrouteur'  ( pwd : pvrouteur ) 

// configuration des infos domoticz 
const char* domoticz_server = "domoticz IP" ;
const String IDX  = "your IDX" ;

WiFiClient domoticz_client;

// Display Settings OLED
const int I2C_DISPLAY_ADDRESS = 0x3c;

//constantes de mesure
#define delta 100 /// Trigger Zone  
int deltaneg = 0 - delta ;
int LCD = 0 ;
#define OLED 1
#define attente 1 /// time between every test cycle ( in sec ) 
int middle = 535 ; 
int cycle = 52;

//constantes de fonctionnement
#define USE_SERIAL  Serial
#define linky  A0
#define lcd_scd D1  // LCD OR OLED
#define lcd_scl D2  // LCD OR OLED 
#define sens D5   /// niveau de la diode 
#define pinreset 1 /// if D0 and reset connected >> 1

/// constantes de debug 
#define debug 0  // recherche la valeur milieu >> middle   
int sent = 1 ;//--- 1 if Send information to domoticz
#define modeserial 0
#define oscillo 1   /// Occillo mode on web page 

int somme ;
int signe = 0;
int temp = 0; 
int timer = 0 ; 
String retour, retourbrut; 
int nbtent=0;
int moyenne; 
String message; 

// LCD init
LiquidCrystal_I2C lcd(0x3F, 20, 4);

// Initialize the OLED display using Wire library
//SSD1306Wire  display(I2C_DISPLAY_ADDRESS, lcd_scd, lcd_scl);
SSD1306Wire  display(0x3c, D1, D2);

/// init Web Server 
ESP8266WebServer server ( 80 );
  
///////////////////////

////////////////////

void setup() {
// init port
  pinMode(linky, INPUT);

// init serial 
  USE_SERIAL.begin(9600); 

//init LCD
if ( LCD ==1 ) { 
  
  lcd.begin(20,4);
  lcd.init();
  lcd.noBacklight();

// init wifi 
  lcd.setCursor(2, 0);
  lcd.print("PVrouter starting "); 
}


  // Initialising OLED
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.clear();

 /// start Wifi
  WiFiManager wifiManager;
  wifiManager.autoConnect("Pvrouteur", "pvrouteur"); 
  
  
 
///// start Wifi 
if ( LCD ==1 ) {     lcd.setCursor(2, 1); }
  
// Connexion WiFi établie / WiFi connexion is OK
  Serial.println ( "" );
  Serial.print ( "IP address: " ); Serial.println ( WiFi.localIP() );
  lcd.setCursor(2, 1);
  lcd.print("                ");

// ip address on LCD display
  lcd.setCursor(2, 0);
  lcd.print("IP address:        "); 
  lcd.setCursor(2, 1);      
  lcd.print(WiFi.localIP());
 

    // On branche la fonction qui gère la premiere page / link to the function that manage launch page
  server.on ( "/", handleRoot );
  server.begin();
  Serial.println ( "starting Web server" );
  Serial.println ( "setup ok" );

  affiche_info_main();

}


/////////////////////////////////////////////////////////////////////////////////


void loop() {

/// preparation des mesures
  temp = 0; 
  timer = 0 ; 
  int dimmer = 0;
  somme = 0; 
  int i = 0;

// lecture de la valeur min 

/// initialisation d'un cycle pour analyse
  front(0);
  front(1);

// analyse par volumes

//// lecture dans la période de la demi alternance fait par la diode  pour calcul de l'air
  while ( timer < cycle )
  {
    temp =  analogRead(linky); signe = digitalRead(sens);
    if ( signe == 0 ) {
      somme = somme + temp - middle ;
    }
  timer ++ ;
  delayMicroseconds (480); // frequence de 100Hz environ
  } 

////  détection d'une conso supérieur à la consigne

 message = "Mode stabilise    " ;
 affiche_info_injection(2);

  if ( somme > delta ) {
    dimmer = 10 ;
    Serial.println("prod");
  
    message = "Mode Linky        "; 
    affiche_info_injection(0);
    if ( sent == 1) {EnvoieDomoticz(IDX,String(somme)); delay(3*attente*1000);  }
    
  }

// détection d'une injection inférieur à la consigne. 

  if ( somme < deltaneg ) {
    dimmer = -10;
    Serial.println("inj");
    
    message = "Correc injection    ";
    affiche_info_injection(1);
    if ( sent == 1) {EnvoieDomoticz(IDX,String(somme));  } 
    
  }
  
  lcd.setCursor(4, 3);
  lcd.print(message);

  lcd.setCursor(4, 2); 
  lcd.print("              ");
  lcd.setCursor(4, 2); 
  lcd.print(somme);
  affiche_info_volume(somme);

  if (modeserial == 1 ){
    //////////// affichage des variables sur port série pour regler le delta.
    Serial.print("dimmer: ");
    Serial.println(dimmer);
    Serial.print("somme: ");
    Serial.println(somme);
  }


//// Oscillo mode creation du tableau de mesure pour le graph

if ( oscillo == 1) {
  timer = 0; 
  retourbrut = "<br>Data : <br>";
  retour = "['";
  front (1);
  front(0);
  while ( timer < cycle )
  {
  temp =  analogRead(linky); signe = digitalRead(sens);
  //moyenne = moyenne + abs(temp - middle) ;
  /// mode oscillo graph 
  retour += timer ; 
  retour += "'," ; 
  retour += middle - 25 + signe*50 ; 
  retour += "," ; 
  retour += temp ; 
  retour += "],['" ; 
  
  retourbrut += signe ;
  retourbrut += "x" ;
  retourbrut += temp ; 
  retourbrut += "<br>" ;
  timer ++ ;
  //delay (1) ;
  delayMicroseconds (480);
  } 

  temp =  analogRead(linky); signe = digitalRead(sens);
  retour += timer ; 
  retour += "'," ; 
  retour += middle - 25 + signe*50 ; 
  retour += "," ; 
  retour += temp ; 
  retour += "]" ;
    
  Serial.println(retour);
  }

//// recherche valeur moyenne sur 1 cycle
if ( debug == 1 ) {
  timer = 0; 
  front (1);
  front(0);
  moyenne = 0; 
  int count = 0; 
while ( digitalRead(sens) == 1 )
  {
    temp =  analogRead(linky); 
    moyenne = moyenne + temp ;   
    delayMicroseconds (480);
    count ++; 
  }
while ( digitalRead(sens) == 0 )
  {
    temp =  analogRead(linky); 
    moyenne = moyenne + temp ;   
    delayMicroseconds (480);
    count ++ ;
  }
  
  moyenne = moyenne / count ;
  lcd.setCursor(10, 2); 
  lcd.print("mid:");
  lcd.setCursor(16, 2); 
  lcd.print(moyenne);
  
}

  server.handleClient();  /// affichage de la page web

//pause de X s secondes avant prochaine mesure
  delay (1000*attente);
}

//////////  Functions ////////////////////

///////////fonction d'envoie vers domoticz

int EnvoieDomoticz(String IDX, String Svalue) {
  domoticz_client.connect(domoticz_server, 8080);
  affiche_transmission(1);
  delay(250);
  lcd.setCursor(0, 0); 
  lcd.print(".");
  
  int Resultat=0 ;
  int NbTentEnvoie=0;
  
  String deel1 = "GET /json.htm?type=command&param=udevice&idx="+IDX+"&nvalue=0&svalue=";
  String deel3 = " HTTP/1.1\r\nHost: www.local.lan\r\nConnection: keep-alive\r\nAccept: */*\r\n\r\n";  
  String dataInput = String(deel1 + Svalue + deel3);
  while (Resultat!=1) {
    if (NbTentEnvoie<10){
      
      NbTentEnvoie++ ;
      Serial.print("_");
      domoticz_client.println(dataInput);
      delay(1000);
      // Si le serveur repond lire toute les lignes
      while(domoticz_client.available()){
        String line = domoticz_client.readStringUntil('\r');
        if (line.lastIndexOf("status")>0 and line.lastIndexOf("OK")>0){
          Serial.println("Status OK");
          Resultat = 1 ;
        }
        else Serial.println(line);
      }
     
    }
  else{
      Serial.println("Reset pb with comm");
     if ( pinreset == 1)   { ESP.deepSleep(2 * 1000000, WAKE_RF_DEFAULT); }
     break;
      }  
    
  }  
  // deconexion
  affiche_transmission(0);
  domoticz_client.println("Connection: close");
  domoticz_client.println();  
  
  lcd.setCursor(0, 0); 
  lcd.print(" ");
  
  return(Resultat);
  
}


///// recherche de changement sur la diode.
void front(int variation) {
  
while ( digitalRead(sens) == variation ) {
  delay (1);
  } 
 
}


String getPage(){
  String page = "<html lang=fr-FR><head><meta http-equiv='refresh' content='10'/>";
  page += "<title>Pv router Web Server</title>";
  page += "<style> body { background-color: #fffff; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>";
  page += "</head><body><h1>PV router Status</h1>";
  page += "<h3>Data</h3>";
  page += retour;
  page += "<br><br><p><a href='https://github.com/xlyric/'>xLyric On Github</p>";
  page += "</body></html>";
  return page;
}

String getgraph(){
  String page = "<html lang=fr-FR><head><meta http-equiv='refresh' content='10'/>";
  page += "<title>Pv router Web Server</title>";
  page += "<style> body { background-color: #fffff; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>";
  page += "<script type='text/javascript' src='https://www.gstatic.com/charts/loader.js'></script> ";
  page += "<script type='text/javascript'> ";
  page += "google.charts.load('current', {'packages':['corechart']}); ";
  page += "google.charts.setOnLoadCallback(drawChart);" ;
  page += "function drawChart() {     ";
  page += "var data = google.visualization.arrayToDataTable([   ";
  page += "['time', 'half wave', 'Analog out (Ampere)'],";
  page +=  retour;
  page += "]); ";
  page += "     var options = { ";
  page += "title: 'Oscilloscope Mode', ";
  page += "curveType: 'function', ";
  page += "legend: { position: 'bottom' } ";
  page += "};";
  page += "var chart = new google.visualization.LineChart(document.getElementById('curve_chart')); ";
  page += "chart.draw(data, options); ";
  page += "}</script> ";
  page += "</head><body><h1>PV router Oscillo</h1>";
  page += "<h3>Oscillo</h3>";
  page += "<div id='curve_chart' style='width: 900px; height: 500px'></div>" ;
  page += "<br> middle point" ;
  page += middle ;
  page += "<br> somme " ;
  page += somme ;
  page += "<br><form action='/' method='POST'>";
  page += "<ul><li>boucle : ";
  page += "<INPUT type='text' name='cycle' value='";
  page += cycle;
  page += "'></li></ul><ul><li>moyenne: <INPUT type='text' name='middle' value='";
  page += middle;
  page += "'></li></ul>";
  page += "<ul><li>Envoie Domoticz (etat: ";
  page += sent;
  page += ")";
  page += "<INPUT type='radio' name='sending' value='1' ";
  if ( sent == 1 ) { page +=" checked" ; }  
  page += ">ON <INPUT type='radio' name='sending' value='0'";
  if ( sent == 0 ) { page +=" checked" ; }
  page += ">OFF</li></ul><INPUT type='submit' value='Actualiser'>";
  page += "<br><br> stat : <br>";
  page += message ; 
  page += retourbrut ;
  page += "<br><p><a href='https://github.com/xlyric/'>xLyric On Github</p>";
  page += "</body></html>";
  
  return page;
}


///// analyse de la requete post de la page
void handleRoot(){
  if ( server.hasArg("cycle") || server.hasArg("middle") || server.hasArg("sending") ) {
    handleSubmit();
  } else {
     server.send ( 200, "text/html", getgraph() );
  }
}

//// change les valeurs du programme en fonction du post
void handleSubmit() {
  // Actualise les valeurs 
  String cycleValue, middleValue, sendingValue;
  
  cycleValue = server.arg("cycle");
  if ( cycleValue.toInt() >= -1 ); {
    cycle = cycleValue.toInt();
  }
  
  middleValue = server.arg("middle");
  if ( middleValue.toInt() >= -1 ); {
    middle = middleValue.toInt();
  }

  sendingValue = server.arg("sending");
  if ( sendingValue.toInt() >= -1 ); {
    sent = sendingValue.toInt();
    affiche_info_transmission(sent);
  }
  
  server.send ( 200, "text/html", getgraph() );
  
}

/// affichage du logo des transmissions
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


void affiche_info_main() {
  display.clear();
  
  display.drawXbm(0,0, 16, 16, antenne_logo);
  
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(63, 0, String(WiFi.localIP().toString()));
  
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  ////// affichage de l'état conf
  if ( oscillo == 1 ) {   display.drawString(0,16, "Oscillo");   lcd.setCursor(5, 0);  lcd.print("OSC"); }
  if ( debug == 1 ) {   display.drawString(0,27, "Debug"); lcd.setCursor(10, 0);  lcd.print("DEBUG"); }
  if ( modeserial == 1 ) {   display.drawString(0,38, "Serial"); }
  display.drawString(75,52, VERSION );
  affiche_info_transmission(sent);
  
  display.display();
}

void affiche_info_injection(int injection_mode ) {
  display.setColor(BLACK);
  display.fillRect(75, 20, 128, 30);
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(100,20, "              ");
  if ( injection_mode == 0 ) {   display.drawString(100,20, "Linky"); }
  if ( injection_mode == 1 ) {   display.drawString(100,20, "Injection"); }
  if ( injection_mode == 2 ) {   display.drawString(100,20, "Stabilise"); }
  display.display();
}

void affiche_info_volume(int volume ) {
  display.setColor(BLACK);
  display.fillRect(40, 100, 100, 110);
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(40,100, String(volume)); 
  display.display();
}

void affiche_info_transmission (int transmission ){
  display.setColor(BLACK);
  display.fillRect(0, 52, 50, 62);
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  if ( transmission == 0 ) {   display.drawString(0,52, "Trans Off"); }
  if ( transmission == 1 ) {   display.drawString(0,52, "Trans On"); }
  display.display();
}

