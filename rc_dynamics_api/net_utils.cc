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

#include "net_utils.h"

#include <string.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace rc {

using namespace std;

uint32_t IPToUInt(const std::string ip) {
  int a, b, c, d;
  uint32_t addr = 0;

  if (sscanf(ip.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d) != 4)
    return 0;

  addr = a << 24;
  addr |= b << 16;
  addr |= c << 8;
  addr |= d;
  return addr;
}

bool isIPInRange(const std::string ip, const std::string network, const std::string mask) {
  uint32_t ip_addr = IPToUInt(ip);
  uint32_t network_addr = IPToUInt(network);
  uint32_t mask_addr = IPToUInt(mask);

  uint32_t net_lower = (network_addr & mask_addr);
  uint32_t net_upper = (net_lower | (~mask_addr));

  if (ip_addr >= net_lower &&
      ip_addr <= net_upper)
    return true;
  return false;
}


bool getThisHostsIP(string &thisHostsIP,
                    const string &otherHostsIP,
                    const string &networkInterface)
{
  // scan all network interfaces (for the desired one)
  struct ifaddrs *ifAddrStruct = NULL;
  struct ifaddrs *ifa = NULL;
  void *tmpAddrPtr = NULL;
  getifaddrs(&ifAddrStruct);
  bool foundValid = false;
  char addressBuffer[INET_ADDRSTRLEN], netmaskBuffer[INET_ADDRSTRLEN];
  for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
  {
    // check if any valid IP4 address
    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
      continue;

    tmpAddrPtr = &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
    inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);

    // if user specified desired network interface
    if (networkInterface != "")
    {
      // check if this is the desired
      if (strcmp(networkInterface.c_str(), ifa->ifa_name) == 0)
      {
        foundValid = true;
        break;
      }
    }

      // we may use heuristics based on rc_visard's IP address
    else if (otherHostsIP != "")
    {
      tmpAddrPtr = &((struct sockaddr_in *) ifa->ifa_netmask)->sin_addr;
      inet_ntop(AF_INET, tmpAddrPtr, netmaskBuffer, INET_ADDRSTRLEN);
      if (isIPInRange(addressBuffer, otherHostsIP, netmaskBuffer))
      {
        foundValid = true;
        break;
      }
    }

      // we may use some very basic heuristics to find a 'good' interface, i.e.
      // simply checking the type of the interface by its name for being an
      // ethernet or wifi adaper
    else {
      if (  (strncmp("eth", ifa->ifa_name, 3) == 0) ||
            (strncmp("en", ifa->ifa_name, 2) == 0)  ||
            (strncmp("wl", ifa->ifa_name, 2) == 0)  )
      {
        foundValid = true;
        break;
      }
    }
  }

  if (foundValid)
    thisHostsIP = string(addressBuffer);
  return foundValid;
}

bool isValidIPAddress(const std::string &ip)
{
  // use inet_pton to check if given string is a valid IP address
  static struct sockaddr_in sa;
  return TEMP_FAILURE_RETRY(inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr))) ==
         1;
}

}
