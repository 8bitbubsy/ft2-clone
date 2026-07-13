#!/bin/bash
# REST API usage examples for ft2-api

echo "=== ft2-api REST API Examples ==="
echo ""

# Start the server
echo "Starting server:"
echo "$ ft2-api --server 8080"
echo ""

# Example 1: Health check
echo "Example 1: Health check"
echo "$ curl http://localhost:8080/api/health"
echo ""

# Example 2: Render a MOD file
echo "Example 2: Render a MOD file"
echo "$ curl -F 'file=@song.mod' http://localhost:8080/api/render"
echo ""

# Example 3: Render with custom parameters
echo "Example 3: Render with custom sample rate (48kHz)"
echo "$ curl -F 'file=@song.mod' \\"
echo "       -F 'rate=48000' \\"
echo "       -F 'bits=16' \\"
echo "       http://localhost:8080/api/render"
echo ""

# Example 4: Download rendered WAV
echo "Example 4: Download rendered WAV file"
echo "$ curl http://localhost:8080/api/download/song.wav -o song.wav"
echo ""

# Example 5: Complete workflow
echo "Example 5: Complete workflow (render and download)"
echo "$ curl -s -F 'file=@input.mod' http://localhost:8080/api/render | jq '.download_url'"
echo ""

# Example 6: Python example
echo "Example 6: Python example"
echo "#!/usr/bin/env python3"
echo "import requests"
echo ""
echo "with open('song.mod', 'rb') as f:"
echo "    response = requests.post("
echo "        'http://localhost:8080/api/render',"
echo "        files={'file': f},"
echo "        data={'rate': '48000'}"
echo "    )"
echo "    print(response.json())"
echo ""
