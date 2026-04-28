#include "NfcInterface.h"
#include "configs.h"
#include <Wire.h>
#include "driver/gpio.h"

#ifdef HAS_NFC

NfcInterface nfc_obj;

NfcInterface::NfcInterface() {
    this->device_addr = 0x55;
}

void NfcInterface::recover_bus(int sda, int scl) {
    gpio_reset_pin((gpio_num_t)sda);
    gpio_reset_pin((gpio_num_t)scl);
    
    pinMode(sda, INPUT_PULLUP);
    pinMode(scl, OUTPUT);
    digitalWrite(scl, HIGH);
    delay(1);

    for (int i = 0; i < 10; i++) {
        digitalWrite(scl, LOW);
        delayMicroseconds(10);
        digitalWrite(scl, HIGH);
        delayMicroseconds(10);
        if (digitalRead(sda) == HIGH && i > 0) break;
    }

    pinMode(sda, OUTPUT);
    digitalWrite(sda, LOW);
    delayMicroseconds(10);
    digitalWrite(scl, HIGH);
    delayMicroseconds(10);
    digitalWrite(sda, HIGH);
    delayMicroseconds(10);
    
    pinMode(sda, INPUT);
    pinMode(scl, INPUT);
    delay(10);
}



void NfcInterface::begin() {
#if defined(NFC_SDA) && defined(NFC_SCL)
    Wire.end(); 
    delay(50);
    
    // 1. Physical cleanup
    this->recover_bus(NFC_SDA, NFC_SCL);
    delay(100); 
    
    // 2. Clear Any leftovers in the I2C peripheral
    // Using 40kHz for maximum signal integrity on C5 Marauder
    if (!Wire.begin(NFC_SDA, NFC_SCL, 40000)) {
        Serial.println(F("[NFC] Wire.begin failed!"));
    }
    Wire.setTimeOut(200); // More generous timeout
    
#ifdef NFC_FD
    pinMode(NFC_FD, INPUT_PULLUP);
    Serial.printf("[NFC] Field Detect (FD) initialized on GPIO %d\n", NFC_FD);
#endif
#else
    Wire.begin();
    Wire.setClock(40000); 
#endif
    delay(100);
    device_addr = find_chip();
}

uint8_t NfcInterface::find_chip() {
    uint8_t common_addresses[] = {0x55, 0x02, 0x2A};
    for (uint8_t addr : common_addresses) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            return addr;
        }
    }
    return 0x55;
}

void NfcInterface::deep_scan() {
    Serial.println(F("\r\n[NFC-DIAG] Starting Full Hardware Discovery Scan..."));
    
    struct PinPair { int sda; int scl; const char* name; };
    PinPair pairs[] = {
        {7, 6, "Pines 7/6 (Anterior)"},
        {2, 3, "Pines 2/3 (Diagrama)"},
        {0, 1, "Pines 0/1"},
        {4, 5, "Pines 4/5"},
        {10, 11, "Pines 10/11"}
    };

    bool overall_found = false;

    for (auto pair : pairs) {
        Serial.printf("[NFC-DIAG] Probando %s (SDA:%d, SCL:%d)...\r\n", pair.name, pair.sda, pair.scl);
        
        Wire.end();
        delay(100);
        
        this->recover_bus(pair.sda, pair.scl);
        
        // Setup pins with strong pull-up
        pinMode(pair.sda, INPUT_PULLUP);
        pinMode(pair.scl, INPUT_PULLUP);
        delay(50);

        if (Wire.begin(pair.sda, pair.scl)) {
            Wire.setClock(10000); // Extreme stability
            int devices_on_this_pair = 0;
            
            for (uint8_t addr = 1; addr < 127; addr++) {
                Wire.beginTransmission(addr);
                if (Wire.endTransmission() == 0) {
                    Serial.printf("  >> ¡DISPOSITIVO ENCONTRADO en 0x%02X!\r\n", addr);
                    devices_on_this_pair++;
                    overall_found = true;
                    if (addr == 0x55 || addr == 0x2A) {
                        Serial.println(F("  [!] Este parece ser el chip NFC (NT3H2111)."));
                        device_addr = addr;
                    }
                }
                delay(2);
            }
            if (devices_on_this_pair == 0) Serial.println(F("  No se encontró nada en este par."));
        } else {
            Serial.println(F("  Fallo al iniciar Wire en estos pines."));
        }
    }

    if (!overall_found) {
        Serial.println(F("\r\n[NFC-DIAG] ERROR: No se detectó ningún dispositivo I2C en ningún pin."));
        Serial.println(F("[NFC-DIAG] REVISA: 1. Alimentación (3.3V/GND), 2. Continuidad, 3. Soldaduras."));
    } else {
        Serial.println(F("\r\n[NFC-DIAG] Escaneo finalizado. Restaurando pines por defecto..."));
    }
    
    // CRITICAL: Restore original pins defined in configs.h
    this->begin(); 
}

