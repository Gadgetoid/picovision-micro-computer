#include "usb.hpp"
#include "bsp/board.h"
#include "tusb.h"

// hid
static int hid_report_id = -1;
bool hid_keyboard_detected = false;
bool hid_mouse_detected = false;
uint8_t hid_keys[6]{};

const char *nibble_to_bitstring[16] = {
  "0000", "0001", "0010", "0011",
  "0100", "0101", "0110", "0111",
  "1000", "1001", "1010", "1011",
  "1100", "1101", "1110", "1111",
};

extern void mouse_callback(int8_t x, int8_t y, uint8_t buttons, int8_t wheel);
extern void keyboard_callback(uint8_t *keys, uint8_t modifiers);

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
  uint16_t vid = 0, pid = 0;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  printf("Mount %i %i, %04x:%04x\n", dev_addr, instance, vid, pid);

  auto protocol = tuh_hid_interface_protocol(dev_addr, instance);

  if(protocol == HID_ITF_PROTOCOL_KEYBOARD && !hid_keyboard_detected) {
    printf("Got HID keyboard...\n");
  } else if (protocol == HID_ITF_PROTOCOL_MOUSE && !hid_mouse_detected) {
    printf("Got HID mouse...\n");
  } else {
    printf("Got protocol %i \n", protocol);
  }
  
  hid_keyboard_detected = protocol == HID_ITF_PROTOCOL_KEYBOARD;
  hid_mouse_detected = protocol == HID_ITF_PROTOCOL_MOUSE;

  tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  hid_keyboard_detected = false;
}

// should this be here or in input.cpp?
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {

  auto report_data = hid_report_id == -1 ? report : report + 1;

  //printf("Report %i %i, %i\n", dev_addr, instance, hid_report_id);

  auto protocol = tuh_hid_interface_protocol(dev_addr, instance);


  if(protocol == HID_ITF_PROTOCOL_KEYBOARD) {
    hid_keyboard_detected = true;
    auto keyboard_report = (hid_keyboard_report_t const*) report;
    memcpy(hid_keys, keyboard_report->keycode, 6);
    printf("Keyboard %i %i %i %i %i %i 0b%s%s\n",
            hid_keys[0],
            hid_keys[1],
            hid_keys[2],
            hid_keys[3],
            hid_keys[4],
            hid_keys[5],
            nibble_to_bitstring[keyboard_report->modifier >> 4],
            nibble_to_bitstring[keyboard_report->modifier & 0x0F]);
    keyboard_callback(hid_keys, keyboard_report->modifier);
  }
  if(protocol == HID_ITF_PROTOCOL_MOUSE) {
    hid_mouse_detected = true;
    auto mouse_report = (hid_mouse_report_t const*) report;
    printf("Mouse %i %i %i 0b%s%s\n", mouse_report->x, mouse_report->y,
            nibble_to_bitstring[mouse_report->buttons >> 4],
            nibble_to_bitstring[mouse_report->buttons & 0x0F]);
    mouse_callback(mouse_report->x, mouse_report->y, mouse_report->buttons, mouse_report->wheel);
  }

  tuh_hid_receive_report(dev_addr, instance);
}

// cdc
static uint8_t cdc_index = 0; // TODO: multiple devices?

void tuh_cdc_mount_cb(uint8_t idx) {
  cdc_index = idx;
}

void init_usb() {
  board_init();
  tusb_init();
  tuh_init(BOARD_TUH_RHPORT);
}

void update_usb() {
  tuh_task();
}

void usb_debug(const char *message) {

}

bool usb_cdc_connected() {
  //return tuh_cdc_mounted(cdc_index);
  return false;
}

uint16_t usb_cdc_read(uint8_t *data, uint16_t len) {
  //return tuh_cdc_read(cdc_index, data, len);
  return 0u;
}

uint32_t usb_cdc_read_available() {
  //return tuh_cdc_read_available(cdc_index);
  return 0u;
}

void usb_cdc_write(const uint8_t *data, uint16_t len) {
 /* uint32_t done = tuh_cdc_write(cdc_index,data, len);

  while(done < len) {
    tuh_task();
    if(!tuh_cdc_mounted(cdc_index))
      break;

    done += tuh_cdc_write(cdc_index, data + done, len - done);
  }*/
}

void usb_cdc_flush_write() {
  //tuh_cdc_write_flush(cdc_index);
}
