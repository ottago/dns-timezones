/*
 * Minimal timezone DNS parser for ESP8266/ESP32
 * Parses TXT records in format:
 * std=+1000
 * cur=+1100,dst=1
 * 2026-04-06T00:00:00>+1000
 * valid=2026-12-24
 */

#include <Arduino.h>

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
  
  // Extract YYYY-MM-DDTHH:MM:SS
  int year = atoi(str);
  int month = atoi(str + 5);
  int day = atoi(str + 8);
  int hour = atoi(str + 11);
  int minute = atoi(str + 14);
  int second = atoi(str + 17);
  
  // Simple epoch calculation (approximate)
  // This is minimal - use proper time library for accuracy
  uint32_t days = (year - 1970) * 365 + (year - 1969) / 4;
  
  // Add days for months (approximate)
  int monthDays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  if (month > 0 && month <= 12) {
    days += monthDays[month - 1];
    if (month > 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
      days++; // Leap year
    }
  }
  
  days += day - 1;
  
  return days * 86400UL + hour * 3600UL + minute * 60UL + second;
}

// Parse single TXT record line
bool parseTxtLine(const char* line, TimezoneInfo& tz, uint32_t currentTime) {
  if (!line) return false;
  
  // std=+1000
  if (strncmp(line, "std=", 4) == 0) {
    tz.std_offset = parseOffset(line + 4);
    return true;
  }
  
  // cur=+1100,dst=1
  if (strncmp(line, "cur=", 4) == 0) {
    tz.cur_offset = parseOffset(line + 4);
    char* dst_pos = strchr(line, ',');
    if (dst_pos && strncmp(dst_pos + 1, "dst=", 4) == 0) {
      tz.is_dst = (dst_pos[5] == '1');
    }
    return true;
  }
  
  // 2026-04-06T00:00:00>+1000 (transition)
  if (strlen(line) > 20 && line[4] == '-' && line[7] == '-') {
    char* arrow_pos = strchr(line, '>');
    if (arrow_pos) {
      uint32_t changeTime = parseDateTime(line);
      
      // Only store the next upcoming change
      if (changeTime > currentTime && (tz.next_change == 0 || changeTime < tz.next_change)) {
        tz.next_change = changeTime;
        tz.next_offset = parseOffset(arrow_pos + 1);
      }
    }
    return true;
  }
  
  return false;
}

// Parse complete DNS TXT response
bool parseTimezoneData(const char* txtRecords[], int numRecords, TimezoneInfo& tz, uint32_t currentTime) {
  // Initialize
  memset(&tz, 0, sizeof(tz));
  
  // Parse each line
  for (int i = 0; i < numRecords; i++) {
    parseTxtLine(txtRecords[i], tz, currentTime);
  }
  
  return (tz.std_offset != 0 || tz.cur_offset != 0); // Basic validation
}

// Get current offset in minutes
int16_t getCurrentOffset(const TimezoneInfo& tz, uint32_t currentTime) {
  if (tz.next_change > 0 && currentTime >= tz.next_change) {
    return tz.next_offset;
  }
  return tz.cur_offset;
}

// Example usage
void setup() {
  Serial.begin(115200);
  
  // Simulate DNS TXT records
  const char* records[] = {
    "std=+1000",
    "cur=+1100,dst=1", 
    "2025-04-06T02:00:00>+1000",
    "2025-10-05T02:00:00>+1100",
    "valid=2025-12-31"
  };
  
  TimezoneInfo tz;
  uint32_t now = 1735016822; // Current Unix timestamp
  
  if (parseTimezoneData(records, 5, tz, now)) {
    Serial.printf("Standard offset: %d minutes\n", tz.std_offset);
    Serial.printf("Current offset: %d minutes\n", tz.cur_offset);
    Serial.printf("DST active: %s\n", tz.is_dst ? "yes" : "no");
    Serial.printf("Next change: %u -> %d minutes\n", tz.next_change, tz.next_offset);
    
    int16_t currentOffset = getCurrentOffset(tz, now);
    Serial.printf("Effective offset now: %d minutes\n", currentOffset);
  }
}

void loop() {
  // Your main code
}
