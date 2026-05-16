#include <pm2008_i2c.h>
#include <SoftwareSerial.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h> // [추가] LCD 라이브러리
#include "DHT.h"
#define DHTPIN 8 // 파란색 선 8번 
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
PM2008_I2C pm2008;
LiquidCrystal_I2C lcd(0x27, 16, 2); // [추가] LCD 설정 (주소 0x27, 16칸 2줄)
// A4 초록색 선 , A5 흰색 선
// 아두이노 핀 설정
const int UV_PIN = A0;
const int LED_G = 4; // 초록색 선 4번
const int LED_Y = 5; //노란색 선 5번
const int LED_R = 6;  //주황색선 6번

// 블루투스 핀 설정 (RX: 2번, TX: 3번) 흰색선 3번 , 회색선 2번
SoftwareSerial mySerial(2, 3); 
String receivedData = ""; 

bool powerOn = false;                // 전원 상태
bool lastPowerOn = false; 

String buffer = "";

unsigned long lastSendMs = 0;              
const unsigned long SEND_INTERVAL = 1000;

int num1 = 0;                        
int num2 = 0;                        
int num3 = 0;

String pad2(int v) {                 
  if (v < 10) return "0" + String(v); 
  return String(v);                   
} 

void sendNums(int n1, int n2, int n3, int n4) {         // 전송 함수 시작
  mySerial.print(pad2(n1));                     // Num1 값(2자리 고정) 출력
  mySerial.print(",");                          // 구분자
  mySerial.print(pad2(n2));                     // Num2 값(2자리 고정) 출력
  mySerial.print(",");                         // 구분자
  mySerial.print(pad2(n3));                   // Num3 값(2자리 고정) 출력 + 줄바꿈(\n)
  mySerial.print(", ");                         // 구분자
  mySerial.println(pad2(n4));

  Serial.print("PM2.5:");                        // PC에도 동일 출력(디버깅)
  Serial.print(pad2(n1));                       // Num1 값 출력(2자리)
  Serial.print(",uvIndex:");                       // Num2 라벨 출력
  Serial.print(pad2(n2));                       // Num2 값 출력(2자리)
  Serial.print(",temp:");                       // Num3 라벨 출력
  Serial.print(pad2(n3));                     // Num3 값 출력(2자리) + 줄바꿈
  Serial.print(",humi:");                       // Num3 라벨 출력
  Serial.println(pad2(n4));  
}       

void setup() {
  dht.begin(); 
  Serial.begin(9600);
  mySerial.begin(9600); 
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.clear(); // 시작 전 화면 지우기
  lcd.print("System Ready...");

  // --- PM2008M 초기화 및 가동 ---
  Wire.begin();    
  delay(1000);
  pm2008.begin();       
  
  delay(1000);          
  pm2008.command();     
  delay(2000);          

  pinMode(LED_G, OUTPUT);
  pinMode(LED_Y, OUTPUT);
  pinMode(LED_R, OUTPUT);

  Serial.println("==============================================");
  Serial.println("  환경 알림이 시작  ");
  Serial.println("==============================================");
  
  digitalWrite(LED_G, LOW);
  digitalWrite(LED_Y, LOW);
  digitalWrite(LED_R, LOW);
  
}

void loop() {
  int err;
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // --- 1. 센서 값 읽어오기 ---
  uint8_t ret = pm2008.read();
  int pm10 = 0, pm25 = 0, pm1_0 = 0;
  
  if (ret == 0) {
    pm1_0 = pm2008.pm1p0_tsi;
    pm25 = pm2008.pm2p5_tsi;
    pm10 = pm2008.pm10_tsi;
  
  // 자외선 센서 계산
  int uvAnalog = analogRead(UV_PIN);
  float uvVoltage = uvAnalog * (5.0 / 1023.0);
  float uvIndex = uvVoltage / 0.1; 

  //LCD 데이터 출력 ---

  while (mySerial.available()) {               // 수신 데이터 있으면
    char c = mySerial.read();                  // 1문자 읽기

    if (c == '\n') {                           // 줄 끝이면
      buffer.trim();                           // \r, 공백 제거

      if (buffer.length() > 0) {               // 빈 줄 아니면
        if (buffer == "1") powerOn = true;     // ON
        if (buffer == "0") powerOn = false;    // OFF
      }

      buffer = "";                             // 버퍼 비움
    } else {
      buffer += c;                             // 문자 누적
    }
  }

  if (powerOn != lastPowerOn) {                // 상태 변화면
    if (powerOn) {                             // ON이면
      Serial.println("POWER: ON");             // PC 로그
      mySerial.println("POWER:ON");            // BT 알림
    } else {                                   // OFF이면
      Serial.println("POWER: OFF");            // PC 로그
      mySerial.println("POWER:OFF");           // BT 알림

    }
    lastPowerOn = powerOn;                     // 상태 갱신
  }
  
  if (!powerOn) return;
  
  lcd.clear();
  // --- 3. 블루투스로 데이터 전송 (앱 인벤터용) ---
  lcd.setCursor(0, 0); // 1번째 줄
  lcd.print("PM2.5: "); 
  lcd.print(pm25); 
  lcd.print(" ug/m3  "); // 뒤에 공백은 잔상 제거용

  lcd.setCursor(0, 1); // 2번째 줄
  lcd.print("UV Index: "); 
  lcd.print(uvIndex);
 
  unsigned long now = millis();               
  if (now - lastSendMs >= SEND_INTERVAL) {     
    lastSendMs = now;                                              
    sendNums(pm25, uvIndex*10, t, h);                
  }
  
  if (pm25 >= 36 || uvIndex >= 6.0) {
    Serial.println(" ==> [나쁨]");
    digitalWrite(LED_R, HIGH);
    digitalWrite(LED_Y, LOW);
    digitalWrite(LED_G, LOW);
  } 
  else if (pm25 >= 16 || uvIndex >= 3.0) {
    Serial.println(" ==> [보통]");
    digitalWrite(LED_Y, HIGH);
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, LOW);
    
  } 
  else {
    Serial.println(" ==> [좋음]");
    digitalWrite(LED_G, HIGH);
    digitalWrite(LED_Y, LOW);
    digitalWrite(LED_R, LOW);
   
  }
  
  delay(2000); // 2초마다 측정
}}


