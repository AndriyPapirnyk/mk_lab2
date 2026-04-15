#include <WiFi.h>
#include <WebServer.h>

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

volatile bool buttonPartnerFlag = false;
volatile unsigned long lastPartnerIsrTime = 0;

void IRAM_ATTR buttonISR() {
  unsigned long interruptTime = millis();
  if (interruptTime - lastIsrTime > 50) {
    isrButtonState = digitalRead(buttonPin);
    buttonFlag = true;
    lastIsrTime = interruptTime;
  }
}

void IRAM_ATTR buttonPartnerISR() {
  unsigned long interruptTime = millis();
  if (interruptTime - lastPartnerIsrTime > 50) {
    buttonPartnerFlag = true;
    lastPartnerIsrTime = interruptTime;
  }
}

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Lab1</title>
  <style>
    body { font-family: Arial; text-align: center; margin:0px auto; padding-top: 30px;}
    button { padding: 15px 30px; font-size: 18px; margin: 10px; cursor: pointer; border-radius: 5px; border: none; background-color: #2f4468; color: white;}
    .led { width: 50px; height: 50px; display: inline-block; margin: 10px; border: 2px solid #333; background-color: #ddd; border-radius: 5px; }
    .led.red.on { background-color: red; }
    .led.orange.on { background-color: orange; }
    .led.green.on { background-color: #00ff00; }
  </style>
  <script>
    setInterval(function() {
      fetch('/state')
        .then(response => response.text())
        .then(data => {
          let parts = data.split(',');
          document.getElementById('status').innerText = parts[1] === '1' ? 'Pause' : 'Working';
          let idx = parseInt(parts[0]);
          let paused = parts[1] === '1';
          document.getElementById('led0').className = 'led red ' + (idx === 0 && !paused ? 'on' : '');
          document.getElementById('led1').className = 'led orange ' + (idx === 1 && !paused ? 'on' : '');
          document.getElementById('led2').className = 'led green ' + (idx === 2 && !paused ? 'on' : '');
        });
    }, 300);
    function toggleOwn() { fetch('/toggle'); }
    function sendPartner() { fetch('/send'); }
  </script>
</head>
<body>
  <h1>lab1: alg 8 Papirnyk Andriy</h1>
  <div>
    <div id="led0" class="led red"></div>
    <div id="led1" class="led orange"></div>
    <div id="led2" class="led green"></div>
  </div>
  <p>Alg state: <b id="status">%STATE%</b></p>
  <button onclick="toggleOwn()">Switch my alg</button>
  <button onclick="sendPartner()" style="background-color: #a83232;">Switch partner alg</button>
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
  server.send(200, "text/plain", "ok");
}

void handleSend() {
  Serial.print("B"); 
  server.send(200, "text/plain", "ok");
}

void handleGetState() {
  String state = String(currentLed) + "," + String(isPaused ? 1 : 0);
  server.send(200, "text/plain", state);
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

void setup() {
  Serial.begin(9600); // rx0 tx0

  for (int i = 0; i < numLeds; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }
  digitalWrite(ledPins[currentLed], HIGH); 

  pinMode(buttonPin, INPUT); 
  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonISR, CHANGE);

  pinMode(buttonPartnerPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPartnerPin), buttonPartnerISR, FALLING);

  WiFi.softAP(ap_ssid, ap_password);
  
  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.on("/send", handleSend);
  server.on("/state", handleGetState);
  server.begin();
}

void loop() {
  server.handleClient(); 

  if (buttonPartnerFlag) {
    Serial.print("B"); 
    buttonPartnerFlag = false;
  }

  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'A') {
      isPaused = !isPaused;
    }
  }

  processButtonLogic();   
  executeLedAlgorithm();   
}
