"""
MNIST Keras Demo - Train and Convert to EIF

This script:
1. Creates a simple CNN for MNIST
2. Trains it on MNIST dataset
3. Converts it to EIF C code

Usage:
    python train_mnist.py
    python -m eif_convert mnist_model.h5 -o output/
"""

import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'  # Suppress TF warnings

import numpy as np

try:
    import tensorflow as tf
    from tensorflow import keras
    from tensorflow.keras import layers
except ImportError:
    print("TensorFlow required. Install with: pip install tensorflow")
    exit(1)

# Try to import eif_convert
import sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../..'))
try:
    import tools.eif_convert as eif
    HAS_EIF = True
except ImportError:
    HAS_EIF = False


def create_model():
    """Create a simple CNN for MNIST."""
    model = keras.Sequential([
        layers.Input(shape=(28, 28, 1)),
        
        # Conv block 1
        layers.Conv2D(16, 3, padding='same'),
        layers.BatchNormalization(),
        layers.ReLU(),
        layers.MaxPooling2D(2),
        
        # Conv block 2
        layers.Conv2D(32, 3, padding='same'),
        layers.BatchNormalization(),
        layers.ReLU(),
        layers.MaxPooling2D(2),
        
        # Classifier
        layers.Flatten(),
        layers.Dense(64),
        layers.ReLU(),
        layers.Dropout(0.5),
        layers.Dense(10, activation='softmax')
    ])
    
    model.compile(
        optimizer='adam',
        loss='sparse_categorical_crossentropy',
        metrics=['accuracy']
    )
    
    return model


def load_mnist():
    """Load and preprocess MNIST dataset."""
    (x_train, y_train), (x_test, y_test) = keras.datasets.mnist.load_data()
    
    # Normalize to [0, 1] and add channel dimension
    x_train = x_train.astype('float32') / 255.0
    x_test = x_test.astype('float32') / 255.0
    x_train = np.expand_dims(x_train, -1)
    x_test = np.expand_dims(x_test, -1)
    
    return (x_train, y_train), (x_test, y_test)


def train_model(model, x_train, y_train, x_test, y_test, epochs=5):
    """Train the model."""
    print("\n" + "="*60)
    print("Training MNIST CNN")
    print("="*60)
    
    model.fit(
        x_train, y_train,
        batch_size=128,
        epochs=epochs,
        validation_split=0.1,
        verbose=1
    )
    
    # Evaluate
    print("\n" + "="*60)
    print("Evaluation")
    print("="*60)
    loss, acc = model.evaluate(x_test, y_test, verbose=0)
    print(f"Test accuracy: {acc*100:.2f}%")
    
    return model


def save_model(model, filepath='mnist_model.h5'):
    """Save the trained model."""
    model.save(filepath)
    print(f"\nModel saved to: {filepath}")
    return filepath


def convert_to_eif(model_path, output_dir='output'):
    """Convert model to EIF C code."""
    if not HAS_EIF:
        print("\neif_convert not available. Run manually:")
        print(f"  python -m eif_convert {model_path} -o {output_dir}/")
        return
    
    print("\n" + "="*60)
    print("Converting to EIF")
    print("="*60)
    
    files = eif.convert(
        model=model_path,
        output=output_dir,
        quantize='q15',
        per_channel=True,
        model_name='mnist',
        verbose=True
    )
    
    print(f"\n✅ Generated {len(files)} files in {output_dir}/")


def main():
    print("="*60)
    print("MNIST EIF Demo")
    print("="*60)
    
    # Load data
    print("\nLoading MNIST dataset...")
    (x_train, y_train), (x_test, y_test) = load_mnist()
    print(f"Train: {x_train.shape}, Test: {x_test.shape}")
    
    # Create model
    print("\nCreating model...")
    model = create_model()
    model.summary()
    
    # Train
    model = train_model(model, x_train, y_train, x_test, y_test, epochs=3)
    
    # Save
    model_path = save_model(model, 'mnist_model.h5')
    
    # Convert
    convert_to_eif(model_path, 'output')
    
    print("\n" + "="*60)
    print("Next steps:")
    print("="*60)
    print("1. Copy output/*.h to your embedded project")
    print("2. Include the headers and call mnist_inference()")
    print("3. See run_inference.c for example usage")


if __name__ == '__main__':
    main()
