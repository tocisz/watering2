#include <VirtualWire.h>
#include "tc8.h"
#include "events.h"

#define ANALOG_ENABLE 9

void setup() {
  Serial.begin(9600);  // Debugging only
  Serial.println("setup");
  
  TIMER_2_init(); // RTC

  digitalWrite(ANALOG_ENABLE, 0);
  pinMode(ANALOG_ENABLE, OUTPUT);

  // blink outputs
  for (int i = 5; i <= 8; ++i) {
    digitalWrite(i, 0);
    pinMode(i, OUTPUT);
  }

  // Initialise the IO and ISR
  vw_setup(2000);  // Bits per sec
  vw_rx_start();       // Start the receiver PLL running
}

const int send_delay = 200;

uint8_t buf[VW_MAX_MESSAGE_LEN];
uint8_t buflen = VW_MAX_MESSAGE_LEN;

///////////// buffers for transmission //////////////////////////
struct time_frame {
  uint8_t id;
  uint8_t type;
  uint32_t timestamp;
} time_frame_req, time_frame_resp;

struct analog_read_req_t {
  uint8_t id;
  uint8_t type;
  uint8_t pin;
} analog_read_req;

struct analog_read_resp_t {
  uint8_t id;
  uint8_t type;
  uint16_t value;
} analog_read_resp;

struct blink_req_t {
  uint8_t id;
  uint8_t type;
  uint8_t pin;
  uint16_t ms;
} blink_req;

struct empty_resp_t {
  uint8_t id;
  uint8_t type;
} blink_resp;
///////////////////////////////////////////////////////////////////

void loop() {
  if (vw_get_message(buf, &buflen)) // Non-blocking
  {

    uint8_t id = buf[0];
    uint8_t type = buf[1];

    if (type == 1) { // time_req
      memcpy(&time_frame_req, buf, sizeof(time_frame));
      Serial.print("Got timestamp ");
      Serial.println(time_frame_req.timestamp);

      if (time_frame_req.timestamp > 0)
        set_unix_time(time_frame_req.timestamp);

      // Send response
      delay(send_delay);
      time_frame_resp.id = id;
      time_frame_resp.type = 0;
      time_frame_resp.timestamp = get_unix_time();
      vw_send((uint8_t *)&time_frame_resp, sizeof(time_frame));

    } else if (type == 2) { // analog read
      memcpy(&analog_read_req, buf, sizeof(analog_read_req_t));
      Serial.print("Reading analog pin ");
      Serial.println(analog_read_req.pin);
      digitalWrite(ANALOG_ENABLE, 1);
      delay(100);
      uint16_t value = analogRead(PC0+analog_read_req.pin);
      digitalWrite(ANALOG_ENABLE, 0);

      // Send response
      delay(send_delay);
      analog_read_resp.id = id;
      analog_read_resp.type = 0;
      analog_read_resp.value = value;
      vw_send((uint8_t *)&analog_read_resp, sizeof(analog_read_resp_t));
      
    } else if (type == 3) { // blink
      memcpy(&blink_req, buf, sizeof(blink_req_t));
      Serial.print("Blinking pin ");
      Serial.print(blink_req.pin);
      Serial.print(" for ");
      Serial.println(blink_req.ms);
      // Blink after sanding reply to prevent timeout...

      // Send response
      delay(send_delay);
      blink_resp.id = id;
      blink_resp.type = 0;
      vw_send((uint8_t *)&blink_resp, sizeof(empty_resp_t));

      // ...blink now.
      Serial.print(blink_req.pin);
      Serial.println(" ON");
      digitalWrite(blink_req.pin, 1);
      delay(blink_req.ms);
      digitalWrite(blink_req.pin, 0);
      Serial.print(blink_req.pin);
      Serial.println(" OFF");
      
    }

  }

  if (Events.new_1s_cycle) {
    Events.new_1s_cycle = false;
  }
}

ISR(TIMER2_OVF_vect)
{
  ++unix_time;
  Events.new_1s_cycle = true;
}

