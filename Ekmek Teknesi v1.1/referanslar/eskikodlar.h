#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>

#define TIMER_INTERRUPT_DEBUG      0
#define _TIMERINTERRUPT_LOGLEVEL_  0
// Select a Timer Clock
#define USING_TIM_DIV1                true           // for shortest and most accurate timer
#define USING_TIM_DIV16               false           // for medium time and medium accurate timer
#define USING_TIM_DIV256              false            // for longest timer but least accurate. Default

#include <ESP8266_ISR_Timer.h> 
#include "ESP8266TimerInterrupt.h"

/* Set these to your desired softAP credentials. They are not configurable at runtime */
#ifndef APSSID
#define APSSID "Ekmek Teknesi"
#define APPSK  "12345678"
#endif
#define data 3
#define clock 1
#define btn1 0
#define btn2 2
#define dott 5
#define TIMER_INTERVAL_MS 2

volatile uint32_t TimerCount = 0;
ESP8266Timer ITimer;

byte sayilarb[]  = {B11111100, B01100000, B11011010, B11110010, B01100110, B10110110, B10111110, B11100000, B11111110, B11110110};
byte sayilara[] = {B00000011, B10011111, B00100101, B00001101, B10011001, B01001001, B01000001, B00011111, B00000001, B00001001};
byte bos  = B00000000;
byte ekmeksayi = 0;
bool segment1 = true;
int16_t buton1 = 0;
int16_t buton2 = 0;

const char *softAP_ssid = APSSID;
const char *softAP_password = APPSK;

/* hostname for mDNS. Should work at least on windows. Try http://esp8266.local */
const char *myHostname = "ekmekteknesi";

/* Don't set this wifi credentials. They are configurated at runtime and stored on EEPROM */
char ssid[33] = "";
char password[65] = "";

// DNS server
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Web server
ESP8266WebServer server(80);

/* Soft AP network parameters */
IPAddress apIP(172, 217, 28, 1);
IPAddress netMsk(255, 255, 255, 0);


/** Should I connect to WLAN asap? */
boolean connect;

/** Last time I tried to connect to WLAN */
unsigned long lastConnectTry = 0;

/** Current WLAN status */
unsigned int status = WL_IDLE_STATUS;

/** Is this an IP? */
boolean isIp(String str) {
  for (size_t i = 0; i < str.length(); i++) {
    int c = str.charAt(i);
    if (c != '.' && (c < '0' || c > '9')) {
      return false;
    }
  }
  return true;
}

/** IP to String? */
String toStringIp(IPAddress ip) {
  String res = "";
  for (int i = 0; i < 3; i++) {
    res += String((ip >> (8 * i)) & 0xFF) + ".";
  }
  res += String(((ip >> 8 * 3)) & 0xFF);
  return res;
}


/** Redirect to captive portal if we got a request for another domain. Return true in that case so the page handler do not try to handle the request again. */
boolean captivePortal() {
  if (!isIp(server.hostHeader()) && server.hostHeader() != (String(myHostname) + ".local")) {
    Serial.println("Request redirected to captive portal");
    server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
    server.send(302, "text/plain", "");   // Empty content inhibits Content-length header so we have to close the socket ourselves.
    server.client().stop(); // Stop is needed because we sent no content length
    return true;
  }
  return false;
}

