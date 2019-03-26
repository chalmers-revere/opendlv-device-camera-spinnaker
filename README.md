## OpenDLV Microservice to interface with Spinnaker-supported cameras (e.g., PtGrey GiGE cameras)

This repository provides source code to interface Spinnaker-supported cameras
for the OpenDLV software ecosystem. This microservice provides the captured frames
in two separate shared memory areas, one for a picture in [I420 format](https://wiki.videolan.org/YUV/#I420)
and one in ARGB format.

[![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)


## Table of Contents
* [Dependencies](#dependencies)
* [Usage](#usage)
* [Build from sources on the example of Ubuntu 16.04 LTS](#build-from-sources-on-the-example-of-ubuntu-1604-lts)
* [License](#license)


## Dependencies
You need a C++14-compliant compiler to compile this project. The following
dependency is shipped as part of the source distribution:

* [libcluon](https://github.com/chrberger/libcluon) - [![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)
* [libyuv](https://chromium.googlesource.com/libyuv/libyuv/+/master) - [![License: BSD 3-Clause](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)](https://opensource.org/licenses/BSD-3-Clause) - [Google Patent License Conditions](https://chromium.googlesource.com/libyuv/libyuv/+/master/PATENTS)
* [Spinnaker](https://eu.ptgrey.com/support/downloads)


## Building and Usage
Currently, we do not provide and distribute pre-built Docker images but we provide
the build instructions in `Dockerfile.amd64` for x86_64 platforms that can be
easily integrated in a `docker-compose.yml` file.

To run this microservice using `docker-compose`, you can simply add the following
section to your `docker-compose.yml` file to let Docker build this software for you:

```yml
version: '2' # Must be present exactly once at the beginning of the docker-compose.yml file
services:    # Must be present exactly once at the beginning of the docker-compose.yml file
    device-camera-spinnaker-amd64:
        build:
            context: https://github.com/chalmers-revere/opendlv-device-camera-spinnaker.git
            dockerfile: Dockerfile.amd64
        restart: on-failure
        network_mode: "host"
        ipc: "host"
        volumes:
        - /tmp:/tmp
        command: "--camera=0 --width=640 --height=480 --name.i420=ptg0.i420 --name.argb=ptg0.argb --verbose"
```

The parameters to the application are:

* `--camera=ID`: Serial number for Spinnaker-compatible camera to be used
* `--name.i420=XYZ`: Name of the shared memory for the I420 formatted image; when omitted, `cam0.i420` is chosen
* `--name.argb=XYZ`: Name of the shared memory for the ARGB formatted image; when omitted, `cam0.argb` is chosen
* `--width=W`: Desired width of a frame
* `--height=H`: Desired height of a frame
* `--offsetX`: X for desired ROI (default: 0)
* `--offsetY`: Y for desired ROI (default: 0)
* `--verbose:`: Display captured image


## License

* This project is released under the terms of the GNU GPLv3 License

