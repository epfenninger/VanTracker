/*
  FILE: tracker.ino
  AUTHOR: Ethan P
  PURPOSE: Provide reliable GPS Tracking
*/

#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024 // Set RX buffer to 1Kb
#define SerialAT Serial1

//Uncomment to see all AT commands, if wanted
//#define DUMP_AT_COMMANDS


// set GSM PIN, if any
#define GSM_PIN ""

// Your GPRS credentials, if any. Hologram doesn't have GPS users/pass so probably leave it blank.
const char apn[]  = "hologram";     //SET TO YOUR APN
const char gprsUser[] = "";
const char gprsPass[] = "";
//Local (VAN Wifi) credentials if applicable. 
const char wifiSSID[] = "";
const char wifiPass[] = "";

//Make sure you have these libraries installed in your Arduino applications
#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>
#include <WiFi.h>

//This allows you to see the AT commands if you need it. 
#ifdef DUMP_AT_COMMANDS  // if enabled it requires the streamDebugger lib
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

//Pretty much leave everything below. Some of it isn't used.
#define UART_BAUD   115200
#define PIN_DTR     25
#define PIN_TX      27
#define PIN_RX      26
#define PWR_PIN     4

#define SD_MISO     2
#define SD_MOSI     15
#define SD_SCLK     14
#define SD_CS       13
#define LED_PIN     12

//Set the channel used to 4, but anything besides 0 works. Leave the port and server alone, that is for Hologram
TinyGsmClient  client(modem, 4);
const int      port = 9999;
const char server[]   = "cloudsocket.hologram.io";
//This is your home assistant server that is local in the van. It is probably homeassistant.local, or homeassistant.van, or just use the IP address
const char localserver[] = "";
const int localport = 8123;
//This is your Hologram API and device key. Device key comes from the webhooks section, API comes from org settings.
String api_key = "";
String deviceid = "";
//Default message time - I have it set at an hour.
int messageTime = 3600;
//Device resets after a number of connection fails
int fails = 0;
//Increment variable - used later
int x;
//Time last message was sent in milliseconds from boot
int lastMessage = 0;
//Port that the device recieves messages on locally (local WIFI!) UNCOMMMENT IF USING WIFI
//const int ServerPort = 8010;
//WiFiServer Server(ServerPort);

//Sets up WiFi send/recieve. UNCOMMENT IF YOU WANT TO USE THIS FEATURE
//WiFiClient RemoteClient;
//HttpClient localClient = HttpClient(RemoteClient, localserver, localport);


//Send message data with tags to Hologram
void send_message(String data, String tags) {
  Serial.println("Sending Message to Hologram");
  String hologramMessage;


  hologramMessage = "{\"k\":\"" + deviceid + "\",\"d\":\"" +
                    data + "\",\"t\":\"" + tags + "\"}";

  client.connect(server, port);

  client.println(hologramMessage);


  client.stop();
}

//Turns on the GPS module
void enableGPS(void)
{
  // Set SIM7000G GPIO4 LOW ,turn on GPS power
  // CMD:AT+SGPIO=0,4,1,1
  // Only in version 20200415 is there a function to control GPS power
  modem.sendAT("+SGPIO=0,4,1,1");
  if (modem.waitResponse(10000L) != 1) {
    DBG(" SGPIO=0,4,1,1 false ");
  }
  modem.enableGPS();


}

//Disables GPS module
void disableGPS(void)
{
  // Set SIM7000G GPIO4 LOW ,turn off GPS power
  // CMD:AT+SGPIO=0,4,1,0
  // Only in version 20200415 is there a function to control GPS power
  modem.sendAT("+SGPIO=0,4,1,0");
  if (modem.waitResponse(10000L) != 1) {
    DBG(" SGPIO=0,4,1,0 false ");
  }
  modem.disableGPS();
}

//send latitude and longitude to Hologram as [lat:long]
void sendLatLong(void)
{
  enableGPS();
  Serial.println("Getting GPS lock");
  float lat,  lon;
  while (1) {
    if (modem.getGPS(&lat, &lon)) {
      Serial.println("The location has been locked, the latitude and longitude are:");
      Serial.print("latitude:"); Serial.println(lat);
      Serial.print("longitude:"); Serial.println(lon);
      break;
    }
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(2000);
  }

  disableGPS();

  send_message(String(lat, 4) + ":" + String(lon, 4), "loc");

}

