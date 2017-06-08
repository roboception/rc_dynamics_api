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


#ifndef RC_DEVICE_NET_UTILS_H
#define RC_DEVICE_NET_UTILS_H

#include <string>
#include <stdint.h>

namespace rc
{

/**
 * Converts a string-represented ip into uint (e.g. for subnet masking)
 *  taken from: https://www.stev.org/post/ccheckanipaddressisinaipmask
 * @param ip
 * @return
 */
uint32_t IPToUInt(const std::string ip);

/**
 * Checks if a given ip is in range of a network defined by ip/subnet
 *  taken from: https://www.stev.org/post/ccheckanipaddressisinaipmask
 * @param ip
 * @param network
 * @param mask
 * @return
 */
bool isIPInRange(const std::string ip, const std::string network, const std::string mask);

}

#endif //RC_DEVICE_NET_UTILS_H
