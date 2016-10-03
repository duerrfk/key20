/**
 * This file is part of Key20.
 *
 * Copyright 2016 Frank Duerr
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <nrf.h>
#include <nrf_gpio.h>
#include <ble.h>
#include <softdevice_handler.h>
#include <ble_advdata.h>
#include <app_timer.h>
#include <app_button.h>
#include <pstorage.h>
#include <curve25519-cortexm0.h>
#include <avrnacl.h>
#include <hd44780nrf51.h>
#include <ble_hci.h>
#include "app_event_queue.h"

// Pinout of development board (DK):
// * Pin 17: Button 1
// * Pin 18: Button 2 
// * Pin 21: LED 1
// * Pin 22: LED 2
#ifdef TARGET_BOARD_NRF51DK
#define PIN_BUTTON_RED 17
#define PIN_BUTTON_GREEN 18
#define PIN_LOCK 22
#define PIN_LED 21
#define PIN_LCD_RS 16
#define PIN_LCD_E 19
#define PIN_LCD_DB4 12
#define PIN_LCD_DB5 13
#define PIN_LCD_DB6 14
#define PIN_LCD_DB7 15
#else
// Pinout of Key20 lock controller board:
#define PIN_BUTTON_RED 2
#define PIN_BUTTON_GREEN 4
#define PIN_LOCK 3
// Actually, the Key20 board has no LED.
// Pin 21 is not connected on this board, so it will also do no harm.
#define PIN_LED 21
#define PIN_LCD_RS 16
#define PIN_LCD_E 14
#define PIN_LCD_DB4 12
#define PIN_LCD_DB5 10
#define PIN_LCD_DB6 8
#define PIN_LCD_DB7 6
#endif

// Maximum number of pending application events. 
// In order to decouple event processing from event generation happening
// in the context of interrupts, we use an application event queue.
// Application events are then processed outside the interrupt context.
#define APP_EVENT_QUEUE_SIZE 16

// Number of keys.
#define KEY_COUNT 4

// Application-level events.
#define APP_EVENT_AUTH_TIMEOUT 0
#define APP_EVENT_BUTTON_RED_PRESSED 1
#define APP_EVENT_BUTTON_GREEN_PRESSED 2
#define APP_EVENT_CLIENT_CONNECTED 3
#define APP_EVENT_CLIENT_DISCONNECTED 4
#define APP_EVENT_SUBSCRIBED_CFG_OUT 5
#define APP_EVENT_SUBSCRIBED_NONCE 6
#define APP_EVENT_KEY_PART_RCVD 7
#define APP_EVENT_HMAC_PART_RCVD 8
#define APP_EVENT_PSTORE_READY 9
#define APP_EVENT_LOCK_ACTION_TIMEOUT 10
#define APP_EVENT_INDICATION_NONCE_RCVD 11
#define APP_EVENT_INDICATION_CFG_OUT_RCVD 12

// Length of Diffie-Hellman keys using Eliptic Curve 25519 [bytes].
#define ECDH_KEY_LENGTH crypto_scalarmult_curve25519_BYTES

// Length of a SHAR512 [bytes].
#define SHA512_HASH_LENGTH crypto_hash_sha512_BYTES

// Length of an HMAC512-256 [bytes].
#define HMAC512_256 crypto_auth_hmacsha512256_BYTES

// Max. length of the Nonce characteristic.
#define MAX_LENGTH_NONCE_CHAR 16
 
// Max. length of Unlock characteristic [bytes].
#define MAX_LENGTH_UNLOCK_CHAR 18

// Max. length of config-in characteristic [bytes].
#define MAX_LENGTH_CFG_IN_CHAR 18

// Max. length of config-out characteristic [bytes].
#define MAX_LENGTH_CFG_OUT_CHAR 18

// Length of nonces used to avoid replay attacks [bytes].
// 128 bit nonces provide for sufficient security, assuming the following
// parameters:
// 
// 1. one request per millisecond and 100 years of operation 
//    -> less than n = 2**42 requests to protect.
// 2. m = 2**128 different nonces.
//
// Then the birthday paradox can be used to calculate the probability p of 
// at least one pair of requests sharing the same nonce, or, inversely,
// no nonces shared by any pair of requests. An approximation of p for n << m 
// is p(n,m) = 1 - e**((-n**2)/(2*m)), which practically evaluates to 0 for
// n = 2**42 and m = 2**128. Even for n = 2**52 (one request per us), 
// p(2**52,2**128) < 3e-8 which is about the probability to be hit by 
// lightning (about 5.5e-8).
// A nonce of 16 bytes also fits nicely into a characteristic.
#define NONCE_LENGTH 16

#define DEVICE_NAME "Key20"
// Minimum connection interval in 1.25 ms. Minimum allowed value: 7.5 ms.
// 16 -> 20 ms.
#define MIN_CONN_INTERVAL 16
// Maximum connection interval in 1.25 ms. Maximum allowed value: 4000 ms.
// 100 -> 125 ms.
#define MAX_CONN_INTERVAL 100
// Number of connection intervals the device can stay silent.
#define SLAVE_LATENCY 2
// Connection supervision timeout, i.e., time until a link is considered
// lost, in 10 ms.
// 400 -> 4 s. 
#define CONN_SUP_TIMEOUT 400
// Advertisement interval in 0.625 ms; min. 20 ms, max 10.24 s.
// 64 -> 40 ms.
#define ADV_INTERVAL 64
// How long to advertise in seconds (0 = forever)
#define ADV_TIMEOUT 0

// Prescaler of RTC1 (low-frequency clock at 32.768 kHz), which is used by the 
// app timer (RTC0 is used by the softdevice, and, therefore, cannot be used by 
// the application). 
#define APP_TIMER_PRESCALER 0
#define APP_TIMER_QUEUE_SIZE 6

// Delay for debouncing buttons [ms].
#define BUTTON_DETECTION_DELAY APP_TIMER_TICKS(50, APP_TIMER_PRESCALER)

// Authentication timeout [ms].
#define AUTH_TIMER_TIMEOUT APP_TIMER_TICKS(10000, APP_TIMER_PRESCALER)

// Time for operating the lock [ms].
#define LOCK_ACTION_TIMER_TIMEOUT APP_TIMER_TICKS(2000, APP_TIMER_PRESCALER)

// Service and charateristic UUIDs in Little Endian format.
// The 16 bit values will become byte 12 and 13 of the 128 bit UUID:
// 0x0a9dXXXX-5ff4-4c58-8a53627de7cf1faf
#define UUID_BASE {0xaf, 0x1f, 0xcf, 0xe7, 0x7d, 0x62, 0x53, 0x8a, \
	           0x58, 0x4c, 0xf4, 0x5f, 0x00, 0x00, 0x9d, 0x0a}
#define UUID_SERVICE 0x0001
#define UUID_CHARACTERISTIC_NONCE 0x0002
#define UUID_CHARACTERISTIC_UNLOCK 0x0003
#define UUID_CHARACTERISTIC_CFG_IN 0x0004
#define UUID_CHARACTERISTIC_CFG_OUT 0x0005

// Application states.
enum app_states {idle, cfg_wait_connection, cfg_wait_subscription, 
		 cfg_wait_key_part1, cfg_wait_key_part2, cfg_wait_decision, 
		 auth_wait_hmac_part1, auth_wait_hmac_part2, cfg_wait_key_store, 
		 auth_wait_lock_action_timeout, booting, cfg_wait_disconnect,
		 auth_wait_disconnect, aborted_wait_disconnect, 
		 auth_wait_subscription, auth_wait_nonce_rcvd,
		 cfg_wait_server_key_part1_rcvd, cfg_wait_server_key_part2_rcvd};

enum app_states app_state;

// A valid key store will have this (random) pattern as the first bytes.
// If not, this is an indication that the key store has never been
// written before, thus, there are not valid keys stored, and the
// key store should be initialized ("formatted") first.
// pstore_preamble must be word aligned to be used as memory location for
// pstorage operations.
uint8_t pstore_preamble[] __attribute__((aligned(4))) = 
      {0xfe, 0xec, 0x91, 0xf1, 0x06, 0xc4,
      0x40, 0x24, 0xbf, 0x19, 0x69, 0x7f, 
      0x96, 0x4d, 0xc6, 0x67};

// Definition of the LCD. 
struct hd44780 lcd = {
      .pin_rs = PIN_LCD_RS, 
      .pin_e = PIN_LCD_E, 
      .pin_db4 = PIN_LCD_DB4, 
      .pin_db5 = PIN_LCD_DB5, 
      .pin_db6 = PIN_LCD_DB6,
      .pin_db7 = PIN_LCD_DB7, 
      .rows = 2,
      .columns = 16
};

APP_TIMER_DEF(auth_timer);
APP_TIMER_DEF(lock_action_timer);

// keys variable must be word aligned to be used as memory location for
// pstorage operations.
uint8_t keys[KEY_COUNT][ECDH_KEY_LENGTH] __attribute__((aligned(4)));
// Bitset signaling which keys are valid (key is valid iff bit != 0).
// First key = bit0, second key = bit1, etc.
uint8_t keys_valid = 0;

uint8_t uuid_type;
uint16_t service_handle;
ble_gatts_char_handles_t char_handle_nonce;
ble_gatts_char_handles_t char_handle_unlock;
// The cfg_in characteristic is used for configuration messages from the
// client to the server (this device).
ble_gatts_char_handles_t char_handle_cfg_in;
// The cfg_out characteristic is used for configuration messages from the
// server (this device) to the client.
ble_gatts_char_handles_t char_handle_cfg_out;
uint16_t conn_handle = BLE_CONN_HANDLE_INVALID; 

uint8_t nonce[NONCE_LENGTH];

// During Diffie-Hellman key exchange, we need to keep some temporary keys.
// All keys are stored and transmitted in Little Endian format.
uint8_t keyexchange_server_secret_key[ECDH_KEY_LENGTH];
uint8_t keyexchange_server_public_key[ECDH_KEY_LENGTH];
uint8_t keyexchange_client_public_key[ECDH_KEY_LENGTH];
uint8_t keyexchange_shared_secret[ECDH_KEY_LENGTH];
uint8_t keyexchange_key_no = 0;

// For unlocking, the client has to provide an HMAC and key number.
uint8_t unlock_key_no = 0;
uint8_t unlock_hmac_client[HMAC512_256];

struct app_event_queue app_event_queue;

pstorage_handle_t pstore_handle;
volatile bool is_pstore_ready = false;

// Prototypes.

static void display_text(const char *text1, unsigned int length1,
			 const char *text2, unsigned int length2);
static void set_nonce_char();

// Implementations.

static void led_off()
{
     // LED is active low -> set to turn off.
     nrf_gpio_pin_set(PIN_LED);
}

static void led_on()
{
     // LED is active low -> clear to turn on.
     nrf_gpio_pin_clear(PIN_LED);
}

static void led_init()
{
     nrf_gpio_cfg_output(PIN_LED);
     led_off();
}

/**
 * We follow the "let it crash" paradigm known from Erlang. If something
 * unexpected happens (in particular, an SDK function not returning
 * NRF_SUCCESS), we reboot the system. So "die" actually means, die and
 * be reborn ("self-healing"). Besides that, we of course try to avoid 
 * situations where things might fail (defensive programming). However, 
 * in very unlikely failure situations, we opt for crashing and rebooting 
 * rather than overloading code (and precious code memory) with failure 
 * handling routines.
 */
