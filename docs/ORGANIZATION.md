# Documentation Organization Summary

The documentation has been reorganized into a clear, logical structure:

## 📂 Directory Structure

```
docs/
├── README.md                          # Documentation index (START HERE)
│
├── 📚 guides/                         # Step-by-step guides for common tasks
│   ├── PASSWORD_RESET.md             # How to reset forgotten passwords
│   ├── PROVISIONING.md               # Device provisioning workflow
│   ├── REGISTRATION.md               # User registration & system setup
│   ├── RETENTION.md                  # Data retention configuration
│   └── SECURITY.md                   # Security best practices
│
├── 🎓 tutorials/                      # Hands-on learning resources
│   ├── CLI.md                        # Complete datumctl tutorial
│   ├── FIRMWARE.md                   # IoT firmware development
│   └── SSE_COMMANDS.md               # Real-time commands guide
│
├── 📖 reference/                      # Technical documentation
│   ├── API.md                        # REST API specification
│   ├── RATE_LIMITING.md              # Rate limiting details
│   ├── STORAGE.md                    # Storage architecture
│   └── TSTORAGE.md                   # Time-series database
│
└── ⚡ performance/                    # Benchmarks and test results
    ├── BUNTDB_TEST_RESULTS.md        # Metadata storage tests
    ├── FINAL_PERFORMANCE_REPORT.md   # Overall performance analysis
    └── TSTORAGE_TEST_RESULTS.md      # Time-series storage tests
```

## 🎯 Quick Navigation

### For New Users
1. **[Quick Start](../QUICKSTART.md)** → Get running in 5 minutes
2. **[Registration Guide](docs/guides/REGISTRATION.md)** → Setup your system
3. **[CLI Tutorial](docs/tutorials/CLI.md)** → Learn the tools

### For Developers
- **[API Reference](docs/reference/API.md)** → REST API endpoints
- **[Storage Architecture](docs/reference/STORAGE.md)** → How data is stored
- **[Firmware Guide](docs/tutorials/FIRMWARE.md)** → Build device firmware

### For System Admins
- **[Security Guide](docs/guides/SECURITY.md)** → Secure your deployment
- **[Password Reset](docs/guides/PASSWORD_RESET.md)** → Recover access
- **[Data Retention](docs/guides/RETENTION.md)** → Manage data lifecycle

### For Performance Analysis
- **[Performance Report](docs/performance/FINAL_PERFORMANCE_REPORT.md)** → Full analysis
- **[Storage Benchmarks](docs/performance/)** → Detailed test results

## 📝 Documentation Types

### Guides (How-to)
**Purpose**: Solve specific problems
**Format**: Step-by-step instructions
**Example**: "How to reset a forgotten password"

### Tutorials (Learning)
**Purpose**: Learn by doing
**Format**: Complete walkthroughs
**Example**: "Building your first IoT device"

### Reference (Lookup)
**Purpose**: Technical specifications
**Format**: API docs, schemas, configs
**Example**: "REST API endpoint reference"

### Performance (Analysis)
**Purpose**: System capabilities
**Format**: Benchmarks, test results
**Example**: "Database performance under load"

## 🔗 All Documentation Links Updated

✅ Internal cross-references maintained
✅ Relative paths corrected
✅ README.md index created
✅ Category folders organized

## 📖 How to Use

1. **Start here**: [docs/README.md](docs/README.md)
2. **Find your topic**: Browse by category
3. **Follow links**: All cross-references work
4. **Quick access**: Use the navigation sections

## 🛠️ Maintenance

When adding new documentation:
- **Guides**: Problem-solving, troubleshooting, configuration
- **Tutorials**: Complete learning paths, examples
- **Reference**: API docs, technical specs, architecture
- **Performance**: Benchmarks, load tests, optimization reports

Update [docs/README.md](docs/README.md) index when adding new files.
