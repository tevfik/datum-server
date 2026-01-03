FROM ubuntu:22.04

# Prerequisites
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    git \
    unzip \
    xz-utils \
    zip \
    libglu1-mesa \
    openjdk-17-jdk \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Environment Variables
ENV JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
ENV ANDROID_HOME=/opt/android-sdk
ENV PATH=${PATH}:${ANDROID_HOME}/cmdline-tools/latest/bin:${ANDROID_HOME}/platform-tools

# 1. Install Android SDK
RUN mkdir -p ${ANDROID_HOME}/cmdline-tools \
    && curl -L https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip -o /tmp/cmdline-tools.zip \
    && unzip /tmp/cmdline-tools.zip -d ${ANDROID_HOME}/cmdline-tools \
    && mv ${ANDROID_HOME}/cmdline-tools/cmdline-tools ${ANDROID_HOME}/cmdline-tools/latest \
    && rm /tmp/cmdline-tools.zip

# Accept Licenses & Install Components
RUN yes | sdkmanager --licenses \
    && sdkmanager "platforms;android-34" "build-tools;34.0.0" "platform-tools"

# 2. Install Flutter SDK
ENV FLUTTER_HOME=/opt/flutter
ENV PATH=${PATH}:${FLUTTER_HOME}/bin

# Clone Flutter (Stable 3.27.x)
# Note: Cloning specific version or stable branch
RUN git clone --depth 1 --branch 3.27.0 https://github.com/flutter/flutter.git ${FLUTTER_HOME}

# Config & Precache
RUN git config --global --add safe.directory ${FLUTTER_HOME} \
    && flutter config --no-analytics \
    && flutter config --android-sdk ${ANDROID_HOME} \
    && flutter precache

# Verify
RUN flutter doctor -v
