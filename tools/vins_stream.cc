/*
* Roboception GmbH
* Munich, Germany
* www.roboception.com
*
* Copyright (c) 2017 Roboception GmbH
* All rights reserved
*
* Author: Christian Emmerich
*/

#include <rcvinsapi/pose_interface.h>
#include <rcgcapi/device.h>
#include <rcgcapi/config.h>

#include <fstream>
#include <getopt.h>
#include <signal.h>
#include <chrono>


using namespace std;


static bool stop_vins_streaming = false;


void signal_callback_handler(int signum)
{
  printf("Caught signal %d\n",signum);
  stop_vins_streaming = true;
}


void printUsage(char *arg)
{
  cout << "Requests a pose stream from the specified rc_visard device and "
          "prints received poses (or records them as csv-file, see -o option)."
       << "\nUsage: "
       << arg
       << " -d deviceId [-m maxNumPoses][-s maxRecTimeSecs][-o outputFile]"
       << endl;
}


void printPoseAsCSVLine(shared_ptr<rc::VINSRemoteInterface::PoseType> pose, ofstream& s)
{
  static bool firstTime = true;

  auto posestamped = pose->pose();
  auto cov = posestamped.pose().covariance();

  // first time also print header
  if (firstTime)
  {
    s << "TIMESTAMP,PARENT,NAME,X,Y,Z,QX,QY,QZ,QW";
    for (int i = 0; i<cov.size(); i++)
    {
      s << ",COV_" << i;
    }
    s << endl;
  }
  firstTime = false;

  // print pose as csv-line
  s << posestamped.timestamp().sec() << posestamped.timestamp().nsec()
    << "," << pose->parent()  << "," << pose->name()
    << "," << posestamped.pose().position().x()
    << "," << posestamped.pose().position().y()
    << "," << posestamped.pose().position().z()
    << "," << posestamped.pose().orientation().x()
    << "," << posestamped.pose().orientation().y()
    << "," << posestamped.pose().orientation().z()
    << "," << posestamped.pose().orientation().w();
  for (int i = 0; i<cov.size(); i++)
  {
    s << "," << cov.Get(i);
  }
  s << endl;
}


int main(int argc, char *argv[])
{
  // Register signals and signal handler
  signal(SIGINT, signal_callback_handler);
  signal(SIGTERM, signal_callback_handler);


  /**
   * Parse program options (e.g. device_id )
   */
  string deviceId, outputFileName;
  unsigned int maxNumRecordingPoses = 50, maxRecordingTimeSecs = 5;
  bool userSetDeviceId = false;
  bool userSetOutputFile = false;
  bool userSetMaxPoses = false;
  bool userSetRecordingTime = false;

  int opt;
  while ((opt = getopt(argc, argv, "hm:d:o:s:")) != -1) {
    switch (opt) {
      case 'd':
        deviceId = string(optarg);
        userSetDeviceId = true;
        break;
      case 'm':
        maxNumRecordingPoses = (unsigned int) max(0, atoi(optarg));
        userSetMaxPoses = true;
        break;
      case 's':
        maxRecordingTimeSecs = (unsigned int) max(0, atoi(optarg));
        userSetRecordingTime = true;
        break;
      case 'o':
        outputFileName = string(optarg);
        userSetOutputFile = true;
        break;
      case 'h':
        printUsage(argv[0]);
        return EXIT_SUCCESS;
      default: /* '?' */
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }
  }

  if (!userSetDeviceId)
  {
    cerr << "Please specify device id." << endl;
    printUsage(argv[0]);
    return EXIT_FAILURE;
  }

  if (!userSetMaxPoses && !userSetRecordingTime)
  {
    userSetMaxPoses = true;;
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
      cerr << "Could not open file '" << outputFileName << "' for writing!"
           << endl;
      return EXIT_FAILURE;
    }
  }


  /**
   * open genicam device in order to extract network configuration
   */
  shared_ptr<rcg::Device> dev=rcg::getDevice(deviceId.c_str());
  if (!dev)
  {
    cerr << "Device '" << deviceId << "' not found!" << endl;
    rcg::System::clearSystems();
    return EXIT_FAILURE;
  }
  dev->open(rcg::Device::ACCESS::READONLY);

  shared_ptr<GenApi::CNodeMapRef> nodemap = dev->getRemoteNodeMap();
  string addr = rcg::getString(nodemap, "GevCurrentIPAddress");
  string subnet = rcg::getString(nodemap, "GevCurrentSubnetMask");
  cout << "Connected to '" << deviceId << "' with network config " << addr
       << " (" << subnet << ")" << endl;

  dev->close(); // from here on, we dont need genicam device anymore


  /**
   * Instantiate VINSRemoteInterface and start listening to pose stream
   */
  rc::VINSRemoteInterface vins(addr, subnet);
  unsigned int cntPoses=0;
  try
  {
    cout << "Starting VINS on device..." << endl;
    vins.start(false);

    cout << "Initializing pose stream..." << endl;
    if (!vins.initPoseReceiver())
    {
      cerr << "Could not initialize pose stream!" << endl;
    } else
    {
      unsigned int timeoutMillis = 100;
      vins.setReceiveTimeout(timeoutMillis);
      cout << "Listening to poses..." << endl;

      chrono::time_point<chrono::system_clock> start = chrono::system_clock::now();
      chrono::duration<double> elapsedSecs(0);
      while (!stop_vins_streaming
             && (!userSetMaxPoses || cntPoses < maxNumRecordingPoses)
             && (!userSetRecordingTime ||
                 elapsedSecs.count() < maxRecordingTimeSecs)
              )
      {
        shared_ptr<rc::VINSRemoteInterface::PoseType> pose = vins.receivePose();
        if (pose)
        {
          ++cntPoses;
          if (outputFile.is_open())
          {
            printPoseAsCSVLine(pose, outputFile);
          }
          else
          {
            cout << "received pose " << endl << pose->DebugString() << endl;
          }
        }
        else
        {
          cerr << "did not receive any data during last " << timeoutMillis
               << " ms." << endl;
        }
        elapsedSecs = chrono::system_clock::now() - start;
      }
    }
  } catch (exception &e)
  {
    cout << "Caught exception during streaming, stopping: " << e.what() << endl;
  }


  /**
   * Stopping streaming and clean-up
   */
  try
  {
    cout << "stopping VINS on device..." << endl;
    vins.destroyPoseReceiver();
    vins.stop();

  } catch (exception &e)
  {
    cout << "Caught exception: " << e.what() << endl;
  }

  if (outputFile.is_open())
  {
    outputFile.close();
    cout << "Recorded " << cntPoses << " poses to '" << outputFileName << "'." << endl;
  } else {
    cout << "Received  " << cntPoses << " poses." << endl;
  }

  rcg::System::clearSystems();

  return EXIT_SUCCESS;
}