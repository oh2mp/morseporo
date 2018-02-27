#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <FS.h>
#include <Ticker.h>

#define APSSID "OH99PORO"
#define APPASS "12345678"
#define USESERIAL 1

#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13
#define D8 15
#define D9 3
#define D10 1

#define RELAY1 D1
#define RELAY2 D2

File file;
String html;
String okhtml;
String message;
String morsebin = "111010111010001110111010111000000"; // CQ placeholder
int morseindex = 0;
char *(morse[256]);
ESP8266WebServer server(80);
IPAddress apIP(10,0,36,99);
DNSServer dnsServer;

void ICACHE_RAM_ATTR morseTimerISR () {
    morseindex++;
    if (morseindex > morsebin.length()) {morseindex = 0;}
    if (morsebin.charAt(morseindex) == 0x31) {
        digitalWrite(RELAY1, HIGH);
        digitalWrite(RELAY2, HIGH);
    } else {
        digitalWrite(RELAY1, LOW);
        digitalWrite(RELAY2, LOW);
    }
    timer1_write(1000000);
}

void setup() {
    for (int i = 0; i < 256; i++) {morse[i] = NULL;}

    morse[(int)'a'] = ".-";
    morse[(int)'b'] = "-...";
    morse[(int)'c'] = "-.-.";
    morse[(int)'d'] = "-..";
    morse[(int)'e'] = ".";
    morse[(int)'f'] = "..-.";
    morse[(int)'g'] = "--.";
    morse[(int)'h'] = "....";
    morse[(int)'i'] = "..";
    morse[(int)'j'] = ".---";
    morse[(int)'k'] = "-.-";
    morse[(int)'l'] = ".-..";
    morse[(int)'m'] = "--";
    morse[(int)'n'] = "-.";
    morse[(int)'o'] = "---";
    morse[(int)'p'] = ".--.";
    morse[(int)'q'] = "--.-";
    morse[(int)'r'] = ".-.";
    morse[(int)'s'] = "...";
    morse[(int)'t'] = "-";
    morse[(int)'u'] = "..-";
    morse[(int)'v'] = "...-";
    morse[(int)'w'] = ".--";
    morse[(int)'x'] = "-..-";
    morse[(int)'y'] = "-.--";
    morse[(int)'z'] = "--..";
    morse[0xE4] = ".-.-";      // ä
    morse[0xF6] = "---.";      // ö
    morse[0xE5] = ".--.-";     // å
    morse[0xFC] = "..--";      // ü

    morse[(int)'1'] = ".----";
    morse[(int)'2'] = "..---";
    morse[(int)'3'] = "...--";
    morse[(int)'4'] = "....-";
    morse[(int)'5'] = ".....";
    morse[(int)'6'] = "-....";
    morse[(int)'7'] = "--...";
    morse[(int)'8'] = "---..";
    morse[(int)'9'] = "----.";
    morse[(int)'0'] = "-----";

    morse[(int)'.'] = ".-.-.-";
    morse[(int)','] = "--..--";
    morse[(int)'?'] = "..--..";
    morse[(int)'/'] = "-..-.";
    morse[(int)'-'] = "-....-";
    morse[0x20] = " ";

    pinMode(RELAY1, OUTPUT);
    pinMode(RELAY2, OUTPUT);

    digitalWrite(RELAY1, LOW);
    digitalWrite(RELAY2, LOW);
    
    SPIFFS.begin();
    file = SPIFFS.open("/index.html", "r");
    html = file.readString();
    file.close();
    file = SPIFFS.open("/ok.html", "r");
    okhtml = file.readString();
    file.close();
    file = SPIFFS.open("/message.txt", "r");
    message = file.readString();
    message.toLowerCase();
    file.close();
    message2morse();
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(APSSID, APPASS);

    dnsServer.setTTL(300);
    dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
    dnsServer.start(53, "*", apIP);

    timer1_attachInterrupt(morseTimerISR);
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
    timer1_write(1000000);
  
    server.on("/", handleRoot);
    server.on("/save", handleSave);
    server.onNotFound([]() {
        server.sendHeader("Location", "/"); 
        server.send(200, "text/plain", "foo");
    });
    server.begin();
        
    #ifdef USESERIAL
    Serial.begin(115200);
    Serial.println(message);
    Serial.println(morsebin);
    #endif
}

void message2morse() {
    cli();
    morsebin = "";
    for (int i = 0; i < message.length(); i++) {
         int foo = message.charAt(i);
         if (morse[foo]) {
             morsebin += morse[foo];
             morsebin += "000";
         }
    }
    morsebin.replace(".","10");
    morsebin.replace("-","1110");
    morsebin.replace(" ","000");
    morsebin += "000000";
    morseindex = 0;
    sei();
}


void handleSave() {
    server.sendHeader("Refresh", "3;url=/"); 
    server.send(200, "text/html; charset=ISO-8859-1", okhtml);

    String oldmessage = message;
    String newmessage = server.arg("msg");
    if (newmessage.length() < 1) {return;}
    newmessage.trim();
    newmessage.remove(256);
    newmessage.toLowerCase();

    message = "";
    for (int i = 0; i < newmessage.length(); i++) {
         int foo = newmessage.charAt(i);
         if (morse[foo]) {
             message += newmessage.charAt(i);
         }
    }
    if (message.length() < 1) {
        message = oldmessage;
        return;
    }
             
    file = SPIFFS.open("/message.txt", "w");
    file.print(message);
    file.close();
    message2morse();

    #ifdef USESERIAL
    Serial.println(message);
    Serial.println(morsebin);
    #endif
}

void handleRoot() {
    String htmlout = html;
    htmlout.replace("###MESSAGE###", message);
  
    server.send(200, "text/html; charset=ISO-8859-1", htmlout);
}

void loop() {
    dnsServer.processNextRequest();
    server.handleClient();
}

