/*
 RemoteSwitch transmitter based on ESP8266/NodeMCU and MQTT
    Copyright (C) 2016  Oliver Faßbender

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
//------------------------------------------------------------------------------------------------
const String NAME = "ESP433MHzTransmitter";
const String VERSION = "V 1.2.0"; 
//------------------------------------------------------------------------------------------------
/* 
 *  http://arduino.esp8266.com/stable/package_esp8266com_index.json
 *  
 *  Force WLAN Config     D4 / GPIO2  // Press during normal work, causes acess point to open for WLAN config
 *                                    // declared in local_conf.h
 *  Transmitter           D6 / GPIO12 // declared in local_conf.h
 *  USB/Flashing          D9 / GPIO3 
 *                        D10/ GPIO1 
 *  Free (check reset)    D8 / GPIO15 // must be low for reset (bad with I2C pullups)
 *                        D4 / GPIO2  // sometimes used for buildin LED (must be high for reset)
 *                        D3 / GPIO0  // must be low for reset
 */
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include "Adafruit_MQTT.h"        //Required Adafruit MQTT Lib Version >=0.16.1
#include "Adafruit_MQTT_Client.h"
#include <RCSwitch.h>             //https://github.com/sui77/rc-switch
#include "local_conf.h"
//------------------------------------------------------------------------------------------------
#ifndef USE_SSL
 WiFiClient client;
#else
 WiFiClientSecure client;
#endif
//------------------------------------------------------------------------------------------------
// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_SERVERPORT, MQTT_USERNAME, MQTT_PASSWORD);
//------------------------------------------------------------------------------------------------
// multicast DNS responder / Zeroconf
#ifdef WITH_MDNS
#include <ESP8266mDNS.h>
MDNSResponder mdns;
#endif //WITH_MDNS
//------------------------------------------------------------------------------------------------
#ifdef USE_SSL
 //flag for saving data
 bool shouldSaveConfig = false;
 // SSL fingerprint (SHA1?)
 #define SSL_HASH_MAX 100
 char ssl_hash[SSL_HASH_MAX] = "";
#endif //USE_SSL

/****************************** Feeds ***************************************/

#ifdef TRISTATE
 #define TRISTATE_FEED CHANNEL_BASENAME "/tristate"
 Adafruit_MQTT_Subscribe transmitter_tristate = Adafruit_MQTT_Subscribe(&mqtt, TRISTATE_FEED);
#endif
#define BINARY_FEED CHANNEL_BASENAME "/binary"
Adafruit_MQTT_Subscribe transmitter_binary = Adafruit_MQTT_Subscribe(&mqtt, BINARY_FEED);
#define DECIMAL_FEED CHANNEL_BASENAME "/decimal"
Adafruit_MQTT_Subscribe transmitter_decimal = Adafruit_MQTT_Subscribe(&mqtt, DECIMAL_FEED);
#define CONFIG_FEED CHANNEL_BASENAME "/config"
Adafruit_MQTT_Subscribe transmitter_config = Adafruit_MQTT_Subscribe(&mqtt, CONFIG_FEED);

/*************************** Sketch Code ************************************/




RCSwitch RCtransmitter = RCSwitch();
unsigned int CurrentProtocol = RCProtocol;
unsigned int CurrentPulseLength = RCPulseLength;
unsigned int CurrentRepeatTransmit = RCRepeatTransmit;

//------------------------------------------------------------------------------------------------
#ifdef USE_SSL
 //callback notifying us of the need to save config
 void saveConfigCallback () {
  #ifdef DEBUG
    Serial.println("Should save config");
  #endif //DEBUG
  shouldSaveConfig = true;
 }

