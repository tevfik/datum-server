package auth

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"encoding/base64"
	"errors"
	"io"
)

// WiFiEncryption provides AES-256-GCM encryption for WiFi passwords
// Uses device master secret as the encryption key

var (
	ErrInvalidCiphertext = errors.New("invalid ciphertext")
	ErrInvalidKeyLength  = errors.New("key must be 32 bytes for AES-256")
)

// EncryptWiFiPassword encrypts a WiFi password using AES-256-GCM
// The key should be a 32-byte master secret
func EncryptWiFiPassword(password string, key []byte) (string, error) {
	if len(key) < 32 {
		// Pad key to 32 bytes if shorter
		paddedKey := make([]byte, 32)
		copy(paddedKey, key)
		key = paddedKey
	} else if len(key) > 32 {
		key = key[:32]
	}

	block, err := aes.NewCipher(key)
	if err != nil {
		return "", err
	}

	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return "", err
	}

	nonce := make([]byte, gcm.NonceSize())
	if _, err := io.ReadFull(rand.Reader, nonce); err != nil {
		return "", err
	}

	ciphertext := gcm.Seal(nonce, nonce, []byte(password), nil)
	return base64.StdEncoding.EncodeToString(ciphertext), nil
}

// DecryptWiFiPassword decrypts a WiFi password using AES-256-GCM
// The key should be the same 32-byte master secret used for encryption
func DecryptWiFiPassword(encryptedPassword string, key []byte) (string, error) {
	if len(key) < 32 {
		// Pad key to 32 bytes if shorter
		paddedKey := make([]byte, 32)
		copy(paddedKey, key)
		key = paddedKey
	} else if len(key) > 32 {
		key = key[:32]
	}

	ciphertext, err := base64.StdEncoding.DecodeString(encryptedPassword)
	if err != nil {
		return "", err
	}

	block, err := aes.NewCipher(key)
	if err != nil {
		return "", err
	}

	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return "", err
	}

	if len(ciphertext) < gcm.NonceSize() {
		return "", ErrInvalidCiphertext
	}

	nonce, ciphertext := ciphertext[:gcm.NonceSize()], ciphertext[gcm.NonceSize():]
	plaintext, err := gcm.Open(nil, nonce, ciphertext, nil)
	if err != nil {
		return "", err
	}

	return string(plaintext), nil
}

// DeriveEncryptionKey derives a 32-byte encryption key from device master secret
func DeriveEncryptionKey(masterSecret string) []byte {
	key := make([]byte, 32)
	secretBytes := []byte(masterSecret)

	// Simple key derivation - copy and repeat if needed
	for i := 0; i < 32; i++ {
		if i < len(secretBytes) {
			key[i] = secretBytes[i]
		} else {
			key[i] = secretBytes[i%len(secretBytes)]
		}
	}

	return key
}
