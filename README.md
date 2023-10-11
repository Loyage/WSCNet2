# WSCNet2

## Introduction

This code is an upgraded version of [WSCNet: Biomedical image recognition for cell encapsulated microfluidic droplets](https://github.com/Loyage/WSCNet), which is a set of biomedical image recognition algorithm designed for cell encapsulation microfluidic droplet. Compared with the first version, the addition of the user interface has improved the usability of the software, in addition, the algorithm has also been improved to some extent, and the recognition accuracy has been further improved.

This project is developed in C++. Third-party libraries used include Qt5, opencv-4.8.0 and libtorch-2.1.0 with cuda v11.7.

The interface of our software:

![image-20231011190417126](.\images\gui.png)

## Install

We recommend running the software directly by downloading from the [release](https://github.com/Loyage/WSCNet2/releases). Note that additional download of the software [packages](https://github.com/Loyage/WSCNet2/releases/tag/Packages) is required. The software is large, please wait patiently when dowloading. **At present, the software only supports the x64 Windows operating system.**

## Usage

The software supports three types of identification objects: image, video, folder.

1. **Folder**, select folder and click 'Recognize' button, all the images in this folder will be processed, the results will be saved in `imgResult` and `textResult` subfolder.
2. **Image**, based on selected folder, select one image under the folder to be recognized.
3. **Video**, the video will be cut into frames and recoginized. At last, they will be combined to a processed video.

We have provided example images and video to test the software effects: [examples](https://github.com/Loyage/WSCNet2/tree/master/examples)

You can get better results by adjusting the parameters. If you want to classify the droplets to empty, single cell or multiply cells, please select a model to do it. The model we provide in examples are trained on several specific datasets, if your dataset has distinct features with ours then you need to train your own model. Steps to train a new model refer to project : [WSCNet_train](https://github.com/Loyage/WSCNet_train)