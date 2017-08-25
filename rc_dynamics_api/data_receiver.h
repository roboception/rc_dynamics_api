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


#ifndef RC_DYNAMICS_API_DATASTREAM_H
#define RC_DYNAMICS_API_DATASTREAM_H

#include <memory>
#include <sstream>

#include <netinet/in.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <string.h>

namespace rc
{
namespace dynamics
{

template<class ProtobufType>
class DataReceiver
        : public std::enable_shared_from_this<DataReceiver<ProtobufType>>
{
  public:

    using Ptr = std::shared_ptr<DataReceiver>;

    /**
     * Creates a data receiver of specific data type bound to the user-given
     * IP address and port number.
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
        throw std::runtime_error(
                "Error during setting receive timeout: setsockopt() returned with errno " +
                std::to_string(errno));
      }
    }


    /**
     * Receives the next item from data stream. This method blocks until the
     * next data is available or when it runs into user-specified timeout (see
     * setTimeout(...)).
     *
     * @return a valid rc_dynamics data from the stream, or NULL if timeout
     */
    virtual std::shared_ptr<ProtobufType> receive()
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
          return NULL;
        }
        else
        {
          throw std::runtime_error(
                  "Error during socket recvfrom: errno " + std::to_string(e));
        }
      }

      // parse msgs as probobuf
      auto protoObj = std::shared_ptr<ProtobufType>(new ProtobufType());
      protoObj->ParseFromArray(_buffer, msg_size);
      return protoObj;
    }


  protected:

    DataReceiver(std::string ip_address, unsigned int &port)
    {
      // may result in weird errors when not initialized properly
      _recvtimeout.tv_sec = 0;
      _recvtimeout.tv_usec = 1000 * 10;

      // open socket for UDP listening
      _sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
      if (_sockfd < 0)
      {
        std::stringstream msg;
        msg << "Error creating socket: errno " << errno;
        throw std::runtime_error(msg.str());
      }

      // bind socket to IP address and port number
      struct sockaddr_in myaddr;
      myaddr.sin_family = AF_INET;
      inet_aton(ip_address.c_str(), &myaddr.sin_addr); // set IP addrs
      myaddr.sin_port = htons(port);
      if (bind(_sockfd, (sockaddr *) &myaddr, sizeof(sockaddr)) < 0)
      {
        std::stringstream msg;
        msg << "Error binding socket to port number " << port
            << ": errno " << errno;
        throw std::invalid_argument(msg.str());
      }

      // if socket was bound to arbitrary port, we need to figure out to which
      // port number
      if (port == 0)
      {
        socklen_t len = sizeof(myaddr);
        if (getsockname(_sockfd, (struct sockaddr *) &myaddr, &len) < 0)
        {
          close(_sockfd);
          std::stringstream msg;
          msg << "Error getting socket name to figure out port number: errno: "
              << errno;
          throw std::invalid_argument(msg.str());
        }
        port = ntohs(myaddr.sin_port);
      }
    }

    int _sockfd;
    struct timeval _recvtimeout;
    char _buffer[512];
};

}
}

#endif //RC_DYNAMICS_API_DATASTREAM_H
