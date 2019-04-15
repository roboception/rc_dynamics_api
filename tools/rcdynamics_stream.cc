/*
 * This file is part of the rc_dynamics_api package.
 *
 * Copyright (c) 2017 Roboception GmbH
 * All rights reserved
 *
 * Author: Christian Emmerich
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <fstream>
#include <signal.h>
#include <chrono>
#include <iomanip>

#include "rc_dynamics_api/remote_interface.h"
#include "csv_printing.h"

#ifdef WIN32
#include <winsock2.h>
#undef max
#undef min
#endif

using namespace std;
using namespace rc::dynamics;

/**
 * catching signals for proper program escape
 */
static bool caught_signal = false;
void signal_callback_handler(int signum)
{
  printf("Caught signal %d, stopping program!\n", signum);
  caught_signal = true;
}

/**
 * Print usage of example including command line args
 */
void printUsage(char* arg)
{
  cout << "\nLists available rcdynamics data streams of the specified rc_visard IP, "
          "\nor requests a data stream and either prints received messages or records "
          "\nthem as csv-file, see -o option."
       << "\n\nUsage: \n"
       << arg << " -v <rcVisardIP> -l | -s <stream> [-a] [-i <networkInterface>]"
                 " [-n <maxNumData>][-t <maxRecTimeSecs>][-o <outputFile>]"
       << endl;
}

