#!/usr/bin/env python3
"""
BLE GATT HID Client Test Script for u-blox HID Keyboard

This script acts as a BLE central to test the NORA-W36 HID GATT server.
It connects, discovers services, and verifies that all HID service
characteristics are correctly configured according to the HID over GATT spec.

Requirements:
    pip install bleak

Usage:
    python test_ble_gatt_hid_client.py [device_name_or_address]
"""

import asyncio
import sys
from bleak import BleakClient, BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic

# Standard BLE HID Service UUIDs (Bluetooth SIG assigned)
HID_SERVICE_UUID = "00001812-0000-1000-8000-00805f9b34fb"              # HID Service
BATTERY_SERVICE_UUID = "0000180f-0000-1000-8000-00805f9b34fb"         # Battery Service
DEVICE_INFO_SERVICE_UUID = "0000180a-0000-1000-8000-00805f9b34fb"     # Device Information Service
GAP_SERVICE_UUID = "00001800-0000-1000-8000-00805f9b34fb"             # Generic Access
GATT_SERVICE_UUID = "00001801-0000-1000-8000-00805f9b34fb"            # Generic Attribute

# HID Service Characteristics (0x1812)
HID_INFO_CHAR_UUID = "00002a4a-0000-1000-8000-00805f9b34fb"           # HID Information
HID_REPORT_MAP_CHAR_UUID = "00002a4b-0000-1000-8000-00805f9b34fb"     # Report Map
HID_CONTROL_POINT_CHAR_UUID = "00002a4c-0000-1000-8000-00805f9b34fb"  # HID Control Point
HID_REPORT_CHAR_UUID = "00002a4d-0000-1000-8000-00805f9b34fb"         # Report (Input/Output/Feature)
HID_PROTOCOL_MODE_CHAR_UUID = "00002a4e-0000-1000-8000-00805f9b34fb"  # Protocol Mode
HID_BOOT_KBD_INPUT_CHAR_UUID = "00002a22-0000-1000-8000-00805f9b34fb" # Boot Keyboard Input Report
HID_BOOT_KBD_OUTPUT_CHAR_UUID = "00002a32-0000-1000-8000-00805f9b34fb"# Boot Keyboard Output Report

# Battery Service Characteristics (0x180F)
BATTERY_LEVEL_CHAR_UUID = "00002a19-0000-1000-8000-00805f9b34fb"      # Battery Level

# Descriptor UUIDs
CCCD_UUID = "00002902-0000-1000-8000-00805f9b34fb"                    # Client Characteristic Configuration
REPORT_REFERENCE_UUID = "00002908-0000-1000-8000-00805f9b34fb"        # Report Reference
EXTERNAL_REPORT_REF_UUID = "00002907-0000-1000-8000-00805f9b34fb"     # External Report Reference

# Default device name pattern (without random suffix)
DEFAULT_DEVICE_NAME = "u-blox HID Keyboard"

# Set to True to only connect to u-blox devices, False to connect to any HID device
UBLOX_ONLY = True

# Expected characteristics for HID Keyboard per HID over GATT spec
EXPECTED_HID_CHARACTERISTICS = {
    HID_INFO_CHAR_UUID: {
        "name": "HID Information",
        "required_props": ["read"],
        "expected_len": 4,  # bcdHID (2) + bCountryCode (1) + Flags (1)
    },
    HID_REPORT_MAP_CHAR_UUID: {
        "name": "Report Map",
        "required_props": ["read"],
        "min_len": 10,  # Report Map should be substantial
    },
    HID_CONTROL_POINT_CHAR_UUID: {
        "name": "HID Control Point",
        "required_props": ["write-without-response"],
    },
    HID_PROTOCOL_MODE_CHAR_UUID: {
        "name": "Protocol Mode",
        "required_props": ["read"],  # Write Without Response also typical
        "expected_len": 1,
    },
    HID_REPORT_CHAR_UUID: {
        "name": "Report",
        "required_props": [],  # Multiple instances with different properties
        "multiple": True,  # Can have multiple (Input, Output, Feature)
    },
    HID_BOOT_KBD_INPUT_CHAR_UUID: {
        "name": "Boot Keyboard Input Report",
        "required_props": ["notify"],  # or indicate
        "expected_len": 8,
    },
    HID_BOOT_KBD_OUTPUT_CHAR_UUID: {
        "name": "Boot Keyboard Output Report",
        "required_props": ["write-without-response"],
        "expected_len": 1,
    },
}


