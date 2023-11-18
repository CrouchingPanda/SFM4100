#include <TinyWireM.h>
#include <Tiny4kOLED.h>

// datasheet
static constexpr uint8_t SFM4100_I2C_ADDRESS = 0x01;
static constexpr uint8_t SFM4100_READ_REQUEST = 0xF1;
static constexpr uint16_t SFM4100_CRC_POLYNOMIAL = 0x131;

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
  uint8_t flowBytes[2];
  flowBytes[0] = TinyWireM.read();
  flowBytes[1] = TinyWireM.read();
  uint8_t checksum = TinyWireM.read();

  static int16_t flowInSCCM = 0;
  if (isCRCValid(flowBytes, checksum)) flowInSCCM = flowBytes[1] | (flowBytes[0] << 8);
  return flowInSCCM;
}

bool isCRCValid(uint8_t data[], uint8_t checksum) {
  uint8_t crc = 0;
  uint8_t byteCount;
  
  for (byteCount = 0; byteCount < 2; byteCount++) {
    crc ^= (data[byteCount]);
    for (uint8_t bit = 8; bit > 0; --bit) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ SFM4100_CRC_POLYNOMIAL;
      } else {
        crc = (crc << 1);
      }
    }  
  }
  
  return crc == checksum;
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