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


#ifndef RC_DYNAMICS_API_REMOTEINTERFACE_H
#define RC_DYNAMICS_API_REMOTEINTERFACE_H

#include <string>
#include <list>
#include <memory>
#include <iostream>

#include <netinet/in.h>

#include "roboception/msgs/frame.pb.h"

#include "data_receiver.h"
#include "net_utils.h"

namespace rc
{
namespace dynamics
{
/**
 * Simple remote interface to access the dynamic state estimates
 * of an rc_visard device as data streams.
 *
 * It offers methods to
 *  * interface the rc_dynamics module on the rc_visard device, i.e. starting,
 *      stopping, and checking its state
 *  * manage data streams, i.e. adding, deleting, and checking destinations of
 *      the streams
 *  * an easy-to-use convenience function to directly start listening to a
 *      specific data stream (see createReceiverForStream())
 *
 *  NOTE: For convenience, a RemoteInterface object automatically keeps track
 *      of all data stream destinations requested by itself on the rc_visard
 *      device, and deletes them again when it is going to be destructed.
 *      Therefore, it is highly important that a RemoteInterface is desctucted
 *      properly.
 *      In order to do so it, is highly recommended to wrap method calls of
 *      RemoteInterface objects with try-catch-blocks as they might throw
 *      exceptions and therefore avoid proper destruction of the object.
 */
class RemoteInterface : public std::enable_shared_from_this<RemoteInterface>
{
  public:

    using Ptr = std::shared_ptr<RemoteInterface>;

    /**
     * PoseType
     */
    using PoseType = roboception::msgs::Frame;

    enum State
    {
      RUNNING,
      STOPPED
    };


    /**
     * Creates a local instance of rc_visard's remote pose interface
     *
     * @param rcVisardIP rc_visard's inet address as string, e.g "192.168.0.12"
     * @param requestsTimeout timeout in [ms] for doing REST-API calls
     */
    static Ptr create(const std::string &rcVisardIP,
               unsigned int requestsTimeout = 5000);

    virtual ~RemoteInterface();

    /**
     * Sets VINS module to running state.
     *
     * @param flagRestart do a restart, if enable==true and VINS module was already running
     */
    void start(bool flagRestart = false);

    /**
     * Stops VINS module
     */
    void stop();

    /**
     * Checks state of VINS module (running, stopped, ...)
     *
     * @return the current VINS state
     */
    State getState();


    /**
     * Returns a list of all destinations registered to the specified
     * rc_dynamics stream.
     * Streams here are represented as their destinations using IP address and
     * port number.
     *
     * @param type a specific rc_dynamics data stream (e.g. "pose" or "dynamics")
     * @return list of destinations of represented as strings, e.g. "192.168.0.1:30000"
     */
    std::list<std::string> getDestinationsOfStream(const std::string &type);


    /**
     * Adds a destination to a stream, i.e. request rc_visard to stream data of
     * the specified type to the given destination.
     *
     * @param type stream type, e.g. "pose", "pose_rt" or "dynamics"
     * @param destination string-represented destination of the data stream, e.g. "192.168.0.1:30000"
     */
    void addDestinationToStream(const std::string &type,
                                const std::string &destination);

    /**
     * Deletes a destination from a stream, i.e. request rc_visard to stop
     * streaming data of the specified type to the given destination.
     *
     * @param type stream type, e.g. "pose", "pose_rt" or "dynamics"
     * @param destination string-represented destination of the data stream, e.g. "192.168.0.1:30000"
     */
    void deleteDestinationFromStream(const std::string &type,
                                     const std::string &destination);

    /**
     * Deletes all destinations from a stream.
     *
     * @param type stream type, e.g. "pose", "pose_rt" or "dynamics"
     */
    void deleteAllDestinationsFromStream(const std::string &type);