class HIDValidationResult:
    """Stores validation results for HID service."""
    def __init__(self):
        self.errors = []
        self.warnings = []
        self.info = []
        
    def add_error(self, msg):
        self.errors.append(f"❌ ERROR: {msg}")
        
    def add_warning(self, msg):
        self.warnings.append(f"⚠️  WARNING: {msg}")
        
    def add_info(self, msg):
        self.info.append(f"✓ {msg}")
        
    def print_summary(self):
        print(f"\n{'='*60}")
        print("VALIDATION SUMMARY")
        print(f"{'='*60}")
        
        if self.info:
            print("\n✅ PASSED:")
            for msg in self.info:
                print(f"  {msg}")
                
        if self.warnings:
            print("\n⚠️  WARNINGS:")
            for msg in self.warnings:
                print(f"  {msg}")
                
        if self.errors:
            print("\n❌ ERRORS:")
            for msg in self.errors:
                print(f"  {msg}")
                
        print(f"\n{'='*60}")
        if self.errors:
            print(f"RESULT: FAILED ({len(self.errors)} errors, {len(self.warnings)} warnings)")
        elif self.warnings:
            print(f"RESULT: PASSED WITH WARNINGS ({len(self.warnings)} warnings)")
        else:
            print("RESULT: PASSED ✅")
        print(f"{'='*60}")


def notification_handler(characteristic: BleakGATTCharacteristic, data: bytearray):
    """Handle notifications from HID characteristics."""
    char_name = get_char_name(characteristic.uuid)
    print(f"\n[NOTIFICATION] {char_name}:")
    print(f"  UUID: {characteristic.uuid}")
    print(f"  Data ({len(data)} bytes): {data.hex()}")
    
    # Parse HID keyboard report if it's a keyboard input
    if len(data) == 8:
        modifier = data[0]
        keys = data[2:8]
        print(f"  Parsed as keyboard report:")
        print(f"    Modifier: 0x{modifier:02X}", end="")
        if modifier:
            mods = []
            if modifier & 0x01: mods.append("L-Ctrl")
            if modifier & 0x02: mods.append("L-Shift")
            if modifier & 0x04: mods.append("L-Alt")
            if modifier & 0x08: mods.append("L-GUI")
            if modifier & 0x10: mods.append("R-Ctrl")
            if modifier & 0x20: mods.append("R-Shift")
            if modifier & 0x40: mods.append("R-Alt")
            if modifier & 0x80: mods.append("R-GUI")
            print(f" ({', '.join(mods)})")
        else:
            print(" (none)")
        active_keys = [k for k in keys if k != 0]
        print(f"    Keys: {[f'0x{k:02X}' for k in active_keys] if active_keys else '(none)'}")