static void die()
{
     display_text("Error", 5, NULL, 0);
     __disable_irq();
 
     // In a development system, we loop forever.
     // Remove the endless loop in a productive system to auto-reset.
     //while (1);
     
     // In a productive system, we automatically reset the system on errors.
     sd_nvic_SystemReset();
}

static void lock_init()
{
     nrf_gpio_cfg_output(PIN_LOCK);
     // The lock is active high.
     nrf_gpio_pin_clear (PIN_LOCK);
}

static void start_advertising()
{
    uint32_t err_code;
    ble_gap_adv_params_t adv_params;

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.type = BLE_GAP_ADV_TYPE_ADV_IND;
    adv_params.p_peer_addr = NULL;
    adv_params.fp = BLE_GAP_ADV_FP_ANY;
    adv_params.interval = ADV_INTERVAL;
    adv_params.timeout = ADV_TIMEOUT;

    err_code = sd_ble_gap_adv_start(&adv_params);
    if (err_code != NRF_SUCCESS)
	 die();
}

static void char_cfg_in_write_evt(ble_gatts_evt_write_t *evt_write)
{
     uint8_t key_part_number;
     struct app_event app_event;

     if (evt_write->handle == char_handle_cfg_in.value_handle && 
	 evt_write->len == MAX_LENGTH_CFG_IN_CHAR) {
	  keyexchange_key_no = evt_write->data[0];
	  if (keyexchange_key_no >= KEY_COUNT) {
	       // Invalid key number. 
	       return;
	  }
	  key_part_number = evt_write->data[1];
	  if (key_part_number == 0) 
	       memcpy(keyexchange_client_public_key, 
		      &evt_write->data[2], 16);
	  else
	       memcpy(&keyexchange_client_public_key[16], 
		      &evt_write->data[2], 16);
	  app_event.event_type = APP_EVENT_KEY_PART_RCVD;
	  app_event_queue_add(&app_event_queue, app_event);
     }
}

static void char_unlock_write_evt(ble_gatts_evt_write_t *evt_write)
{
     uint8_t key_part_number;
     struct app_event app_event;
     
     if (evt_write->handle == char_handle_unlock.value_handle && 
	 evt_write->len == MAX_LENGTH_UNLOCK_CHAR) {
	  unlock_key_no = evt_write->data[0];
	  if (unlock_key_no >= KEY_COUNT) {
	       // Invalid key number.
	       return;
	  }
	  key_part_number = evt_write->data[1];
	  if (key_part_number == 0) 
	       memcpy(&unlock_hmac_client[0], &evt_write->data[2], 16);
	  else
	       memcpy(&unlock_hmac_client[16], &evt_write->data[2], 16);
	  app_event.event_type = APP_EVENT_HMAC_PART_RCVD;
	  app_event_queue_add(&app_event_queue, app_event);
     }
}

static void cccd_cfg_out_write_evt(ble_gatts_evt_write_t *evt_write)
{
     struct app_event app_event;
     
     // A subscription is made by the client by writing the characteristic's
     // CCCD (Client Characteristic Configuration Descriptor). A value of 
     // 0x0001 signals a subscription to notifications;
     // 0x0002 signals a subscription to indications. See:
     // See https://www.bluetooth.com/specifications/gatt/
     // viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.
     // client_characteristic_configuration.xml
     if (evt_write->handle == char_handle_cfg_out.cccd_handle) {
	  if (evt_write->data[0] == 0x02 && evt_write->data[1] == 0x00) {
	       app_event.event_type = APP_EVENT_SUBSCRIBED_CFG_OUT;
	       app_event_queue_add(&app_event_queue, app_event);
	  }
     }
}

static void cccd_nonce_write_evt(ble_gatts_evt_write_t *evt_write)
{
     struct app_event app_event;
	  
     // A subscription is made by the client by writing the characteristic's
     // CCCD (Client Characteristic Configuration Descriptor). A value of 
     // 0x0001 signals a subscription to notifications;
     // 0x0002 signals a subscription to indications. See:
     // https://www.bluetooth.com/specifications/gatt/
     // viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.
     // client_characteristic_configuration.xml
     if (evt_write->handle == char_handle_nonce.cccd_handle) {
	  if (evt_write->data[0] == 0x02 && evt_write->data[1] == 0x00) {
	       app_event.event_type = APP_EVENT_SUBSCRIBED_NONCE;
	       app_event_queue_add(&app_event_queue, app_event);
	  }
     }
}

static void on_sys_evt(uint32_t sys_evt)
{
}

static void sys_evt_dispatch(uint32_t sys_evt)
{
    pstorage_sys_event_handler(sys_evt);
    on_sys_evt(sys_evt);
}

static void nonce_indication_hvc_evt(ble_gatts_evt_hvc_t *evt_hvc)
{
     struct app_event app_event;

     if (evt_hvc->handle == char_handle_nonce.value_handle) {
	  app_event.event_type = APP_EVENT_INDICATION_NONCE_RCVD;
	  app_event_queue_add(&app_event_queue, app_event);
     }
}

