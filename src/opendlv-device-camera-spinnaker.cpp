/*
 * Copyright (C) 2019  Christian Berger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cluon-complete.hpp"

#include <Spinnaker.h>

#include <sys/time.h>
#include <libyuv.h>
#include <X11/Xlib.h>

#include <cstdlib>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <memory>

int32_t main(int32_t argc, char **argv) {
    int32_t retCode{0};
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if ( (0 == commandlineArguments.count("camera")) ||
         (0 == commandlineArguments.count("width")) ||
         (0 == commandlineArguments.count("height")) ) {
        std::cerr << argv[0] << " interfaces with a Pylon camera (given by the numerical identifier, e.g., 0) and provides the captured image in two shared memory areas: one in I420 format and one in ARGB format." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --camera=<identifier> --width=<width> --height=<height> [--name.i420=<unique name for the shared memory in I420 format>] [--name.argb=<unique name for the shared memory in ARGB format>] --width=W --height=H [--offsetX=X] [--offsetY=Y] [--packetsize=1500] [--fps=17] [--verbose]" << std::endl;
        std::cerr << "         --camera:     numerical identifier for pylon-compatible camera to be used" << std::endl;
        std::cerr << "         --name.i420:  name of the shared memory for the I420 formatted image; when omitted, 'video0.i420' is chosen" << std::endl;
        std::cerr << "         --name.argb:  name of the shared memory for the I420 formatted image; when omitted, 'video0.argb' is chosen" << std::endl;
        std::cerr << "         --width:      desired width of a frame" << std::endl;
        std::cerr << "         --height:     desired height of a frame" << std::endl;
        std::cerr << "         --offsetX:    X for desired ROI (default: 0)" << std::endl;
        std::cerr << "         --offsetY:    Y for desired ROI (default: 0)" << std::endl;
        std::cerr << "         --fps:        desired acquisition frame rate (depends on bandwidth)" << std::endl;
        std::cerr << "         --verbose:    display captured image" << std::endl;
        std::cerr << "Example: " << argv[0] << " --camera=0 --width=640 --height=480 --verbose" << std::endl;
        retCode = 1;
    }
    else {
        const uint32_t CAMERA{static_cast<uint32_t>((commandlineArguments.count("camera") != 0) ?std::stoi(commandlineArguments["camera"]) : 0)};
        const uint32_t WIDTH{static_cast<uint32_t>(std::stoi(commandlineArguments["width"]))};
        const uint32_t HEIGHT{static_cast<uint32_t>(std::stoi(commandlineArguments["height"]))};
        const uint32_t OFFSET_X{static_cast<uint32_t>((commandlineArguments.count("offsetX") != 0) ?std::stoi(commandlineArguments["offsetX"]) : 0)};
        const uint32_t OFFSET_Y{static_cast<uint32_t>((commandlineArguments.count("offsetY") != 0) ?std::stoi(commandlineArguments["offsetY"]) : 0)};
        const float FPS{static_cast<float>((commandlineArguments.count("fps") != 0) ?std::stof(commandlineArguments["fps"]) : 17)};
        const bool VERBOSE{commandlineArguments.count("verbose") != 0};

        // Set up the names for the shared memory areas.
        std::string NAME_I420{"video0.i420"};
        if ((commandlineArguments["name.i420"].size() != 0)) {
            NAME_I420 = commandlineArguments["name.i420"];
        }
        std::string NAME_ARGB{"video0.argb"};
        if ((commandlineArguments["name.argb"].size() != 0)) {
            NAME_ARGB = commandlineArguments["name.argb"];
        }

        std::unique_ptr<cluon::SharedMemory> sharedMemoryI420(new cluon::SharedMemory{NAME_I420, WIDTH * HEIGHT * 3/2});
        if (!sharedMemoryI420 || !sharedMemoryI420->valid()) {
            std::cerr << "[opendlv-device-camera-spinnaker]: Failed to create shared memory '" << NAME_I420 << "'." << std::endl;
            return retCode = 1;
        }

        std::unique_ptr<cluon::SharedMemory> sharedMemoryARGB(new cluon::SharedMemory{NAME_ARGB, WIDTH * HEIGHT * 4});
        if (!sharedMemoryARGB || !sharedMemoryARGB->valid()) {
            std::cerr << "[opendlv-device-camera-spinnaker]: Failed to create shared memory '" << NAME_ARGB << "'." << std::endl;
            return retCode = 1;
        }

        if ( (sharedMemoryI420 && sharedMemoryI420->valid()) &&
             (sharedMemoryARGB && sharedMemoryARGB->valid()) ) {
            std::clog << "[opendlv-device-camera-spinnaker]: Data from camera '" << commandlineArguments["camera"]<< "' available in I420 format in shared memory '" << sharedMemoryI420->name() << "' (" << sharedMemoryI420->size() << ") and in ARGB format in shared memory '" << sharedMemoryARGB->name() << "' (" << sharedMemoryARGB->size() << ")." << std::endl;

            // TODO: Acquire any resources.
            Spinnaker::SystemPtr system = Spinnaker::System::GetInstance();

            // Accessing the low-level X11 data display.
            Display* display{nullptr};
            Visual* visual{nullptr};
            Window window{0};
            XImage* ximage{nullptr};
            if (VERBOSE) {
                display = XOpenDisplay(NULL);
                visual = DefaultVisual(display, 0);
                window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, WIDTH, HEIGHT, 1, 0, 0);
                sharedMemoryARGB->lock();
                {
                    ximage = XCreateImage(display, visual, 24, ZPixmap, 0, sharedMemoryARGB->data(), WIDTH, HEIGHT, 32, 0);
                }
                sharedMemoryARGB->unlock();
                XMapWindow(display, window);
            }

            std::vector<uint8_t> imageBuffer;

            // Frame grabbing loop.
            while (!cluon::TerminateHandler::instance().isTerminated.load()) {
                // TODO: Grab frame.

                cluon::data::TimeStamp ts{cluon::time::now()};

                sharedMemoryI420->lock();
                sharedMemoryI420->setTimeStamp(ts);
                {
                    libyuv::YUY2ToI420(imageBuffer.data(), WIDTH * 2 /* 2*WIDTH for YUYV 422*/,
                                       reinterpret_cast<uint8_t*>(sharedMemoryI420->data()), WIDTH,
                                       reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT)), WIDTH/2,
                                       reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT + ((WIDTH * HEIGHT) >> 2))), WIDTH/2,
                                       WIDTH, HEIGHT);
                }
                sharedMemoryI420->unlock();

                sharedMemoryARGB->lock();
                sharedMemoryARGB->setTimeStamp(ts);
                {
                    libyuv::I420ToARGB(reinterpret_cast<uint8_t*>(sharedMemoryI420->data()), WIDTH,
                                       reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT)), WIDTH/2,
                                       reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT + ((WIDTH * HEIGHT) >> 2))), WIDTH/2,
                                       reinterpret_cast<uint8_t*>(sharedMemoryARGB->data()), WIDTH * 4, WIDTH, HEIGHT);

                    if (VERBOSE) {
                        XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 0, 0, WIDTH, HEIGHT);
                    }
                }
                sharedMemoryARGB->unlock();

                // Wake up any pending processes.
                sharedMemoryI420->notifyAll();
                sharedMemoryARGB->notifyAll();
            }

            // Release any resources.
            system->ReleaseInstance();
        }
        retCode = 0;
    }
    return retCode;
}

