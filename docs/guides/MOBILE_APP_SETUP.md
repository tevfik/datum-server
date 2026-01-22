# Mobile App Setup Guide

This guide describes how to set up the development environment for the Datum Camera App (`examples/datum_camera_app`).

## Prerequisites

- **Flutter SDK**: Version 3.27.x or later.
- **Dart SDK**: Included with Flutter.
- **Android Studio**: For Android development (optional if using VS Code, but SDK tools are required).
- **Xcode**: For iOS development (macOS only).
- **Linux Build Tools**: For Linux development.

## Setup Instructions

### 1. Clone the Repository

```bash
git clone https://github.com/yourusername/datum-server.git
cd datum-server/examples/datum_camera_app
```

### 2. Install Dependencies

```bash
flutter pub get
```

### 3. Linux Development Setup

To build or run the app on Linux, you must install the following system dependencies.

**Ubuntu/Debian:**

```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build clang pkg-config libgtk-3-dev liblzma-dev libstdc++-12-dev libsecret-1-dev libjsoncpp-dev libglu1-mesa-dev
```

**Fedora:**

```bash
sudo dnf install cmake ninja-build clang pkg-config gtk3-devel xz-devel libstdc++-devel libsecret-devel jsoncpp-devel mesa-libGLU-devel
```

**Note:** The `libsecret-1-dev` package is critical for `flutter_secure_storage` to work on Linux.

### 4. Running the App

**Android:**
Connect an Android device or start an emulator.
```bash
flutter run -d android
```

**Linux:**
```bash
flutter run -d linux
```

### 5. Building for Release

**Android APK:**
```bash
flutter build apk --release
```

**Linux Bundle:**
```bash
flutter build linux --release
```

## Troubleshooting

### "CMake Error: libsecret-1 not found"
If you encounter this error during the Linux build:
```
CMake Error ... The following required packages were not found: - libsecret-1>=0.18.4
```
It means you are missing the `libsecret` development library. Run the installation command mentioned in the **Linux Development Setup** section above.

### "Keyring access failed" on Linux
If the app crashes on startup on Linux with keyring errors, ensure you have a keyring daemon running (like `gnome-keyring` or `kwallet`) and it is unlocked.
