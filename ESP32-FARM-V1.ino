#include <ModbusMaster.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define RS485_RX 4
#define RS485_TX 15
#define RS485_DIR 2

#define XY_MD02_ID 1

#define Soil_EN 3
#define Soil_ADC 36

#define pump 25
#define solenoid 14

ModbusMaster node;

// WiFi and MQTT
const char* ssid = "<< WIFI_NAME >>";
const char* password = "<< WIFI_PASS >>";
const char* mqtt_server = "<< MQTT_BROKER >>";
const int mqtt_port = 1883;
const char* clientID = "<< MQTT_CLIENTID >>";

const char* sensorTopic = "sensor/value";
const char* pumpTopic = "control/pump";
const char* solenoidTopic = "control/solenoid";

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
    if (client.connect(clientID)) {
      Serial.println("connected");
      client.subscribe(pumpTopic);
      client.subscribe(solenoidTopic);
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

  node.begin(XY_MD02_ID, Serial2);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Increase keep-alive interval
  client.setKeepAlive(120); // Set keep-alive interval to 120 seconds
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
  uint8_t result = node.readInputRegisters(1, 2);
  if (result == node.ku8MBSuccess) {
    temp = node.getResponseBuffer(0) / 10.0f;
    humi = node.getResponseBuffer(1) / 10.0f;
  }

  Serial.print("Soil Moisture: ");
  Serial.println(Soil_Moisture);

  Serial.printf("Temp: %.01f *C\n", temp);
  Serial.printf("Humi: %.01f %%RH\n", humi);

  // Create JSON payload
  snprintf(myStr, sizeof(myStr), "{\"Temperature\":%.2f, \"Humidity\":%.2f, \"SoilMoisture\":%.2f}", temp, humi, Soil_Moisture);

  // Publish sensor data as JSON
  if (client.publish(sensorTopic, myStr, false)) {
    Serial.println("Publish succeeded");
  } else {
    Serial.println("Publish failed");
  }

  delay(1000);  // Delay for stability
}
