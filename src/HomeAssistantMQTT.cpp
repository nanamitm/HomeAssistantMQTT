#if defined(ESP8266) || defined(ESP32)

#include "HomeAssistantMQTT.h"
#define DEBUG
#define CFG_ON_SERIAL

HomeAssistantMQTT::HomeAssistantMQTT()
{
  for (int i = 0; i < HAMQTT_MAXITEMS; i++)
    values[i] = 0;
}

void HomeAssistantMQTT::setConfigCallback(ConfigCallbackFunction f)
{
  cb_config_callback = f;
}

void HomeAssistantMQTT::setCallback(CallbackFunction f)
{
  cb_callback = f;
}

void HomeAssistantMQTT::begin(WiFiClient* wifiClient, const char* server, const uint16_t port, bool useOffline)
{
  int ln = Manufacturer.length() + DeviceName.length() + 8;
  MqttStateTopic = new char[ln];
  strcpy(MqttStateTopic, Manufacturer.c_str());
  strcat(MqttStateTopic, "/");
  strcat(MqttStateTopic, DeviceName.c_str());
  strcat(MqttStateTopic, "/state");

#ifdef DEBUG
  Serial.print("MqttStateTopic: \"");
  Serial.print(MqttStateTopic);
  Serial.println("\"");
#endif

  mqttClient = new PubSubClient(*wifiClient);
  mqttClient->setBufferSize(1024);
  mqttClient->setServer(server, port);
  mqttClient->setCallback( [this](char* topic, byte * payload, unsigned int length) {
    this->MqttCallback(topic, payload, length);
  });
  mqttClient->setKeepAlive(5);

  connect( useOffline);

  if ( cb_config_callback) cb_config_callback();
}

void HomeAssistantMQTT::loop(bool useOffline)
{
  if (!mqttClient->connected())
    connect( useOffline);

  mqttClient->loop();
}

void HomeAssistantMQTT::connect(bool useOffline)
{
  while (!mqttClient->connected())
  {
#ifdef CFG_ON_SERIAL
    Serial.print("Attempting MQTT connection...");
#endif
    String mqttClientId = DeviceName;
    auto res = useOffline ? mqttClient->connect(mqttClientId.c_str(), MqttUser.c_str(), MqttPassword.c_str(), MqttStateTopic, 1, true, R"({"state":"offline"})")
               : mqttClient->connect(mqttClientId.c_str(), MqttUser.c_str(), MqttPassword.c_str());

    if (res)
    {
      mqttClient->publish(MqttStateTopic, R"({"state":"online"})", true);
#ifdef CFG_ON_SERIAL
      Serial.println("connected");
#endif
    }
    else
    {
#ifdef CFG_ON_SERIAL
      Serial.print("failed, rc=");
      Serial.print(mqttClient->state());
      Serial.println(" will try again in 5 seconds");
#endif
      delay(5000);
    }
  }
}

bool HomeAssistantMQTT::connected()
{
  return mqttClient->connected();
}

void HomeAssistantMQTT::publishConfigSensor(String deviceClass, String stateClass, String name, String icon, String unit, String startupValue)
{
  publishConfig("sensor", "", deviceClass, stateClass, name, icon, unit, false, true, "", "", startupValue);
}

void HomeAssistantMQTT::publishConfigBinarySensor(String deviceClass, String name, String icon, String payloadOff, String payloadOn, String startupValue)
{
  String complement = R"(,"payload_off":")" + payloadOff + R"(","payload_on":")" + payloadOn + "\"";
  publishConfig("binary_sensor", "", deviceClass, "", name, icon, "", false, true, "", complement, startupValue);
}

void HomeAssistantMQTT::publishConfigNumber(String category, String name, String icon, String unit, String min, String max, String startupValue, String mode)
{
  String complement = R"(,"min":)" + min + R"(,"max":)" + max + R"(,"mode":")" + mode + "\"";
  publishConfig("number", category, "", "", name, icon, unit, true, true, "", complement, startupValue);
}

