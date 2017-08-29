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

#include <signal.h>

using namespace std;
namespace rcdyn = rc::dynamics;


/**
 * catching signals for proper program escape
 */
static bool caught_signal = false;
void signal_callback_handler(int signum)
{
  printf("Caught signal %d, stopping program!\n",signum);
  caught_signal = true;
}


/**
 * Print usage of example including command line args
 */
void printUsage(char *arg)
{
  cout << "\nRequests a data stream from the specified rc_visard IP "
          "\nand simply prints received data to std out."
       << "\n\nUsage: \n\t"
       << arg
       << " -v rcVisardIP -s stream [-i networkInterface][-n numMessages]"
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
  string ip_str, ifa_str = "", type_str = "";
  unsigned int maxNumMsgs = 50, cnt = 0;
  bool userSetIp = false;
  bool userSetStreamType = false;

  int opt;
  while ((opt = getopt(argc, argv, "hn:v:i:s:")) != -1)
  {
    switch (opt)
    {
      case 's': // stream type
        type_str = string(optarg);
        userSetStreamType = true;
        break;
      case 'i':
        ifa_str = string(optarg);
        break;
      case 'v':
        ip_str = string(optarg);
        userSetIp = true;
        break;
      case 'n':
        maxNumMsgs = (unsigned int) max(0, atoi(optarg));
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
  if (!userSetStreamType)
  {
    cerr << "Please specify stream type." << endl;
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
    rcdynInterface->start();
  }
  catch (exception &e)
  {
    cout << "ERROR! Could not start rc_dynamics module on rc_visard: "
         << e.what() << endl;
    return EXIT_FAILURE;
  }

  try
  {
    // easy-to-use creation of a data receiver, parameterized via stream type
    cout << "creating receiver and waiting for first messages to arrive..."
         << endl;
    auto receiver = rcdynInterface->createReceiverForStream(type_str, ifa_str);
    receiver->setTimeout(250);

    // receive rc_dynamics proto msgs and print them
    for (; cnt < maxNumMsgs && !caught_signal; ++cnt)
    {
      auto msg = receiver->receive(
              rcdynInterface->getProtobufTypeOfStream(type_str));
      if (msg)
      {
        cout << "received msg " << endl << msg->DebugString() << endl;
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

  cout << "Received  " << cnt << " " << type_str << " messages." << endl;
  return EXIT_SUCCESS;
}