#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SD.h>
#include <sd_diskio.h>
#include <ff.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_sntp.h>
#include <esp_system.h>
#include <sys/time.h>
#include <time.h>
#include <algorithm>
#include <math.h>
#include <stdlib.h>
#include <vector>

namespace {

// Najważniejsze ustawienia sprzętu, sieci, podświetlenia, kursu i ekranu.
constexpr uint8_t BacklightPin = 21;
constexpr uint8_t StockLedRedPin = 4;
constexpr uint8_t StockLedGreenPin = 16;
constexpr uint8_t StockLedBluePin = 17;
// Czujnik światła typowej płytki ESP32-2432S028 jest podłączony do GPIO34.
constexpr uint8_t AmbientLightPin = 34;
constexpr uint8_t SdCardChipSelectPin = 5;
constexpr uint8_t SdCardClockPin = 18;
constexpr uint8_t SdCardMisoPin = 19;
constexpr uint8_t SdCardMosiPin = 23;
constexpr uint32_t SdCardSpiFrequency = 4000000;
constexpr uint8_t TouchClockPin = 25;
constexpr uint8_t TouchMisoPin = 39;
constexpr uint8_t TouchMosiPin = 32;
constexpr uint8_t TouchChipSelectPin = 33;
constexpr uint8_t BacklightPwmChannel = 0;
constexpr uint8_t StockLedRedPwmChannel = 1;
constexpr uint8_t StockLedGreenPwmChannel = 2;
constexpr uint8_t StockLedBluePwmChannel = 3;
constexpr uint16_t BacklightPwmFrequency = 5000;
constexpr uint8_t BacklightPwmResolution = 8;
constexpr uint16_t StockLedPwmFrequency = 5000;
constexpr bool StockLedActiveLow = true;
// Wartość 3/255 daje około 1% wypełnienia PWM.
constexpr uint8_t MinimumBacklight = 3;
constexpr uint8_t MaximumBacklight = 255;
// Płytka zwraca około 1117 w ciemności; jaśniejsze światło obniża odczyt.
constexpr int AmbientLightDarkRaw = 1117;
constexpr int AmbientLightBrightRawMinimum = 50;
constexpr int AmbientLightBrightRawMaximum = 1000;
constexpr uint8_t DefaultAutoBacklightMinimum = 3;
constexpr uint8_t DefaultAutoBacklightMaximum = 255;
constexpr uint16_t ScreenWidth = 320;
constexpr uint16_t ScreenHeight = 240;
constexpr size_t MaxSavedNetworks = 12;
constexpr size_t NetworksPerPage = 5;
constexpr uint32_t ConnectTimeoutMs = 20000;
constexpr uint32_t NetworkScanStartDelayMs = 1000;
constexpr uint32_t QuoteRefreshIntervalMs = 60000;
constexpr uint32_t QuoteRetryIntervalMs = 10000;
constexpr time_t QuoteTlsStartEpoch = 1700000000;
// Przewijanie menu: liczba widocznych wierszy, ich wysokość i płynność animacji.
constexpr size_t OptionsVisibleRows = 5;
constexpr int16_t OptionsRowTop = 40;
constexpr int16_t OptionsRowHeight = 34;
constexpr int16_t OptionsScrollViewportHeight =
    OptionsVisibleRows * OptionsRowHeight;
constexpr int16_t OptionsTapSlop = 12;
constexpr uint32_t OptionsScrollFrameIntervalMs = 24;
constexpr uint8_t TouchCalibrationVersion = 2;
constexpr size_t SetupPasswordLength = 12;
constexpr char EventLogPath[] = "/log.txt";
constexpr char BackupEventLogPath[] = "/log.bak";
constexpr char LegacyEventLogPath[] = "/monitorakcji.txt";
constexpr char YahooHistoryUrl[] =
    "https://query1.finance.yahoo.com/v8/finance/chart/IPW.WA?range=1d&interval=5m";
// Położenie punktów używanych do kalibracji dotyku.
constexpr int32_t CalibrationScreenLeft = 15;
constexpr int32_t CalibrationScreenRight = 304;
constexpr int32_t CalibrationScreenTop = 15;
constexpr int32_t CalibrationScreenBottom = 224;

// Nieaktualny certyfikat NewConnect; zapytania Yahoo korzystają z YahooRootCa.
const char GpwRootCa[] PROGMEM = R"PEM(
-----BEGIN CERTIFICATE-----
MIIFbzCCA1egAwIBAgIRAOfdITiiyUFG/18SF4mVBVMwDQYJKoZIhvcNAQEMBQAw
UTELMAkGA1UEBhMCUEwxITAfBgNVBAoTGEFzc2VjbyBEYXRhIFN5c3RlbXMgUy5B
LjEfMB0GA1UEAxMWQ2VydHVtIFRMUyBSU0EgUm9vdCBDQTAeFw0yMzAxMjcwODMz
MDJaFw00ODAxMjYwODMzMDJaMFExCzAJBgNVBAYTAlBMMSEwHwYDVQQKExhBc3Nl
Y28gRGF0YSBTeXN0ZW1zIFMuQS4xHzAdBgNVBAMTFkNlcnR1bSBUTFMgUlNBIFJv
b3QgQ0EwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQDyHZKIvOi1jcw8
P+EZFDmpWybsrbIB/yZniNNGu8IqmyZj+Pv8/rZ0qAG3PFnWskD4SBrGO2FCHFWr
n5etBJKklH4Qgte8pk/y+AeTDYu8o2e/IQBtHUjmCFta7dvtjzMWffgHhtmvxrZK
Kbm5JyqvIA+3jXx5VjhWorwUskUnNUMHHoirKIUF4VE7719CPwuHoP90Waf3VSHj
h8HoV/bCfd47gzEFqMdv9bmUk73Fb2oVM4g6jp6hO9SXr36khTIyYsr5UKmeEtpJ
B3wtjyr1G5ywSXrLVbZ5+eEqNw493ndZ0jVKzyIUxAhKarB7y5hMjnota5aiV9nm
DmZg+ktNhvHbgbxMw3N65EI2fMKp2GSKmnnte5enxc7TGzFiPHF6TPUQxZIr5DDY
E0BbQ9/SnZf0oxbiTvYudr3u90NLSbFWxmY879X/6yRVpG9rfkwpK3CHAawlX2h6
JdssfHKZOTOSQlfxY1rvE1q7kNmOy4cHVMOUrwl5jDeYMYP1ox/B+RqxJpZf4ugr
E9LZTPLMn3BYCC7IwmJJ3oudT+is2E8iqgwp6OKW8qtz4TFg28gyqfzze6tT6jhF
wA3/DXxHacf+q7AI3fmj0zLqHC2gB76afn4814xHJnJyHn9l47F/sUampf4Bx+ZZ
gc+hSkWD29CJAyH9KWsNuJUkb7HKJQIDAQABo0IwQDAPBgNVHRMBAf8EBTADAQH/
MB0GA1UdDgQWBBTrNOQAoeZ7fNIj9MrXTsDLhWcVdjAOBgNVHQ8BAf8EBAMCAQYw
DQYJKoZIhvcNAQEMBQADggIBAEq8DpLvJqMG3xorPYQ34qX5uh0xEkQ9N+Xo8dnI
VTt7XwsYQpE+u07fg7+eb/J7QxUawS8tJ+CWNDoDp1+Fwv2S5Bi2L0DDTYTekKQy
Cn/T84Kw+Vv/jYDldvqeKKt3i+n473uQnqyD100jD2EykEqNQnXPAP1qTyHCmJk3
0EOJaCsK3Om7BahmCNDbttgHmuvbEXJp1CyQzDH7LUXfF5tCYeKbhWJf1rc1yM7S
U/fKD6eQ4Hk3X2/+IswaXK5abj7461KjUqx834Pk1tF4Pgip0gbodXcTmWQUOjwd
bhb4PZen7HET0XpE4ljAsaKpIF5N/SCpJQQvqnYZmF0LQPRA1SYTibi/faVdoKvr
I/nbUh2xVrnxForpehqJPtaDTdAToatDcflfy9UqbNAJ6eiPnarX0kKw1JGzcbHr
lNo2kmxMtGeeu23lNi5DCzbG59pJTujhRJi3/EdUjNI/D5Q+pQjT1cHfgpqQkAIh
/jQ6mwZrB22/hGoorAY7IW8bmyUpB41s3qLbD0mJXTlcHyUx8b4Sm4nkoTeCNgV6
amFiT2qFd8U8hyNTz7mVt8KZBftvSAHnzm1vxHubXh4HsWPEvtrR7HersIOMFUx/
7jn9H9GVKdCjF2w31oHA/tuY4XFJd923/cy7DNmWjZJjUCLPkSBtLChgA08hxsg1
DEv4
-----END CERTIFICATE-----
)PEM";

const char YahooRootCa[] PROGMEM = R"PEM(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
)PEM";

const char *TimeZone = "CET-1CEST,M3.5.0,M10.5.0/3";

enum class Screen : uint8_t {
  Clock,
  Options,
  WifiInfo,
  Brightness,
  SdCard,
  SdCardFormatConfirm,
  SdLog,
  Networks,
  Password,
  Calibration
};

// Jedna sieć Wi-Fi wyświetlana na liście zapisanych lub dostępnych sieci.
struct Network {
  String ssid;
  int32_t rssi = -100;
  bool saved = false;
  bool visible = false;
};

// Jedna próbka ceny z Yahoo, opisana minutą sesji giełdowej.
struct StockQuoteSample {
  uint16_t minuteOfDay;
  float price;
};

enum class TextAlignment : uint8_t { Left, Center, Right };

// Wbudowane fonty TFT_eSPI nie mają polskich liter. Najpierw rysowana jest
// podstawowa litera, a potem znak diakrytyczny.
int16_t drawPolishText(TFT_eSPI &display, const String &text, int32_t x,
                       int32_t y, uint8_t font, TextAlignment alignment) {
  struct Accent {
    size_t characterIndex;
    char kind;
  };
  Accent accents[48];
  size_t accentCount = 0;
  String plainText;
  plainText.reserve(text.length());

  for (size_t i = 0; i < text.length(); ++i) {
    const uint8_t firstByte = static_cast<uint8_t>(text[i]);
    if (firstByte < 0x80) {
      plainText += static_cast<char>(firstByte);
      continue;
    }

    if ((firstByte & 0xE0) != 0xC0 || i + 1 >= text.length()) {
      plainText += '?';
      continue;
    }
    const uint8_t secondByte = static_cast<uint8_t>(text[++i]);
    if ((secondByte & 0xC0) != 0x80) {
      plainText += '?';
      continue;
    }

    const uint16_t codePoint =
        ((firstByte & 0x1F) << 6) | (secondByte & 0x3F);
    char baseLetter = '?';
    char accent = 0;
    switch (codePoint) {
      case 0x0104: baseLetter = 'A'; accent = '_'; break;  // Ą
      case 0x0105: baseLetter = 'a'; accent = '_'; break;  // ą
      case 0x0106: baseLetter = 'C'; accent = '\''; break; // Ć
      case 0x0107: baseLetter = 'c'; accent = '\''; break; // ć
      case 0x0118: baseLetter = 'E'; accent = '_'; break;  // Ę
      case 0x0119: baseLetter = 'e'; accent = '_'; break;  // ę
      case 0x0141: baseLetter = 'L'; accent = '/'; break;  // Ł
      case 0x0142: baseLetter = 'l'; accent = '/'; break;  // ł
      case 0x0143: baseLetter = 'N'; accent = '\''; break; // Ń
      case 0x0144: baseLetter = 'n'; accent = '\''; break; // ń
      case 0x00D3: baseLetter = 'O'; accent = '\''; break; // Ó
      case 0x00F3: baseLetter = 'o'; accent = '\''; break; // ó
      case 0x015A: baseLetter = 'S'; accent = '\''; break; // Ś
      case 0x015B: baseLetter = 's'; accent = '\''; break; // ś
      case 0x0179: baseLetter = 'Z'; accent = '\''; break; // Ź
      case 0x017A: baseLetter = 'z'; accent = '\''; break; // ź
      case 0x017B: baseLetter = 'Z'; accent = '.'; break;  // Ż
      case 0x017C: baseLetter = 'z'; accent = '.'; break;  // ż
      default: break;
    }
    plainText += baseLetter;
    if (accent != 0 && accentCount < sizeof(accents) / sizeof(accents[0])) {
      accents[accentCount++] = {plainText.length() - 1, accent};
    }
  }

  const int16_t textWidth = display.textWidth(plainText, font);
  const int32_t left = alignment == TextAlignment::Center
                           ? x - textWidth / 2
                       : alignment == TextAlignment::Right ? x - textWidth
                                                           : x;
  display.TFT_eSPI::drawString(plainText, left, y, font);

  const uint16_t color = static_cast<uint16_t>(display.textcolor);
  const int16_t markSize = font == 1 ? 1 : font == 2 ? 1 : 2;
  const int16_t accentTop = font == 1 ? 2 : font == 2 ? 3 : 4;
  const int16_t glyphHeight = display.fontHeight(font);
  for (size_t i = 0; i < accentCount; ++i) {
    const Accent &mark = accents[i];
    const String prefix = plainText.substring(0, mark.characterIndex);
    const String letter = plainText.substring(mark.characterIndex,
                                              mark.characterIndex + 1);
    const int16_t letterX =
        left + display.textWidth(prefix, font);
    const int16_t letterWidth = display.textWidth(letter, font);
    const int16_t centerX = letterX + letterWidth / 2;

    if (mark.kind == '\'') {
      display.drawLine(centerX - markSize, y + accentTop + markSize,
                       centerX + markSize, y + accentTop, color);
    } else if (mark.kind == '.') {
      display.drawPixel(centerX, y + accentTop, color);
    } else if (mark.kind == '_') {
      display.drawLine(centerX, y + glyphHeight - markSize * 2,
                       centerX + markSize, y + glyphHeight - markSize, color);
    } else {
      display.drawLine(centerX - markSize, y + glyphHeight / 2 + markSize,
                       centerX + markSize, y + glyphHeight / 2 - markSize,
                       color);
    }
  }
  return textWidth;
}

// Przekierowuje zwykłe rysowanie tekstu, aby poprawnie wyświetlać polskie litery.
class PolishTFT : public TFT_eSPI {
 public:
  using TFT_eSPI::drawCentreString;
  using TFT_eSPI::drawRightString;
  using TFT_eSPI::drawString;

  int16_t drawString(const String &text, int32_t x, int32_t y, uint8_t font) {
    return drawPolishText(*this, text, x, y, font, TextAlignment::Left);
  }

  int16_t drawString(const char *text, int32_t x, int32_t y, uint8_t font) {
    return drawString(String(text), x, y, font);
  }

  int16_t drawCentreString(const String &text, int32_t x, int32_t y,
                           uint8_t font) {
    return drawPolishText(*this, text, x, y, font, TextAlignment::Center);
  }

  int16_t drawCentreString(const char *text, int32_t x, int32_t y,
                           uint8_t font) {
    return drawCentreString(String(text), x, y, font);
  }

  int16_t drawRightString(const String &text, int32_t x, int32_t y,
                          uint8_t font) {
    return drawPolishText(*this, text, x, y, font, TextAlignment::Right);
  }

  int16_t drawRightString(const char *text, int32_t x, int32_t y,
                          uint8_t font) {
    return drawRightString(String(text), x, y, font);
  }
};

// Sterowniki sprzętu i usługi używane przez interfejs.
PolishTFT tft;
TFT_eSprite optionsSprite(&tft);
XPT2046_Touchscreen touch(33, 36);
Preferences preferences;
WebServer webServer(80);
DNSServer dnsServer;
SemaphoreHandle_t stockQuoteMutex = nullptr;
std::vector<Network> networks;
Screen currentScreen = Screen::Clock;

// Stan ekranu, dotyku, połączenia Wi-Fi i animacji.
String selectedSsid;
String passwordInput;
String pendingPassword;
String statusMessage;
String lastDrawnTime;
String lastDrawnDate;
String lastDrawnWifiState;
String lastDrawnStockPrice;
String lastDrawnStockChange;
String lastDrawnStockAge;
String keyboardRows[3];
String setupSsid;
String setupPassword;
String webSessionToken;

uint32_t connectStartedAt = 0;
uint32_t lastClockUpdate = 0;
uint32_t lastTouchAt = 0;
uint32_t lastScanIndicatorUpdate = 0;
uint32_t calibrationReleasedAt = 0;
uint32_t networkScanStartAt = 0;
size_t networkPage = 0;
uint8_t networkScanStartAttempts = 0;
int16_t scanIndicatorPosition = -1;
uint16_t calibrationRawX[4] = {};
uint16_t calibrationRawY[4] = {};
uint8_t calibrationStep = 0;
int32_t touchXLeft = 0;
int32_t touchXRight = 4095;
int32_t touchYTop = 0;
int32_t touchYBottom = 4095;
bool touchWasDown = false;
bool connectionInProgress = false;
bool wifiWasConnected = false;
bool rememberAfterConnect = false;
bool networkScanPending = false;
bool resumeConnectionAfterScan = false;
bool passwordUppercase = false;
bool passwordSymbols = false;
bool touchCalibrated = false;
bool calibrationFromOptions = false;
bool calibrationTouchArmed = false;
bool timeSyncEnabled = true;
bool accessPointEnabled = true;
bool autoBacklightEnabled = false;
bool stockLedEnabled = true;
bool brightnessValueChanged = false;
bool brightnessSensitivityChanged = false;
bool brightnessAutoRangeChanged = false;
uint8_t manualBacklightLevel = 180;
uint8_t autoBacklightSensitivity = 50;
uint8_t autoBacklightMinimum = DefaultAutoBacklightMinimum;
uint8_t autoBacklightMaximum = DefaultAutoBacklightMaximum;
int filteredAmbientLight = -1;
int currentAmbientBacklightLevel = 180;
uint32_t lastBacklightUpdateAt = 0;
uint32_t lastBrightnessTouchAt = 0;
uint32_t lastBrightnessScreenDrawAt = 0;
bool optionsTouchActive = false;
uint16_t optionsTouchStartX = 0;
int16_t optionsTouchStartY = 0;
int16_t optionsTouchLastY = 0;
int16_t optionsScrollOffset = 0;
int16_t optionsPressedItem = -1;
float optionsScrollVelocity = 0.0f;
uint32_t optionsScrollAnimationAt = 0;
uint32_t optionsTouchSampleAt = 0;
uint32_t optionsLastRenderAt = 0;
bool optionsTouchMoved = false;
bool optionsScrollAnimating = false;
bool optionsSpriteReady = false;
bool optionsSpriteInitAttempted = false;
bool sdCardAvailable = false;
bool sdCardFilesystemMounted = false;
uint64_t sdCardTotalBytes = 0;
uint64_t sdCardUsedBytes = 0;
String sdCardMessage = "Nie sprawdzono karty";
std::vector<String> sdLogLines;
size_t sdLogPageStart = 0;
// Ostatni kurs i historia sesji; zadanie Yahoo używa ich pod ochroną mutexa.
String stockQuoteStatus = "Czekam na WiFi";
float stockQuotePrice = 0.0f;
float stockQuoteChangePercent = 0.0f;
bool stockQuoteAvailable = false;
bool stockQuoteChangeAvailable = false;
time_t stockQuoteTimestamp = 0;
std::vector<StockQuoteSample> stockQuoteSamples;
int32_t stockHistoryDay = -1;
uint32_t stockHistoryRevision = 0;
uint32_t lastDrawnStockHistoryRevision = UINT32_MAX;

struct LogEvent {
  enum class Type : uint8_t { Event, StockQuote };
  Type type = Type::Event;
  char text[128];
  time_t quoteTimestamp = 0;
  float quotePrice = 0.0f;
  float quoteChangePercent = 0.0f;
  bool quoteChangeAvailable = false;
};

QueueHandle_t eventLogQueue = nullptr;
uint32_t lastEventLogWriteAt = 0;
uint32_t lastEventLogWarningAt = 0;
uint32_t eventLogWriteIntervalMs = 250;

void beginTouchCalibration(bool fromOptions);
void drawBrightnessControls();
void drawBrightnessStatus();
void processEventLogQueue();

// Dodaje zdarzenie do kolejki; zapis na karcie wykonuje główna pętla.
void queueEventLog(const String &event) {
  if (eventLogQueue == nullptr) {
    Serial.println("Nie można zakolejkować logu: brak kolejki zdarzeń");
    return;
  }
  LogEvent entry = {};
  entry.type = LogEvent::Type::Event;
  event.toCharArray(entry.text, sizeof(entry.text));
  if (xQueueSend(eventLogQueue, &entry, 0) != pdTRUE) {
    Serial.println("Kolejka logu SD jest pełna; zdarzenie pominięto");
  }
}

// Kolejkuje pobrany kurs; zapis CSV wykona główna pętla obsługująca kartę SD.
void queueStockQuoteLog(time_t timestamp, float price, float changePercent,
                        bool changeAvailable) {
  if (eventLogQueue == nullptr) {
    Serial.println("Nie można zakolejkować ceny: brak kolejki zdarzeń");
    return;
  }
  LogEvent entry = {};
  entry.type = LogEvent::Type::StockQuote;
  entry.quoteTimestamp = timestamp;
  entry.quotePrice = price;
  entry.quoteChangePercent = changePercent;
  entry.quoteChangeAvailable = changeAvailable;
  if (xQueueSend(eventLogQueue, &entry, 0) != pdTRUE) {
    Serial.println("Kolejka logu SD jest pełna; zapis ceny pominięto");
  }
}