void HomeAssistantMQTT::publishConfigText(String category, String name, String icon, String min, String max, String startupValue)
{
  String complement = R"(,"min":)" + min + R"(,"max":)" + max + "";
  publishConfig("text", category, "", "", name, icon, "", true, true, "", complement, startupValue);
}

void HomeAssistantMQTT::publishConfigButton(String category, String name, String icon, String commandTopicName, String payload)
{
  String complement = R"(,"payload_press":")" + payload + "\"";
  publishConfig("button", category, "", "", name, icon, "", true, false, commandTopicName, complement, "");
}

void HomeAssistantMQTT::publishConfigSelect(String category, String name, String icon, String options[], unsigned short optionsCount, String startupValue)
{
  String complement = R"(,"options":[")";
  for (int i = 0; i < optionsCount; i++)
  {
    complement += (i > 0 ? "\",\"" : "") + options[i];
  }
  complement += "\"]";
  publishConfig("select", category, "", "", name, icon, "", true, true, "", complement, startupValue);
}

void HomeAssistantMQTT::publishConfigSwitch(String category, String name, String icon, String startupValue)
{
  String nameForTopic = name;
  nameForTopic.replace(" ", "_");

  String complement = R"(,"payload_off":"false","payload_on":"true")";
  publishConfig("switch", category, "", "", name, icon, "", true, true, "", complement, startupValue);
}

void HomeAssistantMQTT::publishConfig(const char* type, String category, String deviceClass, String stateClass, String name, String icon, String unit, bool commandTopic, bool stateTopic, String commandTopicName, String complement, String startupValue)
{
  String nameForTopic = (name.length() > 0 ? name : deviceClass);
  nameForTopic.replace(" ", "_");
  String COMMAND_TOPIC = Manufacturer + "/" + DeviceName + "/set/" + (commandTopicName.length() > 0 ? commandTopicName : nameForTopic);

  StateTopic = new char[Manufacturer.length() + DeviceName.length() + 1];
  strcpy(StateTopic, Manufacturer.c_str());
  strcat(StateTopic, "/");
  strcat(StateTopic, DeviceName.c_str());

  String topic = "homeassistant/" + String(type) + "/" + DeviceName + "/" + nameForTopic + "/config";
  String data = R"({"availability":[{"topic":")" + Manufacturer + "/" + DeviceName + R"(/state","value_template":"{{ value_json.state }}"}])"
                + R"(,"device":{"identifiers":[")" + DeviceName + R"("],"manufacturer":")" + Manufacturer + R"(","model":")" + Model + R"(","name":")" + Name + R"(","sw_version":")" + Version + "\"}"
                + R"(,"enabled_by_default":true)"

                + (category.length() > 0 ? R"(,"entity_category":"config")" : "")

                + R"(,"unique_id":")" + DeviceName + "_" + nameForTopic + "\""
                + (name.length() > 0 ? R"(,"name":")" + name + "\"" : "")
                + (icon.length() > 0 ? R"(,"icon":")" + icon + "\"" : "")
                + (unit.length() > 0 ? R"(,"unit_of_measurement":")" + unit + "\"" : "")
                + (deviceClass.length() > 0 ? R"(,"device_class":")" + deviceClass + "\"" : "")
                + (stateClass.length() > 0 ? R"(,"state_class":")" + stateClass + "\"" : "")

                + complement

                + (commandTopic ? R"(,"command_topic":")" + COMMAND_TOPIC + "\"" : "")

                + (stateTopic ? R"(,"state_topic":")" + String(StateTopic) + "\"" : "")
                + (stateTopic ? R"(,"value_template":"{{ value_json.)" + nameForTopic + " }}\"" : "")

                + "}";

#ifdef CFG_ON_SERIAL
  Serial.print("  - ");
  Serial.println(topic.c_str());
  Serial.print("    ");
  Serial.println(data.c_str());
#endif

  mqttClient->publish(topic.c_str(), data.c_str(), true);

  if (commandTopic)
    mqttClient->subscribe(COMMAND_TOPIC.c_str());

  if (stateTopic)
    setValue(nameForTopic, startupValue);
}

