#!/bin/bash

# Usage:
#   ./flash_tool.sh sign <filepath>
#   ./flash_tool.sh send <filepath>

VECU_IP="127.0.0.1"
VECU_PORT="9000"

if [ -z "$2" ]; then
    echo "Usage: ./flash_tool.sh [sign|send] <filepath>"
    echo "  sign  - sign the firmware file"
    echo "  send  - encrypt and send to vECU"
    exit 1
fi

FIRMWARE="$2"
SIG_FILE="${FIRMWARE}.sig"
ENC_FILE="${FIRMWARE}.enc"
SIG_ENC_FILE="${FIRMWARE}.sig.enc"

# Check file exists
if [ ! -f "$FIRMWARE" ]; then
    echo "Error: file not found: $FIRMWARE"
    exit 1
fi

# Check supported extensions
EXT="${FIRMWARE##*.}"
case "$EXT" in
    bin|hex|s19|srec|elf|out|axf)
        echo "File type: .$EXT -- supported"
        ;;
    *)
        echo "Warning: unknown file type .$EXT -- proceeding anyway"
        ;;
esac

echo "Rane PKI Flash Tool"
echo "-------------------"
echo "File: $FIRMWARE"
echo "Size: $(du -h $FIRMWARE | cut -f1)"
echo "-------------------"

if [ "$1" == "sign" ]; then
    echo "[SIGN] Signing with SHA-256 + RSA-2048..."
    openssl dgst -sha256 -sign keys/private.pem \
      -out "$SIG_FILE" "$FIRMWARE"
    echo "[SIGN] Signature saved to $SIG_FILE"
    exit 0
fi

if [ "$1" == "send" ]; then
    # Check signature exists
    if [ ! -f "$SIG_FILE" ]; then
        echo "Error: no signature found for $FIRMWARE"
        echo "Run sign first: ./flash_tool.sh sign $FIRMWARE"
        exit 1
    fi

    echo "[1/3] Encrypting firmware (AES-256)..."
    openssl enc -aes-256-cbc -pbkdf2 \
      -in "$FIRMWARE" \
      -out "$ENC_FILE" \
      -pass file:keys/aes.key

    echo "[2/3] Encrypting signature (AES-256)..."
    openssl enc -aes-256-cbc -pbkdf2 \
      -in "$SIG_FILE" \
      -out "$SIG_ENC_FILE" \
      -pass file:keys/aes.key

    echo "[3/3] Sending to vECU at $VECU_IP:$VECU_PORT..."
    (
      cat "$ENC_FILE"
      sleep 1
      cat "$SIG_ENC_FILE"
    ) | nc $VECU_IP $VECU_PORT

    echo "-------------------"
    echo "Done."
    exit 0
fi

echo "Error: unknown command $1"
echo "Usage: ./flash_tool.sh [sign|send] <filepath>"
exit 1
