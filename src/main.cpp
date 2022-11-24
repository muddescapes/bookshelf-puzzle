#include <Arduino.h>
#include <ESPHelper.h>
#include <ControlCenter.h>
#include <mqtt_client.h>

// Connect to Claremont-ETC wifi
constexpr const char *WIFI_NAME = "Claremont-ETC";
constexpr const char *WIFI_PASSWORD = "Cl@remontI0T";

// Connect to MQTT
constexpr const char *MQTT_BROKER = "mqtt://broker.hivemq.com";
constexpr const char *MQTT_TOPIC = "muddescapes-esp/test";

// Declare variables: 
// These will be set by input voltage to ESP:
bool completed_puzzle = false;
bool electromagnet_monitor = HIGH;

// This is an internal variable for me to keep track of what should be happening
bool bookshelf_locked = true;
bool FPGA_status = HIGH;

// BEGIN TEMPLATE CODE (DO NOT CHANGE)

typedef void (*mqtt_callback_t)(esp_mqtt_event_handle_t);

void mqtt_cb_connected(esp_mqtt_event_handle_t event) {
    Serial.println("connected to MQTT broker");
    esp_mqtt_client_subscribe(event->client, MQTT_TOPIC, 0);
}

static void mqtt_cb_subscribed(esp_mqtt_event_handle_t event) {
    Serial.printf("subscribed to topic %.*s\n", event->topic_len, event->topic);
}

static void mqtt_cb_published(esp_mqtt_event_handle_t event) {
    ESP_LOGI(TAG, "published message with id %d", event->msg_id);
}

static void mqtt_cb_message(esp_mqtt_event_handle_t event) {
    if (event->data_len != event->total_data_len) {
        Serial.println("ignoring incomplete message (too long)");
        return;
    }
    // Serial.printf("message arrived on topic %.*s: %.*s\n", event->topic_len, event->topic, event->data_len, event->data);
    
    // END TEMPLATE CODE

    // Here, we "listen" for certain messages and can act upon them. 
    // Example below: We listen for the string "test". If we hear it, we print to serial monitor "test message received" (internal, local)
    // and we publish the same message to MQTT (external, public)

    if (strncmp(event->data, "test", event->data_len) == 0) {
        const String resp("test message received");
        Serial.printf(resp.c_str());
        esp_mqtt_client_enqueue(event->client, MQTT_TOPIC, resp.c_str(), resp.length(), 0, 0, true);
    }

    // Here, if we receive this specific string that will be outputted from control-center, it overrides my local variables turn_on_led

    if (strncmp(event->data, "set bookshelf_locked {locked, unlocked} status=unlocked", event->data_len) == 0) {
        Serial.println("Received command: unlock bookshelf");
        bookshelf_locked = false;
    }

    if (strncmp(event->data, "set bookshelf_locked {locked, unlocked} status=locked", event->data_len) == 0) {
        Serial.println("Received command: lock bookshelf");
        bookshelf_locked = true;
    }

    if (strncmp(event->data, "set completed_puzzle {yes, no} status=yes", event->data_len) == 0) {
        Serial.println("Received command: manually complete bookshelf puzzle");
        completed_puzzle = true;
    }

    if (strncmp(event->data, "set completed_puzzle {yes, no} status=no", event->data_len) == 0) {
        Serial.println("Received command: reset bookshelf puzzle");
        completed_puzzle = false;
    }

    if (strncmp(event->data, "set FPGA {HIGH, LOW} status=HIGH", event->data_len) == 0) {
        Serial.println("Received command: make FPGA high");
        digitalWrite(33, HIGH);
        FPGA_status = HIGH;
    }

    if (strncmp(event->data, "set FPGA {HIGH, LOW} status=LOW", event->data_len) == 0) {
        Serial.println("Received command: Reset FPGA");
        digitalWrite(33, LOW);
        delay(100);
        digitalWrite(33, HIGH);
        FPGA_status = HIGH;
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    // https://github.com/jkhsjdhjs/esp32-mqtt-door-opener/blob/eee9e60e4f3a623913d470b4c7cbbc844300561d/main/src/mqtt.c
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
    reinterpret_cast<mqtt_callback_t>(handler_args)(event);
}

unsigned int msg_time = 0;
esp_mqtt_client_handle_t client = NULL;
ControlCenter::Variable var_completed_puzzle(client, "completed_puzzle", "yes, no", MQTT_TOPIC);
ControlCenter::Variable var_electromagnet_monitor(client, "electromagnet_monitor", "HIGH, LOW", MQTT_TOPIC);
ControlCenter::Variable var_bookshelf_locked(client, "bookshelf_locked", "locked, unlocked", MQTT_TOPIC);
ControlCenter::Variable var_fpga(client, "FPGA", "HIGH, LOW", MQTT_TOPIC);

void setup() {
    Serial.begin(115200);

    // CUSTOM CODE FOR YOUR PUZZLE BEGIN: 

    // For Arduinos, we have to specify which pins are inputs and outputs. Be sure to check the ESP-32 Huzzah Pinout to see which pins can be I/O and which are digital/analog

    // insert inputs to actual puzzle here    
    pinMode(27, INPUT); //puzzle completer
    pinMode(21, OUTPUT); // electromagnet unlocker/locker
    pinMode(14, INPUT); // electromagnet monitor
    pinMode(33, OUTPUT); // reset Cristian's FPGA

    // CUSTOM CODE END

    setup_wifi(WIFI_NAME, WIFI_PASSWORD);

    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mqtt.html
    esp_mqtt_client_config_t mqtt_cfg = {0};
    mqtt_cfg.uri = MQTT_BROKER;

    client = esp_mqtt_client_init(&mqtt_cfg);
    assert(client);

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, MQTT_EVENT_CONNECTED, mqtt_event_handler, reinterpret_cast<void *>(mqtt_cb_connected)));
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, MQTT_EVENT_SUBSCRIBED, mqtt_event_handler, reinterpret_cast<void *>(mqtt_cb_subscribed)));
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, MQTT_EVENT_PUBLISHED, mqtt_event_handler, reinterpret_cast<void *>(mqtt_cb_published)));
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, MQTT_EVENT_DATA, mqtt_event_handler, reinterpret_cast<void *>(mqtt_cb_message)));

    ESP_ERROR_CHECK(esp_mqtt_client_start(client));

    msg_time = millis();

    digitalWrite(33, LOW);
    digitalWrite(33, HIGH);
}

