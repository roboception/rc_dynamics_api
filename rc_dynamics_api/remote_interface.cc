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

#include <cpr/cpr.h>
#include <json.hpp>


using namespace std;
using json = nlohmann::json;

namespace rc
{
namespace dynamics
{

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

    static shared_ptr<TrackedDataReceiver>
    create(const string &ip_address, unsigned int &port,
           const string &stream,
           shared_ptr<RemoteInterface> creator)
    {
      return shared_ptr<TrackedDataReceiver>(
              new TrackedDataReceiver(ip_address, port, stream, creator));
    }

    virtual ~TrackedDataReceiver()
    {
      try
      {
        _creator->deleteDestinationFromStream(_stream, _dest);
      }
      catch (exception &e)
      {
        cerr
                << "[TrackedDataReceiver] Could not remove my destination "
                << _dest << " for stream type " << _stream
                << " from rc_visard: "
                << e.what() << endl;
      }
    }

  protected:

    TrackedDataReceiver(const string &ip_address, unsigned int &port,
                        const string &stream,
                        shared_ptr<RemoteInterface> creator)
            : DataReceiver(ip_address, port),
              _dest(ip_address + ":" + to_string(port)),
              _stream(stream),
              _creator(creator)
    {}

    string _dest, _stream;
    shared_ptr<RemoteInterface> _creator;
};

// map to store already created RemoteInterface objects
map<string, RemoteInterface::Ptr> RemoteInterface::_remoteInterfaces = map<string,RemoteInterface::Ptr>();

RemoteInterface::Ptr
RemoteInterface::create(const string &rcVisardInetAddrs,
                        unsigned int requestsTimeout)
{
  // check if interface is already opened
  auto found = RemoteInterface::_remoteInterfaces.find(rcVisardInetAddrs);
  if (found != RemoteInterface::_remoteInterfaces.end())
  {
    return found->second;
  }

  // if not, create it
  auto newRemoteInterface = Ptr(
          new RemoteInterface(rcVisardInetAddrs, requestsTimeout));
  RemoteInterface::_remoteInterfaces[rcVisardInetAddrs] = newRemoteInterface;

  return newRemoteInterface;
}


RemoteInterface::RemoteInterface(const string &rcVisardIP,
                                 unsigned int requestsTimeout) :
        _visardAddrs(rcVisardIP),
        _baseUrl("http://" + _visardAddrs + "/api/v1"),
        _timeoutCurl(requestsTimeout)
{
  _reqStreams.clear();
  _protobufMap.clear();

  // check if given string is a valid IP address
  if (!isValidIPAddress(rcVisardIP))
  {
    throw invalid_argument("Given IP address is not a valid address: "
                           + rcVisardIP);
  }

  // initial connection to rc_visard and get streams, i.e. do get request on
  // respective url (no parameters needed for this simple service call)
  cpr::Url url = cpr::Url{_baseUrl + "/datastreams"};
  auto get = cpr::Get(url, cpr::Timeout{_timeoutCurl});
  handleCPRResponse(get);

  // parse text of response into json object
  auto j = json::parse(get.text);
  for (const auto& stream : j) {
    _availStreams.push_back(stream["name"]);
    _protobufMap[stream["name"]] = stream["protobuf"];
  }
}


RemoteInterface::~RemoteInterface()
{
  cleanUpRequestedStreams();
  for (const auto& s : _reqStreams)
  {
    if (s.second.size() > 0)
    {
      cerr << "[RemoteInterface] Could not stop all previously requested"
              " streams of type " << s.first <<  " on rc_visard. Please check "
              "device manually"
              " (" << _baseUrl << "/datastreams/" << s.first << ")"
              " for not containing any of the following legacy streams and"
              " delete them otherwise, e.g. using the swagger UI ("
           << "http://" + _visardAddrs + "/api/swagger/)"
           << ": "
           << toString(s.second)
           << endl;
    }
  }
}


void RemoteInterface::start(bool flagRestart)
{
  // do put request on respective url (no parameters needed for this simple service call)
  string serviceToCall = (flagRestart) ? "restart" : "start";
  cpr::Url url = cpr::Url{
          _baseUrl + "/nodes/rc_dynamics/services/" + serviceToCall};
  auto put = cpr::Put(url, cpr::Timeout{_timeoutCurl});
  handleCPRResponse(put);
}


void RemoteInterface::stop()
{
  // do put request on respective url (no parameters needed for this simple service call)
  cpr::Url url = cpr::Url{_baseUrl + "/nodes/rc_dynamics/services/stop"};
  auto put = cpr::Put(url, cpr::Timeout{_timeoutCurl});
  handleCPRResponse(put);
}


RemoteInterface::State RemoteInterface::getState()
{
  // do get request on respective url (no parameters needed for this simple service call)
  cpr::Url url = cpr::Url{_baseUrl + "/nodes/rc_dynamics/status"};
  auto get = cpr::Get(url, cpr::Timeout{_timeoutCurl});
  handleCPRResponse(get);

  // parse text of response into json object
  auto j = json::parse(get.text);
  if (j["status"].get<string>() == "running")
    return State::RUNNING;
  else
    return State::STOPPED;
}

list<string> RemoteInterface::getAvailableStreams()
{
  return _availStreams;
}


string RemoteInterface::getPbMsgTypeOfStream(const string &stream)
{
  checkStreamTypeAvailable(stream);
  return _protobufMap[stream];
}


list<string> RemoteInterface::getDestinationsOfStream(const string &stream)
{
  checkStreamTypeAvailable(stream);

  list<string> destinations;

  // do get request on respective url (no parameters needed for this simple service call)
  cpr::Url url = cpr::Url{_baseUrl + "/datastreams/" + stream};
  auto get = cpr::Get(url, cpr::Timeout{_timeoutCurl});
  handleCPRResponse(get);

  // parse result as json
  auto j = json::parse(get.text);
  for (auto dest : j["destinations"])
  {
    destinations.push_back(dest.get<string>());
  }
  return destinations;
}


void RemoteInterface::addDestinationToStream(const string &stream,
                                             const string &destination)
{
  checkStreamTypeAvailable(stream);

  // do put request on respective url (no parameters needed for this simple service call)
  cpr::Url url = cpr::Url{_baseUrl + "/datastreams/" + stream};
  auto put = cpr::Put(url, cpr::Timeout{_timeoutCurl},
                      cpr::Parameters{{"destination", destination}});
  handleCPRResponse(put);

  // keep track of added destinations
  _reqStreams[stream].push_back(destination);
}


void RemoteInterface::deleteDestinationFromStream(const string &stream,
                                                  const string &destination)
{
  checkStreamTypeAvailable(stream);

  // do delete request on respective url (no parameters needed for this simple service call)
  cpr::Url url = cpr::Url{_baseUrl + "/datastreams/" + stream};
  auto del = cpr::Delete(url, cpr::Timeout{_timeoutCurl},
                         cpr::Parameters{{"destination", destination}});
  handleCPRResponse(del);

  // delete destination also from list of requested streams
  auto& destinations = _reqStreams[stream];
  auto found = find(destinations.begin(), destinations.end(), destination);
  if (found != destinations.end())
    destinations.erase(found);
}


DataReceiver::Ptr
RemoteInterface::createReceiverForStream(const string &stream,
                                         const string &destInterface,
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
            "Given network interface specification was '" << destInterface
        << "'.";
    throw invalid_argument(msg.str());
  }