static void cfg_out_indication_hvc_evt(ble_gatts_evt_hvc_t *evt_hvc)
{
     struct app_event app_event;

     if (evt_hvc->handle == char_handle_cfg_out.value_handle) {
	  app_event.event_type = APP_EVENT_INDICATION_CFG_OUT_RCVD;
	  app_event_queue_add(&app_event_queue, app_event);
     }
}

static void ble_evt_handler(ble_evt_t *ble_evt)
{
     ble_gatts_evt_write_t *evt_write;
     struct app_event app_event;

     switch (ble_evt->header.evt_id) {
     case BLE_GAP_EVT_CONNECTED:
	  conn_handle = ble_evt->evt.gap_evt.conn_handle;
	  app_event.event_type = APP_EVENT_CLIENT_CONNECTED;
	  app_event_queue_add(&app_event_queue, app_event);
	  break;
     case BLE_GAP_EVT_DISCONNECTED:
	  conn_handle = BLE_CONN_HANDLE_INVALID;
	  app_event.event_type = APP_EVENT_CLIENT_DISCONNECTED;
	  app_event_queue_add(&app_event_queue, app_event);
	  break;
     case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
	  // Pairing not supported.
	  sd_ble_gap_sec_params_reply(conn_handle, 
				      BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP,
				      NULL, NULL);
	  break;
     case BLE_GATTS_EVT_WRITE:
          evt_write = &ble_evt->evt.gatts_evt.params.write;
	  char_cfg_in_write_evt(evt_write);
	  char_unlock_write_evt(evt_write);
	  cccd_cfg_out_write_evt(evt_write);
	  cccd_nonce_write_evt(evt_write);
	  break;
     case BLE_GATTS_EVT_HVC:
	  // Indication has been acknowledged by the client.
	  nonce_indication_hvc_evt(&ble_evt->evt.gatts_evt.params.hvc);
	  cfg_out_indication_hvc_evt(&ble_evt->evt.gatts_evt.params.hvc);
	  break;
     case BLE_GATTS_EVT_SYS_ATTR_MISSING:
	  // No system attributes have been stored.
	  sd_ble_gatts_sys_attr_set(conn_handle, NULL, 0, 0);
	  break;
     case BLE_GAP_EVT_TIMEOUT:
	  // TODO: Should we do something?
	  break;
    }
}

static void ble_stack_init()
{
     // The softdevice uses RTC0 (32 kHz real-time clock) for timing.
     // We use an external crystal with 20 ppm accuracy.
     SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, false);

     // Enable BLE stack. 
     ble_enable_params_t ble_enable_params;
     memset(&ble_enable_params, 0, sizeof(ble_enable_params));
     if (sd_ble_enable(&ble_enable_params) != NRF_SUCCESS)
	  die();
     
     // Set Bluetooth address of device.
     ble_gap_addr_t addr;
     if (sd_ble_gap_address_get(&addr) != NRF_SUCCESS)
	  die();
     if (sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_NONE, &addr) !=
	 NRF_SUCCESS)
	  die();
     
     // Subscribe for BLE events.
     if (softdevice_ble_evt_handler_set(ble_evt_handler) != NRF_SUCCESS)
	  die();
     
     // Subscribe for system events (required to dispatch pstorage events)
     if (softdevice_sys_evt_handler_set(sys_evt_dispatch) !=
	 NRF_SUCCESS)
	  die();
}

static void gap_init()
{
     ble_gap_conn_params_t gap_conn_params;
     ble_gap_conn_sec_mode_t sec_mode;

     // Open link, no encryption required on BLE layer. 
     // Security is handled only on the application layer. 
     BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
     
     // Set device name.
     if (sd_ble_gap_device_name_set(&sec_mode,
				    (const uint8_t *) DEVICE_NAME,
				    strlen(DEVICE_NAME)) != NRF_SUCCESS)
	  die();
     
     // Set connection parameters.
     memset(&gap_conn_params, 0, sizeof(gap_conn_params));
     gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
     gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
     gap_conn_params.slave_latency = SLAVE_LATENCY;
     gap_conn_params.conn_sup_timeout = CONN_SUP_TIMEOUT;     
     if (sd_ble_gap_ppcp_set(&gap_conn_params) != NRF_SUCCESS)
	  die();
}

static void add_characteristic_nonce(uint16_t service_handle)
{
     // Characteristic UUID.
     ble_uuid_t ble_uuid;
     ble_uuid.type = uuid_type;
     ble_uuid.uuid = UUID_CHARACTERISTIC_NONCE;

     // Define characteristic presentation format.
     // The nonce is a 16 byte number, which is declared as opaque struct.
     ble_gatts_char_pf_t char_presentation_format;
     memset(&char_presentation_format, 0, sizeof(char_presentation_format));
     char_presentation_format.format = BLE_GATT_CPF_FORMAT_STRUCT;
     char_presentation_format.exponent = 0;
     char_presentation_format.unit = 0x2700; // unitless

     // Define CCCD attributes. 
     // CCCD (Client Characteristic Configuration Descriptor) is used by the
     // client to enable notifications or indications by writing a flag to the
     // CCCD attribute.
     ble_gatts_attr_md_t cccd_meta_data;
     memset(&cccd_meta_data, 0, sizeof(cccd_meta_data));
     // CCCD must be readanle and writeable. 
     BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_meta_data.read_perm);
     BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_meta_data.write_perm);
     cccd_meta_data.vloc = BLE_GATTS_VLOC_STACK;

     // Define characteristic meta data.
     // The nonce is only readable.
     ble_gatts_char_md_t char_meta_data;
     memset(&char_meta_data, 0, sizeof(char_meta_data));
     char_meta_data.char_props.read = 1;
     char_meta_data.char_props.write = 0;
     char_meta_data.char_props.notify = 0;
     char_meta_data.char_props.indicate = 1;
     char_meta_data.p_char_user_desc = NULL;
     char_meta_data.p_char_pf = &char_presentation_format;
     char_meta_data.p_user_desc_md = NULL;
     // CCCD (Client Characteristic Configuration Descriptor) needs to be 
     // set for characteristics allowing for notifications and indications.
     char_meta_data.p_cccd_md = &cccd_meta_data;
     char_meta_data.p_sccd_md = NULL;

     // Define attribute meta data. 
     ble_gatts_attr_md_t char_attr_meta_data;
     memset(&char_attr_meta_data, 0, sizeof(char_attr_meta_data));
     // No security needed. All security implemented on the application layer.
     BLE_GAP_CONN_SEC_MODE_SET_OPEN(&char_attr_meta_data.read_perm);
     BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&char_attr_meta_data.write_perm);
     // value location
     char_attr_meta_data.vloc = BLE_GATTS_VLOC_STACK;
     // always request read authorization from application 
     char_attr_meta_data.rd_auth = 0;
     // always request write authorization from application
     char_attr_meta_data.wr_auth = 0;
     // variable length attribute
     char_attr_meta_data.vlen = 0;

     // Define characteristic attributes. 
     // Nonce is a fixed length 16 byte data structure.
     ble_gatts_attr_t char_attributes;
     memset(&char_attributes, 0, sizeof(char_attributes));
     char_attributes.p_uuid = &ble_uuid;
     char_attributes.p_attr_md = &char_attr_meta_data;
     char_attributes.init_len = MAX_LENGTH_NONCE_CHAR;
     char_attributes.init_offs = 0;
     char_attributes.max_len = MAX_LENGTH_NONCE_CHAR;
     // For attributes managed by the application (BLE_GATTS_VLOC_USER)
     // rather than the BLE stack, set a pointer to the memory location here.
     char_attributes.p_value = NULL;

     // Add characteristic to service.
     if (sd_ble_gatts_characteristic_add(service_handle,
					 &char_meta_data,
					 &char_attributes,
					 &char_handle_nonce) != NRF_SUCCESS)
	  die();

     // Set initial characteristic value.
     //set_nonce_char();
}

