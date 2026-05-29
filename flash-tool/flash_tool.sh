#!/bin/bash

FIRMWARE="firmware/audio.bin"
VECU_IP="127.0.0.1"
VECU_PORT="9000"

echo "Rane PKI Flash Tool"
echo "-------------------"

# Only sign if explicitly asked
if [ "$1" == "sign" ]; then
    echo "[SIGN] Signing firmware (SHA-256 + RSA)..."
    openssl dgst -sha256 -sign keys/private.pem \
      -out firmware/audio.sig firmware/audio.bin
    echo "[SIGN] Done. Signature saved to firmware/audio.sig"
    exit 0
fi

# Flash using existing signature
echo "[1/3] Encrypting firmware (AES-256)..."
openssl enc -aes-256-cbc -pbkdf2 \
  -in firmware/audio.bin \
  -out firmware/audio.enc \
  -pass file:keys/aes.key

echo "[2/3] Encrypting signature (AES-256)..."
openssl enc -aes-256-cbc -pbkdf2 \
  -in firmware/audio.sig \
  -out firmware/audio.sig.enc \
  -pass file:keys/aes.key

echo "[3/3] Sending to vECU at $VECU_IP:$VECU_PORT..."
(
  cat firmware/audio.enc
  sleep 1
  cat firmware/audio.sig.enc
) | nc $VECU_IP $VECU_PORT

echo "-------------------"
echo "Done."
