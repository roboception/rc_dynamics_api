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




/**
 * Convenience function to scan this host's (multiple) network interface(s) for
 * a valid IP address.
 * Users may give a hint either be specifying the preferred network interface
 * to be used, or the IP address of another host that should be reachable from
 * the returned IP address.
 *
 * @param thisHostsIP IP address to be used as stream destination (only valid if returned true)
 * @param otherHostsIP rc_visard's IP address, e.g. "192.168.0.20"
 * @param networkInterface rc_visard's subnet config, e.g. "255.255.255.0"
 * @return true if valid IP address was found among network interfaces
 */
bool getThisHostsIP(std::string &thisHostsIP,
                    std::string otherHostsIP = "",
                    std::string networkInterface = "");

}

#endif //RC_DEVICE_NET_UTILS_H