def get_char_name(uuid: str) -> str:
    """Get human-readable name for a characteristic UUID."""
    uuid_lower = uuid.lower()
    names = {
        HID_INFO_CHAR_UUID: "HID Information (0x2A4A)",
        HID_REPORT_MAP_CHAR_UUID: "Report Map (0x2A4B)",
        HID_CONTROL_POINT_CHAR_UUID: "HID Control Point (0x2A4C)",
        HID_REPORT_CHAR_UUID: "Report (0x2A4D)",
        HID_PROTOCOL_MODE_CHAR_UUID: "Protocol Mode (0x2A4E)",
        HID_BOOT_KBD_INPUT_CHAR_UUID: "Boot Keyboard Input (0x2A22)",
        HID_BOOT_KBD_OUTPUT_CHAR_UUID: "Boot Keyboard Output (0x2A32)",
        BATTERY_LEVEL_CHAR_UUID: "Battery Level (0x2A19)",
        "00002a00-0000-1000-8000-00805f9b34fb": "Device Name (0x2A00)",
        "00002a01-0000-1000-8000-00805f9b34fb": "Appearance (0x2A01)",
        "00002a04-0000-1000-8000-00805f9b34fb": "Peripheral Preferred Conn Params (0x2A04)",
        "00002a05-0000-1000-8000-00805f9b34fb": "Service Changed (0x2A05)",
        "00002a29-0000-1000-8000-00805f9b34fb": "Manufacturer Name (0x2A29)",
        "00002a24-0000-1000-8000-00805f9b34fb": "Model Number (0x2A24)",
        "00002a26-0000-1000-8000-00805f9b34fb": "Firmware Revision (0x2A26)",
        "00002a28-0000-1000-8000-00805f9b34fb": "Software Revision (0x2A28)",
        "00002a50-0000-1000-8000-00805f9b34fb": "PnP ID (0x2A50)",
    }
    return names.get(uuid_lower, f"Unknown ({uuid[:8]}...)")


def get_service_name(uuid: str) -> str:
    """Get human-readable name for a service UUID."""
    uuid_lower = uuid.lower()
    names = {
        HID_SERVICE_UUID: "HID Service (0x1812)",
        BATTERY_SERVICE_UUID: "Battery Service (0x180F)",
        DEVICE_INFO_SERVICE_UUID: "Device Information (0x180A)",
        GAP_SERVICE_UUID: "Generic Access (0x1800)",
        GATT_SERVICE_UUID: "Generic Attribute (0x1801)",
    }
    return names.get(uuid_lower, uuid)


def parse_hid_info(data: bytes) -> dict:
    """Parse HID Information characteristic value."""
    if len(data) < 4:
        return {"error": f"Too short ({len(data)} bytes, expected 4)"}
    
    bcd_hid = (data[1] << 8) | data[0]  # Little-endian
    country_code = data[2]
    flags = data[3]
    
    return {
        "bcdHID": f"{(bcd_hid >> 8) & 0xFF}.{bcd_hid & 0xFF:02d}",
        "bCountryCode": country_code,
        "flags": flags,
        "normally_connectable": bool(flags & 0x02),
        "remote_wake": bool(flags & 0x01),
    }


def parse_protocol_mode(data: bytes) -> str:
    """Parse Protocol Mode characteristic value."""
    if len(data) < 1:
        return "Empty"
    mode = data[0]
    if mode == 0:
        return "Boot Protocol (0x00)"
    elif mode == 1:
        return "Report Protocol (0x01)"
    else:
        return f"Unknown (0x{mode:02X})"


async def scan_for_device(name_filter: str = None, timeout: float = 10.0):
    """Scan for BLE devices and find HID keyboard device."""
    print(f"\nScanning for BLE devices (timeout: {timeout}s)...")
    print(f"Filter mode: {'u-blox only' if UBLOX_ONLY else 'any HID device'}")
    
    devices = await BleakScanner.discover(timeout=timeout, return_adv=True)
    
    print(f"\nFound {len(devices)} devices:")
    hid_devices = []
    
    for address, (device, adv_data) in devices.items():
        # Check for HID service in advertisement
        is_hid = False
        service_uuids = adv_data.service_uuids or []
        for svc_uuid in service_uuids:
            if "1812" in svc_uuid.lower():  # HID Service
                is_hid = True
                break
        
        # Check if it's a u-blox device
        is_ublox = device.name and "u-blox" in device.name
        
        # Check name filter match
        name_match = False
        if name_filter and device.name:
            name_match = name_filter.lower() in device.name.lower()
        
        # Decide whether to include this device
        include_device = False
        marker = ""
        
        if UBLOX_ONLY:
            # Only include u-blox devices
            if is_ublox:
                marker = " [u-blox]"
                if is_hid:
                    marker += " [HID]"
                include_device = True
            elif is_hid:
                marker = " [HID] (not u-blox, skipped)"
            elif name_match:
                marker = " [NAME MATCH] (not u-blox, skipped)"
        else:
            # Include any HID device or name match
            if is_hid:
                marker = " [HID]"
                if is_ublox:
                    marker += " [u-blox]"
                include_device = True
            elif name_match:
                marker = " [NAME MATCH]"
                include_device = True
        
        # Only print devices with names
        if device.name:
            print(f"  {device.address}: {device.name}{marker}")
            
            # Print service UUIDs if available
            if service_uuids:
                for svc_uuid in service_uuids:
                    svc_name = get_service_name(svc_uuid)
                    print(f"    Service: {svc_name}")
            
            # Print manufacturer data if available
            if adv_data.manufacturer_data:
                for mfr_id, data in adv_data.manufacturer_data.items():
                    print(f"    Manufacturer Data (0x{mfr_id:04X}): {data.hex()}")
        
        if include_device:
            hid_devices.append(device)
    
    return hid_devices


