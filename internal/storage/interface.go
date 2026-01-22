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
	UpdateUserRefreshToken(userID, refreshToken string) error
	DeleteUser(userID string) error

	// Device management
	CreateDevice(device *Device) error
	GetDevice(id string) (*Device, error)
	GetDeviceByUID(uid string) (*Device, error)
	GetDeviceByAPIKey(apiKey string) (*Device, error)
	GetUserDevices(userID string) ([]Device, error)
	GetAllDevices() ([]Device, error)
	GetAllDevicesAndOwners() ([]Device, map[string]User, error)
	UpdateDevice(id string, status string) error
	UpdateDeviceThingDescription(id string, td map[string]interface{}) error // New method
	UpdateDeviceConfig(id string, config map[string]interface{}) error       // Remote Config
	UpdateDeviceAPIKey(id string, newKey string) error                       // New method
	UpdateDeviceLastSeen(id string) error
	DeleteDevice(deviceID, userID string) error
	ForceDeleteDevice(id string) error

	// Token Operations
	RotateDeviceKey(deviceID string, newToken string, tokenExpiresAt time.Time, gracePeriodDays int) (*Device, error)
	RevokeDeviceKey(deviceID string) (*Device, error)
	GetDeviceByToken(token string) (*Device, bool, error)
	GetDeviceTokenInfo(deviceID string) (map[string]interface{}, error)
	InitializeDeviceToken(deviceID, masterSecret, token string, tokenExpiresAt time.Time) (*Device, error)
	CleanupExpiredGracePeriods() (int, error)

	// Data Operations
	StoreData(point *DataPoint) error
	StoreDataBatch(points []*DataPoint) error
	GetLatestData(deviceID string) (*DataPoint, error)
	GetDataHistoryWithRange(deviceID string, start, end time.Time, limit int) ([]DataPoint, error)
	GetDataHistory(deviceID string, limit int) ([]DataPoint, error)
	GetDatabaseStats() (map[string]interface{}, error)

	// Command Operations
	CreateCommand(cmd *Command) error
	GetCommand(cmdID string) (*Command, error)
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
	PurgeProvisioningRequests(maxAge time.Duration) (int, error)
	CleanupPublicData(maxAge time.Duration) (int, error)

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
	UpdatePublicDataRetention(days int) error
	UpdateRegistrationConfig(allow bool) error

	// Generic Document Store (Collections)
	CreateDocument(userID, collection string, doc map[string]interface{}) (string, error)
	ListDocuments(userID, collection string) ([]map[string]interface{}, error)
	GetDocument(userID, collection, docID string) (map[string]interface{}, error)
	UpdateDocument(userID, collection, docID string, doc map[string]interface{}) error
	DeleteDocument(userID, collection, docID string) error
	ListAllCollections() ([]CollectionInfo, error)
}

// CollectionInfo represents metadata about a collection
type CollectionInfo struct {
	UserID     string `json:"user_id"`
	Collection string `json:"collection"`
	DocCount   int    `json:"doc_count"`
}
