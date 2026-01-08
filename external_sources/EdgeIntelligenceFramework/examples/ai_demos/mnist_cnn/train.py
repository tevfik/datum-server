import tensorflow as tf
import numpy as np

# 1. Load Data
mnist = tf.keras.datasets.mnist
(x_train, y_train), (x_test, y_test) = mnist.load_data()
x_train, x_test = x_train / 255.0, x_test / 255.0

# Add channel dimension
x_train = x_train[..., tf.newaxis].astype("float32")
x_test = x_test[..., tf.newaxis].astype("float32")

# 2. Define Model
model = tf.keras.models.Sequential([
    tf.keras.layers.Conv2D(4, (3, 3), activation='relu', input_shape=(28, 28, 1)),
    tf.keras.layers.MaxPooling2D((2, 2)),
    tf.keras.layers.Flatten(),
    tf.keras.layers.Dense(10, activation='softmax')
])

# 3. Train
model.compile(optimizer='adam',
              loss='sparse_categorical_crossentropy',
              metrics=['accuracy'])

model.fit(x_train, y_train, epochs=1)

# 4. Export to TFLite
converter = tf.lite.TFLiteConverter.from_keras_model(model)
tflite_model = converter.convert()

with open('mnist.tflite', 'wb') as f:
    f.write(tflite_model)

print("Model saved to mnist.tflite")

# Save a test sample
sample_img = x_test[0]
sample_label = y_test[0]
np.save("sample_img.npy", sample_img)
print(f"Saved sample image with label: {sample_label}")
