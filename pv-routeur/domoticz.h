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
      delay(500);
      // Si le serveur repond lire toute les lignes
      while(domoticz_client.available()){
        String line = domoticz_client.readStringUntil('\r');
        if (line.lastIndexOf("status")>0 and line.lastIndexOf("OK")>0){
          Serial.println("Status OK");
          Resultat = 1 ;
        }
      }
    }
  else{
      Serial.println("Reset");
      ESP.deepSleep(2 * 1000000, WAKE_RF_DEFAULT);
    }  
    
  }  
  // deconexion
  domoticz_client.println("Connection: close");
  domoticz_client.println();  
  return(Resultat);
}
