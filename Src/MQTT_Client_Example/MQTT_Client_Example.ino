#include <Client.h>
#include <WiFi.h>
#include "Multitasking.h"
#include "MQTTClient.h"

#define _DEBUG_ 1
#define USER_BUTTON 0
bool Wifi_Connect();

char ssid[] = "TP-Link_EE70";
char pass[] = "dung01021994";
char host[] = "broker.hivemq.com";//broker.hivemq.com
// char host[] = "192.168.0.102";//broker.hivemq.com
uint16_t port = 1883;//1883
char clientid[] = "1234";
char username[] = "Dev1";
char password[] = "1234";
char topicname1[] = "dung/hello";
char topicname2[] = "dung/allo";
char topicname3[] = "dung/hallo";

CooperativeMultitasking tasks;
WiFiClient wificlient;
MQTTClient mqttclient(&tasks, &wificlient, host, port, clientid, username, password);
// MQTTTopic topic(&mqttclient, topicname);

Guard isPressed;
Continuation check_connection;
Continuation publish_task;

void setup() {
  Serial.begin(115200);
  pinMode(USER_BUTTON, INPUT_PULLUP);
  Wifi_Connect();
  tasks.now(task_schedule);
}

void loop() {
  tasks.run(); // receive connect acknowledgement, send publish, receive publish acknowledgement
}

bool Wifi_Connect(){
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    for (int i=0; i<50; i++){
      if (WiFi.status() != WL_CONNECTED) {
          delay(500);
          Serial.print(".");
      }
      else{
          break;
      }
    }

    // Make sure that we're actually connected, otherwise go to deep sleep
    if(WiFi.status() != WL_CONNECTED){
        Serial.println("");
        Serial.println("OPPS!!!");
        /* Epaper_Display_Error_WiFi(); */
        return false;
        //goToDeepSleep();
    }
    #if _DEBUG_
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    #endif
}

bool isPressed(){
  return (!digitalRead(USER_BUTTON));
}

void publish_task(){
  Serial.println("mqtt publish!");
  if (mqttclient.connect()) {
    // delay(100);
    String str1 = "Hi, this is msg no.1";
    const char *c1 = str1.c_str();
    mqttclient.publish(true, topicname1,c1);

    String str2 = "Hi, this is msg no.2";
    const char *c2 = str2.c_str();
    mqttclient.publish(true, topicname2,c2);

    String str3 = "Hi, this is msg no.3";
    const char *c3 = str3.c_str();
    mqttclient.publish(true, topicname3,c3);

    while (tasks.available()) {
      tasks.run();
    }
  }
  else{
    Serial.println("Can not connect to broker!");
    Serial.println();
  }
  mqttclient.disconnect();
  Serial.println("Disconnected broker!");
  tasks.now(task_schedule);
}

void check_connection(){
  Serial.println("Check connection...");
  switch (WiFi.status()) {
    case WL_CONNECT_FAILED:
    case WL_CONNECTION_LOST:
    case WL_DISCONNECTED: WiFi.begin(ssid, pass); // reconnect WiFi if necessary
  }
  tasks.now(task_schedule);
}

void task_schedule() {
  Serial.println("Main");
  auto task1 = tasks.after(10000, check_connection); // call on() in 10000 ms
  auto task2 = tasks.ifThen(isPressed, publish_task); // if isPressed() then call publish_task()
  // auto task3 = tasks.ifThen(isSerial, check);
  tasks.onlyOneOf(task1, task2); // do either task1 or task2
}