import struct
import os
import math

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
    # Green theme: Leaf
    xor_data = bytearray()
    
    # Coordinates: y=0 is bottom, y=31 is top
    for y in range(height):
        for x in range(width):
            # Default transparent
            b, g, r, a = 0, 0, 0, 0
            
            # Leaf Construction
            # Use intersection of two circles to create a leaf shape
            # Axis of leaf: Diagonal from (0,0) to (32,32)
            # Centers of circles should be on the perpendicular diagonal
            
            # Center 1 (Top-Left area)
            cx1, cy1 = 4, 28
            # Center 2 (Bottom-Right area)
            cx2, cy2 = 28, 4
            
            # Radius needs to be large enough to cover the center
            # Distance between centers is approx sqrt(24^2 + 24^2) = 33.9
            # Midpoint distance = 17.
            # So radius must be > 17.
            radius = 24.0
            
            d1 = math.sqrt((x - cx1)**2 + (y - cy1)**2)
            d2 = math.sqrt((x - cx2)**2 + (y - cy2)**2)
            
            in_leaf = (d1 < radius) and (d2 < radius)
            
            # Stem logic
            # A small curved line at the bottom left
            # Let's approximate with a small rectangle or line near (6,6)
            # Distance to main diagonal
            dist_diag = (y - x) / math.sqrt(2) # signed distance
            pos_diag = (x + y) / math.sqrt(2)
            
            # Stem area: near the base of the leaf
            # Leaf tips are roughly where d1=radius and d2=radius on the diagonal.
            # Calculation: (x-4)^2 + (x-28)^2 = 24^2 => 2x^2 - 64x + ...
            # Let's just empirically say the stem is around x=6, y=6
            
            in_stem = False
            if not in_leaf:
                if abs(dist_diag) < 1.5 and pos_diag > 6 and pos_diag < 12:
                    in_stem = True
            
            if in_leaf:
                # Green Gradient
                # Varies from dark green at bottom to lighter at top
                # Normalized position 0..1
                t = (x + y) / 64.0 
                
                # Base color (Dark Green) to Light Green
                # R: 0 -> 40
                # G: 100 -> 220
                # B: 0 -> 40
                
                cur_r = int(0 + t * 60)
                cur_g = int(120 + t * 135)
                cur_b = int(0 + t * 80)
                
                # Main Vein (Central)
                if abs(dist_diag) < 0.5:
                    cur_r, cur_g, cur_b = 100, 255, 120 # Bright vein
                elif abs(dist_diag) < 0.8:
                    cur_r, cur_g, cur_b = 60, 230, 80 # Vein edge
                    
                # Border
                edge_dist = min(radius - d1, radius - d2)
                if edge_dist < 1.5:
                    cur_r, cur_g, cur_b = 0, 80, 0 # Dark border
                
                b, g, r, a = cur_b, cur_g, cur_r, 255
                
            elif in_stem:
                # Brownish Green Stem
                b, g, r, a = 10, 100, 60, 255
            
            xor_data.extend([b, g, r, a])

    # AND Mask (1 bit per pixel). 0 = opaque, 1 = transparent.
    and_bits = []
    idx = 0
    for y in range(height):
        row_bits = 0
        for x in range(width):
            # Get alpha from xor_data
            alpha = xor_data[idx*4 + 3]
            idx += 1
            
            # If alpha is 0 (fully transparent), set bit to 1.
            # If alpha > 0 (partially or fully opaque), set bit to 0.
            if alpha == 0:
                row_bits |= (1 << (7 - (x % 8)))
            
            if (x + 1) % 8 == 0:
                and_bits.append(row_bits)
                row_bits = 0
    
    and_data = bytearray(and_bits)

    xor_size = len(xor_data)
    and_size = len(and_data)
    total_size = 40 + xor_size + and_size
    
    # Offset to image data = Header(6) + DirectoryEntry(16)
    offset = 6 + 16
    
    # Image Directory Entry
    entry = struct.pack('<BBBBHHII', width, height, 0, 0, 1, 32, total_size, offset)

    # BITMAPINFOHEADER
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
