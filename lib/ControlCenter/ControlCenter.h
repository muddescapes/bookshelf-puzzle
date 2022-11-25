#include <Arduino.h>
#include <mqtt_client.h>

namespace ControlCenter {
class Variable {
   private:
    const esp_mqtt_client_handle_t &client;
    String name;
    String options;
    const char *topic;
    String prev_status;

   public:
    Variable(const esp_mqtt_client_handle_t &client, const String name, const String options, const char *topic) : client(client), name(name), options(options), topic(topic){};

    void resend() const {
        String message = name + " {" + options + "} status=" + prev_status;

        // send variable updates with qos 2
        esp_mqtt_client_enqueue(client, topic, message.c_str(), message.length(), 2, 0, true);
    }

    void update(const String &status) {
        if (status == prev_status) {
            return;
        }

        prev_status = status;
        resend();
    }
};
}  // namespace ControlCenter