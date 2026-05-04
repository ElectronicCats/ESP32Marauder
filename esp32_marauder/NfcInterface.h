#pragma once
#ifndef NfcInterface_h
#define NfcInterface_h

#include "configs.h"

#ifdef HAS_NFC

#include <Arduino.h>
#include <Wire.h>

class NfcInterface {
  private:
    uint8_t device_addr;
    
    int read_i2c_block(uint8_t block_addr, uint8_t *data);
    int write_i2c_block(uint8_t block_addr, uint8_t *data);
    int setup_capability_container();
    int write_ndef_data(uint8_t *ndef_data, int ndef_len);
    void recover_bus(int sda, int scl);

  public:
    NfcInterface();
    
    char ndef_uri[200];
    char ndef_text[200];
    char ndef_vcard_name[64];
    char ndef_vcard_phone[64];
    char ndef_vcard_email[64];

    void begin();
    uint8_t find_chip();
    void deep_scan(); // Brute force pin discovery
    void read_tag_content();
    
    int write_ndef_uri(const char* uri);
    int write_ndef_text(const char* text, const char* lang_code = "en");
    int write_ndef_vcard(const char* name, const char* phone, const char* email);
    int write_ndef_wifi(const char* ssid, const char* pass, const char* auth = "WPA2");
};

extern NfcInterface nfc_obj;

#endif // HAS_NFC
#endif // NfcInterface_h
