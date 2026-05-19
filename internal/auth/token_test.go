package auth

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestGenerateMasterSecret(t *testing.T) {
	secret1, err := GenerateMasterSecret()
	require.NoError(t, err)
	assert.NotEmpty(t, secret1)
	assert.GreaterOrEqual(t, len(secret1), MasterSecretMinLength)

	// Generate another to verify uniqueness
	secret2, err := GenerateMasterSecret()
	require.NoError(t, err)
	assert.NotEqual(t, secret1, secret2, "secrets should be unique")
}