static void add_characteristic_unlock(uint16_t service_handle)
{
     // Characteristic UUID.
     ble_uuid_t ble_uuid;
     ble_uuid.type = uuid_type;
     ble_uuid.uuid = UUID_CHARACTERISTIC_UNLOCK;

     // Define characteristic presentation format.
     // This characteristic transports a 256 bit HMAC to authenticate the
     // door open request. 256 bit = 32 bytes are too long for a 
     // characteristic. So we split up the request into two 16 byte write
     // operations to the characteristic. Moreover, we include the key 
     // and HMAC part number in the request as another byte. Thus, this is 
     // a 18 byte opaque struct.
     ble_gatts_char_pf_t char_presentation_format;
     memset(&char_presentation_format, 0, sizeof(char_presentation_format));
     char_presentation_format.format = BLE_GATT_CPF_FORMAT_STRUCT;
     char_presentation_format.exponent = 0;
     char_presentation_format.unit = 0x2700; // unitless

     // Define characteristic meta data.
     // The Unlock characteristic must support writing to receive authentication
     // data.
     ble_gatts_char_md_t char_meta_data;
     memset(&char_meta_data, 0, sizeof(char_meta_data));
     char_meta_data.char_props.read = 1;
     char_meta_data.char_props.write = 1;
     char_meta_data.char_props.notify = 0;
     char_meta_data.char_props.indicate = 0;
     char_meta_data.p_char_user_desc = NULL;
     char_meta_data.p_char_pf = &char_presentation_format;
     char_meta_data.p_user_desc_md = NULL;
     // CCCD (Client Characteristic Configuration Descriptor) only needs to be 
     // set for characteristics allowing for notifications and indications.
     char_meta_data.p_cccd_md = NULL;
     char_meta_data.p_sccd_md = NULL;

     // Define attribute meta data. 
     ble_gatts_attr_md_t char_attr_meta_data;
     memset(&char_attr_meta_data, 0, sizeof(char_attr_meta_data));
     // No security needed. All security implemented on the application layer.
     BLE_GAP_CONN_SEC_MODE_SET_OPEN(&char_attr_meta_data.read_perm);
     BLE_GAP_CONN_SEC_MODE_SET_OPEN(&char_attr_meta_data.write_perm);
     // value location
     char_attr_meta_data.vloc = BLE_GATTS_VLOC_STACK;
     // always request read authorization from application 
     char_attr_meta_data.rd_auth = 0;
     // always request write authorization from application
     char_attr_meta_data.wr_auth = 0;
     // variable length attribute
     char_attr_meta_data.vlen = 0;

     // Define characteristic attributes. 
     ble_gatts_attr_t char_attributes;
     memset(&char_attributes, 0, sizeof(char_attributes));
     char_attributes.p_uuid = &ble_uuid;
     char_attributes.p_attr_md = &char_attr_meta_data;
     char_attributes.init_len = MAX_LENGTH_UNLOCK_CHAR;
     char_attributes.init_offs = 0;
     char_attributes.max_len = MAX_LENGTH_UNLOCK_CHAR;
     // For attributes managed by the application (BLE_GATTS_VLOC_USER)
     // rather than the BLE stack, set a pointer to the memory location here.
     char_attributes.p_value = NULL;

     // Add characteristic to service.
     if (sd_ble_gatts_characteristic_add(service_handle,
					 &char_meta_data,
					 &char_attributes,
					 &char_handle_unlock) != NRF_SUCCESS)
	  die();
}

static void add_characteristic_cfg_in(uint16_t service_handle)
{
     // Characteristic UUID.
     ble_uuid_t ble_uuid;
     ble_uuid.type = uuid_type;
     ble_uuid.uuid = UUID_CHARACTERISTIC_CFG_IN;

     // Define characteristic presentation format.
     // This characteristic transports a 32 byte public key (split into two
     // 16 byte parts due to characteristic length restrictions) for
     // key exchange, plus one byte defining the key number, and one byte 
     // defining the key part number (0 or 1). Thus, this is an 18 byte opaque 
     // struct.
     ble_gatts_char_pf_t char_presentation_format;
     memset(&char_presentation_format, 0, sizeof(char_presentation_format));
     char_presentation_format.format = BLE_GATT_CPF_FORMAT_STRUCT;
     char_presentation_format.exponent = 0;
     char_presentation_format.unit = 0x2700; // unitless

     // Define characteristic meta data.
     // The config-in characteristic must support writing to receive
     // configuration data.
     ble_gatts_char_md_t char_meta_data;
     memset(&char_meta_data, 0, sizeof(char_meta_data));
     char_meta_data.char_props.read = 1;
     char_meta_data.char_props.write = 1;
     char_meta_data.char_props.notify = 0;
     char_meta_data.char_props.indicate = 0;
     char_meta_data.p_char_user_desc = NULL;
     char_meta_data.p_char_pf = &char_presentation_format;
     char_meta_data.p_user_desc_md = NULL;
     // CCCD (Client Characteristic Configuration Descriptor) only needs to be 
     // set for characteristics allowing for notifications and indications.
     char_meta_data.p_cccd_md = NULL;
     char_meta_data.p_sccd_md = NULL;

     // Define attribute meta data. 
     ble_gatts_attr_md_t char_attr_meta_data;
     memset(&char_attr_meta_data, 0, sizeof(char_attr_meta_data));
     // No security needed. All security implemented on the application layer.
     BLE_GAP_CONN_SEC_MODE_SET_OPEN(&char_attr_meta_data.read_perm);
     BLE_GAP_CONN_SEC_MODE_SET_OPEN(&char_attr_meta_data.write_perm);
     // value location
     char_attr_meta_data.vloc = BLE_GATTS_VLOC_STACK;
     // always request read authorization from application 
     char_attr_meta_data.rd_auth = 0;
     // always request write authorization from application
     char_attr_meta_data.wr_auth = 0;
     // variable length attribute
     char_attr_meta_data.vlen = 0;

     // Define characteristic attributes. 
     ble_gatts_attr_t char_attributes;
     memset(&char_attributes, 0, sizeof(char_attributes));
     char_attributes.p_uuid = &ble_uuid;
     char_attributes.p_attr_md = &char_attr_meta_data;
     char_attributes.init_len = MAX_LENGTH_CFG_IN_CHAR;
     char_attributes.init_offs = 0;
     char_attributes.max_len = MAX_LENGTH_CFG_IN_CHAR;
     // For attributes managed by the application (BLE_GATTS_VLOC_USER)
     // rather than the BLE stack, set a pointer to the memory location here.
     char_attributes.p_value = NULL;

     // Add characteristic to service.
     if (sd_ble_gatts_characteristic_add(service_handle,
					 &char_meta_data,
					 &char_attributes,
					 &char_handle_cfg_in) != NRF_SUCCESS)
	  die();
}

