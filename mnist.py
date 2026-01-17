# initialize a python environment, or conda or whatever floats your boat.

# use that to install tensorflow, tensorflow_datasets and numpy

# use the same environment then to compile and run the program and then you won't need it anymore

# the files with ".mat" will be listed in your directory and they will all be binaries

import numpy as np
import tensorflow_datasets as tfds


def dsTonp(ds):
    images = []
    labels = []

    for image, label in ds:
        images.append(image.numpy())
        labels.append(label.numpy())

    return np.array(images), np.array(labels)


train_ds, test_ds = tfds.load("mnist", split=["train", "test"], as_supervised=True)

train_images, train_labels = dsTonp(train_ds)
test_images, test_labels = dsTonp(test_ds)

train_images = train_images.astype(np.float32) / 255.0
test_images = test_images.astype(np.float32) / 255.0

train_labels = train_labels.astype(np.float32)
test_labels = test_labels.astype(np.float32)

train_images.tofile("train_images.mat")
train_labels.tofile("train_labels.mat")
test_images.tofile("test_images.mat")
test_labels.tofile("test_labels.mat")

print("shape of trained image", train_images.shape)
print("shape of trained label", train_labels.shape)
print("shape of test image", test_images.shape)
print("shape of test image", test_labels.shape)

# output:
# shape of trained image (60000, 28, 28, 1)
# shape of trained label (60000,)
# shape of test image (10000, 28, 28, 1)
# shape of test image (10000,)