void handleRoot() {
  if (captivePortal()) { // If caprive portal redirect instead of displaying the page.
    return;
  }
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");

  String Page;
  Page += F(
            "<!DOCTYPE html><html lang='en'><head>"
            "<META http-equiv= 'Content-Type' content= 'text/html' charset='UTF-8' name='viewport' content='width=device-width'> "
            "<title>CaptivePortal</title></head><body>"
            "<h1>HOŞGELDİNİZ!!</h1>");
  if (server.client().localIP() == apIP) {
    Page += String(F("<p>Ekmek Teknesi portalına bağlandınız"));
  } else {
    Page += String(F("<p>Wifi ağınızdan Ekmek Teknesine bağlandınız</p>"));
    
  }
    Page += String(F("<br/><br/><label style='font-size:50px;'>"))+ekmeksayi+String(F("</label><br/><br/>"));
    Page += String(F("<form name= 'btnformname' id= 'btnformid' method= 'POST' > "));
    Page += String(F("<input type= 'submit' value= 'Arttır' formaction= '/arti'> "));
    Page += String(F("<label > &emsp; &emsp; &emsp;</label> "));
    Page += String(F("<input type= 'submit' value= 'Azalt' formaction= '/eksi'> "));
    Page += String(F("<label > &emsp; &emsp; &emsp;</label> "));
    Page += String(F("<input type= 'submit' value= 'Sıfırla' formaction= '/sifir'> "));
    Page += String(F("</form> "));
  Page += F( 
            "<br><br><p><a href='/wifi'>Wifi ayarlarını yapılandırın</a>.</p>"
            "</body></html>");

  server.send(200, "text/html", Page);
}

void handleArti() {
  ekmeksayi++;
  handleRoot();
}

void handleEksi() {
  ekmeksayi--;
  handleRoot();
}

void handleSifir() {
  ekmeksayi = 0;
  handleRoot();
}




/** Wifi config page handler */
void handleWifi() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");

  String Page;
  Page += F(
            "<!DOCTYPE html><html lang='en'><head>"
            "<META http-equiv= 'Content-Type' content= 'text/html' charset='UTF-8' name='viewport' content='width=device-width'> "
            "<title>CaptivePortal</title></head><body>"
            "<h1>Wifi Yapılandırma</h1>");
  if (server.client().localIP() == apIP) {
    Page +=  softAP_ssid + String(F("<p> portalından bağlandınız. </p>"));
  } else {
    Page += ssid + String(F("<p> wifi ağından bağlandınız: </p>"));
  }
  Page +=
    String(F(
             "\r\n<br />"
             "<table><tr><th align='left'>Portal bilgileri:</th></tr>"
             "<tr><td>SSID : ")) + String(softAP_ssid) + F("</td></tr>"
             "<tr><td>IP : ") + toStringIp(WiFi.softAPIP()) +
             F("</td></tr></table>\r\n<br/>"
             "<table><tr><th align='left'>WLAN bilgileri:</th></tr>"
             "<tr><td>SSID : ") + String(ssid) + F("</td></tr>"
             "<tr><td>IP : ") + toStringIp(WiFi.localIP()) +
             F("</td></tr></table>\r\n<br/>"
             "<table><tr><th align='left'>WLAN listesi (ağınızı bulamıyorsanız sayfayı yenileyin)</th></tr>");
  //Serial.println("ağ arama başladı");
  int n = WiFi.scanNetworks();
  //Serial.println("arama bitti");
  if (n > 0) {
    for (int i = 0; i < n; i++) {
      Page += String(F("\r\n<tr><td><button type='button' onclick='function")) + i + String(F("()'>")) + WiFi.SSID(i) + String(F("</button>")) + ((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? F(" ") : F(" ")) + F(" (") + WiFi.RSSI(i) + F(")</td></tr>");
      
    }
  } else {
    Page += F("<tr><td>WLAN bulunamadı</td></tr>");
  }
  Page += F(
            "</table>"
            "\r\n<br /><form method='POST' action='wifisave'><h4>Bağlanılacak ağ:</h4>"
            "<input type='text' id='networkid' placeholder='network' name='n'/>"
            "<br /><input type='password' placeholder='password' name='p'/>"
            "<br /><input type='submit' value='Kaydet'/></form>"
            "<p><a href='/'>ana sayfaya dön.</a>.</p>"
            "</body>");
            
  if (n > 0) {
    Page += String(F("<script>"));
    for (int i = 0; i < n; i++) {
      Page += String(F("function function")) + i + String(F("() {document.getElementById('networkid').value ='")) + WiFi.SSID(i) + String(F("';}"));
    }
    
    Page += String(F("</script>"));
  }
  
  Page += String(F("</html>"));
  server.send(200, "text/html", Page);
  server.client().stop(); // Stop is needed because we sent no content length
}