//Sends message on local Wifi. UNCOMMENT IF YOU'RE USING THIS FEATURE
//void sendWifiMessage(String message)
//{
//  Serial.println("making POST request");
//  Serial.println(message);
//  String postData = "request=" + message;
//  String contentType = "application/x-www-form-urlencoded";
//
//
//  localClient.beginRequest();
//  localClient.post("/api/webhook/-kcwLZgpmg7tAm6bt0pq41OEV", contentType, postData);
//  localClient.beginBody();
//  localClient.print(postData);
//  localClient.endRequest();
//
//  int httpCode = localClient.responseStatusCode();
//  Serial.println(httpCode);
//
//}

//Reboots the modem and sends the startup commands again, just to try and reconnect if there are any issues
void initModem(void)
{
  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  Serial.println("Initializing modem...");
  if (!modem.restart()) {
    Serial.println("Failed to restart modem, attempting to continue without restarting");
  }


  //Sets up wifi. UNCOMMENT IF YOU'RE USING WIFI
  //WiFi.mode(WIFI_STA);
  //WiFi.begin(wifiSSID, wifiPass);

  //Serial.print("Connecting to WiFi ..");
  //int attempts = 0;
  //while (WiFi.status() != WL_CONNECTED) {
  // Serial.print('.');
  //  delay(1000);
  //
  //  if ( attempts > 15)
  //  {
  //    break;
  //  }
  //  attempts++;
  //}
  //Serial.println(WiFi.localIP());

  //Server.begin();

  String name = modem.getModemName();
  delay(500);
  Serial.println("Modem Name: " + name);

  String modemInfo = modem.getModemInfo();
  delay(500);
  Serial.println("Modem Info: " + modemInfo);


  // Set SIM7000G GPIO4 LOW ,turn off GPS power
  // CMD:AT+SGPIO=0,4,1,0
  // Only in version 20200415 is there a function to control GPS power
  modem.sendAT("+SGPIO=0,4,1,0");
  if (modem.waitResponse(10000L) != 1) {
    Serial.println(" SGPIO=0,4,1,0 false ");
  }

  //Sets modem to high power mode - needed
  modem.sendAT("+CFUN=1 ");
  if (modem.waitResponse(10000L) != 1) {
    DBG(" +CFUN=1  false ");
  }
  delay(200);

  //Gets network mode and sets preferred mode as NB-IOT
  /*
    2 Automatic
    13 GSM only
    38 LTE only
    51 GSM and LTE only
  * * * */
  String res;
  res = modem.setNetworkMode(38);
  if (res != "1") {
    Serial.println("setNetworkMode  false ");
    return ;
  }
  delay(200);

  /*
    1 CAT-M
    2 NB-Iot
    3 CAT-M and NB-IoT
  * * */
  res = modem.setPreferredMode(1);
  if (res != "1") {

    Serial.println("setPreferredMode  false ");
    return ;
  }
  delay(200);
  
  //Connects to HOlogram
  Serial.println("Connecting to: " + String(apn));
  if (!modem.gprsConnect(apn)) {
    delay(10000);
    return;
  }
  
  //Sets multi-IP mode. Fails a lot (only needs to be set once) 
  modem.sendAT("+CIPMUX=1 ");
  if (modem.waitResponse(10000L) != 1) {
    Serial.println(" cipmux error ");
  }
  delay(200);
  
  //Sends commands to set up modem server. Not necessary every time, but why not (THIS WILL FAIL A LOT. Its ok!)
  modem.sendAT("+CIPSERVER=1,4010");
  if (modem.waitResponse(10000L) != 1) {
    Serial.println("Error opening server ");
  }
  delay(200);
}


void setup()
{
  // Set console baud rate
  Serial.begin(115200);
  delay(10);

  // Set LED OFF
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);
  delay(300);
  digitalWrite(PWR_PIN, LOW);

  Serial.println("\nWait...");

  delay(1000);

  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

  initModem();


  //Lets Hologram knows what mode is set. All times are in seconds - so default is 1hr messages. Adjust this if you're adjusting message times
  if (messageTime < 3600) {
    send_message("Fast GPS Mode Enabled", "status");
  } else if (messageTime < 21600)
  {
    send_message("Normal GPS Mode Enabled", "status");
  } else {
    send_message("Slow GPS Mode Enabled", "status");
  }

   if (modem.isNetworkConnected() && modem.isGprsConnected()) {
      Serial.println("All good");
      sendLatLong();
    }


}

