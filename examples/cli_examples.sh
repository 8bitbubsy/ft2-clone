#!/bin/bash
# CLI usage examples for ft2-api

echo "=== ft2-api CLI Examples ==="
echo ""

# Example 1: Basic rendering at default settings (44.1kHz, 16-bit)
echo "Example 1: Basic rendering"
echo "$ ft2-api --cli render song.mod song.wav"
echo ""

# Example 2: Render at 48kHz, 16-bit
echo "Example 2: Render at 48kHz"
echo "$ ft2-api --cli render song.mod song.wav --rate 48000 --bits 16"
echo ""

# Example 3: High quality rendering (48kHz, 32-bit float)
echo "Example 3: High quality rendering (48kHz, 32-bit float)"
echo "$ ft2-api --cli render song.mod song.wav --rate 48000 --bits 32"
echo ""

# Example 4: With amplification
echo "Example 4: With amplification boost"
echo "$ ft2-api --cli render song.mod song.wav --amp 24"
echo ""

# Example 5: Render specific pattern range
echo "Example 5: Render patterns 0-15 only"
echo "$ ft2-api --cli render song.mod song.wav --start 0 --stop 15"
echo ""

# Example 6: All options combined
echo "Example 6: All options combined"
echo "$ ft2-api --cli render song.mod song.wav \\"
echo "    --rate 48000 \\"
echo "    --bits 32 \\"
echo "    --amp 20 \\"
echo "    --start 0 \\"
echo "    --stop 63"
echo ""

# Example 7: Batch processing multiple files
echo "Example 7: Batch processing with shell loop"
echo "$ for mod in *.mod; do"
echo "    wav=\${mod%.mod}.wav"
echo "    ft2-api --cli render \"\$mod\" \"\$wav\" --rate 48000"
echo "  done"
echo ""
