#include <Arduino.h>
#include <HTTPClient.h>
#include <FastLED.h>
#include <ArduinoJson.h>

#define HTTP_TIMEOUT_MS   5000
#define PIN_LED     27
#define PIN_BUTTON  39

#define ISS_ID 25544

static const char digits[10][5] {
    {0b1110, 0b1010, 0b1010, 0b1010, 0b1110}, // 0
    {0b0100, 0b1100, 0b0100, 0b0100, 0b1110}, // 1
    {0b1110, 0b0010, 0b1110, 0b1000, 0b1110}, // 2
    {0b1110, 0b0010, 0b0110, 0b0010, 0b1110}, // 3
    {0b1000, 0b1010, 0b1110, 0b0010, 0b0010}, // 4
    {0b1110, 0b1000, 0b1110, 0b0010, 0b1110}, // 5
    {0b0110, 0b1000, 0b1110, 0b1010, 0b1110}, // 6
    {0b1110, 0b0010, 0b0100, 0b0100, 0b0100}, // 7
    {0b1110, 0b1010, 0b1110, 0b1010, 0b1110}, // 8
    {0b1110, 0b1010, 0b1110, 0b0010, 0b1100}, // 9
};

static const char *root_ca = "-----BEGIN CERTIFICATE-----\n" \
"MIIDSjCCAjKgAwIBAgIQRK+wgNajJ7qJMDmGLvhAazANBgkqhkiG9w0BAQUFADA/\n" \
"MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\n" \
"DkRTVCBSb290IENBIFgzMB4XDTAwMDkzMDIxMTIxOVoXDTIxMDkzMDE0MDExNVow\n" \
"PzEkMCIGA1UEChMbRGlnaXRhbCBTaWduYXR1cmUgVHJ1c3QgQ28uMRcwFQYDVQQD\n" \
"Ew5EU1QgUm9vdCBDQSBYMzCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB\n" \
"AN+v6ZdQCINXtMxiZfaQguzH0yxrMMpb7NnDfcdAwRgUi+DoM3ZJKuM/IUmTrE4O\n" \
"rz5Iy2Xu/NMhD2XSKtkyj4zl93ewEnu1lcCJo6m67XMuegwGMoOifooUMM0RoOEq\n" \
"OLl5CjH9UL2AZd+3UWODyOKIYepLYYHsUmu5ouJLGiifSKOeDNoJjj4XLh7dIN9b\n" \
"xiqKqy69cK3FCxolkHRyxXtqqzTWMIn/5WgTe1QLyNau7Fqckh49ZLOMxt+/yUFw\n" \
"7BZy1SbsOFU5Q9D8/RhcQPGX69Wam40dutolucbY38EVAjqr2m7xPi71XAicPNaD\n" \
"aeQQmxkqtilX4+U9m5/wAl0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNV\n" \
"HQ8BAf8EBAMCAQYwHQYDVR0OBBYEFMSnsaR7LHH62+FLkHX/xBVghYkQMA0GCSqG\n" \
"SIb3DQEBBQUAA4IBAQCjGiybFwBcqR7uKGY3Or+Dxz9LwwmglSBd49lZRNI+DT69\n" \
"ikugdB/OEIKcdBodfpga3csTS7MgROSR6cz8faXbauX+5v3gTt23ADq1cEmv8uXr\n" \
"AvHRAosZy5Q6XkjEGB5YGV8eAlrwDPGxrancWYaLbumR9YbK+rlmM6pZW87ipxZz\n" \
"R8srzJmwN0jP41ZL9c8PDHIyh8bwRLtTcm1D9SZImlJnt1ir/md2cXjbDaJWFBM5\n" \
"JDGFoqgCWjBH4d1QB7wCCZAA62RjYJsWvIjJEubSfZGL+T0yjWW06XyxV3bqxbYo\n" \
"Ob8VZRzI9neWagqNdwvYkQsEjgfbKbYK7p2CNTUQ\n" \
"-----END CERTIFICATE-----\n";

static CRGB leds[25];
static WiFiClient wifiClient;
static WiFiClientSecure wifiClientSecure;

