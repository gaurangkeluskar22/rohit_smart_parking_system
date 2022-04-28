#include <Wire.h>
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <FirebaseESP8266.h>

#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include "DHT.h"
#define DHTTYPE DHT11
#define FIREBASE_HOST "rohit-smart-parking-system-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "Wgh75l9N3QxwljMz3vfaYShH2qNWELk2fy6KhleA"
#define WIFI_SSID "iBall-Baton"
#define WIFI_PASSWORD "G123456789"


#define NTP_OFFSET   60 * 60      // In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "europe.pool.ntp.org"
// buzzer pin 
#define buzzerPin 10
String apiKey = "19NCAN4R3KH0NHDL";
const char* server = "api.thingspeak.com";
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 2);
Servo servo;
DHT dht(2, DHTTYPE);
FirebaseData fbdo;
WiFiClient client;

float THRESHOLD = 40;
int slot1status = 0;
int slot2status = 0;

int IR = 0;               // 1st IR Sensor to open gate
int IR1 = 14;             // 1st IR Sensor
int IR2 = 12;             // 2nd IR Sensor

int Slot = 2;    //Total number of parking Slots
bool occ1=false;
bool occ2=false;


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

void setup() {
  Serial.begin(115200);
  timeClient.begin();
  delay(1000);
  pinMode(buzzerPin,OUTPUT);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);
  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("Connected to ");
  Serial.print(WIFI_SSID);
  
  Firebase.begin(FIREBASE_HOST,FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  
  lcd.begin(16,2);
  lcd.init();
  lcd.backlight();

  servo.attach(16);
  servo.write(85);

  dht.begin();
  
  pinMode(IR, INPUT);
  pinMode(IR1, INPUT);
  pinMode(IR2, INPUT);
  
}


void loop() {
  timeClient.update();
  if(Firebase.getInt(fbdo,"Status")) {
      if (fbdo.dataTypeEnum() == fb_esp_rtdb_data_type_integer) {
      Serial.println(fbdo.to<int>());
    }
  } else {
    Serial.println(fbdo.errorReason());
  }



  float h = dht.readHumidity();
  float t = dht.readTemperature();
  Serial.println(h);
  Serial.println(t);
  send_h_t_data_to_firebase(h,t);

  if(h>THRESHOLD){
      digitalWrite(buzzerPin,HIGH);
      delay(5000);
      digitalWrite(buzzerPin,LOW);
      THRESHOLD = THRESHOLD+10;
  }
  

  display_available_slot_message(Slot);  
  

  if(digitalRead(IR) == LOW){
    if(Slot>0){
      //Slot=Slot-1;
      servo.write(200);
      delay(2500);
      servo.write(85);
      delay(2500);
      //display_available_slot_message(Slot);  
      
    }else{
      display_full_slot_message();
    }
   }

   if(digitalRead(IR1) == LOW and occ1==false){
    // if car present and slot is not occupied
    if(Slot>0){
      Firebase.setInt(fbdo,"slot1 status",1);
      slot1status=1;
      Slot=Slot-1;
      occ1=true;
      display_available_slot_message(Slot);
    }
   }
   else if(digitalRead (IR1) == LOW and occ1==true){
    // if car is present and slot is occupied
    }
    else{
      if(Slot<2 and occ1==true and digitalRead(IR1) != LOW){
        Firebase.setInt(fbdo,"slot1 status",0);
        slot1status=0;
        occ1=false;
        Slot = Slot+1;
        display_available_slot_message(Slot);
      }
    }

    if(digitalRead (IR2) == LOW and occ2==false){
    // if car present and slot is not occupied
    if(Slot>0){
      Firebase.setInt(fbdo,"slot2 status",1);
      slot2status=1;
      Slot=Slot-1;
      occ2=true;
      display_available_slot_message(Slot);
    }
   }
   else if(digitalRead (IR2) == LOW and occ2==true){
    // if car is present and slot is occupied
    }
    else{
      if(Slot<2 and occ2==true and digitalRead(IR2) != LOW){
        Firebase.setInt(fbdo,"slot2 status",0);
        slot2status=0;
        occ2=false;
        Slot = Slot+1;
        display_available_slot_message(Slot);
      }
    }

    
}



void display_available_slot_message(int Slot){
  lcd.setCursor(0,0);
  lcd.print("   Welcome  ");
  lcd.setCursor(0,1);
  lcd.print("Slot Left: ");
  lcd.print(Slot);
}

void display_full_slot_message(){
  lcd.setCursor (0,0);
  lcd.print("    SORRY :(    ");  
  lcd.setCursor (0,1);
  lcd.print("  Parking Full  "); 
  delay (3000);
  lcd.clear(); 
}

void send_h_t_data_to_firebase(float h,float t){
  Firebase.setFloat(fbdo,"Humidity",h);
  Firebase.setFloat(fbdo,"Temperature",t);
  
  FirebaseJson json;
  unsigned long epochTime =  timeClient.getEpochTime();
  json.set("temperature", t);
  json.set("humidity", h);
  json.set("TimeStamp",epochTime);
  
  Firebase.pushJSON(fbdo, "DHT11", json);

  if(client.connect(server,80)){
    String postStr = apiKey;
    postStr += "&field1=";
    postStr += String(t);
    postStr += "&field2=";
    postStr += String(h);
    postStr += "&field3=";
    postStr += String(slot1status);
    postStr += "&field4=";
    postStr += String(slot2status);
    
    postStr += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: "+apiKey+"\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
  }
  client.stop();
  
  delay(5000);
}