void verifyFingerprint() {
  const char* host = MQTT_SERVER;
  #ifdef DEBUG
    Serial.print("Connecting to ");
    Serial.println(host);
  #endif //DEBUG
  if (! client.connect(host, MQTT_SERVERPORT)) {
    #ifdef DEBUG
      Serial.println("Connection failed. Halting execution.");
    #endif //DEBUG
    // basically die and wait for WDT to reset me
    while (1);
  }
  if (client.verify((char*)ssl_hash, host)) {
    #ifdef DEBUG
      Serial.println("Connection secure.");
    #endif //DEBUG
  } else {
    #ifdef DEBUG
      Serial.println("Connection insecure! Halting execution.");
    #endif //DEBUG
    // basically die and wait for WDT to reset me
    while(1);
  }
}
#endif //USE_SSL
//------------------------------------------------------------------------------------------------
// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;
  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }
  #ifdef DEBUG
    Serial.print(F("Connecting to MQTT... "));
  #endif //DEBUG
  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { 
   // connect will return 0 for connected
   #ifdef DEBUG
     Serial.println(mqtt.connectErrorString(ret));
     Serial.println(F("Retrying MQTT connection in 500 milliseconds..."));
   #endif //DEBUG
   mqtt.disconnect();
   delay(500);  // wait 
   retries--;
   if (retries == 0) {
    // basically die and wait for WDT to reset me
    while (1);
   }
  }
  // check the fingerprint of io.adafruit.com's SSL cert
  #ifdef VERIFY_SSL
    verifyFingerprint();
  #endif //VERIFY_SSL
  #ifdef DEBUG
    Serial.println("MQTT Connected!");
  #endif //DEBUG
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  wifiManager.setMinimumSignalQuality();
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(300);
  //set hostname
  #ifdef HOSTNAME
    WiFi.hostname(HOSTNAME);
  #else
    WiFi.hostname(NAME);
  #endif
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("ESPConnectAP", "ESP8266")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");


  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  // Setup MQTT subscription
  #ifdef TRISTATE
   mqtt.subscribe(&transmitter_tristate);
   Serial.println( F("Registered " TRISTATE_FEED) );
  #endif
  mqtt.subscribe(&transmitter_binary);
  Serial.println( F("Registered " BINARY_FEED) );
  mqtt.subscribe(&transmitter_decimal);
  Serial.println( F("Registered " DECIMAL_FEED) );
  mqtt.subscribe(&transmitter_config);
  Serial.println( F("Registered " CONFIG_FEED) );

  RCtransmitter.enableTransmit(RCSWITCH_PIN);
  RCtransmitter.setProtocol(RCProtocol);
  RCtransmitter.setPulseLength(RCPulseLength);
  RCtransmitter.setRepeatTransmit(RCRepeatTransmit);  
  
}

