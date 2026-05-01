#include <WiFi.h>
#include <WebServer.h>

#define DEBUG_MODE 1

#ifdef DEBUG_MODE
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

const int ledPins[] = {2, 4, 5};
const int numLeds = 3;
const int buttonPin = 19;
const int buttonPartnerPin = 22; 

const char* ap_ssid = "ESP32_Lab1_WROOM";
const char* ap_password = "12345678";
WebServer server(80);

bool isPaused = false;
int currentLed = 0;
unsigned long lastBlinkTime = 0;
const int blinkInterval = 500; 

volatile bool buttonFlag = false;  
volatile int isrButtonState = LOW; 
volatile unsigned long lastIsrTime = 0;
unsigned long buttonPressTime = 0;
bool isHolding = false;
const int holdThreshold = 1000;         

volatile bool partnerButtonFlag = false;
volatile unsigned long lastPartnerIsrTime = 0;

void IRAM_ATTR buttonISR() {
  unsigned long interruptTime = millis();
  if (interruptTime - lastIsrTime > 300) { 
    isrButtonState = digitalRead(buttonPin);
    buttonFlag = true;
    lastIsrTime = interruptTime;
  }
}

void IRAM_ATTR partnerButtonISR() {
  unsigned long interruptTime = millis();
  if (interruptTime - lastPartnerIsrTime > 300) {
    partnerButtonFlag = true;
    lastPartnerIsrTime = interruptTime;
  }
}

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Lab1</title>
  <script>
    setInterval(function() {
      fetch('/state')
        .then(response => response.text())
        .then(data => {
          document.getElementById('status').innerText = data;
        });
    }, 500);
  </script>
</head>
<body>
<h1>lab1: alg 8 Papirnyk Andriy</h1>
<p>Alg state: <b id="status">%STATE%</b></p>
<button onclick="location.href='/toggle'">Switch alg</button>
<button onclick="fetch('/send')" style="background-color: #a83232; color: white;">Kick Partner</button>
</body>
</html>
)rawliteral";

void handleRoot() {
  String page = htmlPage;
  page.replace("%STATE%", isPaused ? "Pause" : "Working");
  server.send(200, "text/html", page); 
}

void handleToggle() {
  isPaused = !isPaused;
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSend() {
  partnerButtonFlag = true;
  server.send(200, "text/plain", "ok");
}

void handleGetState() {
  server.send(200, "text/plain", isPaused ? "Pause" : "Working");
}

void handleWebClient() {
  server.handleClient(); 
}

void processButtonLogic() {
  if (buttonFlag) {
    buttonFlag = false; 
    
    if (isrButtonState == HIGH) {
      buttonPressTime = millis();
      isHolding = false;
    } else {
      isHolding = false; 
    }
  }

  if (isrButtonState == HIGH && !isHolding) {
    if (millis() - buttonPressTime >= holdThreshold) {
      isPaused = !isPaused;
      isHolding = true; 
      DEBUG_PRINTLN(isPaused ? "\nAlgoritm stopped" : "\nAlgoritm resumed");
    }
  }
}

void executeLedAlgorithm() {
  if (!isPaused) {
    if (millis() - lastBlinkTime >= blinkInterval) {
      lastBlinkTime = millis();
      
      digitalWrite(ledPins[currentLed], LOW); 
      currentLed = (currentLed + 1) % numLeds; 
      digitalWrite(ledPins[currentLed], HIGH); 
    }
  }
}

void handlePartnerButton() {
  if (partnerButtonFlag) {
    partnerButtonFlag = false;
    DEBUG_PRINTLN("\n>>> Sending 'B' to Partner");
    Serial.print('B'); 
  }
}

void handleUART() {
  static unsigned long lastValidRxTime = 0;
  unsigned long startReadTime = millis();
  
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'A') {
      if (millis() - lastValidRxTime > 500) {
        lastValidRxTime = millis();
        DEBUG_PRINTLN("\n<<< Received 'A' from partner");
        isPaused = !isPaused;
        DEBUG_PRINTLN(isPaused ? "Algoritm stopped" : "Algoritm resumed");
      }
    }
    if (millis() - startReadTime > 50) break;
    yield();
  }
}

void setup() {
  Serial.begin(9600, SERIAL_8E1);

  for (int i = 0; i < numLeds; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }
  digitalWrite(ledPins[currentLed], HIGH);


  pinMode(buttonPin, INPUT); 
  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonISR, CHANGE);

  pinMode(buttonPartnerPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPartnerPin), partnerButtonISR, FALLING);

  DEBUG_PRINTLN("\n Running");
  WiFi.softAP(ap_ssid, ap_password);
  IPAddress IP = WiFi.softAPIP();
  DEBUG_PRINTLN("\n--- Wi-Fi Info ---");
  DEBUG_PRINT("SSID: "); DEBUG_PRINTLN(ap_ssid);
  DEBUG_PRINT("IP Address: "); DEBUG_PRINTLN(IP);
  DEBUG_PRINT("MAC Address: "); DEBUG_PRINTLN(WiFi.softAPmacAddress());
  DEBUG_PRINTLN("------------------\n");

  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.on("/send", handleSend); 
  server.on("/state", handleGetState);
  server.begin();
  DEBUG_PRINTLN("Web-server running!");
}

void loop() {
  handleWebClient();       
  processButtonLogic();   
  executeLedAlgorithm();   
  handlePartnerButton(); 
  handleUART(); 
}
