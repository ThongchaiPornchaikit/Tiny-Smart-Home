#include <WiFiS3.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- ตั้งค่า Wi-Fi ---
char ssid[] = "Free";
char pass[] = "dargondark";

// --- ตั้งค่า LCD I2C ---
// (ที่อยู่ I2C ที่พบบ่อยคือ 0x27 หรือ 0x3F, ลองตรวจสอบด้วย I2C Scanner)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- ตั้งค่า Servo (ประตู) ---
Servo doorServo;
const int servoPin = 9;
bool isDoorOpen = false;
unsigned long lastSeenTime = 0;
const long doorCloseDelay = 3000; // 3 วินาที
bool autoDoorMode = true; // เริ่มต้นด้วยโหมดประตูอัตโนมัติ

// --- ตั้งค่า Ultrasonic ---
const int trigPin = 12;
const int echoPin = 11;
const int distanceThreshold = 20; // ระยะ (ซม.) ที่จะถือว่ามีคนอยู่หน้าประตู

// --- ตั้งค่า LDR และ LED ---
const int ldrPin = A0;
const int lightThreshold = 400; // ปรับค่าความมืดตรงนี้
bool autoLightMode = true;

// --- ตั้งค่า RGB LED (Common Cathode) ---
const int redPin = 3;
const int greenPin = 5;
const int bluePin = 6;

// --- ตั้งค่า Web Server ---
int status = WL_IDLE_STATUS;
WiFiServer server(80);

void setup() {
  Serial.begin(9600);
  while (!Serial);

  // --- เริ่มต้นอุปกรณ์ ---
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  
  doorServo.attach(servoPin);
  delay(500);
  doorServo.write(30); // เริ่มต้นประตูปิด

  // --- เริ่มต้น LCD ---
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Booting..");

  // --- เชื่อมต่อ Wi-Fi ---
  setupWifiAndServer();
  
  lcd.clear();
  lcd.print("System Online!");
}

void loop() {
  handleDoorSensor();
  handleLightSystem();
  handleWebServer();
}

// ===============================================
// ฟังก์ชันจัดการส่วนต่างๆ
// ===============================================

void handleDoorSensor() {
  if (!autoDoorMode) return;

  long duration, distance;
  
  // ยิงคลื่น Ultrasonic
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // รับคลื่นสะท้อน
  duration = pulseIn(echoPin, HIGH);
  distance = (duration / 2) / 29.1; // แปลงเป็น เซนติเมตร

  // Serial.println(distance); // สำหรับ Debug

  if (distance < distanceThreshold && distance > 0) {
    // ถ้าเจอคน
    if (!isDoorOpen) {
      openDoor();
    }
    lastSeenTime = millis(); // อัปเดตเวลาที่เจอคนล่าสุด
  } else {
    // ถ้าไม่เจอคน
    if (isDoorOpen && (millis() - lastSeenTime > doorCloseDelay)) {
      closeDoor();
    }
  }
}

void handleLightSystem() {
  if (autoLightMode) {
    int ldrValue = analogRead(ldrPin);
    // Serial.println(ldrValue); // สำหรับ Debug
    
    if (ldrValue < lightThreshold) {
      // มืด -> เปิดไฟ (สีขาวนวล)
      setLedColor(255, 230, 200);
    } else {
      // สว่าง -> ปิดไฟ
      setLedColor(0, 0, 0);
    }
  }
  // ถ้า autoLightMode เป็น false, ไฟจะถูกควบคุมโดยเว็บเท่านั้น
}

void handleWebServer() {
  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (currentLine.length() == 0) {
            sendHtmlPage(client);
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }

        // --- Light Control ---
        if (currentLine.endsWith("GET /auto")) {
          autoLightMode = true;
        } else if (currentLine.endsWith("GET /off")) {
          autoLightMode = false;
          setLedColor(0, 0, 0);
        } else if (currentLine.endsWith("GET /red")) {
          autoLightMode = false;
          setLedColor(255, 0, 0);
        } else if (currentLine.endsWith("GET /green")) {
          autoLightMode = false;
          setLedColor(0, 255, 0);
        } else if (currentLine.endsWith("GET /blue")) {
          autoLightMode = false;
          setLedColor(0, 0, 255);
        } else if (currentLine.endsWith("GET /white")) {
          autoLightMode = false;
          setLedColor(255, 255, 255);
        } 
        
        // --- Door Control (ส่วนที่เพิ่มใหม่) ---
        else if (currentLine.endsWith("GET /door_toggle")) {
          autoDoorMode = false; // ปิดโหมดอัตโนมัติเมื่อสั่งงานเอง
          toggleDoor();
        }
        else if (currentLine.endsWith("GET /door_auto")) {
          autoDoorMode = true; // กลับไปใช้โหมดอัตโนมัติ
        }
        
      }
    }
    client.stop();
  }
}

