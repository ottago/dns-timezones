#!/usr/bin/env python3
"""
Generate DNS TXT records for timezone data.
Provides current offset and future transitions for IoT devices.
"""

import zoneinfo
from datetime import datetime, timedelta
from typing import List, Tuple

def get_timezone_transitions(tz_name: str, start_date: datetime, end_date: datetime) -> List[Tuple[datetime, int]]:
    """Get all timezone transitions within date range."""
    try:
        tz = zoneinfo.ZoneInfo(tz_name)
        transitions = []
        
        # Check every day for transitions (inefficient but simple)
        current = start_date
        prev_offset = None
        
        while current <= end_date:
            dt_local = current.replace(tzinfo=tz)
            offset_seconds = dt_local.utcoffset().total_seconds()
            
            if prev_offset is not None and offset_seconds != prev_offset:
                transitions.append((current, int(offset_seconds)))
            
            prev_offset = offset_seconds
            current += timedelta(days=1)
            
        return transitions
    except Exception:
        return []

def format_offset(seconds: int) -> str:
    """Format offset seconds as +HHMM or -HHMM."""
    hours, remainder = divmod(abs(seconds), 3600)
    minutes = remainder // 60
    sign = '+' if seconds >= 0 else '-'
    return f"{sign}{hours:02d}{minutes:02d}"

def generate_txt_record(tz_name: str) -> str:
    """Generate TXT record content for a timezone."""
    try:
        tz = zoneinfo.ZoneInfo(tz_name)
        now = datetime.now(tz)
        
        # Get standard time (usually winter time)
        jan_dt = datetime(now.year, 1, 15, tzinfo=tz)
        jul_dt = datetime(now.year, 7, 15, tzinfo=tz)
        std_offset = min(jan_dt.utcoffset().total_seconds(), jul_dt.utcoffset().total_seconds())
        
        # Current state
        current_offset = now.utcoffset().total_seconds()
        is_dst = current_offset != std_offset
        
        # Get transitions for next year
        start_date = now.replace(hour=0, minute=0, second=0, microsecond=0)
        end_date = start_date + timedelta(days=365)
        transitions = get_timezone_transitions(tz_name, start_date, end_date)
        
        # Build TXT record lines
        lines = [
            f"std={format_offset(int(std_offset))}",
            f"cur={format_offset(int(current_offset))},dst={'1' if is_dst else '0'}"
        ]
        
        # Add transitions
        for transition_dt, new_offset in transitions:
            iso_time = transition_dt.strftime('%Y-%m-%dT%H:%M:%S')
            lines.append(f"{iso_time}>{format_offset(new_offset)}")
        
        # Add validity
        valid_until = end_date.strftime('%Y-%m-%d')
        lines.append(f"valid={valid_until}")
        
        return lines
        
    except Exception as e:
        return [f"error={str(e)}"]

def generate_timezone_list(timezones, domain_suffix):
    """Generate discovery records for timezone list."""
    print("; Timezone Discovery Records")
    print(f"list.{domain_suffix}. IN TXT \"count={len(timezones)}\"")
    
    # Group by region for easier discovery
    regions = {}
    for tz_name in timezones:
        if '/' in tz_name:
            region = tz_name.split('/')[0]
            if region not in regions:
                regions[region] = []
            regions[region].append(tz_name)
    
    # Generate region lists
    for region, tz_list in sorted(regions.items()):
        region_domain = f"{region.lower()}.list.{domain_suffix}"
        print(f"; {region} timezones")
        print(f'{region_domain}. IN TXT "count={len(tz_list)}"')
        
        # Add timezone entries (display_name|dns_name format)
        for tz_name in sorted(tz_list):
            display_name = tz_name.replace('_', ' ')  # Make readable
            dns_name = tz_name.lower().replace('/', '.')
            print(f'{region_domain}. IN TXT "{display_name}|{dns_name}"')
        print()

def main():
    """Generate DNS records for all available timezones."""
    domain_suffix = "tz.ottago.com"
    
    print("# DNS TXT Records for Timezone Data")
    print(f"# Generated: {datetime.now().isoformat()}")
    print()
    
    # Get all available timezones
    all_timezones = sorted(zoneinfo.available_timezones())
    valid_timezones = [tz for tz in all_timezones if '/' in tz]
    
    # Generate discovery records first
    generate_timezone_list(valid_timezones, domain_suffix)
    
    # Generate individual timezone records
    for tz_name in valid_timezones:
        # Convert timezone name to DNS-safe format
        dns_name = tz_name.lower().replace('/', '.')
        full_domain = f"{dns_name}.{domain_suffix}"
        
        # Generate TXT record content
        txt_lines = generate_txt_record(tz_name)
        
        print(f"; {tz_name}")
        for i, line in enumerate(txt_lines):
            print(f'{full_domain}. IN TXT "{line}"')
        print()

if __name__ == "__main__":
    main()
