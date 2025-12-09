import struct
import os

def create_icon(filename):
    # Ensure directory exists
    os.makedirs(os.path.dirname(filename), exist_ok=True)
    
    # ICO Header
    # Reserved (2), Type (2, 1=ICO), Count (2)
    header = struct.pack('<HHH', 0, 1, 1)

    # Image dimensions
    width = 32
    height = 32
    
    # Pixel Data (BGRA)
    # Blue icon with a lighter "E" shape or just a border
    xor_data = bytearray()
    for y in range(height):
        for x in range(width):
            # Default Blue background
            b, g, r, a = 220, 120, 60, 255 # Nice Blue-ish
            
            # Simple border
            if x == 0 or x == width-1 or y == 0 or y == height-1:
                b, g, r = 180, 180, 180
            
            # Draw an "E" shape
            # Vertical bar
            if x >= 6 and x <= 10 and y >= 6 and y <= 25:
                b, g, r = 255, 255, 255
            # Top bar
            if x >= 6 and x <= 22 and y >= 21 and y <= 25: # Inverted Y (BMP is bottom-up)? 
                # Actually BMP is usually bottom-up, so y=0 is bottom.
                # Let's assume standard bottom-up DIB.
                # If y=0 is bottom, then y=31 is top.
                pass
            
            # Let's just do a simple gradient to be safe
            if (x - 16)**2 + (y - 16)**2 < 100:
                b, g, r = 255, 200, 100 # Center dot
                
            xor_data.extend([b, g, r, a])

    # AND Mask (1 bit per pixel). 0 = opaque, 1 = transparent.
    # 32 pixels wide = 32 bits = 4 bytes per row.
    and_data = bytearray(width * height // 8)

    xor_size = len(xor_data)
    and_size = len(and_data)
    total_size = 40 + xor_size + and_size # 40 is BITMAPINFOHEADER size

    # Image Directory Entry
    # Width(1), Height(1), Colors(1), Reserved(1), Planes(2), BPP(2), Size(4), Offset(4)
    offset = 6 + 16 # Header + 1 Entry
    entry = struct.pack('<BBBBHHII', width, height, 0, 0, 1, 32, total_size, offset)

    # BITMAPINFOHEADER
    # Size (4), Width (4), Height (4), Planes (2), BPP (2), Compression (4), ImageSize (4), ...
    # Height is height * 2 for ICO (XOR + AND masks)
    bih = struct.pack('<IiiHHIIIIII', 40, width, height * 2, 1, 32, 0, xor_size + and_size, 0, 0, 0, 0)

    with open(filename, 'wb') as f:
        f.write(header)
        f.write(entry)
        f.write(bih)
        f.write(xor_data)
        f.write(and_data)
    
    print(f"Created {filename}")

if __name__ == '__main__':
    create_icon('src/app.ico')