void sendHtmlPage(WiFiClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();
  
  client.println("<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  client.println("<meta charset=\"utf-8\"><title>Smart Home</title>");
  client.println("<style>");
  client.println(":root { --bg-color: #f4f7f6; --card-color: #ffffff; --text-color: #333; --accent-color: #007bff; --shadow-color: rgba(0,0,0,0.08); }");
  client.println("body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background-color: var(--bg-color); margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; color: var(--text-color); }");
  client.println("h1 { color: #444; font-weight: 600; margin-bottom: 30px; }");
  client.println(".card { background-color: var(--card-color); border-radius: 12px; box-shadow: 0 4px 12px var(--shadow-color); width: 100%; max-width: 400px; margin-bottom: 20px; padding: 25px; box-sizing: border-box; }");
  client.println("h2 { font-size: 1.25rem; font-weight: 600; margin-top: 0; margin-bottom: 20px; border-bottom: 1px solid #eee; padding-bottom: 10px; }");
  client.println(".btn-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }");
  client.println(".btn { display: block; text-decoration: none; padding: 15px; font-size: 1rem; font-weight: 500; color: var(--text-color); background-color: #f0f0f0; border: 1px solid #ddd; border-radius: 8px; cursor: pointer; text-align: center; transition: background-color 0.2s ease, box-shadow 0.2s ease; }");
  client.println(".btn:hover { background-color: #e9e9e9; box-shadow: 0 2px 4px var(--shadow-color); }");
  client.println(".btn-full { grid-column: 1 / -1; }");
  client.println(".btn-auto { background-color: #007bff; color: white; border: none; }");
  client.println(".btn-off { background-color: #6c757d; color: white; border: none; }");
  client.println(".btn-door-manual { background-color: #5a6268; color: white; border: none; }");
  client.println(".btn-door-auto { background-color: #28a745; color: white; border: none; }");
  client.println(".btn-red { background-color: #f44336; color: white; border: none; }");
  client.println(".btn-green { background-color: #4CAF50; color: white; border: none; }");
  client.println(".btn-blue { background-color: #007bff; color: white; border: none; }");
  client.println(".btn-white { background-color: #f8f9fa; color: #333; border: 1px solid #ddd; }");
  client.println("</style></head>");
  
  client.println("<body><h1>My Minimal Smart Home</h1>");
  
  // Card 1: Light Control
  client.println("<div class=\"card\">");
  client.println("<h2>Light Control</h2>");
  client.println("<div class=\"btn-grid\">");
  client.println("<a href=\"/auto\" class=\"btn btn-full btn-auto\">Auto Light Mode</a>");
  client.println("<a href=\"/red\" class=\"btn btn-red\">Red</a>");
  client.println("<a href=\"/green\" class=\"btn btn-green\">Green</a>");
  client.println("<a href=\"/blue\" class=\"btn btn-blue\">Blue</a>");
  client.println("<a href=\"/white\" class=\"btn btn-white\">White</a>");
  client.println("<a href=\"/off\" class=\"btn btn-full btn-off\">Turn Off Light</a>");
  client.println("</div></div>");

  // Card 2: Door Control
  client.println("<div class=\"card\">");
  client.println("<h2>Door Control</h2>");
  client.println("<div class=\"btn-grid\">");
  client.println("<a href=\"/door_auto\" class=\"btn btn-full btn-door-auto\">Auto Door Mode</a>");
  client.println("<a href=\"/door_toggle\" class=\"btn btn-full btn-door-manual\">Open / Close Manually</a>");
  client.println("</div></div>");
  
  client.println("</body></html>");
  client.println();
}

void openDoor() {
  doorServo.write(120); // หมุนไป 180 องศา (เปิดสุด)
  isDoorOpen = true;
  Serial.println("Door Opened");
  
  // แสดงข้อความต้อนรับบน LCD
  lcd.clear();
  lcd.setCursor(2, 0); // (col, row)
  lcd.print("Welcome Home!");
}

void closeDoor() {
  doorServo.write(30); // หมุนกลับไป 0 องศา (ปิด)
  isDoorOpen = false;
  Serial.println("Door Closed");

  // ลบข้อความต้อนรับ
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Door Closed");
}

void toggleDoor() {
  if (isDoorOpen) {
    closeDoor();
  } else {
    openDoor();
  }
}

// ฟังก์ชันควบคุมสี LED (แบบ Common Anode - แก้ไขแล้ว)
void setLedColor(int r, int g, int b) {
  // กลับตรรกะ: 0 คือสว่างสุด, 255 คือดับ
  analogWrite(redPin, 255 - r);
  analogWrite(greenPin, 255 - g);
  analogWrite(bluePin, 255 - b);
}
void setupWifiAndServer(){
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("WiFi module failed!");
    while (true);
  }
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);
  while (status != WL_CONNECTED) {
    status = WiFi.begin(ssid, pass);
    delay(5000);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi!");
  server.begin();
  IPAddress ip = WiFi.localIP();
  Serial.print("Web Server is at IP address: ");
  Serial.println(ip);
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Web Server IP:");
  lcd.setCursor(0,1);
  lcd.print(ip);
}