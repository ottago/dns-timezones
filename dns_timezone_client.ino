/*
 * DNS Timezone Client for ESP8266/ESP32
 * Fetches timezone data via DNS TXT records and applies local time
 */

#ifdef ESP32
  #include <WiFi.h>
  #include <WiFiUdp.h>
#else
  #include <ESP8266WiFi.h>
  #include <WiFiUdp.h>
#endif

#include <time.h>

// Configuration
const char* WIFI_SSID = "your-wifi-ssid";
const char* WIFI_PASSWORD = "your-wifi-password";
const char* TIMEZONE_DOMAIN = "australia.sydney.tz.ottago.com";

struct TimezoneInfo {
  int16_t std_offset;     // Standard offset in minutes
  int16_t cur_offset;     // Current offset in minutes  
  bool is_dst;            // Currently in DST
  uint32_t next_change;   // Unix timestamp of next change
  int16_t next_offset;    // Offset after next change
};

// Convert +HHMM or -HHMM to minutes
int16_t parseOffset(const char* str) {
  if (!str || strlen(str) < 5) return 0;
  
  int sign = (str[0] == '+') ? 1 : -1;
  int hours = (str[1] - '0') * 10 + (str[2] - '0');
  int minutes = (str[3] - '0') * 10 + (str[4] - '0');
  
  return sign * (hours * 60 + minutes);
}

// Parse ISO date to Unix timestamp (simplified)
uint32_t parseDateTime(const char* str) {
  if (!str || strlen(str) < 19) return 0;
  
  int year = atoi(str);
  int month = atoi(str + 5);
  int day = atoi(str + 8);
  int hour = atoi(str + 11);
  int minute = atoi(str + 14);
  
  // Approximate epoch calculation
  uint32_t days = (year - 1970) * 365 + (year - 1969) / 4;
  int monthDays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  if (month > 0 && month <= 12) {
    days += monthDays[month - 1];
    if (month > 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
      days++;
    }
  }
  days += day - 1;
  
  return days * 86400UL + hour * 3600UL + minute * 60UL;
}

// Parse single TXT record line
bool parseTxtLine(const char* line, TimezoneInfo& tz, uint32_t currentTime) {
  if (!line) return false;
  
  if (strncmp(line, "std=", 4) == 0) {
    tz.std_offset = parseOffset(line + 4);
    return true;
  }
  
  if (strncmp(line, "cur=", 4) == 0) {
    tz.cur_offset = parseOffset(line + 4);
    char* dst_pos = strchr(line, ',');
    if (dst_pos && strncmp(dst_pos + 1, "dst=", 4) == 0) {
      tz.is_dst = (dst_pos[5] == '1');
    }
    return true;
  }
  
  if (strlen(line) > 20 && line[4] == '-' && line[7] == '-') {
    char* arrow_pos = strchr(line, '>');
    if (arrow_pos) {
      uint32_t changeTime = parseDateTime(line);
      if (changeTime > currentTime && (tz.next_change == 0 || changeTime < tz.next_change)) {
        tz.next_change = changeTime;
        tz.next_offset = parseOffset(arrow_pos + 1);
      }
    }
    return true;
  }
  
  return false;
}

