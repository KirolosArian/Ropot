/* CloudRover V1.0 - ESP32-CAM (AI-Thinker, OV2640) live MJPEG stream
 * Board: "AI Thinker ESP32-CAM" | Partition: Huge APP | Upload with ESP32-CAM-MB shield
 * Open  http://<IP shown in Serial Monitor>/        (page with video)
 *       http://<IP>:81/stream                        (raw MJPEG stream)
 */
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

const char *WIFI_SSID = "YOUR_WIFI_NAME";      // <-- EDIT
const char *WIFI_PASS = "YOUR_WIFI_PASSWORD";  // <-- EDIT

// AI-Thinker pin map
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22
#define FLASH_LED 4

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *STREAM_CT = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t web_httpd = NULL, stream_httpd = NULL;

static esp_err_t index_handler(httpd_req_t *req) {
  const char *html = "<html><body style='background:#111;color:#0ff;text-align:center;font-family:sans-serif'>"
                     "<h2>CloudRover Live</h2><img id='v' style='width:96%;max-width:640px;border:2px solid #0ff'>"
                     "<script>document.getElementById('v').src='http://'+location.hostname+':81/stream';</script>"
                     "</body></html>";
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, html, strlen(html));
}

static esp_err_t stream_handler(httpd_req_t *req) {
  esp_err_t res = httpd_resp_set_type(req, STREAM_CT);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  char part[64];
  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) { res = ESP_FAIL; break; }
    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res == ESP_OK) {
      size_t n = snprintf(part, sizeof(part), STREAM_PART, fb->len);
      res = httpd_resp_send_chunk(req, part, n);
    }
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    if (res != ESP_OK) break;                 // client disconnected
  }
  return res;
}

void startServers() {
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  httpd_uri_t idx = {"/", HTTP_GET, index_handler, NULL};
  httpd_uri_t str = {"/stream", HTTP_GET, stream_handler, NULL};
  cfg.server_port = 80;
  if (httpd_start(&web_httpd, &cfg) == ESP_OK) httpd_register_uri_handler(web_httpd, &idx);
  cfg.server_port = 81; cfg.ctrl_port += 1;
  if (httpd_start(&stream_httpd, &cfg) == ESP_OK) httpd_register_uri_handler(stream_httpd, &str);
}

void setup() {
  Serial.begin(115200);
  pinMode(FLASH_LED, OUTPUT); digitalWrite(FLASH_LED, LOW);
  camera_config_t c;
  c.ledc_channel = LEDC_CHANNEL_0; c.ledc_timer = LEDC_TIMER_0;
  c.pin_d0 = Y2_GPIO_NUM; c.pin_d1 = Y3_GPIO_NUM; c.pin_d2 = Y4_GPIO_NUM; c.pin_d3 = Y5_GPIO_NUM;
  c.pin_d4 = Y6_GPIO_NUM; c.pin_d5 = Y7_GPIO_NUM; c.pin_d6 = Y8_GPIO_NUM; c.pin_d7 = Y9_GPIO_NUM;
  c.pin_xclk = XCLK_GPIO_NUM; c.pin_pclk = PCLK_GPIO_NUM; c.pin_vsync = VSYNC_GPIO_NUM;
  c.pin_href = HREF_GPIO_NUM; c.pin_sccb_sda = SIOD_GPIO_NUM; c.pin_sccb_scl = SIOC_GPIO_NUM;
  c.pin_pwdn = PWDN_GPIO_NUM; c.pin_reset = RESET_GPIO_NUM;
  c.xclk_freq_hz = 20000000; c.pixel_format = PIXFORMAT_JPEG;
  c.frame_size = FRAMESIZE_VGA;               // 640x480; use FRAMESIZE_QVGA if slow
  c.jpeg_quality = 12; c.fb_count = 2;
  c.fb_location = CAMERA_FB_IN_PSRAM; c.grab_mode = CAMERA_GRAB_LATEST;
  if (esp_camera_init(&c) != ESP_OK) { Serial.println("Camera init FAILED - check ribbon cable"); delay(3000); ESP.restart(); }
  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 0); s->set_hmirror(s, 0);   // set 1 if image is upside down / mirrored

  WiFi.mode(WIFI_STA); WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300); Serial.print(".");
    if (millis() - t > 20000) { Serial.println("\nWiFi failed, restarting"); ESP.restart(); }
  }
  startServers();
  Serial.print("\nCloudRover camera ready: http://"); Serial.println(WiFi.localIP());
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) { WiFi.reconnect(); delay(5000); }   // keep link alive
  delay(1000);
}
