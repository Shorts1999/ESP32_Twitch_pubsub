#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include "secret.h"
#include <HTTPClient.h>
#include <Adafruit_NeoPixel.h>

// #define IRC


//Setup SSID and Password:
const char ssid[] = NETWORK_SSID;
const char password[] = NETWORK_PASSWORD;

//Declare websocket
WebSocketsClient webSocket;
long long pingTimer = 0;
#define PING_PERIOD 600000u   //600 000 ms = 10 minutes

String message;


//Twitch parameters
const char twitch_oauth[] = OAUTH_TOKEN;
const char twitch_name[] = TWITCH_NAME;
const char twitch_channel[] = TWITCH_CHANNEL;

//Colour Constants:
#define RED     0xff0000U
#define BLUE    0x0000ffU
#define GREEN   0x00ff00U
#define MAGENTA 0xff00ffU
#define CYAN    0x00ffffU
#define YELLOW  0xffff00U
#define BLACK   0x000000U
#define WHITE   0xffffffU
const uint32_t colours[] = { RED, BLUE,GREEN,MAGENTA,CYAN,YELLOW,BLACK,WHITE };

//LED setup:
#define LED_PIN 14
#define NUM_LEDS 64
Adafruit_NeoPixel lights(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
unsigned long discoDuration = 0;

String pingMessage;

//prototypes:
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length);

//FreeRTOS task for disco lights:
void disco(void *args) {
  uint8_t colour;
  while (1) {
    if (discoDuration > 0) {
      colour = random8(6);
      for (uint8_t i = 0; i < NUM_LEDS; i++) {
        lights.setPixelColor(i, colours[colour]);
      }
      lights.show();
      vTaskDelay(500 / portTICK_PERIOD_MS);
      discoDuration--;
      Serial.printf("Disco duration: %ld\n", discoDuration);
    }
    else {
      // Serial.printf(""); //Needed to keep task alive apparently?
      //Set strip to white if no Disco:
      for (uint8_t i = 0; i < NUM_LEDS; i++) {
        lights.setPixelColor(i, WHITE);
      }
      lights.show();
    }
    // Serial.printf("DISCO IS RUNNING\n");
  }
}

void websocket(void *args) {
  while (1)  webSocket.loop();
}

//IRC VERSION:
#if defined(IRC)
void setup() {
  //Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.begin(115200);
  while (WiFi.status() != WL_CONNECTED) { //Wait to be connected
    Serial.print(".");
    delay(500);
  }
  Serial.print("\n\n IP address: ");
  Serial.println(WiFi.localIP());
  //set up LEDs
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);

  //Set up websocket:
  webSocket.begin("irc-ws.chat.twitch.tv", 80, "/");

  //Declare event handler:
  webSocket.onEvent(webSocketEvent);

  //Reconnect interval:
  webSocket.setReconnectInterval(5000);
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  Serial.printf("In websocketEvent");
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("[WSc] Connected to: %s\n", payload);
      webSocket.sendTXT("PASS " + String(twitch_oauth) + "\r\n");
      webSocket.sendTXT("NICK " + String(twitch_name) + "\r\n");
      webSocket.sendTXT("JOIN " + String(twitch_channel) + "\r\n");
      break;
    case WStype_TEXT:
    {
      Serial.printf("> %s\n", payload);
      String strcpy = String((char *)payload); //Converting to string to get "indexOf()" method
      int cmd_start = strcpy.indexOf(":!led "); //
      if (cmd_start > 0) {
        unsigned long decodedCmd = strtoul((char *)(payload + cmd_start + 5), NULL, 16); //Convert string hexLED data to unsigned long
        Serial.println(decodedCmd, HEX);
        for (int i; i < NUM_LEDS; i++) { //Turn all LEDs in the strip to submitted colour
          leds[i] = decodedCmd;
        }
        FastLED.show();
      }
      break;
    }
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] Disconnected!\n");
      webSocket.sendTXT("PART " + String(twitch_channel) + "\r\n");
      break;
    case WStype_ERROR:
      Serial.println("Error connecting to websocket");
      break;
    case WStype_PING:
      webSocket.sendTXT("tmi.twitch.tv"); //is this correct?
      break;
    default:
      Serial.printf("> %s\n", payload);
      break;
  }
}