    /**
     * Convenience method that automatically
     *
     *  1) creates a data receiver (including binding socket to a local network interface)
     *  2) adds a destination to the respective stream on rc_visard device
     *  3) waits/checks for the stream being established
     *  4) (removes the destination automatically from rc_visard device if data receiver is no longer used)
     *
     * Stream can only be established successfully if VINS module is running on
     * rc_visard, see getState() and start(...) methods.
     *
     *
     * If desired interface for receiving is unspecified (or "") this host's
     * network interfaces are scanned to find a suitable IP address among those.
     * Similar, if port number is unspecified (or 0) it will be assigned
     * arbitrarily as available by network interface layer.
     *
     * @param destInterface empty or one of this hosts network interfaces, e.g. "eth0"
     * @param destPort 0 or this hosts port number
     * @return true, if stream could be initialized successfully
     */
    template<class ProtobufType>
    typename DataReceiver<ProtobufType>::Ptr
    createReceiverForStream(const std::string &type,
                            const std::string &destInterface = "",
                            unsigned int destPort = 0)
    {
      // figure out local inet address for streaming
      std::string destAddress;
      if (!rc::getThisHostsIP(destAddress, _visardAddrs, destInterface))
      {
        std::stringstream msg;
        msg << "Could not infer a valid IP address "
                "for this host as the destination of the stream! "
                "Given network interface specification was '" << destInterface
            << "'.";
        throw std::invalid_argument(msg.str());
      }

      // create data receiver with port as specified
      typename DataReceiver<ProtobufType>::Ptr receiver =
              TrackedDataReceiver<ProtobufType>::create(destAddress, destPort,
                                                        type,
                                                        shared_from_this());

      // do REST-API call requesting a UDP stream from rc_visard device
      std::string destination = destAddress + ":" + std::to_string(destPort);
      addDestinationToStream(type, destination);

      // waiting for first message; we set a long timeout for receiving data
      unsigned int initialTimeOut = 5000;
      receiver->setTimeout(initialTimeOut);
      std::shared_ptr<ProtobufType> protoMsg = receiver->receive();
      if (!protoMsg)
      {
        std::stringstream msg;
        msg << "Did not receive any data within the last "
            << initialTimeOut << " ms. "
            << "Either rc_visard does not seem to send the data properly "
                    "(is rc_dynamics module running?) or you seem to have serious "
                    "network/connection problems!";
        throw std::runtime_error(msg.str());
      }

      // stream established, prepare everything for normal pose receiving
      receiver->setTimeout(100);
      return receiver;
    }


  protected:

    /**
     * Class for data stream receivers that are created by this
     * remote interface in order to keep track of created streams.
     *
     * @tparam ProtobufType
     */
    template<class ProtobufType>
    class TrackedDataReceiver : public DataReceiver<ProtobufType>
    {
      public:
        static std::shared_ptr<TrackedDataReceiver>
        create(const std::string &ip_address, unsigned int &port,
               const std::string &stream,
               std::shared_ptr<RemoteInterface> creator)
        {
          return std::shared_ptr<TrackedDataReceiver>(
                  new TrackedDataReceiver(ip_address, port, stream, creator));
        }

        virtual ~TrackedDataReceiver()
        {
          try
          {
            _creator->deleteDestinationFromStream(_stream, _dest);
          }
          catch (std::exception &e)
          {
            std::cerr
                    << "[TrackedDataReceiver] Could not remove my destination "
                    << _dest << " for stream type " << _stream
                    << " from rc_visard: "
                    << e.what() << std::endl;
          }
        }

      protected:

        TrackedDataReceiver(const std::string &ip_address, unsigned int &port,
                            const std::string &stream,
                            std::shared_ptr<RemoteInterface> creator)
                : DataReceiver<ProtobufType>(ip_address, port)
        {
          _dest = ip_address + ":" + std::to_string(port);
          _stream = stream;
          _creator = creator;
        }

        std::string _dest, _stream;
        std::shared_ptr<RemoteInterface> _creator;
    };

    static std::map<std::string, RemoteInterface::Ptr> _remoteInterfaces;

    RemoteInterface(std::string rcVisardInetAddrs,
                    unsigned int requestsTimeout = 5000);

    void cleanUpRequestedStreams();

    std::string _visardAddrs;
    std::map<std::string, std::list<std::string>> _reqStreams;
    std::string _baseUrl;
    int _timeoutCurl;
};

}
}


#endif //RC_DYNAMICS_API_REMOTEINTERFACE_H
