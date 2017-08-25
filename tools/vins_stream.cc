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

#include <rc_dynamics_api/remote_interface.h>

#include <fstream>
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
  cout << "\nRequests a pose stream from the specified rc_visard IP "
          "\nand prints received poses (or records them as csv-file, "
          "\nsee -o option)."
       << "\n\nUsage: \n\t"
       << arg
       << " -v rcVisardIP [-i networkInterface]"
          "\n\t\t[-m maxNumPoses][-s maxRecTimeSecs][-o outputFile]"
       << endl;
}


void printPoseAsCSVLine(shared_ptr<rc::dynamics::RemoteInterface::PoseType> pose, ofstream& s)
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
   * Parse program options (e.g. IP )
   */
  string outputFileName, ip_str, ifa_str = "";
  unsigned int maxNumRecordingPoses = 50, maxRecordingTimeSecs = 5;
  bool userSetOutputFile = false;
  bool userSetMaxPoses = false;
  bool userSetRecordingTime = false;
  bool userSetIp = false;

  int opt;
  while ((opt = getopt(argc, argv, "hm:v:i:o:s:")) != -1)
  {
    switch (opt)
    {
      case 'i':
        ifa_str = string(optarg);
        break;
      case 'v':
        ip_str = string(optarg);
        userSetIp = true;
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

  if (!userSetIp)
  {
    cerr << "Please specify device IP." << endl;
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
   * Instantiate VINSRemoteInterface and start listening to pose stream
   */
  using namespace rc::dynamics;
  RemoteInterface::Ptr vins = RemoteInterface::create(ip_str);
  unsigned int cntPoses = 0;
  try
  {
    cout << "Starting VINS on device..." << endl;
    vins->start(false);

    cout << "Initializing pose stream..." << endl;
    DataReceiver<RemoteInterface::PoseType>::Ptr receiver =
            vins->createReceiverForStream<RemoteInterface::PoseType>("pose",
                                                                    ifa_str);

    unsigned int timeoutMillis = 100;
    receiver->setTimeout(timeoutMillis);
    cout << "Listening to poses..." << endl;

    chrono::time_point<chrono::system_clock> start = chrono::system_clock::now();
    chrono::duration<double> elapsedSecs(0);
    while (!stop_vins_streaming
           && (!userSetMaxPoses || cntPoses < maxNumRecordingPoses)
           && (!userSetRecordingTime ||
               elapsedSecs.count() < maxRecordingTimeSecs)
            )
    {
      shared_ptr<RemoteInterface::PoseType> pose = receiver->receive();
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
  catch (exception &e)
  {
    cout << "Caught exception during streaming, stopping: " << e.what() << endl;
  }


  /**
   * Stopping streaming and clean-up
   */
  try
  {
    cout << "stopping VINS on device..." << endl;
    vins->stop();

  }
  catch (exception &e)
  {
    cout << "Caught exception: " << e.what() << endl;
  }

  if (outputFile.is_open())
  {
    outputFile.close();
    cout << "Recorded " << cntPoses << " poses to '" << outputFileName << "'."
         << endl;
  }
  else
  {
    cout << "Received  " << cntPoses << " poses." << endl;
  }

  return EXIT_SUCCESS;
}