void loop() {
    
    // CUSTOM CODE FOR YOUR PUZZLE BEGIN: 
    // This is simply Arduino code. You can look up lots of tutorials for this stuff! 

    electromagnet_monitor = digitalRead(14);

    if (digitalRead(27)) {
        completed_puzzle = true;
    }

    if (!bookshelf_locked) {
        digitalWrite(21, LOW);
    }
    else {
        digitalWrite(21, HIGH);

        if (completed_puzzle) {
            bookshelf_locked = false;
        }
        else {
            bookshelf_locked = true;
        }
    }

    // Serial print for debugging

    // String s = "digitalRead(14) ";
    // s = s + digitalRead(14);
    // s = s + "      27 ";
    // s = s + digitalRead(27);
    // Serial.println(s);

    // Creation of message for MQTT. The format of an MQTT message must be: 
    // "variable-name {option1, option2} status=currentstatus"
    // in order for it to show up correctly on control center
    var_completed_puzzle.update(completed_puzzle ? "yes" : "no");
    var_electromagnet_monitor.update(electromagnet_monitor ? "HIGH" : "LOW");
    var_bookshelf_locked.update(bookshelf_locked ? "locked" : "unlocked");
    var_fpga.update(FPGA_status ? "HIGH" : "LOW");

    if (millis() - msg_time > 1000 && client) { //change the value here to change how often it publishes
        msg_time = millis();
        Serial.println("publishing heartbeat");

        // Sending our created MQTT strings to the internet. Copy the line below for as many messages as you have. 
        String message("alive {yes} status=yes");
        esp_mqtt_client_enqueue(client, MQTT_TOPIC, message.c_str(), message.length(), 0, 0, true);
    }

    // CUSTOM CODE END
}