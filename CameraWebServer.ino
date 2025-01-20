#include <WiFi.h>
#include "esp_camera.h"
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// Replace with your network credentials
const char* ssid = "ESP32-Access-Point";
const char* password = "123456789";

// Set web server port number to 80
WiFiServer server(80);

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

void findRedDotCoordinates(camera_fb_t* fb, int& x, int& y) {
  // Initialize coordinates
  x = -1;
  y = -1;

  int maxRedIntensity = 0; // To track the brightest red spot
  uint8_t* data = fb->buf; // Pointer to frame buffer data
  int width = fb->width;
  int height = fb->height;

  for (int i = 0; i < width * height * 2; i += 2) { // Iterate through each pixel (RGB565 format)
    uint8_t highByte = data[i];
    uint8_t lowByte = data[i + 1];

    // Extract RGB components
    uint8_t red = (highByte & 0xF8);         // 5 bits for red
    uint8_t green = ((highByte & 0x07) << 3) | ((lowByte & 0xE0) >> 5); // 6 bits for green
    uint8_t blue = (lowByte & 0x1F) << 3;    // 5 bits for blue

    // Check if the red intensity is high and higher than green and blue
    if (red > 100 && red > green * 1.5 && red > blue * 1.5) {
      int intensity = red - (green + blue); // Calculate the "redness"
      if (intensity > maxRedIntensity) {
        maxRedIntensity = intensity;
        int pixelIndex = i / 2;
        x = pixelIndex % width;
        y = pixelIndex / width;
      }
    }
  }
}

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

    int redX, redY;
    findRedDotCoordinates(fb, redX, redY);

    // Display the coordinates of the red dot on the web page
    String coordinates = "No red laser dot detected";
    if (redX != -1 && redY != -1) {
      coordinates = "Red laser dot detected at: (" + String(redX) + ", " + String(redY) + ")";
      Serial.println(coordinates);
    } else {
      Serial.println(coordinates);
    }

    // Send the coordinates to the client
    client.printf("--frame\r\n");
    client.printf("Content-Type: image/jpeg\r\n\r\n");
    client.write(fb->buf, fb->len);
    client.printf("\r\n");

    // Send coordinates as text (just below the camera feed)
    client.println("<br>");
    client.println("<h3>" + coordinates + "</h3>");
    client.println("<br>");

    esp_camera_fb_return(fb);

    // Break the loop if the client disconnects
    if (!client.connected()) break;
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

  // Start the web server
  server.begin();

  // Initialize the camera
  setupCamera();
}

void loop() {
  WiFiClient client = server.available(); // Listen for incoming clients

  if (client) { // If a new client connects
    Serial.println("New Client.");
    String currentLine = ""; // Make a String to hold incoming data from the client
    while (client.connected()) { // Loop while the client's connected
      if (client.available()) { // If there's bytes to read from the client
        char c = client.read(); // Read a byte
        Serial.write(c); // Print it to the serial monitor
        header += c;

        if (c == '\n') { // If the byte is a newline character
          if (currentLine.length() == 0) {
            // Handle `/stream` endpoint for camera feed
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
}
