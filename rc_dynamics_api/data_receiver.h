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

#ifndef RC_DYNAMICS_API_DATASTREAM_H
#define RC_DYNAMICS_API_DATASTREAM_H

#include <memory>
#include <sstream>

#include <netinet/in.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <string.h>

#include "net_utils.h"
#include "socket_exception.h"

#include "roboception/msgs/frame.pb.h"
#include "roboception/msgs/dynamics.pb.h"
#include "roboception/msgs/imu.pb.h"

namespace rc
{
namespace dynamics
{


/**
 * A simple receiver object for handling data streamed by rc_visard's
 * rc_dynamics module.
 */
class DataReceiver
        : public std::enable_shared_from_this<DataReceiver>
{
  public:

    using Ptr = std::shared_ptr<DataReceiver>;

    /**
     * Creates a data receiver bound to the user-given IP address and port
     * number.
     *
     * For binding to an arbitrary port, the given port number might be 0. In
     * this case, the actually chosen port number is returned.
     *
     * @param ip_address IP address for receiving data
     * @param port port number for receiving data
     * @return
     */
    static Ptr create(std::string ip_address, unsigned int &port)
    {
      return Ptr(new DataReceiver(ip_address, port));
    }

    virtual ~DataReceiver()
    {
      close(_sockfd);
    }

    /**
     * Sets a user-specified timeout for the receivePose() method.
     *
     * @param ms timeout in milliseconds
     */
    virtual void setTimeout(unsigned int ms)
    {
      _recvtimeout.tv_sec = ms / 1000;
      _recvtimeout.tv_usec = (ms % 1000) * 1000;
      if (setsockopt(_sockfd, SOL_SOCKET, SO_RCVTIMEO,
                     (const char *) &_recvtimeout,
                     sizeof(struct timeval)) < 0)
      {
        throw SocketException("Error while setting receive timeout!", errno);
      }
    }


    /**
     * Receives the next message from data stream (template-parameter version)
     *
     * This method blocks until the next message is received and returns it as
     * specified by the template parameter PbMsgType, or when it runs into
     * user-specified timeout (see setTimeout(...)).
     *
     * NOTE: The specified PbMsgType *must match* the type with which the
     * received data was serialized during sending. Otherwise it will result in
     * undefined behaviour!
     *
     * @return the next rc_dynamics data stream message as PbMsgType, or NULL if timeout
     */
    template<class PbMsgType>
    std::shared_ptr<PbMsgType> receive()
    {
      // receive msg from socket; blocking call (timeout)
      int msg_size = TEMP_FAILURE_RETRY(
              recvfrom(_sockfd, _buffer, sizeof(_buffer), 0, NULL, NULL));
      if (msg_size < 0)
      {
        int e = errno;
        if (e == EAGAIN || e == EWOULDBLOCK)
        {
          // timeouts are allowed to happen, then return NULL pointer
          return nullptr;
        }
        else
        {
          throw SocketException("Error during socket recvfrom!", e);
        }
      }

      // parse msgs as probobuf
      auto pbMsg = std::shared_ptr<PbMsgType>(new PbMsgType());
      pbMsg->ParseFromArray(_buffer, msg_size);
      return pbMsg;
    }

    /**
     * Receives the next message from data stream (string-parameter version)
     *
     * This method blocks until the next message is available and returns it -
     * de-serialized as specified by the pbMsgType parameter - as a pb::Message
     * base class pointer, or until it runs into user-specified timeout (see
     * setTimeout(...)).
     *
     * NOTE: The specified PbMsgType *must match* the type with which the
     * received data was serialized during sending. Otherwise it will result in
     * undefined behaviour!
     *
     * @return the next rc_dynamics data stream message as a pb::Message base class pointer, or NULL if timeout
     */
    virtual std::shared_ptr<::google::protobuf::Message> receive(const std::string &pbMsgType)
    {
      auto found = _recv_func_map.find(pbMsgType);
      if (found == _recv_func_map.end())
      {
        std::stringstream msg;
        msg << "Unsupported protobuf message type '" << pbMsgType
            << "'. Only the following types are supported: ";
        for (auto const &p : _recv_func_map) msg << p.first << " ";
        throw std::invalid_argument(msg.str());
      }
      return _recv_func_map[pbMsgType]();
    }

  protected:

    DataReceiver(std::string ip_address, unsigned int &port)
    {
      // check if given string is a valid IP address
      if (!rc::isValidIPAddress(ip_address))
      {
        throw std::invalid_argument("Given IP address is not a valid address: "
                               + ip_address);
      }

      // may result in weird errors when not initialized properly
      _recvtimeout.tv_sec = 0;
      _recvtimeout.tv_usec = 1000 * 10;

      // open socket for UDP listening
      _sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
      if (_sockfd < 0)
      {
        throw SocketException("Error while creating socket!", errno);
      }

      // bind socket to IP address and port number
      struct sockaddr_in myaddr;
      myaddr.sin_family = AF_INET;
      inet_aton(ip_address.c_str(), &myaddr.sin_addr); // set IP addrs
      myaddr.sin_port = htons(port);
      if (bind(_sockfd, (sockaddr *) &myaddr, sizeof(sockaddr)) < 0)
      {
        throw SocketException("Error while binding socket!", errno);
      }

      // if socket was bound to arbitrary port, we need to figure out to which
      // port number
      if (port == 0)
      {
        socklen_t len = sizeof(myaddr);
        if (getsockname(_sockfd, (struct sockaddr *) &myaddr, &len) < 0)
        {
          close(_sockfd);
          throw SocketException("Error while getting socket name!", errno);
        }
        port = ntohs(myaddr.sin_port);
      }

      // register all known protobuf message types
      _recv_func_map[roboception::msgs::Frame::descriptor()->name()] = std::bind(
              &DataReceiver::receive<roboception::msgs::Frame>, this);
      _recv_func_map[roboception::msgs::Imu::descriptor()->name()] = std::bind(
              &DataReceiver::receive<roboception::msgs::Imu>, this);
      _recv_func_map[roboception::msgs::Dynamics::descriptor()->name()] = std::bind(
              &DataReceiver::receive<roboception::msgs::Dynamics>, this);
    }

    int _sockfd;
    struct timeval _recvtimeout;
    char _buffer[512];

    typedef std::map<std::string, std::function<std::shared_ptr<::google::protobuf::Message>()>> map_type;
    map_type _recv_func_map;
};

}
}

#endif //RC_DYNAMICS_API_DATASTREAM_H
