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


#ifndef RC_DEVICE_POSE_INTERFACE_H
#define RC_DEVICE_POSE_INTERFACE_H

#include <string>
#include <list>
#include <memory>

#include <netinet/in.h>

#include "roboception/msgs/pose_stamped.pb.h"

namespace rc
{
/**
 * Simple remote interface to access the VINS functionalities on the rc_visard.
 */
class VINSRemoteInterface {
  public:

    /**
     * PoseType
     */
    using PoseType = roboception::msgs::PoseStamped;

    enum State {
      RUNNING,
      STOPPED
    };

    /**
     * Creates a local instance of rc_visard's remote pose interface
     *
     * @param rcVisardInetAddrs rc_visard's inet address as string, e.g "192.168.0.12"
     * @param rcVisardSubnet rc_visard's subnet mask as string, e.g "255.255.255.0"
     * @param requestsTimeout timeout in [ms] for doing REST-API calls
     */
    VINSRemoteInterface(std::string rcVisardInetAddrs, std::string rcVisardSubnet, unsigned int requestsTimeout = 5000);
    ~VINSRemoteInterface();

    /**
     * Sets VINS module to running state
     * @param flagRestart do a restart, if enable==true and VINS module was already running
     */
    void start(bool flagRestart = false);

    /**
     * Stops VINS module
     */
    void stop();

    /**
     * Checks state of VINS module (running, stopped, ...)
     * @return the current VINS state
     */
    State getState();

    /**
     * Returns a list of all streams which are actively running on rc_visard.
     * Streams here are represented as their destinations using inet address and
     * port number, e.g. "192.168.0.1:30000"
     * @return list of active streams represented as strings
     */
    std::list<std::string> getActiveStreams();

    /**
     * Initializes a pose stream from rc_visard to this host using the given
     * destination inet address and destination port. This includes
     *  1) binding a socket to a local network interface
     *  2) checking if/which interface is reachable from rc_visard device
     *  3) requesting to start a pose stream on rc_visard device
     *  4) waiting/checking for the stream being established.
     *
     * If address is unspecified (or "") this host's network interfaces are
     * scanned to find a suitable inet address among those. Similar, if port
     * number is unspecified (or 0) it will be assigned arbitrarily as
     * available by network interface layer.
     *
     * Stream can only be established successfully if VINS module is running on
     * rc_visard, see getState() and start(...) methods.
     *
     * @param destAdrrs empty or this hosts inet address as string, e.g "192.168.0.1"
     * @param destPort 0 or this hosts port number
     * @return true, if stream could be initialized successfully
     */
    bool initPoseReceiver(std::string destAdrrs = "", unsigned int destPort = 0);

    /**
     * Antagonist method to initPoseReceiver(), i.e. stops background processes
     * which were created to stream poses.
     *
     * There is no actual need to call this method. It will be called
     * automatically when calling initPoseReceiver(...) again, or this object
     * is being destructed.
     */
    void destroyPoseReceiver();

    /**
     * Receives the next pose from pose stream. This method blocks until the
     * next pose is available or when it runs into user-specified timeout (see
     * setReceiveTimeout(...)).
     *
     * Method initPoseReceiver(...) has to be called first!
     *
     * @return a valid pose from the stream, or NULL if timeout
     */
    std::shared_ptr<PoseType> receivePose();

    /**
     * Sets a user-specified timeout for the receivePose() method.
     *
     * @param ms timeout in milliseconds
     */
    void setReceiveTimeout(unsigned int ms);

  protected:
    void cleanUpRequestedStreams();

    std::string _visardAddrs, _visardSubnet;

    bool _streamInitialized;
    std::list<std::string> _reqStreams;

    std::string _baseUrl;
    int _timeoutCurl;
    std::string _lastResponse;

    int _sockfd;
    struct timeval _recvtimeout;
    char _buffer[256];   // TODO: correct buffer size??
    struct sockaddr_in _otheraddr;
    socklen_t _otheraddrLength;
};

}


#endif //RC_DEVICE_POSE_INTERFACE_H
