package auth

import (
	"testing"
)

func TestEncryptDecryptWiFiPassword(t *testing.T) {
	tests := []struct {
		name     string
		password string
		key      string
	}{
		{"simple password", "MyWiFiPassword123", "device_master_secret_1234567890"},
		{"empty password", "", "another_master_secret_1234567890"},
		{"special chars", "P@$$w0rd!#$%^&*()", "key_with_special_chars_xxxxxxx"},
		{"unicode", "日本語パスワード", "unicode_key_test_123456789012345"},
		{"long password", "ThisIsAVeryLongWiFiPasswordThatShouldBeEncryptedCorrectly123", "long_key_test_12345678901234567"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			key := DeriveEncryptionKey(tt.key)

			encrypted, err := EncryptWiFiPassword(tt.password, key)
			if err != nil {
				t.Fatalf("Failed to encrypt: %v", err)
			}

			if encrypted == tt.password && tt.password != "" {
				t.Error("Encrypted password should not equal plaintext")
			}

			decrypted, err := DecryptWiFiPassword(encrypted, key)
			if err != nil {
				t.Fatalf("Failed to decrypt: %v", err)
			}

			if decrypted != tt.password {
				t.Errorf("Decrypted password mismatch: got %q, want %q", decrypted, tt.password)
			}
		})
	}
}

func TestEncryptWithDifferentKeys(t *testing.T) {
	password := "SecretWiFi123"
	key1 := DeriveEncryptionKey("master_secret_device_1")
	key2 := DeriveEncryptionKey("master_secret_device_2")

	encrypted1, _ := EncryptWiFiPassword(password, key1)
	encrypted2, _ := EncryptWiFiPassword(password, key2)

	// Same password with different keys should produce different ciphertexts
	if encrypted1 == encrypted2 {
		t.Error("Different keys should produce different ciphertexts")
	}

	// Decrypting with wrong key should fail
	_, err := DecryptWiFiPassword(encrypted1, key2)
	if err == nil {
		t.Error("Decrypting with wrong key should fail")
	}
}

func TestDeriveEncryptionKey(t *testing.T) {
	tests := []struct {
		name   string
		secret string
	}{
		{"short secret", "abc"},
		{"exact 32 bytes", "12345678901234567890123456789012"},
		{"longer than 32", "1234567890123456789012345678901234567890"},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			key := DeriveEncryptionKey(tt.secret)

			if len(key) != 32 {
				t.Errorf("Expected 32-byte key, got %d bytes", len(key))
			}
		})
	}
}

func BenchmarkWiFiEncryption(b *testing.B) {
	password := "BenchmarkWiFiPassword123"
	key := DeriveEncryptionKey("benchmark_master_secret_12345678")

	b.Run("Encrypt", func(b *testing.B) {
		for i := 0; i < b.N; i++ {
			EncryptWiFiPassword(password, key)
		}
	})

	encrypted, _ := EncryptWiFiPassword(password, key)
	b.Run("Decrypt", func(b *testing.B) {
		for i := 0; i < b.N; i++ {
			DecryptWiFiPassword(encrypted, key)
		}
	})
}
