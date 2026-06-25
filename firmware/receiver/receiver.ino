/* ESP32 NRF24L01+ Sniffer / Receiver
   - Sweeps 8 channels, uses RPD (testRPD) to detect energy (> -64 dBm)
   - Accumulates counts per channel for a short window and prints JSON:
     {"counts":[.., .., ..]}
   - Matches your Python GUI's expected input format.

   Wiring (example):
     nRF24L01+ VCC -> 3.3V
     GND -> GND
     CE  -> 4    (change below if needed)
     CSN -> 5    (change below if needed)
     SCK -> 18
     MOSI-> 23
     MISO-> 19
*/

#include <SPI.h>
#include <RF24.h>

// Pins - adjust to your wiring
#define CE_PIN 4
#define CSN_PIN 5

RF24 radio(CE_PIN, CSN_PIN);

// Channel list (8 channels) - pick an 8-channel window in 0..125
// These are nRF "RF channels" (0..125). You can tune these values.
const uint8_t CHANNELS[8] = { 30, 32, 34, 36, 38, 40, 42, 44 };

// Timing
const unsigned long WINDOW_MS = 120;   // accumulate counts for 120 ms per JSON output
const uint16_t SAMPLE_DELAY_US = 200;  // microsecond pause after enabling RX (RPD read stabilization)

uint16_t counts[8];

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }

  if (!radio.begin()) {
    Serial.println(F("{\"error\":\"radio init failed\"}"));
    while (1) delay(1000);
  }

  radio.setAutoAck(false);
  radio.setRetries(0,0);
  radio.setPayloadSize(1);
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_1MBPS); // 1Mbps often good for sniffing
  radio.maskIRQ(true, true, true); // we don't want interrupts
  radio.stopListening();

  // zero counts
  for (int i=0;i<8;i++) counts[i]=0;

  // small settle
  delay(100);
  Serial.println(F("{\"info\":\"sniffer started\"}"));
}

void loop() {
  unsigned long t0 = millis();

  // sweep quickly many times within WINDOW_MS to accumulate counts
  while (millis() - t0 < WINDOW_MS) {
    for (uint8_t i = 0; i < 8; ++i) {
      uint8_t ch = CHANNELS[i];
      radio.setChannel(ch);
      // Put radio in RX mode briefly
      radio.startListening();
      // wait a little for the radio to sample the RF environment
      delayMicroseconds(SAMPLE_DELAY_US);
      // testRPD returns 1 if energy > -64 dBm else 0 (for nRF24L01+)
      bool rpd = radio.testRPD(); // available in RF24 library for nRF24L01+
      radio.stopListening();

      if (rpd) counts[i]++;

      // very small gap to allow other chips to operate
      delayMicroseconds(20);
    }
  }

  // Scale counts to a 0-255 range to match your GUI normalization expectations
  // We expect counts to be small integers depending on WINDOW_MS and sweep speed.
  uint8_t out[8];
  uint16_t maxc = 0;
  for (int i=0;i<8;i++) if (counts[i] > maxc) maxc = counts[i];

  for (int i=0;i<8;i++) {
    if (maxc == 0) out[i] = 0;
    else {
      // map counts[i] proportionally into 0..255 but cap to 255
      uint32_t v = (uint32_t)counts[i] * 255 / maxc;
      if (v > 255) v = 255;
      out[i] = (uint8_t)v;
    }
    // reset counts for next window
    counts[i] = 0;
  }

  // Build JSON line
  Serial.print("{\"counts\":[");
  for (int i=0;i<8;i++) {
    Serial.print(out[i]);
    if (i < 7) Serial.print(",");
  }
  Serial.println("]}");

  // small sleep to avoid flooding USB serial
  delay(5); this code is receiver?