int NfcInterface::read_i2c_block(uint8_t block_addr, uint8_t *data) {
    if (device_addr == 0) return 0;
    
    for (int retry = 0; retry < 3; retry++) {
#if defined(NFC_SDA) && defined(NFC_SCL)
        Wire.beginTransmission(device_addr);
        Wire.write(block_addr);
        if (Wire.endTransmission(false) != 0) {
            delay(10);
            continue; // Retry
        }
        
        uint8_t bytes_read = Wire.requestFrom(device_addr, (uint8_t)16);
        if (bytes_read == 16) {
            for (int i = 0; i < 16; i++) {
                data[i] = Wire.read();
            }
            return 1;
        }
#endif
        delay(10);
    }
    return 0;
}

void NfcInterface::read_tag_content() {
    Serial.println("Reading NFC Tag Content (Blocks 1-8):");
    Serial.println("Block | Hexadecimal Data                | ASCII");
    Serial.println("-----------------------------------------------------");
    
    uint8_t data[16];
    for (int b = 1; b <= 8; b++) {
        if (read_i2c_block(b, data)) {
            Serial.printf("%02d    | ", b);
            
            // Hex display
            for (int i = 0; i < 16; i++) {
                Serial.printf("%02X ", data[i]);
            }
            Serial.print("| ");
            
            // ASCII display
            for (int i = 0; i < 16; i++) {
                if (data[i] >= 32 && data[i] <= 126) {
                    Serial.print((char)data[i]);
                } else {
                    Serial.print(".");
                }
            }
            Serial.println();
        } else {
            Serial.printf("Error reading block %d\n", b);
        }
    }
}

int NfcInterface::write_i2c_block(uint8_t block_addr, uint8_t *data) {
    if (device_addr == 0) return 0;

    for (int retry = 0; retry < 5; retry++) {
#if defined(NFC_SDA) && defined(NFC_SCL)
        Wire.beginTransmission(device_addr);
        Wire.write(block_addr);
        for (int i = 0; i < 16; i++) {
            Wire.write(data[i]);
        }
        if (Wire.endTransmission() == 0) {
            delay(10); // EEPROM write time (typically 5ms)
            return 1;
        }
#endif
        delay(20); // Wait longer on write failure
    }
    return 0;
}

int NfcInterface::setup_capability_container() {
    uint8_t current_block[16];
    
    if (!read_i2c_block(0x00, current_block)) {
        Serial.println("ERROR: Could not read CC block");
        return 0;
    }
    
    // Preserve UID and configure CC
    current_block[12] = 0xE1;  // NDEF Magic
    current_block[13] = 0x10;  // Version 1.0 + R/W
    current_block[14] = 0x6D;  // NTAG I2C 1K
    current_block[15] = 0x00;  // No features
    
    return write_i2c_block(0x00, current_block);
}

