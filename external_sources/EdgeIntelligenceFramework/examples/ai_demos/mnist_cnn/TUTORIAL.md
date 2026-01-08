# MNIST CNN Tutorial: Image Classification on MCU

## Learning Objectives

- Convolutional Neural Network basics
- LeNet-5 style architecture
- INT8 quantization for embedded
- Running CNN inference on ESP32

**Level**: Beginner to Intermediate  
**Time**: 40 minutes

---

## 1. CNN Architecture

### LeNet-5 for MNIST

```
Input: 28×28×1 grayscale image

Conv2D(1→6, 5×5, stride=1)     → 24×24×6
MaxPool(2×2)                    → 12×12×6
Conv2D(6→16, 5×5, stride=1)    → 8×8×16
MaxPool(2×2)                    → 4×4×16
Flatten                         → 256
Dense(256→120)                  → 120
Dense(120→84)                   → 84
Dense(84→10)                    → 10 (digits 0-9)
```

---

## 2. Convolution Operation

```
Input Patch:         Kernel:          Output:
│ 1  2  3 │         │-1  0  1│
│ 4  5  6 │    *    │-2  0  2│   =   -4
│ 7  8  9 │         │-1  0  1│
```

```c
// EIF Conv2D layer
eif_nn_layer_t conv = eif_nn_layer_conv2d(
    1,      // in_channels
    6,      // out_channels
    5,      // kernel_size
    1,      // stride
    0       // padding
);
eif_nn_model_add_layer(&model, &conv);
```

---

## 3. Full Model Setup

```c
eif_nn_model_t model;
eif_nn_model_init(&model, &pool);

// Conv1 + ReLU + Pool
eif_nn_model_add_layer(&model, &(eif_nn_layer_t){
    .type = EIF_LAYER_CONV2D, .in_ch = 1, .out_ch = 6, .k = 5});
eif_nn_model_add_layer(&model, &(eif_nn_layer_t){
    .type = EIF_LAYER_RELU});
eif_nn_model_add_layer(&model, &(eif_nn_layer_t){
    .type = EIF_LAYER_MAXPOOL, .k = 2, .stride = 2});

// Conv2 + ReLU + Pool
eif_nn_model_add_layer(&model, &(eif_nn_layer_t){
    .type = EIF_LAYER_CONV2D, .in_ch = 6, .out_ch = 16, .k = 5});
eif_nn_model_add_layer(&model, &(eif_nn_layer_t){
    .type = EIF_LAYER_RELU});
eif_nn_model_add_layer(&model, &(eif_nn_layer_t){
    .type = EIF_LAYER_MAXPOOL, .k = 2, .stride = 2});

// Dense layers
eif_nn_model_add_layer(&model, &(eif_nn_layer_t){
    .type = EIF_LAYER_FLATTEN});
eif_nn_model_add_layer(&model, &(eif_nn_layer_t){
    .type = EIF_LAYER_DENSE, .in_features = 256, .out_features = 10});
eif_nn_model_add_layer(&model, &(eif_nn_layer_t){
    .type = EIF_LAYER_SOFTMAX});
```

---

## 4. Inference

```c
// Input: 28×28 grayscale image
float input[28 * 28];
float output[10];

// Normalize to 0-1
for (int i = 0; i < 28*28; i++) {
    input[i] = image[i] / 255.0f;
}

// Run inference
eif_nn_model_invoke(&model, input, output);

// Get prediction
int digit = argmax(output, 10);
printf("Predicted: %d (confidence: %.1f%%)\n", 
       digit, output[digit] * 100);
```

---

## 5. Quantization (INT8)

### Benefits

| Precision | Model Size | Speed |
|-----------|------------|-------|
| Float32 | 100% | 1× |
| INT8 | 25% | 2-4× |

### Conversion

```bash
python tools/compression/model_compress.py model.onnx --quantize
```

---

## 6. ESP32-CAM Digit Recognition

```c
void digit_task(void* arg) {
    while (1) {
        // Capture image
        camera_fb_t* fb = esp_camera_fb_get();
        
        // Preprocess: resize to 28×28, grayscale
        float input[28*28];
        preprocess_for_mnist(fb, input);
        
        // Inference
        float output[10];
        eif_nn_model_invoke(&model, input, output);
        
        int digit = argmax(output, 10);
        ESP_LOGI("MNIST", "Detected: %d", digit);
        
        esp_camera_fb_return(fb);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
```

---

## 7. Summary

### Model Size
- Float32: ~60 KB
- INT8: ~15 KB

### Performance
- ESP32 @ 240MHz: ~20ms inference
- Accuracy: ~98% on MNIST test set
