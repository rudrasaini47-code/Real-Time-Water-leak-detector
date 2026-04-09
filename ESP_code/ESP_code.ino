#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ── CONFIGURATION ──────────────────────────────────────────
const char* WIFI_SSID     = "ESP"; 
const char* WIFI_PASSWORD = "12345678"; 

ESP8266WebServer server(80);

// Data Variables
float  flowIn = 0.0, flowOut = 0.0, flowDiff = 0.0;
int    leak = 0, valveOpen = 1;
bool   hasData = false;
String rawString = "No data yet"; // For debugging on the website
unsigned long lastUpdate = 0;
String inputBuffer = "";

void setup() {
  Serial.begin(9600); // Must match Arduino Baud Rate
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // LED OFF

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // FIX: Connection Loop
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, LOW); // Solid ON when connected
  }

  server.on("/", handleRoot);
  server.on("/api/flow-data", handleFlowData);
  server.begin();
}

void loop() {
  server.handleClient();
  readSerial();
}

// ── API ENDPOINT ───────────────────────────────────────────
void handleFlowData() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String json = "{";
  json += "\"flow_in\":"   + String(flowIn, 2)   + ",";
  json += "\"flow_out\":"  + String(flowOut, 2)  + ",";
  json += "\"flow_diff\":" + String(flowDiff, 2) + ",";
  json += "\"leak\":"      + String(leak)         + ",";
  json += "\"valve_open\":" + String(valveOpen)   + ",";
  json += "\"raw\":\""     + rawString            + "\""; 
  json += "}";
  server.send(200, "application/json", json);
}

// ── WEBSITE HTML ───────────────────────────────────────────
void handleRoot() {
  String ip = WiFi.localIP().toString();
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{background:#080d14;color:#c8dff0;font-family:monospace;padding:20px;text-align:center}";
  html += ".card{background:#121a24;border:1px solid #1a2d44;padding:20px;border-radius:10px;display:inline-block;min-width:280px}";
  html += "h2{color:#00e5ff} .val{font-size:1.8em;color:#00e676;font-weight:bold}";
  html += ".debug{margin-top:20px;font-size:0.8em;color:#566d8a;word-break:break-all}</style></head><body>";

  html += "<div class='card'><h2>FlowWatch v2</h2>";
  html += "<p>Flow IN<br><span id='fin' class='val'>0.00</span> L/min</p>";
  html += "<p>Flow OUT<br><span id='fout' class='val'>0.00</span> L/min</p>";
  html += "<hr style='border-color:#1a2d44'>";
  html += "<p id='lstatus'>Status: Initializing...</p></div>";
  
  // Debug text to see the raw serial string
  html += "<div class='debug'>Raw Serial: <span id='raw'>...</span></div>";

  html += "<script>";
  html += "function update(){ fetch('/api/flow-data').then(r=>r.json()).then(d=>{";
  html += "document.getElementById('fin').innerText=d.flow_in.toFixed(2);";
  html += "document.getElementById('fout').innerText=d.flow_out.toFixed(2);";
  html += "document.getElementById('raw').innerText=d.raw;";
  html += "document.getElementById('lstatus').innerHTML=d.leak==1?'<b style=\"color:#ff1744\">⚠ LEAK DETECTED</b>':'<b>System OK</b>';";
  html += "}).catch(e=>document.getElementById('lstatus').innerText='Fetch Error: '+e); }";
  html += "setInterval(update,1000);</script></body></html>";

  server.send(200, "text/html", html);
}

// ── SERIAL PARSING ─────────────────────────────────────────
void readSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      inputBuffer.trim();
      if (inputBuffer.length() > 0) {
        rawString = inputBuffer; // Save for debug
        parseLine(inputBuffer);
      }
      inputBuffer = "";
    } else if (c != '\r') {
      inputBuffer += c;
    }
  }
}

void parseLine(String line) {
  // Must match Arduino: "IN:0.00,OUT:0.00,LEAK:0,VALVE:1,DIFF:0.00"
  if (line.indexOf("IN:") < 0) return;

  flowIn    = extractFloat(line, "IN:",    ",OUT:");
  flowOut   = extractFloat(line, "OUT:",   ",LEAK:");
  leak      = (int)extractFloat(line, "LEAK:",  ",VALVE:");
  valveOpen = (int)extractFloat(line, "VALVE:", ",DIFF:");
  
  // FIX: Look for the rest of the string after DIFF:
  int diffIndex = line.indexOf("DIFF:");
  if (diffIndex >= 0) {
    flowDiff = line.substring(diffIndex + 5).toFloat();
  }

  hasData = true;
  lastUpdate = millis();
}

float extractFloat(String src, String startMark, String endMark) {
  int s = src.indexOf(startMark);
  if (s < 0) return 0.0;
  s += startMark.length();
  int e = src.indexOf(endMark, s);
  if (e < 0) return 0.0;
  return src.substring(s, e).toFloat();
}