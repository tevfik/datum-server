import tensorflow as tf
import numpy as np
import os

def create_mobilenet_block_model():
    # Input: 32x32 RGB image
    input_shape = (32, 32, 3)
    inputs = tf.keras.Input(shape=input_shape)

    # 1. Depthwise Conv2D (3x3, stride 1)
    x = tf.keras.layers.DepthwiseConv2D(kernel_size=(3, 3), strides=(1, 1), padding='same', use_bias=False)(inputs)
    x = tf.keras.layers.ReLU(max_value=6.0)(x) # ReLU6

    # 2. Pointwise Conv2D (1x1, filters=16)
    x = tf.keras.layers.Conv2D(filters=16, kernel_size=(1, 1), strides=(1, 1), padding='same', use_bias=False)(x)
    x = tf.keras.layers.ReLU(max_value=6.0)(x) # ReLU6

    # 3. Global Average Pooling
    x = tf.keras.layers.GlobalAveragePooling2D()(x)

    # 4. Dense (Output 10 classes)
    outputs = tf.keras.layers.Dense(10, activation='softmax')(x)

    model = tf.keras.Model(inputs=inputs, outputs=outputs)
    
    # Set dummy weights for deterministic output
    # (Optional, but helps debugging)
    
    return model

if __name__ == '__main__':
    model = create_mobilenet_block_model()
    model.summary()

    # Convert to TFLite
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    tflite_model = converter.convert()

    output_path = 'mobilenet_block.tflite'
    with open(output_path, 'wb') as f:
        f.write(tflite_model)
    
    print(f"Model saved to {output_path}")
