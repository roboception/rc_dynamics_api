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

#include "net_utils.h"

#include <stdio.h>

namespace rc {

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

}

