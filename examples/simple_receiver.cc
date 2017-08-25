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
namespace rcdyn = rc::dynamics;


/**
 * catching signals for proper program escape
 */
static bool caught_signal = false;
void signal_callback_handler(int signum)
{
  caught_signal = true;
}


/**
 * Print usage of example including command line args
 */
void printUsage(char *arg)
{
  cout << "\nRequests a pose stream from the specified rc_visard IP "
          "\nand prints received poses to std out."
       << "\n\nUsage: \n\t"
       << arg
       << " -v rcVisardIP [-i networkInterface][-m maxNumPoses]"
       << endl;
}


int main(int argc, char *argv[])
{
  // Register signals and signal handler for proper program escape
  signal(SIGINT, signal_callback_handler);
  signal(SIGTERM, signal_callback_handler);


  /**
   * Parse program options (e.g. IP, desired interface for receiving data, ...)
   */
  string ip_str, ifa_str = "";
  unsigned int maxNumRecordingPoses = 50, cnt = 0;
  bool userSetIp = false;

  int opt;
  while ((opt = getopt(argc, argv, "hm:v:i:")) != -1)
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
    cerr << "Please specify rc_visard IP." << endl;
    printUsage(argv[0]);
    return EXIT_FAILURE;
  }


  /**
   * Instantiate rc::dynamics::RemoteInterface and start streaming
   */
  cout << "connecting rc_visard " << ip_str << "..." << endl;
  auto rcdynInterface = rcdyn::RemoteInterface::create(ip_str);

  try
  {
    // start the rc::dynamics module on the rc_visard
    cout << "starting rc_dynamics module on rc_visard..." << endl;
    rcdynInterface->start(false);
  }
  catch (exception &e)
  {
    cout << "ERROR! Could not start rc_dynamics module on rc_visard: "
         << e.what() << endl;
    return EXIT_FAILURE;
  }

  try
  {
    // easy-to-use creation of a pose receiver
    cout << "creating receiver and waiting for first messages to arrive..." << endl;
    auto receiver = rcdynInterface->createReceiverForStream<rcdyn::RemoteInterface::PoseType>(
            "pose", ifa_str);
    receiver->setTimeout(250);

    // receive poses and print them
    for (; cnt < maxNumRecordingPoses && !caught_signal; ++cnt)
    {
      shared_ptr<rcdyn::RemoteInterface::PoseType> pose = receiver->receive();
      if (pose)
      {
        cout << "received pose " << endl << pose->DebugString() << endl;
      }
    }
  }
  catch (exception &e)
  {
    cout << "ERROR during streaming: " << e.what() << endl;
  }


  /**
   * Stopping streaming and clean-up
   */
  try
  {
    cout << "stopping rc_dynamics module on rc_visard..." << endl;
    rcdynInterface->stop();
  }
  catch (exception &e)
  {
    cout << "ERROR! Could not start rc_dynamics module on rc_visard: "
         << e.what() << endl;
  }

  cout << "Received  " << cnt << " poses." << endl;
  return EXIT_SUCCESS;
}