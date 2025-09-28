BOOTLOADER_SIZE     = (0x6000)
BOOTLOADER_BIN_FILE = "bootloader.bin"

with open(BOOTLOADER_BIN_FILE, "rb") as f:
    raw_file = f.read()

numbytes_to_pad = (BOOTLOADER_SIZE - len(raw_file))
padding = bytes([0xff] * numbytes_to_pad)

with open(BOOTLOADER_BIN_FILE, "wb") as f:
    f.write(raw_file + padding)