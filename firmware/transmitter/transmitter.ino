ESP32 NRF24L01+ Drone Simulator
   - Sends bursts across the same 8 channels used by the sniffer.
   - Two modes:
       1) DRONE_BURST: strong narrowband bursts on a single channel (like some consumer drones)
       2) FPV_CHATTER: frequent short packets across multiple channels (like analog/FPV chatter)
   - Press the built-in button (if wired) to switch modes or set mode via code.

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

// Pins - adjust as needed
#define CE_PIN 4
#define CSN_PIN 5

RF24 radio(CE_PIN, CSN_PIN);

// Use same channels as receiver to test detection
const uint8_t CHANNELS[8] = { 30, 32, 34, 36, 38, 40, 42, 44 };

enum Mode { MODE_DRONE_BURST, MODE_FPV_CHATTER, MODE_NOISE };
Mode mode = MODE_DRONE_BURST;

// simple payload -- not meaningful, just noise
uint8_t payload[12];

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  if (!radio.begin()) {
    Serial.println(F("radio init failed"));
    while (1) delay(1000);
  }

  radio.setAutoAck(false);
  radio.setRetries(0,0);
  radio.setPayloadSize(sizeof(payload));
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_1MBPS);

  radio.stopListening();

  Serial.println(F("drone-sim started"));
  Serial.println(F("Mode: DRONE_BURST"));

  randomSeed(analogRead(34));
}

void loop() {
  if (mode == MODE_DRONE_BURST) {
    simulate_drone_burst();
  } else if (mode == MODE_FPV_CHATTER) {
    simulate_fpv_chatter();
  } else {
    simulate_noise();
  }

  // For testing convenience, allow switching mode by sending commands via Serial:
  // Send "burst", "chatter", or "noise" over serial to change mode.
  if (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    s.trim();
    if (s.equalsIgnoreCase("burst")) {
      mode = MODE_DRONE_BURST;
      Serial.println("Mode: DRONE_BURST");
    } else if (s.equalsIgnoreCase("chatter")) {
      mode = MODE_FPV_CHATTER;
      Serial.println("Mode: MODE_FPV_CHATTER");
    } else if (s.equalsIgnoreCase("noise")) {
      mode = MODE_NOISE;
      Serial.println("Mode: MODE_NOISE");
    }
  }
}

/* Strong narrowband drone burst:
   - pick one channel (random or rotating)
   - send rapid packets for a short time
*/
void simulate_drone_burst() {
  // choose a channel for the burst
  static uint8_t ch_idx = 0;
  ch_idx = (ch_idx + 1) & 7; // rotate on each burst
  uint8_t ch = CHANNELS[ch_idx];
  radio.setChannel(ch);

  // burst length: send N packets quickly
  int burstPackets = 25 + random(0,25); // 25-50 packets
  for (int i=0;i<burstPackets;i++) {
    // make payload semi-random
    for (uint8_t k=0;k<sizeof(payload);k++) payload[k] = random(0,256);
    radio.write(payload, sizeof(payload));
    // very short gap => creates strong energy on that channel
    delayMicroseconds(400); // tune for stronger/weaker signature
  }

  // pause between bursts
  delay(80 + random(0,80));
}

/* FPV-style chatter:
   - send short packets across many channels frequently (broadband)
*/
void simulate_fpv_chatter() {
  // perform a short hop sequence
  int hops = 6 + random(0,6);
  for (int h=0; h<hops; h++) {
    uint8_t ch = CHANNELS[random(0,8)];
    radio.setChannel(ch);
    for (uint8_t k=0;k<sizeof(payload);k++) payload[k] = random(0,256);
    radio.write(payload, sizeof(payload));
    delayMicroseconds(200 + random(0,400));
  }
  delay(30 + random(0,80));
}

/* Random noise: occasional packets across random channels */
void simulate_noise() {
  if (random(0,100) < 30) { // 30% chance each loop to send something
    uint8_t ch = CHANNELS[random(0,8)];
    radio.setChannel(ch);
    for (uint8_t k=0;k<sizeof(payload);k++) payload[k] = random(0,256);
    radio.write(payload, sizeof(payload));
  }
  delay(40 + random(0,100));
}