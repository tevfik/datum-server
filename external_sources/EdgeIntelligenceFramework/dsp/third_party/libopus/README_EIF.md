# Libopus Integration

This directory is a placeholder for the official **libopus** library.

## Setup Instructions
To enable Opus support, you must download the official libopus source code and place it here.

1.  Download libopus (e.g., v1.4 or v1.5) from [opus-codec.org](https://opus-codec.org/downloads/) or Clone from Git:
    ```bash
    git clone https://gitlab.xiph.org/xiph/opus.git .
    ```
2.  Ensure the directory structure looks like:
    ```
    dsp/third_party/libopus/
      include/
        opus.h
        opus_types.h
        ...
      src/
        opus_encoder.c
        ...
      celt/
      silk/
      CMakeLists.txt (The one provided by EIF or the original if compatible)
    ```

The EIF build system in `dsp/CMakeLists.txt` is configured to look for these files.
