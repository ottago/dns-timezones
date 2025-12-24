# DNS Timezones

Provides timezone information via DNS TXT records for IoT devices with minimal memory footprint.

## System Overview

**Server Side:**
- `generate_tz_dns.py` - Generates DNS TXT records for all IANA timezones
- Records contain current offset, DST status, and next year of transitions

**Client Side:**
- `dns_timezone_client.ino` - ESP8266/ESP32 sketch for DNS lookup and parsing
- Minimal memory usage (~50 bytes RAM)
- Compatible with both ESP8266 and ESP32

## DNS Record Format

**Individual timezone records:**
```
australia.sydney.tz.ottago.com TXT "std=+1000"
australia.sydney.tz.ottago.com TXT "cur=+1100,dst=1"  
australia.sydney.tz.ottago.com TXT "2025-04-06T02:00:00>+1000"
australia.sydney.tz.ottago.com TXT "2025-10-05T02:00:00>+1100"
australia.sydney.tz.ottago.com TXT "valid=2025-12-31"
```

**Discovery records:**
```
list.tz.ottago.com TXT "count=553"
australia.list.tz.ottago.com TXT "count=23"
australia.list.tz.ottago.com TXT "Australia/Sydney|australia.sydney"
australia.list.tz.ottago.com TXT "Australia/Melbourne|australia.melbourne"
```

**Discovery format:** `Display Name|dns_name`
- Display Name: Human-readable (e.g., "Australia/Sydney")
- DNS Name: Domain prefix (e.g., "australia.sydney")

## Usage

1. **Generate DNS records:**
   ```bash
   python3 generate_tz_dns.py > timezone_records.txt
   ```

2. **Upload to DNS provider** (Route53, Cloudflare, etc.)

3. **Configure ESP device:**
   - Update WiFi credentials in `dns_timezone_client.ino`
   - Set desired timezone domain
   - Upload to ESP8266/ESP32

4. **Device automatically:**
   - Connects to WiFi
   - Syncs UTC time via NTP
   - Fetches timezone data via DNS
   - Applies local time offset

## Memory Efficiency

- **TimezoneInfo struct:** 12 bytes
- **Parser functions:** ~2KB flash
- **Total RAM impact:** <50 bytes
- **No timezone database** needed on device

Perfect for IoT devices that need accurate local time without storing full timezone databases.