void loop() {
  #ifdef WLANCONFIGTRIGGER_PIN
   // is configuration portal requested?
   if ( digitalRead(WLANCONFIGTRIGGER_PIN) == LOW ) {
     WiFiManager wifiManager;
     wifiManager.setTimeout(300);
     if (!wifiManager.startConfigPortal("ESPDemandAP")) {
       Serial.println("failed to connect and hit timeout");
       delay(3000);
       //reset and try again, or maybe put it to deep sleep
       ESP.reset();
       delay(5000);
     }
     //if you get here you have connected to the WiFi
     Serial.println("connected...yeey :)");
   }
  #endif
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();
  // this is our 'wait for incoming subscription packets' busy subloop
  // try to spend your time here
  
  //------------------------------------------------------
  Adafruit_MQTT_Subscribe *subscription;
  while (subscription = mqtt.readSubscription(3000))  {
    #ifdef TRISTATE
    //-------------------------------
    // Check if its the tristate feed
    if (subscription == &transmitter_tristate) {
      Serial.print( F("Tristatetransmitter: ") );
      boolean CodeOk = true;
      String tristatecode = (char *)transmitter_tristate.lastread;
      Serial.println( tristatecode.length(),DEC );
      tristatecode.toUpperCase();
      if ( strlen((char *)transmitter_tristate.lastread) >= SUBSCRIPTIONDATALEN ) {
        tristatecode.remove(SUBSCRIPTIONDATALEN);
      }
      char buffer[maxlengthoftristatecode];
      tristatecode.toCharArray(buffer,tristatecode.length()+1); // Returns the length of the String, in characters. (Note that this doesn't include a trailing null character.)  // buffer must be one more in size
      for (unsigned int loop = 0; loop < strlen(buffer); loop++){
        if ( buffer[loop] != '0' ) {
          if ( buffer[loop] != '1' ) {
            if ( buffer[loop] != 'F' ) {
              CodeOk = false;
            }
          }
        }
      }
      Serial.print( buffer );
      if ( CodeOk == true ) {
        Serial.println( F(" transmitting") );
        RCtransmitter.send( buffer );
      }
      else {
        Serial.println( F("Code not binary, not transmitting") );
      }
    }      
    #endif
    //-------------------------------
    // Check if its the binary feed
    if (subscription == &transmitter_binary) {
      Serial.print( F("Binaertransmitter: ") );
      boolean CodeOk = true;
      String binarycode = (char *)transmitter_binary.lastread;
      Serial.println( binarycode.length(),DEC );
      if ( strlen((char *)transmitter_binary.lastread) >= SUBSCRIPTIONDATALEN ) {
        binarycode.remove(SUBSCRIPTIONDATALEN);
      }
      char buffer[maxlengthofbinarycode];
      binarycode.toCharArray(buffer,binarycode.length()+1); // Returns the length of the String, in characters. (Note that this doesn't include a trailing null character.)  // buffer must be one more in size
      for (unsigned int loop = 0; loop < strlen(buffer); loop++){
        if ( buffer[loop] != '0' ) {
          if ( buffer[loop] != '1' ) {
            CodeOk = false;
          }
        }
      }
      Serial.print( buffer );
      if ( CodeOk == true ) {
        Serial.println( F(" transmitting") );
        RCtransmitter.send( buffer );
      }
      else {
        Serial.println( F("Code not binary, not transmitting") );
      }
    }      
    //-------------------------------
    // Check if its the decimal feed
    if (subscription == &transmitter_decimal) {
      Serial.print( F("Decimaltransmitter: ") );
      boolean CodeOk = false;
      unsigned long buffer = atoi((char *)transmitter_binary.lastread); // Returns the length of the String, in characters. (Note that this doesn't include a trailing null character.)  // buffer must be one more in size
      if ( buffer != 0 ) {
        CodeOk = true;
      }
      Serial.print( buffer );
      if ( CodeOk == true ) {
        Serial.println( F(" transmitting") );
        RCtransmitter.send( buffer, 24 ); // send decimal value as 24 bit message
      }
      else {
        Serial.println( F("Code not binary, not transmitting") );
      }
    }      
    //-------------------------------
    // Check if its the config feed
    if (subscription == &transmitter_config) {
      Serial.print( F("Transmitter configuration: ") );
      //char configstring[maxlengthofconfig];
      String configstring = "";
      configstring.concat ( (char *)transmitter_config.lastread );  
      Serial.println( configstring );
      //reset transmitter to default values (values from local_conf.h)
      if ( configstring.startsWith("RCRESET") ) {
        Serial.println( F("transmitter reset received") );
        RCtransmitter.setProtocol(RCProtocol);
        RCtransmitter.setPulseLength(RCPulseLength);
        RCtransmitter.setRepeatTransmit(RCRepeatTransmit);  
        CurrentProtocol = RCProtocol;
        CurrentPulseLength = RCPulseLength;
        CurrentRepeatTransmit = RCRepeatTransmit;
      }  
      // reboot ESP8266
      if ( configstring.startsWith("REBOOT") ) {
        Serial.println( F("transmitter REBOOT received") );
        delay(3000);
        ESP.reset();
        //ESP.restart();//Same results as ESP.reset()
        delay(5000);
      }  
      /// PulseLength
      if ( configstring.startsWith("RCPULSELENGTH=") ) {
        Serial.println( F("RCPULSELENGTH received") );        
        configstring.remove( 0, configstring.indexOf('=')+1 ) ;
        if ( ( configstring.toInt() > RCMinPulseLength ) && ( configstring.toInt() <= RCMaxPulseLength ) ) {
          RCtransmitter.setPulseLength(configstring.toInt());
          CurrentPulseLength = configstring.toInt();
        }
        else
        {
          Serial.println ( F("Not a value or value out of range") );
        }
      }
      /// Protocol
      if ( configstring.startsWith("RCPROTOCOL=") ) {
        Serial.println( F("RCPROTOCOL received") );        
        configstring.remove( 0, configstring.indexOf('=')+1 ) ;
        if ( ( configstring.toInt() > 0 ) && ( configstring.toInt() <= RCMaxProtocol ) ) {
          RCtransmitter.setProtocol(configstring.toInt());
          CurrentProtocol = configstring.toInt();
        }
        else
        {
          Serial.println ( F("Not a value or value out of range") );
        }
      }
      /// RepeatTransmit
      if ( configstring.startsWith("RCREPEATTRANSMIT=") ) {
        Serial.println( F("RCREPEATTRANSMIT received") );        
        configstring.remove( 0, configstring.indexOf('=')+1 ) ;
        if ( ( configstring.toInt() > RCMinRepeatTransmit ) && ( configstring.toInt() <= RCMaxRepeatTransmit ) ) {
          RCtransmitter.setRepeatTransmit(configstring.toInt());  
          CurrentRepeatTransmit = configstring.toInt();
        }
        else
        {
          Serial.println ( F("Not a value or value out of range") );
        }
      }
    }
  }
//------------------------------------------------------
  // ping the server to keep the mqtt connection alive
  if(! mqtt.ping()) {
    mqtt.disconnect();
  }
  delay(100);
}
