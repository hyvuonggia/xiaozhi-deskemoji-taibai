#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include "Face.h"

// WiFi configuration
const char* ssid = "your";  // Replace with your WiFi name (SSID)
const char* password = "your";  // Replace with your WiFi password
const char* mqtt_server = "your"; // Replace with your MQTT broker address
const char* mqtt_user = "your"; // Replace with your MQTT username
const char* mqtt_password = "your"; // Replace with your MQTT password

WiFiClient espClient;
PubSubClient client(espClient);
// Web server object
ESP8266WebServer server(80);

// Face object
Face *face;

// Initialize the WiFi connection
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

// Handle HTTP requests for the root path
void handleRoot() {
  String html = "<html><head><meta charset='UTF-8'></head><body><h1>Bảng điều khiển biểu cảm</h1>"
                "<p><a href=\"/set-emotion?emotion=Normal\">Bình thường</a></p>"
                "<p><a href=\"/set-emotion?emotion=Angry\">Giận</a></p>"
                "<p><a href=\"/set-emotion?emotion=Glee\">Vui sướng</a></p>"
                "<p><a href=\"/set-emotion?emotion=Happy\">Vui vẻ</a></p>"
                "<p><a href=\"/set-emotion?emotion=Sad\">Buồn</a></p>"
                "<p><a href=\"/set-emotion?emotion=Worried\">Lo lắng</a></p>"
                "<p><a href=\"/set-emotion?emotion=Focused\">Tập trung</a></p>"
                "<p><a href=\"/set-emotion?emotion=Annoyed\">Bực bội</a></p>"
                "<p><a href=\"/set-emotion?emotion=Surprised\">Ngạc nhiên</a></p>"
                "<p><a href=\"/set-emotion?emotion=Skeptic\">Nghi ngờ</a></p>"
                "<p><a href=\"/set-emotion?emotion=Frustrated\">Chán nản</a></p>"
                "<p><a href=\"/set-emotion?emotion=Unimpressed\">Thờ ơ</a></p>"
                "<p><a href=\"/set-emotion?emotion=Sleepy\">Buồn ngủ</a></p>"
                "<p><a href=\"/set-emotion?emotion=Suspicious\">Nghi ngờ</a></p>"
                "<p><a href=\"/set-emotion?emotion=Squint\">Nheo mắt</a></p>"
                "<p><a href=\"/set-emotion?emotion=Furious\">Cuồng nộ</a></p>"
                "<p><a href=\"/set-emotion?emotion=Scared\">Sợ hãi</a></p>"
                "<p><a href=\"/set-emotion?emotion=Awe\">Kinh ngạc</a></p>"
                "</body></html>";
  server.send(200, "text/html", html);
}

// Handle the set-emotion HTTP request
void handleSetEmotion() {
  String emotion = server.arg("emotion");
  setEmotion(emotion);
  server.send(200, "text/html", "<html><head><meta charset='UTF-8'></head><body><h1>Đã đặt biểu cảm: " + emotion + "</h1>"
                                "<p><a href=\"/\">Về trang chính</a></p></body></html>");
}

void setEmotion(String emotion) {
  if (emotion == "Normal") {
    face->Expression.GoTo_Normal();
  } else if (emotion == "Angry") {
    face->Expression.GoTo_Angry();
  } else if (emotion == "Glee") {
    face->Expression.GoTo_Glee();
  } else if (emotion == "Happy") {
    face->Expression.GoTo_Happy();
  } else if (emotion == "Sad") {
    face->Expression.GoTo_Sad();
  } else if (emotion == "Worried") {
    face->Expression.GoTo_Worried();
  } else if (emotion == "Focused") {
    face->Expression.GoTo_Focused();
  } else if (emotion == "Annoyed") {
    face->Expression.GoTo_Annoyed();
  } else if (emotion == "Surprised") {
    face->Expression.GoTo_Surprised();
  } else if (emotion == "Skeptic") {
    face->Expression.GoTo_Skeptic();
  } else if (emotion == "Frustrated") {
    face->Expression.GoTo_Frustrated();
  } else if (emotion == "Unimpressed") {
    face->Expression.GoTo_Unimpressed();
  } else if (emotion == "Sleepy") {
    face->Expression.GoTo_Sleepy();
  } else if (emotion == "Suspicious") {
    face->Expression.GoTo_Suspicious();
  } else if (emotion == "Squint") {
    face->Expression.GoTo_Squint();
  } else if (emotion == "Furious") {
    face->Expression.GoTo_Furious();
  } else if (emotion == "Scared") {
    face->Expression.GoTo_Scared();
  } else if (emotion == "Awe") {
    face->Expression.GoTo_Awe();
  }
}

void setup() {
  Serial.begin(9600);

  // Initialize the WiFi connection
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // Create a new face object; set screen width, height and eye size
  face = new Face(/* screenWidth = */ 128, /* screenHeight = */ 64, /* eyeSize = */ 40);
  // Set the current expression to Normal
  face->Expression.GoTo_Normal();

  // Automatically switch between behaviors (randomly pick a new behavior, weighted per emotion)
  face->RandomBehavior = true;

  // Automatic blinking
  face->RandomBlink = true;
  // Set the blink interval
  face->Blink.Timer.SetIntervalMillis(4000);

  // Automatically pick a new random look direction
  face->RandomLook = false;

  // Configure the web server routes
  server.on("/", handleRoot);
  server.on("/set-emotion", handleSetEmotion);
  server.begin();

  Serial.println("HTTP server started");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  // Handle web server client requests
  server.handleClient();

  // Update the face display
  face->Update();
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP8266Client_face", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      client.subscribe("emotion/control");
    } else {
      Serial.print("Connection failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == "emotion/control") {
    setEmotion(message);
    String response = "Emotion set to " + message;
    Serial.println(response);
    client.publish("emotion/status", response.c_str());
  }
}
