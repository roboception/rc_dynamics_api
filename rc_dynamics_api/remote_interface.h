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

#ifndef RC_DYNAMICS_API_REMOTEINTERFACE_H
#define RC_DYNAMICS_API_REMOTEINTERFACE_H

#include <string>
#include <list>
#include <memory>
#include <iostream>

#include <netinet/in.h>

#include "roboception/msgs/frame.pb.h"
#include "roboception/msgs/dynamics.pb.h"
#include "roboception/msgs/imu.pb.h"

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
 *      Therefore, it is highly important that a RemoteInterface is destructed
 *      properly.
 *      In order to do so it, is recommended to wrap method calls of
 *      RemoteInterface objects with try-catch-blocks as they might throw
 *      exceptions and therefore avoid proper destruction of the object.
 */
class RemoteInterface : public std::enable_shared_from_this<RemoteInterface>
{
  public:

    using Ptr = std::shared_ptr<RemoteInterface>;

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
     * Sets rc_dynamics module to running state.
     *
     * @param flagRestart do a restart, if enable==true and rc_dynamics module was already running
     */
    void start(bool flagRestart = false);

    /**
     * Stops rc_dynamics module
     */
    void stop();

    /**
     * Checks state of rc_dynamics module (running, stopped, ...)
     *
     * @return the current rc_dynamics state
     */
    State getState();

    /**
     * Returns a list all available streams on rc_visard
     * @return
     */
    std::list<std::string> getAvailableStreams();

    /**
     * Returns the name of the protobuf message class that corresponds to a
     * given data stream and is required for de-serializing the respective
     * messages.
     *
     * @param stream a specific rc_dynamics data stream (e.g. "pose", "pose_rt" or "dynamics")
     * @return the corresponding protobuf message type as string (e.g. "Frame" or "Dynamics")
     */
    std::string getPbMsgTypeOfStream(const std::string &stream);


    /**
     * Returns a list of all destinations registered to the specified
     * rc_dynamics stream.
     * Streams here are represented as their destinations using IP address and
     * port number.
     *
     * @param stream a specific rc_dynamics data stream (e.g. "pose" or "dynamics")
     * @return list of destinations of represented as strings, e.g. "192.168.0.1:30000"
     */
    std::list<std::string> getDestinationsOfStream(const std::string &stream);


    /**
     * Adds a destination to a stream, i.e. request rc_visard to stream data of
     * the specified type to the given destination.
     *
     * @param stream stream type, e.g. "pose", "pose_rt" or "dynamics"
     * @param destination string-represented destination of the data stream, e.g. "192.168.0.1:30000"
     */
    void addDestinationToStream(const std::string &stream,
                                const std::string &destination);

    /**
     * Deletes a destination from a stream, i.e. request rc_visard to stop
     * streaming data of the specified type to the given destination.
     *
     * @param stream stream type, e.g. "pose", "pose_rt" or "dynamics"
     * @param destination string-represented destination of the data stream, e.g. "192.168.0.1:30000"
     */
    void deleteDestinationFromStream(const std::string &stream,
                                     const std::string &destination);

    /**
     * Deletes all destinations from a stream.
     *
     * @param stream stream type, e.g. "pose", "pose_rt" or "dynamics"
     */
    void deleteAllDestinationsFromStream(const std::string &stream);

    /**
     * Convenience method that automatically
     *
     *  1) creates a data receiver (including binding socket to a local network interface)
     *  2) adds a destination to the respective stream on rc_visard device
     *  3) waits/checks for the stream being established
     *  4) (removes the destination automatically from rc_visard device if data receiver is no longer used)
     *
     * Stream can only be established successfully if rc_dynamics module is running on
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
    DataReceiver::Ptr
    createReceiverForStream(const std::string &stream,
                            const std::string &destInterface = "",
                            unsigned int destPort = 0);


  protected:

    static std::map<std::string, RemoteInterface::Ptr> _remoteInterfaces;

    RemoteInterface(const std::string& rcVisardIP,
                    unsigned int requestsTimeout = 5000);

    void cleanUpRequestedStreams();
    void checkStreamTypeAvailable(const std::string& stream);

    std::string _visardAddrs;
    std::map<std::string, std::list<std::string>> _reqStreams;
    std::list<std::string> _availStreams;
    std::map<std::string, std::string> _protobufMap;
    std::string _baseUrl;
    int _timeoutCurl;
};

}
}


#endif //RC_DYNAMICS_API_REMOTEINTERFACE_H
