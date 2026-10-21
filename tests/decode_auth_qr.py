#!/usr/bin/env python3
"""Optional independent host decode after host/test-auth.sh; requires zxing-cpp/Pillow."""
from pathlib import Path
from PIL import Image
import zxingcpp
root = Path(__file__).resolve().parents[1]
result = zxingcpp.read_barcode(Image.open(root/'build/auth-qr.pgm'))
assert result and result.text == (root/'build/auth-qr-url.txt').read_text()
print('Independent QR decode matched the complete fixture authorization URL')
