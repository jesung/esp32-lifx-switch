/* 
  ESP32 x LIFX PIR sensor by Jesung Park
  Modified from https://github.com/lgruen/lifx-switch-esp32 to be compilable on Arduino IDE 1.8.19
  Using ESP32-WROOM-32D and PIR sensor
  Tested on Mar 30, 2022
*/

#include <WiFi.h>
#include "bulb.h"
#include "color.h"
#include "lifx.h"
#include "lifx.c"

#define pirIn 21
#define pirOut 23

// Replace with your network credentials (STATION)
const char* ssid = "ssid";
const char* password = "password";

// declare variables for reconnecting wifi
unsigned long previousMillis = 0;
unsigned long interval = 30000;

// variables for light
static const char* TAG = "Light switch";
bool power = true;
int val = 0;
static const char* CONFIG_ESP_LIFX_MAC = "D0:73:D5:50:EF:20";

static bool lifx_initialized = false;
static bulb_service_t** bulbs;
static int bulb_index = -1;


void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

static void print_bulb(bulb_service_t* bulb) {
  ESP_LOGI(TAG,
           "Bulb\n"
           "    target: 0x%llx\n"
           "    service: %d\n"
           "    port: %d\n",
           bulb->target, bulb->service, bulb->port);
  Serial.println("Bulb");
  Serial.print("target: ");
  Serial.println(bulb->target);
  Serial.print("service: ");
  Serial.println(bulb->service);
  Serial.print("port: ");
  Serial.println(bulb->port);
}

// Converts a MAC address string like "D0:73:D5:50:EF:20"
// to the corresponding integer (e.g. 0x20ef50d573d0).
static uint64_t mac_string_to_uint64(const char* mac_str) {
  unsigned char mac[6];
  if (sscanf(mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[5], &mac[4],
             &mac[3], &mac[2], &mac[1], &mac[0]) != 6) {
    ESP_LOGE(TAG, "Couldn't parse MAC address");
    abort();
  }
  uint64_t result = 0;
  for (int i = 0; i < 6; ++i) {
    result <<= 8;
    result |= (uint64_t)mac[i];
  }
  return result;
}

static bool init_lifx() {
  ESP_LOGI(TAG, "Initializing LIFX library");
  int err = init_lifx_lib();
  if (err != 0) {
    ESP_LOGE(TAG, "init_lifx_lib error: %d", err);
    return false;
  }
  err = discoverBulbs(&bulbs);
  if (err != 0) {
    ESP_LOGE(TAG, "discoverBulbs error: %d", err);
    return false;
  }

  const uint64_t bulb_mac = mac_string_to_uint64(CONFIG_ESP_LIFX_MAC);
  ESP_LOGI(TAG, "Looking for bulb with MAC address %s (0x%llx)",
           CONFIG_ESP_LIFX_MAC, bulb_mac);
  for (int i = 0; bulbs[i] != NULL; ++i) {
    print_bulb(bulbs[i]);
    if (bulbs[i]->target == bulb_mac) {
      bulb_index = i;
      ESP_LOGI(TAG, "Bulb found at index %d", i);
      break;
    }
  }

  if (bulb_index == -1) {
    ESP_LOGE(TAG, "Couldn't find bulb");
    return false;
  }

  return true;
}

static void toggle_light() {
  bool on;
  int err = getPower(bulbs[bulb_index], &on);
  if (err != 0) {
    ESP_LOGE(TAG, "getPower error: %d", err);
    return;
  }
  ESP_LOGI(TAG, "Current state: %s", on ? "on" : "off");

  err = setPower(bulbs[bulb_index], !on, 500);
  if (err != 0) {
    ESP_LOGE(TAG, "setPower error: %d", err);
    return;
  }
}

static void set_light(bool *power) {
  // set light to opposite of power and update power accordingly
  *power = !*power;
  int err = setPower(bulbs[bulb_index], *power, 500);
  if (err != 0) {
    ESP_LOGE(TAG, "setPower error: %d", err);
    return;
  }
}

void setup() {
  Serial.begin(115200);
  initWiFi();
  Serial.print("RRSI: ");
  Serial.println(WiFi.RSSI());

  if (init_lifx()) {
    lifx_initialized = true;
    Serial.print("LIFX initialized");
    print_bulb(bulbs[bulb_index]);
  }
  else {
    Serial.print("LIFX failed to initialize");
  }

  pinMode(pirIn, INPUT_PULLUP);
  pinMode(pirOut, OUTPUT);
  digitalWrite(pirOut, HIGH);

  Serial.println("Testing light... turning on and off");
  delay(100);
  set_light(&power);
  delay(3000);
  set_light(&power);
  delay(1000);
}

void loop() {
  unsigned long currentMillis = millis();
  
  // if WiFi is down, try reconnecting
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= interval)) {
    Serial.print(millis());
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }
  val = digitalRead(pirIn);
    
  if (val == LOW && !power) {
    // val is flipped because pin is on PULLUP mode
    Serial.println("turning on light");
    set_light(&power);
  }
  else if (val == HIGH && power) {
    // PIR sensor will switch to off signal after 35 seconds
    Serial.println("turning off light");
    set_light(&power);
  }
  
  delay(100);
}
