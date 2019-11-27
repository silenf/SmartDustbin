/*
 * Twilio SMS and MMS on ESP8266 Example.
 */
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266WebServer.h>
#include <BlynkSimpleEsp8266.h>
//#define LED D6
#include <SoftwareSerial.h>
#include "twilio.hpp"

// Use software serial for debugging?
#define USE_SOFTWARE_SERIAL 0
#define TRIGGER D1
#define ECHO    D3
#define LED D7
#define BLYNK_PRINT Serial

// Print debug messages over serial?
#define USE_SERIAL 1
static const int RXPin = 2, TXPin = 14;
SoftwareSerial ss(RXPin, TXPin);  // The serial connection to the GPS device

// Your network SSID and password
const char* ssid = "qwerty";
const char* pass = "87654321";

// Find the api.twilio.com SHA1 fingerprint, this one was valid as 
// of August 2019.
const char fingerprint[] = "06 86 86 C0 A0 ED 02 20 7A 55 CC F0 75 BB CF 24 B1 D9 C0 49";
String apiKey = "JPZKSYIDNIO48XA2";
// Twilio account specific details, from https://twilio.com/console
// Please see the article: 
// https://www.twilio.com/docs/guides/receive-and-reply-sms-and-mms-messages-esp8266-c-and-ngrok

// If this device is deployed in the field you should only deploy a revocable
// key. This code is only suitable for prototyping or if you retain physical
// control of the installation.
const char* account_sid = "YOUR_ACCOUNT_SID_TWILIO";
const char* auth_token = "YOUR_AUTH_TOKEN_TWILIO";
const char* server = "api.thingspeak.com";
char auth[] = "YOUR_AUTH_TOKEN";
const int LED_ao = 4; //Auth for Blynk
#define LED_a1 D6
#define LED_a2 D7

WiFiClient client;


// Details for the SMS we'll send with Twilio.  Should be a number you own 
// (check the console, link above).
String to_number    = "whatsapp:+91999XXXXXXX";
String from_number = "whatsapp:+1415523XXXX";
String message_body    = "Dustbin #3 initialized/Rebooted";

// The 'authorized number' to text the ESP8266 for our example
String master_number    = "whatsapp:+1415523XXXX";

// Optional - a url to an image.  See 'MediaUrl' here: 
// https://www.twilio.com/docs/api/rest/sending-messages
String media_url = "";

// Global twilio objects
Twilio *twilio;
ESP8266WebServer twilio_server(8000);

//  Optional software serial debugging
#if USE_SOFTWARE_SERIAL == 1
#include <SoftwareSerial.h>
// You'll need to set pin numbers to match your setup if you
// do use Software Serial
extern SoftwareSerial swSer(14, 4, false, 256);
#else
#define swSer Serial
#endif

/*
 * Callback function when we hit the /message route with a webhook.ss
 * Use the global 'twilio_server' object to respond.
 */
 void handle_message() {
        #if USE_SERIAL == 1
        swSer.println("Incoming connection!  Printing body:");
        #endif
        bool authorized = false;
        char command = '\0';

        // Parse Twilio's request to the ESP
        for (int i = 0; i < twilio_server.args(); ++i) {
                #if USE_SERIAL == 1
                swSer.print(twilio_server.argName(i));
                swSer.print(": ");
                swSer.println(twilio_server.arg(i));
                #endif

                if (twilio_server.argName(i) == "From" and 
                    twilio_server.arg(i) == master_number) {
                    authorized = true;
                } else if (twilio_server.argName(i) == "Body") {
                        if (twilio_server.arg(i) == "?" or
                            twilio_server.arg(i) == "0" or
                            twilio_server.arg(i) == "1") {
                                command = twilio_server.arg(i)[0];
                        }
                }
        } // end for loop parsing Twilio's request

        // Logic to handle the incoming SMS
        // (Some board are active low so the light will have inverse logic)
        String response = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
        if (command != '\0') {
                if (authorized) {
                        switch (command) {
                        case '0':
                                digitalWrite(LED_BUILTIN, LOW);
                                response += "<Response><Message>"
                                "Turning light off!"
                                "</Message></Response>";
                                break;
                        case '1':
                                digitalWrite(LED_BUILTIN, HIGH);
                                response += "<Response><Message>"
                                "Turning light on!"
                                "</Message></Response>";
                                break;
                        case '?':
                        default:
                                response += "<Response><Message>"
                                "0 - Light off, 1 - Light On, "
                                "? - Help\n"
                                "The light is currently: ";
                                response += digitalRead(LED_BUILTIN);
                                response += "</Message></Response>";
                                break;               
                        }
                } else {
                        response += "<Response><Message>"
                        "Unauthorized!"
                        "</Message></Response>";
                }

        } else {
                response += "<Response><Message>"
                "Look: a SMS response from an ESP8266!"
                "</Message></Response>";
        }

        twilio_server.send(200, "application/xml", response);
}