static void add_characteristic_cfg_out(uint16_t service_handle)
{
     // Characteristic UUID.
     ble_uuid_t ble_uuid;
     ble_uuid.type = uuid_type;
     ble_uuid.uuid = UUID_CHARACTERISTIC_CFG_OUT;

     // Define characteristic presentation format.
     // This characteristic transports a 32 byte public key (split into two
     // 16 byte parts due to characteristic length restrictions) for
     // key exchange. Two more bytes are added to define the key number and 
     // key part number of the split key. Thus, this is a 18 byte opaque struct.
     ble_gatts_char_pf_t char_presentation_format;
     memset(&char_presentation_format, 0, sizeof(char_presentation_format));
     char_presentation_format.format = BLE_GATT_CPF_FORMAT_STRUCT;
     char_presentation_format.exponent = 0;
     char_presentation_format.unit = 0x2700; // unitless

     // Define CCCD attributes. 
     // CCCD (Client Characteristic Configuration Descriptor) is used by the
     // client to enable notifications or indications by writing a flag to the
     // CCCD attribute.
     ble_gatts_attr_md_t cccd_meta_data;
     memset(&cccd_meta_data, 0, sizeof(cccd_meta_data));
     // CCCD must be readable and writeable. 
     BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_meta_data.read_perm);
     BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_meta_data.write_perm);
     cccd_meta_data.vloc = BLE_GATTS_VLOC_STACK;

     // Define characteristic meta data.
     // The config-out characteristic must suppport notifications for sending
     // config data to the client.
     ble_gatts_char_md_t char_meta_data;
     memset(&char_meta_data, 0, sizeof(char_meta_data));
     char_meta_data.char_props.read = 1;
     char_meta_data.char_props.write = 0;
     char_meta_data.char_props.notify = 0;
     char_meta_data.char_props.indicate = 1;
     char_meta_data.p_char_user_desc = NULL;
     char_meta_data.p_char_pf = &char_presentation_format;
     char_meta_data.p_user_desc_md = NULL;
     // CCCD (Client Characteristic Configuration Descriptor) needs to be 
     // set for characteristics allowing for notifications and indications.
     char_meta_data.p_cccd_md = &cccd_meta_data;
     char_meta_data.p_sccd_md = NULL;

     // Define attribute meta data. 
     ble_gatts_attr_md_t char_attr_meta_data;
     memset(&char_attr_meta_data, 0, sizeof(char_attr_meta_data));
     // No security needed. All security implemented on the application layer.
     BLE_GAP_CONN_SEC_MODE_SET_OPEN(&char_attr_meta_data.read_perm);
     BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&char_attr_meta_data.write_perm);
     // value location
     char_attr_meta_data.vloc = BLE_GATTS_VLOC_STACK;
     // always request read authorization from application 
     char_attr_meta_data.rd_auth = 0;
     // always request write authorization from application
     char_attr_meta_data.wr_auth = 0;
     // variable length attribute
     char_attr_meta_data.vlen = 0;

     // Define characteristic attributes. 
     ble_gatts_attr_t char_attributes;
     memset(&char_attributes, 0, sizeof(char_attributes));
     char_attributes.p_uuid = &ble_uuid;
     char_attributes.p_attr_md = &char_attr_meta_data;
     char_attributes.init_len = MAX_LENGTH_CFG_OUT_CHAR;
     char_attributes.init_offs = 0;
     char_attributes.max_len = MAX_LENGTH_CFG_OUT_CHAR;
     // For attributes managed by the application (BLE_GATTS_VLOC_USER)
     // rather than the BLE stack, set a pointer to the memory location here.
     char_attributes.p_value = NULL;

     // Add characteristic to service.
     if (sd_ble_gatts_characteristic_add(service_handle,
					 &char_meta_data,
					 &char_attributes,
					 &char_handle_cfg_out) != NRF_SUCCESS)
	  die();
}

static void service_init()
{
     uint32_t err_code;
     
     // Add base UUID to list of base UUIDs.
     // The uuid_type field filled by this function call can be used later to
     // refer to this base UUID.
     ble_uuid128_t base_uuid = {UUID_BASE};
     err_code = sd_ble_uuid_vs_add(&base_uuid, &uuid_type);
     if (err_code != NRF_SUCCESS)
	  die();

     // Build 128 bit service UUID by referring to base UUID using uuid_type
     // and specifying the two bytes that will replace byte 12 and 13 of the
     // base UUID. 
     ble_uuid_t ble_uuid;
     ble_uuid.type = uuid_type;
     ble_uuid.uuid = UUID_SERVICE;
     
     err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, 
					 &ble_uuid, &service_handle);
     if (err_code != NRF_SUCCESS)
	  die();
     
     // Add characteristics to service.
     add_characteristic_nonce(service_handle);
     add_characteristic_unlock(service_handle);
     add_characteristic_cfg_in(service_handle);
     add_characteristic_cfg_out(service_handle);
}

/*
static void conn_params_error_handler(uint32_t nrf_error)
{
     die();
}

static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED) {
	 if (sd_ble_gap_disconnect(m_conn_handle, 
				   BLE_HCI_CONN_INTERVAL_UNACCEPTABLE) !=
	     NRF_SUCCESS)
	      die();
    }
}

static void conn_params_init()
{
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));
    cp_init.p_conn_params = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail = false;
    cp_init.evt_handler = on_conn_params_evt;
    cp_init.error_handler = conn_params_error_handler;

    if (ble_conn_params_init(&cp_init) != NRF_SUCCESS)
	 die();
}
*/

static void advertising_init(void)
{
     ble_uuid_t adv_uuids[] = {{UUID_SERVICE, uuid_type}};
     
     ble_advdata_t advdata;
     memset(&advdata, 0, sizeof(advdata));
     advdata.name_type = BLE_ADVDATA_FULL_NAME;
     advdata.include_appearance = false;
     // LE General Discoverable Mode.
     advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
     // Send complete set of UUIDs.
     advdata.uuids_complete.uuid_cnt = sizeof(adv_uuids)/sizeof(adv_uuids[0]);
     advdata.uuids_complete.p_uuids = adv_uuids;
     
     // No scan response data needs to be defined (second parameter) since 
     // everything fits into the advertisement message (the scan response can 
     // be requested by the central device to get more information from the 
     // peripheral).
     if (ble_advdata_set(&advdata, NULL) != NRF_SUCCESS)
	  die();
}

static void lock_action_timer_evt_handler(void *p_context)
{
     UNUSED_PARAMETER(p_context);
     struct app_event app_event = {.event_type = APP_EVENT_LOCK_ACTION_TIMEOUT};
     app_event_queue_add(&app_event_queue, app_event);
}

static void auth_timer_evt_handler(void *p_context)
{
     UNUSED_PARAMETER(p_context);
     struct app_event app_event = {.event_type = APP_EVENT_AUTH_TIMEOUT};
     app_event_queue_add(&app_event_queue, app_event);
}

static void timers_init()
{
     // Initialize application timer using RTC1 (RTC0 is used by the
     // BLE softdevice).
     APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_QUEUE_SIZE, false);

     if (app_timer_create(&lock_action_timer, APP_TIMER_MODE_SINGLE_SHOT,
			  lock_action_timer_evt_handler) != NRF_SUCCESS)
	  die();

     if (app_timer_create(&auth_timer, APP_TIMER_MODE_SINGLE_SHOT,
			  auth_timer_evt_handler) != NRF_SUCCESS)
	  die();
}

static void start_lock_action_timer()
{
     if (app_timer_start(lock_action_timer, LOCK_ACTION_TIMER_TIMEOUT, NULL) !=
	 NRF_SUCCESS)
	  die();
}

static void start_auth_timer()
{
     if (app_timer_start(auth_timer, AUTH_TIMER_TIMEOUT, NULL) !=
	 NRF_SUCCESS)
	  die();
}

static void stop_auth_timer()
{
     app_timer_stop(auth_timer);
}

static void button_evt_handler(uint8_t pin_no, uint8_t button_action)
{
     struct app_event app_event;

     switch (pin_no) {
     case PIN_BUTTON_RED :
	  if (APP_BUTTON_PUSH == button_action) {
	       app_event.event_type = APP_EVENT_BUTTON_RED_PRESSED;
	       app_event_queue_add(&app_event_queue, app_event);
	  }
	  break;
     case PIN_BUTTON_GREEN :
	  if (APP_BUTTON_PUSH == button_action) {
	       app_event.event_type = APP_EVENT_BUTTON_GREEN_PRESSED;
	       app_event_queue_add(&app_event_queue, app_event);
	  }
	  break;
     }
}

static void buttons_init()
{
     // Switch is active low -> second parameter = false.
     // Enable pull up resistor -> third parameter = NRF_GPIO_PIN_PULLUP.
     static app_button_cfg_t buttons[] = {
	  {PIN_BUTTON_RED, false, NRF_GPIO_PIN_PULLUP, button_evt_handler},
	  {PIN_BUTTON_GREEN, false, NRF_GPIO_PIN_PULLUP, button_evt_handler}
     };

     if (app_button_init(buttons, sizeof(buttons) / sizeof(buttons[0]),
			 BUTTON_DETECTION_DELAY) != NRF_SUCCESS)
	  die();
}

static void start_button_event_detection()
{
     if (app_button_enable() != NRF_SUCCESS)
	  die();
}

static void create_nonce()
{
     uint8_t available;
     unsigned int remaining = NONCE_LENGTH;
     unsigned int offset = 0;
     while (remaining > 0) {
	  if (sd_rand_application_bytes_available_get(&available) != 
	      NRF_SUCCESS)
	       die();
	  uint8_t length = (remaining < available) ? remaining : available;
	  if (sd_rand_application_vector_get(&nonce[offset], length) != 
	      NRF_SUCCESS)
	       die();
	  remaining -= length;
	  offset += length;
     }
}

