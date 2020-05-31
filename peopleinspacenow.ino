#include <Arduino.h>
#include <HTTPClient.h>
#include <FastLED.h>
#include <ArduinoJson.h>

#define PEOPLE_TIMEOUT_MS   5000
#define DATA_PIN_LED 27

static CRGB leds[25];
static WiFiClient wifiClient;

static bool fetch_people(String url, String & response)
{
    HTTPClient httpClient;
    httpClient.begin(wifiClient, url);
    httpClient.setTimeout(PEOPLE_TIMEOUT_MS);

    // retry GET a few times until we get a valid HTTP code
    int res = 0;
    for (int i = 0; i < 3; i++) {
        printf("> GET %s\n", url.c_str());
        res = httpClient.GET();
        if (res > 0) {
            break;
        }
    }

    // evaluate result
    bool result = (res == HTTP_CODE_OK);
    response = result ? httpClient.getString() : httpClient.errorToString(res);
    httpClient.end();
    printf("< %d: %s\n", res, response.c_str());
    return result;
}

static bool parse_people(String json, int &number)
{
    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, json) != DeserializationError::Ok) {
        return false;
    }

    JsonObject root = doc.as < JsonObject > ();
    number = root["number"];
    JsonArray people = root["people"];
  for (JsonObject person:people) {
        String name = person["name"];
        String craft = person["craft"];
        Serial.printf("%s in %s\n", name.c_str(), craft.c_str());
    }
    return true;
}


void setup(void)
{
    FastLED.addLeds < WS2812B, DATA_PIN_LED, GRB > (leds, 25);
    FastLED.setBrightness(20);

    Serial.begin(115200);
    Serial.println("\nHow many people are in space now?");

    WiFi.begin("revspace-pub-2.4ghz");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
}

void loop(void)
{
    static int period_prev = -1;
    int period = millis() / 600000;
    if (period != period_prev) {
        period_prev = period;

        String response;
        if (fetch_people("http://api.open-notify.org/astros.json", response)) {
            int number;
            if (parse_people(response, number)) {
                Serial.printf("%d people in space!\n", number);
            } else {
                Serial.printf("Error decoding JSON!\n");
            }
        } else {
            Serial.printf("Error performing HTTP GET!\n");
        }
    }

    FastLED.show();
}
