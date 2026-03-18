#include <WiFi.h> 
#include <WebSocketsServer.h>
#include <WebServer.h>
#include <ESP32Servo.h>

/* ================== WIFI ================== */
const char* ssid = "ESP32_FIREBOT";
const char* password = "12345678";

WebSocketsServer webSocket(81);
WebServer server(80);

/* ================== MOTOR ================== */
#define IN1 14
#define IN2 27
#define IN3 26
#define IN4 25

/* ================== ULTRASONIC ================== */
#define TRIG_LEFT 18
#define ECHO_LEFT 16
#define TRIG_FRONT 19
#define ECHO_FRONT 17
#define TRIG_RIGHT 23
#define ECHO_RIGHT 22

/* ================== FLAME ================== */
#define FLAME_LEFT 39
#define FLAME_CENTER 36
#define FLAME_RIGHT 34

/* ================== OUTPUT ================== */
#define SERVO_PIN 13
#define PUMP_PIN 21

Servo nozzleServo;

/* ================== STATES ================== */
bool autoMode = false;
bool manualPump = false;
bool fireDetected = false;

/* ================== UI PAGE ================== */
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>🔥 FireBot</title>
<style>
body { text-align:center; font-family:sans-serif; background:#111; color:#fff; }
button { width:90px; height:50px; margin:6px; font-size:16px; border-radius:10px; }
h2 { color:orange; }
#status { margin-top:10px; }
</style>
</head>
<body>

<h2>🔥 Fire Fighting Robot</h2>

<button onclick="send('F')">Forward</button><br>
<button onclick="send('L')">Left</button>
<button onclick="send('S')">Stop</button>
<button onclick="send('R')">Right</button><br>
<button onclick="send('B')">Back</button><br><br>

<button onclick="send('AUTO')">AUTO</button>
<button onclick="send('MANUAL')">MANUAL</button><br><br>

<button onclick="send('P_ON')">Pump ON</button>
<button onclick="send('P_OFF')">Pump OFF</button><br><br>

<button onclick="send('SL')">Left Spray</button>
<button onclick="send('SC')">Center</button>
<button onclick="send('SR')">Right Spray</button>

<p id="status">Status: Disconnected</p>

<script>
let ws;
function connectWS(){
    ws = new WebSocket("ws://" + location.hostname + ":81/");
    ws.onopen = () => { document.getElementById('status').innerHTML = 'Status: Connected'; }
    ws.onclose = () => { 
        document.getElementById('status').innerHTML = 'Status: Disconnected'; 
        setTimeout(connectWS, 2000); // reconnect
    }
}
connectWS();

function send(cmd){
  if(ws && ws.readyState === WebSocket.OPEN){
      ws.send(cmd);
  }
}
</script>

</body>
</html>
)rawliteral";

/* ================== MOTOR ================== */
void setMotor(int a,int b,int c,int d){
  digitalWrite(IN1,a);
  digitalWrite(IN2,b);
  digitalWrite(IN3,c);
  digitalWrite(IN4,d);
}

void forward(){ setMotor(1,0,0,1); }
void backward(){ setMotor(0,1,1,0); }
void left(){ setMotor(0,1,0,1); }
void right(){ setMotor(1,0,1,0); }
void stopRobot(){ setMotor(0,0,0,0); }

/* ================== DISTANCE ================== */
int getDistance(int trig,int echo){
  digitalWrite(trig,LOW); delayMicroseconds(2);
  digitalWrite(trig,HIGH); delayMicroseconds(10);
  digitalWrite(trig,LOW);

  long d = pulseIn(echo,HIGH,20000);
  if(d==0) return 300;
  return d*0.034/2;
}

/* ================== FIRE ================== */
bool detectFire(){
  int L = digitalRead(FLAME_LEFT);
  int C = digitalRead(FLAME_CENTER);
  int R = digitalRead(FLAME_RIGHT);

  if(L==LOW || C==LOW || R==LOW){
    fireDetected = true;
    stopRobot();

    if(C==LOW) nozzleServo.write(90);
    else if(L==LOW) nozzleServo.write(120);
    else nozzleServo.write(60);

    return true;
  }

  fireDetected = false;
  return false;
}

/* ================== AUTO ================== */
void autoDrive(){
  if(detectFire()) return;

  int f = getDistance(TRIG_FRONT,ECHO_FRONT);
  int l = getDistance(TRIG_LEFT,ECHO_LEFT);
  int r = getDistance(TRIG_RIGHT,ECHO_RIGHT);

  if(f > 25){
    forward();
  } else {
    stopRobot();
    unsigned long startTime = millis();
    while(millis() - startTime < 300){
      webSocket.loop();
      server.handleClient();
    }

    if(l > r) left();
    else right();

    startTime = millis();
    while(millis() - startTime < 300){
      webSocket.loop();
      server.handleClient();
    }
  }
}

/* ================== WEBSOCKET ================== */
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length){
  if(type != WStype_TEXT) return;

  String cmd = String((char*)payload);

  if(cmd=="AUTO") autoMode=true;
  else if(cmd=="MANUAL") autoMode=false;

  else if(cmd=="F") forward();
  else if(cmd=="B") backward();
  else if(cmd=="L") left();
  else if(cmd=="R") right();
  else if(cmd=="S") stopRobot();

  else if(cmd=="P_ON") manualPump=true;
  else if(cmd=="P_OFF") manualPump=false;

  else if(cmd=="SL") nozzleServo.write(120);
  else if(cmd=="SC") nozzleServo.write(90);
  else if(cmd=="SR") nozzleServo.write(60);
}

/* ================== SETUP ================== */
void setup(){
  Serial.begin(115200);

  pinMode(IN1,OUTPUT); pinMode(IN2,OUTPUT);
  pinMode(IN3,OUTPUT); pinMode(IN4,OUTPUT);
  stopRobot();

  pinMode(TRIG_LEFT,OUTPUT); pinMode(ECHO_LEFT,INPUT);
  pinMode(TRIG_FRONT,OUTPUT); pinMode(ECHO_FRONT,INPUT);
  pinMode(TRIG_RIGHT,OUTPUT); pinMode(ECHO_RIGHT,INPUT);

  pinMode(FLAME_LEFT,INPUT);
  pinMode(FLAME_CENTER,INPUT);
  pinMode(FLAME_RIGHT,INPUT);

  pinMode(PUMP_PIN,OUTPUT);
  digitalWrite(PUMP_PIN,LOW);

  nozzleServo.attach(SERVO_PIN);
  nozzleServo.write(90);

  WiFi.softAP(ssid,password);
  Serial.println(WiFi.softAPIP());

  // Web UI
  server.on("/", [](){
    server.send_P(200,"text/html",webpage);
  });
  server.begin();

  // WebSocket
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
}

/* ================== LOOP ================== */
void loop(){
  webSocket.loop();
  server.handleClient();

  bool pumpState = fireDetected || manualPump;
  digitalWrite(PUMP_PIN, pumpState);

  // 🚫 SAFETY: stop when pump ON
  if(pumpState){
    stopRobot();
    return;
  }

  if(autoMode){
    autoDrive();
  }
}