// DNS TXT lookup using UDP
bool lookupTxtRecords(const char* domain, TimezoneInfo& tz) {
  WiFiUDP udp;
  IPAddress dnsServer(8, 8, 8, 8); // Google DNS
  
  if (!udp.begin(12345)) {
    Serial.println("UDP begin failed");
    return false;
  }
  
  // Build DNS query packet
  uint8_t packet[512];
  int pos = 0;
  
  // DNS Header
  packet[pos++] = 0x12; packet[pos++] = 0x34; // Transaction ID
  packet[pos++] = 0x01; packet[pos++] = 0x00; // Flags (standard query)
  packet[pos++] = 0x00; packet[pos++] = 0x01; // Questions: 1
  packet[pos++] = 0x00; packet[pos++] = 0x00; // Answer RRs: 0
  packet[pos++] = 0x00; packet[pos++] = 0x00; // Authority RRs: 0
  packet[pos++] = 0x00; packet[pos++] = 0x00; // Additional RRs: 0
  
  // Question section - encode domain name
  const char* label = domain;
  while (*label) {
    const char* dot = strchr(label, '.');
    int len = dot ? (dot - label) : strlen(label);
    packet[pos++] = len;
    memcpy(packet + pos, label, len);
    pos += len;
    label = dot ? dot + 1 : label + strlen(label);
  }
  packet[pos++] = 0x00; // End of name
  
  packet[pos++] = 0x00; packet[pos++] = 0x10; // Type: TXT (16)
  packet[pos++] = 0x00; packet[pos++] = 0x01; // Class: IN (1)
  
  // Send query
  udp.beginPacket(dnsServer, 53);
  udp.write(packet, pos);
  if (!udp.endPacket()) {
    Serial.println("DNS query send failed");
    udp.stop();
    return false;
  }
  
  // Wait for response
  unsigned long timeout = millis() + 5000;
  while (millis() < timeout) {
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
      uint8_t response[512];
      int len = udp.read(response, sizeof(response));
      udp.stop();
      
      // Parse DNS response (simplified)
      if (len < 12) return false;
      
      uint16_t answers = (response[6] << 8) | response[7];
      if (answers == 0) {
        Serial.println("No TXT records found");
        return false;
      }
      
      // Skip to answer section (simplified parsing)
      int offset = 12;
      
      // Skip question section
      while (offset < len && response[offset] != 0) {
        if (response[offset] & 0xC0) {
          offset += 2; // Compressed name
          break;
        }
        offset += response[offset] + 1;
      }
      if (response[offset] == 0) offset++;
      offset += 4; // Skip QTYPE and QCLASS
      
      // Parse answer records
      memset(&tz, 0, sizeof(tz));
      uint32_t currentTime = time(nullptr);
      
      for (int i = 0; i < answers && offset < len; i++) {
        // Skip name (assume compression)
        if (response[offset] & 0xC0) offset += 2;
        else {
          while (offset < len && response[offset] != 0) {
            offset += response[offset] + 1;
          }
          offset++;
        }
        
        if (offset + 10 > len) break;
        
        uint16_t type = (response[offset] << 8) | response[offset + 1];
        uint16_t rdlength = (response[offset + 8] << 8) | response[offset + 9];
        offset += 10;
        
        if (type == 16 && offset + rdlength <= len) { // TXT record
          // Extract TXT data (skip length byte)
          if (rdlength > 1) {
            char txtData[256];
            int txtLen = min(rdlength - 1, 255);
            memcpy(txtData, response + offset + 1, txtLen);
            txtData[txtLen] = 0;
            
            Serial.printf("TXT: %s\n", txtData);
            parseTxtLine(txtData, tz, currentTime);
          }
        }
        
        offset += rdlength;
      }
      
      return (tz.std_offset != 0 || tz.cur_offset != 0);
    }
    delay(10);
  }
  
  Serial.println("DNS query timeout");
  udp.stop();
  return false;
}

// Get current offset considering transitions
int16_t getCurrentOffset(const TimezoneInfo& tz, uint32_t currentTime) {
  if (tz.next_change > 0 && currentTime >= tz.next_change) {
    return tz.next_offset;
  }
  return tz.cur_offset;
}

TimezoneInfo myTimezone;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("DNS Timezone Client Starting...");
  
  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  
  // Initialize NTP for UTC time
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP time sync");
  
  while (time(nullptr) < 1000000000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  
  // Fetch timezone data
  Serial.printf("Looking up timezone data for: %s\n", TIMEZONE_DOMAIN);
  
  if (lookupTxtRecords(TIMEZONE_DOMAIN, myTimezone)) {
    Serial.println("✓ Timezone data loaded successfully");
    Serial.printf("Standard offset: %+d minutes\n", myTimezone.std_offset);
    Serial.printf("Current offset: %+d minutes\n", myTimezone.cur_offset);
    Serial.printf("DST active: %s\n", myTimezone.is_dst ? "Yes" : "No");
    
    if (myTimezone.next_change > 0) {
      Serial.printf("Next change: %u -> %+d minutes\n", 
                   myTimezone.next_change, myTimezone.next_offset);
    }
  } else {
    Serial.println("✗ Failed to load timezone data");
    // Fallback to hardcoded offset
    myTimezone.cur_offset = 11 * 60; // +11:00 for Sydney summer
  }
}

void loop() {
  time_t utcTime = time(nullptr);
  int16_t offset = getCurrentOffset(myTimezone, utcTime);
  time_t localTime = utcTime + (offset * 60);
  
  Serial.printf("UTC: %s", ctime(&utcTime));
  Serial.printf("Local (%+d min): %s", offset, ctime(&localTime));
  Serial.println("---");
  
  delay(30000); // Update every 30 seconds
}