void saveCredentials() {
  EEPROM.begin(512);
  EEPROM.put(0, ssid);
  EEPROM.put(0 + sizeof(ssid), password);
  char ok[2 + 1] = "OK";
  EEPROM.put(0 + sizeof(ssid) + sizeof(password), ok);
  EEPROM.commit();
  EEPROM.end();
}

/** Handle the WLAN save form and redirect to WLAN config page again */
void handleWifiSave() {
  //Serial.println("wifi kaydet");
  server.arg("n").toCharArray(ssid, sizeof(ssid) - 1);
  server.arg("p").toCharArray(password, sizeof(password) - 1);
  server.sendHeader("Location", "wifi", true);
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(302, "text/plain", "");    // Empty content inhibits Content-length header so we have to close the socket ourselves.
  server.client().stop(); // Stop is needed because we sent no content length
  saveCredentials();
  connect = strlen(ssid) > 0; // Request WLAN connect with new credentials if there is a SSID
}

void handleNotFound() {
  if (captivePortal()) { // If caprive portal redirect instead of displaying the error page.
    return;
  }
  String message = F("File Not Found\n\n");
  message += F("URI: ");
  message += server.uri();
  message += F("\nMethod: ");
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += F("\nArguments: ");
  message += server.args();
  message += F("\n");
  message += F("<br><br><p><a href='/'>ana sayfaya dön.</a>.</p>");
  

  for (uint8_t i = 0; i < server.args(); i++) {
    message += String(F(" ")) + server.argName(i) + F(": ") + server.arg(i) + F("\n");
  }
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.send(404, "text/plain", message);
}

/** Load WLAN credentials from EEPROM */
void loadCredentials() {
  EEPROM.begin(512);
  EEPROM.get(0, ssid);
  EEPROM.get(0 + sizeof(ssid), password);
  char ok[2 + 1];
  EEPROM.get(0 + sizeof(ssid) + sizeof(password), ok);
  EEPROM.end();
  if (String(ok) != String("OK")) {
    ssid[0] = 0;
    password[0] = 0;
  }
  //Serial.println("Recovered credentials:");
  //Serial.println(ssid);
  //Serial.println(strlen(password) > 0 ? "********" : "<no password>");
}


void btnoku() {

  bool button1 = digitalRead(btn1);
  bool button2 = digitalRead(btn2);
  //Serial.print(button1);
  //Serial.print("     ");
  //Serial.print(button2);

  if(button1 == false && button2 == true)
  {
    buton1++;
    if (buton1>150 && buton2<50)
    {
      buton1 = 0;
      buton2 = 0;
      ekmeksayi++;
    }
  }
  else if(button1 == true && button2 == false)
  {
    buton2++;
    if (buton1<50 && buton2>150)
    {
      buton1 = 0;
      buton2 = 0;
      ekmeksayi--;
    }
  }
  else if(button1 == true && button2 == true)
  {
    if (buton1>50 && buton2<50)
    {
      buton1 = 0;
      buton2 = 0;
      ekmeksayi++;
    }
    else if (buton1<50 && buton2>50)
    {
      buton1 = 0;
      buton2 = 0;
      ekmeksayi--;
    }
    else if (buton1>100 && buton2>100)
    {
      buton1 = 0;
      buton2 = 0;
      ekmeksayi = 0;
    }
    else 
    {
      buton1 = 0;
      buton2 = 0;
    }
    

    // iki butonada basılırsa ne olacak
    
  }
  else
  {
    buton1++;
    buton2++;
     // iki butonada basılıyorsa ne olacak
  }
  if (ekmeksayi<0)
  {
    ekmeksayi = 0;
  }
  else if (ekmeksayi>99)
  {
    ekmeksayi = 0;
  }
  
  //Serial.print(buton1);
  //Serial.print("     ");
  //Serial.println(button2);
}

