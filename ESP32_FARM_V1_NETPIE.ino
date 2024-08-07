#include <ModbusMaster.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define RS485_RX 4
#define RS485_TX 15
#define RS485_DIR 2

#define XY_MD02_ID 1
#define W_SPD_ID 2
#define W_DRT_ID 3

#define Soil_EN 3
#define Soil_ADC 36

#define pump 25
#define solenoid 14

ModbusMaster node1;
ModbusMaster node2;
ModbusMaster node3;

// WiFi and MQTT
const char* ssid = "KatanaIOT";
const char* password = "113333555555";
const char* mqtt_server = "broker.netpie.io";
const int mqtt_port = 1883;
const char* client_id = "1794fdbd-84f2-4d64-b56c-e626cde6a488"; 
const char* token = "RAgXEtcLv8SC4wDSvHDCGHnwTANhTNGt";
const char* secret = "URMVaKfXtQ6YYZDLxrqHBg5FS2S2ugLJ";

const char* sensorTopic = "@shadow/data/update";
const char* pumpTopic = "@msg/control/pump";
const char* solenoidTopic = "@msg/control/solenoid";

char myStr[128];

WiFiClient espClient;
PubSubClient client(espClient);

void preTransmission() {
  digitalWrite(RS485_DIR, 1);
}

void postTransmission() {
  digitalWrite(RS485_DIR, 0);
}

void setup_wifi() {
  delay(10);
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
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == pumpTopic) {
    if (message == "1") {
      digitalWrite(pump, HIGH);
    } else {
      digitalWrite(pump, LOW);
    }
  } else if (String(topic) == solenoidTopic) {
    if (message == "1") {
      digitalWrite(solenoid, HIGH);
    } else {
      digitalWrite(solenoid, LOW);
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(client_id, token, secret)) {
      Serial.println("connected");
      client.subscribe(pumpTopic);
      client.subscribe(solenoidTopic);
      client.setCallback(callback);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);

  pinMode(Soil_EN, OUTPUT);
  pinMode(RS485_DIR, OUTPUT);
  pinMode(pump, OUTPUT);
  pinMode(solenoid, OUTPUT);

  digitalWrite(Soil_EN, 0);
  digitalWrite(RS485_DIR, 0);

  node1.begin(XY_MD02_ID, Serial2);
  node1.preTransmission(preTransmission);
  node1.postTransmission(postTransmission);

  node2.begin(W_SPD_ID, Serial2);
  node2.preTransmission(preTransmission);
  node2.postTransmission(postTransmission);

  node3.begin(W_DRT_ID, Serial2);
  node3.preTransmission(preTransmission);
  node3.postTransmission(postTransmission);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Read Soil Moisture
  digitalWrite(Soil_EN, HIGH);
  delay(100);
  int ADC_value = analogRead(Soil_ADC);
  digitalWrite(Soil_EN, LOW);
  float Soil_Moisture = map(ADC_value, 620, 840, 100, 0);

  // Read Temperature and Humidity
  float temp = 0;
  float humi = 0;
  float w_spd = 0;
  char* w_drt = "C";
  uint8_t result;

  result = node1.readInputRegisters(0x01, 2);
  if (result == node1.ku8MBSuccess) {
    temp = node1.getResponseBuffer(0) / 10.0f;
    humi = node1.getResponseBuffer(1) / 10.0f;
  }
  result = node2.readInputRegisters(0x00, 1);
  if (result == node2.ku8MBSuccess) {
    w_spd = node2.getResponseBuffer(0) / 10.0;
  }
  result = node3.readInputRegisters(0x00, 1);
  if (result == node3.ku8MBSuccess) {
    switch( node3.getResponseBuffer(0) ){
      case 0:
        w_drt = "N";
        break;
      case 1:
        w_drt = "NE";
        break;
      case 2:
        w_drt = "E";
        break;
      case 3:
        w_drt = "SE";
        break;
      case 4:
        w_drt = "S";
        break;
      case 5:
        w_drt = "SW";
        break;
      case 6:
        w_drt = "W";
        break;
      case 7:
        w_drt = "NW";
        break;
    }
  }

  Serial.printf("Soil Moisture: %.2f %%\n", Soil_Moisture);
  Serial.printf("Temp: %.2f *C\n", temp);
  Serial.printf("Humi: %.2f %%RH\n", humi);
  Serial.printf("Wind Speed : %.2f m/s from %s\n", w_spd, w_drt);

  sprintf(myStr, "{\"data\":{\"Temperature\":%.2f, \"Humidity\":%.2f, \"SoilMoisture\":%.2f, \"WindSpeed\":%.2f, \"WindDirect\":\"%s\"}}", temp, humi, Soil_Moisture, w_spd, w_drt);
  if (client.publish(sensorTopic, myStr, false)) {
    Serial.println("Publish succeeded");
  } else {
    Serial.println("Publish failed");
  }

  delay(1000);  // Delay for stability
}
