# Datum IoT Platform - Documentation Index

Welcome to the Datum IoT Platform documentation. This documentation is organized into the following categories:

## 📚 Guides

Step-by-step guides for common tasks and setup procedures:

- **[Testing Guide](guides/TESTING.md)** - Comprehensive testing documentation, coverage reports, and best practices
- **[Deployment Guide](guides/DEPLOYMENT.md)** - Production deployment with Docker, Kubernetes, systemd, and cloud platforms
- **[Registration Guide](guides/REGISTRATION.md)** - How to setup, register users, and manage accounts
- **[Password Reset Guide](guides/PASSWORD_RESET.md)** - Reset forgotten passwords and recover access
- **[Security Guide](guides/SECURITY.md)** - Security best practices and authentication
- **[Provisioning Guide](guides/PROVISIONING.md)** - Device provisioning and management
- **[Data Retention Guide](guides/RETENTION.md)** - Configure data retention policies

## 🎓 Tutorials

Hands-on tutorials for working with the platform:

- **[Quick Start Guide](tutorials/QUICK_START.md)** - Get your first device running in 10 minutes
- **[Use Cases](tutorials/USE_CASES.md)** - Real-world application examples
- **[CLI Tutorial](tutorials/CLI.md)** - Complete guide to using the datumctl command-line tool
- **[Firmware Development](tutorials/FIRMWARE.md)** - Build firmware for IoT devices
- **[SSE Commands](tutorials/SSE_COMMANDS.md)** - Server-Sent Events and real-time commands

## 📖 Reference

Technical reference documentation:

- **[API Reference](reference/API.md)** - Complete REST API documentation
- **[Storage Reference](reference/STORAGE.md)** - Storage architecture and design
- **[TSStorage Reference](reference/TSTORAGE.md)** - Time-series storage details
- **[Rate Limiting](reference/RATE_LIMITING.md)** - Rate limiting configuration and behavior

## ⚡ Performance

Performance benchmarks and test results:

- **[Performance Report](performance/FINAL_PERFORMANCE_REPORT.md)** - Comprehensive performance analysis
- **[BuntDB Test Results](performance/BUNTDB_TEST_RESULTS.md)** - Metadata storage benchmarks
- **[TSStorage Test Results](performance/TSTORAGE_TEST_RESULTS.md)** - Time-series storage benchmarks

## 📊 Diagrams

Visual documentation and architecture diagrams:

- **[Architecture Diagrams](diagrams/ARCHITECTURE.md)** - System architecture, data flow, and deployment diagrams

## Quick Links

### Getting Started
1. [Quick Start Guide](../QUICKSTART.md) - Get up and running in 5 minutes
2. [Registration Guide](guides/REGISTRATION.md) - Setup your first admin user
3. [CLI Tutorial](tutorials/CLI.md) - Learn the command-line tools

### Development
- [Contributing Guide](../CONTRIBUTING.md) - How to contribute to the project
- [Testing Guide](guides/TESTING.md) - Writing and running tests
- [Deployment Guide](guides/DEPLOYMENT.md) - Deploy to production

### Common Tasks
- [Create devices](tutorials/CLI.md#device-management)
- [Query data](tutorials/CLI.md#data-queries)
- [Reset password](guides/PASSWORD_RESET.md)
- [Configure retention](guides/RETENTION.md)
- [Setup monitoring](guides/DEPLOYMENT.md#monitoring)
- [Manage users](guides/REGISTRATION.md#user-registration)

### API & Development
- [REST API](reference/API.md)
- [Firmware Development](tutorials/FIRMWARE.md)
- [Storage Architecture](reference/STORAGE.md)

## Additional Resources

### Project Root Documentation
- [README.md](../README.md) - Project overview
- [QUICKSTART.md](../QUICKSTART.md) - Fast setup guide
- [PROJECT.md](../PROJECT.md) - Project structure and architecture

### Need Help?

- Check the [CLI Tutorial](tutorials/CLI.md) for command-line usage
- Read the [API Reference](reference/API.md) for API details
- See [REGISTRATION.md](guides/REGISTRATION.md) for user management
- Review [SECURITY.md](guides/SECURITY.md) for security practices

## Documentation Organization

```
docs/
├── README.md                          # This index file
├── diagrams/                          # Visual documentation
│   └── ARCHITECTURE.md               # System architecture diagrams
├── guides/                            # How-to guides
│   ├── DEPLOYMENT.md                 # Production deployment
│   ├── PASSWORD_RESET.md             # Password recovery
│   ├── PROVISIONING.md               # Device provisioning
│   ├── REGISTRATION.md               # User registration & setup
│   ├── RETENTION.md                  # Data retention policies
│   ├── SECURITY.md                   # Security best practices
│   └── TESTING.md                    # Testing guide
├── tutorials/                         # Step-by-step tutorials
│   ├── CLI.md                        # Command-line tool tutorial
│   ├── FIRMWARE.md                   # Firmware development guide
│   ├── QUICK_START.md                # Quick start guide
│   ├── SSE_COMMANDS.md               # Real-time commands tutorial
│   └── USE_CASES.md                  # Real-world use cases
├── reference/                         # Technical references
│   ├── API.md                        # REST API documentation
│   ├── RATE_LIMITING.md              # Rate limiting reference
│   ├── STORAGE.md                    # Storage architecture
│   └── TSTORAGE.md                   # Time-series storage
└── performance/                       # Benchmarks & reports
    ├── BUNTDB_TEST_RESULTS.md        # BuntDB benchmarks
    ├── FINAL_PERFORMANCE_REPORT.md   # Overall performance
    └── TSTORAGE_TEST_RESULTS.md      # TSStorage benchmarks
```

---

**Datum IoT Platform** - High-performance IoT data platform built with Go