/** Store WLAN credentials to EEPROM */
void TimerHandler()
{
  btnoku();
  int onlar = 0;
  int birler = 0;

  onlar = ekmeksayi/10;
  birler = ekmeksayi-(onlar*10);
  //Serial.print(onlar);
  //Serial.println(birler);
  if (segment1 == true)
  {
    segment1 = false;
    shiftOut(data, clock, LSBFIRST, sayilara[birler]);
    digitalWrite(dott,LOW);
  }
  else 
  { 
    segment1 = true;
    shiftOut(data, clock, LSBFIRST, sayilarb[onlar]);
    digitalWrite(dott,HIGH);
  }
}

void setup() {

  pinMode(clock, OUTPUT); // make the clock pin an output
  pinMode(data , OUTPUT); // make the data pin an output
  pinMode(dott , OUTPUT); // make the data pin an output
  pinMode(btn1, INPUT_PULLUP); // make the clock pin an output
  pinMode(btn2 , INPUT_PULLUP); // make the data pin an output
  shiftOut(data, clock, LSBFIRST, bos);

  // shiftOut(data, clock, LSBFIRST, sayilara[2]);
  // digitalWrite(dott,LOW);

  // timer
  if (ITimer.attachInterruptInterval(TIMER_INTERVAL_MS * 1000, TimerHandler))
  {
    Serial.print(F("Starting  ITimer OK, millis() = ")); Serial.println(millis());
  }
  else
    Serial.println(F("Can't set ITimer. Select another freq. or timer"));


  delay(1000);
  //Serial.begin(115200);
  //Serial.println();
  //Serial.println("Configuring access point...");
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(softAP_ssid, softAP_password);
  delay(500); // Without delay I've seen the IP address blank
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  /* Setup the DNS server redirecting all the domains to the apIP */
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  /* Setup web pages: root, wifi config pages, SO captive portal detectors and not found. */
  server.on("/", handleRoot);
  server.on("/wifi", handleWifi);
  server.on("/wifisave", handleWifiSave);
  server.on("/generate_204", handleRoot);  //Android captive portal. Maybe not needed. Might be handled by notFound handler.
  server.on("/fwlink", handleRoot);  //Microsoft captive portal. Maybe not needed. Might be handled by notFound handler.
  server.on("/arti", handleArti);
  server.on("/eksi", handleEksi);  
  server.on("/sifir", handleSifir);  
  server.onNotFound(handleNotFound);
  server.begin(); // Web server start
  Serial.println("HTTP server started");
  loadCredentials(); // Load WLAN credentials from network
  connect = strlen(ssid) > 0; // Request WLAN connect if there is a SSID
}

void connectWifi() {
  //Serial.println("Connecting as wifi client...");
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  int connRes = WiFi.waitForConnectResult();
  //Serial.print("connRes: ");
  //Serial.println(connRes);
}

void loop() {
  if (connect) {
    Serial.println("Connect requested");
    connect = false;
    connectWifi();
    lastConnectTry = millis();
  }
  {
    unsigned int s = WiFi.status();
    if (s == 0 && millis() > (lastConnectTry + 60000)) {
      /* If WLAN disconnected and idle try to connect */
      /* Don't set retry time too low as retry interfere the softAP operation */
      connect = true;
    }
    if (status != s) { // WLAN status change
      Serial.print("Status: ");
      Serial.println(s);
      status = s;
      if (s == WL_CONNECTED) {
        /* Just connected to WLAN */
        Serial.println("");
        Serial.print("Connected to ");
        Serial.println(ssid);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        // Setup MDNS responder
        if (!MDNS.begin(myHostname)) {
          Serial.println("Error setting up MDNS responder!");
        } else {
          Serial.println("mDNS responder started");
          // Add service to MDNS-SD
          MDNS.addService("http", "tcp", 80);
        }
      } else if (s == WL_NO_SSID_AVAIL) {
        WiFi.disconnect();
      }
    }
    if (s == WL_CONNECTED) {
      MDNS.update();
    }
  }
  // Do work:
  //DNS
  dnsServer.processNextRequest();
  //HTTP
  server.handleClient();

  

}




