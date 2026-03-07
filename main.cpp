/************* MOTOR DRIVER *************/
#define IN1 14
#define IN2 27
#define IN3 26
#define IN4 25
#define ENA 32
#define ENB 33

/************* ULTRASONIC SENSORS *************/
#define TRIG_L 18
#define ECHO_L 16

#define TRIG_F 19
#define ECHO_F 17

#define TRIG_R 23
#define ECHO_R 22

/************* FLAME SENSORS *************/
#define FLAME_L 39
#define FLAME_C 36
#define FLAME_R 34

/************* ACTUATORS *************/
#define SERVO_PIN 13
#define PUMP 21

/************* WIFI *************/
#include <WiFi.h>
#include <Servo.h>

const char* ssid = "ESP32_FIREBOT";
const char* password = "12345678";

WiFiServer server(80);
Servo fireServo;

bool autoMode = false;

/************* MOTOR FUNCTIONS *************/
void forward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
}
void backward() {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}
void left() {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
}
void right() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}
void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

/************* ULTRASONIC FUNCTION *************/
long readDistance(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH, 30000);
  return duration * 0.034 / 2;
}

/************* FIRE HANDLING *************/
bool handleFire() {
  int fL = digitalRead(FLAME_L);
  int fC = digitalRead(FLAME_C);
  int fR = digitalRead(FLAME_R);

  if (fL == LOW || fC == LOW || fR == LOW) {
    stopMotors();
    digitalWrite(PUMP, HIGH);

    if (fL == LOW)      fireServo.write(140);
    else if (fR == LOW) fireServo.write(40);
    else                fireServo.write(90);

    return true;
  }
  digitalWrite(PUMP, LOW);
  return false;
}

/************* AUTO MODE LOGIC *************/
void autoLogic() {
  if (handleFire()) return;

  long dL = readDistance(TRIG_L, ECHO_L);
  long dF = readDistance(TRIG_F, ECHO_F);
  long dR = readDistance(TRIG_R, ECHO_R);

  if (dF > 25) {
    forward();
  } 
  else if (dL > dR) {
    left();
  } 
  else {
    right();
  }
}

/************* SETUP *************/
void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);

  pinMode(TRIG_L, OUTPUT); pinMode(ECHO_L, INPUT);
  pinMode(TRIG_F, OUTPUT); pinMode(ECHO_F, INPUT);
  pinMode(TRIG_R, OUTPUT); pinMode(ECHO_R, INPUT);

  pinMode(FLAME_L, INPUT);
  pinMode(FLAME_C, INPUT);
  pinMode(FLAME_R, INPUT);

  pinMode(PUMP, OUTPUT);
  digitalWrite(PUMP, LOW);

  analogWrite(ENA, 200);
  analogWrite(ENB, 200);

  fireServo.attach(SERVO_PIN);
  fireServo.write(90);

  WiFi.softAP(ssid, password);
  server.begin();

  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.softAPIP());
}

/************* LOOP *************/
void loop() {

  if (autoMode) autoLogic();

  WiFiClient client = server.available();
  if (!client) return;

  String req = client.readStringUntil('\r');
  client.flush();

  if (req.indexOf("/auto") != -1) autoMode = true;
  if (req.indexOf("/manual") != -1) autoMode = false;

  if (!autoMode) {
    if (req.indexOf("/forward") != -1) forward();
    else if (req.indexOf("/backward") != -1) backward();
    else if (req.indexOf("/left") != -1) left();
    else if (req.indexOf("/right") != -1) right();
    else if (req.indexOf("/stop") != -1) stopMotors();
  }

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html\n");
  client.println("<h2>ESP32 Fire Fighting Robot</h2>");
  client.println("<a href='/auto'>AUTO MODE</a><br>");
  client.println("<a href='/manual'>MANUAL MODE</a><br><br>");
  client.println("<a href='/forward'>Forward</a><br>");
  client.println("<a href='/left'>Left</a>");
  client.println("<a href='/right'>Right</a><br>");
  client.println("<a href='/backward'>Backward</a><br>");
  client.println("<a href='/stop'>Stop</a>");
  client.stop();
}
