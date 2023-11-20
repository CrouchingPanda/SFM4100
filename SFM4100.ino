#include <TinyWireM.h>
#include <Tiny4kOLED.h>

// datasheet
static constexpr uint8_t SFM4100_I2C_ADDRESS = 0x01;
static constexpr uint8_t SFM4100_READ_REQUEST = 0xF1;
static constexpr uint16_t SFM4100_CRC_POLYNOMIAL = 0b100110001; // x^8 + x^5 + x^4 + 1

static constexpr uint8_t OLED_WIDTH = 128; // device-dependent
static constexpr uint8_t OLED_HEIGHT = 32; // device-dependent
static constexpr uint8_t OLED_UPDATE_PERIOD_MS = 50; // arbitrary

static constexpr uint8_t UNIT_TOGGLE_BUTTON_PIN = 3;

enum FlowUnit {
  SCCM, // standard cubic centimiters per minute
  SLM   // standard liters per minute
};

void setup() {
  TinyWireM.begin();

  oled.begin(OLED_WIDTH, OLED_HEIGHT, sizeof(tiny4koled_init_128x32br), tiny4koled_init_128x32br);
  oled.enableChargePump();
  oled.setRotation(1);
  oled.setFontX2Smooth(FONT6X8P);
  oled.clear();
  oled.on();
  oled.switchRenderFrame();
}

void loop() {
  int16_t flow = readFlowInSCCM();
  FlowUnit unit = getFlowUnit();
  updateDisplayedFlow(flow, unit);
}

int16_t readFlowInSCCM() {
  TinyWireM.beginTransmission(SFM4100_I2C_ADDRESS);
  TinyWireM.write(SFM4100_READ_REQUEST);
  TinyWireM.endTransmission();

  TinyWireM.requestFrom(SFM4100_I2C_ADDRESS, 3);
  uint8_t highByte = TinyWireM.read();
  uint8_t lowByte = TinyWireM.read();
  uint8_t checksum = TinyWireM.read();

  static int16_t flowInSCCM = 0;
  if (isCRCValid(highByte, lowByte, checksum)) flowInSCCM = (highByte << 8) | lowByte;
  return flowInSCCM;
}

bool isCRCValid(uint8_t highByte, uint8_t lowByte, uint8_t checksum) {
  uint8_t crc = 0;
  crc ^= highByte;
  crc = doPolynomialDivision(crc);
  crc ^= lowByte;
  crc = doPolynomialDivision(crc);
  return crc == checksum;
}

uint8_t doPolynomialDivision(uint8_t dividend) {
  for (uint8_t bit = 8; bit > 0; --bit) {
    if (dividend & 0x80) {
      dividend = (dividend << 1) ^ SFM4100_CRC_POLYNOMIAL;
    } else {
      dividend = (dividend << 1);
    }
  }
  return dividend;
}

FlowUnit getFlowUnit() {
  static FlowUnit unit = SCCM;
  static bool previousButtonPressed = false;
  bool buttonPressed = analogRead(UNIT_TOGGLE_BUTTON_PIN) < 3;
  if (buttonPressed && !previousButtonPressed) {
    previousButtonPressed = true;
    unit = (unit == SCCM) ? SLM : SCCM;
  } else if (!buttonPressed) {
    previousButtonPressed = false;
  }
  return unit;
}

void updateDisplayedFlow(int16_t flowInSCCM, FlowUnit unit) {
  static uint32_t lastUpdateTimestampMs = 0;
  if (millis() - lastUpdateTimestampMs < OLED_UPDATE_PERIOD_MS) return;

  oled.clear();
  oled.setCursor(0, 2);

  char buffer[12];
  if (unit == SCCM) {
    sprintf(buffer, "SCCM: %5d", flowInSCCM);
  } else {
    int8_t wholeLiters = flowInSCCM / 1000;
    int16_t fractions = flowInSCCM % 1000;
    const char* format = (wholeLiters == 0 && fractions < 0) ? "SLM: -0.%03d" : "SLM: %2d.%03d";
    sprintf(buffer, format, wholeLiters, abs(fractions));
  }
  
  oled.print(buffer);
  oled.switchFrame();

  lastUpdateTimestampMs = millis();
}
