#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

const char WiFiSSID[] = "SOC-LAB";
const char WiFiPSK[] = "itpsecurity";

const int LED_PIN = 5; // Thing's onboard LED
const String ALLOWED_IP = "192.168.20.128";

const String USERNAME = "NotAdmin";
const String PASSWORD = "NotDefault";

const int WHITE_LENGTH = 3;
const int BLACK_LENGTH = 10;

int WHITE_COUNT = 0;
int BLACK_COUNT = 0;

const int MAX_ATTEMPTS = 3;

char* WHITE_LIST[WHITE_LENGTH];
char* BLACK_LIST[BLACK_LENGTH];

WiFiServer server(80);

void setup() 
{
  initHardware();
  connectWiFi();
  server.begin();
  setupMDNS();
  initList(WHITE_LIST, WHITE_LENGTH);
  initList(BLACK_LIST, BLACK_LENGTH);
}

void loop() 
{
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Convert IPAddress object to char *
  char *clientIP = (char *)calloc(16, sizeof(char));
  strcpy(clientIP, client.remoteIP().toString().c_str());
  
  // Print out status of White/Blacklist
  Serial.println("Whitelist:");
  printList(WHITE_LIST, WHITE_LENGTH);
  Serial.println("Blacklist:");
  printList(BLACK_LIST, BLACK_LENGTH);
  
  int blacklisted = checkList(clientIP, BLACK_LIST, BLACK_LENGTH);
  
  if (blacklisted == 1){
    Serial.println("Client IP in Blacklist, exiting...");
    return;
  } else {
    Serial.println("Client IP not in Blacklist");
  }

  int whitelisted = checkList(clientIP, WHITE_LIST, WHITE_LENGTH);

  // Client not in whitelist
  if (whitelisted == 0){
    if (authenticate() == 1){
      delay(1);
      modifyList(clientIP, WHITE_LIST, WHITE_LENGTH, WHITE_COUNT);
    }
    else {
      delay(1);
      modifyList(clientIP, BLACK_LIST, BLACK_LENGTH, BLACK_COUNT);
      return;
    }
  }

  // Read the first line of the request
  String req = client.readStringUntil('\r');
  Serial.println(req);
  client.flush();

  // Match the request
  int val = -1; // We'll use 'val' to keep track of both the
                // request type (read/set) and value if set.
  if (req.indexOf("/led/0") != -1)
    val = 1; // Will write LED high
  else if (req.indexOf("/led/1") != -1)
    val = 0; // Will write LED low
  else if (req.indexOf("/blacklist/clear") != -1)
    clearList(BLACK_LIST, BLACK_LENGTH);
  else if (req.indexOf("/whitelist/clear") != -1)
    clearList(WHITE_LIST, WHITE_LENGTH);
  
  // Set GPIO5 according to the request
  if (val >= 0)
    digitalWrite(LED_PIN, val);

  client.flush();

  // Prepare the response. Start with the common header:
  String s = "HTTP/1.1 200 OK\r\n";
  s += "Content-Type: text/html\r\n\r\n";
  s += "<!DOCTYPE HTML>\r\n<html>\r\n";
  // If we're setting the LED, print out a message saying we did
  if (val >= 0)
  {
    s += "LED is now ";
    s += (val)?"off":"on ";
    s += "<br> <a href=/> home </a><br>";
  }
  else
  {
    s += "Welcome to the IoT using the ESP8266 Thing Dev Board <br>";
    s += "Turn an LED <a href=/led/1> On </a> or <a href=/led/0> Off </a> <br>";
  }
  s += "<br> <a href=/blacklist/clear> Clear blacklist </a>";
  s += "<br> <a href=/whitelist/clear> Clear whitelist </a>";
  s += "</html>\n";

  // Send the response to the client
  client.print(s);
  delay(1);
  Serial.println("Client disonnected");

  // The client will actually be disconnected 
  // when the function returns and 'client' object is detroyed
}

/*
 * Input: <client IP> <list> <list size>
 * Output: 1 if <client IP> is in <list>
 *         0 if <client IP> not in <list>
 */
int checkList(char * clientIP, char ** list, const int lenList){
  for (int i=0;i<lenList;i++){
    // strcmp(char* str1, char* str2) returns 0 if str1 == str2
    if (strcmp(list[i], clientIP) == 0) {
      return 1;
    }
  }
  return 0;
}

char * convertIP(String strIP){
  Serial.println(strIP);
  char charBuf[17];
  unsigned int len = sizeof(charBuf);
  strIP.toCharArray(charBuf,len);
  return charBuf;
}

