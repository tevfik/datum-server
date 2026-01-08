# Few-Shot Learning Tutorial: Learning from Limited Examples

## Learning Objectives

- Prototypical networks for few-shot classification
- N-way K-shot learning setup
- Distance-based classification
- Gesture recognition with 5 examples

**Level**: Intermediate  
**Time**: 30 minutes

---

## 1. The Few-Shot Problem

**Traditional ML**: Train on 1000s of examples  
**Few-Shot**: Train on 5 examples per class!

### N-way K-shot

```
5-way 1-shot: 5 classes, 1 example each
5-way 5-shot: 5 classes, 5 examples each
```

---

## 2. Prototypical Networks

### Key Idea

Each class has a **prototype** = mean of its examples.

```
Class A examples: [▲] [△] [▵]  → Prototype: [▲]
Class B examples: [●] [○]      → Prototype: [●]

New sample [◆] → Closer to [▲] → Class A!
```

### Algorithm

```c
// 1. Compute prototypes
for each class c:
    prototype[c] = mean(embeddings of class c examples)

// 2. Classify new sample
distances = [dist(query, prototype[c]) for c in classes]
prediction = argmin(distances)
```

---

## 3. EIF Implementation

```c
eif_fewshot_t fs;
eif_fewshot_init(&fs, embed_dim, n_classes, &pool);

// Register support examples
for (int c = 0; c < n_classes; c++) {
    for (int k = 0; k < k_shot; k++) {
        float* example = get_example(c, k);
        eif_fewshot_add_support(&fs, c, example);
    }
}

// Compute prototypes
eif_fewshot_compute_prototypes(&fs);

// Classify query
float query[EMBED_DIM];
get_embedding(input, query);
int pred = eif_fewshot_classify(&fs, query);
```

---

## 4. ESP32 Example: Quick Gesture Learning

```c
// User trains new gestures with 5 examples each
void learn_gesture(int class_id) {
    for (int i = 0; i < 5; i++) {
        printf("Show gesture %d, example %d...\n", class_id, i);
        
        // Collect IMU data
        float features[EMBED_DIM];
        collect_gesture_features(features);
        
        eif_fewshot_add_support(&fs, class_id, features);
    }
}

// After learning all classes
eif_fewshot_compute_prototypes(&fs);

// Now classify in real-time
while (1) {
    float query[EMBED_DIM];
    collect_gesture_features(query);
    int gesture = eif_fewshot_classify(&fs, query);
    printf("Detected gesture: %d\n", gesture);
}
```

---

## Summary

### Key Concepts
- Prototype = class center
- Euclidean distance for matching
- No retraining needed for new classes

### Memory: O(n_classes × embed_dim)
