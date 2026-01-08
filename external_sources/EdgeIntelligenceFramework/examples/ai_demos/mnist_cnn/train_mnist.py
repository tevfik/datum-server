import tensorflow as tf
import numpy as np
import os

# 1. Train Model
mnist = tf.keras.datasets.mnist
(x_train, y_train), (x_test, y_test) = mnist.load_data()
x_train, x_test = x_train / 255.0, x_test / 255.0
x_train = x_train[..., tf.newaxis].astype("float32")
x_test = x_test[..., tf.newaxis].astype("float32")

model = tf.keras.models.Sequential([
  tf.keras.layers.Conv2D(4, 3, activation='relu', input_shape=(28, 28, 1)),
  tf.keras.layers.MaxPooling2D(),
  tf.keras.layers.Conv2D(8, 3, activation='relu'),
  tf.keras.layers.MaxPooling2D(),
  tf.keras.layers.Flatten(),
  tf.keras.layers.Dense(10)
])

model.compile(optimizer='adam',
              loss=tf.keras.losses.SparseCategoricalCrossentropy(from_logits=True),
              metrics=['accuracy'])

model.fit(x_train, y_train, epochs=1)

# 2. Convert to TFLite
converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()

with open('mnist.tflite', 'wb') as f:
    f.write(tflite_model)

print("Model saved to mnist.tflite")

# 3. Save a test image
test_img = x_test[0]
test_img.tofile("test_img.bin")
print("Test image saved to test_img.bin (Label: {})".format(y_test[0]))