int NfcInterface::write_ndef_data(uint8_t *ndef_data, int ndef_len) {
    uint8_t buffer[16];
    int blocks_needed = (ndef_len + 15) / 16;
    int data_pos = 0;
    
    for (int block = 0; block < blocks_needed; block++) {
        memset(buffer, 0x00, 16);
        
        // Copy up to 16 bytes
        for (int i = 0; i < 16 && data_pos < ndef_len; i++) {
            buffer[i] = ndef_data[data_pos++];
        }
        
        // Add terminator in the last block
        if (block == blocks_needed - 1) {
            for (int i = data_pos % 16; i < 16; i++) {
                if (i == data_pos % 16) {
                    buffer[i] = 0xFE; // Terminator TLV
                } else {
                    buffer[i] = 0x00;
                }
            }
        }
        
        if (!write_i2c_block(0x01 + block, buffer)) {
            Serial.printf("ERROR: Failed at block %d\r\n", block + 1);
            return 0;
        }
    }
    
    return 1;
}

int NfcInterface::write_ndef_uri(const char* uri) {
    this->begin(); // FORCE RE-INIT BUS
    if (device_addr == 0) return 0;
    
    if (!setup_capability_container()) return 0;
    
    // NDEF URI Prefix detection
    uint8_t prefix_id = 0x00; // No prefix (default)
    const char* uri_body = uri;

    if (strncmp(uri, "http://www.", 11) == 0) {
        prefix_id = 0x01;
        uri_body = uri + 11;
    } else if (strncmp(uri, "https://www.", 12) == 0) {
        prefix_id = 0x02;
        uri_body = uri + 12;
    } else if (strncmp(uri, "http://", 7) == 0) {
        prefix_id = 0x03;
        uri_body = uri + 7;
    } else if (strncmp(uri, "https://", 8) == 0) {
        prefix_id = 0x04;
        uri_body = uri + 8;
    } else if (strncmp(uri, "tel:", 4) == 0) {
        prefix_id = 0x05;
        uri_body = uri + 4;
    } else if (strncmp(uri, "mailto:", 7) == 0) {
        prefix_id = 0x06;
        uri_body = uri + 7;
    }

    int uri_len = strlen(uri_body);
    int payload_len = 1 + uri_len;    // Prefix byte + URI body
    int record_len = 3 + 1 + payload_len; // Header(1) + TypeLen(1) + PayloadLen(1) + Type(1) + Payload
    int total_len = 2 + record_len;   // TLV Type(1) + TLV Length(1) + Record
    
    uint8_t ndef_data[128];
    if (total_len > sizeof(ndef_data)) {
        Serial.println("ERROR: URI too long");
        return 0;
    }
    
    int pos = 0;
    ndef_data[pos++] = 0x03;        // NDEF TLV Type
    ndef_data[pos++] = record_len;  // NDEF Length
    ndef_data[pos++] = 0xD1;        // Header (MB=1, ME=1, SR=1, TNF=0x01)
    ndef_data[pos++] = 0x01;        // Type Length ('U')
    ndef_data[pos++] = payload_len; // Payload Length
    ndef_data[pos++] = 0x55;        // Type 'U' (URI)
    ndef_data[pos++] = prefix_id;   // Compressed Prefix
    
    for (int i = 0; i < uri_len; i++) {
        ndef_data[pos++] = uri_body[i];
    }
    return write_ndef_data(ndef_data, total_len);
}

int NfcInterface::write_ndef_text(const char* text, const char* lang_code) {
    this->begin(); // FORCE RE-INIT BUS
    if (device_addr == 0) return 0;
    
    if (!setup_capability_container()) return 0;
    
    int text_len = strlen(text);
    int lang_len = strlen(lang_code);
    int payload_len = 1 + lang_len + text_len;
    int record_len = 3 + 1 + payload_len;
    int total_len = 2 + record_len;
    
    uint8_t ndef_data[256];
    if (total_len > sizeof(ndef_data)) {
        Serial.println("ERROR: Text too long");
        return 0;
    }
    
    int pos = 0;
    ndef_data[pos++] = 0x03;        // NDEF TLV Type
    ndef_data[pos++] = record_len;  // NDEF Length
    ndef_data[pos++] = 0xD1;        // Header
    ndef_data[pos++] = 0x01;        // Type Length
    ndef_data[pos++] = payload_len; // Payload Length
    ndef_data[pos++] = 0x54;        // Type 'T'
    ndef_data[pos++] = lang_len;    // Status byte
    
    for (int i = 0; i < lang_len; i++) {
        ndef_data[pos++] = lang_code[i];
    }
    for (int i = 0; i < text_len; i++) {
        ndef_data[pos++] = text[i];
    }
    
    return write_ndef_data(ndef_data, total_len);
}

