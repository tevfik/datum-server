/**
 * API Type Helper Utilities
 *
 * This file provides type-safe helpers for using the auto-generated OpenAPI types.
 *
 * Usage:
 * ```typescript
 * import type { ApiSchemas, ApiPaths } from '@/types';
 *
 * // Schema types for request/response bodies
 * const user: ApiSchemas['UserResponse'] = { id: '...', email: '...' };
 *
 * // Path types for route definitions
 * type LoginResponse = ApiPaths['/auth/login']['post']['responses']['200']['content']['application/json'];
 * ```
 */

// Re-export generated types
export * from './api';

// Type aliases for common schema types
import type { components, paths } from './api';

/** Shorthand for component schemas */
export type ApiSchemas = components['schemas'];

/** Shorthand for path definitions */
export type ApiPaths = paths;

// Common schema type aliases for convenience
export type ErrorResponse = ApiSchemas['Error'];
export type AuthResponse = ApiSchemas['AuthResponse'];
export type DeviceResponse = ApiSchemas['DeviceResponse'];
export type DeviceCreateRequest = ApiSchemas['DeviceCreateRequest'];
export type LoginRequest = ApiSchemas['LoginRequest'];
export type RegisterRequest = ApiSchemas['RegisterRequest'];
export type DataPayload = ApiSchemas['DataPayload'];
export type CommandRequest = ApiSchemas['CommandRequest'];
export type SetupRequest = ApiSchemas['SetupRequest'];
export type CreateUserRequest = ApiSchemas['CreateUserRequest'];
export type LogEntry = ApiSchemas['LogEntry'];
