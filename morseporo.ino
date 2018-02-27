/* This code will be commented later */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <FS.h>
#include <Ticker.h>

/* set SSID and password for AP */
#define APSSID "OH99PORO"
#define APPASS "12345678"

/* Comment out if you don't need debug info to serial port */
#define USESERIAL 1

/* Generic mapping of pins */
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

/* We use pins D1 and D2 to control two solid state relays. */
#define RELAY1 D1
#define RELAY2 D2

/* Definition of global variables */
File file;
String html;
String okhtml;
String message;
String morsebin = "111010111010001110111010111000000"; // CQ placeholder
int morseindex = 0;
char *(morse[256]);
ESP8266WebServer server(80); // Listen http
IPAddress apIP(10,0,36,99);  // The IP address of the AP
DNSServer dnsServer;

/* This function is run by timer interrupt and handles the blinking
   The morse message is encoded to a string containing 0's and 1's and
   depending of it the relays are switched on or off. On every interrupt
   we take next "bit" from the string and control the relays.
   String morsebin contains the "binary" string and morseindex is the
   index where we are going.
*/ 
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

/* setup */
void setup() {
    /* Initialize morse array */
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

    /* Initialize the data pins */
    pinMode(RELAY1, OUTPUT);
    pinMode(RELAY2, OUTPUT);

    digitalWrite(RELAY1, LOW);
    digitalWrite(RELAY2, LOW);

    /* Read the files from SPIFFS into the memory. File message.txt contains the message */
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
    message2morse(); // convert to "binary" string

    /* Start access point */
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(APSSID, APPASS);

    /* Start DNS server and set up a captive portal */
    dnsServer.setTTL(300);
    dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
    dnsServer.start(53, "*", apIP);

    /* Initialize timer interrupt */
    timer1_attachInterrupt(morseTimerISR);
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
    timer1_write(1000000);

    /* Define the URI's and error handler for web server and start it */
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

/* This function converts message to "binary" string */
void message2morse() {
    cli(); // Disable interrupts
    morsebin = "";
    /* iterate through message, allow only valid characters and add 3 time unit space to the end */
    for (int i = 0; i < message.length(); i++) {
         int foo = message.charAt(i);
         if (morse[foo]) {
             morsebin += morse[foo];
             morsebin += "000";
         }
    }
    /* replace dots, dashes and spaces to bin patterns */
    morsebin.replace(".","10");
    morsebin.replace("-","1110");
    morsebin.replace(" ","000");
    /* add word space for six time units */
    morsebin += "000000";
    morseindex = 0;
    sei(); // Enable interrupts
}

/* Handle http request for saving the message from the form */
void handleSave() {
    server.sendHeader("Refresh", "3;url=/"); 
    server.send(200, "text/html; charset=ISO-8859-1", okhtml);

    String oldmessage = message;
    String newmessage = server.arg("msg");
    if (newmessage.length() < 1) {return;} // Don't accept empty message
    /* Trim whitespaces off, truncate message to 256 chars and convert to lowercase */
    newmessage.trim();
    newmessage.remove(256);
    newmessage.toLowerCase();

    /* Iterate through message and add only allowed characters */
    message = "";
    for (int i = 0; i < newmessage.length(); i++) {
         int foo = newmessage.charAt(i);
         if (morse[foo]) {
             message += newmessage.charAt(i);
         }
    }
    
    /* if message became empty, rollback to old message */
    if (message.length() < 1) {
        message = oldmessage;
        return;
    }

    /* save message to file and convert to "binary" */
    file = SPIFFS.open("/message.txt", "w");
    file.print(message);
    file.close();
    message2morse();

    #ifdef USESERIAL
    Serial.println(message);
    Serial.println(morsebin);
    #endif
}

/* Handle http request to root */
void handleRoot() {
    String htmlout = html;
    htmlout.replace("###MESSAGE###", message);
  
    server.send(200, "text/html; charset=ISO-8859-1", htmlout);
}

/* Main loop. Wait and handle DNS and HTTP requests forever */
void loop() {
    dnsServer.processNextRequest();
    server.handleClient();
}
/* The end */