void loop() {
  webSocket.loop();
  // if(Serial.available()){

  //   char currentChar = Serial.read();
  //   if(currentChar=='\n' || currentChar=='\r'){ //If newline character, send the message
  //     Serial.println(message);
  //     webSocket.sendTXT("PRIVMSG " + String(twitch_channel) + " :" + message + "\r\n"); //Write the message to the twitch chat
  //     message.clear(); //Clear the string to start a new message
  //   }
  //   else message+=String(currentChar); //Otherwise, add the character to the string
  // }
}
#else
void setup() {
  //Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.begin(115200);
  while (WiFi.status() != WL_CONNECTED) { //Wait to be connected
    Serial.print(".");
    delay(10);
  }
  randomSeed(millis()); //Time to connect can vary, giving different seeds
  Serial.print("\n\n IP address: ");
  Serial.println(WiFi.localIP());

  lights.begin();
  lights.setBrightness(150);
  discoDuration += 10;


  //HTTP Get:
  HTTPClient https;
  // https.begin("https://id.twitch.tv/oauth2/token?");
  // String requestURL = "client_id=" + String(CLIENT_ID) + "&client_secret=" + String(CLIENT_SECRET) + "&grant_type=client_credentials&scope=bits:read channel:read:redemptions channel:read:subscriptions";
  // Serial.println(String("\nhttps://id.twitch.tv/oauth2/token?") + requestURL);
  // uint8_t responseCode = https.POST(requestURL);
  // String getOauthMessage = https.getString();
  // Serial.println(getOauthMessage);
  // https.end();

  // DynamicJsonDocument doc(500);
  // deserializeJson(doc, getOauthMessage);
  // const char *Oauth = doc["access_token"];

  https.begin("https://id.twitch.tv/oauth2/validate");
  https.addHeader("Authorization", "Bearer " + String(NEW_TOKEN));
  https.GET();
  Serial.println(https.getString());
  https.end();
  //Start a JSON message:
  DynamicJsonDocument listen(400);

  listen["type"] = "LISTEN";
  listen["data"]["topics"][0] = "channel-points-channel-v1." + String(CHANNEL_ID);
  listen["data"]["auth_token"] = NEW_TOKEN;

  serializeJson(listen, message);


  //Set up websocket:
  webSocket.beginSSL("pubsub-edge.twitch.tv", 443, "/");

  //Declare event handler:
  Serial.println("attaching Event");
  webSocket.onEvent(webSocketEvent);

  //Reconnect interval:
  webSocket.setReconnectInterval(5000);
  xTaskCreatePinnedToCore(disco, "DISCO", 2048, NULL, 10, NULL, APP_CPU_NUM);
  xTaskCreatePinnedToCore(websocket, "PUBSUB", 8192, NULL, 10, NULL, APP_CPU_NUM);
  while (1);
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  // Serial.printf("In websocketEvent\n");
  switch (type) {
    case WStype_CONNECTED:
      Serial.printf("[WSc] Connected ");
      Serial.printf("with payload: %s \n", payload);
      Serial.println();
      pingMessage = "{\"type\":\"PING\"}";
      webSocket.sendTXT(pingMessage);
      // Serial.println(pingMessage);
      webSocket.sendTXT(message);
      // Serial.println(message);
      break;
    case WStype_TEXT:
    {
      DynamicJsonDocument twitchResponse(2048);
      String twitchResponseType;
      String twitchResponseMessage;
      deserializeJson(twitchResponse, payload);
      twitchResponseType = twitchResponse["type"].as<String>();
      if (twitchResponseType == "MESSAGE") {
        DynamicJsonDocument twitchMessage(2048);
        twitchResponseMessage = twitchResponse["data"]["message"].as<String>();
        deserializeJson(twitchMessage, twitchResponseMessage);
        // serializeJsonPretty(twitchMessage, Serial);
        String redemptionUser = twitchMessage["data"]["redemption"]["user"]["display_name"].as<String>();
        String redemptionTitle = twitchMessage["data"]["redemption"]["reward"]["title"].as<String>();
        Serial.printf("\n\n%s redeemed %s \n\n", redemptionUser.c_str(), redemptionTitle.c_str());
        if (redemptionTitle == "disco") {
          Serial.printf("Extending Disco Duration, now: ");
          discoDuration += 15;
          Serial.printf("%ld\n", discoDuration);
        }
      }
      else serializeJsonPretty(twitchResponse, Serial);
      if (twitchResponseType == "PING") {
        Serial.println("Sending PONG");
        webSocket.sendTXT("{\"type\": \"PONG\"");
      }
      break;
    }
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] Disconnected!\n");
      Serial.println(length);
      break;
    case WStype_ERROR:
      Serial.println("Error connecting to websocket");
      break;
    case WStype_PING:
      webSocket.sendTXT("{\"type\": \"PING\"");
      webSocket.sendTXT("{\"type\": \"PONG\"");
      break;
      webSocket.sendTXT("{\"type\": \"PING\"");
      webSocket.sendTXT("{\"type\": \"PONG\"");
      break;
    default:
      Serial.printf("> %s\n", payload);
      break;
  }
}

void loop() {
  // if(Serial.available()){

  //   char currentChar = Serial.read();
  //   if(currentChar=='\n' || currentChar=='\r'){ //If newline character, send the message
  //     Serial.println(message);
  //     webSocket.sendTXT("PRIVMSG " + String(twitch_channel) + " :" + message + "\r\n"); //Write the message to the twitch chat
  //     message.clear(); //Clear the string to start a new message
  //   }
  //   else message+=String(currentChar); //Otherwise, add the character to the string
  // }
}
#endif