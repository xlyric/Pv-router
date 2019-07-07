#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>

// test avec recherche de valeur min

// WIFI
const char* WIFI_SSID = "My_ssID";
const char* WIFI_PWD = "Mypasswd";

// domoticz 
const char* domoticz_server = "ip_of_Domoticz" ;
const String IDX  = "my_idx_number" ;

WiFiClient domoticz_client;

//constantes 
#define USE_SERIAL  Serial
#define linky  A0
#define lcd_scd D1
#define lcd_scl D2
#define sens D5
#define delta 100 
int deltaneg = 0 - delta ;

/// constantes de debug 
#define debug 0 
#define modeserial 0
#define pinreset 1 /// si D0 et reset connecté >> 1

#define attente 5 /// nombre de seconde entre chaque cycle de mesure.
#define middle 538 // zero by default 

int signe = 0;
int temp = 0; 
int timer = 0 ; 

int nbtent=0;
int refreshdom = 0;
//// calcul de dephasage et correction  
int frontmontant = 0 ; 


// LCD init
  LiquidCrystal_I2C lcd(0x3F, 20, 4);
///////////////////////

////////////////////

void setup() {
// init port
  pinMode(linky, INPUT);

// init serial 
  USE_SERIAL.begin(9600); 

//init LCD
  lcd.begin(20,4);
  lcd.init();
  lcd.noBacklight();

// init wifi 
  lcd.setCursor(2, 0);
  lcd.print("PVrouter starting "); 
  
  if ( debug == 0) { 
///// start Wifi 
    WiFi.begin(WIFI_SSID, WIFI_PWD);
    while (WiFi.status() != WL_CONNECTED) {
      if (nbtent<10){
        nbtent++ ;   
        delay(1000);
        Serial.print(".");
      }
    
    else{
// reboot if no network 
      Serial.println("Reset");
      if ( pinreset == 1)   { ESP.deepSleep(2 * 1000000, WAKE_RF_DEFAULT); }
      break;  /// si reset connecté, reboot, sinon on continu sans wifi...
      }    
    } 
  
// Connexion WiFi établie / WiFi connexion is OK
  Serial.println ( "" );
  Serial.print ( "Connected to " ); Serial.println ( WIFI_SSID );
  Serial.print ( "IP address: " ); Serial.println ( WiFi.localIP() );
  }
  
// ip address on LCD display
  lcd.setCursor(2, 0);
  lcd.print("IP address:        "); 
  lcd.setCursor(2, 1);      
  lcd.print(WiFi.localIP());

  Serial.println ( "setup ok" );
 

}


///////////////////


void loop() {

  temp = 0; 
  timer = 0 ; 
   

// lecture de la valeur min 

/// initialisation d'un cycle pour analyse
  front(1);

/// préparation d'un front 0

// analyse par volumes
  int dimmer = 0;
  int somme = 0; 
  int i = 0;
 
  front(0);

//// lecture dans la période de la demi alternance fait par la diode  pour calcul de l'air
  while ( timer < 70 )
  {
    temp =  analogRead(linky); signe = digitalRead(sens);
    if ( signe == 0 ) {
      somme = somme + temp - middle ;
    }
  timer ++ ;
  delayMicroseconds (480);
  } 

////  détection d'une conso supérieur à la consigne
  lcd.setCursor(4, 3); 
 String message = "Mode stabilisé" ;

  if ( somme > delta ) {
    dimmer = 10 ;
    Serial.println("prod");
  
    message = "Mode Linky"; 
    if ( debug == 0) {EnvoieDomoticz(IDX,String(somme)); delay(10000);  }
  }

// détection d'une injection inférieur à la consigne. 

  if ( somme < deltaneg ) {
    dimmer = -10;
    Serial.println("inj");
    message = "Mode injection";
    if ( debug == 0) {EnvoieDomoticz(IDX,String(somme)); } 
  }
  
  
  lcd.print(message);

  if (modeserial == 1 ){
    //////////// affichage des variables sur port série pour regler le delta.
    Serial.print("dimmer: ");
    Serial.println(dimmer);
    Serial.print("somme: ");
    Serial.println(somme);
  }

//pause de X s secondes avant prochaine mesure
  delay (1000*attente);

}


///////////fonction d'envoie vers domoticz

int EnvoieDomoticz(String IDX, String Svalue) {
  domoticz_client.connect(domoticz_server, 8080);
  delay(500);

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
  domoticz_client.println("Connection: close");
  domoticz_client.println();  
  return(Resultat);
}


///// recherche de changement sur la diode.
void front(int variation) {
  
while ( digitalRead(sens) == variation ) {
  delay (1);
  } 
 
}

