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

#include "remote_interface.h"
#include "unexpected_receive_timeout.h"

#include "json.hpp"
#include <cpr/cpr.h>
#include <regex>

using namespace std;
using json = nlohmann::json;

namespace rc
{
namespace dynamics
{
// Definitions of static const members
const std::string RemoteInterface::State::IDLE = "IDLE";
const std::string RemoteInterface::State::RUNNING = "RUNNING";
const std::string RemoteInterface::State::FATAL = "FATAL";
const std::string RemoteInterface::State::WAITING_FOR_INS = "WAITING_FOR_INS";
const std::string RemoteInterface::State::WAITING_FOR_INS_AND_SLAM = "WAITING_FOR_INS_AND_SLAM";
const std::string RemoteInterface::State::WAITING_FOR_SLAM = "WAITING_FOR_SLAM";
const std::string RemoteInterface::State::RUNNING_WITH_SLAM = "RUNNING_WITH_SLAM";

string toString(cpr::Response resp)
{
  stringstream s;
  s << "status code: " << resp.status_code << endl
    << "url: " << resp.url << endl
    << "text: " << resp.text << endl
    << "error: " << resp.error.message;
  return s.str();
}

string toString(list<string> list)
{
  stringstream s;
  s << "[";
  for (auto it = list.begin(); it != list.end();)
  {
    s << *it;
    if (++it != list.end())
    {
      s << ", ";
    }
  }
  s << "]";
  return s.str();
}

void handleCPRResponse(cpr::Response r)
{
  if (r.status_code != 200)
  {
    throw runtime_error(toString(r));
  }
}

/**
 * Class for data stream receivers that are created by this
 * remote interface in order to keep track of created streams.
 *
 */
class TrackedDataReceiver : public DataReceiver
{
public:
  static shared_ptr<TrackedDataReceiver> create(const string& ip_address, unsigned int& port, const string& stream,
                                                shared_ptr<RemoteInterface> creator)
  {
    return shared_ptr<TrackedDataReceiver>(new TrackedDataReceiver(ip_address, port, stream, creator));
  }

  virtual ~TrackedDataReceiver()
  {
    try
    {
      _creator->deleteDestinationFromStream(_stream, _dest);
    }
    catch (exception& e)
    {
      cerr << "[TrackedDataReceiver] Could not remove my destination " << _dest << " for stream type " << _stream
           << " from rc_visard: " << e.what() << endl;
    }
  }

protected:
  TrackedDataReceiver(const string& ip_address, unsigned int& port, const string& stream,
                      shared_ptr<RemoteInterface> creator)
    : DataReceiver(ip_address, port), _dest(ip_address + ":" + to_string(port)), _stream(stream), _creator(creator)
  {
  }