// WidgetMap myMap(V0);
void setup() {
        WiFi.begin(ssid, pass);

  Serial.begin (9600);
  Blynk.begin(auth, ssid, pass);
  pinMode(TRIGGER, OUTPUT);
  pinMode(LED_ao, OUTPUT); 
  pinMode(ECHO, INPUT);
  pinMode(BUILTIN_LED, OUTPUT);
  int index = 1;
  float lat = 51.5074;
  float lon = 0.1278;
 
        
        twilio = new Twilio(account_sid, auth_token, fingerprint);

        #if USE_SERIAL == 1
        swSer.begin(115200);
        while (WiFi.status() != WL_CONNECTED) {
                delay(1000);
                swSer.print(".");
        }
        swSer.println("");
        swSer.println("Connected to WiFi, IP address: ");
        swSer.println(WiFi.localIP());
        #else
        while (WiFi.status() != WL_CONNECTED) delay(1000);
        #endif

        // Response will be filled with connection info and Twilio API responses
        // from this initial SMS send.
        String response;
        bool success = twilio->send_message(
                to_number,
                from_number,
                message_body,
                response,
                media_url
        );

        // Set up a route to /message which will be the webhook url
        twilio_server.on("/message", handle_message);
        twilio_server.begin();

        // Use LED_BUILTIN to find the LED pin and set the GPIO to output
        pinMode(LED_BUILTIN, OUTPUT);

        #if USE_SERIAL == 1
        swSer.println(response);
        #endif
}


/* 
 *  In our main loop, we listen for connections from Twilio in handleClient().
 */
void loop() {

 long duration, distance;

  digitalWrite(TRIGGER, LOW);  
  delayMicroseconds(2); 
  
  digitalWrite(TRIGGER, HIGH);
  delayMicroseconds(10); 
  
  digitalWrite(TRIGGER, LOW);
  duration = pulseIn(ECHO, HIGH);
  distance = (duration/2) / 29.1;


 if (isnan(distance)) 
                 {
                     Serial.println("Failed to read from DHT sensor!");
                      return;
                 }

                         if (client.connect(server,80))   //   "184.106.153.149" or api.thingspeak.com
                      {  
                            
                             String postStr = apiKey;
                             postStr +="&field1=";
                             postStr += String(distance);
                           //  postStr +="&field2=";
                             //postStr += String(h);
                             postStr += "\r\n\r\n";
 
                             client.print("POST /update HTTP/1.1\n");
                             client.print("Host: api.thingspeak.com\n");
                             client.print("Connection: close\n");
                             client.print("X-THINGSPEAKAPIKEY: "+apiKey+"\n");
                             client.print("Content-Type: application/x-www-form-urlencoded\n");
                             client.print("Content-Length: ");
                             client.print(postStr.length());
                             client.print("\n\n");
                             client.print(postStr);
 
                             //Serial.print("Temperature: ");
                             //Serial.print(t);
                             //Serial.print(" degrees Celcius, Humidity: ");
                             //Serial.print(h);
                             //Serial.println("%. Send to Thingspeak.");
                        }
          client.stop();
 
          Serial.println("Waiting...");


  

   if (distance <= 26 && distance > 20) {
    Blynk.virtualWrite(V0, 255);
      analogWrite(LED_ao, 0); 
      analogWrite(LED_a1, 0);
      analogWrite(LED_a2, 255); 
}
  else {
    Blynk.virtualWrite(V0, 0);
  }

 if (distance <= 20 && distance > 15) {
    Blynk.virtualWrite(V1, 255);
    analogWrite(LED_ao, 0);
    analogWrite(LED_a2, 0);
    analogWrite(LED_a1, 0);
    
    analogWrite(LED_a2, 255);
}
  else {
    Blynk.virtualWrite(V1, 0);
  }

   if (distance <= 15 && distance > 10) {
    Blynk.virtualWrite(V2, 255);
    analogWrite(LED_ao, 0);
    analogWrite(LED_a1, 0);
    analogWrite(LED_a2, 255);
}
  else {
    Blynk.virtualWrite(V2, 0);
  }

   if (distance <= 10 && distance > 5) {
    Blynk.virtualWrite(V3, 255);
    analogWrite(LED_ao, 0);  
    analogWrite(LED_a2, 0);
    analogWrite(LED_a1, 255);
}
  else {
    Blynk.virtualWrite(V3, 0);
  }

   if (distance <= 5 && distance > 1) {
    Blynk.virtualWrite(V4, 255);
    analogWrite(LED_a1, 0);  
    analogWrite(LED_a2, 0);
      analogWrite(LED_ao, 255);  
         twilio_server.handleClient();

}
  else {
    Blynk.virtualWrite(V4, 0);
  }

  
  
  Serial.print(distance);
  Serial.println("Centimeter:");
  Blynk.virtualWrite(V5, distance);
  delay(200);
  Blynk.run();
  
     
}