// Uruchamia synchronizację NTP ze strefą czasową, jeśli pozwalają na to ustawienia.
void startNtpSynchronization() {
  if (!timeSyncEnabled || WiFi.status() != WL_CONNECTED) {
    return;
  }
  configTzTime(TimeZone, "pool.ntp.org", "time.google.com", "time.nist.gov");
  Serial.println("NTP synchronization started");
  queueEventLog("Uruchomiono synchronizację czasu NTP");
}

// Uruchamia hotspot konfiguracyjny do ustawiania Wi-Fi i zegara z telefonu.
bool startSetupAccessPoint() {
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(setupSsid.c_str(), setupPassword.c_str())) {
    Serial.println("Błąd uruchamiania punktu dostępowego konfiguracji");
    queueEventLog("BŁĄD: nie udało się uruchomić punktu dostępowego");
    return false;
  }
  if (!dnsServer.start(53, "*", WiFi.softAPIP())) {
    Serial.println("Błąd uruchamiania DNS captive portalu");
    queueEventLog("BŁĄD: nie udało się uruchomić DNS captive portalu");
  }
  Serial.printf("Setup WiFi: SSID=%s, IP=%s\n",
                setupSsid.c_str(),
                WiFi.softAPIP().toString().c_str());
  return true;
}

// Wyłącza hotspot konfiguracyjny bez rozłączania routera.
void stopSetupAccessPoint() {
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  if (!WiFi.mode(WIFI_STA)) {
    Serial.println("Błąd wyłączania trybu punktu dostępowego");
    queueEventLog("BŁĄD: nie udało się wyłączyć punktu dostępowego");
  }
  queueEventLog("Wyłączono punkt dostępowy");
  Serial.println("Setup access point stopped");
}

// Rysuje zaokrąglony przycisk w kolorach używanych przez dany ekran.
void drawButton(int16_t x, int16_t y, int16_t width, int16_t height,
                const String &label, uint16_t fill, uint16_t text = TFT_WHITE,
                bool pressed = false) {
  const int16_t offset = pressed ? 2 : 0;
  const uint16_t buttonFill = pressed ? TFT_DARKGREY : fill;
  tft.fillRoundRect(x, y + offset, width, height - offset, 5, buttonFill);
  tft.drawRoundRect(x, y + offset, width, height - offset, 5,
                    pressed ? TFT_WHITE : TFT_LIGHTGREY);
  tft.setTextColor(text, buttonFill);
  tft.drawCentreString(label, x + width / 2,
                       y + offset + (height - 16) / 2, 2);
}

// Rysuje niebieski pasek tytułowy ekranów ustawień i konfiguracji.
void drawHeader(const String &title, const String &buttonLabel = "") {
  tft.fillRect(0, 0, ScreenWidth, 32, TFT_NAVY);
  const bool showBack = currentScreen != Screen::Clock &&
                        !(currentScreen == Screen::Calibration && !touchCalibrated);
  if (showBack) {
    tft.drawLine(8, 16, 23, 16, TFT_WHITE);
    tft.drawLine(8, 16, 15, 9, TFT_WHITE);
    tft.drawLine(8, 16, 15, 23, TFT_WHITE);
  }
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString(title, showBack ? 30 : 8, 8, 2);
  if (buttonLabel.length() > 0) {
    tft.setTextColor(TFT_YELLOW, TFT_NAVY);
    tft.drawRightString(buttonLabel, 312, 9, 2);
  }
}

