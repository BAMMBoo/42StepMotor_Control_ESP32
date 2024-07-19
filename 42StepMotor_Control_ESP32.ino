#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Arduino.h>
#include <Preferences.h>

Preferences preferences; // 闪存

const int stepPin_x = 12;   // 水平电机x的脉冲引脚连接到IO12
const int dirPin_x = 14;    // 水平电机x的方向引脚连接到IO14

const int stepPin_y = 2;   // 俯仰电机y的脉冲引脚连接到IO2
const int dirPin_y = 4;    // 俯仰电机y的方向引脚连接到IO4


const int MicroStep = 3200;        // 3200个脉冲转一圈 
const float motorStepAngle = 0.1125;  // 步进电机的步进角度0.1125
const int gearboxRatio = 188;   // 减速器的减速比
const float actualStepAngle = motorStepAngle / gearboxRatio;  // 实际步进角度


float currentAngle_x = 0.01;  // 水平X的当前角度
float currentAngle_y = 0.01;  // 俯仰Y的当前角度

// WiFi settings
const char* ssid = "AWOL"; // WiFi名称
const char* password = "123456789"; // WiFi密码

// MQTT Broker settings
const char *MQTT_SERVER   = "broker.emqx.io";  // EMQX broker endpoint
const char *SUBTOPIC  = "esp32/617";     // MQTT topic 收
const char *PUBTOPIC  = "python/617";     // MQTT topic 发
const char *MQTT_USRNAME  = "emqx";  // MQTT username for authentication
const char *MQTT_PASSWD   = "public";  // MQTT password for authentication
const int MQTT_PORT  = 1883;  // MQTT port (TCP)
const char* CLIENT_ID    = "My_ESP32";  //当前设备的clientid标志

WiFiClient espClient;
PubSubClient  client(espClient);
long lastMsg = 0;
float rec_X = 0.00;
float rec_Y = 0.00;
float rec_Z = 0.00;


void setup()
{
  Serial.begin(115200);
  pinMode(stepPin_y, OUTPUT);
  pinMode(dirPin_y, OUTPUT);

  // 初始化Preferences库
  preferences.begin("motor", false);

  // 读取上次保存的角度，如果没有数据，则默认为0度
  currentAngle_x = preferences.getFloat("angle_x", 0.00);
  currentAngle_y = preferences.getFloat("angle_y", 90.00); 
  Serial.print("Initial Angle_X:");
  Serial.print(currentAngle_x);
  Serial.print("Initial Angle_Y:");
  Serial.print(currentAngle_y);

  // 水平X初始化为0度
  moveMotorToAngle(0, currentAngle_x, 90.00);
  // 俯仰Y初始化为90度
  moveMotorToAngle(1, currentAngle_y, 90.00);


  // 连接wifi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // 连接MQTT服务器
  client.setServer(MQTT_SERVER, MQTT_PORT); //设定MQTT服务器与使用的端口，1883是默认的MQTT端口
  client.setCallback(callback);             //设定回调方式，当ESP8266收到订阅消息时会调用此方法

  // 将 “初始角度” 发出去
  // String X = String(currentAngle_x);
  // String Y = String(currentAngle_y);
  // String pub_data = X +","+ Y;
  // client.publish(PUBTOPIC, pub_data.c_str());
}


void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(CLIENT_ID,MQTT_USRNAME,MQTT_PASSWD)) {
      Serial.println("connected");
      // 连接成功时订阅主题
      client.subscribe(SUBTOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  // for (int i = 0; i < length; i++) {
  //   Serial.print((char)payload[i]); // 打印主题内容
  // }
  String receivedData = "";
  for (int i = 0; i < length; i++) {
    receivedData += (char)payload[i];
  }

  // 两个逗号的下标
  int commaIndex1 = receivedData.indexOf(',');
  int commaIndex2 = receivedData.lastIndexOf(',');
  String flagString = receivedData.substring(0, commaIndex1);
  String str_x = receivedData.substring(commaIndex1 + 1, commaIndex2);
  String str_y = receivedData.substring(commaIndex2 + 1);

  int flag = flagString.toInt();
  float rec_x = str_x.toFloat();
  float rec_y = str_y.toFloat();


  if (flag == 1) {              // 指定水平电机X的角度
    moveMotorToAngle(0, currentAngle_x, rec_x);
  }
  else if(flag == 2){           // 指定俯仰电机Y的角度
    moveMotorToAngle(1, currentAngle_y, rec_y);
  }
  //3-6 控制器, 每次增加0.001度
  else if (flag == 3) {         // 水平电机X角度 加 0.01
    moveMotorToAngle(0, currentAngle_x, currentAngle_x + 0.01);     
  }
  else if (flag == 4){          // 水平电机X角度 减 0.01
    moveMotorToAngle(0, currentAngle_x, currentAngle_x - 0.01);
  }
  else if (flag == 5){          // 俯仰电机Y角度 加 0.01
    moveMotorToAngle(1, currentAngle_y, currentAngle_y + 0.01);  
  }
  else if (flag == 6){          // 俯仰电机Y角度 减 0.01
    moveMotorToAngle(1, currentAngle_y, currentAngle_y - 0.01); 
  }

  if (currentAngle_x > 270.0) {
      currentAngle_x = 270.0;
    }
  if (currentAngle_x < 0.0){
    currentAngle_x = 0.0;
  }
  if (currentAngle_y > 180.0){
    currentAngle_y = 180.0;
  }
  if(currentAngle_y < 0.0){
    currentAngle_y = 0.0;
  }

  // 将角度发出去
  String X = String(currentAngle_x);
  String Y = String(currentAngle_y);
  String pub_data = X +","+ Y;
  client.publish(PUBTOPIC, pub_data.c_str());
  
  Serial.println(flag);
  Serial.println(pub_data);
}
void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}


// 传入电机标识motor（0为水平x，1为俯仰y）、原始角度和目标角度，并移动到目标角度
void moveMotorToAngle(int motor, float originalAngle, float targetAngle) {

  // 用于函数中的电机区分
  int stepPin = 0;
  int dirPin = 0;

  // 判断电机表示
  if (motor == 0){
    stepPin = stepPin_x;
    dirPin = dirPin_x;
  }
  else{
    stepPin = stepPin_y;
    dirPin = dirPin_y;
  }

  // 计算需要的步数
  float angleDifference = targetAngle - originalAngle;
  int steps;
  if (angleDifference < 0) {
    digitalWrite(dirPin, LOW);  // 设置方向为逆时针
  }
  else{
    digitalWrite(dirPin, HIGH); // 顺时针
  }
  
  steps = abs(angleDifference / actualStepAngle);
  Serial.println("步数：");
  Serial.println(steps);

  // 发送脉冲信号控制电机运动
  for (int i = 0; i < steps; i++) {
    digitalWrite(stepPin, HIGH);  // 发送高电平脉冲
    delayMicroseconds(500);       // 控制脉冲持续时间
    digitalWrite(stepPin, LOW);   // 脉冲信号恢复低电平
    delayMicroseconds(500);       // 控制脉冲间隔时间
  }

  // 保存当前角度到闪存
  if(motor == 0){
    currentAngle_x = targetAngle;
    preferences.putFloat("angle_x", currentAngle_x);
  } 
  else{
    currentAngle_y = targetAngle;
    preferences.putFloat("angle_y", currentAngle_y);
  }  
}
