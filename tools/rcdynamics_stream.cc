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
using namespace rc::dynamics;

namespace gpb = ::google::protobuf;
namespace rcmsgs = roboception::msgs;


/**
 * define different printing behaviors - one for each protobuf type
 */
template<class T>
void printAsCsv(const gpb::Message*, ofstream &s);

// printing behaviour for Pose type
template<>
void printAsCsv<rcmsgs::Pose>(
        const gpb::Message *m, ofstream &s)
{
  auto pose = (const rcmsgs::Pose*) m;
  s << "," << pose->position().x()
     << "," << pose->position().y()
     << "," << pose->position().z()
     << "," << pose->orientation().x()
     << "," << pose->orientation().y()
     << "," << pose->orientation().z()
     << "," << pose->orientation().w();
  auto cov = pose->covariance();
  for (int i = 0; i<cov.size(); i++)
  {
    s << "," << cov.Get(i);
  }
}

// printing behaviour for Frame type
template<>
void printAsCsv<rcmsgs::Frame>(
        const gpb::Message *m, ofstream &s)
{
  auto pose = (const rcmsgs::Frame*) m;
  auto posestamped = pose->pose();
  auto cov = posestamped.pose().covariance();
  s << posestamped.timestamp().sec() << posestamped.timestamp().nsec()
     << "," << pose->parent()  << "," << pose->name();
  printAsCsv<rcmsgs::Pose>(&posestamped.pose(), s);
}

// printing behaviour for Imu type
template<>
void printAsCsv<rcmsgs::Imu>(
        const gpb::Message *m, ofstream &s)
{
  auto imu = (const rcmsgs::Imu*) m;
  s << imu->timestamp().sec() << imu->timestamp().nsec()
     << "," << imu->linear_acceleration().x()
     << "," << imu->linear_acceleration().y()
     << "," << imu->linear_acceleration().z()
     << "," << imu->angular_velocity().x()
     << "," << imu->angular_velocity().y()
     << "," << imu->angular_velocity().z();
}

// printing behaviour for Dynamics type
template<>
void printAsCsv<rcmsgs::Dynamics>(
        const gpb::Message *m, ofstream &s)
{
  auto dyn = (const rcmsgs::Dynamics*) m;
  s << dyn->timestamp().sec() << dyn->timestamp().nsec();
  printAsCsv<rcmsgs::Pose>(&dyn->pose(), s);
  s << "," << dyn->pose_frame()
     << "," << dyn->pose_frame()
     << "," << dyn->linear_velocity().x()
     << "," << dyn->linear_velocity().y()
     << "," << dyn->linear_velocity().z()
     << "," << dyn->linear_velocity_frame()
     << "," << dyn->angular_velocity().x()
     << "," << dyn->angular_velocity().y()
     << "," << dyn->angular_velocity().z()
     << "," << dyn->angular_velocity_frame()
     << "," << dyn->linear_acceleration().x()
     << "," << dyn->linear_acceleration().y()
     << "," << dyn->linear_acceleration().z()
     << "," << dyn->linear_acceleration_frame();
  auto cov = dyn->covariance();
  for (int i = 0; i<cov.size(); i++)
  {
    s << "," << cov.Get(i);
  }
  printAsCsv<rcmsgs::Frame>(&dyn->cam2imu_transform(), s);
  s << "," << dyn->possible_jump();
}


/**
 * Class to register and handle all the different printing behaviours, one for
 * each msg type
 */
class CSVPrinter
{
  public:
    CSVPrinter()
    {
      // register printing behaviour for all known protobuf types
      printerMap[rcmsgs::Frame::descriptor()->name()] = std::bind(
              &printAsCsv<rcmsgs::Frame>,
              std::placeholders::_1, std::placeholders::_2);
      printerMap[rcmsgs::Imu::descriptor()->name()] = std::bind(
              &printAsCsv<rcmsgs::Imu>,
              std::placeholders::_1, std::placeholders::_2);
      printerMap[rcmsgs::Dynamics::descriptor()->name()] = std::bind(
              &printAsCsv<rcmsgs::Dynamics>,
              std::placeholders::_1, std::placeholders::_2);
    }

