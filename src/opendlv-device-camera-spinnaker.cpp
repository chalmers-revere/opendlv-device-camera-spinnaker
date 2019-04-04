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

#include <X11/Xlib.h>
#include <libyuv.h>
#include <sys/time.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
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
        std::cerr << "         --camera:     Identifier of Spinnaker-compatible camera to be used" << std::endl;
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
    } else {
        const uint32_t CAMERA{static_cast<uint32_t>(std::stoi(commandlineArguments["camera"]))};
        const uint32_t WIDTH{static_cast<uint32_t>(std::stoi(commandlineArguments["width"]))};
        const uint32_t HEIGHT{static_cast<uint32_t>(std::stoi(commandlineArguments["height"]))};
        const uint32_t OFFSET_X{static_cast<uint32_t>((commandlineArguments.count("offsetX") != 0) ? std::stoi(commandlineArguments["offsetX"]) : 0)};
        const uint32_t OFFSET_Y{static_cast<uint32_t>((commandlineArguments.count("offsetY") != 0) ? std::stoi(commandlineArguments["offsetY"]) : 0)};
        const float FPS{static_cast<float>((commandlineArguments.count("fps") != 0) ? std::stof(commandlineArguments["fps"]) : 17)};
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

        std::unique_ptr<cluon::SharedMemory> sharedMemoryI420(new cluon::SharedMemory{NAME_I420, WIDTH * HEIGHT * 3 / 2});
        if (!sharedMemoryI420 || !sharedMemoryI420->valid()) {
            std::cerr << "[opendlv-device-camera-spinnaker]: Failed to create shared memory '" << NAME_I420 << "'." << std::endl;
            return retCode = 1;
        }

        std::unique_ptr<cluon::SharedMemory> sharedMemoryARGB(new cluon::SharedMemory{NAME_ARGB, WIDTH * HEIGHT * 4});
        if (!sharedMemoryARGB || !sharedMemoryARGB->valid()) {
            std::cerr << "[opendlv-device-camera-spinnaker]: Failed to create shared memory '" << NAME_ARGB << "'." << std::endl;
            return retCode = 1;
        }

        if ((sharedMemoryI420 && sharedMemoryI420->valid()) && (sharedMemoryARGB && sharedMemoryARGB->valid())) {
            std::clog << "[opendlv-device-camera-spinnaker]: Data from camera '" << commandlineArguments["camera"] << "' available in I420 format in shared memory '" << sharedMemoryI420->name() << "' (" << sharedMemoryI420->size() << ") and in ARGB format in shared memory '" << sharedMemoryARGB->name() << "' (" << sharedMemoryARGB->size() << ")." << std::endl;

            // Open desired camera.
            Spinnaker::SystemPtr system{Spinnaker::System::GetInstance()};
            Spinnaker::CameraList listOfCameras{system->GetCameras()};
            Spinnaker::CameraPtr camera{listOfCameras.GetByIndex(CAMERA)};
            if (nullptr == camera) {
                std::cerr << "[opendlv-device-camera-spinnaker]: Failed to open camera '" << CAMERA << "'." << std::endl;
                return retCode = 1;
            }
            camera->Init();

            Spinnaker::GenApi::INodeMap &cameraNodeMap{camera->GetTLDeviceNodeMap()};
            {
                Spinnaker::GenApi::FeatureList_t features;
                Spinnaker::GenApi::CCategoryPtr category{cameraNodeMap.GetNode("DeviceInformation")};
                if (Spinnaker::GenApi::IsAvailable(category) && Spinnaker::GenApi::IsReadable(category)) {
                    category->GetFeatures(features);
                    for (auto it = features.begin(); it != features.end(); it++) {
                        Spinnaker::GenApi::CNodePtr featureNode{*it};
                        std::clog << "  " << featureNode->GetName() << ": ";
                        Spinnaker::GenApi::CValuePtr valuePtr = (Spinnaker::GenApi::CValuePtr)featureNode;
                        std::clog << (Spinnaker::GenApi::IsReadable(valuePtr) ? valuePtr->ToString() : "Node not readable");
                        std::clog << std::endl;
                    }
                } else {
                    std::cerr << "[opendlv-device-camera-spinnaker]: Could not read device control information." << std::endl;
                }
            }

            // Disable trigger mode.
            {
                if (Spinnaker::GenApi::RW != camera->TriggerMode.GetAccessMode()) {
                    std::cerr << "[opendlv-device-camera-spinnaker]: Could not disable trigger mode." << std::endl;
                    return retCode = 1;
                }
                camera->TriggerMode.SetValue(Spinnaker::TriggerModeEnums::TriggerMode_Off);
            }

            Spinnaker::GenApi::INodeMap &nodeMap              = camera->GetNodeMap();
            Spinnaker::GenApi::CEnumerationPtr ptrPixelFormat = nodeMap.GetNode("PixelFormat");
            if (IsAvailable(ptrPixelFormat) && IsWritable(ptrPixelFormat)) {
                // Retrieve the desired entry node from the enumeration node
                Spinnaker::GenApi::CEnumEntryPtr ptrPixelFormatYUV = ptrPixelFormat->GetEntryByName("YUV422Packed");
                if (IsAvailable(ptrPixelFormatYUV) && IsReadable(ptrPixelFormatYUV)) {
                    // Retrieve the integer value from the entry node:
                    int64_t pixelFormatYUV = ptrPixelFormatYUV->GetValue();

                    // Set integer as new value for enumeration node
                    ptrPixelFormat->SetIntValue(pixelFormatYUV);

                    std::clog << "[opendlv-device-camera-spinnaker]: Pixel format set to " << ptrPixelFormat->GetCurrentEntry()->GetSymbolic() << "." << std::endl;
                } else {
                    std::cerr << "[opendlv-device-camera-spinnaker]: Error: Pixel format YUV422Packed not available." << std::endl;
                }
            } else {
                std::cerr << "[opendlv-device-camera-spinnaker]: Error: Pixel format not available." << std::endl;
            }

            // Disable auto frame rate.
            try {
                Spinnaker::GenApi::CBooleanPtr acquisitionFrameRateEnable = nodeMap.GetNode("AcquisitionFrameRateEnable");
                if (IsAvailable(acquisitionFrameRateEnable) && IsReadable(acquisitionFrameRateEnable)) {
                    acquisitionFrameRateEnable->SetValue(1);
                    camera->AcquisitionFrameRate.SetValue(FPS);
                } else {
                    std::cerr << "[opendlv-device-camera-spinnaker]: Could not disable frame rate." << std::endl;
                }
            }
            catch (...) {
                std::cerr << "[opendlv-device-camera-spinnaker]: Could not set frame rate." << std::endl;
            }

            // Enable auto exposure.
            camera->ExposureAuto.SetValue(Spinnaker::ExposureAutoEnums::ExposureAuto_Continuous);

            // Enable auto gain.
            camera->GainAuto.SetValue(Spinnaker::GainAutoEnums::GainAuto_Continuous);

            // Enable auto white balance.
            camera->BalanceWhiteAuto.SetValue(Spinnaker::BalanceWhiteAutoEnums::BalanceWhiteAuto_Continuous);

            // Enable PTP.
            try {
                camera->GevIEEE1588.SetValue(true);
            }
            catch (...) {
                std::cerr << "[opendlv-device-camera-spinnaker]: Could not enable PTP." << std::endl;
            }

            // Define WIDTH, HEIGHT, OFFSETX, OFFSETY.
            camera->Height.SetValue(HEIGHT);
            camera->Width.SetValue(WIDTH);
            camera->OffsetX.SetValue(OFFSET_X);
            camera->OffsetY.SetValue(OFFSET_Y);

            // Accessing the low-level X11 data display.
            Display *display{nullptr};
            Visual *visual{nullptr};
            Window window{0};
            XImage *ximage{nullptr};
            if (VERBOSE) {
                display = XOpenDisplay(NULL);
                visual  = DefaultVisual(display, 0);
                window  = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, WIDTH, HEIGHT, 1, 0, 0);
                sharedMemoryARGB->lock();
                ximage = XCreateImage(display, visual, 24, ZPixmap, 0, sharedMemoryARGB->data(), WIDTH, HEIGHT, 32, 0);
                sharedMemoryARGB->unlock();
                XMapWindow(display, window);
            }

            // Start camera.
            camera->AcquisitionMode.SetValue(Spinnaker::AcquisitionModeEnums::AcquisitionMode_Continuous);
            camera->BeginAcquisition();

            // Frame grabbing loop.
            while (!cluon::TerminateHandler::instance().isTerminated.load()) {
                Spinnaker::ImagePtr image{camera->GetNextImage()};
                if ((Spinnaker::IMAGE_NO_ERROR == image->GetImageStatus()) && (image->GetTimeStamp() > 0)) {
                    uint64_t imageTimestamp = image->GetTimeStamp();
                    int width               = image->GetWidth();
                    int height              = image->GetHeight();

                    std::clog << "Grabbed frame of size " << width << "x" << height << " at " << imageTimestamp << std::endl;
                    cluon::data::TimeStamp ts{cluon::time::now()};

                    if ((static_cast<uint32_t>(width) == WIDTH) && (static_cast<uint32_t>(height) == HEIGHT)) {
                        sharedMemoryI420->lock();
                        sharedMemoryI420->setTimeStamp(ts);
                        {
                            libyuv::UYVYToI420(reinterpret_cast<uint8_t *>(image->GetData()), WIDTH * 2 /* 2*WIDTH for YUYV 422*/,
                                               reinterpret_cast<uint8_t *>(sharedMemoryI420->data()), WIDTH,
                                               reinterpret_cast<uint8_t *>(sharedMemoryI420->data() + (WIDTH * HEIGHT)), WIDTH / 2,
                                               reinterpret_cast<uint8_t *>(sharedMemoryI420->data() + (WIDTH * HEIGHT + ((WIDTH * HEIGHT) >> 2))), WIDTH / 2,
                                               WIDTH, HEIGHT);
                        }
                        sharedMemoryI420->unlock();

                        sharedMemoryARGB->lock();
                        sharedMemoryARGB->setTimeStamp(ts);
                        {
                            libyuv::I420ToARGB(reinterpret_cast<uint8_t *>(sharedMemoryI420->data()), WIDTH,
                                               reinterpret_cast<uint8_t *>(sharedMemoryI420->data() + (WIDTH * HEIGHT)), WIDTH / 2,
                                               reinterpret_cast<uint8_t *>(sharedMemoryI420->data() + (WIDTH * HEIGHT + ((WIDTH * HEIGHT) >> 2))), WIDTH / 2,
                                               reinterpret_cast<uint8_t *>(sharedMemoryARGB->data()), WIDTH * 4,
                                               WIDTH, HEIGHT);

                            if (VERBOSE) {
                                XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 0, 0, WIDTH, HEIGHT);
                            }
                        }
                        sharedMemoryARGB->unlock();

                        // Wake up any pending processes.
                        sharedMemoryI420->notifyAll();
                        sharedMemoryARGB->notifyAll();

                        image->Release();
                    } else {
                        std::cerr << "[opendlv-device-camera-spinnaker]: Grabbed frame of size " << width << "x" << height << " does not match size of shared memory!" << std::endl;
                    }
                }
            }

            camera->EndAcquisition();

            // Release any resources.
            camera->DeInit();
            listOfCameras.Clear();
            system->ReleaseInstance();
        }
        retCode = 0;
    }
    return retCode;
}