  // create data receiver with port as specified
  DataReceiver::Ptr receiver = TrackedDataReceiver::create(destAddress,
                                                           destPort, stream,
                                                           shared_from_this());

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
  // for each stream type check currently running streams on rc_visard device
  for (auto const &s : _reqStreams)
  {
    // get a list of currently active streams of this type on rc_visard device
    list<string> rcVisardsActiveStreams;
    try
    {
      rcVisardsActiveStreams = getDestinationsOfStream(s.first);
    }
    catch (exception &e)
    {
      cerr << "[RemoteInterface] Could not get list of active " << s.first
           << " streams for cleaning up previously requested streams: "
           << e.what() << endl;
      continue;
    }

    // try to stop all previously requested streams of this type
    for (auto activeStream : rcVisardsActiveStreams)
    {
      auto found = find(s.second.begin(), s.second.end(), activeStream);
      if (found != s.second.end())
      {
        try {
          deleteDestinationFromStream(s.first, activeStream);
        } catch (exception &e) {
          cerr << "[RemoteInterface] Could not delete destination "
               << activeStream << " from " << s.first << " stream: "
               << e.what() << endl;
        }
      }
    }

  }
}

void RemoteInterface::checkStreamTypeAvailable(const string& stream) {
  auto found = find(_availStreams.begin(), _availStreams.end(), stream);
  if (found == _availStreams.end())
  {
    stringstream msg;
    msg << "Stream of type '" << stream << "' is not available on rc_visard "
        << _visardAddrs;
    throw invalid_argument(msg.str());
  }
}


}
}