int main(int argc, char* argv[])
{
#ifdef WIN32
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

  // Register signals and signal handler
  signal(SIGINT, signal_callback_handler);
  signal(SIGTERM, signal_callback_handler);

  /**
   * Parse program options (e.g. IP )
   */
  string outputFileName, visardIP, networkIface = "", streamName;
  unsigned int maxNumRecordingMsgs = 50, maxRecordingTimeSecs = 5;
  bool userAutostart = false;
  bool userSetOutputFile = false;
  bool userSetMaxNumMsgs = false;
  bool userSetRecordingTime = false;
  bool userSetIp = false;
  bool userSetStreamType = false;
  bool onlyListStreams = false;

  int i = 1;
  while (i < argc)
  {
    std::string p = argv[i++];

    if (p == "-l")
    {
      onlyListStreams = true;
    }
    else if (p == "-s" && i < argc)
    {
      streamName = string(argv[i++]);
      userSetStreamType = true;
    }
    else if (p == "-a")
    {
      userAutostart = true;
    }
    else if (p == "-i" && i < argc)
    {
      networkIface = string(argv[i++]);
    }
    else if (p == "-v" && i < argc)
    {
      visardIP = string(argv[i++]);
      userSetIp = true;
    }
    else if (p == "-n" && i < argc)
    {
      maxNumRecordingMsgs = (unsigned int)std::max(0, atoi(argv[i++]));
      userSetMaxNumMsgs = true;
    }
    else if (p == "-t" && i < argc)
    {
      maxRecordingTimeSecs = (unsigned int)std::max(0, atoi(argv[i++]));
      userSetRecordingTime = true;
    }
    else if (p == "-o" && i < argc)
    {
      outputFileName = string(argv[i++]);
      userSetOutputFile = true;
    }
    else if (p == "-h")
    {
      printUsage(argv[0]);
      return EXIT_SUCCESS;
    }
    else
    {
      printUsage(argv[0]);
      return EXIT_FAILURE;
    }
  }

  if (!userSetIp)
  {
    cerr << "Please specify rc_visard IP." << endl;
    printUsage(argv[0]);
    return EXIT_FAILURE;
  }

  if (!userSetStreamType && !onlyListStreams)
  {
    cerr << "Please specify stream type." << endl;
    printUsage(argv[0]);
    return EXIT_FAILURE;
  }

  if (!userSetMaxNumMsgs && !userSetRecordingTime)
  {
    userSetMaxNumMsgs = true;
  }

  /**
   * open file for recording if required
   */
  ofstream outputFile;
  if (userSetOutputFile)
  {
    outputFile.open(outputFileName);
    if (!outputFile.is_open())
    {
      cerr << "Could not open file '" << outputFileName << "' for writing!" << endl;
      return EXIT_FAILURE;
    }
  }

  /**
   * Instantiate RemoteInterface
   */
  cout << "connecting to rc_visard " << visardIP << "..." << endl;
  auto rcvisardDynamics = RemoteInterface::create(visardIP);

  /* Only list available streams of device and exit */
  if (onlyListStreams)
  {
    auto streams = rcvisardDynamics->getAvailableStreams();
    string firstColumn = "Available streams:";
    size_t firstColumnWidth = firstColumn.length();
    for (auto&& s : streams)
      if (s.length() > firstColumnWidth)
        firstColumnWidth = s.length();
    firstColumnWidth += 5;
    cout << left << setw(firstColumnWidth) << firstColumn << "Protobuf message types:" << endl;
    for (auto&& s : streams)
      cout << left << setw(firstColumnWidth) << s << rcvisardDynamics->getPbMsgTypeOfStream(s) << endl;
    cout << endl << "rc_dynamics is in state: " << rcvisardDynamics->getState();
    cout << endl;
    return EXIT_SUCCESS;
  }

  /* For all streams except 'imu' the rc_dynamcis node has to be started */
  if (userAutostart && streamName != "imu")
  {
    try
    {
      cout << "starting SLAM on rc_visard..." << endl;
      rcvisardDynamics->startSlam();
    }
    catch (exception&)
    {
      try
      {
        // start the rc::dynamics module on the rc_visard
        cout << "SLAM not available!" << endl;
        cout << "starting stereo INS on rc_visard..." << endl;
        rcvisardDynamics->start();
      }
      catch (exception& e)
      {
        cout << "ERROR! Could not start rc_dynamics module on rc_visard: " << e.what() << endl;
        return EXIT_FAILURE;
      }
    }
  }

  /**
   * Request a data stream and start receiving as well as processing the data
   */
  unsigned int cntMsgs = 0;
  try
  {
    cout << "Initializing " << streamName << " data stream..." << endl;
    auto receiver = rcvisardDynamics->createReceiverForStream(streamName, networkIface);

    unsigned int timeoutMillis = 100;
    receiver->setTimeout(timeoutMillis);
    cout << "Listening for " << streamName << " messages..." << endl;

    chrono::time_point<chrono::system_clock> start = chrono::system_clock::now();
    chrono::duration<double> elapsedSecs(0);
    while (!caught_signal && (!userSetMaxNumMsgs || cntMsgs < maxNumRecordingMsgs) &&
           (!userSetRecordingTime || elapsedSecs.count() < maxRecordingTimeSecs))
    {
      auto msg = receiver->receive(rcvisardDynamics->getPbMsgTypeOfStream(streamName));
      if (msg)
      {
        if (outputFile.is_open())
        {
          if (cntMsgs == 0)
          {
            csv::Header h;
            outputFile << (h << *msg) << endl;
          }
          csv::Line l;
          outputFile << (l << *msg) << endl;
        }
        else
        {
          cout << "received " << streamName << " msg:" << endl << msg->DebugString() << endl;
        }
        ++cntMsgs;
      }
      else
      {
        cerr << "did not receive any data during last " << timeoutMillis << " ms." << endl;
      }
      elapsedSecs = chrono::system_clock::now() - start;
    }
  }
  catch (exception& e)
  {
    cout << "Caught exception during streaming, stopping: " << e.what() << endl;
  }

  /**
   * Stopping streaming and clean-up
   * 'imu' stream works regardless if the rc_dynamics module is running, so no need to stop it
   */
  if (userAutostart && streamName != "imu")
  {
    try
    {
      cout << "stopping rc_dynamics module on rc_visard..." << endl;
      rcvisardDynamics->stop();
    }
    catch (exception& e)
    {
      cout << "Caught exception: " << e.what() << endl;
    }
  }

  if (outputFile.is_open())
  {
    outputFile.close();
    cout << "Recorded " << cntMsgs << " " << streamName << " messages to '" << outputFileName << "'." << endl;
  }
  else
  {
    cout << "Received  " << cntMsgs << " " << streamName << " messages." << endl;
  }

#ifdef WIN32
  ::WSACleanup();
#endif

  return EXIT_SUCCESS;
}