// Szuka sieci na liście w RAM; zwraca -1, jeśli jej tam nie ma.
int findNetwork(const String &ssid) {
  for (size_t i = 0; i < networks.size(); ++i) {
    if (networks[i].ssid == ssid) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

// Odczytuje zapisane hasło sieci z pamięci Preferences.
bool loadSavedCredentials(const String &ssid, String &password) {
  const uint8_t count = preferences.getUChar("count", 0);
  for (uint8_t i = 0; i < count; ++i) {
    const String key = "s" + String(i);
    if (preferences.getString(key.c_str(), "") == ssid) {
      password = preferences.getString(("p" + String(i)).c_str(), "");
      return true;
    }
  }
  return false;
}

// Wczytuje zapisane nazwy sieci z Preferences podczas uruchamiania.
void loadSavedNetworks() {
  networks.clear();
  const uint8_t count = preferences.getUChar("count", 0);
  for (uint8_t i = 0; i < count; ++i) {
    const String ssid = preferences.getString(("s" + String(i)).c_str(), "");
    if (ssid.isEmpty()) {
      continue;
    }
    Network network;
    network.ssid = ssid;
    network.saved = true;
    networks.push_back(network);
  }
}

// Zapisuje dane sieci w Preferences i oznacza ją jako zapisaną.
bool saveCredentials(const String &ssid, const String &password) {
  const uint8_t count = preferences.getUChar("count", 0);
  int8_t index = -1;
  for (uint8_t i = 0; i < count; ++i) {
    if (preferences.getString(("s" + String(i)).c_str(), "") == ssid) {
      index = static_cast<int8_t>(i);
      break;
    }
  }

  if (index < 0) {
    if (count >= MaxSavedNetworks) {
      statusMessage = "Pamięć sieci pełna (12). Usuń wpis w Preferences.";
      queueEventLog("BŁĄD: pamięć zapisanych sieci WiFi jest pełna");
      return false;
    }
    index = static_cast<int8_t>(count);
    preferences.putUChar("count", count + 1);
  }

  preferences.putString(("s" + String(index)).c_str(), ssid);
  preferences.putString(("p" + String(index)).c_str(), password);
  preferences.putString("last", ssid);

  const int networkIndex = findNetwork(ssid);
  if (networkIndex >= 0) {
    networks[networkIndex].saved = true;
  }
  return true;
}

// Formatuje kurs z dwoma miejscami po przecinku.
String formatStockPrice(float price) {
  String formatted = String(price, 2);
  formatted.replace('.', ',');
  return formatted;
}

// Zamienia datę na format RRRRMMDD do porównywania sesji giełdowych.
int32_t stockDayKey(const struct tm &localTime) {
  return (localTime.tm_year + 1900) * 10000 +
         (localTime.tm_mon + 1) * 100 + localTime.tm_mday;
}

// Rysuje ikonę ustawień w prawym górnym rogu ekranu głównego.
void drawSettingsIcon() {
  constexpr int16_t centerX = 267;
  constexpr int16_t centerY = 17;
  const uint16_t color = TFT_LIGHTGREY;
  tft.drawCircle(centerX, centerY, 7, color);
  tft.drawCircle(centerX, centerY, 3, color);
  tft.drawLine(centerX, centerY - 11, centerX, centerY - 7, color);
  tft.drawLine(centerX, centerY + 7, centerX, centerY + 11, color);
  tft.drawLine(centerX - 11, centerY, centerX - 7, centerY, color);
  tft.drawLine(centerX + 7, centerY, centerX + 11, centerY, color);
  tft.drawLine(centerX - 8, centerY - 8, centerX - 5, centerY - 5, color);
  tft.drawLine(centerX + 5, centerY + 5, centerX + 8, centerY + 8, color);
  tft.drawLine(centerX + 5, centerY - 5, centerX + 8, centerY - 8, color);
  tft.drawLine(centerX - 8, centerY + 8, centerX - 5, centerY + 5, color);
}

// Zamienia siłę sygnału Wi-Fi na zero–cztery kreski ikony.
int8_t wifiSignalBars() {
  if (WiFi.status() != WL_CONNECTED) {
    return 0;
  }
  const int32_t rssi = WiFi.RSSI();
  return rssi >= -55 ? 4 : rssi >= -67 ? 3 : rssi >= -75 ? 2 : rssi >= -85 ? 1 : 0;
}

// Rysuje kreski sygnału; brak połączenia oznacza szary symbol z ukośnikiem.
void drawWifiSignalIcon(int8_t activeBars, bool connected) {
  constexpr int16_t iconX = 293;
  constexpr int16_t baseY = 27;
  constexpr int16_t barWidth = 3;
  constexpr int16_t gap = 2;
  const uint16_t activeColor = connected ? TFT_GREEN : TFT_DARKGREY;
  const int8_t heights[] = {5, 8, 11, 14};

  for (int8_t i = 0; i < 4; ++i) {
    const int16_t x = iconX + i * (barWidth + gap);
    const int16_t y = baseY - heights[i];
    if (connected && i < activeBars) {
      tft.fillRect(x, y, barWidth, heights[i], activeColor);
    } else {
      tft.drawRect(x, y, barWidth, heights[i], TFT_DARKGREY);
    }
  }
  if (!connected) {
    tft.drawLine(iconX, 8, iconX + 18, 27, TFT_RED);
  }
}

// Rysuje zegar, datę oraz ikony ustawień i Wi-Fi.
void drawHomeHeader() {
  tft.fillRect(0, 0, ScreenWidth, 34, TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("--:--", 8, 4, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("--.--.----", 94, 11, 2);
  drawSettingsIcon();
  const bool connected = WiFi.status() == WL_CONNECTED;
  drawWifiSignalIcon(connected ? wifiSignalBars() : 0, connected);
  lastDrawnWifiState = "";
  lastDrawnTime = "";
  lastDrawnDate = "";
}

// Dioda RGB płytki jest ze wspólną anodą, więc kanały PWM są odwrócone.
void setStockLedColor(uint8_t red, uint8_t green, uint8_t blue) {
  if (!stockLedEnabled) {
    red = 0;
    green = 0;
    blue = 0;
  }
  const uint8_t redDuty = StockLedActiveLow ? 255 - red : red;
  const uint8_t greenDuty = StockLedActiveLow ? 255 - green : green;
  const uint8_t blueDuty = StockLedActiveLow ? 255 - blue : blue;
  ledcWrite(StockLedRedPwmChannel, redDuty);
  ledcWrite(StockLedGreenPwmChannel, greenDuty);
  ledcWrite(StockLedBluePwmChannel, blueDuty);
}

// Odtwarza kolor LED z aktualnego kursu, np. po włączeniu diody w opcjach.
void updateStockLedFromQuote() {
  float changePercent = 0.0f;
  bool available = false;
  bool changeAvailable = false;
  if (stockQuoteMutex != nullptr &&
      xSemaphoreTake(stockQuoteMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    available = stockQuoteAvailable;
    changeAvailable = stockQuoteChangeAvailable;
    changePercent = stockQuoteChangePercent;
    xSemaphoreGive(stockQuoteMutex);
  }

  if (!available || !changeAvailable || changePercent == 0.0f) {
    setStockLedColor(192, 192, 192);
  } else if (changePercent > 0.0f) {
    setStockLedColor(0, 255, 0);
  } else {
    setStockLedColor(255, 0, 0);
  }
}

// Rysuje duży kurs na wykresie: zielony oznacza wzrost, czerwony spadek,
// a szary brak zmiany lub brak danych.
void drawClockStylePrice(float price, bool available, bool changeAvailable,
                         float changePercent) {
  const String priceText = available ? formatStockPrice(price) : "--,--";
  // Zwiększ te wymiary, aby cyfry były większe; szerokość musi mieścić cały kurs.
  constexpr int16_t digitWidth = 52;
  constexpr int16_t digitHeight = 120;
  constexpr int16_t thickness = 11;
  constexpr int16_t verticalHeight =
      (digitHeight - 3 * thickness) / 2;
  constexpr int16_t digitSpacing = 3;
  constexpr int16_t commaWidth = 12;

  int16_t totalWidth = 0;
  for (size_t i = 0; i < priceText.length(); ++i) {
    totalWidth += priceText[i] == ',' ? commaWidth : digitWidth;
    if (i + 1 < priceText.length()) {
      totalWidth += digitSpacing;
    }
  }
  int16_t x = (ScreenWidth - totalWidth) / 2;
  constexpr int16_t y = 83;  // Pionowe położenie kursu na tle wykresu.
  const uint16_t color =
      !available || !changeAvailable
          ? TFT_LIGHTGREY
          : changePercent > 0.0f ? TFT_GREEN
          : changePercent < 0.0f ? TFT_RED
                                : TFT_LIGHTGREY;
  if (!available || !changeAvailable || changePercent == 0.0f) {
    setStockLedColor(192, 192, 192);
  } else if (changePercent > 0.0f) {
    setStockLedColor(0, 255, 0);
  } else {
    setStockLedColor(255, 0, 0);
  }

  for (size_t i = 0; i < priceText.length(); ++i) {
    const char character = priceText[i];
    if (character == ',') {
      tft.fillCircle(x + 2, y + digitHeight - 2, 3, color);
      tft.drawLine(x + 4, y + digitHeight, x + 1, y + digitHeight + 5,
                   color);
      x += commaWidth + digitSpacing;
      continue;
    }

    // Każdy bit oznacza jeden z siedmiu segmentów cyfry.
    uint8_t segments = 0;
    if (character >= '0' && character <= '9') {
      const uint8_t digitSegments[] = {
          0x3F, 0x06, 0x5B, 0x4F, 0x66,
          0x6D, 0x7D, 0x07, 0x7F, 0x6F};
      segments = digitSegments[character - '0'];
    } else if (character == '-') {
      segments = 0x40;
    }

    if (segments & 0x01) {
      tft.fillRect(x + thickness, y, digitWidth - 2 * thickness,
                   thickness, color);
    }
    if (segments & 0x02) {
      tft.fillRect(x + digitWidth - thickness, y + thickness, thickness,
                   verticalHeight, color);
    }
    if (segments & 0x04) {
      tft.fillRect(x + digitWidth - thickness,
                   y + 2 * thickness + verticalHeight, thickness,
                   verticalHeight, color);
    }
    if (segments & 0x08) {
      tft.fillRect(x + thickness, y + digitHeight - thickness,
                   digitWidth - 2 * thickness, thickness, color);
    }
    if (segments & 0x10) {
      tft.fillRect(x, y + 2 * thickness + verticalHeight, thickness,
                   verticalHeight, color);
    }
    if (segments & 0x20) {
      tft.fillRect(x, y + thickness, thickness, verticalHeight, color);
    }
    if (segments & 0x40) {
      tft.fillRect(x + thickness, y + thickness + verticalHeight,
                   digitWidth - 2 * thickness, thickness, color);
    }
    x += digitWidth + digitSpacing;
  }
}

// Pokazuje czas od ostatniej transakcji Yahoo, a nie opóźnienie całego źródła.
String formatStockQuoteAge(time_t timestamp) {
  if (timestamp <= 0) {
    return "Kurs sprzed: --";
  }
  const time_t now = time(nullptr);
  if (now < timestamp) {
    return "Kurs sprzed: --";
  }

  const time_t ageMinutes = (now - timestamp) / 60;
  if (ageMinutes == 0) {
    return "Kurs sprzed: <1 min";
  }
  if (ageMinutes < 60) {
    return "Kurs sprzed: " + String(static_cast<long>(ageMinutes)) + " min";
  }
  const time_t ageHours = ageMinutes / 60;
  const time_t remainingMinutes = ageMinutes % 60;
  return "Kurs sprzed: " + String(static_cast<long>(ageHours)) + "h " +
         String(static_cast<long>(remainingMinutes)) + "m";
}

// Rysuje wykres sesji i nakłada duży kurs na jego pole.
void drawStockChart() {
  // Granice pola wykresu i zakres godzin sesji; zmiana wpływa na skalowanie.
  constexpr int16_t plotLeft = 32;
  constexpr int16_t plotTop = 64;
  constexpr int16_t plotWidth = 280;
  constexpr int16_t plotHeight = 150;
  constexpr uint16_t marketStartMinute = 9 * 60;
  constexpr uint16_t marketEndMinute = 17 * 60;
  constexpr uint16_t marketDuration = marketEndMinute - marketStartMinute;

  tft.fillRect(0, 54, ScreenWidth, ScreenHeight - 54, TFT_BLACK);
  const bool haveLock =
      stockQuoteMutex != nullptr &&
      xSemaphoreTake(stockQuoteMutex, pdMS_TO_TICKS(20)) == pdTRUE;
  if (!haveLock) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawCentreString("Historia niedostępna", 160, 130, 1);
    return;
  }

  if (stockHistoryDay > 0) {
    char sessionDate[20];
    const int32_t year = stockHistoryDay / 10000;
    const int32_t month = (stockHistoryDay / 100) % 100;
    const int32_t day = stockHistoryDay % 100;
    snprintf(sessionDate, sizeof(sessionDate), "Sesja %02ld.%02ld.%04ld",
             static_cast<long>(day), static_cast<long>(month),
             static_cast<long>(year));
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawRightString(sessionDate, 312, 54, 1);
  }

  // Dopasowuje skalę cen do dzisiejszych próbek i dodaje niewielki margines.
  float minimum = 0.0f;
  float maximum = 0.0f;
  bool hasSamples = false;
  for (const StockQuoteSample &sample : stockQuoteSamples) {
    if (sample.minuteOfDay < marketStartMinute ||
        sample.minuteOfDay > marketEndMinute) {
      continue;
    }
    if (!hasSamples) {
      minimum = sample.price;
      maximum = sample.price;
      hasSamples = true;
    } else {
      minimum = min(minimum, sample.price);
      maximum = max(maximum, sample.price);
    }
  }

  if (hasSamples) {
    const float padding = max(0.01f, (maximum - minimum) * 0.12f);
    minimum -= padding;
    maximum += padding;
  } else {
    minimum = 0.0f;
    maximum = 1.0f;
  }

  for (int8_t i = 0; i <= 2; ++i) {
    const int16_t y = plotTop + i * (plotHeight - 1) / 2;
    tft.drawFastHLine(plotLeft, y, plotWidth, TFT_DARKGREY);
    const float labelPrice = maximum - (maximum - minimum) * i / 2.0f;
    String label = String(labelPrice, 2);
    label.replace('.', ',');
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawRightString(label, plotLeft - 5, y - 3, 1);
  }

  for (int8_t i = 0; i <= 4; ++i) {
    const int16_t x = plotLeft + i * (plotWidth - 1) / 4;
    tft.drawFastVLine(x, plotTop, plotHeight, TFT_DARKGREY);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    const String timeLabel = String(9 + i * 2) + ":00";
    if (i == 0) {
      tft.drawString(timeLabel, x, 219, 1);
    } else if (i == 4) {
      tft.drawRightString(timeLabel, x, 219, 1);
    } else {
      tft.drawCentreString(timeLabel, x, 219, 1);
    }
  }

  if (!hasSamples) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawCentreString("Oczekiwanie na notowania Yahoo", 180, 130, 1);
  } else {
    bool previousPoint = false;
    int16_t previousX = 0;
    int16_t previousY = 0;
    for (const StockQuoteSample &sample : stockQuoteSamples) {
      if (sample.minuteOfDay < marketStartMinute ||
          sample.minuteOfDay > marketEndMinute) {
        continue;
      }
      const int16_t x = plotLeft + static_cast<int32_t>(
                                       sample.minuteOfDay - marketStartMinute) *
                                       (plotWidth - 1) / marketDuration;
      const int16_t y = plotTop + plotHeight - 1 -
                        static_cast<int32_t>(
                            (sample.price - minimum) / (maximum - minimum) *
                            (plotHeight - 1));
      if (previousPoint) {
        tft.drawLine(previousX, previousY, x, y, TFT_CYAN);
      } else {
        tft.fillCircle(x, y, 2, TFT_CYAN);
      }
      previousX = x;
      previousY = y;
      previousPoint = true;
    }
  }

  const float quotePrice = stockQuotePrice;
  const bool quoteAvailable = stockQuoteAvailable;
  const float quoteChangePercent = stockQuoteChangePercent;
  const bool quoteChangeAvailable = stockQuoteChangeAvailable;
  lastDrawnStockHistoryRevision = stockHistoryRevision;
  xSemaphoreGive(stockQuoteMutex);
  drawClockStylePrice(quotePrice, quoteAvailable, quoteChangeAvailable,
                      quoteChangePercent);
}

// Rysuje zmianę dzienną i wiek kursu nad wykresem.
void drawStockQuote() {
  float price = 0.0f;
  float changePercent = 0.0f;
  bool available = false;
  bool changeAvailable = false;
  time_t quoteTimestamp = 0;
  String status;
  if (stockQuoteMutex != nullptr &&
      xSemaphoreTake(stockQuoteMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    price = stockQuotePrice;
    changePercent = stockQuoteChangePercent;
    available = stockQuoteAvailable;
    changeAvailable = stockQuoteChangeAvailable;
    quoteTimestamp = stockQuoteTimestamp;
    status = stockQuoteStatus;
    xSemaphoreGive(stockQuoteMutex);
  }

  const String priceText = available ? formatStockPrice(price) : "--,--";
  String changeText = available ? "Yahoo Finance" : status;
  const String quoteAge = formatStockQuoteAge(quoteTimestamp);
  uint16_t changeColor = TFT_LIGHTGREY;
  if (changeAvailable) {
    changeText = String(changePercent >= 0.0f ? "+" : "") +
                 String(changePercent, 2) + "%";
    changeText.replace('.', ',');
    changeColor = changePercent > 0.0f ? TFT_GREEN
                  : changePercent < 0.0f ? TFT_RED
                                         : TFT_LIGHTGREY;
  }

  tft.fillRect(0, 36, ScreenWidth, 18, TFT_BLACK);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.setTextColor(changeColor, TFT_BLACK);
  tft.drawString(changeText, 8, 39, 1);
  tft.drawRightString(quoteAge, 312, 39, 1);
  lastDrawnStockPrice = priceText;
  lastDrawnStockChange = changeText;
  lastDrawnStockAge = quoteAge;
}

// Czyści i odtwarza ekran główny po powrocie lub pełnym odświeżeniu.
void drawClockScreen() {
  currentScreen = Screen::Clock;
  tft.fillScreen(TFT_BLACK);
  drawHomeHeader();
  lastDrawnStockPrice = "";
  lastDrawnStockChange = "";
  lastDrawnStockAge = "";
  lastDrawnStockHistoryRevision = UINT32_MAX;
  drawStockQuote();
  drawStockChart();
}

constexpr size_t OptionsItemCount = 9;

// Maksymalne przesunięcie listy, przy którym ostatnia opcja mieści się na ekranie.
int16_t maxOptionsScrollOffset() {
  const int16_t contentHeight =
      static_cast<int16_t>(OptionsItemCount * OptionsRowHeight);
  return max<int16_t>(
      0, static_cast<int16_t>(contentHeight - OptionsScrollViewportHeight));
}

// Rysuje jeden wiersz menu lub przełącznik.
void drawOptionRow(size_t item, int16_t y, TFT_eSPI &canvas,
                   bool pressed = false) {
  const char *labels[] = {
      "Wybierz sieć WiFi",
      "Kalibracja dotyku",
      "Synchronizacja czasu",
      "Punkt dostępowy",
      "Konfiguracja WiFi / IP",
      "Jasność ekranu",
      "Automatyczna jasność",
      "Karta SD",
      "Dioda RGB kursu"};
  const bool isSwitch = item == 2 || item == 3 || item == 6 || item == 8;
  const bool enabled = item == 2 ? timeSyncEnabled
                      : item == 3 ? accessPointEnabled
                      : item == 6 ? autoBacklightEnabled
                                  : stockLedEnabled;
  const uint16_t fill = pressed ? TFT_DARKGREY : TFT_NAVY;
  constexpr int16_t rowX = 8;
  constexpr int16_t rowWidth = 304;
  constexpr int16_t rowHeight = 30;

  canvas.fillRoundRect(rowX, y, rowWidth, rowHeight, 4, fill);
  canvas.drawRoundRect(rowX, y, rowWidth, rowHeight, 4,
                       pressed ? TFT_WHITE : TFT_DARKGREY);
  canvas.setTextColor(TFT_WHITE, fill);
  drawPolishText(canvas, labels[item], 17, y + 9, 2, TextAlignment::Left);

  if (isSwitch) {
    const uint16_t trackColor = enabled ? TFT_DARKGREEN : TFT_MAROON;
    canvas.fillRoundRect(260, y + 7, 40, 16, 8, trackColor);
    canvas.fillCircle(enabled ? 292 : 268, y + 15, 6, TFT_WHITE);
  } else {
    canvas.setTextColor(TFT_LIGHTGREY, fill);
    canvas.drawRightString(">", 298, y + 8, 2);
  }
}

// Rysuje widoczne opcje w buforze obrazu, aby ograniczyć migotanie.
void drawOptionsContent() {
  if (optionsSpriteReady) {
    optionsSprite.fillSprite(TFT_BLACK);
    for (size_t item = 0; item < OptionsItemCount; ++item) {
      const int16_t rowY =
          static_cast<int16_t>(item * OptionsRowHeight) - optionsScrollOffset;
      if (rowY + 30 > 0 && rowY < OptionsScrollViewportHeight) {
        drawOptionRow(item, rowY, optionsSprite,
                      static_cast<int16_t>(item) == optionsPressedItem);
      }
    }
    optionsSprite.pushSprite(0, OptionsRowTop);
  } else {
    tft.setViewport(0, OptionsRowTop, ScreenWidth,
                    OptionsScrollViewportHeight, false);
    tft.fillRect(0, OptionsRowTop, ScreenWidth, OptionsScrollViewportHeight,
                 TFT_BLACK);
    for (size_t item = 0; item < OptionsItemCount; ++item) {
      const int16_t rowY = OptionsRowTop +
                           static_cast<int16_t>(item * OptionsRowHeight) -
                           optionsScrollOffset;
      if (rowY + 30 > OptionsRowTop &&
          rowY < OptionsRowTop + OptionsScrollViewportHeight) {
        drawOptionRow(item, rowY, tft,
                      static_cast<int16_t>(item) == optionsPressedItem);
      }
    }
    tft.resetViewport();
  }

  tft.fillRect(0, 214, ScreenWidth, 22, TFT_BLACK);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Przesuń listę, aby przewinąć", 8, 220, 1);

  const size_t firstItem =
      static_cast<size_t>(optionsScrollOffset / OptionsRowHeight);
  const size_t lastItem = min(
      OptionsItemCount,
      static_cast<size_t>((optionsScrollOffset + OptionsScrollViewportHeight +
                           OptionsRowHeight - 1) /
                          OptionsRowHeight));
  tft.drawRightString(
      String(firstItem + 1) + "-" + String(lastItem) + "/" +
          String(OptionsItemCount),
      312, 220, 1);
}

// Tworzy bufor menu; po błędzie alokacji rysowanie działa bez niego.
void initializeOptionsSprite() {
  if (optionsSpriteInitAttempted) {
    return;
  }
  optionsSpriteInitAttempted = true;
  optionsSprite.setColorDepth(8);
  optionsSpriteReady =
      optionsSprite.createSprite(ScreenWidth, OptionsScrollViewportHeight) !=
      nullptr;
  if (!optionsSpriteReady) {
    Serial.println("Could not allocate options scroll buffer");
    queueEventLog("BŁĄD: nie udało się utworzyć bufora przewijania menu");
  }
}

// Rysuje nagłówek opcji i aktualnie widoczne wiersze menu.
void drawOptionsScreen() {
  currentScreen = Screen::Options;
  tft.fillScreen(TFT_BLACK);
  drawHeader("OPCJE", "ZEGAR");
  initializeOptionsSprite();
  drawOptionsContent();
  optionsLastRenderAt = millis();
}

// Kontynuuje przewijanie menu z bezwładnością po oderwaniu palca.
void updateOptionsScrollAnimation() {
  if (currentScreen != Screen::Options || !optionsScrollAnimating) {
    return;
  }

  const uint32_t now = millis();
  const uint32_t elapsed = now - optionsScrollAnimationAt;
  if (elapsed < OptionsScrollFrameIntervalMs) {
    return;
  }
  optionsScrollAnimationAt = now;

  const float displacement = optionsScrollVelocity * elapsed / 1000.0f;
  const int16_t previousOffset = optionsScrollOffset;
  optionsScrollOffset = constrain(
      static_cast<int16_t>(optionsScrollOffset + lroundf(displacement)), 0,
      maxOptionsScrollOffset());
  optionsScrollVelocity *= expf(-static_cast<float>(elapsed) / 260.0f);

  if (optionsScrollOffset == 0 ||
      optionsScrollOffset == maxOptionsScrollOffset()) {
    optionsScrollVelocity = 0.0f;
  }

  if (fabsf(optionsScrollVelocity) < 28.0f) {
    optionsScrollAnimating = false;
    optionsScrollVelocity = 0.0f;
  }

  if (previousOffset != optionsScrollOffset) {
    drawOptionsContent();
    optionsLastRenderAt = now;
  }
}

// Pokazuje nazwę routera, stan połączenia i lokalny adres IP.
void drawWifiInfoScreen() {
  currentScreen = Screen::WifiInfo;
  tft.fillScreen(TFT_BLACK);
  drawHeader("WIFI / IP", "OPCJE");

  const bool connected = WiFi.status() == WL_CONNECTED;
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Router WiFi:", 16, 51, 2);
  tft.setTextColor(connected ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
  tft.drawString(connected ? WiFi.SSID() : "Brak połączenia", 16, 73, 2);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("IP routera:", 16, 103, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(connected ? WiFi.localIP().toString() : "---", 16, 125, 2);

  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Punkt dostępowy:", 16, 155, 2);
  tft.setTextColor(accessPointEnabled ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
  tft.drawString(accessPointEnabled ? setupSsid : "Wyłączony", 16, 177, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  if (accessPointEnabled) {
    tft.drawString("192.168.4.1  Hasło: " + setupPassword, 16, 205, 1);
  } else if (connected) {
    tft.drawString("Panel: http://" + WiFi.localIP().toString(), 16, 205, 1);
  }
}

// Ustawia PWM podświetlenia; poziom ręczny pozostaje zapisany także w trybie auto.
void setBacklightLevel(uint8_t level) {
  currentAmbientBacklightLevel =
      constrain(level, MinimumBacklight, MaximumBacklight);
  ledcWrite(BacklightPwmChannel, currentAmbientBacklightLevel);
}

// Odczytuje czujnik światła i płynnie dostosowuje podświetlenie w trybie auto.
void updateBacklight() {
  if (millis() - lastBacklightUpdateAt < 200) {
    return;
  }
  lastBacklightUpdateAt = millis();

  uint32_t sensorTotal = 0;
  for (uint8_t sample = 0; sample < 8; ++sample) {
    sensorTotal += analogRead(AmbientLightPin);
  }
  const int sensorReading = static_cast<int>(sensorTotal / 8);
  filteredAmbientLight =
      filteredAmbientLight < 0
          ? sensorReading
          : (filteredAmbientLight * 7 + sensorReading) / 8;

  const int ambientLightBrightRaw = map(
      autoBacklightSensitivity, 0, 100, AmbientLightBrightRawMinimum,
      AmbientLightBrightRawMaximum);
  const uint8_t targetLevel =
      autoBacklightEnabled
          ? static_cast<uint8_t>(map(
                constrain(filteredAmbientLight, ambientLightBrightRaw,
                          AmbientLightDarkRaw),
                ambientLightBrightRaw, AmbientLightDarkRaw,
                autoBacklightMaximum, autoBacklightMinimum))
          : manualBacklightLevel;
  const int previousLevel = currentAmbientBacklightLevel;
  if (targetLevel != previousLevel) {
    setBacklightLevel(targetLevel);
  }

  if (currentScreen == Screen::Brightness &&
      currentAmbientBacklightLevel != previousLevel) {
    drawBrightnessControls();
  }
  if (currentScreen == Screen::Brightness &&
      millis() - lastBrightnessScreenDrawAt >= 1000) {
    lastBrightnessScreenDrawAt = millis();
    drawBrightnessStatus();
  }
}

void drawBrightnessControls() {
  constexpr int16_t sliderLeft = 30;
  constexpr int16_t sliderWidth = 260;
  const auto drawSlider = [&](const char *label, uint8_t percentage,
                              int16_t labelY, int16_t trackY) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(label, 20, labelY, 2);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawRightString(String(percentage) + "%", 300, labelY, 2);
    const int16_t filledWidth =
        static_cast<int16_t>(percentage) * sliderWidth / 100;
    tft.fillRoundRect(sliderLeft, trackY, sliderWidth, 12, 6, TFT_DARKGREY);
    tft.fillRoundRect(sliderLeft, trackY, filledWidth, 12, 6, TFT_CYAN);
    tft.fillCircle(sliderLeft + filledWidth, trackY + 6, 8, TFT_WHITE);
  };

  tft.fillRect(0, 33, ScreenWidth, 164, TFT_BLACK);
  drawSlider("Poziom ręczny",
             map(manualBacklightLevel, MinimumBacklight, MaximumBacklight, 1,
                 100),
             34, 54);
  drawSlider("Czułość automatyki", autoBacklightSensitivity,
             74, 94);
  drawSlider("Jasność minimalna",
             map(autoBacklightMinimum, MinimumBacklight, MaximumBacklight, 1,
                 100),
             114, 134);
  drawSlider("Jasność maksymalna",
             map(autoBacklightMaximum, MinimumBacklight, MaximumBacklight, 1,
                 100),
             154, 174);
}

void drawBrightnessStatus() {
  tft.fillRect(0, 225, ScreenWidth, 15, TFT_BLACK);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawCentreString(
      "Czujnik światła: " + String(filteredAmbientLight) + " / 4095",
      ScreenWidth / 2, 228, 1);
}

// Ekran jasności: suwakiem zmienia się poziom ręczny, a przełącznikiem tryb auto.
void drawBrightnessScreen() {
  currentScreen = Screen::Brightness;
  tft.fillScreen(TFT_BLACK);
  drawHeader("PODŚWIETLENIE", "OPCJE");
  drawBrightnessControls();

  tft.fillRoundRect(28, 197, 264, 28, 5,
                    autoBacklightEnabled ? TFT_DARKGREEN : TFT_NAVY);
  tft.drawRoundRect(28, 197, 264, 28, 5, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, autoBacklightEnabled ? TFT_DARKGREEN : TFT_NAVY);
  tft.drawString("Automatyczna regulacja", 38, 203, 2);
  tft.fillRoundRect(247, 201, 36, 20, 10,
                    autoBacklightEnabled ? TFT_GREEN : TFT_MAROON);
  tft.fillCircle(autoBacklightEnabled ? 271 : 259, 211, 7, TFT_WHITE);
  drawBrightnessStatus();
  lastBrightnessScreenDrawAt = millis();
}

// Przywraca magistralę VSPI do pinów dotyku po dostępie do karty SD.
void restoreTouchSpi() {
  SPI.end();
  SPI.begin(TouchClockPin, TouchMisoPin, TouchMosiPin, TouchChipSelectPin);
  touch.begin();
  touch.setRotation(3);
}

// Generates and persists a per-device password for the setup access point.
String loadOrCreateSetupPassword() {
  String password = preferences.getString("apPassword", "");
  if (password.length() == SetupPasswordLength) {
    return password;
  }

  constexpr char PasswordAlphabet[] =
      "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  password.reserve(SetupPasswordLength);
  for (size_t i = 0; i < SetupPasswordLength; ++i) {
    password += PasswordAlphabet[esp_random() & 31];
  }

  if (preferences.putString("apPassword", password) != password.length()) {
    Serial.println("Nie udało się zapisać hasła punktu dostępowego");
    queueEventLog("BŁĄD: nie udało się zapisać hasła punktu dostępowego");
  }
  return password;
}

// Czyta plik porcjami i zachowuje tylko najnowsze wpisy w ograniczonym buforze.
bool readRecentLogLines(std::vector<String> &lines, size_t limit) {
  lines.clear();
  limit = constrain(limit, static_cast<size_t>(1), static_cast<size_t>(30));
  SD.end();
  SPI.end();
  SPI.begin(SdCardClockPin, SdCardMisoPin, SdCardMosiPin,
            SdCardChipSelectPin);
  if (!SD.begin(SdCardChipSelectPin, SPI, SdCardSpiFrequency)) {
    SD.end();
    restoreTouchSpi();
    Serial.println("Nie można odczytać logu: brak dostępnej karty SD/FAT32");
    return false;
  }

  const char *logPath =
      SD.exists(EventLogPath) ? EventLogPath : LegacyEventLogPath;
  if (!SD.exists(logPath)) {
    SD.end();
    restoreTouchSpi();
    return true;
  }

  File logFile = SD.open(logPath, FILE_READ);
  if (!logFile) {
    SD.end();
    restoreTouchSpi();
    Serial.println("Nie można otworzyć pliku logu do odczytu");
    return false;
  }

  constexpr size_t ReadChunkSize = 512;
  constexpr size_t MaximumLogLineLength = 192;
  const uint32_t fileSize = logFile.size();
  uint8_t readBuffer[ReadChunkSize];
  char lineBuffer[MaximumLogLineLength];
  size_t lineLength = 0;
  uint32_t remaining = fileSize;
  while (remaining > 0) {
    const size_t requested = min(static_cast<size_t>(remaining),
                                 sizeof(readBuffer));
    const size_t bytesRead = logFile.read(readBuffer, requested);
    if (bytesRead == 0 || bytesRead > requested) {
      Serial.printf("Niepełny odczyt logu SD: rozmiar=%lu, pozostało=%lu, "
                    "pozycja=%lu, odczyt=%lu\n",
                    static_cast<unsigned long>(fileSize),
                    static_cast<unsigned long>(remaining),
                    static_cast<unsigned long>(logFile.position()),
                    static_cast<unsigned long>(bytesRead));
      logFile.close();
      SD.end();
      restoreTouchSpi();
      return false;
    }
    remaining -= static_cast<uint32_t>(bytesRead);

    for (size_t i = 0; i < bytesRead; ++i) {
      const char value = static_cast<char>(readBuffer[i]);
      if (value == '\n') {
        if (lineLength > 0 && lineBuffer[lineLength - 1] == '\r') {
          --lineLength;
        }
        if (lineLength > 0) {
          lineBuffer[lineLength] = '\0';
          if (lines.size() == limit) {
            lines.erase(lines.begin());
          }
          lines.emplace_back(lineBuffer);
        }
        lineLength = 0;
      } else if (lineLength < sizeof(lineBuffer) - 1) {
        lineBuffer[lineLength++] = value;
      }
    }
  }
  if (lineLength > 0) {
    lineBuffer[lineLength] = '\0';
    if (lines.size() == limit) {
      lines.erase(lines.begin());
    }
    lines.emplace_back(lineBuffer);
  }

  logFile.close();
  SD.end();
  restoreTouchSpi();
  return true;
}

// Odczytuje informacje o karcie i odłącza ją, zanim wznowi obsługę dotyku.
void inspectSdCard() {
  sdCardAvailable = false;
  sdCardFilesystemMounted = false;
  sdCardTotalBytes = 0;
  sdCardUsedBytes = 0;
  SD.end();
  SPI.end();
  SPI.begin(SdCardClockPin, SdCardMisoPin, SdCardMosiPin,
            SdCardChipSelectPin);

  if (SD.begin(SdCardChipSelectPin, SPI, SdCardSpiFrequency)) {
    sdCardAvailable = SD.cardType() != CARD_NONE;
    sdCardFilesystemMounted = sdCardAvailable;
    sdCardTotalBytes = SD.totalBytes();
    sdCardUsedBytes = SD.usedBytes();
    sdCardMessage = sdCardAvailable ? "Karta gotowa" : "Nie wykryto karty";
    SD.end();
    restoreTouchSpi();
    return;
  }

  SD.end();

  // Sprawdza kartę bez montowania FAT, aby dało się sformatować pustą kartę.
  const uint8_t drive =
      sdcard_init(SdCardChipSelectPin, &SPI, SdCardSpiFrequency);
  if (drive != 0xFF) {
    sdCardAvailable = true;
    sdCardTotalBytes =
        static_cast<uint64_t>(sdcard_num_sectors(drive)) *
        sdcard_sector_size(drive);
    if (sdcard_uninit(drive) != 0) {
      Serial.println("Nie udało się odłączyć karty SD po sprawdzeniu");
    }
    sdCardMessage = "Karta wykryta; wymaga FAT32";
  } else {
    sdCardMessage = "Nie wykryto karty";
  }
  restoreTouchSpi();
}

// Dopisuje jedno notowanie do CSV i obraca plik po przekroczeniu limitu.
bool appendStockQuoteCsv(const LogEvent &entry) {
  constexpr char CsvPath[] = "/imagepower.csv";
  constexpr char BackupCsvPath[] = "/imagepower.bak";
  constexpr size_t MaximumCsvSize = 256 * 1024;
  constexpr char CsvHeader[] = "timestamp,price_pln,change_percent\r\n";

  if (SD.exists(CsvPath)) {
    File currentFile = SD.open(CsvPath, FILE_READ);
    if (!currentFile) {
      Serial.println("Nie można sprawdzić rozmiaru pliku CSV z kursem");
      return false;
    }
    const size_t currentSize = currentFile.size();
    currentFile.close();
    if (currentSize >= MaximumCsvSize) {
      SD.remove(BackupCsvPath);
      if (!SD.rename(CsvPath, BackupCsvPath)) {
        Serial.println("Nie można obrócić pliku CSV z kursem");
        return false;
      }
    }
  }

  File csvFile = SD.open(CsvPath, FILE_APPEND);
  if (!csvFile) {
    Serial.println("Nie można otworzyć pliku CSV z kursem");
    return false;
  }
  if (csvFile.size() == 0 &&
      csvFile.print(CsvHeader) != sizeof(CsvHeader) - 1) {
    csvFile.close();
    Serial.println("Nie można zapisać nagłówka pliku CSV z kursem");
    return false;
  }

  char timestamp[20] = "0000-00-00 00:00:00";
  struct tm localTime = {};
  if (entry.quoteTimestamp > 0 &&
      localtime_r(&entry.quoteTimestamp, &localTime) != nullptr) {
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S",
             &localTime);
  }

  char row[80];
  const int rowLength = entry.quoteChangeAvailable
                           ? snprintf(row, sizeof(row), "%s,%.2f,%.2f\r\n",
                                      timestamp, entry.quotePrice,
                                      entry.quoteChangePercent)
                           : snprintf(row, sizeof(row), "%s,%.2f,\r\n",
                                      timestamp, entry.quotePrice);
  const bool writeSucceeded =
      rowLength > 0 && static_cast<size_t>(rowLength) < sizeof(row) &&
      csvFile.write(reinterpret_cast<const uint8_t *>(row),
                    static_cast<size_t>(rowLength)) ==
          static_cast<size_t>(rowLength);
  csvFile.flush();
  csvFile.close();
  if (!writeSucceeded) {
    Serial.println("Błąd zapisu notowania do pliku CSV");
  }
  return writeSucceeded;
}

// Dopisuje oczekujące zdarzenia do pliku i zawsze przywraca SPI dotyku.
void processEventLogQueue() {
  if (eventLogQueue == nullptr || uxQueueMessagesWaiting(eventLogQueue) == 0 ||
      millis() - lastEventLogWriteAt < eventLogWriteIntervalMs) {
    return;
  }
  lastEventLogWriteAt = millis();

  SD.end();
  SPI.end();
  SPI.begin(SdCardClockPin, SdCardMisoPin, SdCardMosiPin,
            SdCardChipSelectPin);
  if (!SD.begin(SdCardChipSelectPin, SPI, SdCardSpiFrequency)) {
    SD.end();
    restoreTouchSpi();
    eventLogWriteIntervalMs = 5000;
    if (millis() - lastEventLogWarningAt >= 30000) {
      Serial.println("Nie można zapisać logu: brak dostępnej karty SD/FAT32");
      lastEventLogWarningAt = millis();
    }
    return;
  }

  constexpr size_t MaximumLogSize = 128 * 1024;
  if (!SD.exists(EventLogPath) && SD.exists(LegacyEventLogPath) &&
      !SD.rename(LegacyEventLogPath, EventLogPath)) {
    SD.end();
    restoreTouchSpi();
    eventLogWriteIntervalMs = 5000;
    Serial.println("Nie można zmienić nazwy starego pliku logu SD");
    return;
  }

  File logFile = SD.open(EventLogPath, FILE_APPEND);
  if (!logFile) {
    SD.end();
    restoreTouchSpi();
    eventLogWriteIntervalMs = 5000;
    Serial.println("Nie można otworzyć pliku logu na karcie SD");
    return;
  }

  if (logFile.size() >= MaximumLogSize) {
    logFile.close();
    SD.remove(BackupEventLogPath);
    if (!SD.rename(EventLogPath, BackupEventLogPath)) {
      SD.end();
      restoreTouchSpi();
      eventLogWriteIntervalMs = 5000;
      Serial.println("Nie można obrócić pliku logu karty SD");
      return;
    }
    logFile = SD.open(EventLogPath, FILE_APPEND);
    if (!logFile) {
      SD.end();
      restoreTouchSpi();
      eventLogWriteIntervalMs = 5000;
      Serial.println("Nie można utworzyć nowego pliku logu karty SD");
      return;
    }
  }

  bool writeFailed = false;
  for (uint8_t count = 0; count < 8; ++count) {
    LogEvent entry = {};
    if (xQueuePeek(eventLogQueue, &entry, 0) != pdTRUE) {
      break;
    }

    if (entry.type == LogEvent::Type::StockQuote) {
      if (!appendStockQuoteCsv(entry)) {
        writeFailed = true;
        break;
      }
      xQueueReceive(eventLogQueue, &entry, 0);
      continue;
    }

    char timestamp[20] = "0000-00-00 00:00:00";
    const time_t now = time(nullptr);
    if (now > 0) {
      struct tm localTime;
      if (localtime_r(&now, &localTime) != nullptr) {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S",
                 &localTime);
      }
    }
    const String line =
        String(timestamp) + "; " + String(entry.text) + "\r\n";
    if (logFile.print(line) != line.length()) {
      writeFailed = true;
      break;
    }
    logFile.flush();
    xQueueReceive(eventLogQueue, &entry, 0);
  }

  logFile.close();
  SD.end();
  restoreTouchSpi();
  eventLogWriteIntervalMs = writeFailed ? 5000 : 250;
  if (writeFailed) {
    Serial.println("Błąd zapisu wpisu do pliku logu SD");
  }
}

// Formatuje kartę jako FAT32; wywołanie następuje dopiero po potwierdzeniu.
bool formatSdCard() {
  SD.end();
  SPI.end();
  SPI.begin(SdCardClockPin, SdCardMisoPin, SdCardMosiPin,
            SdCardChipSelectPin);

  const uint8_t drive =
      sdcard_init(SdCardChipSelectPin, &SPI, SdCardSpiFrequency);
  if (drive == 0xFF) {
    Serial.println("Nie można zainicjalizować karty SD do formatowania");
    queueEventLog("BŁĄD: nie można zainicjalizować karty SD do formatowania");
    restoreTouchSpi();
    return false;
  }

  uint8_t *workBuffer = static_cast<uint8_t *>(malloc(4096));
  if (workBuffer == nullptr) {
    sdcard_uninit(drive);
    restoreTouchSpi();
    Serial.println("Brak pamięci na bufor formatowania karty SD");
    queueEventLog("BŁĄD: brak pamięci na bufor formatowania karty SD");
    return false;
  }

  char volume[3] = {static_cast<char>('0' + drive), ':', '\0'};
  const FRESULT formatResult =
      f_mkfs(volume, FM_FAT32 | FM_SFD, 0, workBuffer, 4096);
  free(workBuffer);
  const uint8_t uninitResult = sdcard_uninit(drive);
  restoreTouchSpi();

  if (formatResult != FR_OK || uninitResult != 0) {
    Serial.printf("Formatowanie karty SD nie powiodło się: FAT=%d, SD=%u\n",
                  static_cast<int>(formatResult), uninitResult);
    queueEventLog("BŁĄD: formatowanie karty SD nie powiodło się");
    return false;
  }
  Serial.println("Karta SD sformatowana jako FAT32");
  queueEventLog("Sformatowano kartę SD jako FAT32");
  return true;
}

// Pokazuje stan i pojemność karty; nagłówek umożliwia ponowne sprawdzenie.
void drawSdCardScreen(bool refreshCard = true) {
  currentScreen = Screen::SdCard;
  if (refreshCard) {
    inspectSdCard();
    if (!sdCardAvailable) {
      queueEventLog("BŁĄD: nie wykryto karty SD");
    } else if (!sdCardFilesystemMounted) {
      queueEventLog("BŁĄD: karta SD nie ma czytelnego systemu FAT");
    }
  }
  tft.fillScreen(TFT_BLACK);
  drawHeader("KARTA SD", "ODŚWIEŻ");
  tft.setTextColor(sdCardAvailable ? TFT_GREEN : TFT_YELLOW, TFT_BLACK);
  tft.drawCentreString(sdCardMessage, ScreenWidth / 2, 54, 2);

  if (sdCardAvailable) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("Pojemność:", 24, 91, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawRightString(
        String(static_cast<double>(sdCardTotalBytes) / 1048576.0, 1) + " MB",
        296, 91, 2);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    if (sdCardFilesystemMounted) {
      tft.drawString("Zajęte:", 24, 121, 2);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawRightString(
          String(static_cast<double>(sdCardUsedBytes) / 1048576.0, 1) +
              " MB",
          296, 121, 2);
    } else {
      tft.drawCentreString("System plików nieczytelny", ScreenWidth / 2,
                           121, 2);
    }
    drawButton(24, 153, 272, 34,
               sdCardFilesystemMounted ? "PODGLĄD LOGU" : "LOG NIEDOSTĘPNY",
               sdCardFilesystemMounted ? TFT_DARKCYAN : TFT_DARKGREY,
               sdCardFilesystemMounted ? TFT_WHITE : TFT_LIGHTGREY);
    drawButton(24, 194, 272, 34, "FORMATUJ FAT32", TFT_MAROON);
  } else {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawCentreString("Włóż kartę FAT32 i odśwież", ScreenWidth / 2,
                         101, 2);
    drawButton(24, 194, 272, 34, "BRAK KARTY", TFT_DARKGREY,
               TFT_LIGHTGREY);
  }
}

// Pokazuje do 15 krótkich wpisów na stronie; odczyt tylko przy wejściu/odświeżeniu.
void drawSdLogScreen(bool reload = true) {
  currentScreen = Screen::SdLog;
  tft.fillScreen(TFT_BLACK);
  drawHeader("PODGLĄD LOGU", "ODŚWIEŻ");
  if (reload) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawCentreString("Wczytywanie logu...", ScreenWidth / 2, 108, 2);
    sdLogLines.clear();
    if (!sdCardFilesystemMounted ||
        !readRecentLogLines(sdLogLines, 30)) {
      sdLogLines.clear();
      tft.fillRect(0, 32, ScreenWidth, ScreenHeight - 32, TFT_BLACK);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawCentreString("Nie można odczytać logu SD",
                           ScreenWidth / 2, 105, 2);
      return;
    }
    constexpr size_t EntriesPerPage = 15;
    sdLogPageStart = sdLogLines.size() > 0
                         ? ((sdLogLines.size() - 1) / EntriesPerPage) *
                               EntriesPerPage
                         : 0;
  }

  tft.fillRect(0, 32, ScreenWidth, ScreenHeight - 32, TFT_BLACK);
  if (sdLogLines.empty()) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawCentreString("Brak wpisów w /log.txt",
                         ScreenWidth / 2, 105, 2);
    return;
  }

  constexpr size_t EntriesPerPage = 15;
  constexpr int16_t FirstLineY = 44;
  constexpr int16_t LineHeight = 10;
  const size_t pageEnd =
      min(sdLogPageStart + EntriesPerPage, sdLogLines.size());
  const size_t firstNumber = sdLogPageStart + 1;
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Wpisy " + String(firstNumber) + "-" +
                     String(pageEnd) + " z " + String(sdLogLines.size()),
                 8, 33, 1);

  int16_t y = FirstLineY;
  for (size_t entry = sdLogPageStart; entry < pageEnd; ++entry) {
    String displayLine;
    size_t characterCount = 0;
    for (size_t position = 0; position < sdLogLines[entry].length();) {
      const uint8_t firstByte =
          static_cast<uint8_t>(sdLogLines[entry][position]);
      size_t characterLength = 1;
      if ((firstByte & 0xE0) == 0xC0) {
        characterLength = 2;
      } else if ((firstByte & 0xF0) == 0xE0) {
        characterLength = 3;
      } else if ((firstByte & 0xF8) == 0xF0) {
        characterLength = 4;
      }
      if (characterCount == 48) {
        displayLine += "...";
        break;
      }
      displayLine +=
          sdLogLines[entry].substring(position, position + characterLength);
      position += characterLength;
      ++characterCount;
    }
    drawPolishText(tft, displayLine, 8, y, 1, TextAlignment::Left);
    y += LineHeight;
  }

  drawButton(8, 211, 145, 22, "STARSZE",
             sdLogPageStart > 0 ? TFT_DARKCYAN : TFT_DARKGREY,
             sdLogPageStart > 0 ? TFT_WHITE : TFT_LIGHTGREY);
  const bool hasNewerEntries =
      sdLogPageStart + EntriesPerPage < sdLogLines.size();
  drawButton(167, 211, 145, 22, "NOWSZE",
             hasNewerEntries ? TFT_DARKCYAN : TFT_DARKGREY,
             hasNewerEntries ? TFT_WHITE : TFT_LIGHTGREY);
}

// Ostrzega, że formatowanie bezpowrotnie usuwa zawartość karty.
void drawSdFormatConfirmation() {
  currentScreen = Screen::SdCardFormatConfirm;
  tft.fillScreen(TFT_BLACK);
  drawHeader("FORMATOWANIE SD");
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawCentreString("UWAGA!", ScreenWidth / 2, 56, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("Wszystkie pliki na karcie", ScreenWidth / 2, 106, 2);
  tft.drawCentreString("zostaną usunięte.", ScreenWidth / 2, 131, 2);
  drawButton(12, 180, 138, 40, "ANULUJ", TFT_DARKGREY);
  drawButton(170, 180, 138, 40, "FORMATUJ", TFT_MAROON);
}

// Odświeża tylko zmienione elementy zegara, kursu, Wi-Fi i wykresu.
void updateClock() {
  if (currentScreen != Screen::Clock || millis() - lastClockUpdate < 400) {
    return;
  }
  lastClockUpdate = millis();

  struct tm localTime;
  if (getLocalTime(&localTime, 10)) {
    char timeText[8];
    char dateText[20];
    strftime(timeText, sizeof(timeText), "%H:%M", &localTime);
    strftime(dateText, sizeof(dateText), "%d.%m.%Y", &localTime);

    if (lastDrawnTime != timeText) {
      tft.fillRect(4, 2, 80, 30, TFT_BLACK);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.drawString(timeText, 8, 4, 4);
      lastDrawnTime = timeText;
    }
    if (lastDrawnDate != dateText) {
      tft.fillRect(90, 7, 132, 22, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString(dateText, 94, 11, 2);
      lastDrawnDate = dateText;
    }
  } else {
    if (lastDrawnTime != "--:--") {
      tft.fillRect(4, 2, 80, 30, TFT_BLACK);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.drawString("--:--", 8, 4, 4);
      lastDrawnTime = "--:--";
    }
    const String timeStatus = timeSyncEnabled ? "NTP..." : "Brak czasu";
    if (lastDrawnDate != timeStatus) {
      tft.fillRect(90, 7, 132, 22, TFT_BLACK);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString(timeStatus, 94, 13, 1);
      lastDrawnDate = timeStatus;
    }
  }

  float price = 0.0f;
  float changePercent = 0.0f;
  bool quoteAvailable = false;
  bool changeAvailable = false;
  time_t quoteTimestamp = 0;
  String quoteStatus;
  if (stockQuoteMutex != nullptr &&
      xSemaphoreTake(stockQuoteMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    price = stockQuotePrice;
    changePercent = stockQuoteChangePercent;
    quoteAvailable = stockQuoteAvailable;
    changeAvailable = stockQuoteChangeAvailable;
    quoteTimestamp = stockQuoteTimestamp;
    quoteStatus = stockQuoteStatus;
    xSemaphoreGive(stockQuoteMutex);
  }
  const String priceText = quoteAvailable ? formatStockPrice(price) : "--,--";
  String changeText = quoteAvailable ? "Yahoo Finance" : quoteStatus;
  if (changeAvailable) {
    changeText = String(changePercent >= 0.0f ? "+" : "") +
                 String(changePercent, 2) + "%";
    changeText.replace('.', ',');
  }
  const String quoteAge = formatStockQuoteAge(quoteTimestamp);
  const bool stockDisplayChanged =
      lastDrawnStockPrice != priceText || lastDrawnStockChange != changeText ||
      lastDrawnStockAge != quoteAge;
  if (stockDisplayChanged) {
    drawStockQuote();
  }

  const bool connected = WiFi.status() == WL_CONNECTED;
  const int8_t bars = connected ? wifiSignalBars() : 0;
  const String wifiState =
      String(connected ? "1:" : "0:") + String(static_cast<int>(bars));
  if (lastDrawnWifiState != wifiState) {
    tft.fillRect(289, 4, 24, 26, TFT_BLACK);
    drawWifiSignalIcon(bars, connected);
    lastDrawnWifiState = wifiState;
  }

  uint32_t historyRevision = 0;
  if (stockQuoteMutex != nullptr &&
      xSemaphoreTake(stockQuoteMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    historyRevision = stockHistoryRevision;
    xSemaphoreGive(stockQuoteMutex);
  }
  if (stockDisplayChanged || lastDrawnStockHistoryRevision != historyRevision) {
    drawStockChart();
  }
}

// Łączy z routerem i opcjonalnie wraca do ekranu głównego.
void startConnection(const String &ssid, const String &password, bool remember,
                     bool redraw = true) {
  queueEventLog("Rozpoczęto łączenie z WiFi: " + ssid);
  wifiWasConnected = false;
  selectedSsid = ssid;
  pendingPassword = password;
  rememberAfterConnect = remember;
  statusMessage = "Łączenie z " + ssid;
  connectionInProgress = true;
  connectStartedAt = millis();
  WiFi.mode(accessPointEnabled ? WIFI_AP_STA : WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(ssid.c_str(), password.c_str());
  if (redraw) {
    currentScreen = Screen::Clock;
    drawClockScreen();
  }
}

// Łączy z siecią, której hasło jest już zapisane.
void connectSavedNetwork(const String &ssid) {
  String password;
  if (!loadSavedCredentials(ssid, password)) {
    statusMessage = "Brak zapisanego hasła dla " + ssid;
    return;
  }
  startConnection(ssid, password, false);
}

// Ponownie łączy z ostatnią siecią wybraną przez użytkownika.
void connectLastSavedNetwork() {
  const String lastSsid = preferences.getString("last", "");
  if (!lastSsid.isEmpty()) {
    connectSavedNetwork(lastSsid);
  }
}

// Sprawdza, czy skanowanie Wi-Fi oczekuje na start lub już trwa.
bool isNetworkScanActive() {
  return networkScanPending || WiFi.scanComplete() == WIFI_SCAN_RUNNING;
}

// Rozpoczyna skanowanie i zapamiętuje, czy po nim wznowić połączenie z routerem.
void startNetworkScan() {
  if (isNetworkScanActive()) {
    return;
  }

  if (connectionInProgress) {
    resumeConnectionAfterScan = true;
    wifiWasConnected = false;
    WiFi.disconnect();
    connectionInProgress = false;
  } else if (WiFi.status() == WL_CONNECTED) {
    selectedSsid = WiFi.SSID();
    if (loadSavedCredentials(selectedSsid, pendingPassword)) {
      rememberAfterConnect = false;
      resumeConnectionAfterScan = true;
      wifiWasConnected = false;
      WiFi.disconnect();
    }
  }

  WiFi.mode(accessPointEnabled ? WIFI_AP_STA : WIFI_STA);
  WiFi.scanDelete();
  networkScanPending = true;
  networkScanStartAt = millis();
  networkScanStartAttempts = 0;
  statusMessage = "Skanowanie sieci...";
  scanIndicatorPosition = -1;
  queueEventLog("Rozpoczęto skanowanie sieci WiFi");
}

// Wznawia przerwane połączenie z routerem po zakończeniu skanowania.
void resumeStationConnectionAfterScan() {
  if (!resumeConnectionAfterScan) {
    return;
  }

  resumeConnectionAfterScan = false;
  startConnection(selectedSsid, pendingPassword, rememberAfterConnect, false);
}

// Przenosi wyniki skanowania na listę i wznawia ewentualne połączenie.
void finishNetworkScan() {
  const int count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING) {
    return;
  }
  if (count == WIFI_SCAN_FAILED) {
    statusMessage = "Skanowanie WiFi nie powiodło się";
    queueEventLog("BŁĄD: skanowanie sieci WiFi nie powiodło się");
    WiFi.scanDelete();
    resumeStationConnectionAfterScan();
    return;
  }

  for (Network &network : networks) {
    network.visible = false;
    network.rssi = -100;
  }
  for (int i = 0; i < count; ++i) {
    const String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) {
      continue;
    }
    int index = findNetwork(ssid);
    if (index < 0) {
      Network network;
      network.ssid = ssid;
      networks.push_back(network);
      index = static_cast<int>(networks.size() - 1);
    }
    networks[index].visible = true;
    networks[index].rssi = WiFi.RSSI(i);
  }
  WiFi.scanDelete();
  networkPage = 0;
  size_t visibleNetworkCount = 0;
  for (const Network &network : networks) {
    if (network.visible) {
      ++visibleNetworkCount;
    }
  }
  statusMessage = visibleNetworkCount == 0
                      ? "Nie znaleziono sieci"
                      : "Znaleziono sieci: " + String(visibleNetworkCount);
  queueEventLog(statusMessage);
  resumeStationConnectionAfterScan();
}

// Pokazuje postęp skanowania albo liczbę znalezionych sieci.
void drawNetworkScanStatus() {
  const bool scanning = isNetworkScanActive();
  const String message = scanning
                            ? "Skanowanie sieci..."
                            : (statusMessage.isEmpty() ? "Dotknij sieci, aby połączyć" : statusMessage);
  const uint16_t color = scanning ? TFT_YELLOW
                                  : (statusMessage.startsWith("Skanowanie WiFi nie")
                                         ? TFT_RED
                                         : TFT_LIGHTGREY);
  tft.fillRect(0, 33, ScreenWidth, 14, TFT_BLACK);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(message, 10, 35, 1);
  tft.fillRect(10, 47, 300, 4, TFT_DARKGREY);
  scanIndicatorPosition = -1;
  if (scanning) {
    scanIndicatorPosition = 0;
    tft.fillRect(10, 47, 8, 4, TFT_CYAN);
  }
}

// Animuje znacznik podczas skanowania Wi-Fi.
void updateNetworkScanIndicator() {
  constexpr int16_t markerWidth = 8;
  constexpr int16_t travelWidth = 300 - markerWidth;
  const int16_t phase = static_cast<int16_t>((millis() / 20) % (2 * travelWidth));
  const int16_t position = phase <= travelWidth ? phase : 2 * travelWidth - phase;
  if (scanIndicatorPosition >= 0) {
    tft.fillRect(10 + scanIndicatorPosition, 47, markerWidth, 4, TFT_DARKGREY);
  }
  tft.fillRect(10 + position, 47, markerWidth, 4, TFT_CYAN);
  scanIndicatorPosition = position;
}

// Rysuje stronicowaną listę zapisanych i wykrytych sieci Wi-Fi.
void drawNetworkScreen() {
  currentScreen = Screen::Networks;
  tft.fillScreen(TFT_BLACK);
  drawHeader("WYBIERZ SIEC", "SKANUJ");
  drawNetworkScanStatus();

  const size_t pageCount = max(static_cast<size_t>(1),
                               (networks.size() + NetworksPerPage - 1) / NetworksPerPage);
  const size_t start = networkPage * NetworksPerPage;
  for (size_t row = 0; row < NetworksPerPage && start + row < networks.size(); ++row) {
    const Network &network = networks[start + row];
    const int16_t y = 57 + static_cast<int16_t>(row) * 31;
    const uint16_t color = network.saved ? TFT_DARKGREEN : TFT_NAVY;
    tft.fillRoundRect(8, y, 304, 28, 4, color);
    tft.drawRoundRect(8, y, 304, 28, 4, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, color);
    String label = network.ssid;
    if (label.length() > (network.saved ? 28 : 39)) {
      label = label.substring(0, network.saved ? 25 : 36) + "...";
    }

    tft.drawString(label, 15, y + 9, 1);

    if (network.saved) {
      tft.fillRoundRect(246, y + 4, 60, 20, 3, TFT_MAROON);
      tft.setTextColor(TFT_WHITE, TFT_MAROON);
      tft.drawCentreString("USUN", 276, y + 8, 1);
    } else if (network.visible) {
      tft.setTextColor(TFT_YELLOW, color);
      tft.drawRightString(String(network.rssi) + "dBm", 304, y + 8, 1);
    }
  }

  if (networks.empty()) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawCentreString(isNetworkScanActive()
                             ? "Szukam sieci WiFi..."
                             : (statusMessage.isEmpty() ? "Brak zapisanych sieci" : statusMessage),
                         ScreenWidth / 2, 100, 2);
  }

  drawButton(8, 213, 50, 23, "<", TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString(String(networkPage + 1) + "/" + String(pageCount),
                       160, 218, 1);
  drawButton(262, 213, 50, 23, ">", TFT_DARKGREY);
}

// Usuwa zapisane hasło i rozłącza zapomnianą sieć, jeśli jest aktywna.
void forgetSavedNetwork(const String &ssid) {
  const uint8_t count = preferences.getUChar("count", 0);
  uint8_t index = count;
  for (uint8_t i = 0; i < count; ++i) {
    if (preferences.getString(("s" + String(i)).c_str(), "") == ssid) {
      index = i;
      break;
    }
  }
  if (index == count) {
    return;
  }

  for (uint8_t i = index; i + 1 < count; ++i) {
    const String nextSsid = preferences.getString(("s" + String(i + 1)).c_str(), "");
    const String nextPassword = preferences.getString(("p" + String(i + 1)).c_str(), "");
    preferences.putString(("s" + String(i)).c_str(), nextSsid);
    preferences.putString(("p" + String(i)).c_str(), nextPassword);
  }
  preferences.remove(("s" + String(count - 1)).c_str());
  preferences.remove(("p" + String(count - 1)).c_str());
  preferences.putUChar("count", count - 1);
  if (preferences.getString("last", "") == ssid) {
    preferences.remove("last");
  }
  for (Network &network : networks) {
    if (network.ssid == ssid) {
      network.saved = false;
    }
  }
  networks.erase(std::remove_if(networks.begin(), networks.end(),
                                [&ssid](const Network &network) {
                                  return network.ssid == ssid && !network.visible;
                                }),
                 networks.end());
  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == ssid) {
    connectionInProgress = false;
    WiFi.disconnect();
  }
  statusMessage = "Zapomniano sieć: " + ssid;
  queueEventLog("Usunięto zapisaną sieć WiFi: " + ssid);
}

// Wraca do ekranu, z którego otwarto bieżącą stronę konfiguracji.
void returnToPreviousScreen() {
  switch (currentScreen) {
    case Screen::Options:
      drawClockScreen();
      break;
    case Screen::WifiInfo:
    case Screen::Brightness:
      drawOptionsScreen();
      break;
    case Screen::SdCard:
      drawOptionsScreen();
      break;
    case Screen::SdCardFormatConfirm:
      drawSdCardScreen();
      break;
    case Screen::SdLog:
      drawSdCardScreen();
      break;
    case Screen::Networks:
      drawOptionsScreen();
      break;
    case Screen::Password:
      drawNetworkScreen();
      break;
    case Screen::Calibration:
      if (touchCalibrated) {
        drawOptionsScreen();
      }
      break;
    case Screen::Clock:
      break;
  }
}

// Strona WWW do ustawiania czasu, wyboru sieci Wi-Fi i kalibracji dotyku.
const char webPage[] PROGMEM = R"HTML(
<!doctype html>
<html lang="pl">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Zegar ESP32</title>
  <style>
    body{font:16px system-ui,sans-serif;max-width:480px;margin:40px auto;padding:0 18px;background:#101827;color:#eef4ff}
    main{background:#1c293b;border-radius:16px;padding:24px}
    h1{margin-top:0}#clock{font-size:2rem;color:#40e0d0;margin:20px 0}
    label{display:block;margin:14px 0 6px}input,button{box-sizing:border-box;width:100%;padding:12px;border:0;border-radius:8px;font:inherit}
    button{appearance:none;-webkit-appearance:none;margin-top:18px;background:#087f8c;color:white;font-weight:700;cursor:pointer;touch-action:manipulation;user-select:none;-webkit-tap-highlight-color:transparent;box-shadow:0 4px 0 #075d68;transition:transform 80ms ease,box-shadow 80ms ease,filter 80ms ease}
    button:active:not(:disabled){transform:translateY(3px);box-shadow:0 1px 0 #075d68;filter:brightness(.82)}
    button:focus-visible{outline:3px solid #40e0d0;outline-offset:3px}
    button:disabled{cursor:not-allowed;opacity:.5;box-shadow:none}
    #message{min-height:1.5em;color:#a8e6cf}
    input[type=checkbox]{width:auto;margin-right:10px}
    input[type=range]{padding:0;accent-color:#40e0d0}
    section{border-top:1px solid #536174;margin-top:24px;padding-top:8px}
    .switch{display:flex;align-items:center;margin:14px 0}
    .value{color:#40e0d0}
    .danger{background:#a83232}
    #sdLog{box-sizing:border-box;max-height:300px;overflow:auto;white-space:pre-wrap;overflow-wrap:anywhere;background:#101827;padding:12px;border-radius:8px;font:13px ui-monospace,monospace;color:#d4e2f5}
    @media(min-width:700px){
      body{font-size:14px;max-width:1080px;margin:20px auto;padding:0 24px}
      main{padding:18px;border-radius:12px}
      h1{font-size:1.55rem}
      h2{font-size:1.2rem}
      #clock{font-size:1.65rem;margin:12px 0}
      label{margin:10px 0 4px}
      input,button{padding:9px}
      button{margin-top:12px}
      section{margin-top:18px}
      .switch{margin:10px 0}
      #sdLog{max-height:420px;font-size:12px}
    }
    @media(min-width:900px){
      main{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:20px}
      main>h1,main>.page-message{grid-column:1/-1}
      main>.panel{box-sizing:border-box;margin:0;padding:16px;background:#25354a;border:1px solid #536174;border-radius:12px}
      #sdLog{max-height:520px}
    }
  </style>
</head>
<body>
  <main>
    <h1>Zegar ESP32</h1>
    <section class="panel">
      <div>Aktualna data i godzina:</div>
      <div id="clock">Ładowanie...</div>
      <form id="form">
        <label for="date">Data</label>
        <input id="date" name="date" type="date" required>
        <label for="time">Godzina</label>
        <input id="time" name="time" type="time" required>
        <button type="submit">Ustaw datę i godzinę</button>
      </form>
    </section>
    <section class="panel">
      <h2>Wi-Fi</h2>
      <button id="scan" type="button">Szukaj sieci</button>
      <label for="ssid">Sieć Wi-Fi</label>
      <select id="ssid" name="ssid" style="box-sizing:border-box;width:100%;padding:12px;border:0;border-radius:8px;font:inherit"></select>
      <label for="password">Hasło (puste dla zapisanej sieci)</label>
      <input id="password" type="password" maxlength="63" autocomplete="new-password">
      <button id="connect" type="button">Połącz i zapisz</button>
      <div id="wifiMessage"></div>
    </section>
    <section class="panel">
      <h2>Opcje urządzenia</h2>
      <div id="deviceStatus"></div>
      <label class="switch"><input id="timeSync" type="checkbox">Synchronizacja czasu NTP</label>
      <label class="switch"><input id="accessPoint" type="checkbox">Punkt dostępowy konfiguracji</label>
      <label class="switch"><input id="autoBrightness" type="checkbox">Automatyczna jasność</label>
      <label class="switch"><input id="stockLed" type="checkbox">Dioda RGB kursu</label>
      <label for="brightness">Jasność ręczna: <span class="value" id="brightnessValue"></span></label>
      <input id="brightness" type="range" min="1" max="100">
      <label for="sensitivity">Czułość automatyki: <span class="value" id="sensitivityValue"></span></label>
      <input id="sensitivity" type="range" min="0" max="100">
      <label for="minimum">Minimalna jasność automatyczna: <span class="value" id="minimumValue"></span></label>
      <input id="minimum" type="range" min="1" max="100">
      <label for="maximum">Maksymalna jasność automatyczna: <span class="value" id="maximumValue"></span></label>
      <input id="maximum" type="range" min="1" max="100">
      <button id="saveSettings" type="button">Zapisz opcje</button>
      <div id="settingsMessage"></div>
    </section>
    <section class="panel">
      <h2>Karta SD</h2>
      <div id="sdStatus">Sprawdzanie karty...</div>
      <button id="refreshSd" type="button">Odśwież stan karty</button>
      <button id="refreshSdLog" type="button">Odśwież podgląd logu</button>
      <pre id="sdLog">Ładowanie logu...</pre>
      <button id="formatSd" class="danger" type="button">Formatuj kartę FAT32</button>
      <div id="sdMessage"></div>
    </section>
    <section class="panel">
      <h2>Dotyk</h2>
      <button id="resetCalibration" type="button">Resetuj kalibrację dotyku</button>
      <div id="calibrationMessage"></div>
    </section>
    <div id="message" class="page-message"></div>
  </main>
  <script>
    async function refresh(){
      try{
        const response=await fetch('/api/time');
        if(!response.ok) throw new Error('Brak czasu');
        const data=await response.json();
        document.getElementById('clock').textContent=data.date+' '+data.time;
        if(!document.getElementById('date').value) document.getElementById('date').value=data.isoDate;
        if(!document.getElementById('time').value) document.getElementById('time').value=data.isoTime;
      }catch(error){document.getElementById('clock').textContent='Czekam na synchronizację czasu';}
    }
    document.getElementById('form').addEventListener('submit',async event=>{
      event.preventDefault();
      const body=new URLSearchParams(new FormData(event.currentTarget));
      const response=await fetch('/api/time',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
      const result=await response.json();
      document.getElementById('message').textContent=result.message;
      if(response.ok) await refresh();
    });
    const ssidSelect=document.getElementById('ssid');
    async function scanNetworks(){
      const button=document.getElementById('scan');
      button.disabled=true;button.textContent='Skanowanie...';
      try{
        await fetch('/api/scan',{method:'POST'});
        let data;
        do{
          await new Promise(resolve=>setTimeout(resolve,800));
          const response=await fetch('/api/networks');
          data=await response.json();
          if(!response.ok) throw new Error(data.message||'Błąd skanowania');
        }while(data.scanning);
        ssidSelect.replaceChildren();
        for(const network of data.networks){
          const option=document.createElement('option');
          option.value=network.ssid;
          option.textContent=network.ssid+(network.saved?' (zapisana)':'')+' '+network.rssi+' dBm';
          ssidSelect.appendChild(option);
        }
        document.getElementById('wifiMessage').textContent=data.message||'Skanowanie zakończone';
      }catch(error){document.getElementById('wifiMessage').textContent=error.message;}
      finally{button.disabled=false;button.textContent='Szukaj sieci';}
    }
    document.getElementById('scan').addEventListener('click',scanNetworks);
    document.getElementById('connect').addEventListener('click',async()=>{
      const body=new URLSearchParams({ssid:ssidSelect.value,password:document.getElementById('password').value});
      const response=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
      const result=await response.json();
      document.getElementById('wifiMessage').textContent=result.message;
      if(response.ok){
        const timer=setInterval(async()=>{
          const status=await fetch('/api/wifi/status');
          const state=await status.json();
          document.getElementById('wifiMessage').textContent=state.message;
          if(!state.connecting){clearInterval(timer);if(state.connected)document.getElementById('password').value='';}
        },1000);
      }
    });
    document.getElementById('resetCalibration').addEventListener('click',async()=>{
      if(!confirm('Usunąć kalibrację i ponownie uruchomić kalibrację na ekranie?')) return;
      const response=await fetch('/api/calibration/reset',{method:'POST'});
      const result=await response.json();
      document.getElementById('calibrationMessage').textContent=result.message;
    });
    const settingInputs={timeSync:'timeSync',accessPoint:'accessPoint',autoBrightness:'autoBrightness',stockLed:'stockLed',
      brightness:'brightness',sensitivity:'sensitivity',minimum:'minimum',maximum:'maximum'};
    function showSettingValues(){
      for(const [id] of Object.entries(settingInputs)){
        const input=document.getElementById(id);
        const output=document.getElementById(id+'Value');
        if(output) output.textContent=input.value+'%';
      }
    }
    async function loadSettings(){
      const response=await fetch('/api/settings');
      const data=await response.json();
      if(!response.ok) throw new Error(data.message||'Nie można pobrać opcji');
      document.getElementById('timeSync').checked=data.timeSync;
      document.getElementById('accessPoint').checked=data.accessPoint;
      document.getElementById('autoBrightness').checked=data.autoBrightness;
      document.getElementById('stockLed').checked=data.stockLed;
      document.getElementById('brightness').value=data.brightness;
      document.getElementById('sensitivity').value=data.sensitivity;
      document.getElementById('minimum').value=data.minimum;
      document.getElementById('maximum').value=data.maximum;
      document.getElementById('deviceStatus').textContent=data.wifi;
      showSettingValues();
    }
    for(const id of ['brightness','sensitivity','minimum','maximum']){
      document.getElementById(id).addEventListener('input',showSettingValues);
    }
    document.getElementById('saveSettings').addEventListener('click',async()=>{
      const body=new URLSearchParams();
      for(const [id,key] of Object.entries(settingInputs)){
        const input=document.getElementById(id);
        body.set(key,input.type==='checkbox'?(input.checked?'1':'0'):input.value);
      }
      const response=await fetch('/api/settings',{method:'POST',
        headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
      const result=await response.json();
      document.getElementById('settingsMessage').textContent=result.message;
      if(response.ok) await loadSettings();
    });
    async function loadSdStatus(){
      const response=await fetch('/api/sd');
      const data=await response.json();
      document.getElementById('sdStatus').textContent=data.message+
        (data.available?' — '+data.totalMb+' MB, zajęte '+data.usedMb+' MB':'');
      document.getElementById('formatSd').disabled=!data.available;
      document.getElementById('refreshSdLog').disabled=!data.filesystem;
    }
    document.getElementById('refreshSd').addEventListener('click',loadSdStatus);
    async function loadSdLog(){
      const output=document.getElementById('sdLog');
      const controller=new AbortController();
      const timeout=setTimeout(()=>controller.abort(),10000);
      output.textContent='Odczyt logu z karty SD...';
      try{
        const response=await fetch('/api/sd/log',{cache:'no-store',signal:controller.signal});
        const data=await response.json();
        if(!response.ok) throw new Error(data.message||'Nie można odczytać logu');
        output.textContent=data.lines.length
          ? data.lines.join('\n')
          : 'Brak wpisów w /log.txt';
      }finally{clearTimeout(timeout);}
    }
    document.getElementById('refreshSdLog').addEventListener('click',()=>{
      loadSdLog().catch(error=>document.getElementById('sdLog').textContent=error.message);
    });
    document.getElementById('formatSd').addEventListener('click',async()=>{
      if(!confirm('Formatowanie bezpowrotnie usunie wszystkie pliki z karty. Kontynuować?')) return;
      const body=new URLSearchParams({confirm:'FORMATUJ'});
      const response=await fetch('/api/sd/format',{method:'POST',
        headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
      const result=await response.json();
      document.getElementById('sdMessage').textContent=result.message;
      await loadSdStatus();
      await loadSdLog().catch(error=>document.getElementById('sdLog').textContent=error.message);
    });
    refresh();
    setInterval(refresh,1000);
    scanNetworks();
    loadSettings().catch(error=>document.getElementById('settingsMessage').textContent=error.message);
    loadSdStatus().catch(error=>document.getElementById('sdStatus').textContent=error.message);
    loadSdLog().catch(error=>document.getElementById('sdLog').textContent=error.message);
  </script>
</body>
</html>
)HTML";

// Strona otwierana automatycznie po połączeniu telefonu z hotspotem zegara.
const char captivePortalPage[] PROGMEM = R"HTML(
<!doctype html>
<html lang="pl">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Konfiguracja MonitorAkcji</title>
  <style>
    body{font:16px system-ui,sans-serif;max-width:420px;margin:40px auto;padding:0 18px;background:#101827;color:#eef4ff}
    main{background:#1c293b;border-radius:16px;padding:24px}
    a{display:block;margin-top:20px;padding:14px;border-radius:8px;background:#087f8c;color:white;text-align:center;text-decoration:none;font-weight:700}
  </style>
</head>
<body>
  <main>
    <h1>MonitorAkcji</h1>
    <p>Połączono z urządzeniem. Otwórz panel, aby skonfigurować zegar i sieć Wi-Fi.</p>
    <a href="/login">Otwórz panel konfiguracji</a>
  </main>
</body>
</html>
)HTML";

// Formularz logowania chroniący panel konfiguracyjny.
const char loginPage[] PROGMEM = R"HTML(
<!doctype html>
<html lang="pl">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Logowanie MonitorAkcji</title>
  <style>
    body{font:16px system-ui,sans-serif;max-width:420px;margin:40px auto;padding:0 18px;background:#101827;color:#eef4ff}
    main{background:#1c293b;border-radius:16px;padding:24px}
    label{display:block;margin:14px 0 6px}
    input,button{box-sizing:border-box;width:100%;padding:12px;border:0;border-radius:8px;font:inherit}
    button{margin-top:20px;background:#087f8c;color:white;font-weight:700}
    #message{min-height:1.5em;color:#ffcc80}
  </style>
</head>
<body>
  <main>
    <h1>Logowanie</h1>
    <form action="/login" method="post">
      <label for="username">Użytkownik</label>
      <input id="username" name="username" value="admin" autocomplete="username" required>
      <label for="password">Hasło</label>
      <input id="password" name="password" type="password" autocomplete="current-password" required>
      <button type="submit">Zaloguj</button>
    </form>
    <div id="message"></div>
  </main>
</body>
</html>
)HTML";

// Sprawdza token sesji wśród ciasteczek przeglądarki.
bool hasValidWebSession() {
  const String cookie = webServer.header("Cookie");
  const String tokenName = "MonitorAkcji=";
  const int tokenPosition = cookie.indexOf(tokenName);
  if (tokenPosition < 0) {
    return false;
  }

  const int tokenStart = tokenPosition + tokenName.length();
  int tokenEnd = cookie.indexOf(';', tokenStart);
  if (tokenEnd < 0) {
    tokenEnd = cookie.length();
  }
  return cookie.substring(tokenStart, tokenEnd) == webSessionToken;
}

// Sprawdza token sesji panelu; żądania bez ważnego tokenu są odrzucane.
bool requireWebAuth() {
  if (hasValidWebSession()) {
    return true;
  }
  webServer.send(401, "application/json", "{\"message\":\"Zaloguj się ponownie\"}");
  return false;
}

// Weryfikuje dane logowania i ustawia ciasteczko sesji.
void handleWebLogin() {
  if (webServer.arg("username") != "admin" ||
      webServer.arg("password") != setupPassword) {
    webServer.send(
        401, "text/html; charset=utf-8",
        "<!doctype html><html lang=\"pl\"><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Logowanie</title><body style=\"font:16px sans-serif;"
        "max-width:420px;margin:40px auto;padding:18px;background:#101827;"
        "color:#eef4ff\"><h1>Nieprawidłowy login lub hasło</h1>"
        "<p>Sprawdź dane i spróbuj ponownie.</p>"
        "<a style=\"color:#40e0d0\" href=\"/login\">Wróć do logowania</a>"
        "</body></html>");
    return;
  }
  webServer.sendHeader("Set-Cookie",
                       "MonitorAkcji=" + webSessionToken +
                           "; Path=/; HttpOnly; SameSite=Lax");
  webServer.sendHeader("Location", "/panel", true);
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(303, "text/plain; charset=utf-8", "Zalogowano. Otwieranie panelu...");
}

// Zabezpiecza tekst przed znakami, które mogłyby uszkodzić odpowiedź JSON.
String jsonEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char character = value[i];
    if (character == '"' || character == '\\') {
      escaped += '\\';
      escaped += character;
    } else if (static_cast<uint8_t>(character) >= 0x20) {
      escaped += character;
    }
  }
  return escaped;
}

// Zwraca aktualny czas urządzenia w formacie JSON.
void handleWebTimeRequest() {
  if (!requireWebAuth()) return;
  struct tm localTime;
  if (!getLocalTime(&localTime, 10)) {
    webServer.send(503, "application/json", "{\"error\":\"Brak zsynchronizowanego czasu\"}");
    return;
  }

  char dateText[11];
  char timeText[9];
  char isoDate[11];
  char isoTime[6];
  strftime(dateText, sizeof(dateText), "%d.%m.%Y", &localTime);
  strftime(timeText, sizeof(timeText), "%H:%M:%S", &localTime);
  strftime(isoDate, sizeof(isoDate), "%Y-%m-%d", &localTime);
  strftime(isoTime, sizeof(isoTime), "%H:%M", &localTime);
  const String response = "{\"date\":\"" + String(dateText) + "\",\"time\":\"" +
                          String(timeText) + "\",\"isoDate\":\"" + String(isoDate) +
                          "\",\"isoTime\":\"" + String(isoTime) + "\"}";
  webServer.send(200, "application/json", response);
}

// Rozpoczyna skanowanie sieci na żądanie z panelu WWW.
void handleWebScanStart() {
  if (!requireWebAuth()) return;
  loadSavedNetworks();
  startNetworkScan();
  webServer.send(202, "application/json", "{\"scanning\":true,\"message\":\"Skanowanie sieci\"}");
}

// Zwraca status skanowania i listę znalezionych sieci.
void handleWebNetworks() {
  if (!requireWebAuth()) return;
  const int scanResult = WiFi.scanComplete();
  if (!networkScanPending && statusMessage == "Skanowanie sieci..." &&
      scanResult != WIFI_SCAN_RUNNING) {
    finishNetworkScan();
  }

  String response = "{\"scanning\":";
  response += isNetworkScanActive() ? "true" : "false";
  response += ",\"message\":\"" + jsonEscape(statusMessage) + "\",\"networks\":[";
  bool first = true;
  for (const Network &network : networks) {
    if (!first) {
      response += ',';
    }
    first = false;
    response += "{\"ssid\":\"" + jsonEscape(network.ssid) + "\",\"saved\":";
    response += network.saved ? "true" : "false";
    response += ",\"rssi\":" + String(network.visible ? network.rssi : 0) + "}";
  }
  response += "]}";
  webServer.send(200, "application/json", response);
}

// Waliduje dane Wi-Fi z panelu i rozpoczyna łączenie.
void handleWebWifiConnect() {
  if (!requireWebAuth()) return;
  const String ssid = webServer.arg("ssid");
  String password = webServer.arg("password");
  if (ssid.isEmpty() || ssid.length() > 32) {
    webServer.send(400, "application/json", "{\"message\":\"Wybierz prawidłową sieć WiFi\"}");
    return;
  }
  if (password.isEmpty() && !loadSavedCredentials(ssid, password)) {
    webServer.send(400, "application/json", "{\"message\":\"Podaj hasło do tej sieci\"}");
    return;
  }
  if (password.length() > 63) {
    webServer.send(400, "application/json", "{\"message\":\"Hasło jest za długie\"}");
    return;
  }

  startConnection(ssid, password, true, false);
  webServer.send(202, "application/json", "{\"message\":\"Rozpoczęto łączenie z siecią\"}");
}

// Zwraca informację o postępie łączenia z routerem.
void handleWebWifiStatus() {
  if (!requireWebAuth()) return;
  const bool connected = WiFi.status() == WL_CONNECTED;
  const String message = connected
                             ? "Połączono: " + WiFi.SSID() + " (" + WiFi.localIP().toString() + ")"
                             : (connectionInProgress ? "Łączenie z siecią..."
                                                     : (statusMessage.isEmpty()
                                                            ? "Brak połączenia"
                                                            : statusMessage));
  const String response = "{\"connecting\":" + String(connectionInProgress ? "true" : "false") +
                          ",\"connected\":" + String(connected ? "true" : "false") +
                          ",\"message\":\"" + jsonEscape(message) + "\"}";
  webServer.send(200, "application/json", response);
}

// Odczytuje całkowitą wartość liczbową i odrzuca puste oraz błędne dane.
bool parseWebInteger(const String &argument, long minimum, long maximum,
                     long &value) {
  if (argument.isEmpty()) {
    return false;
  }
  char *end = nullptr;
  const long parsed = strtol(argument.c_str(), &end, 10);
  if (end == argument.c_str() || *end != '\0' || parsed < minimum ||
      parsed > maximum) {
    return false;
  }
  value = parsed;
  return true;
}

// Zwraca wszystkie przełączniki i poziomy dostępne w opcjach urządzenia.
void handleWebSettingsGet() {
  if (!requireWebAuth()) return;
  const bool connected = WiFi.status() == WL_CONNECTED;
  String wifiStatus =
      connected ? WiFi.SSID() + " — IP " + WiFi.localIP().toString()
                : "Brak połączenia z routerem";
  if (accessPointEnabled) {
    wifiStatus += " — AP " + WiFi.softAPIP().toString();
  }
  wifiStatus += " — czujnik GPIO34: " + String(filteredAmbientLight);

  const String response =
      "{\"timeSync\":" + String(timeSyncEnabled ? "true" : "false") +
      ",\"accessPoint\":" + String(accessPointEnabled ? "true" : "false") +
      ",\"autoBrightness\":" +
      String(autoBacklightEnabled ? "true" : "false") +
      ",\"stockLed\":" + String(stockLedEnabled ? "true" : "false") +
      ",\"brightness\":" +
      String(map(manualBacklightLevel, MinimumBacklight, MaximumBacklight, 1,
                 100)) +
      ",\"sensitivity\":" + String(autoBacklightSensitivity) +
      ",\"minimum\":" +
      String(map(autoBacklightMinimum, MinimumBacklight, MaximumBacklight, 1,
                 100)) +
      ",\"maximum\":" +
      String(map(autoBacklightMaximum, MinimumBacklight, MaximumBacklight, 1,
                 100)) +
      ",\"sensor\":" + String(filteredAmbientLight) +
      ",\"wifi\":\"" + jsonEscape(wifiStatus) + "\"}";
  webServer.send(200, "application/json", response);
}

// Waliduje i zapisuje komplet ustawień pokazywanych w panelu WWW.
void handleWebSettingsUpdate() {
  if (!requireWebAuth()) return;

  long brightnessPercent = 0;
  long sensitivity = 0;
  long minimumPercent = 0;
  long maximumPercent = 0;
  const String timeSyncArgument = webServer.arg("timeSync");
  const String accessPointArgument = webServer.arg("accessPoint");
  const String autoBrightnessArgument = webServer.arg("autoBrightness");
  const String stockLedArgument = webServer.arg("stockLed");
  if ((timeSyncArgument != "0" && timeSyncArgument != "1") ||
      (accessPointArgument != "0" && accessPointArgument != "1") ||
      (autoBrightnessArgument != "0" && autoBrightnessArgument != "1") ||
      (stockLedArgument != "0" && stockLedArgument != "1") ||
      !parseWebInteger(webServer.arg("brightness"), 1, 100,
                       brightnessPercent) ||
      !parseWebInteger(webServer.arg("sensitivity"), 0, 100, sensitivity) ||
      !parseWebInteger(webServer.arg("minimum"), 1, 100, minimumPercent) ||
      !parseWebInteger(webServer.arg("maximum"), 1, 100, maximumPercent) ||
      minimumPercent > maximumPercent) {
    webServer.send(400, "application/json",
                   "{\"message\":\"Nieprawidłowe ustawienia (sprawdź zakresy jasności)\"}");
    return;
  }

  const bool requestedTimeSync = timeSyncArgument == "1";
  const bool requestedAccessPoint = accessPointArgument == "1";
  const bool requestedAutoBrightness = autoBrightnessArgument == "1";
  const bool requestedStockLed = stockLedArgument == "1";
  if (requestedAccessPoint != accessPointEnabled) {
    if (requestedAccessPoint) {
      if (!startSetupAccessPoint()) {
        webServer.send(500, "application/json",
                       "{\"message\":\"Nie udało się uruchomić punktu dostępowego\"}");
        return;
      }
    } else {
      stopSetupAccessPoint();
    }
    accessPointEnabled = requestedAccessPoint;
    preferences.putBool("apEnabled", accessPointEnabled);
  }

  timeSyncEnabled = requestedTimeSync;
  preferences.putBool("timeSync", timeSyncEnabled);
  if (timeSyncEnabled) {
    startNtpSynchronization();
  } else {
    sntp_stop();
  }

  manualBacklightLevel = map(brightnessPercent, 1, 100, MinimumBacklight,
                             MaximumBacklight);
  autoBacklightSensitivity = sensitivity;
  autoBacklightMinimum = map(minimumPercent, 1, 100, MinimumBacklight,
                             MaximumBacklight);
  autoBacklightMaximum = map(maximumPercent, 1, 100, MinimumBacklight,
                             MaximumBacklight);
  autoBacklightEnabled = requestedAutoBrightness;
  stockLedEnabled = requestedStockLed;
  preferences.putUChar("brightness", manualBacklightLevel);
  preferences.putUChar("autoSensitivity", autoBacklightSensitivity);
  preferences.putUChar("autoMin", autoBacklightMinimum);
  preferences.putUChar("autoMax", autoBacklightMaximum);
  preferences.putBool("autoBrightness", autoBacklightEnabled);
  preferences.putBool("stockLed", stockLedEnabled);
  updateBacklight();
  updateStockLedFromQuote();
  queueEventLog(
      "Zmieniono opcje z panelu WWW: NTP " +
      String(timeSyncEnabled ? "on" : "off") + ", AP " +
      String(accessPointEnabled ? "on" : "off") + ", auto jasność " +
      String(autoBacklightEnabled ? "on" : "off") + ", ręczna " +
      String(brightnessPercent) + "%, czułość " + String(sensitivity) +
      "%, zakres " + String(minimumPercent) + "-" +
      String(maximumPercent) + "%, dioda RGB " +
      String(stockLedEnabled ? "on" : "off"));
  webServer.send(200, "application/json",
                 "{\"message\":\"Zapisano opcje urządzenia\"}");
}

// Zwraca status i pojemność karty SD, również gdy trzeba ją dopiero sformatować.
void handleWebSdStatus() {
  if (!requireWebAuth()) return;
  inspectSdCard();
  const String response =
      "{\"available\":" + String(sdCardAvailable ? "true" : "false") +
      ",\"filesystem\":" +
      String(sdCardFilesystemMounted ? "true" : "false") +
      ",\"message\":\"" + jsonEscape(sdCardMessage) + "\",\"totalMb\":" +
      String(static_cast<double>(sdCardTotalBytes) / 1048576.0, 1) +
      ",\"usedMb\":" +
      String(static_cast<double>(sdCardUsedBytes) / 1048576.0, 1) + "}";
  webServer.send(200, "application/json", response);
}

// Zwraca maksymalnie 30 najnowszych wpisów z logu po uwierzytelnieniu.
void handleWebSdLog() {
  if (!requireWebAuth()) return;
  std::vector<String> lines;
  if (!readRecentLogLines(lines, 30)) {
    webServer.send(503, "application/json",
                   "{\"message\":\"Nie można odczytać logu z karty SD\"}");
    return;
  }

  String response = "{\"lines\":[";
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) response += ',';
    response += "\"" + jsonEscape(lines[i]) + "\"";
  }
  response += "]}";
  webServer.send(200, "application/json", response);
}

// Formatuje SD wyłącznie po jawnym potwierdzeniu przesłanym z panelu.
void handleWebSdFormat() {
  if (!requireWebAuth()) return;
  if (webServer.arg("confirm") != "FORMATUJ") {
    webServer.send(400, "application/json",
                   "{\"message\":\"Brak potwierdzenia formatowania\"}");
    return;
  }
  if (!formatSdCard()) {
    webServer.send(500, "application/json",
                   "{\"message\":\"Formatowanie FAT32 nie powiodło się\"}");
    return;
  }
  inspectSdCard();
  webServer.send(200, "application/json",
                 "{\"message\":\"Karta sformatowana jako FAT32\"}");
}

// Usuwa zapisaną kalibrację i rozpoczyna ją ponownie na ekranie urządzenia.
void handleWebCalibrationReset() {
  if (!requireWebAuth()) return;
  preferences.remove("touchXL");
  preferences.remove("touchXR");
  preferences.remove("touchYT");
  preferences.remove("touchYB");
  preferences.remove("touchVer");
  preferences.remove("touchCal");
  touchCalibrated = false;
  touchXLeft = 0;
  touchXRight = 4095;
  touchYTop = 0;
  touchYBottom = 4095;
  queueEventLog("Panel WWW: zresetowano kalibrację dotyku");
  webServer.send(200, "application/json",
                 "{\"message\":\"Kalibracja usunięta. Skonfiguruj dotyk na ekranie urządzenia.\"}");
  beginTouchCalibration(false);
}

// Ustawia zegar z daty i godziny przesłanych przez panel WWW.
void handleWebTimeUpdate() {
  if (!requireWebAuth()) return;
  int year, month, day, hour, minute;
  const String date = webServer.arg("date");
  const String time = webServer.arg("time");
  if (sscanf(date.c_str(), "%4d-%2d-%2d", &year, &month, &day) != 3 ||
      sscanf(time.c_str(), "%2d:%2d", &hour, &minute) != 2) {
    webServer.send(400, "application/json", "{\"message\":\"Nieprawidłowa data lub godzina\"}");
    return;
  }

  struct tm localTime = {};
  localTime.tm_year = year - 1900;
  localTime.tm_mon = month - 1;
  localTime.tm_mday = day;
  localTime.tm_hour = hour;
  localTime.tm_min = minute;
  localTime.tm_isdst = -1;
  const time_t epoch = mktime(&localTime);
  char normalizedDate[11];
  char normalizedTime[6];
  strftime(normalizedDate, sizeof(normalizedDate), "%Y-%m-%d", &localTime);
  strftime(normalizedTime, sizeof(normalizedTime), "%H:%M", &localTime);
  if (epoch <= 0 || String(normalizedDate) != date || String(normalizedTime) != time) {
    webServer.send(400, "application/json", "{\"message\":\"Nieprawidłowa data lub godzina\"}");
    return;
  }

  timeval newTime = {};
  newTime.tv_sec = epoch;
  if (settimeofday(&newTime, nullptr) != 0) {
    webServer.send(500, "application/json", "{\"message\":\"Nie udalo sie ustawic czasu\"}");
    return;
  }
  preferences.putULong64("manualTime", static_cast<uint64_t>(epoch));
  if (WiFi.status() == WL_CONNECTED) {
    startNtpSynchronization();
  }
  queueEventLog("Panel WWW: ustawiono datę i godzinę");
  webServer.send(200, "application/json", "{\"message\":\"Ustawiono date i godzine\"}");
}

// Kieruje przeglądarkę do panelu konfiguracji hotspotu.
void redirectToCaptivePortal() {
  if (!accessPointEnabled) {
    webServer.send(404, "text/plain; charset=utf-8", "Nie znaleziono strony");
    return;
  }
  webServer.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(302, "text/plain; charset=utf-8", "");
}

// Obsługuje adresy wykrywania captive portalu w telefonach.
void handleCaptivePortal() {
  if (!accessPointEnabled) {
    webServer.sendHeader("Location", "/login", true);
    webServer.sendHeader("Cache-Control", "no-store");
    webServer.send(302, "text/plain; charset=utf-8", "");
    return;
  }
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send_P(200, "text/html; charset=utf-8", captivePortalPage);
}

// Rejestruje strony i API panelu, a następnie uruchamia serwer HTTP.
void beginWebServer() {
  const char *requestHeaders[] = {"Cookie"};
  webServer.collectHeaders(requestHeaders, 1);
  webServer.on("/", HTTP_GET, handleCaptivePortal);
  webServer.on("/login", HTTP_GET, []() {
    webServer.send_P(200, "text/html; charset=utf-8", loginPage);
  });
  webServer.on("/login", HTTP_POST, handleWebLogin);
  webServer.on("/panel", HTTP_GET, []() {
    if (!hasValidWebSession()) {
      webServer.sendHeader("Location", "/login", true);
      webServer.sendHeader("Cache-Control", "no-store");
      webServer.send(302, "text/plain; charset=utf-8", "");
      return;
    }
    webServer.send_P(200, "text/html; charset=utf-8", webPage);
  });
  webServer.on("/api/time", HTTP_GET, handleWebTimeRequest);
  webServer.on("/api/time", HTTP_POST, handleWebTimeUpdate);
  webServer.on("/api/scan", HTTP_POST, handleWebScanStart);
  webServer.on("/api/networks", HTTP_GET, handleWebNetworks);
  webServer.on("/api/wifi", HTTP_POST, handleWebWifiConnect);
  webServer.on("/api/wifi/status", HTTP_GET, handleWebWifiStatus);
  webServer.on("/api/settings", HTTP_GET, handleWebSettingsGet);
  webServer.on("/api/settings", HTTP_POST, handleWebSettingsUpdate);
  webServer.on("/api/sd", HTTP_GET, handleWebSdStatus);
  webServer.on("/api/sd/log", HTTP_GET, handleWebSdLog);
  webServer.on("/api/sd/format", HTTP_POST, handleWebSdFormat);
  webServer.on("/api/calibration/reset", HTTP_POST, handleWebCalibrationReset);
  const char *captivePortalPaths[] = {
      "/generate_204", "/gen_204", "/hotspot-detect.html",
      "/connecttest.txt", "/ncsi.txt", "/canonical.html", "/success.txt"};
  for (const char *path : captivePortalPaths) {
    webServer.on(path, HTTP_ANY, handleCaptivePortal);
  }
  webServer.onNotFound([]() {
    redirectToCaptivePortal();
  });
  webServer.begin();
}

// Pokazuje kolejny z czterech punktów potrzebnych do kalibracji dotyku.
void drawCalibrationScreen() {
  currentScreen = Screen::Calibration;
  tft.fillScreen(TFT_BLACK);
  drawHeader("KALIBRACJA");

  constexpr int16_t targetX[] = {15, 304, 304, 15};
  constexpr int16_t targetY[] = {15, 15, 224, 224};
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("Dotknij kolejno zaznaczone punkty", ScreenWidth / 2, 105, 2);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawCentreString(String(calibrationStep + 1) + "/4", ScreenWidth / 2, 132, 2);
  if (accessPointEnabled) {
    tft.drawCentreString("WiFi: " + setupSsid, ScreenWidth / 2, 165, 1);
    tft.drawCentreString("Hasło: " + setupPassword, ScreenWidth / 2, 180, 1);
    tft.drawCentreString("Wejdź na http://192.168.4.1", ScreenWidth / 2, 195, 1);
  } else {
    tft.drawCentreString("Punkt dostępowy wyłączony", ScreenWidth / 2, 180, 1);
  }
  tft.drawCircle(targetX[calibrationStep], targetY[calibrationStep], 12, TFT_YELLOW);
  tft.drawFastHLine(targetX[calibrationStep] - 18, targetY[calibrationStep], 37, TFT_YELLOW);
  tft.drawFastVLine(targetX[calibrationStep], targetY[calibrationStep] - 18, 37, TFT_YELLOW);
}

// Zeruje postęp kalibracji i wyświetla pierwszy punkt.
void beginTouchCalibration(bool fromOptions) {
  calibrationFromOptions = fromOptions;
  calibrationStep = 0;
  calibrationReleasedAt = 0;
  calibrationTouchArmed = false;
  queueEventLog("Rozpoczęto kalibrację dotyku");
  drawCalibrationScreen();
}

// Oblicza mapowanie ekranu, sprawdza jego poprawność i zapisuje je w Preferences.
void finishTouchCalibration() {
  const int32_t calibratedXLeft =
      (static_cast<int32_t>(calibrationRawX[0]) + calibrationRawX[3]) / 2;
  const int32_t calibratedXRight =
      (static_cast<int32_t>(calibrationRawX[1]) + calibrationRawX[2]) / 2;
  const int32_t calibratedYTop =
      (static_cast<int32_t>(calibrationRawY[0]) + calibrationRawY[1]) / 2;
  const int32_t calibratedYBottom =
      (static_cast<int32_t>(calibrationRawY[2]) + calibrationRawY[3]) / 2;

  if (abs(calibratedXRight - calibratedXLeft) < 200 ||
      abs(calibratedYBottom - calibratedYTop) < 200) {
    calibrationStep = 0;
    statusMessage = "Kalibracja nieudana. Spróbuj ponownie.";
    queueEventLog("BŁĄD: kalibracja dotyku nie powiodła się");
    drawCalibrationScreen();
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawCentreString(statusMessage, ScreenWidth / 2, 155, 1);
    return;
  }

  touchXLeft = calibratedXLeft;
  touchXRight = calibratedXRight;
  touchYTop = calibratedYTop;
  touchYBottom = calibratedYBottom;
  preferences.putInt("touchXL", touchXLeft);
  preferences.putInt("touchXR", touchXRight);
  preferences.putInt("touchYT", touchYTop);
  preferences.putInt("touchYB", touchYBottom);
  preferences.putUChar("touchVer", TouchCalibrationVersion);
  preferences.putBool("touchCal", true);
  touchCalibrated = true;
  statusMessage = "";
  queueEventLog("Zakończono kalibrację dotyku");
  drawClockScreen();

  if (!calibrationFromOptions) {
    connectLastSavedNetwork();
  }
}

// Zapisuje surowy pomiar dotyku dla bieżącego punktu kalibracyjnego.
void handleCalibrationTouch(int16_t rawX, int16_t rawY) {
  calibrationRawX[calibrationStep] = static_cast<uint16_t>(rawX);
  calibrationRawY[calibrationStep] = static_cast<uint16_t>(rawY);
  ++calibrationStep;
  if (calibrationStep >= 4) {
    finishTouchCalibration();
  } else {
    drawCalibrationScreen();
  }
}

// Rysuje klawiaturę ekranową do wpisywania hasła Wi-Fi.
void drawKeyboard() {
  const String lowerRows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm.,-"};
  const String upperRows[] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM.,_"};
  const String symbolRows[] = {"1234567890", "@#$%&*-+()", "!?/.:,;_'\""};
  const String *rows = passwordSymbols ? symbolRows : (passwordUppercase ? upperRows : lowerRows);

  tft.fillScreen(TFT_BLACK);
  drawHeader("HASLO WIFI", "ANULUJ");
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(selectedSsid.substring(0, 27), 8, 36, 2);
  tft.fillRoundRect(8, 57, 304, 26, 4, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  String visiblePassword = passwordInput;
  if (visiblePassword.length() > 48) {
    visiblePassword = visiblePassword.substring(visiblePassword.length() - 48);
  }
  tft.drawString(visiblePassword, 15, 64, 1);

  for (uint8_t row = 0; row < 3; ++row) {
    const int16_t y = 88 + row * 33;
    const int16_t keyWidth = 30;
    for (uint8_t column = 0; column < 10; ++column) {
      const String key( rows[row][column] );
      const int16_t x = 10 + column * keyWidth;
      tft.fillRoundRect(x, y, 28, 29, 3, TFT_NAVY);
      tft.drawRoundRect(x, y, 28, 29, 3, TFT_LIGHTGREY);
      tft.setTextColor(TFT_WHITE, TFT_NAVY);
      tft.drawCentreString(key, x + 14, y + 7, 2);
    }
  }

  drawButton(8, 190, 57, 32, "SHIFT", TFT_DARKGREY);
  drawButton(70, 190, 47, 32, passwordSymbols ? "ABC" : "123", TFT_DARKGREY);
  drawButton(122, 190, 72, 32, "SPACJA", TFT_DARKGREY);
  drawButton(199, 190, 48, 32, "USUN", TFT_MAROON);
  drawButton(252, 190, 60, 32, "OK", TFT_DARKGREEN);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawCentreString("Dotknij OK, aby połączyć i zapamiętać", ScreenWidth / 2, 226, 1);
}

// Przygotowuje pusty formularz hasła dla wskazanej sieci.
void beginPasswordEntry(const String &ssid) {
  selectedSsid = ssid;
  passwordInput = "";
  passwordUppercase = false;
  passwordSymbols = false;
  currentScreen = Screen::Password;
  drawKeyboard();
}

// Obsługuje dotknięcia listy sieci, przycisku skanowania i stronicowania.
void handleNetworkTap(uint16_t x, uint16_t y) {
  if (y < 32 && x > 205) {
    startNetworkScan();
    statusMessage = "Skanowanie sieci...";
    drawNetworkScreen();
    return;
  }
  if (y >= 213) {
    if (x < 70 && networkPage > 0) {
      --networkPage;
      queueEventLog("Wyświetlono poprzednią stronę sieci WiFi");
      drawNetworkScreen();
    } else if (x > 245) {
      const size_t pageCount = max(static_cast<size_t>(1),
                                   (networks.size() + NetworksPerPage - 1) / NetworksPerPage);
      if (networkPage + 1 < pageCount) {
        ++networkPage;
        queueEventLog("Wyświetlono następną stronę sieci WiFi");
        drawNetworkScreen();
      }
    }
    return;
  }
  if (y >= 57 && y < 212) {
    const size_t row = (y - 57) / 31;
    const size_t index = networkPage * NetworksPerPage + row;
    if (index < networks.size() && y < 85 + row * 31) {
      const Network network = networks[index];
      if (network.saved && x >= 246) {
        forgetSavedNetwork(network.ssid);
        drawNetworkScreen();
      } else if (x < 246 && network.saved) {
        queueEventLog("Wybrano zapisaną sieć WiFi: " + network.ssid);
        connectSavedNetwork(network.ssid);
      } else if (x < 246) {
        queueEventLog("Wybrano sieć WiFi do konfiguracji: " + network.ssid);
        beginPasswordEntry(network.ssid);
      }
    }
  }
}

// Obsługuje klawisze, kasowanie i zatwierdzenie hasła Wi-Fi.
void handlePasswordTap(uint16_t x, uint16_t y) {
  if (y < 32 && x > 230) {
    drawNetworkScreen();
    return;
  }

  if (y >= 88 && y < 187) {
    const uint8_t row = (y - 88) / 33;
    const uint8_t column = min(static_cast<uint16_t>(9), static_cast<uint16_t>((x - 10) / 30));
    if (x >= 10 && row < 3 && passwordInput.length() < 63) {
      const String lowerRows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm.,-"};
      const String upperRows[] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM.,_"};
      const String symbolRows[] = {"1234567890", "@#$%&*-+()", "!?/.:,;_'\""};
      const String *rows = passwordSymbols ? symbolRows : (passwordUppercase ? upperRows : lowerRows);
      passwordInput += rows[row][column];
      drawKeyboard();
    }
    return;
  }

  if (y >= 190 && y < 222) {
    if (x < 67) {
      passwordUppercase = !passwordUppercase;
      drawKeyboard();
    } else if (x < 119) {
      passwordSymbols = !passwordSymbols;
      drawKeyboard();
    } else if (x < 196) {
      if (passwordInput.length() < 63) {
        passwordInput += ' ';
      }
      drawKeyboard();
    } else if (x < 250) {
      if (!passwordInput.isEmpty()) {
        passwordInput.remove(passwordInput.length() - 1);
      }
      drawKeyboard();
    } else if (x >= 250) {
      startConnection(selectedSsid, passwordInput, true);
    }
  }
}

// Włącza wybraną funkcję z menu opcji.
void handleOptionsTap(uint16_t x, uint16_t y) {
  if (y < 32) {
    if (x < 28) {
      returnToPreviousScreen();
    } else if (x > 230) {
      drawClockScreen();
    }
    return;
  }
  if (y < OptionsRowTop ||
      y >= OptionsRowTop + OptionsScrollViewportHeight) {
    return;
  }

  const size_t item =
      static_cast<size_t>((y - OptionsRowTop + optionsScrollOffset) /
                          OptionsRowHeight);
  if (item >= OptionsItemCount) {
    return;
  }

  switch (item) {
    case 0:
      queueEventLog("Otwarto wybór sieci WiFi");
      loadSavedNetworks();
      startNetworkScan();
      drawNetworkScreen();
      break;
    case 1:
      queueEventLog("Wybrano kalibrację dotyku");
      beginTouchCalibration(true);
      break;
    case 2:
      timeSyncEnabled = !timeSyncEnabled;
      preferences.putBool("timeSync", timeSyncEnabled);
      queueEventLog(String("Synchronizacja czasu NTP: ") +
                    (timeSyncEnabled ? "włączona" : "wyłączona"));
      if (timeSyncEnabled) {
        startNtpSynchronization();
      } else {
        sntp_stop();
        Serial.println("NTP synchronization stopped");
      }
      drawOptionsScreen();
      break;
    case 3:
      accessPointEnabled = !accessPointEnabled;
      preferences.putBool("apEnabled", accessPointEnabled);
      queueEventLog(String("Punkt dostępowy: ") +
                    (accessPointEnabled ? "włączony" : "wyłączony"));
      if (accessPointEnabled) {
        if (!startSetupAccessPoint()) {
          accessPointEnabled = false;
          preferences.putBool("apEnabled", false);
          WiFi.mode(WIFI_STA);
        }
      } else {
        stopSetupAccessPoint();
      }
      drawOptionsScreen();
      break;
    case 4:
      queueEventLog("Wyświetlono informacje WiFi i IP");
      drawWifiInfoScreen();
      break;
    case 5:
      queueEventLog("Otwarto ustawienia podświetlenia");
      drawBrightnessScreen();
      break;
    case 6:
      autoBacklightEnabled = !autoBacklightEnabled;
      preferences.putBool("autoBrightness", autoBacklightEnabled);
      queueEventLog(String("Automatyczna jasność: ") +
                    (autoBacklightEnabled ? "włączona" : "wyłączona"));
      updateBacklight();
      drawOptionsScreen();
      break;
    case 7:
      queueEventLog("Otwarto obsługę karty SD");
      drawSdCardScreen();
      break;
    case 8:
      stockLedEnabled = !stockLedEnabled;
      preferences.putBool("stockLed", stockLedEnabled);
      updateStockLedFromQuote();
      queueEventLog(String("Dioda RGB kursu: ") +
                    (stockLedEnabled ? "włączona" : "wyłączona"));
      drawOptionsScreen();
      break;
    default:
      break;
  }
}

// Obsługuje odświeżanie statusu karty oraz dwustopniowe potwierdzenie formatu.
void handleSdCardTap(uint16_t x, uint16_t y) {
  if (currentScreen == Screen::SdCard) {
    if (y < 32 && x > 230) {
      drawSdCardScreen();
    } else if (y >= 153 && y < 187 && sdCardFilesystemMounted) {
      queueEventLog("Wyświetlono podgląd logu karty SD");
      drawSdLogScreen();
    } else if (y >= 194 && y < 228 && sdCardAvailable) {
      drawSdFormatConfirmation();
    }
    return;
  }

  if (currentScreen == Screen::SdLog) {
    if (y < 32 && x > 230) {
      drawSdLogScreen();
    } else if (y >= 211 && y < 235) {
      if (x < 160 && sdLogPageStart >= 15) {
        sdLogPageStart -= 15;
        drawSdLogScreen(false);
      } else if (x >= 160 &&
                 sdLogPageStart + 15 < sdLogLines.size()) {
        sdLogPageStart += 15;
        drawSdLogScreen(false);
      }
    }
    return;
  }

  if (currentScreen != Screen::SdCardFormatConfirm || y < 180 || y >= 225) {
    return;
  }
  if (x < 160) {
    queueEventLog("Anulowano formatowanie karty SD");
    drawSdCardScreen();
    return;
  }

  tft.fillScreen(TFT_BLACK);
  drawHeader("FORMATOWANIE SD");
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawCentreString("Formatowanie... nie wyjmuj karty",
                       ScreenWidth / 2, 110, 2);
  const bool formatted = formatSdCard();
  inspectSdCard();
  sdCardMessage = formatted ? "Sformatowano jako FAT32"
                            : "Formatowanie nie powiodło się";
  drawSdCardScreen(false);
}

// Odczytuje dotyk i przekazuje go do obsługi bieżącego ekranu.
void handleTouch() {
  const bool down = touch.touched();
  if (currentScreen == Screen::Calibration) {
    if (!down) {
      if (!calibrationTouchArmed) {
        if (calibrationReleasedAt == 0) {
          calibrationReleasedAt = millis();
        } else if (millis() - calibrationReleasedAt >= 180) {
          calibrationTouchArmed = true;
        }
      }
    } else if (calibrationTouchArmed && millis() - lastTouchAt > 250) {
      const TS_Point point = touch.getPoint();
      calibrationTouchArmed = false;
      calibrationReleasedAt = 0;
      lastTouchAt = millis();
      handleCalibrationTouch(point.x, point.y);
    }
    touchWasDown = down;
    return;
  }

  if (currentScreen == Screen::Brightness) {
    if (down) {
      if (!touchWasDown || millis() - lastBrightnessTouchAt >= 35) {
        const TS_Point point = touch.getPoint();
        const uint16_t x = static_cast<uint16_t>(constrain(
            map(point.x, touchXLeft, touchXRight, CalibrationScreenLeft,
                CalibrationScreenRight),
            0, ScreenWidth - 1));
        const uint16_t y = static_cast<uint16_t>(constrain(
            map(point.y, touchYTop, touchYBottom, CalibrationScreenTop,
                CalibrationScreenBottom),
            0, ScreenHeight - 1));

        if (!touchWasDown && millis() - lastTouchAt > 250) {
          lastTouchAt = millis();
          if (y < 32 && (x < 28 || x > 230)) {
            returnToPreviousScreen();
            touchWasDown = true;
            return;
          }
          if (y >= 197 && y < 227) {
            autoBacklightEnabled = !autoBacklightEnabled;
            preferences.putBool("autoBrightness", autoBacklightEnabled);
            queueEventLog(String("Automatyczna jasność: ") +
                          (autoBacklightEnabled ? "włączona" : "wyłączona"));
            updateBacklight();
            drawBrightnessScreen();
          }
        }
        if (y >= 47 && y < 75) {
          if (autoBacklightEnabled) {
            autoBacklightEnabled = false;
            preferences.putBool("autoBrightness", false);
            queueEventLog(
                "Automatyczna jasność wyłączona przez zmianę poziomu ręcznego");
            drawBrightnessScreen();
          }
          const uint8_t level = static_cast<uint8_t>(constrain(
              map(x, 30, 290, MinimumBacklight, MaximumBacklight),
              MinimumBacklight, MaximumBacklight));
          if (level != manualBacklightLevel) {
            manualBacklightLevel = level;
            brightnessValueChanged = true;
            if (!autoBacklightEnabled) {
              setBacklightLevel(manualBacklightLevel);
            }
            drawBrightnessControls();
          }
        } else if (y >= 87 && y < 116) {
          const uint8_t sensitivity = static_cast<uint8_t>(constrain(
              map(x, 30, 290, 0, 100), 0, 100));
          if (sensitivity != autoBacklightSensitivity) {
            autoBacklightSensitivity = sensitivity;
            brightnessSensitivityChanged = true;
            drawBrightnessControls();
            if (autoBacklightEnabled) {
              updateBacklight();
            }
          }
        } else if (y >= 127 && y < 156) {
          const uint8_t minimum = static_cast<uint8_t>(constrain(
              map(x, 30, 290, MinimumBacklight, MaximumBacklight),
              MinimumBacklight, MaximumBacklight));
          if (minimum != autoBacklightMinimum) {
            autoBacklightMinimum = minimum;
            if (autoBacklightMaximum < autoBacklightMinimum) {
              autoBacklightMaximum = autoBacklightMinimum;
            }
            brightnessAutoRangeChanged = true;
            drawBrightnessControls();
            if (autoBacklightEnabled) {
              updateBacklight();
            }
          }
        } else if (y >= 167 && y < 196) {
          const uint8_t maximum = static_cast<uint8_t>(constrain(
              map(x, 30, 290, MinimumBacklight, MaximumBacklight),
              MinimumBacklight, MaximumBacklight));
          if (maximum != autoBacklightMaximum) {
            autoBacklightMaximum = maximum;
            if (autoBacklightMinimum > autoBacklightMaximum) {
              autoBacklightMinimum = autoBacklightMaximum;
            }
            brightnessAutoRangeChanged = true;
            drawBrightnessControls();
            if (autoBacklightEnabled) {
              updateBacklight();
            }
          }
        }
        lastBrightnessTouchAt = millis();
      }
      touchWasDown = true;
      return;
    }

    if (touchWasDown && brightnessValueChanged) {
      preferences.putUChar("brightness", manualBacklightLevel);
      queueEventLog("Ustawiono jasność ręczną: " +
                    String(map(manualBacklightLevel, MinimumBacklight,
                               MaximumBacklight, 1, 100)) +
                    "%");
      brightnessValueChanged = false;
    }
    if (touchWasDown && brightnessSensitivityChanged) {
      preferences.putUChar("autoSensitivity", autoBacklightSensitivity);
      queueEventLog("Ustawiono czułość automatyki jasności: " +
                    String(autoBacklightSensitivity) + "%");
      brightnessSensitivityChanged = false;
    }
    if (touchWasDown && brightnessAutoRangeChanged) {
      preferences.putUChar("autoMin", autoBacklightMinimum);
      preferences.putUChar("autoMax", autoBacklightMaximum);
      queueEventLog(
          "Ustawiono zakres automatycznej jasności: min " +
          String(map(autoBacklightMinimum, MinimumBacklight, MaximumBacklight,
                     1, 100)) +
          "%, max " +
          String(map(autoBacklightMaximum, MinimumBacklight, MaximumBacklight,
                     1, 100)) +
          "%");
      brightnessAutoRangeChanged = false;
    }
    touchWasDown = false;
    return;
  }

  if (currentScreen == Screen::Options) {
    if (down) {
      if (!optionsTouchActive && !touchWasDown &&
          millis() - lastTouchAt > 250) {
        const TS_Point point = touch.getPoint();
        optionsTouchStartX = static_cast<uint16_t>(constrain(
            map(point.x, touchXLeft, touchXRight, CalibrationScreenLeft,
                CalibrationScreenRight),
            0, ScreenWidth - 1));
        optionsTouchStartY = static_cast<int16_t>(constrain(
            map(point.y, touchYTop, touchYBottom, CalibrationScreenTop,
                CalibrationScreenBottom),
            0, ScreenHeight - 1));
        optionsTouchLastY = optionsTouchStartY;
        optionsTouchSampleAt = millis();
        optionsTouchMoved = false;
        optionsScrollAnimating = false;
        optionsScrollVelocity = 0.0f;
        optionsPressedItem = -1;
        optionsTouchActive = true;
        if (optionsTouchStartX >= 250 &&
            optionsTouchStartY >= OptionsRowTop &&
            optionsTouchStartY <
                OptionsRowTop + OptionsScrollViewportHeight) {
          const size_t item = static_cast<size_t>(
              (optionsTouchStartY - OptionsRowTop + optionsScrollOffset) /
              OptionsRowHeight);
          if (item < OptionsItemCount) {
            optionsPressedItem = static_cast<int16_t>(item);
            drawOptionsContent();
            optionsLastRenderAt = millis();
          }
        }
      } else if (optionsTouchActive) {
        const TS_Point point = touch.getPoint();
        const int16_t currentY = static_cast<int16_t>(constrain(
            map(point.y, touchYTop, touchYBottom, CalibrationScreenTop,
                CalibrationScreenBottom),
            0, ScreenHeight - 1));
        if (optionsTouchStartY >= OptionsRowTop &&
            optionsTouchStartY <
                OptionsRowTop + OptionsScrollViewportHeight) {
          const int16_t movement = optionsTouchLastY - currentY;
          if (abs(currentY - optionsTouchStartY) > OptionsTapSlop) {
            optionsTouchMoved = true;
            if (optionsPressedItem >= 0) {
              optionsPressedItem = -1;
              drawOptionsContent();
              optionsLastRenderAt = millis();
            }
          }
          if (optionsTouchMoved && movement != 0) {
            const uint32_t now = millis();
            const uint32_t elapsed = max(
                static_cast<uint32_t>(1), now - optionsTouchSampleAt);
            optionsScrollOffset = constrain(
                optionsScrollOffset + movement, 0, maxOptionsScrollOffset());
            optionsScrollVelocity = constrain(
                static_cast<float>(movement) * 1000.0f / elapsed, -1800.0f,
                1800.0f);
            if (optionsTouchMoved &&
                now - optionsLastRenderAt >= OptionsScrollFrameIntervalMs) {
              drawOptionsContent();
              optionsLastRenderAt = now;
            }
            optionsTouchSampleAt = now;
          }
        }
        optionsTouchLastY = currentY;
      }
      touchWasDown = true;
      return;
    }

    if (optionsTouchActive && touchWasDown) {
      if (optionsTouchMoved) {
        optionsScrollAnimating = fabsf(optionsScrollVelocity) >= 100.0f;
        optionsScrollAnimationAt = millis();
        if (!optionsScrollAnimating) {
          optionsScrollVelocity = 0.0f;
          drawOptionsContent();
          optionsLastRenderAt = millis();
        }
      } else if (optionsTouchStartY < OptionsRowTop ||
                 (optionsTouchStartX >= 250 && optionsPressedItem >= 0 &&
                  optionsTouchLastY >= OptionsRowTop &&
                  optionsTouchLastY <
                      OptionsRowTop + OptionsScrollViewportHeight &&
                  static_cast<size_t>(
                      (optionsTouchLastY - OptionsRowTop +
                       optionsScrollOffset) /
                      OptionsRowHeight) ==
                      static_cast<size_t>(optionsPressedItem))) {
        optionsPressedItem = -1;
        handleOptionsTap(optionsTouchStartX, optionsTouchStartY);
      } else {
        optionsPressedItem = -1;
      }
      optionsTouchActive = false;
      optionsPressedItem = -1;
      lastTouchAt = millis();
    }
    touchWasDown = false;
    return;
  }

  if (down && !touchWasDown && millis() - lastTouchAt > 250) {
    const TS_Point point = touch.getPoint();
    const uint16_t x = static_cast<uint16_t>(constrain(
        map(point.x, touchXLeft, touchXRight, CalibrationScreenLeft,
            CalibrationScreenRight),
        0, ScreenWidth - 1));
    const uint16_t y = static_cast<uint16_t>(constrain(
        map(point.y, touchYTop, touchYBottom, CalibrationScreenTop,
            CalibrationScreenBottom),
        0, ScreenHeight - 1));

    Serial.printf("Touch x=%u y=%u raw=%d,%d\n", x, y, point.x, point.y);
    lastTouchAt = millis();

    if (y < 32 && x < 28 && currentScreen != Screen::Clock &&
        !(currentScreen == Screen::Calibration && !touchCalibrated)) {
      returnToPreviousScreen();
      touchWasDown = down;
      return;
    }

    if (currentScreen == Screen::Clock) {
      if (y < 34 && x >= 245 && x < 284) {
        drawOptionsScreen();
      } else if (y < 34 && x >= 284) {
        drawWifiInfoScreen();
      }
    } else if (currentScreen == Screen::WifiInfo) {
      if (y < 32 && x > 230) {
        drawOptionsScreen();
      }
    } else if (currentScreen == Screen::Networks) {
      handleNetworkTap(x, y);
    } else if (currentScreen == Screen::Password) {
      handlePasswordTap(x, y);
    } else if (currentScreen == Screen::SdCard ||
               currentScreen == Screen::SdLog ||
               currentScreen == Screen::SdCardFormatConfirm) {
      handleSdCardTap(x, y);
    }
  }
  touchWasDown = down;
}

// Zapisuje udane połączenie Wi-Fi albo zgłasza przekroczenie limitu czasu.
void updateConnection() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected) {
    wifiWasConnected = true;
  } else if (wifiWasConnected && !connectionInProgress) {
    queueEventLog("BŁĄD: utracono połączenie z siecią WiFi");
    wifiWasConnected = false;
  }

  if (!connectionInProgress) {
    return;
  }
  if (connected) {
    connectionInProgress = false;
    statusMessage = "";
    if (rememberAfterConnect && !selectedSsid.isEmpty()) {
      saveCredentials(selectedSsid, pendingPassword);
    }
    rememberAfterConnect = false;
    pendingPassword = "";
    Serial.printf("WiFi connected: %s, IP=%s\n", WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str());
    queueEventLog("Połączono z WiFi: " + WiFi.SSID() + ", IP " +
                  WiFi.localIP().toString());
    startNtpSynchronization();
  } else if (millis() - connectStartedAt > ConnectTimeoutMs) {
    connectionInProgress = false;
    statusMessage = "Nie udało się połączyć z " + selectedSsid;
    wifiWasConnected = false;
    WiFi.disconnect();
    Serial.println(statusMessage);
    queueEventLog("BŁĄD: nie udało się połączyć z WiFi " + selectedSsid);
  }
}

// Odczytuje tablicę liczb Yahoo; puste wartości null zamienia na NAN.
bool parseYahooArray(const String &json, const char *key,
                     std::vector<double> &values) {
  const String keyToken = "\"" + String(key) + "\"";
  const int keyPosition = json.indexOf(keyToken);
  if (keyPosition < 0) {
    return false;
  }
  const int arrayStart = json.indexOf('[', keyPosition + keyToken.length());
  if (arrayStart < 0) {
    return false;
  }
  const int arrayEnd = json.indexOf(']', arrayStart + 1);
  if (arrayEnd < 0) {
    return false;
  }

  const char *data = json.c_str();
  int cursor = arrayStart + 1;
  while (cursor < arrayEnd) {
    while (cursor < arrayEnd &&
           (data[cursor] == ' ' || data[cursor] == '\r' ||
            data[cursor] == '\n' || data[cursor] == '\t' ||
            data[cursor] == ',')) {
      ++cursor;
    }
    if (cursor >= arrayEnd) {
      break;
    }

    if (strncmp(data + cursor, "null", 4) == 0) {
      values.push_back(NAN);
      cursor += 4;
    } else {
      char *valueEnd = nullptr;
      const double value = strtod(data + cursor, &valueEnd);
      if (valueEnd == data + cursor || valueEnd > data + arrayEnd ||
          !isfinite(value)) {
        return false;
      }
      values.push_back(value);
      cursor = static_cast<int>(valueEnd - data);
    }

    while (cursor < arrayEnd &&
           (data[cursor] == ' ' || data[cursor] == '\r' ||
            data[cursor] == '\n' || data[cursor] == '\t')) {
      ++cursor;
    }
    if (cursor < arrayEnd && data[cursor] != ',') {
      return false;
    }
  }
  return !values.empty();
}

// Odczytuje pojedyncze pole liczbowe z odpowiedzi JSON Yahoo.
bool parseYahooNumber(const String &json, const char *key, double &value) {
  const String keyToken = "\"" + String(key) + "\"";
  const int keyPosition = json.indexOf(keyToken);
  if (keyPosition < 0) {
    return false;
  }
  const int valueStart = json.indexOf(':', keyPosition + keyToken.length());
  if (valueStart < 0) {
    return false;
  }

  const char *data = json.c_str();
  const char *numberStart = data + valueStart + 1;
  while (*numberStart == ' ' || *numberStart == '\r' || *numberStart == '\n' ||
         *numberStart == '\t') {
    ++numberStart;
  }
  char *numberEnd = nullptr;
  const double parsedValue = strtod(numberStart, &numberEnd);
  if (numberEnd == numberStart || !isfinite(parsedValue)) {
    return false;
  }
  value = parsedValue;
  return true;
}

// Pobiera kurs, czas ostatniej transakcji i pięciominutowe próbki sesji.
bool fetchYahooIntraday(String &failureStatus) {
  WiFiClientSecure secureClient;
  secureClient.setCACert(YahooRootCa);

  HTTPClient request;
  if (!request.begin(secureClient, YahooHistoryUrl)) {
    Serial.println("Yahoo chart request could not be initialized");
    failureStatus = "Błąd połączenia Yahoo";
    return false;
  }
  request.setUserAgent("Mozilla/5.0 (ESP32; MonitorAkcji)");
  request.addHeader("Accept", "application/json");
  request.addHeader("Accept-Encoding", "identity");
  request.setConnectTimeout(7000);
  request.setTimeout(10000);
  const int responseCode = request.GET();
  if (responseCode != HTTP_CODE_OK) {
    Serial.printf("Yahoo chart request failed: HTTP %d (%s)\n", responseCode,
                  request.errorToString(responseCode).c_str());
    failureStatus = responseCode < 0 ? "Błąd połączenia Yahoo"
                                     : "Yahoo HTTP " + String(responseCode);
    request.end();
    return false;
  }

  const String response = request.getString();
  request.end();
  if (response.indexOf("\"symbol\":\"IPW.WA\"") < 0 ||
      response.indexOf("\"currency\":\"PLN\"") < 0 ||
      response.indexOf("\"error\":null") < 0) {
    Serial.println("Yahoo chart response did not match IPW.WA/PLN");
    failureStatus = "Nieprawidłowa odpowiedź Yahoo";
    return false;
  }

  double currentPrice = 0.0;
  double currentTimestamp = 0.0;
  double currentChangePercent = 0.0;
  if (!parseYahooNumber(response, "regularMarketPrice", currentPrice) ||
      !parseYahooNumber(response, "regularMarketTime", currentTimestamp) ||
      currentPrice <= 0.0 || currentTimestamp <= QuoteTlsStartEpoch) {
    Serial.println("Yahoo chart response has no current quote metadata");
    failureStatus = "Brak bieżącego kursu Yahoo";
    return false;
  }
  const bool currentChangeAvailable =
      parseYahooNumber(response, "regularMarketChangePercent",
                       currentChangePercent);

  std::vector<double> timestamps;
  std::vector<double> closes;
  if (!parseYahooArray(response, "timestamp", timestamps) ||
      !parseYahooArray(response, "close", closes) ||
      timestamps.size() != closes.size()) {
    Serial.println("Yahoo chart response has invalid timestamp/close arrays");
    failureStatus = "Nieprawidłowa historia Yahoo";
    return false;
  }

  struct tm latestTime = {};
  bool hasLatestTime = false;
  for (size_t i = 0; i < timestamps.size(); ++i) {
    if (!isfinite(timestamps[i]) || timestamps[i] <= 0.0 ||
        !isfinite(closes[i]) || closes[i] <= 0.0) {
      continue;
    }
    const time_t epoch = static_cast<time_t>(timestamps[i]);
    struct tm localTime = {};
    if (localtime_r(&epoch, &localTime) != nullptr &&
        (!hasLatestTime || stockDayKey(localTime) > stockDayKey(latestTime))) {
      latestTime = localTime;
      hasLatestTime = true;
    }
  }
  if (!hasLatestTime) {
    const time_t quoteTime = static_cast<time_t>(currentTimestamp);
    hasLatestTime = localtime_r(&quoteTime, &latestTime) != nullptr;
  }
  if (!hasLatestTime) {
    Serial.println("Yahoo chart response has no usable timestamps");
    failureStatus = "Brak danych Yahoo";
    return false;
  }

  const int32_t historyDay = stockDayKey(latestTime);
  std::vector<StockQuoteSample> samples;
  for (size_t i = 0; i < timestamps.size(); ++i) {
    if (!isfinite(timestamps[i]) || timestamps[i] <= 0.0 ||
        !isfinite(closes[i]) || closes[i] <= 0.0) {
      continue;
    }
    const time_t epoch = static_cast<time_t>(timestamps[i]);
    struct tm localTime = {};
    if (localtime_r(&epoch, &localTime) == nullptr ||
        stockDayKey(localTime) != historyDay) {
      continue;
    }
    const uint16_t minuteOfDay =
        static_cast<uint16_t>(localTime.tm_hour * 60 + localTime.tm_min);
    if (minuteOfDay < 9 * 60 || minuteOfDay > 17 * 60) {
      continue;
    }
    samples.push_back(
        {minuteOfDay, static_cast<float>(closes[i])});
  }

  if (samples.empty()) {
    const time_t quoteTime = static_cast<time_t>(currentTimestamp);
    struct tm quoteLocalTime = {};
    if (localtime_r(&quoteTime, &quoteLocalTime) != nullptr &&
        stockDayKey(quoteLocalTime) == historyDay) {
      samples.push_back({
          static_cast<uint16_t>(quoteLocalTime.tm_hour * 60 +
                               quoteLocalTime.tm_min),
          static_cast<float>(currentPrice)});
    }
  }

  if (stockQuoteMutex == nullptr ||
      xSemaphoreTake(stockQuoteMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    Serial.println("Could not lock Yahoo chart history");
    failureStatus = "Błąd zapisu historii";
    return false;
  }
  const unsigned sampleCount = static_cast<unsigned>(samples.size());
  stockQuoteSamples.swap(samples);
  stockHistoryDay = historyDay;
  stockQuotePrice = static_cast<float>(currentPrice);
  stockQuoteTimestamp = static_cast<time_t>(currentTimestamp);
  stockQuoteAvailable = true;
  stockQuoteChangePercent = static_cast<float>(currentChangePercent);
  stockQuoteChangeAvailable = currentChangeAvailable;
  stockQuoteStatus = "";
  ++stockHistoryRevision;
  xSemaphoreGive(stockQuoteMutex);
  queueStockQuoteLog(static_cast<time_t>(currentTimestamp),
                     static_cast<float>(currentPrice),
                     static_cast<float>(currentChangePercent),
                     currentChangeAvailable);

  Serial.printf("Yahoo IPW.WA history: %u bars for %04ld-%02ld-%02ld\n",
                sampleCount,
                static_cast<long>(latestTime.tm_year + 1900),
                static_cast<long>(latestTime.tm_mon + 1),
                static_cast<long>(latestTime.tm_mday));
  return true;
}

// Zmienia komunikat kursu w sposób bezpieczny dla zadania pobierającego Yahoo.
void setStockQuoteStatus(const String &status) {
  if (stockQuoteMutex != nullptr &&
      xSemaphoreTake(stockQuoteMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (stockQuoteStatus != status) {
      stockQuoteStatus = status;
    }
    xSemaphoreGive(stockQuoteMutex);
  }
}

// Zadanie w tle odświeża dane Yahoo co minutę, a błędy ponawia częściej.
void stockQuoteTask(void *parameter) {
  (void)parameter;
  bool firstRequest = true;
  uint32_t lastRequestAt = 0;
  String lastLoggedFailure;
  uint32_t lastLoggedFailureAt = 0;
  for (;;) {
    bool quoteAvailable = false;
    if (stockQuoteMutex != nullptr &&
        xSemaphoreTake(stockQuoteMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
      quoteAvailable = stockQuoteAvailable;
      xSemaphoreGive(stockQuoteMutex);
    }

    if (WiFi.status() != WL_CONNECTED) {
      setStockQuoteStatus("Brak WiFi");
      firstRequest = true;
    } else if (time(nullptr) < QuoteTlsStartEpoch) {
      setStockQuoteStatus("Brak czasu: włącz NTP");
      firstRequest = true;
    } else if (firstRequest ||
               millis() - lastRequestAt >=
                   (quoteAvailable ? QuoteRefreshIntervalMs
                                   : QuoteRetryIntervalMs)) {
      setStockQuoteStatus("Pobieranie Yahoo...");
      lastRequestAt = millis();
      firstRequest = false;
      String failureStatus;
      if (!fetchYahooIntraday(failureStatus)) {
        setStockQuoteStatus(failureStatus);
        if (failureStatus != lastLoggedFailure ||
            millis() - lastLoggedFailureAt >= 60000) {
          queueEventLog("BŁĄD Yahoo Finance: " + failureStatus);
          lastLoggedFailure = failureStatus;
          lastLoggedFailureAt = millis();
        }
      } else {
        lastLoggedFailure = "";
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

}  // namespace

// Inicjalizuje ekran, dotyk, ustawienia, Wi-Fi, serwer WWW i zadanie Yahoo.
void setup() {
  Serial.begin(115200);
  eventLogQueue = xQueueCreate(24, sizeof(LogEvent));
  if (eventLogQueue == nullptr) {
    Serial.println("Nie można utworzyć kolejki zdarzeń logu SD");
  }
  pinMode(AmbientLightPin, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(AmbientLightPin, ADC_11db);
  ledcSetup(BacklightPwmChannel, BacklightPwmFrequency,
            BacklightPwmResolution);
  ledcAttachPin(BacklightPin, BacklightPwmChannel);
  ledcSetup(StockLedRedPwmChannel, StockLedPwmFrequency,
            BacklightPwmResolution);
  ledcSetup(StockLedGreenPwmChannel, StockLedPwmFrequency,
            BacklightPwmResolution);
  ledcSetup(StockLedBluePwmChannel, StockLedPwmFrequency,
            BacklightPwmResolution);
  ledcAttachPin(StockLedRedPin, StockLedRedPwmChannel);
  ledcAttachPin(StockLedGreenPin, StockLedGreenPwmChannel);
  ledcAttachPin(StockLedBluePin, StockLedBluePwmChannel);
  setStockLedColor(0, 0, 0);
  setBacklightLevel(manualBacklightLevel);

  tft.init();
  tft.invertDisplay(true);
  tft.setRotation(3);
  initializeOptionsSprite();

  SPI.begin(TouchClockPin, TouchMisoPin, TouchMosiPin, TouchChipSelectPin);
  touch.begin();
  touch.setRotation(3);

  if (!preferences.begin("wifi-clock", false)) {
    Serial.println("Błąd inicjalizacji Preferences");
    queueEventLog("BŁĄD: nie udało się zainicjalizować Preferences");
  }
  setenv("TZ", TimeZone, 1);
  tzset();
  queueEventLog("Uruchomienie urządzenia");
  timeSyncEnabled = preferences.getBool("timeSync", true);
  accessPointEnabled = preferences.getBool("apEnabled", true);
  stockLedEnabled = preferences.getBool("stockLed", true);
  updateStockLedFromQuote();
  manualBacklightLevel = constrain(
      preferences.getUChar("brightness", manualBacklightLevel),
      MinimumBacklight, MaximumBacklight);
  autoBacklightEnabled = preferences.getBool("autoBrightness", false);
  autoBacklightSensitivity =
      constrain(preferences.getUChar("autoSensitivity",
                                     autoBacklightSensitivity),
                0, 100);
  autoBacklightMinimum = constrain(
      preferences.getUChar("autoMin", autoBacklightMinimum),
      MinimumBacklight, MaximumBacklight);
  autoBacklightMaximum = constrain(
      preferences.getUChar("autoMax", autoBacklightMaximum),
      autoBacklightMinimum, MaximumBacklight);
  setBacklightLevel(manualBacklightLevel);
  touchCalibrated = preferences.getBool("touchCal", false) &&
                    preferences.getUChar("touchVer", 0) == TouchCalibrationVersion;
  if (touchCalibrated) {
    touchXLeft = preferences.getInt("touchXL", 0);
    touchXRight = preferences.getInt("touchXR", 4095);
    touchYTop = preferences.getInt("touchYT", 0);
    touchYBottom = preferences.getInt("touchYB", 4095);
  }

  WiFi.mode(accessPointEnabled ? WIFI_AP_STA : WIFI_STA);
  WiFi.setSleep(false);
  String deviceId = WiFi.macAddress();
  deviceId.replace(":", "");
  deviceId = deviceId.substring(deviceId.length() - 6);
  setupSsid = "MonitorAkcji-" + deviceId;
  setupPassword = loadOrCreateSetupPassword();
  webSessionToken = String(esp_random(), HEX) + String(esp_random(), HEX);
  if (accessPointEnabled && !startSetupAccessPoint()) {
    accessPointEnabled = false;
    preferences.putBool("apEnabled", false);
    WiFi.mode(WIFI_STA);
  }
  const uint64_t manualTime = preferences.getULong64("manualTime", 0);
  if (manualTime > 0) {
    timeval restoredTime = {};
    restoredTime.tv_sec = static_cast<time_t>(manualTime);
    settimeofday(&restoredTime, nullptr);
  }
  beginWebServer();
  stockQuoteMutex = xSemaphoreCreateMutex();
  if (stockQuoteMutex == nullptr) {
    Serial.println("Could not create stock quote mutex");
    queueEventLog("BŁĄD: nie udało się utworzyć blokady danych Yahoo");
  } else if (xTaskCreate(stockQuoteTask, "yahoo-quote", 8192, nullptr, 1,
                         nullptr) != pdPASS) {
    Serial.println("Could not start Yahoo quote task");
    queueEventLog("BŁĄD: nie udało się uruchomić zadania pobierania Yahoo");
  }
  if (touchCalibrated) {
    drawClockScreen();
    connectLastSavedNetwork();
  } else {
    beginTouchCalibration(false);
  }
}

// Główna pętla obsługuje dotyk, stan Wi-Fi i odświeżanie ekranu.
void loop() {
  handleTouch();
  updateOptionsScrollAnimation();
  if (accessPointEnabled) {
    dnsServer.processNextRequest();
  }
  webServer.handleClient();

  if (networkScanPending &&
      millis() - networkScanStartAt >= NetworkScanStartDelayMs &&
      (!resumeConnectionAfterScan || WiFi.status() != WL_CONNECTED)) {
    const int scanStart = WiFi.scanNetworks(true, false, false, 500);
    if (scanStart == WIFI_SCAN_FAILED) {
      ++networkScanStartAttempts;
      Serial.printf("WiFi scan start failed (%u/5), status=%d\n",
                    networkScanStartAttempts, WiFi.status());
      if (networkScanStartAttempts >= 5) {
        networkScanPending = false;
        statusMessage = "Skanowanie WiFi nie powiodło się";
        queueEventLog("BŁĄD: nie udało się uruchomić skanowania WiFi");
        WiFi.scanDelete();
        resumeStationConnectionAfterScan();
        if (currentScreen == Screen::Networks) {
          drawNetworkScreen();
        }
      } else {
        networkScanStartAt = millis();
      }
    } else {
      networkScanPending = false;
    }
  }

  if (statusMessage == "Skanowanie sieci...") {
    const bool scanRunning = isNetworkScanActive();
    if (scanRunning && millis() - lastScanIndicatorUpdate >= 80) {
      lastScanIndicatorUpdate = millis();
      if (currentScreen == Screen::Networks) {
        updateNetworkScanIndicator();
      }
    } else if (!scanRunning) {
      finishNetworkScan();
      Serial.printf("WiFi scan finished: %s\n", statusMessage.c_str());
      if (currentScreen == Screen::Networks) {
        drawNetworkScreen();
      }
    }
  }

  updateConnection();
  updateBacklight();
  updateClock();
  processEventLogQueue();
  delay(10);
}
