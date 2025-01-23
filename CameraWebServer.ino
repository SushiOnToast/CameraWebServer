#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include "esp_camera.h"
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

using namespace websockets;

// Replace with your network credentials
const char* ssid = "ESP32-Access-Point";
const char* password = "123456789";

// WebSocket server
WebsocketsServer wsServer;

// HTTP server for the camera
WiFiServer httpServer(80);

// Variable to store the HTTP request
String header;

// Camera configuration
void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA; // Use appropriate frame size
  config.jpeg_quality = 12;
  config.fb_count = 2;

  // Initialize the camera
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera initialization failed");
    while (1); // Halt if initialization fails
  }
}

// Handle the camera stream
void handleCameraStream(WiFiClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println();

  while (client.connected()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      continue;
    }

    // Send the camera feed to the client
    client.printf("--frame\r\n");
    client.printf("Content-Type: image/jpeg\r\n\r\n");
    client.write(fb->buf, fb->len);
    client.printf("\r\n");

    esp_camera_fb_return(fb);

    // Break the loop if the client disconnects
    if (!client.connected()) break;
  }
}

// Handle WebSocket client messages
void handleWebSocketMessage(WebsocketsClient& client, const WebsocketsMessage& message) {
  String receivedMessage = message.data();
  Serial.println("Received from client: " + receivedMessage);

  // Example: Respond to specific commands
  if (receivedMessage == "ping") {
    client.send("pong");
  } else if (receivedMessage == "status") {
    client.send("ESP32 is running and ready.");
  } else {
    client.send("Unknown command: " + receivedMessage);
  }
}

void setup() {
  Serial.begin(115200);

  // Set up the Access Point
  Serial.print("Setting AP (Access Point)…");
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Start the HTTP server
  httpServer.begin();

  // Initialize the camera
  setupCamera();

  // Start the WebSocket server
  wsServer.listen(81);
  Serial.println("WebSocket server started on port 81");
}

void loop() {
  // Handle HTTP requests for the camera feed
  WiFiClient client = httpServer.available();
  if (client) {
    Serial.println("New HTTP client.");
    String currentLine = ""; // Make a String to hold incoming data from the client
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c); // Print it to the serial monitor
        header += c;

        if (c == '\n') { // If the byte is a newline character
          if (currentLine.length() == 0) {
            // Handle `/stream` endpoint for the camera feed
            if (header.indexOf("GET /stream") >= 0) {
              handleCameraStream(client);
              break;
            }

            // Default response
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            client.println("<style>html { font-family: Helvetica; text-align: center;} .button { padding: 16px; }</style>");
            client.println("</head>");
            client.println("<body><h1>ESP32 Web Server</h1>");
            client.println("<p><a href=\"/stream\"><button class=\"button\">Stream Camera</button></a></p>");
            client.println("<p>WebSocket server is running on port 81.</p>");
            client.println("</body></html>");
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    header = "";
    client.stop();
    Serial.println("Client disconnected.");
  }

  // Handle WebSocket connections
  WebsocketsClient wsClient = wsServer.accept();
  if (wsClient.available()) {
    Serial.println("New WebSocket client connected.");
    wsClient.onMessage([&](WebsocketsMessage message) {
      handleWebSocketMessage(wsClient, message);
    });
    wsClient.poll();
  }
}