static void ecdh_secret_key(uint8_t secret_key[ECDH_KEY_LENGTH])
{
    uint8_t available;
    unsigned int remaining = ECDH_KEY_LENGTH;
    unsigned int offset = 0;
    while (remaining > 0) {
	 if (sd_rand_application_bytes_available_get(&available) != NRF_SUCCESS)
	      die();
	 uint8_t length = (remaining < available) ? remaining : available;
	 if (sd_rand_application_vector_get(&secret_key[offset], length) != 
	     NRF_SUCCESS)
	      die();
	 remaining -= length;
	 offset += length;
    }

    // We need to clear bits 0-2 and set bit 254 to prevent small-subgroup 
    // attacks and timing attacks, respectively.
    secret_key[0] &= 248;
    secret_key[ECDH_KEY_LENGTH-1] &= 127;
    secret_key[ECDH_KEY_LENGTH-1] |= 64;
}

static void ecdh_public_key(uint8_t public_key[ECDH_KEY_LENGTH], 
			    const uint8_t secret_key[ECDH_KEY_LENGTH])
{
     // In the following call, base is 9.
     crypto_scalarmult_curve25519_base(public_key, secret_key);
}

static void ecdh_shared_secret(uint8_t shared_secret[ECDH_KEY_LENGTH],
			       const uint8_t my_secret_key[ECDH_KEY_LENGTH],
			       const uint8_t other_public_key[ECDH_KEY_LENGTH])
{
     crypto_scalarmult_curve25519(shared_secret, my_secret_key, 
				  other_public_key);
}

static void display_init()
{
     hd44780_init(&lcd);
}

static void display_on()
{
     hd44780_display_on_off(&lcd, true, false, false);
}

static void display_off()
{
     hd44780_display_on_off(&lcd, false, false, false);
}

static void display_text(const char *text1, unsigned int length1,
			 const char *text2, unsigned int length2)
{
     hd44780_clear_display(&lcd);
     if (text1 != NULL)
	  hd44780_print_line(&lcd, text1, length1, 0);
     if (text2 != NULL)
	  hd44780_print_line(&lcd, text2, length2, 1);
}

static void nonce_init()
{
     create_nonce();
}

static void binary_to_hexstr(char *str, const uint8_t *binary, unsigned int len)
{
     char *nextchar = str;
     for (unsigned int i = 0; i < len; i++) {
	  nextchar += sprintf(nextchar, "%02X", binary[i]);
     }
}

static void display_shared_secret_hash() 
{
     // As checksum, we use a SHA512 hash of the shared secret, truncated to 
     // the lower 8 bytes.
     uint8_t hash[SHA512_HASH_LENGTH];
     crypto_hash_sha512(hash, keyexchange_shared_secret, 
			sizeof(keyexchange_shared_secret));

     // Checksum is displayed as 16 hex digits with the lowest-order byte
     // at string index 0/1, i.e., hash[0] is displayed leftmost.
     // Null terminated string (16+1), so we can use std. string functions.
     char str[17]; 
     binary_to_hexstr(str, hash, 8);
     display_text("Key checksum", 12, str, 16);
}

static void store_key(unsigned int keyno)
{
     const unsigned int offset = sizeof(pstore_preamble) + 
	  keyno*ECDH_KEY_LENGTH;
     is_pstore_ready = false;

     // Should we use the store or update operation? 
     // Store: "Flash memory is unreliable when writing to a block already 
     // containing data, a clear operation is needed."
     // Update: "When using the update routine the Persistent Storage Manager 
     // will use a swap area to conserve the blocks not affected. The data 
     // page backed up in the swap area, the data page is erased, the non 
     // affected blocks are copied back, then the updated block is stored. 
     // It is not possible to update multiple blocks in one API call. Module 
     // detects if backing up the page is needed or not and optimizes 
     // accordingly."
     // So write is unreliable, update is reliable. But update is very slow 
     // compared to write:
     // "Flash write operation takes up to 46.3 us and flash erase takes up 
     // to 22.3 ms."
     // Moreover, there's a limit number of erase-write cycles, specifically
     // for nRF51 "20,000 minimum write/erase cycles".
     // We will not store keys very often. So we should opt for reliability
     // using the update operation in a productive system.
     /*
     if (pstorage_store(&pstore_handle, keys[keyno], ECDH_KEY_LENGTH, 
			offset) != NRF_SUCCESS)
	  die();
     */
     if (pstorage_update(&pstore_handle, keys[keyno], ECDH_KEY_LENGTH, 
                         offset) != NRF_SUCCESS)
	  die();
}

static void pstore_cb_handler(pstorage_handle_t *handle, uint8_t op_code,
			      uint32_t result, uint8_t *p_data, 
			      uint32_t data_len)
{
     if (result != NRF_SUCCESS)
	  die();

     // The following switch statement is supposed to fall through.
     switch(op_code) {
     case PSTORAGE_LOAD_OP_CODE :
     case PSTORAGE_STORE_OP_CODE :
     case PSTORAGE_CLEAR_OP_CODE :
     case PSTORAGE_UPDATE_OP_CODE :
	  is_pstore_ready = true;
	  if (app_state != booting) {
	       struct app_event app_event = 
		    {.event_type = APP_EVENT_PSTORE_READY};
	       app_event_queue_add(&app_event_queue, app_event);
	  }
     }
}

static bool is_key_valid(unsigned int keyno)
{
     // An all zero pattern indicates an invalid key.
     for (unsigned int i = 0; i < ECDH_KEY_LENGTH; i++)
	  if (keys[keyno][i] != 0x00)
	       return true;

     // All bytes are zero -> invalid key.
     return false;
}

static bool load_keys()
{
     uint8_t preamble[sizeof(pstore_preamble)];

     // Read preamble (random pattern) to see whether keystore contains valid 
     // keys. If storage is read for the first time without previous writing,
     // it is highly unlikely that the pseudo-random pattern will be found.

     is_pstore_ready = false;
     if (pstorage_load(preamble, &pstore_handle, sizeof(preamble), 0)
	 != NRF_SUCCESS)
	  die();
     // Busy waiting for pstore to become ready.
     while (!is_pstore_ready);
     for (unsigned int i = 0; i < sizeof(preamble); i++) {
	  if (pstore_preamble[i] != preamble[i])
	       return false;
     }

     // Preamble OK. Stored data is valid.
     
     // Load keys.
     unsigned int storage_offset = sizeof(preamble);
     uint8_t flag = 0x01;
     for (unsigned int i = 0; i < KEY_COUNT; i++) {
	  is_pstore_ready = false;
	  if (pstorage_load(keys[i], &pstore_handle, ECDH_KEY_LENGTH, 
			    storage_offset) != NRF_SUCCESS)
	       die();
	  // Busy waiting for pstore to become ready.
	  while (!is_pstore_ready);
	  if (is_key_valid(i))
	       keys_valid |= flag;
	  flag <<= 1;
	  storage_offset += ECDH_KEY_LENGTH;
     }

     return true;
}

static void format_pstore()
{
     is_pstore_ready = false;
     if (pstorage_clear(&pstore_handle, sizeof(pstore_preamble) + 
			KEY_COUNT*ECDH_KEY_LENGTH) != NRF_SUCCESS)
	  die();
     while (!is_pstore_ready);

     is_pstore_ready = false;
     if (pstorage_store(&pstore_handle, pstore_preamble, 
			sizeof(pstore_preamble), 0) != NRF_SUCCESS)
	  die();
     while (!is_pstore_ready);

     unsigned int offset = sizeof(pstore_preamble);
     for (unsigned int i = 0; i < KEY_COUNT; i++) {
	  is_pstore_ready = false;
	  if (pstorage_store(&pstore_handle, keys[i], 
			     ECDH_KEY_LENGTH, offset) != NRF_SUCCESS)
	       die();
	  while (!is_pstore_ready);
	  offset += ECDH_KEY_LENGTH;
     }
}

