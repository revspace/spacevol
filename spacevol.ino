#include <WiFi.h>
#include <SPIFFS.h>
#include <WiFiSettings.h>
#include <MQTT.h>
#include <esp_task_wdt.h>

#define Sprintf(f, ...) ({ char* s; asprintf(&s, f, __VA_ARGS__); String r = s; free(s); r; })

const int buttonpin = 0;
const int lamppin = 17;

WiFiClient wificlient;
MQTTClient mqtt;

String  current_topic;
String  max_n_topic;
int     max_n, n;
bool    blink;
int     blink_ontime, blink_offtime;

void setup() {
    Serial.begin(115200);
    SPIFFS.begin(true);

    pinMode(buttonpin, INPUT);
    pinMode(lamppin, OUTPUT);

    esp_task_wdt_init(60 /* seconds */, true);
    esp_task_wdt_add(NULL);

    String server = WiFiSettings.string(  "mqtt_server", 64, "10.42.42.1",
        "MQTT Server hostname");
    int port      = WiFiSettings.integer( "mqtt_port", 0, 65535, 1883,
        "MQTT Server port");
    current_topic = WiFiSettings.string(  "sv_mqtt_topic", "revspace/doorduino/checked-in",
        "MQTT topic for current value");
    max_n_topic   = WiFiSettings.string(  "sv_mqtt_topic_max", "revspace/n_max",
        "MQTT topic for threshold");
    max_n         = WiFiSettings.integer( "sv_max_n", 10,
        "Startup threshold");
    blink         = WiFiSettings.checkbox("sv_blink", true,
        "Blink instead of steady");
    blink_ontime  = WiFiSettings.integer( "sv_blink_ontime", 3000,
        "Blink on-time in ms");
    blink_offtime = WiFiSettings.integer( "sv_blink_offtime", 1000,
        "Blink off-time in ms");

    for (int i = 0; i < 1000; i++) {
        if (!digitalRead(buttonpin)) WiFiSettings.portal();
        delay(1);
    }
    int attempt = 0;
    while (!WiFiSettings.connect(false)) {
        if (attempt++ > 3) ESP.restart();
        Serial.printf("WiFi connection attempt %d\n", attempt);
        delay(1000);
    }

    mqtt.begin(server.c_str(), port, wificlient);
    mqtt.onMessage([](String &topic, String &payload) {
        int i = payload.toInt();

        if (topic == current_topic) {
            n = i;
        }
        if (topic == max_n_topic) {
            max_n = i;
        }
        Serial.printf("%d/%d\n", n, max_n);
    });
}


void loop() {
    int attempt = 0;
    while (!mqtt.connected()) {
        Serial.println("Connecting to MQTT");
        if (mqtt.connect("")) {
            Serial.printf("Subscribing to %s\n", current_topic.c_str());
            mqtt.subscribe(current_topic);
            Serial.printf("Subscribing to %s\n", max_n_topic.c_str());
            mqtt.subscribe(max_n_topic);
            break;
        }
        if (attempt++ > 60) ESP.restart();
        delay(500);
    }
    mqtt.loop();

    static bool oldstate = false;
    bool newstate =
          n < max_n ? false
        : blink     ? millis() % (blink_ontime + blink_offtime) < blink_ontime
        :             true;

    digitalWrite(lamppin, newstate);
    if (oldstate != newstate) {
        Serial.println(newstate ? "on" : "off");
        oldstate = newstate;
    }
    esp_task_wdt_reset();

    delay(10);
}