/*
 * Input: <clientIP> <list> <lenList> <counter>
 * Output: <void>
 * 
 * Implements a circular queue, inserting <clientIP> into the oldest entry of <list>
 */
void modifyList(char * clientIP, char ** list, const int lenList, int counter){
  if (counter >= lenList) {
    counter = 0;
  }
  list[counter] = (char *)calloc(16, sizeof(char));
  list[counter] = clientIP;
  counter++;
}

/*
 * Initialize memory for list
 */
void initList(char ** list, const int lenList){
  for(int i = 0; i < lenList; i++){
    list[i] = (char *)calloc(16, sizeof(char));
    strcpy(list[i], "NULL");
  }
}

/*
 * Free and reinit list
 */
void clearList(char ** list, const int lenList){
  for(int i = 0; i < lenList; i++){
    free(list[i]);
  }
  initList(list, lenList);
}

/*
 * Prints all non-null list entries
 */
void printList(char ** list, const int lenList){
  for(int i = 0; i < lenList; i++){
    if(strcmp(list[i], "NULL") != 0){
      Serial.print("  [*] ");
      Serial.println(list[i]);
    }
  }
}

int authenticate(){
  
  // Counter to track authorization attempts
  int attempts = 0;
  
  // HTML auth form
  String s = "HTTP/1.1 200 OK\r\n";
  s += "Content-Type: text/html\r\n\r\n";
  s += "<!DOCTYPE HTML>\r\n<html>\r\n";
  s += "<form action='/submit' method='GET'>\r\n";
  s += "Username: <input type='text' name='username'><br>\r\n";
  s += "Password: <input type='password' name='password'><br><br>\r\n";
  s += "<input type='submit' value='Submit'><br><br>\r\n";
  s += "</form></html>";
  
  
  // Loop until MAX_ATTEMPTS is exceeded
  while (attempts <= MAX_ATTEMPTS){
    WiFiClient client = server.available();
    if (!client) {
      continue;
    }
    client.flush();
    client.print(s);
    delay(1);
    String req = client.readStringUntil('\r');
    Serial.println(req);
    Serial.print("Authentication Attempts: ");
    Serial.println(attempts);
    if (req.indexOf("submit?username=" + USERNAME + "&password=" + PASSWORD) != -1){
      String s = "Content-Type: text/html\r\n\r\n";  
      s += "<!DOCTYPE HTML>\r\n<html>\r\n";
      s += "<h1> Your client has been authenticated </h1>";
      s += "<p> <a href=/> Home </p>";
      s += "</html>";
      client.flush();
      client.print(s);
      delay(1);
      Serial.println("Client Whitelisted");
      return 1;
    }
    attempts++;

    // Send message after 3 failed attempts
    if (attempts > MAX_ATTEMPTS){
      String s = "Content-Type: text/html\r\n\r\n";  
      s += "<!DOCTYPE HTML>\r\n<html>\r\n";
      s += "<h1> Failed Authentication </h1>";
      s += "</html>";
      client.flush();
      client.print(s);
      delay(1);
      Serial.println("Client Blacklisted");
      return 0;
    }
  }
}

void connectWiFi()
{
  byte ledStatus = LOW;
  Serial.println();
  Serial.println("Connecting to: " + String(WiFiSSID));
  // Set WiFi mode to station (as opposed to AP or AP_STA)
  WiFi.mode(WIFI_STA);

  // WiFI.begin([ssid], [passkey]) initiates a WiFI connection
  // to the stated [ssid], using the [passkey] as a WPA, WPA2,
  // or WEP passphrase.
  WiFi.begin(WiFiSSID, WiFiPSK);

  // Use the WiFi.status() function to check if the ESP8266
  // is connected to a WiFi network.
  while (WiFi.status() != WL_CONNECTED)
  {
    // Blink the LED
    digitalWrite(LED_PIN, ledStatus); // Write LED high/low
    ledStatus = (ledStatus == HIGH) ? LOW : HIGH;

    // Delays allow the ESP8266 to perform critical tasks
    // defined outside of the sketch. These tasks include
    // setting up, and maintaining, a WiFi connection.
    delay(100);
    // Potentially infinite loops are generally dangerous.
    // Add delays -- allowing the processor to perform other
    // tasks -- wherever possible.
  }
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupMDNS()
{
  if (!MDNS.begin("KL-Thing")) 
  {
    Serial.println("Error setting up MDNS responder!");
    while(1) { 
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");

}

void initHardware()
{
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  WiFi.persistent(false);
}