static void pstore_init()
{
     if (pstorage_init() != NRF_SUCCESS)
	  die();

     pstorage_module_param_t param;
     param.block_size = sizeof(pstore_preamble) + KEY_COUNT*ECDH_KEY_LENGTH;
     param.block_count = 1;
     param.cb = pstore_cb_handler;
     if (pstorage_register(&param, &pstore_handle) != NRF_SUCCESS)
	  die();

     if (!load_keys()) {
	  // First time usage of pstore (nothing written yet to pstore).
	  format_pstore();
	  // No data could be read from pstore -> all keys are invalid.
	  for (unsigned int key = 0; key < KEY_COUNT; key++)
	       memset(keys[key], 0, ECDH_KEY_LENGTH);
	  keys_valid = 0;
     }
}

static void indicate_nonce()
{
     ble_gatts_hvx_params_t params;
     uint16_t len = sizeof(nonce);
     
     // Send nonce value as indication.
     memset(&params, 0, sizeof(params));
     params.type = BLE_GATT_HVX_INDICATION;
     params.handle = char_handle_nonce.value_handle;
     params.p_data = nonce;
     params.p_len = &len;
     if (sd_ble_gatts_hvx(conn_handle, &params) != NRF_SUCCESS)
	  die();
}

static void indicate_public_key(unsigned int part)
{
     ble_gatts_hvx_params_t params;
     uint16_t len = MAX_LENGTH_CFG_OUT_CHAR;
     uint8_t data[MAX_LENGTH_CFG_OUT_CHAR];
     
     if (part == 0) {
	  // Send first 18 bytes of public key as indication.
	  data[0] = keyexchange_key_no;
	  data[1] = 0; // 0 = part 1
	  memcpy(&data[2], &keyexchange_server_public_key[0], 16);
     } else {
	  // Send second 18 bytes of public key as indication.
	  data[1] = 1; // 1 = part 2
	  memcpy(&data[2], &keyexchange_server_public_key[16], 16);
     }

     memset(&params, 0, sizeof(params));
     params.type = BLE_GATT_HVX_INDICATION;
     params.handle = char_handle_cfg_out.value_handle;
     params.p_data = data;
     params.p_len = &len;
     if (sd_ble_gatts_hvx(conn_handle, &params) != NRF_SUCCESS)
	  die();
}

static void lock_action_start()
{
     // The lock is active high.
     nrf_gpio_pin_set(PIN_LOCK);
     start_lock_action_timer();
}

static void lock_action_stop()
{
     // The lock is active high.
     nrf_gpio_pin_clear(PIN_LOCK);
}

static bool check_auth()
{
     if ( ((1 << unlock_key_no)&keys_valid) == 0)
	  return false;

     // crypto_auth_hmacsha512256_verify() returns 0 on successful
     // verification.
     if (crypto_auth_hmacsha512256_verify(unlock_hmac_client, 
					  nonce, NONCE_LENGTH, 
					  keys[unlock_key_no]) == 0)
	  return true;
     else
	  return false;
}

/*
static void set_nonce_char() 
{
     ble_gatts_value_t value;
     memset(&value, 0, sizeof(value));
     value.len = sizeof(nonce);
     value.offset  = 0;
     value.p_value = nonce;
     if (sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID, 
				char_handle_nonce.value_handle, &value) != 
	 NRF_SUCCESS)
	  die();
}
*/

