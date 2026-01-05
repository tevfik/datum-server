package storage

import "time"

// Provider defines the interface for storage operations.
// This allows swapping the underlying implementation (e.g., BuntDB -> PostgreSQL)
// without changing the application logic.
type Provider interface {
	// Lifecycle
	Close() error

	// User Operations
	CreateUser(user *User) error
	GetUserByEmail(email string) (*User, error)
	GetUserByID(userID string) (*User, error)
	GetUserCount() (int, error)
	ListAllUsers() ([]User, error)
	UpdateUser(userID string, role, status string) error
	UpdateUserLastLogin(userID string) error
	UpdateUserPassword(userID string, passwordHash string) error
	DeleteUser(userID string) error

	// Device Operations
	CreateDevice(device *Device) error
	GetDevice(deviceID string) (*Device, error)
	GetDeviceByAPIKey(apiKey string) (*Device, error)
	GetUserDevices(userID string) ([]Device, error)
	DeleteDevice(deviceID, userID string) error
	ListAllDevices() ([]Device, error)
	GetAllDevices() ([]Device, error) // Admin
	UpdateDevice(deviceID string, status string) error
	UpdateDeviceLastSeen(deviceID string) error
	ForceDeleteDevice(deviceID string) error

	// Token Operations
	RotateDeviceKey(deviceID string, newToken string, tokenExpiresAt time.Time, gracePeriodDays int) (*Device, error)
	RevokeDeviceKey(deviceID string) (*Device, error)
	GetDeviceByToken(token string) (*Device, bool, error)
	GetDeviceTokenInfo(deviceID string) (map[string]interface{}, error)
	InitializeDeviceToken(deviceID, masterSecret, token string, tokenExpiresAt time.Time) (*Device, error)
	CleanupExpiredGracePeriods() (int, error)

	// Data Operations
	StoreData(point *DataPoint) error
	GetLatestData(deviceID string) (*DataPoint, error)
	GetDataHistoryWithRange(deviceID string, start, end time.Time, limit int) ([]DataPoint, error)
	GetDataHistory(deviceID string, limit int) ([]DataPoint, error)
	GetDatabaseStats() (map[string]interface{}, error)

	// Command Operations
	CreateCommand(cmd *Command) error
	GetPendingCommands(deviceID string) ([]Command, error)
	AcknowledgeCommand(cmdID string, result map[string]interface{}) error
	GetPendingCommandCount(deviceID string) int

	// Password Reset Operations
	SavePasswordResetToken(userID, token string, ttl time.Duration) error
	GetUserByResetToken(token string) (*User, error)
	DeleteResetToken(token string) error

	// Provisioning Operations
	CreateProvisioningRequest(req *ProvisioningRequest) error
	GetProvisioningRequestByUID(deviceUID string) (*ProvisioningRequest, error)
	GetProvisioningRequest(reqID string) (*ProvisioningRequest, error)
	CompleteProvisioningRequest(reqID string) (*Device, error)
	CancelProvisioningRequest(reqID string) error
	GetUserProvisioningRequests(userID string) ([]ProvisioningRequest, error)
	IsDeviceUIDRegistered(deviceUID string) (bool, string, error)
	DeleteDeviceUIDIndex(deviceUID string) error
	CleanupExpiredProvisioningRequests() (int, error)

	// User API Key Operations
	CreateUserAPIKey(key *APIKey) error
	GetUserAPIKeys(userID string) ([]APIKey, error)
	DeleteUserAPIKey(userID, keyID string) error
	GetUserByUserAPIKey(apiKey string) (*User, error)

	// System Operations
	GetSystemConfig() (*SystemConfig, error)
	SaveSystemConfig(config *SystemConfig) error
	IsSystemInitialized() bool
	InitializeSystem(platformName string, allowRegister bool, retention int) error
	ResetSystem() error
	ExportDatabase() (map[string]interface{}, error)
	UpdateDataRetention(days int) error
	UpdateRegistrationConfig(allow bool) error
}
