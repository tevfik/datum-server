# Matrix Profile Demo

Time series analysis using the Matrix Profile algorithm.

## What is Matrix Profile?

Matrix Profile is a data structure that stores the z-normalized Euclidean distance between each subsequence and its nearest neighbor. It enables:

- **Motif Discovery**: Find repeating patterns
- **Discord Detection**: Find anomalies
- **AB-Join**: Compare two time series
- **Streaming Analysis**: Real-time monitoring

## Features

| Feature | Description |
|---------|-------------|
| MASS Algorithm | FFT-accelerated distance computation |
| Self-Join | Find patterns within same series |
| AB-Join | Compare two different series |
| Motif Discovery | Top-k repeating patterns |
| Discord Detection | Top-k anomalies |
| Streaming | Incremental updates |

## API

```c
#include "eif_matrix_profile.h"

// Compute matrix profile
eif_matrix_profile_t mp;
eif_mp_compute(ts, length, window_size, &mp, &pool);

// Find motifs (repeating patterns)
int motifs[3];
eif_mp_find_motifs(&mp, 3, motifs, NULL);

// Find discords (anomalies)
int discords[3];
eif_mp_find_discords(&mp, 3, discords, NULL);

// AB-Join (compare two series)
eif_mp_compute_ab(ts_a, len_a, ts_b, len_b, window, &mp, &pool);

// Streaming
eif_mp_stream_t stream;
eif_mp_stream_init(&stream, buffer_size, window_size, &pool);
eif_mp_stream_update(&stream, new_value, &pool);
```

## Parameters

| Parameter | Description | Typical Values |
|-----------|-------------|----------------|
| `window_size` | Subsequence length | 10-100 |
| `exclusion_zone` | Auto: window_size/4 | Prevents trivial matches |

## Build & Run

```bash
cd build
make matrix_profile_demo
./bin/matrix_profile_demo
```

## Limitations

- Direct dot product (O(n*m)) used for reliability
- Memory scales with O(n) for profile storage
- Streaming recomputes periodically (not true O(1) STUMPI)

## Reference

[Matrix Profile Homepage](https://www.cs.ucr.edu/~eamonn/MatrixProfile.html)
