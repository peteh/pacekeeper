import asyncio
from bleak import BleakScanner, BleakClient

BT_TARGET_ADDRESS = "C4:A9:B8:F8:47:D4" # out pitpad device

class PITPADService:
#SERVICE: 00001800-0000-1000-8000-00805f9b34fb (Generic Access Profile)
#  CHARACTERISTIC: 00002a01-0000-1000-8000-00805f9b34fb [read]
#  CHARACTERISTIC: 00002a00-0000-1000-8000-00805f9b34fb [read]
#SERVICE: 00001801-0000-1000-8000-00805f9b34fb (Generic Attribute Profile)
#  CHARACTERISTIC: 00002a05-0000-1000-8000-00805f9b34fb [indicate]
#SERVICE: 00001910-0000-1000-8000-00805f9b34fb (Vendor specific)
#  CHARACTERISTIC: 00002b10-0000-1000-8000-00805f9b34fb [read,notify]
    CHARACTERISTIC_NOTIFY1_UUID = "00002b10-0000-1000-8000-00805f9b34fb"
#  CHARACTERISTIC: 00002b11-0000-1000-8000-00805f9b34fb [read,write-without-response,write]
#SERVICE: 0000fba0-0000-1000-8000-00805f9b34fb (Vendor specific)
#  CHARACTERISTIC: 0000fba1-0000-1000-8000-00805f9b34fb [read,write]
#  CHARACTERISTIC: 0000fba2-0000-1000-8000-00805f9b34fb [read,notify]
    CHARACTERISTIC_NOTIFY_STATE_UUID = "0000fba2-0000-1000-8000-00805f9b34fb"


async def list_characteristics(address):
    async with BleakClient(address) as client:
        if not client.is_connected:
            print(f"Failed to connect to {address}")
            return

        print(f"Connected to {address}\n")
        for service in client.services:
            print(f"SERVICE: {service.uuid} ({service.description})")
            for char in service.characteristics:
                props = ",".join(char.properties)
                print(f"  CHARACTERISTIC: {char.uuid} [{props}]")

def notification_handler_state(sender, data):
    print(f"Notification from {sender} (CHARACTERISTIC_NOTIFY_STATE_UUID): {data}")
    
            
async def main():
    print("Scanning for BLE devices...")

    if input("Use default target address (n = scan for devices)? (y/n): ").lower() == 'y':
        target_address = BT_TARGET_ADDRESS
    else:
        devices = await BleakScanner.discover()
        if not devices:
            print("No devices found.")
            return

        # List found devices
        for i, device in enumerate(devices):
            print(f"{i}: {device.name} [{device.address}]")

        index = int(input("Select device index: "))
        target_address = devices[index].address

    # Connect and list characteristics
    #await list_characteristics(target_address)
    
    print(f"Connecting to device at address: {target_address}")
    async with BleakClient(target_address) as client:
        if not client.is_connected:
            print(f"Failed to connect to {target_address}")
            return

        print(f"Connected to {target_address}\n")

        def notification_handler1(sender, data):
            print(f"Notification from {sender} (CHARACTERISTIC_NOTIFY1): {data}")

        

        await client.start_notify(PITPADService.CHARACTERISTIC_NOTIFY1_UUID, notification_handler1)
        await client.start_notify(PITPADService.CHARACTERISTIC_NOTIFY_STATE_UUID, notification_handler_state)

        print("Listening for notifications... Press Ctrl+C to stop.")
        try:
            while True:
                await asyncio.sleep(1)
        except KeyboardInterrupt:
            print("Stopping notifications...")

        await client.stop_notify(PITPADService.CHARACTERISTIC_NOTIFY1_UUID)
        await client.stop_notify(PITPADService.CHARACTERISTIC_NOTIFY_STATE_UUID)

if __name__ == "__main__":
    asyncio.run(main())