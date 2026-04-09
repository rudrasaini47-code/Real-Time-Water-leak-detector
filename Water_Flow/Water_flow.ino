#include <SoftwareSerial.h>

// ── PIN DEFINITIONS ────────────────────────────────────────
#define FLOW_IN_PIN    2      
#define FLOW_OUT_PIN   3      
#define VALVE_PIN      5      
#define RESET_BTN_PIN  6      
#define BUZZER_PIN     7    
#define LED_PIN        8    

// NEW: Use pins 10 and 11 for the ESP8266
SoftwareSerial espSerial(10, 11); // RX, TX

// ... (Keep all your existing variables: flowIn, flowOut, etc.) ...

void triggerAlert(bool active) {
  if (active) {
    digitalWrite(LED_PIN, HIGH);      // LED on solid
    digitalWrite(BUZZER_PIN, HIGH);   // buzzer on
  } else {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

const float LEAK_THRESHOLD     = 2.0;   // L/min difference

// How many consecutive detections before triggering alert
// Prevents false alarms from sensor jitter
const int   CONFIRM_COUNT      = 3;     // 3 × 1 second = 3s sustained difference

// ── YF-S201 CALIBRATION ────────────────────────────────────
// 7.5 pulses/sec = 1 L/min  (YF-S201 datasheet)
// Adjust if your readings seem off (try 4.5–8.5 range)
const float CALIBRATION_FACTOR = 7.5;

// ── TIMING ─────────────────────────────────────────────────
const unsigned long MEASURE_INTERVAL = 1000;   // Calculate flow every 1s
const unsigned long SEND_INTERVAL    = 2000;   // Send to ESP every 2s
const unsigned long DEBOUNCE_MS      = 50;     // Button debounce

// ── PULSE COUNTERS ─────────────────────────────────────────
volatile unsigned long pulseCountIn  = 0;
volatile unsigned long pulseCountOut = 0;

// ── FLOW VALUES ────────────────────────────────────────────
float flowIn        = 0.0;    // Current L/min
float flowOut       = 0.0;    // Current L/min
float flowDiff      = 0.0;    // flowIn - flowOut
float totalIn       = 0.0;    // Total litres since boot
float totalOut      = 0.0;    // Total litres since boot

// ── LEAK & VALVE STATE ─────────────────────────────────────
int   leakDetected   = 0;     // 0=normal, 1=leak
bool  valveOpen      = true;  // true=open, false=closed
int   confirmCounter = 0;     // Consecutive leak detections
bool  leakAcknowledged = false; // Stays true until manual reset

// ── TIMERS ─────────────────────────────────────────────────
unsigned long lastMeasureTime  = 0;
unsigned long lastSendTime     = 0;
unsigned long lastBtnTime      = 0;

// ── INTERRUPT SERVICE ROUTINES ─────────────────────────────
void  countPulseIn()  { pulseCountIn++;  }
void  countPulseOut() { pulseCountOut++; }



void setup() {
  Serial.begin(9600);    // For your computer
  espSerial.begin(9600); // For the ESP8266

  // ... (Keep your pinMode and interrupt settings) ...
  
  pinMode(FLOW_IN_PIN, INPUT_PULLUP);
  pinMode(FLOW_OUT_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(VALVE_PIN, OUTPUT);
  digitalWrite(VALVE_PIN, LOW); 
  pinMode(RESET_BTN_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(FLOW_IN_PIN), countPulseIn, FALLING);
  attachInterrupt(digitalPinToInterrupt(FLOW_OUT_PIN), countPulseOut, FALLING);

  Serial.println("System Ready - Monitoring Serial Monitor and ESP8266");
}

void loop() {
  unsigned long now = millis();

  if (now - lastMeasureTime >= MEASURE_INTERVAL) {
    lastMeasureTime = now;
    calculateFlow();
    checkForLeak();
  }

  if (now - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = now;
    sendToESP();
  }

  checkResetButton();
}

void calculateFlow() {
  // Read and reset pulse counters atomically
  noInterrupts();
  unsigned long pIn  = pulseCountIn;
  unsigned long pOut = pulseCountOut;
  pulseCountIn  = 0;
  pulseCountOut = 0;
  interrupts();

  // Convert pulses → L/min
  // Measured over 1 second: pulses/sec ÷ 7.5 = L/min
  flowIn  = (float)pIn  / CALIBRATION_FACTOR;
  flowOut = (float)pOut / CALIBRATION_FACTOR;

  // Flow difference — the core leak detection metric
  flowDiff = flowIn - flowOut;

  // Accumulate totals (L/min over 1 second = L/60 per measurement)
  totalIn  += flowIn  / 60.0;
  totalOut += flowOut / 60.0;
}


// ═══════════════════════════════════════════════════════════
//  LEAK DETECTION & VALVE CONTROL
// ═══════════════════════════════════════════════════════════
void checkForLeak() {

  // ── Case 1: No flow at all — skip detection ──────────────
  // Avoid false alarms when system is idle
  if (flowIn < 0.5 && flowOut < 0.5) {
    confirmCounter = 0;
    // Only clear leak if it wasn't manually acknowledged
    if (!leakAcknowledged) {
      leakDetected = 0;
    }
    return;
  }

  // ── Case 2: Flow difference exceeds threshold ─────────────
  if (flowDiff > LEAK_THRESHOLD) {
    confirmCounter++;

    if (confirmCounter >= CONFIRM_COUNT && !leakAcknowledged) {
      // Confirmed leak — close the valve
      leakDetected     = 1;
      leakAcknowledged = true;
      closeValve();
      triggerAlert(true);  
    }
  } else {
    // Difference is within normal range — reset counter
    confirmCounter = 0;

    // Only clear the leak flag if not yet acknowledged
    // (keeps valve closed and alert active until manual reset)
    if (!leakAcknowledged) {
      leakDetected = 0;
    }
  }
}


// ═══════════════════════════════════════════════════════════
//  VALVE CONTROL
// ═══════════════════════════════════════════════════════════
void closeValve() {
  digitalWrite(VALVE_PIN, HIGH);   // HIGH = relay ON = valve CLOSED
  valveOpen = false;
}

void openValve() {
  digitalWrite(VALVE_PIN, LOW);    // LOW = relay OFF = valve OPEN
  valveOpen = true;
}


// ═══════════════════════════════════════════════════════════
//  MANUAL RESET BUTTON
//  Press to reopen valve after fixing the leak
// ═══════════════════════════════════════════════════════════
void checkResetButton() {
  unsigned long now = millis();

  // Debounce
  if (now - lastBtnTime < DEBOUNCE_MS) return;

  // Button pressed (pulled LOW)
  if (digitalRead(RESET_BTN_PIN) == LOW) {
    lastBtnTime = now;

    if (leakAcknowledged) {
      // Reset everything
      leakDetected     = 0;
      leakAcknowledged = false;
      confirmCounter   = 0;
      triggerAlert(false); 
      openValve();

      // Notify ESP of manual reset
      Serial.println("FLOWWATCH_RESET");
    }
  }
}

// Update this function to send to espSerial
void sendToESP() {
  // We send to BOTH so you can see it on your screen and the website
  String data = "IN:" + String(flowIn, 2) + 
                ",OUT:" + String(flowOut, 2) + 
                ",LEAK:" + String(leakDetected) + 
                ",VALVE:" + String(valveOpen ? 1 : 0) + 
                ",DIFF:" + String(flowDiff, 2);

  Serial.println(data);    // See it on your PC
  espSerial.println(data); // Send it to the Website
}

// ... (Keep the rest of your helper functions: calculateFlow, checkResetButton, etc.) ...