#include <VirtualWire.h>
#include "events.h"

#define ANALOG_ENABLE 9

int8_t TIMER_2_init()
{
  /* Enable TC2 */
  PRR &= ~(1 << PRTIM2);

  // TCCR2A = (0 << COM2A1) | (0 << COM2A0) /* Normal port operation, OCA disconnected */
  //     | (0 << COM2B1) | (0 << COM2B0) /* Normal port operation, OCB disconnected */
  //     | (0 << WGM21) | (0 << WGM20); /* TC8 Mode 0 Normal */

  TCCR2B = 0                                          /* TC8 Mode 0 Normal */
           | (1 << CS22) | (0 << CS21) | (0 << CS20); /* IO clock divided by 64 */

  TIMSK2 = 0 << OCIE2B   /* Output Compare B Match Interrupt Enable: disabled */
           | 0 << OCIE2A /* Output Compare A Match Interrupt Enable: disabled */
           | 1 << TOIE2; /* Overflow Interrupt Enable: enabled */

  // GTCCR = 0 << TSM /* Timer/Counter Synchronization Mode: disabled */
  //     | 0 << PSRASY /* Prescaler Reset Timer/Counter2: disabled */
  //     | 0 << PSRSYNC; /* Prescaler Reset: disabled */

  ASSR = (1 << AS2); // Enable asynchronous mode

  return 0;
}

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
const short header_length = 2;

uint8_t buf[VW_MAX_MESSAGE_LEN];
uint8_t buflen = VW_MAX_MESSAGE_LEN;

///////////// buffers for transmission //////////////////////////
struct time_frame {
  uint32_t timestamp;
} time_frame_req, time_frame_resp;

struct analog_read_req_t {
  uint8_t pin;
} analog_read_req;

struct analog_read_resp_t {
  uint16_t value;
} analog_read_resp;

struct blink_req_t {
  uint8_t pin;
  uint16_t ms;
} blink_req;
///////////////////////////////////////////////////////////////////

void get_data_from_buf(void *addr, size_t s) {
  memcpy(addr, buf+header_length, s);
}

void send_response(void *addr, size_t s) {
  delay(send_delay);
  // buf[0] = id;
  buf[1] = 0;
  memcpy(buf+header_length, addr, s);
  vw_send(buf, s+header_length);
}

void send_empty_response() {
  delay(send_delay);
  // buf[0] = id;
  buf[1] = 0;
  vw_send(buf, header_length);
}

void loop() {
  buflen = VW_MAX_MESSAGE_LEN;
  if (vw_get_message(buf, &buflen)) // Non-blocking
  {

    uint8_t id = buf[0];
    uint8_t type = buf[1];

    if (type == 1) { // time_req
      get_data_from_buf(&time_frame_req, sizeof(time_frame));
      Serial.print("Got timestamp ");
      Serial.println(time_frame_req.timestamp);

      if (time_frame_req.timestamp > 0)
        set_unix_time(time_frame_req.timestamp);

      // Send response
      time_frame_resp.timestamp = get_unix_time();
      send_response(&time_frame_resp, sizeof(time_frame));

    } else if (type == 2) { // analog read
      get_data_from_buf(&analog_read_req, sizeof(analog_read_req_t));
      Serial.print("Reading analog pin ");
      Serial.println(analog_read_req.pin);
      digitalWrite(ANALOG_ENABLE, 1);
      delay(100);
      uint16_t value = analogRead(PC0+analog_read_req.pin);
      digitalWrite(ANALOG_ENABLE, 0);

      // Send response
      analog_read_resp.value = value;
      send_response(&analog_read_resp, sizeof(analog_read_resp_t));
      
    } else if (type == 3) { // blink
      get_data_from_buf(&blink_req, sizeof(blink_req_t));
      Serial.print("Blinking pin ");
      Serial.print(blink_req.pin);
      Serial.print(" for ");
      Serial.println(blink_req.ms);
      // Blink after sanding reply to prevent timeout...

      // Send response
      send_empty_response();

      // TODO: delay even further, we can miss retransmission by waiting here
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

