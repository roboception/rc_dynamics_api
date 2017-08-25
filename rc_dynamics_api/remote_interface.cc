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


#include "remote_interface.h"

#include "net_utils.h"

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
    throw std::runtime_error(toString(r));
  }
}

map<string, RemoteInterface::Ptr> RemoteInterface::_remoteInterfaces = map<string,RemoteInterface::Ptr>();

RemoteInterface::Ptr
RemoteInterface::create(const std::string &rcVisardInetAddrs,
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


RemoteInterface::RemoteInterface(std::string rcVisardInetAddrs,
                                 unsigned int requestsTimeout) :
        _visardAddrs(rcVisardInetAddrs),
        _baseUrl("http://" + _visardAddrs + "/api/v1"),
        _timeoutCurl(requestsTimeout)
{
  _reqStreams.clear();
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
          _baseUrl + "/nodes/rc_stereo_ins/services/" + serviceToCall};
  auto put = cpr::Put(url, cpr::Timeout{_timeoutCurl});
  handleCPRResponse(put);
}


void RemoteInterface::stop()
{
  // do put request on respective url (no parameters needed for this simple service call)
  cpr::Url url = cpr::Url{_baseUrl + "/nodes/rc_stereo_ins/services/stop"};
  auto put = cpr::Put(url, cpr::Timeout{_timeoutCurl});
  handleCPRResponse(put);
}


RemoteInterface::State RemoteInterface::getState()
{
  // do get request on respective url (no parameters needed for this simple service call)
  cpr::Url url = cpr::Url{_baseUrl + "/nodes/rc_stereo_ins/status"};
  auto get = cpr::Get(url, cpr::Timeout{_timeoutCurl});
  handleCPRResponse(get);

  // parse text of response into json object
  auto j = json::parse(get.text);
  if (j["status"].get<std::string>() == "running")
    return State::RUNNING;
  else
    return State::STOPPED;
}


list<string> RemoteInterface::getDestinationsOfStream(const string &type)
{
  list<string> destinations;

  // do get request on respective url (no parameters needed for this simple service call)
  cpr::Url url = cpr::Url{_baseUrl + "/datastreams/" + type};
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


void RemoteInterface::addDestinationToStream(const string &type,
                                             const string &destination)
{
  // do put request on respective url (no parameters needed for this simple service call)
  cpr::Url url = cpr::Url{_baseUrl + "/datastreams/" + type};
  auto put = cpr::Put(url, cpr::Timeout{_timeoutCurl},
                      cpr::Parameters{{"destination", destination}});
  handleCPRResponse(put);

  // keep track of added destinations
  _reqStreams[type].push_back(destination);
}


void RemoteInterface::deleteDestinationFromStream(const string &type,
                                                  const string &destination)
{
  // do delete request on respective url (no parameters needed for this simple service call)
  cpr::Url url = cpr::Url{_baseUrl + "/datastreams/" + type};
  auto del = cpr::Delete(url, cpr::Timeout{_timeoutCurl},
                         cpr::Parameters{{"destination", destination}});
  handleCPRResponse(del);

  // delete destination also from list of requested streams
  auto& destinations = _reqStreams[type];
  auto found = find(destinations.begin(), destinations.end(), destination);
  if (found != destinations.end())
    destinations.erase(found);
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
    catch (std::exception &e)
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
        } catch (std::exception &e) {
          cerr << "[RemoteInterface] Could not delete destination "
               << activeStream << " from " << s.first << " stream: "
               << e.what() << endl;
        }
      }
    }

  }
}


}
}