void loop()
{
  //Sets X to number of seconds since boot
  x = (millis() / 1000);
  
  //If X minus last message is greater than the time that a message should've been sent, send a message!
  if ((x - lastMessage) >= messageTime)
  {
    if (modem.isNetworkConnected() && modem.isGprsConnected()) {
      Serial.println("All good");
      sendLatLong();
      lastMessage = (millis() / 1000);
    }
  }
  
  //If there is a message on the network, read it. 
  String resp = "";
  while (SerialAT.available()) {
    delay(10);  //delay to allow byte to arrive in input buffer
    char c = SerialAT.read();
    resp += c;
  }
  
  //CIPRXGET means there is a message to be recieved - send a command to ask for the message
  //This checks for several messages - SLOW,FAST,and NORM. SLOW is 6hrs, NORM is 1HR, and FAST is 1min
  //a VAN prefix tells it to forward the message to the local homeassistant server. Uncomment this if you want to use it!
  if (resp.indexOf("+CIPRXGET: 1") > 0)
  {
    modem.sendAT("+CIPRXGET=2," + String(resp.charAt(resp.length() - 3)) + ",5");
  } else if (resp.indexOf("SLOW") > 0)
  {
    Serial.println("SLOW MODE ENABLED");
    messageTime = 21600;
    send_message("SLOW GPS MODE ENABLED", "status");

  } else if (resp.indexOf("FAST") > 0)
  {
    Serial.println("FAST MODE ENABLED");
    messageTime = 60;
    send_message("Fast GPS Mode Enabled", "status");

  } else if (resp.indexOf("NORM") > 0)
  {
    Serial.println("NORMAL MODE ENABLED");
    messageTime = 3600;
    send_message("NORMAL GPS MODE ENABLED", "status");

  } 
  //else if (resp.indexOf("VAN") > 0)
  //{
  //  Serial.println("Forwarding to van");
  //  sendWifiMessage(resp.substring(resp.indexOf("VAN"), resp.lastIndexOf('\r') - 4 ));;
  //}

  //This checks to see if the local wifi server has someone trying to send it a message. Uncomment if using local wifi.
  //if (Server.hasClient())
  //{
  //  // If we are already connected to another computer,
  //  // then reject the new connection. Otherwise accept
  //  // the connection.
  //  if (RemoteClient.connected())
  //  {
  //    Serial.println("Connection rejected");
  //    Server.available().stop();
  //  }
  //  else
  //  {
  //    Serial.println("Connection accepted");
  //   RemoteClient = Server.available();
  //  }
  // }
  //The below code reads the message. Uncomment if using wifi!
  String wResp;
  //while (RemoteClient.available())
  //{
  //  delay(10);
  //  char c = RemoteClient.read();
  //  wResp += c;
  //}
  
  //This parses the message
  //if (wResp.indexOf("application/octet-stream") > 0)
  //{
  //  String wMessage;
  //  String tag;
  //  int equalsIndex;
  //  String nMessage;
  //  Serial.println(wResp);
  //  wMessage = wResp.substring(wResp.lastIndexOf('\n') + 1, wResp.length());
  //  equalsIndex = wMessage.indexOf('=');
  //  tag = wMessage.substring(0, equalsIndex);
  //  nMessage = wMessage.substring(equalsIndex + 1, wMessage.length());
  // if ((nMessage.length() > 0) and (tag.length() > 0))
  //  {
  //    Serial.println("Sending message to hologram from webhook");
  //    Serial.println(nMessage);
  //    Serial.println(tag);
  //    send_message(nMessage, tag);
  //  }
  //}
  
  //Checks to see if wifi isn't connected. If it isn't, give it another shot (only runs ever 30s)
  //if (((x % 30) == 0) && (WiFi.status() != WL_CONNECTED))
  //{
  //WiFi.begin(wifiSSID, wifiPass);
  //}

  //If there is no network (for SIM!) re-init the modem and increment fails
  if (((x % 56) == 0) && (!modem.isNetworkConnected() || !modem.isGprsConnected()))
  {
    Serial.println("No network, resetting - MOD based");
    initModem();
    fails++;
  }
  
  //If fails goes above a certain number, or the device has been running for 15 days then reboot.
  //The 15 days is just a failsafe
  if ((fails >= 5) || (x > 1296000))
  {
    Serial.println("No network, resetting - Fails Based or time based");
    ESP.restart();
  }

 
}