    void print(const string& protobufType, const gpb::Message* m, ofstream &s)
    {
      printerMap[protobufType](m, s);
      s << endl;
    }

  protected:
    std::map<std::string, std::function<void (const gpb::Message*, ofstream &)>> printerMap;
};



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
          "\nand either prints received messages, or records them as csv-file, "
          "\nsee -o option)."
       << "\n\nUsage: \n\t"
       << arg
       << " -v rcVisardIP -s stream [-i networkInterface]"
          "\n\t\t[-n maxNumData][-t maxRecTimeSecs][-o outputFile]"
       << endl;
}


int main(int argc, char *argv[])
{
  // Register signals and signal handler
  signal(SIGINT, signal_callback_handler);
  signal(SIGTERM, signal_callback_handler);


  /**
   * Parse program options (e.g. IP )
   */
  string outputFileName, ip_str, ifa_str = "", type_str;
  unsigned int maxNumRecordingMsgs = 50, maxRecordingTimeSecs = 5;
  bool userSetOutputFile = false;
  bool userSetMaxNumMsgs = false;
  bool userSetRecordingTime = false;
  bool userSetIp = false;
  bool userSetStreamType = false;

  int opt;
  while ((opt = getopt(argc, argv, "hn:v:i:o:t:s:")) != -1)
  {
    switch (opt)
    {
      case 's': // stream type(s)
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
        maxNumRecordingMsgs = (unsigned int) max(0, atoi(optarg));
        userSetMaxNumMsgs = true;
        break;
      case 't':
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
  if (!userSetMaxNumMsgs && !userSetRecordingTime)
  {
    userSetMaxNumMsgs = true;;
  }

  /**
   * open file for recording if required
   */
  CSVPrinter csv;
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
   * Instantiate RemoteInterface and start rc_dynamics module
   */
  cout << "connecting rc_visard " << ip_str << "..." << endl;
  auto dyn = RemoteInterface::create(ip_str);

  try
  {
    // start the rc::dynamics module on the rc_visard
    cout << "starting rc_dynamics module on rc_visard..." << endl;
    dyn->start();
  }
  catch (exception &e)
  {
    cout << "ERROR! Could not start rc_dynamics module on rc_visard: "
         << e.what() << endl;
    return EXIT_FAILURE;
  }

  /**
   * Request a data stream and start receiving as well as processing the data
   */
  unsigned int cntMsgs = 0;
  try
  {
    cout << "Initializing " << type_str << " data stream..." << endl;
    auto receiver = dyn->createReceiverForStream(type_str, ifa_str);

    unsigned int timeoutMillis = 100;
    receiver->setTimeout(timeoutMillis);
    cout << "Listening for " << type_str << " messages..." << endl;

    chrono::time_point<chrono::system_clock> start = chrono::system_clock::now();
    chrono::duration<double> elapsedSecs(0);
    while (!caught_signal
           && (!userSetMaxNumMsgs || cntMsgs < maxNumRecordingMsgs)
           && (!userSetRecordingTime ||
               elapsedSecs.count() < maxRecordingTimeSecs)
            )
    {
      auto msg = receiver->receive(dyn->getProtobufTypeOfStream(type_str));
      if (msg)
      {
        ++cntMsgs;
        if (outputFile.is_open())
        {
          csv.print(dyn->getProtobufTypeOfStream(type_str), msg.get(), outputFile);
        }
        else
        {
          cout << "received " << type_str << " msg:" << endl
               << msg->DebugString() << endl;
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
    cout << "stopping rc_dynamics module on rc_visard..." << endl;
    dyn->stop();

  }
  catch (exception &e)
  {
    cout << "Caught exception: " << e.what() << endl;
  }

  if (outputFile.is_open())
  {
    outputFile.close();
    cout << "Recorded " << cntMsgs << " " << type_str << " messages to '"
         << outputFileName << "'." << endl;
  }
  else
  {
    cout << "Received  " << cntMsgs << " " << type_str << " messages." << endl;
  }

  return EXIT_SUCCESS;
}