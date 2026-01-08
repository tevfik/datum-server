# AI Features Tutorial: ML, CV, and NLP on Edge

This guide walks you through the new AI capabilities added to the Edge Intelligence Framework.

## 1. Machine Learning: K-Means Clustering
**Goal**: Group raw sensor data into clusters without supervision.
- **Doc**: [docs/ML_KMEANS.md](ML_KMEANS.md)
- **Demo**: `examples/ml_demos/kmeans_clustering`

**Run Demo:**
```bash
cd build
./bin/kmeans_demo
```
**Output**: Shows centroids converging and predictions for test points.

## 2. Computer Vision: Haar Features
**Goal**: Efficiently compute image features for object detection.
- **Doc**: [docs/CV_HAAR.md](CV_HAAR.md)
- **Demo**: `examples/cv_demos/haar_cascade`

**Run Demo:**
```bash
cd build
./bin/haar_demo
```
**Output**: Synthesizes a face image and verifies feature sums using Integral Image.

## 3. NLP: Word Embeddings
**Goal**: Understand word meaning and relationships.
- **Doc**: [docs/NLP_EMBEDDINGS.md](NLP_EMBEDDINGS.md)
- **Demo**: `examples/nlp_demos/word_embedding`

**Run Demo:**
```bash
cd build
./bin/embedding_demo
```
**Output**: Calculates similarity between "King", "Queen", "Apple", and solves analogies.

## Integration Guide
All modules are built as static libraries (`libeif_ml.a`, `libeif_cv.a`, `libeif_nlp.a`).
To use them in your own application:
1. Include the relevant header (`eif_ml_kmeans.h`, etc.).
2. Link against the library.
3. Ensure you have `malloc` available (or use the framework's memory manager if integrated).