  string _dest, _stream;
  shared_ptr<RemoteInterface> _creator;
};

// map to store already created RemoteInterface objects
map<string, RemoteInterface::Ptr> RemoteInterface::_remoteInterfaces = map<string, RemoteInterface::Ptr>();

RemoteInterface::Ptr RemoteInterface::create(const string& rcVisardInetAddrs, unsigned int requestsTimeout)
{
  // check if interface is already opened
  auto found = RemoteInterface::_remoteInterfaces.find(rcVisardInetAddrs);
  if (found != RemoteInterface::_remoteInterfaces.end())
  {
    return found->second;
  }

  // if not, create it
  auto newRemoteInterface = Ptr(new RemoteInterface(rcVisardInetAddrs, requestsTimeout));
  RemoteInterface::_remoteInterfaces[rcVisardInetAddrs] = newRemoteInterface;

  return newRemoteInterface;
}

RemoteInterface::RemoteInterface(const string& rcVisardIP, unsigned int requestsTimeout)
  : _visardAddrs(rcVisardIP), _baseUrl("http://" + _visardAddrs + "/api/v1"), _timeoutCurl(requestsTimeout)
{
  _reqStreams.clear();
  _protobufMap.clear();

  // check if given string is a valid IP address
  if (!isValidIPAddress(rcVisardIP))
  {
    throw invalid_argument("Given IP address is not a valid address: " + rcVisardIP);
  }

  // initial connection to rc_visard to get version...
  _visardVersion = 0.0;
  auto get = cpr::Get(cpr::Url{ _baseUrl + "/system" },
                      cpr::Timeout{ _timeoutCurl });
  handleCPRResponse(get);
  string version = json::parse(get.text)["firmware"]["active_image"]["image_version"];
  std::smatch match;
  if (std::regex_search(version, match, std::regex("v(\\d+).(\\d+).(\\d+)")))
  {
    _visardVersion = stof(match[0].str().substr(1,3));
  }

  // ...and get streams, i.e. do get request on
  // respective url (no parameters needed for this simple service call)
  get = cpr::Get(cpr::Url{ _baseUrl + "/datastreams" },
                      cpr::Timeout{ _timeoutCurl });
  handleCPRResponse(get);

  // parse text of response into json object
  auto j = json::parse(get.text);
  for (const auto& stream : j)
  {
    _availStreams.push_back(stream["name"]);
    _protobufMap[stream["name"]] = stream["protobuf"];
  }
}

RemoteInterface::~RemoteInterface()
{
  try {
    cleanUpRequestedStreams();
  } catch (exception& e) {
    cerr << "[RemoteInterface::~RemoteInterface] Could not clean up all previously requested streams: "
         << e.what() << endl;
  }
  for (const auto& s : _reqStreams)
  {
    if (s.second.size() > 0)
    {
      cerr << "[RemoteInterface::~RemoteInterface] Could not stop all previously requested"
              " streams of type "
           << s.first << " on rc_visard. Please check "
                         "device manually"
                         " ("
           << _baseUrl << "/datastreams/" << s.first << ")"
                                                        " for not containing any of the following legacy streams and"
                                                        " delete them otherwise, e.g. using the swagger UI ("
           << "http://" + _visardAddrs + "/api/swagger/)"
           << ": " << toString(s.second) << endl;
    }
  }
}

std::string RemoteInterface::callDynamicsService(std::string serviceName)
{
  cpr::Url url = cpr::Url{ _baseUrl + "/nodes/rc_dynamics/services/" + serviceName };
  auto response = cpr::Put(url, cpr::Timeout{ _timeoutCurl });
  handleCPRResponse(response);
  auto j = json::parse(response.text);
  std::string entered_state;
  bool accepted = true;

  try
  {
    entered_state = j["response"]["current_state"].get<std::string>();
    if (entered_state != State::IDLE and entered_state != State::RUNNING and entered_state != State::FATAL and
        entered_state != State::WAITING_FOR_INS and entered_state != State::WAITING_FOR_INS_AND_SLAM and
        entered_state != State::WAITING_FOR_SLAM and entered_state != State::RUNNING_WITH_SLAM)
    {
      // mismatch between rc_dynamics states and states used in this class?
      throw invalid_state(entered_state);
    }

    accepted = j["response"]["accepted"].get<bool>();
  }
  catch (std::logic_error&)
  {
    // Maybe old interface version? If so just return the numeric code
    // as string - it isn't used by the tools using the old interface
    try
    {
      entered_state = std::to_string(j["response"]["enteredState"].get<int>());
    }
    catch (std::logic_error&)
    {
      // Real problem (may even be unrelated to parsing json. Let the user see what the response is.
      cerr << "Logic error when parsing the response of a service call to rc_dynamics!\n";
      cerr << "Service called: " << url << "\n";
      cerr << "Response:"
           << "\n";
      cerr << response.text << "\n";
      throw;
    }
  }

  if (!accepted)
  {
    throw not_accepted(serviceName);
  }

  return entered_state;
}

std::string RemoteInterface::restart()
{
  return callDynamicsService("restart");
}
std::string RemoteInterface::restartSlam()
{
  return callDynamicsService("restart_slam");
}
std::string RemoteInterface::start()
{
  return callDynamicsService("start");
}
std::string RemoteInterface::startSlam()
{
  return callDynamicsService("start_slam");
}
std::string RemoteInterface::stop()
{
  return callDynamicsService("stop");
}
std::string RemoteInterface::stopSlam()
{
  return callDynamicsService("stop_slam");
}

std::string RemoteInterface::resetSlam()
{
  std::string serviceName = "reset";
  cpr::Url url = cpr::Url{ _baseUrl + "/nodes/rc_slam/services/" + serviceName };
  auto response = cpr::Put(url, cpr::Timeout{ _timeoutCurl });
  handleCPRResponse(response);
  auto j = json::parse(response.text);
  std::string entered_state;
  bool accepted = true;

  try
  {
    entered_state = j["response"]["current_state"].get<std::string>();
    std::vector<std::string> valid_states = { "IDLE",       "RUNNING",   "FATAL", "WAITING_FOR_DATA",
                                              "RESTARTING", "RESETTING", "HALTED" };
    if (std::count(valid_states.begin(), valid_states.end(), entered_state) == 0)
    {
      // mismatch between rc_slam states and states used in this class?
      throw invalid_state(entered_state);
    }

    accepted = j["response"]["accepted"].get<bool>();
  }
  catch (std::logic_error& json_exception)
  {
    // Maybe old interface version? If so just return the numeric code
    // as string - it isn't used by the tools using the old interface
    try
    {
      entered_state = std::to_string(j["response"]["enteredState"].get<int>());
    }
    catch (std::logic_error& json_exception)
    {
      // Real problem (may even be unrelated to parsing json. Let the user see what the response is.
      cerr << "Logic error when parsing the response of a service call to rc_dynamics!\n";
      cerr << "Service called: " << url << "\n";
      cerr << "Response:"
           << "\n";
      cerr << response.text << "\n";
      throw;
    }
  }

  if (!accepted)
  {
    throw not_accepted(serviceName);
  }

  return entered_state;
}

RemoteInterface::ReturnCode RemoteInterface::callSlamService(std::string serviceName, unsigned int timeout_ms)
{
  cpr::Url url = cpr::Url{ _baseUrl + "/nodes/rc_slam/services/" + serviceName };
  auto response = cpr::Put(url, cpr::Timeout{ (int32_t)timeout_ms });
  handleCPRResponse(response);
  auto j = json::parse(response.text);

  ReturnCode return_code;

  try
  {
    return_code.value = j["response"]["return_code"]["value"].get<int>();
    return_code.message = j["response"]["return_code"]["message"];
  }
  catch (std::logic_error& json_exception)
  {
    // Real problem (may even be unrelated to parsing json. Let the user see what the response is.
    cerr << "Logic error when parsing the response of a service call to rc_dynamics!\n";
    cerr << "Service called: " << url << "\n";
    cerr << "Response:"
        << "\n";
    cerr << response.text << "\n";
    throw;
  }

  return return_code;
}

RemoteInterface::ReturnCode RemoteInterface::saveSlamMap(unsigned int timeout_ms)
{
  return callSlamService("save_map", timeout_ms);
}
RemoteInterface::ReturnCode RemoteInterface::loadSlamMap(unsigned int timeout_ms)
{
  return callSlamService("load_map", timeout_ms);
}
RemoteInterface::ReturnCode RemoteInterface::removeSlamMap(unsigned int timeout_ms)
{
  return callSlamService("remove_map", timeout_ms);
}

list<string> RemoteInterface::getAvailableStreams()
{
  return _availStreams;
}

string RemoteInterface::getPbMsgTypeOfStream(const string& stream)
{
  checkStreamTypeAvailable(stream);
  return _protobufMap[stream];
}

list<string> RemoteInterface::getDestinationsOfStream(const string& stream)
{
  checkStreamTypeAvailable(stream);

  list<string> destinations;

  // do get request on respective url (no parameters needed for this simple service call)
  cpr::Url url = cpr::Url{ _baseUrl + "/datastreams/" + stream };
  auto get = cpr::Get(url, cpr::Timeout{ _timeoutCurl });
  handleCPRResponse(get);

  // parse result as json
  auto j = json::parse(get.text);
  for (auto dest : j["destinations"])
  {
    destinations.push_back(dest.get<string>());
  }
  return destinations;
}

void RemoteInterface::addDestinationToStream(const string& stream, const string& destination)
{
  checkStreamTypeAvailable(stream);

  // do put request on respective url (no parameters needed for this simple service call)
  cpr::Url url = cpr::Url{ _baseUrl + "/datastreams/" + stream };
  auto put = cpr::Put(url, cpr::Timeout{ _timeoutCurl }, cpr::Parameters{ { "destination", destination } });
  handleCPRResponse(put);

  // keep track of added destinations
  _reqStreams[stream].push_back(destination);
}

void RemoteInterface::deleteDestinationFromStream(const string& stream, const string& destination)
{
  checkStreamTypeAvailable(stream);

  // do delete request on respective url (no parameters needed for this simple service call)
  cpr::Url url = cpr::Url{ _baseUrl + "/datastreams/" + stream };
  auto del = cpr::Delete(url, cpr::Timeout{ _timeoutCurl }, cpr::Parameters{ { "destination", destination } });
  handleCPRResponse(del);

  // delete destination also from list of requested streams
  auto& destinations = _reqStreams[stream];
  auto found = find(destinations.begin(), destinations.end(), destination);
  if (found != destinations.end())
    destinations.erase(found);
}

void RemoteInterface::deleteDestinationsFromStream(const string& stream, const list<string>& destinations)
{
  checkStreamTypeAvailable(stream);

  // with newer image versions this is the most efficent way, i.e. only one call
  if (_visardVersion >= 1.600001) {

    // do delete request on respective url; list of destinationas are given as body
    json js_destinations = json::array();
    for (const auto& dest: destinations)
    {
      js_destinations.push_back(dest);
    }
    json js_args;
    js_args["destination"] = js_destinations;
    cpr::Url url = cpr::Url{ _baseUrl + "/datastreams/" + stream };
    auto del = cpr::Delete(url, cpr::Timeout{ _timeoutCurl }, cpr::Body{ js_args.dump()},
                            cpr::Header{ { "Content-Type", "application/json" } });
    handleCPRResponse(del);

  // with older image versions we have to work around and do several calls
  } else {
    for (const auto& dest : destinations)
    {
      // do delete request on respective url; destination is given as query param
      cpr::Url url = cpr::Url{ _baseUrl + "/datastreams/" + stream };
      auto del = cpr::Delete(url, cpr::Timeout{ _timeoutCurl }, cpr::Parameters{ { "destination", dest } });
      handleCPRResponse(del);
    }
  }

  // delete destination also from list of requested streams
  auto& reqDestinations = _reqStreams[stream];
  for (auto& destination : destinations)
  {
    auto found = find(reqDestinations.begin(), reqDestinations.end(), destination);
    if (found != reqDestinations.end())
    {
      reqDestinations.erase(found);
    }
  }
}

namespace
{
roboception::msgs::Trajectory toProtobufTrajectory(const json js)
{
  // TODO: find an automatic way to parse Messages from Json
  // * is possible with protobuf >= 3.0.x
  // * https://developers.google.com/protocol-buffers/docs/reference/cpp/google.protobuf.util.json_util
  roboception::msgs::Trajectory pbTraj;

  json::const_iterator js_it;
  if ((js_it = js.find("parent")) != js.end())
  {
    pbTraj.set_parent(js_it.value());
  }
  if ((js_it = js.find("name")) != js.end())
  {
    pbTraj.set_name(js_it.value());
  }
  if ((js_it = js.find("producer")) != js.end())
  {
    pbTraj.set_producer(js_it.value());
  }
  if ((js_it = js.find("timestamp")) != js.end())
  {
    pbTraj.mutable_timestamp()->set_sec(js_it.value()["sec"]);    // TODO: sec
    pbTraj.mutable_timestamp()->set_nsec(js_it.value()["nsec"]);  // TODO: nsec
  }
  for (const auto& js_pose : js["poses"])
  {
    auto pbPose = pbTraj.add_poses();
    auto pbTime = pbPose->mutable_timestamp();
    pbTime->set_sec(js_pose["timestamp"]["sec"]);    // TODO: sec
    pbTime->set_nsec(js_pose["timestamp"]["nsec"]);  // TODO: nsec
    auto pbPosition = pbPose->mutable_pose()->mutable_position();
    pbPosition->set_x(js_pose["pose"]["position"]["x"]);
    pbPosition->set_y(js_pose["pose"]["position"]["y"]);
    pbPosition->set_z(js_pose["pose"]["position"]["z"]);
    auto pbOrientation = pbPose->mutable_pose()->mutable_orientation();
    pbOrientation->set_x(js_pose["pose"]["orientation"]["x"]);
    pbOrientation->set_y(js_pose["pose"]["orientation"]["y"]);
    pbOrientation->set_z(js_pose["pose"]["orientation"]["z"]);
    pbOrientation->set_w(js_pose["pose"]["orientation"]["w"]);
  }
  return pbTraj;
}
}

roboception::msgs::Trajectory RemoteInterface::getSlamTrajectory(const TrajectoryTime& start, const TrajectoryTime& end, unsigned int timeout_ms)
{
  // convert time specification to json obj
  json js_args, js_time, js_start_time, js_end_time;
  js_start_time["sec"] = start.getSec();
  js_start_time["nsec"] = start.getNsec();
  js_end_time["sec"] = end.getSec();
  js_end_time["nsec"] = end.getNsec();
  js_args["args"]["start_time"] = js_start_time;
  js_args["args"]["end_time"] = js_end_time;
  if (start.isRelative())
    js_args["args"]["start_time_relative"] = true;
  if (end.isRelative())
    js_args["args"]["end_time_relative"] = true;

  // get request on slam module
  cpr::Url url = cpr::Url{ _baseUrl + "/nodes/rc_slam/services/get_trajectory" };
  auto get = cpr::Put(url, cpr::Timeout{ (int32_t)timeout_ms }, cpr::Body{ js_args.dump() },
                      cpr::Header{ { "Content-Type", "application/json" } });
  handleCPRResponse(get);

  auto js = json::parse(get.text)["response"]["trajectory"];
  return toProtobufTrajectory(js);
}

DataReceiver::Ptr RemoteInterface::createReceiverForStream(const string& stream, const string& destInterface,
                                                           unsigned int destPort)
{
  checkStreamTypeAvailable(stream);

  // figure out local inet address for streaming
  string destAddress;
  if (!getThisHostsIP(destAddress, _visardAddrs, destInterface))
  {
    stringstream msg;
    msg << "Could not infer a valid IP address "
           "for this host as the destination of the stream! "
           "Given network interface specification was '"
        << destInterface << "'.";
    throw invalid_argument(msg.str());
  }

  // create data receiver with port as specified
  DataReceiver::Ptr receiver = TrackedDataReceiver::create(destAddress, destPort, stream, shared_from_this());

  // do REST-API call requesting a UDP stream from rc_visard device
  string destination = destAddress + ":" + to_string(destPort);
  addDestinationToStream(stream, destination);

  // waiting for first message; we set a long timeout for receiving data
  unsigned int initialTimeOut = 5000;
  receiver->setTimeout(initialTimeOut);
  if (!receiver->receive(_protobufMap[stream]))
  {
    throw UnexpectedReceiveTimeout(initialTimeOut);
    //    stringstream msg;
    //    msg << "Did not receive any data within the last "
    //        << initialTimeOut << " ms. "
    //        << "Either rc_visard does not seem to send the data properly "
    //                "(is rc_dynamics module running?) or you seem to have serious "
    //                "network/connection problems!";
    //    throw runtime_error(msg.str());
  }

  // stream established, prepare everything for normal pose receiving
  receiver->setTimeout(100);
  return receiver;
}

void RemoteInterface::cleanUpRequestedStreams()
{
  // for each stream type stop all previously requested streams
  for (auto const& s : _reqStreams)
  {
    if (!s.second.empty())
    {
      deleteDestinationsFromStream(s.first, s.second);
    }
  }
}

void RemoteInterface::checkStreamTypeAvailable(const string& stream)
{
  auto found = find(_availStreams.begin(), _availStreams.end(), stream);
  if (found == _availStreams.end())
  {
    stringstream msg;
    msg << "Stream of type '" << stream << "' is not available on rc_visard " << _visardAddrs;
    throw invalid_argument(msg.str());
  }
}
}
}