static bool fetch_people(String & response)
{
    String url = "http://api.open-notify.org/astros.json";

    HTTPClient httpClient;
    httpClient.begin(wifiClient, url);
    httpClient.setTimeout(HTTP_TIMEOUT_MS);

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

// origin is top-left
static void draw_pixel(int x, int y, CRGB c)
{
    if ((x >= 0) && (x < 5) && (y >= 0) && (y < 5)) {
        leds[y * 5 + x] = c;
    }
}

static void draw_number(int number)
{
    if (number > 9) {
        number = 9;
    }
    const char *p = digits[number];
    for (int y = 0; y < 5; y++) {
        int d = *p;
        for (int x = 0; x < 5; x++) {
            if ((d & (0b10000 >> x)) != 0) {
                draw_pixel(x, y, CRGB::White);
            }
        }
        p++;
    }
}

static bool fetch_sat(int id, String &response)
{
    HTTPClient httpClient;

    String url = "https://api.wheretheiss.at/v1/satellites/";
    url += id;

    httpClient.begin(wifiClientSecure, url);
    httpClient.setTimeout(HTTP_TIMEOUT_MS);
    printf("> GET %s\n", url.c_str());
    int res = httpClient.GET();

    // evaluate result
    bool result = (res == HTTP_CODE_OK);
    response = result ? httpClient.getString() : httpClient.errorToString(res);
    httpClient.end();
    printf("< %d: %s\n", res, response.c_str());

    return result;
}

static bool parse_sat(String json, String &name, float &lat, float &lon, float &alt)
{
    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, json) != DeserializationError::Ok) {
        return false;
    }

    JsonObject root = doc.as < JsonObject > ();
    name = root["name"].as<String>();
    lat = root["latitude"];
    lon = root["longitude"];
    alt = root["altitude"];

    return true;
}

static void draw_sat(float lat, float lon)
{
    // origin is middle, x goes right, y goes up
    int x = 2 + round(sin(lon * M_PI / 180) * 2.0);
    int y = 2 + round(sin(lat * M_PI / 180) * 2.0);

    printf("x=%d, y=%d\n", x, y);
    CRGB c = ((lon > -90) && (lon < 90)) ? CRGB::Gray : CRGB::DarkGrey;
    draw_pixel(x, 4 - y, c);
}

static void draw_earth(void)
{
    memset(leds, 0, sizeof(leds));

    int y = 0;
    draw_pixel(1, y, CRGB::DarkBlue);
    draw_pixel(2, y, CRGB::DarkBlue);
    draw_pixel(3, y, CRGB::DarkBlue);

    y++;
    draw_pixel(0, y, CRGB::DarkBlue);
    draw_pixel(1, y, CRGB::Blue);
    draw_pixel(2, y, CRGB::Blue);
    draw_pixel(3, y, CRGB::Blue);
    draw_pixel(4, y, CRGB::DarkBlue);

    y++;
    draw_pixel(0, y, CRGB::DarkBlue);
    draw_pixel(1, y, CRGB::Blue);
    draw_pixel(2, y, CRGB::Blue);
    draw_pixel(3, y, CRGB::Blue);
    draw_pixel(4, y, CRGB::DarkBlue);

    y++;
    draw_pixel(0, y, CRGB::DarkBlue);
    draw_pixel(1, y, CRGB::Blue);
    draw_pixel(2, y, CRGB::Blue);
    draw_pixel(3, y, CRGB::Blue);
    draw_pixel(4, y, CRGB::DarkBlue);

    y++;
    draw_pixel(1, y, CRGB::DarkBlue);
    draw_pixel(2, y, CRGB::DarkBlue);
    draw_pixel(3, y, CRGB::DarkBlue);
}

void setup(void)
{
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    FastLED.addLeds < WS2812B, PIN_LED, GRB > (leds, 25);
    FastLED.setBrightness(20);

    Serial.begin(115200);
    Serial.println("\nHow many people are in space now?");

    WiFi.begin("revspace-pub-2.4ghz");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    wifiClientSecure.setCACert(root_ca);
}

void loop(void)
{
    // update satellite position every minute
    static int period_prev = -1;
    int period = millis() / 60000;
    if (period != period_prev) {
        period_prev = period;

        // get ISS position
        String response;
        if (fetch_sat(ISS_ID, response)) {
            String name;
            float lat, lon, alt;
            if (parse_sat(response, name, lat, lon, alt)) {
                printf("%s is at %f, %f, %f\n", name.c_str(), lat, lon, alt);
                draw_earth();
                draw_sat(lat, lon);
            } else {
                Serial.printf("Error decoding JSON!\n");
            }
        } else {
            Serial.printf("Error performing satellite HTTP GET!\n");
        }
    }

    // get number of people in space if button is pressed
    if (digitalRead(PIN_BUTTON) == LOW) {
        FastLED.clear();

        // get people in space
        String response;
        if (fetch_people(response)) {
            int number;
            if (parse_people(response, number)) {
                Serial.printf("%d people in space!\n", number);
                draw_number(number);
            } else {
                Serial.printf("Error decoding JSON!\n");
            }
        } else {
            Serial.printf("Error performing people-in-space HTTP GET!\n");
        }
    }

    FastLED.show();
}