int NfcInterface::write_ndef_vcard(const char* name, const char* phone, const char* email) {
    this->begin(); // FORCE RE-INIT BUS
    if (device_addr == 0) {
        Serial.println("ERROR: NFC Chip not found");
        return 0;
    }
    
    if (!setup_capability_container()) return 0;
    
    char vcard[256];
    int vcard_len = snprintf(vcard, sizeof(vcard),
        "BEGIN:VCARD\r\n"
        "VERSION:3.0\r\n"
        "FN:%s\r\n"
        "TEL:%s\r\n"
        "EMAIL:%s\r\n"
        "END:VCARD\r\n",
        name, phone, email);
    
    if (vcard_len >= sizeof(vcard)) {
        Serial.println("ERROR: vCard too long");
        return 0;
    }
    
    int payload_len = vcard_len;
    int record_len = 3 + 10 + payload_len; // Header + "text/vcard" + vCard
    int total_len = 2 + record_len;
    
    uint8_t ndef_data[512];
    if (total_len > sizeof(ndef_data)) {
        Serial.println("ERROR: NDEF vCard too long");
        return 0;
    }
    
    int pos = 0;
    ndef_data[pos++] = 0x03;        // NDEF TLV Type
    ndef_data[pos++] = record_len;  // NDEF Length
    ndef_data[pos++] = 0xD2;        // Header MIME
    ndef_data[pos++] = 0x0A;        // Type Length (10 = "text/vcard")
    ndef_data[pos++] = payload_len; // Payload Length
    
    // MIME Type
    const char* mime = "text/vcard";
    for (int i = 0; i < 10; i++) {
        ndef_data[pos++] = mime[i];
    }
    
    // vCard data
    for (int i = 0; i < vcard_len; i++) {
        ndef_data[pos++] = vcard[i];
    }
    
    return write_ndef_data(ndef_data, total_len);
}

int NfcInterface::write_ndef_wifi(const char* ssid, const char* pass, const char* auth) {
    this->begin(); 
    if (device_addr == 0) return 0;
    if (!setup_capability_container()) return 0;

    // Simple implementation using WiFi:S:SSID;T:AUTH;P:PASS;; format
    // This is widely supported by Android and iOS
    char wifi_str[256];
    int wifi_len = snprintf(wifi_str, sizeof(wifi_str),
        "WIFI:S:%s;T:%s;P:%s;;",
        ssid, auth, pass);

    int payload_len = wifi_len;
    int record_len = 3 + 1 + payload_len; // Header + TypeLen + PayloadLen + Type('U') + Payload
    int total_len = 2 + record_len;

    uint8_t ndef_data[256];
    int pos = 0;
    ndef_data[pos++] = 0x03;        // TLV Type
    ndef_data[pos++] = record_len;  // TLV Length
    ndef_data[pos++] = 0xD1;        // Well-known record
    ndef_data[pos++] = 0x01;        // Type length
    ndef_data[pos++] = payload_len; // Payload length
    ndef_data[pos++] = 0x55;        // Type 'U' (URI)
    ndef_data[pos++] = 0x00;        // No prefix (manual string)

    for (int i = 0; i < wifi_len; i++) {
        ndef_data[pos++] = wifi_str[i];
    }

    return write_ndef_data(ndef_data, total_len);
}

#endif // HAS_NFC