static void state_transition(struct app_event event) 
{
     switch (app_state) {
     case idle :
	  if (event.event_type == APP_EVENT_BUTTON_RED_PRESSED) {
	       display_text("Waiting for", 11, "client key", 10);
	       app_state = cfg_wait_connection;
	  } else if (event.event_type == APP_EVENT_CLIENT_CONNECTED) {
	       display_text("Authentication", 14, NULL, 0);
	       start_auth_timer();
	       // If we sometimes use bonding, note that bonded devices might 
	       // already have subscribed when they connect. Subscriptions 
	       // are stored for bonded devices. 
	       app_state = auth_wait_subscription;
	  }
	  break;
     case cfg_wait_connection :
	  if (event.event_type == APP_EVENT_BUTTON_RED_PRESSED) {
	       // At this stage, another button press will abort configuration.
	       app_state = idle;
	       display_text("Ready", 5, NULL, 0);	       
	  } else if (event.event_type == APP_EVENT_CLIENT_CONNECTED) {
	       // If we sometimes use bonding, note that bonded devices might 
	       // already have subscribed when they connect. Subscriptions 
	       // are stored for bonded devices. 
	       app_state = cfg_wait_subscription;
	  }
	  break;
     case cfg_wait_subscription :
	  if (event.event_type == APP_EVENT_BUTTON_RED_PRESSED) {
	       // At this stage, another button press will abort configuration.
	       // We need to disconnect the already connected client.
	       if (sd_ble_gap_disconnect(
			conn_handle, 
			BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION) !=
		   NRF_SUCCESS)
		    die();
	       app_state = aborted_wait_disconnect;
	  } else if (event.event_type == APP_EVENT_CLIENT_DISCONNECTED) {
	       // Configuration aborted through client disconnection.
	       app_state = idle;
	       start_advertising();
	       display_text("Ready", 5, NULL, 0);
	  } else if (event.event_type == APP_EVENT_SUBSCRIBED_CFG_OUT)
	       app_state = cfg_wait_key_part1;
	  break;
     case cfg_wait_key_part1 :
	  if (event.event_type == APP_EVENT_BUTTON_RED_PRESSED) {
	       // At this stage, another button press will abort configuration.
	       // We need to disconnect the already connected client.
	       if (sd_ble_gap_disconnect(
			conn_handle, 
			BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION) !=
		   NRF_SUCCESS)
		    die();
	       app_state = aborted_wait_disconnect;
	  } else if (event.event_type == APP_EVENT_CLIENT_DISCONNECTED) {
	       // Configuration aborted through client disconnection.
	       app_state = idle;
	       start_advertising();
	       display_text("Ready", 5, NULL, 0);
	  } else if (event.event_type == APP_EVENT_KEY_PART_RCVD)
	       app_state = cfg_wait_key_part2;
	  break;
     case cfg_wait_key_part2 : 
	  if (event.event_type == APP_EVENT_BUTTON_RED_PRESSED) {
	       // At this stage, another button press will abort configuration.
	       // We need to disconnect the already connected client.
	       if (sd_ble_gap_disconnect(
			conn_handle, 
			BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION) !=
		   NRF_SUCCESS)
		    die();
	       app_state = aborted_wait_disconnect;
	  } else if (event.event_type == APP_EVENT_CLIENT_DISCONNECTED) {
	       // Configuration aborted through client disconnection.
	       app_state = idle;
	       start_advertising();
	       display_text("Ready", 5, NULL, 0);
	  } else if (event.event_type == APP_EVENT_KEY_PART_RCVD) {
	       // Received public key from client.
	       // Now server calculates its keypair and the shared secret.
	       // The server's public key is then send to the client 
	       // to also let the client calculate the shared secret.
	       display_text("Calculating", 11, "secret", 6);
	       ecdh_secret_key(keyexchange_server_secret_key);
	       ecdh_public_key(keyexchange_server_public_key, 
			       keyexchange_server_secret_key);
	       ecdh_shared_secret(keyexchange_shared_secret, 
				  keyexchange_server_secret_key, 
				  keyexchange_client_public_key);
	       display_shared_secret_hash();
	       // Send server public key as indication to client. 
	       indicate_public_key(0); // Sending part 1 of server key.
	       app_state = cfg_wait_server_key_part1_rcvd;
	  }    
	  break;
     case cfg_wait_server_key_part1_rcvd :
	  if (event.event_type == APP_EVENT_BUTTON_RED_PRESSED) {
	       // At this stage, another button press will abort configuration.
	       // We need to disconnect the already connected client.
	       if (sd_ble_gap_disconnect(
			conn_handle, 
			BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION) !=
		   NRF_SUCCESS)
		    die();
	       app_state = aborted_wait_disconnect;
	  } else if (event.event_type == APP_EVENT_CLIENT_DISCONNECTED) {
	       // Configuration aborted through client disconnection.
	       app_state = idle;
	       start_advertising();
	       display_text("Ready", 5, NULL, 0);
	  } else if (event.event_type == APP_EVENT_INDICATION_CFG_OUT_RCVD) {
	       indicate_public_key(1); // Sending part 2 of server key.
	       app_state = cfg_wait_server_key_part2_rcvd;
	  }
	  break;
     case cfg_wait_server_key_part2_rcvd :
	  if (event.event_type == APP_EVENT_BUTTON_RED_PRESSED) {
	       // At this stage, another button press will abort configuration.
	       // We need to disconnect the already connected client.
	       if (sd_ble_gap_disconnect(
			conn_handle, 
			BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION) !=
		   NRF_SUCCESS)
		    die();
	       app_state = aborted_wait_disconnect;
	  } else if (event.event_type == APP_EVENT_CLIENT_DISCONNECTED) {
	       // Configuration aborted through client disconnection.
	       app_state = idle;
	       start_advertising();
	       display_text("Ready", 5, NULL, 0);
	  } else if (event.event_type == APP_EVENT_INDICATION_CFG_OUT_RCVD) {
	       app_state = cfg_wait_disconnect;
	  }
	  break;
     case cfg_wait_disconnect :
	  // After the client received the server public key, it
	  // should disconnect.
	  if (event.event_type == APP_EVENT_BUTTON_RED_PRESSED) {
	       // At this stage, another button press will abort configuration.
	       // We need to disconnect the already connected client.
	       if (sd_ble_gap_disconnect(
			conn_handle, 
			BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION) !=
		   NRF_SUCCESS)
		    die();
	       app_state = aborted_wait_disconnect;
	  } else if (event.event_type == APP_EVENT_CLIENT_DISCONNECTED) {
	       app_state = cfg_wait_decision;
	  }
	  break;
     case cfg_wait_decision :
	  if (event.event_type == APP_EVENT_BUTTON_RED_PRESSED) {
	       // User aborted.
	       display_text("Ready", 5, NULL, 0);
	       app_state = idle;
	       start_advertising();
	  } else if (event.event_type == APP_EVENT_BUTTON_GREEN_PRESSED) {
	       // User confirmed. 
	       // Make new shared secret effective.
	       memcpy(keys[keyexchange_key_no], keyexchange_shared_secret,
		      ECDH_KEY_LENGTH);
	       uint8_t mask = (1 << keyexchange_key_no);
	       keys_valid |= mask;
	       // Make exchanged shared secret persistent.
	       display_text("Storing key", 11, NULL, 0);
	       store_key(keyexchange_key_no);
	       app_state = cfg_wait_key_store;
	  }
	  break;
     case cfg_wait_key_store :
	  if (event.event_type == APP_EVENT_PSTORE_READY) {
	       display_text("Ready", 5, NULL, 0);
	       app_state = idle;
	       start_advertising();
	  }
	  break;
     case auth_wait_subscription :
          if (event.event_type == APP_EVENT_AUTH_TIMEOUT) {
	       // Timeout -> abort authentication.
	       // Disconnect still connected client.
	       if (sd_ble_gap_disconnect(
			conn_handle, 
			BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION) !=
		   NRF_SUCCESS)
		    die();
	       app_state = aborted_wait_disconnect;
	  } else if (event.event_type == APP_EVENT_CLIENT_DISCONNECTED) {
	       // Authentication aborted through client disconnection.
	       stop_auth_timer();
	       app_state = idle;
	       start_advertising();
	       display_text("Ready", 5, NULL, 0);
	  } else if (event.event_type == APP_EVENT_SUBSCRIBED_NONCE) {
	       // Create a new nonce for next authentication request.
	       create_nonce();
	       // Send nonce to client as indication.
	       indicate_nonce();
	       app_state = auth_wait_nonce_rcvd;
	  }
          break;
     case auth_wait_nonce_rcvd :
          if (event.event_type == APP_EVENT_AUTH_TIMEOUT) {
	       // Timeout -> abort authentication.
	       // Disconnect still connected client.
	       if (sd_ble_gap_disconnect(
			conn_handle, 
			BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION) !=
		   NRF_SUCCESS)
		    die();
	       app_state = aborted_wait_disconnect;
	  } else if (event.event_type == APP_EVENT_CLIENT_DISCONNECTED) {
	       // Authentication aborted through client disconnection.
	       stop_auth_timer();
	       app_state = idle;
	       start_advertising();
	       display_text("Ready", 5, NULL, 0);
	  } else if (event.event_type == APP_EVENT_INDICATION_NONCE_RCVD) {
	       app_state = auth_wait_hmac_part1;
	  }
	  break;
     case auth_wait_hmac_part1 :
	  if (event.event_type == APP_EVENT_AUTH_TIMEOUT) {
	       // Timeout -> abort authentication.
	       // Disconnect still connected client.
	       if (sd_ble_gap_disconnect(
			conn_handle, 
			BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION) !=
		   NRF_SUCCESS)
		    die();
	       app_state = aborted_wait_disconnect;
	  } else if (event.event_type == APP_EVENT_CLIENT_DISCONNECTED) {
	       // Authentication aborted through client disconnection.
	       stop_auth_timer();
	       app_state = idle;
	       start_advertising();
	       display_text("Ready", 5, NULL, 0);
	  } else if (event.event_type == APP_EVENT_HMAC_PART_RCVD)
	       app_state = auth_wait_hmac_part2;
	  break;
     case auth_wait_hmac_part2 :
	  if (event.event_type == APP_EVENT_AUTH_TIMEOUT) {
	       // Timeout -> abort authentication.
	       // Disconnect still connected client.
	       if (sd_ble_gap_disconnect(
			conn_handle, 
			BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION) !=
		   NRF_SUCCESS)
		    die();
	       app_state = aborted_wait_disconnect;
	  } else if (event.event_type == APP_EVENT_CLIENT_DISCONNECTED) {
	       // Authentication aborted through client disconnection.
	       stop_auth_timer();
	       app_state = idle;
	       start_advertising();
	       display_text("Ready", 5, NULL, 0);
	  } else if (event.event_type == APP_EVENT_HMAC_PART_RCVD)
	       app_state = auth_wait_disconnect;
	  break;
     case auth_wait_disconnect :
	  if (event.event_type == APP_EVENT_CLIENT_DISCONNECTED) {
	       stop_auth_timer();
	       if (check_auth()) {
		    display_text("Opening door", 12, NULL, 0);
		    lock_action_start();
		    app_state = auth_wait_lock_action_timeout;
	       } else {
		    display_text("Ready", 5, NULL, 0);
		    app_state = idle;
		    start_advertising();
	       }
	  }
	  break;
     case auth_wait_lock_action_timeout :
	  if (event.event_type == APP_EVENT_LOCK_ACTION_TIMEOUT) {
	       lock_action_stop();
	       app_state = idle;
	       start_advertising();
	       display_text("Ready", 5, NULL, 0);
	  }
	  break;
     case aborted_wait_disconnect :
	  if (event.event_type == APP_EVENT_CLIENT_DISCONNECTED) {
	       app_state = idle;
	       start_advertising();
	       display_text("Ready", 5, NULL, 0);
	  }
	  break;
     default :
	  die();
     }
}

int main(void)
{
     app_state = booting;

     led_init();

     display_init();
     display_on();
     display_text("Booting", 7, NULL, 0);

     timers_init();
     buttons_init();
     lock_init();
     ble_stack_init();
     nonce_init();
     gap_init();
     service_init();
     advertising_init();
     pstore_init();
     app_event_queue_init(&app_event_queue);
	  
     display_text("Ready", 5, NULL, 0);

     // We could trigger a change of connection parameters. However,
     // for the very short interaction between peripheral and central,
     // it might not be worth the effort.
     //conn_params_init();

     // Initialization done. From here on, everything is event-triggered.

     app_state = idle;
     start_button_event_detection();
     start_advertising();

     while (1) {
	  // The following function puts the processor into sleep mode
	  // and waits for interrupts to wake up. Wakeup events include
	  // events from the softdevice, which are processed in the BLE event 
	  // loop, or other events like interrupts from application timers and
	  // pressed buttons.
	  sd_app_evt_wait();

	  // Interrupts create application-level events and put them
	  // into the event queue, which is processed outside the interrupt 
	  // context and with a priority low enough not to block time-critical
	  // operations, e.g., from the softdevice.
	  struct app_event app_event;
	  while (app_event_queue_get(&app_event_queue, &app_event) != -1)
	       state_transition(app_event);
     }
}
