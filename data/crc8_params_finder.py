import itertools

# Function to calculate CRC-8
def crc8(data, poly, init_val):
    crc = init_val
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = (crc << 1) ^ poly
            else:
                crc <<= 1
            crc &= 0xFF  # Ensure 8-bit result
    return crc

# Convert binary string to bytearray
def bin_str_to_bytes(data_bin_str):
    byte_list = []
    for i in range(0, len(data_bin_str), 8):
        byte = int(data_bin_str[i:i+8], 2)
        byte_list.append(byte)
    return bytearray(byte_list)

# Input data (binary string)
data_bin_str = '1010110111110001001100000110000000001011'
# Expected CRC in binary
given_crc_bin_str = '00110111'
given_crc = int(given_crc_bin_str, 2)

# Convert data binary string to bytes
data_bytes = bin_str_to_bytes(data_bin_str)

# Common CRC-8 polynomials (you can add more if needed)
polynomials = [
    0x9B,  # CRC-8-CDMA2000
    0x39,  # CRC-8-DARC
    0xD5,  # CRC-8-DVB-S2
    0x07,  # CRC-8-ITU
    0x31,  # CRC-8-Maxim
]

# Possible initialization values (0x00 to 0xFF)
init_values = range(0x00, 0x100)

# Search for matching polynomial and init value
results = []
for poly, init_val in itertools.product(polynomials, init_values):
    crc_result = crc8(data_bytes, poly, init_val)
    if crc_result == given_crc:
        results.append((poly, init_val))

# Print results
if results:
    for poly, init_val in results:
        print(f"Found match! Polynomial: {hex(poly)}, Initialization Value: {hex(init_val)}")
else:
    print("No match found.")