void HomeAssistantMQTT::setValue(String item, String value)
{
  bool bFound = false;
  int i = 0;
  while (i < HAMQTT_MAXITEMS && !bFound)
  {
    if (values[i] != 0)
    {
      if (strcmp(values[i]->item, item.c_str()) == 0)
      {
        strcpy(values[i]->value, value.c_str());
        bFound = true;
      }
    }
    else
    {
      // if we reach an entry with pointer 0, that means we reached end of existing items and didn't find it. Now create a new one.
      ItemValue* iv = new ItemValue;
      iv->item = new char[31];
      iv->value = new char[31];
      strcpy(iv->item, item.c_str());
      strcpy(iv->value, value.c_str());
      values[i] = iv;
      bFound = true;
    }
    i++;
  }
}

String HomeAssistantMQTT::getValue(String item)
{
  int i = 0;
  while (i < HAMQTT_MAXITEMS)
  {
    if (values[i] != 0)
    {
      if (strcmp(values[i]->item, item.c_str()) == 0)
        return String(values[i]->value);
    }
  }
  return String("");
}

void HomeAssistantMQTT::readValues()
{
#ifdef DEBUG
  Serial.print("readValues: topic \"");
  Serial.print(StateTopic);
  Serial.println("\"");
#endif
  mqttClient->subscribe(StateTopic);
}

void HomeAssistantMQTT::sendValues()
{
  int ln = 0;
  int i = 0;
  while (i < HAMQTT_MAXITEMS)
  {
    if (values[i] != 0)
      ln += strlen(values[i]->item) + strlen(values[i]->value) + 6;

    i++;
  }
  ln++;

  String str;
  str = str + "{";
  i = 0;
  while (i < HAMQTT_MAXITEMS)
  {
    if (values[i] != 0)
    {
      str = str + "\"" + values[i]->item + "\":\"" + values[i]->value + "\",";
    }
    i++;
  }
  str[ln - 1] = '}';
  char c[ln];
  str.toCharArray(c, ln + 1);

  mqttClient->publish(StateTopic, c, true);
}

void HomeAssistantMQTT::MqttCallback(char* topic, byte* payload, unsigned int length)
{
#ifdef DEBUG
  Serial.print("Message arrived on topic: '");
  Serial.print(topic);
  Serial.print("' with payload: ");
#endif

  char cPayload[length + 1];

  memcpy( cPayload, payload, length);
  cPayload[length] = '\0';

#ifdef DEBUG
  Serial.print(cPayload);
  Serial.println();
#endif

  if (strcmp(StateTopic, topic) == 0)
  {
    // Received message on device state topic, read the values and unsubscribe from the topic
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, cPayload);
    if (error)
    {
#ifdef DEBUG
      Serial.print("Error reading old values, deserializeJson() returned: ");
      Serial.println(error.c_str());
#endif
    }
    else
    {
      for (JsonPair kv : doc.as<JsonObject>())
      {
#ifdef DEBUG
        Serial.print("Key: \"");
        Serial.print(kv.key().c_str());
        Serial.print("\", Value: \"");
        Serial.print(kv.value().as<const char*>());
        Serial.println("\"");
#endif
        setValue(String(kv.key().c_str()), String(kv.value().as<const char*>()));
        if (cb_callback != NULL)
          cb_callback(String(kv.key().c_str()), String(kv.value().as<const char*>()), true);
      }
      sendValues();
    }

    mqttClient->unsubscribe(StateTopic);
  }
  else
  {
    String COMMAND_TOPIC = Manufacturer + "/" + DeviceName + "/set/";
    if (strncmp(topic, COMMAND_TOPIC.c_str(), COMMAND_TOPIC.length()) == 0)
    {
      char buffer[strlen(topic + COMMAND_TOPIC.length())];
      strcpy(buffer, topic + COMMAND_TOPIC.length());
      if (cb_callback != NULL)
        cb_callback(String(buffer), String(cPayload), false);
    }
  }
}

#endif //#if defined(ESP8266) || defined(ESP32)
