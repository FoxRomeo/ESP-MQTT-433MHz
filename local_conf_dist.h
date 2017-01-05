/*
 * Rename this file to local_conf.ino and add your local settings.
 */

#define WLANCONFIGTRIGGER_PIN  D4
#define RCSWITCH_PIN  D6

#define MQTT_SERVER       "<mqtt-hostname>"
#define MQTT_USERNAME     "<mqtt-user>"
#define MQTT_PASSWORD          "<mqtt-password>"
#define CHANNEL_BASENAME "<mqtt-channel-basename>"

#define USE_SSL
#ifndef USE_SSL
 #define MQTT_SERVERPORT  1883                   // default non-SSL port
#else
 #define MQTT_SERVERPORT  8883                   // default SSL port
 //#define VERIFY_SSL
#endif //USE_SSL

//#define HOSTNAME "ESP433MHzTransmitter" // if not own defined NAME will be used, use HOSTNAME if more than one is used to avoid conflicts
//------------------------------------------------------------------------------------------------
#define DEBUG             // enables serial debug information
//------------------------------------------------------------------------------------------------
#define WITH_MDNS
//------------------------------------------------------------------------------------------------
// Set default pulse length.
#define RCPulseLength 320  // Lib default 350
#define RCMinPulseLength 50
#define RCMaxPulseLength 1000
// Set default protocol (Lib default is 1, will work for most outlets)
#define RCProtocol 1
#define RCMaxProtocol 5
  
// Set default number of transmission repetitions. (Lib default is 10)
#define RCRepeatTransmit 20 
#define RCMinRepeatTransmit 10
#define RCMaxRepeatTransmit 100

//must be SUBSCRIPTIONDATALEN+2
#define maxlengthofconfig SUBSCRIPTIONDATALEN+2
#define maxlengthofbinarycode SUBSCRIPTIONDATALEN+2 // Library default is SUBSCRIPTIONDATALEN=20
/*  must be changes to at least 25 if binary is used (normal binary paket length is 24)
 *  I increased SUBSCRIPTIONDATALEN from 20 to 25 und MAXBUFFERSIZE from 125 to 150 on ESP8266-12E in Adafruit_MQTT.h
 */


//#define TRISTATE 
#ifdef TRISTATE
 #define maxlengthoftristatecode SUBSCRIPTIONDATALEN+2  // Tristate currently not working with RC Switch Lib on ESP8266
#endif
