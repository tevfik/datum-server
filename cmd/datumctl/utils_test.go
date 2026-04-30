package main

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"datum-go/internal/cli/utils"
)

func TestIdentifierContainsEmail(t *testing.T) {
	tests := []struct {
		name     string
		input    string
		expected bool
	}{
		{
			name:     "valid email",
			input:    "test@example.com",
			expected: true,
		},
		{
			name:     "invalid email no at",
			input:    "testexample.com",
			expected: false,
		},
		{
			name:     "invalid email empty",
			input:    "",
			expected: false,
		},
		{
			name:     "id with special chars",
			input:    "user_123",
			expected: false,
		},
		{
			name:     "email with numbers",
			input:    "user123@test.com",
			expected: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			result := utils.IdentifierContainsEmail(tt.input)
			assert.Equal(t, tt.expected, result)
		})
	}
}