async def read_descriptor_value(client: BleakClient, desc_handle: int) -> bytes:
    """Read a descriptor value by handle."""
    try:
        return await client.read_gatt_descriptor(desc_handle)
    except Exception as e:
        print(f"    Error reading descriptor {desc_handle}: {e}")
        return None


async def test_hid_client(address: str):
    """Connect to HID device and validate GATT services."""
    print(f"\n{'='*60}")
    print(f"Connecting to {address}...")
    print(f"{'='*60}")
    
    result = HIDValidationResult()
    
    # Force fresh service discovery by disabling caching
    async with BleakClient(address, winrt={"use_cached_services": False}) as client:
        print(f"\nConnected: {client.is_connected}")
        
        # Small delay before pairing to let connection stabilize
        await asyncio.sleep(0.5)
        
        # Try to pair if needed
        print("Attempting to pair...")
        try:
            paired = await client.pair(protection_level=1)  # 1 = None (Just Works)
            print(f"Pairing result: {paired}")
        except Exception as e:
            print(f"Pairing note: {e} (may already be paired)")
        
        # Wait a bit for services to be ready after pairing
        print("Waiting for GATT services to stabilize...")
        await asyncio.sleep(1.0)
        
        print(f"MTU: {client.mtu_size}")
        
        # Discover services
        print(f"\n{'='*60}")
        print("GATT Services Discovery")
        print(f"{'='*60}")
        
        hid_service = None
        battery_service = None
        
        # Track characteristics found
        hid_chars_found = {}
        report_chars = []  # Multiple Report characteristics
        
        for service in client.services:
            svc_name = get_service_name(service.uuid)
            print(f"\nService: {svc_name}")
            print(f"  UUID: {service.uuid}")
            print(f"  Handle: {service.handle}")
            
            if service.uuid.lower() == HID_SERVICE_UUID:
                hid_service = service
                print("  *** THIS IS THE HID SERVICE (0x1812) ***")
                
            if service.uuid.lower() == BATTERY_SERVICE_UUID:
                battery_service = service
                print("  *** THIS IS THE BATTERY SERVICE (0x180F) ***")
            
            for char in service.characteristics:
                props = ", ".join(char.properties)
                char_name = get_char_name(char.uuid)
                print(f"\n  Characteristic: {char_name}")
                print(f"    UUID: {char.uuid}")
                print(f"    Handle: {char.handle}")
                print(f"    Properties: [{props}]")
                
                # Track HID characteristics
                if service.uuid.lower() == HID_SERVICE_UUID:
                    char_uuid_lower = char.uuid.lower()
                    if char_uuid_lower == HID_REPORT_CHAR_UUID:
                        report_chars.append(char)
                    else:
                        hid_chars_found[char_uuid_lower] = char
                
                # Print descriptors
                for descriptor in char.descriptors:
                    desc_name = "CCCD" if "2902" in descriptor.uuid else \
                               "Report Reference" if "2908" in descriptor.uuid else \
                               "External Report Ref" if "2907" in descriptor.uuid else \
                               descriptor.uuid[:8]
                    print(f"    Descriptor: {desc_name} (handle: {descriptor.handle})")
                    
                    # Read descriptor value
                    desc_value = await read_descriptor_value(client, descriptor.handle)
                    if desc_value:
                        print(f"      Value: {desc_value.hex()}")
                        
                        # Parse Report Reference
                        if "2908" in descriptor.uuid and len(desc_value) >= 2:
                            report_id = desc_value[0]
                            report_type = desc_value[1]
                            type_name = {1: "Input", 2: "Output", 3: "Feature"}.get(report_type, "Unknown")
                            print(f"      → Report ID: {report_id}, Type: {type_name} ({report_type})")
        
        # Validate HID Service
        print(f"\n{'='*60}")
        print("HID Service Validation")
        print(f"{'='*60}")
        
        if not hid_service:
            result.add_error("HID Service (0x1812) NOT FOUND!")
            result.print_summary()
            return False
        else:
            result.add_info("HID Service (0x1812) found")
        
        # Check required characteristics
        for uuid, spec in EXPECTED_HID_CHARACTERISTICS.items():
            name = spec["name"]
            
            if spec.get("multiple"):
                # Handle multiple Report characteristics
                if uuid == HID_REPORT_CHAR_UUID:
                    if report_chars:
                        result.add_info(f"{name} characteristics found: {len(report_chars)} instances")
                    else:
                        result.add_error(f"{name} characteristic NOT FOUND")
                continue
            
            if uuid in hid_chars_found:
                char = hid_chars_found[uuid]
                result.add_info(f"{name} found (handle: {char.handle})")
                
                # Check required properties
                for req_prop in spec.get("required_props", []):
                    if req_prop in char.properties:
                        result.add_info(f"  {name} has required '{req_prop}' property")
                    else:
                        result.add_error(f"  {name} MISSING required '{req_prop}' property")
            else:
                if "Boot" in name:
                    result.add_warning(f"{name} not found (optional for Report mode)")
                else:
                    result.add_error(f"{name} NOT FOUND (required)")
        
        # Validate Battery Service
        print(f"\n{'='*60}")
        print("Battery Service Validation")
        print(f"{'='*60}")
        
        if battery_service:
            result.add_info("Battery Service (0x180F) found")
        else:
            result.add_warning("Battery Service (0x180F) not found (recommended for HID)")
        
        # Test reading HID characteristics
        print(f"\n{'='*60}")
        print("Reading HID Characteristic Values")
        print(f"{'='*60}")
        
        await asyncio.sleep(0.5)
        
        # Read HID Information
        if HID_INFO_CHAR_UUID in hid_chars_found:
            print("\n[READ] HID Information...")
            try:
                value = await client.read_gatt_char(hid_chars_found[HID_INFO_CHAR_UUID])
                print(f"  Raw: {value.hex()}")
                info = parse_hid_info(value)
                print(f"  bcdHID: {info.get('bcdHID', 'N/A')}")
                print(f"  Country Code: {info.get('bCountryCode', 'N/A')}")
                print(f"  Flags: 0x{info.get('flags', 0):02X}")
                print(f"    Remote Wake: {info.get('remote_wake', False)}")
                print(f"    Normally Connectable: {info.get('normally_connectable', False)}")
                result.add_info("HID Information readable")
            except Exception as e:
                result.add_error(f"Failed to read HID Information: {e}")
        
        # Read Protocol Mode
        if HID_PROTOCOL_MODE_CHAR_UUID in hid_chars_found:
            print("\n[READ] Protocol Mode...")
            try:
                value = await client.read_gatt_char(hid_chars_found[HID_PROTOCOL_MODE_CHAR_UUID])
                print(f"  Raw: {value.hex()}")
                print(f"  Mode: {parse_protocol_mode(value)}")
                result.add_info("Protocol Mode readable")
            except Exception as e:
                result.add_error(f"Failed to read Protocol Mode: {e}")
        
        # Read Report Map
        if HID_REPORT_MAP_CHAR_UUID in hid_chars_found:
            print("\n[READ] Report Map...")
            try:
                value = await client.read_gatt_char(hid_chars_found[HID_REPORT_MAP_CHAR_UUID])
                print(f"  Length: {len(value)} bytes")
                print(f"  Data: {value.hex()}")
                if len(value) < 10:
                    result.add_warning(f"Report Map is very short ({len(value)} bytes)")
                else:
                    result.add_info(f"Report Map readable ({len(value)} bytes)")
            except Exception as e:
                result.add_error(f"Failed to read Report Map: {e}")
        
        # Read Battery Level
        if battery_service:
            print("\n[READ] Battery Level...")
            try:
                for char in battery_service.characteristics:
                    if char.uuid.lower() == BATTERY_LEVEL_CHAR_UUID:
                        value = await client.read_gatt_char(char)
                        print(f"  Battery Level: {value[0]}%")
                        result.add_info(f"Battery Level: {value[0]}%")
                        break
            except Exception as e:
                result.add_warning(f"Failed to read Battery Level: {e}")
        
        # Subscribe to notifications from Boot Keyboard Input
        print(f"\n{'='*60}")
        print("Testing Notifications")
        print(f"{'='*60}")
        
        subscribed_chars = []
        
        # Subscribe to Boot Keyboard Input
        if HID_BOOT_KBD_INPUT_CHAR_UUID in hid_chars_found:
            char = hid_chars_found[HID_BOOT_KBD_INPUT_CHAR_UUID]
            if "notify" in char.properties or "indicate" in char.properties:
                print("\n[SUBSCRIBE] Boot Keyboard Input Report...")
                try:
                    await client.start_notify(char, notification_handler)
                    print("  ✓ Subscribed successfully")
                    subscribed_chars.append(char)
                    result.add_info("Boot Keyboard Input notifications enabled")
                except Exception as e:
                    result.add_error(f"Failed to subscribe to Boot Keyboard Input: {e}")
        
        # Subscribe to Report characteristics with notify
        for i, char in enumerate(report_chars):
            if "notify" in char.properties or "indicate" in char.properties:
                print(f"\n[SUBSCRIBE] Report characteristic #{i+1} (handle: {char.handle})...")
                try:
                    await client.start_notify(char, notification_handler)
                    print("  ✓ Subscribed successfully")
                    subscribed_chars.append(char)
                except Exception as e:
                    print(f"  Error: {e}")
        
        if subscribed_chars:
            print("\n[WAITING] Press keys on the HID keyboard (5 seconds)...")
            await asyncio.sleep(5)
            
            # Unsubscribe
            for char in subscribed_chars:
                try:
                    await client.stop_notify(char)
                except Exception:
                    pass
        
        result.print_summary()
        return len(result.errors) == 0


async def main():
    """Main entry point."""
    print("="*60)
    print("u-blox HID Keyboard GATT Client Test")
    print("="*60)
    
    target = None
    if len(sys.argv) > 1:
        target = sys.argv[1]
        print(f"Target specified: {target}")
    
    # If target looks like a MAC address, connect directly
    if target and (":" in target or "-" in target):
        await test_hid_client(target)
        return
    
    # Otherwise scan for devices
    name_filter = target or DEFAULT_DEVICE_NAME
    devices = await scan_for_device(name_filter)
    
    if not devices:
        print(f"\nNo HID devices found.")
        print("\nPlease check:")
        print("  1. Device is advertising (wake it up if it's a mouse/keyboard)")
        print("  2. Bluetooth is enabled on this computer")
        if UBLOX_ONLY:
            print("  3. Device name contains 'u-blox'")
            print("  Tip: Set UBLOX_ONLY = False in script to test with any HID device")
        return
    
    # Connect to first found device
    device = devices[0]
    print(f"\nSelected device: {device.name} ({device.address})")
    
    await test_hid_client(device.address)


if __name__ == "__main__":
    asyncio.run(main())
