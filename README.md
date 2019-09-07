# Pv router

Hi All, 
I create this Pv router for separate power part to I created this pv router to separate the power part of the analysis part. 
I wanted to be able to trace the information to domoticz and leave him the processing of information 
to redispatch the surplus power to different digital dimmer.


Prerequiement : 
A circuit board has been created and is available (tips ) 
<img>https://nsa40.casimages.com/img/2019/09/05/190905103700235594.png</img>

the necessary components are: 
1 SCT013
[img]https://ae01.alicdn.com/kf/HTB1FVJSXEjrK1RkHFNRq6ySvpXaR/YHDC-30A-50A-100A-SCT013-Non-invasive-AC-Current-Sensor-Split-Core-Current-Transformer-New-sct013000.jpg_220x220xz.jpg[/img]
1k resistors, 
a LED, 
a capacitor 150nf polypropylene , 
a 5v regulator, 
connectors, 
a femelle Jack  [img]https://ae01.alicdn.com/kf/HTB1f4P3aovrK1RjSszfq6xJNVXaj/2Pcs-Set-3-5MM-Audio-Jack-Socket-3-Pole-Black-Stereo-Solder-Panel-Mount-Gold-with.jpg_220x220xz.jpg[/img]
a 12V connector [img]https://ae01.alicdn.com/kf/HTB1tgeJXsnrK1RkHFrdq6xCoFXa1/10Pcs-3A-12v-For-DC-Power-Supply-Jack-Socket-Female-Panel-Mount-Connector-5-5mm-2.jpg_220x220xz.jpg[/img]
an oled Display [img]https://ae01.alicdn.com/kf/HTB1uK6AX._rK1Rjy0Fcq6zEvVXac/0-96-inch-IIC-Serial-White-OLED-Display-Module-128X64-I2C-SSD1306-12864-LCD-Screen-Board.jpg_220x220xz.jpg[/img]
and a recovery transformer.


Use : 
change your Domoticz IP and IDX on the program
upload the program on the esp8266 and connect to the new wifi network ( pwd "pvrouteur" ) 
put information on your personal Wifi and reconnect. 

the pv routeur will send information on your Domoticz server. 


Dimmer_heater.lua is configuration file for send to the dimmer. 
Domoticz_pvrouter_script.lua is the main file for sending to the digital dimmer. 

