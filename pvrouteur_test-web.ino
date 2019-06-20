/**************
 *  Pv router 
 *  
 *  Cyril Poisssonnier 2019 
 * **************
 * This cods is for a Wifi pv router and working with domoticz
 * this part detect injection on the grid and send information to domoticz
 * it can be associate to a numeric dimmer with a load. 
 * 
 *  #version 1.0
 * 
 ***************/

#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>

// WIFI
const char* WIFI_SSID = "My_ssID";
const char* WIFI_PWD = "Mypasswd";

// domoticz 
const char* domoticz_server = "ip_of_Domoticz" ;
const String IDX  = "36" ;
WiFiClient domoticz_client;

//constantes 
#define USE_SERIAL  Serial
#define linky  A0
#define lcd_scd D1
#define lcd_scl D2
#define sens D5

#define wattfactor 4.5 // to be adjust in case of

// variables 
int  middle = 544; // zero by default 

int consommation = 0;
int analogmin , analogmax, analogminup , analogmaxup, mini, maxi ;

int signe = 0;
int injection = 0;
int temp = 0; 
int timer = 0 ; 
int nbtent=0;
int refreshdom = 0; 

// LCD init
LiquidCrystal_I2C lcd(0x3F, 20, 4);
///////////////////////

///////////////////


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
      ESP.deepSleep(2 * 1000000, WAKE_RF_DEFAULT);  ///  ==>> connect D0 to reset or comment the line
        }    
  }
  
 // Connexion WiFi établie / WiFi connexion is OK
   Serial.println ( "" );
   Serial.print ( "Connected to " ); Serial.println ( WIFI_SSID );
   Serial.print ( "IP address: " ); Serial.println ( WiFi.localIP() );

   // ip address on LCD display
   lcd.setCursor(2, 0);
   lcd.print("IP address:        "); 
   lcd.setCursor(2, 1);      
   lcd.print(WiFi.localIP());

  Serial.println ( "setup ok" );

    delay (2000);

      }


///////////////////


void loop() {

consommation = 0 ;

temp = 0; 
timer = 0 ; 

maxi = 0; mini = 1023; 
analogmax = 1023;
analogmin = 1023; 
analogminup = 0 ;
analogmaxup = 0 ;

// lecture de la valeur min 

///  loop de for searching max and min value on some cycles
while ( timer < 500 ) { 

// read values
temp = analogRead(linky);
signe = digitalRead(sens); 

// searching the direction of current
if ( signe == HIGH  &&  temp < analogmax ) { analogmax = temp ; }
if ( signe == LOW  &&  temp < analogmin ) { analogmin  = temp; }
if ( signe == HIGH  &&  temp > analogmaxup ) { analogmaxup = temp ; }
if ( signe == LOW  &&  temp > analogminup ) { analogminup  = temp; }

/// find min and max
if ( temp > maxi ) { maxi  = temp; }
if ( temp < mini ) { mini  = temp; }


timer ++; 
delay (1);
}

/// print informations on serial and lcd
Serial.print("sensor= ");
Serial.print(consommation);

Serial.print(" min= ");
Serial.print(analogmin);

Serial.print(" max= ");
Serial.println(analogmax);

lcd.setCursor(2,3);      
lcd.print("       w");
lcd.setCursor(2,3);      
lcd.print(consommation);

lcd.setCursor(0,0);      
lcd.print("          ");
lcd.setCursor(0,0);      
lcd.print(analogmin);

lcd.setCursor(4,0);      
lcd.print(analogminup);

lcd.setCursor(8,0);      
lcd.print("          ");
lcd.setCursor(8,0);      
lcd.print(analogmax);

lcd.setCursor(12,0);      
lcd.print(analogmaxup);



/// erase indication of injection on LCD
lcd.setCursor(10,3);    lcd.print("         ");

/// calculate consumtion 
consommation = abs(maxi - mini) * wattfactor ; 


lcd.setCursor(2, 2);      
lcd.print("consommation :");
lcd.setCursor(2,3);      
lcd.print(consommation);

Serial.print("power = ");
Serial.println(consommation);

/// if injection sent to domoticz after 10 cycles ( stabilize value + antispam )
//  delais 10*250ms 
if ( analogmin > analogmax ) { 
  injection ++; 
lcd.setCursor(10,3);    lcd.print(" --> ");

  if ( injection == 5 ) {
Serial.print("envoie vers domoticz suite injection ");
consommation = 0 - consommation;
EnvoieDomoticz(IDX,String(consommation));
/// wait some time for domoticz launch scripts
delay (5000);
refreshdom = 0;
  }
  
} 

// else wait 60 cycles ( 15s ) 
if ( refreshdom > 30 ) {
refreshdom = 0; injection = 0 ;  
Serial.print("envoie vers domoticz ");
/// delay of 2s 
EnvoieDomoticz(IDX,String(consommation)); // tension
delay (2000);
  }

  refreshdom ++;
 


//pause de 250ms secondes avant prochaine mesure
delay (50);



}


/// function for domoticz

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
     // ESP.deepSleep(2 * 1000000, WAKE_RF_DEFAULT);
    }  
    
  }  
  // deconexion
  domoticz_client.println("Connection: close");
  domoticz_client.println();  
  return(Resultat);
}

   
