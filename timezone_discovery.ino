/*
 * Timezone Discovery Functions for ESP8266/ESP32
 * Discovers available timezones from DNS TXT records
 */

struct TimezoneEntry {
  char display_name[32];
  char dns_name[32];
};

// Parse timezone list entry: "Australia/Sydney|australia.sydney"
bool parseTimezoneEntry(const char* txt, TimezoneEntry& entry) {
  if (!txt) return false;
  
  char* pipe = strchr(txt, '|');
  if (!pipe) return false;
  
  int display_len = min(pipe - txt, 31);
  int dns_len = min(strlen(pipe + 1), 31);
  
  strncpy(entry.display_name, txt, display_len);
  entry.display_name[display_len] = 0;
  
  strncpy(entry.dns_name, pipe + 1, dns_len);
  entry.dns_name[dns_len] = 0;
  
  return true;
}

// Discover timezones for a region (e.g., "australia")
int discoverTimezones(const char* region, TimezoneEntry* entries, int maxEntries) {
  char domain[64];
  snprintf(domain, sizeof(domain), "%s.list.tz.ottago.com", region);
  
  Serial.printf("Discovering timezones for region: %s\n", region);
  
  // Simplified: In real implementation, do DNS TXT lookup for domain
  // Parse each TXT record that contains "|" as timezone entry
  
  // Example hardcoded data for testing
  if (strcmp(region, "australia") == 0) {
    const char* test_records[] = {
      "Australia/Sydney|australia.sydney",
      "Australia/Melbourne|australia.melbourne", 
      "Australia/Brisbane|australia.brisbane",
      "Australia/Perth|australia.perth",
      "Australia/Adelaide|australia.adelaide"
    };
    
    int count = min(5, maxEntries);
    for (int i = 0; i < count; i++) {
      if (!parseTimezoneEntry(test_records[i], entries[i])) {
        return i;
      }
    }
    return count;
  }
  
  return 0; // No entries found
}

// Get list of available regions
int discoverRegions(char regions[][16], int maxRegions) {
  // In real implementation: DNS lookup for "list.tz.ottago.com"
  // Parse response to find region subdomains
  
  // Hardcoded for testing
  const char* test_regions[] = {
    "africa", "america", "antarctica", "arctic", "asia", 
    "atlantic", "australia", "europe", "indian", "pacific"
  };
  
  int count = min(10, maxRegions);
  for (int i = 0; i < count; i++) {
    strncpy(regions[i], test_regions[i], 15);
    regions[i][15] = 0;
  }
  
  return count;
}

// Example usage in setup()
void setupTimezoneDiscovery() {
  Serial.println("=== Timezone Discovery ===");
  
  // Discover available regions
  char regions[10][16];
  int regionCount = discoverRegions(regions, 10);
  
  Serial.printf("Found %d regions:\n", regionCount);
  for (int i = 0; i < regionCount; i++) {
    Serial.printf("  %s\n", regions[i]);
  }
  
  // Discover Australia timezones
  TimezoneEntry timezones[10];
  int tzCount = discoverTimezones("australia", timezones, 10);
  
  Serial.printf("\nFound %d Australia timezones:\n", tzCount);
  for (int i = 0; i < tzCount; i++) {
    Serial.printf("  %s -> %s.tz.ottago.com\n", 
                 timezones[i].display_name, 
                 timezones[i].dns_name);
  }
  
  Serial.println("========================");
}
