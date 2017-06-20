/*
 *  Copyright (c) 2017, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file implements the wpan controller service
 */

#ifndef WPAN_SERVICE
#define WPAN_SERVICE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <jsoncpp/json/json.h>
#include <jsoncpp/json/writer.h>

#include "../mdns-publisher/mdns_publisher.hpp"
#include "../pskc-generator/pskc.hpp"
#include "../wpan-controller/wpan_controller.hpp"
#include "../utils/encoding.hpp"
#include "common/logging.hpp"
#include "utils/hex.hpp"

#define OT_BORDER_ROUTER_PORT 49191
#define OT_EXTENDED_PANID_LENGTH 8
#define OT_HARDWARE_ADDRESS_LENGTH 8
#define OT_NETWORK_NAME_LENGTH 16
#define OT_PANID_LENGTH 4
#define OT_PSKC_MAX_LENGTH 16
#define OT_PUBLISH_SERVICE_INTERVAL 20

namespace ot {
namespace Web {

class WpanService
{
public:

    // *
    //  * This method is constructor of the WpanService.
    //  *

    // WpanService(const std::string &aFilePath);

    /**
     * This method handle joining network http request.
     *
     * @returns The reference to the http response of joining network.
     *
     */
    std::string &HandleJoinNetworkRequest(const std::string &aJoinRequest);

    /**
     * This method handle forming network http request.
     *
     * @returns The reference to the http response of forming network.
     *
     */
    std::string &HandleFormNetworkRequest(const std::string &aFormRequest);

    /**
     * This method handle adding on-mesh prefix http request.
     *
     * @returns The reference to the http response of adding on-mesh prefix.
     *
     */
    std::string &HandleAddPrefixRequest(const std::string &aAddPrefixRequest);

    /**
     * This method handle deleting on-mesh prefix http request.
     *
     * @returns The reference to the http response of deleting on-mesh prefix.
     *
     */
    std::string &HandleDeletePrefixRequest(const std::string &aDeleteRequest);

    /**
     * This method handle getting status http request.
     *
     * @returns The reference to the http response of getting status.
     *
     */
    std::string &HandleStatusRequest(void);

    /**
     * This method handle getting available networks http request.
     *
     * @returns The reference to the http response of getting available networks.
     *
     */
    std::string &HandleAvailableNetworkRequest(void);

    /**
     * This method sets the interface name of the wpantund.
     *
     * @param[in]  aIfName  The pointer to the interface name of wpantund.
     *
     */
    void SetInterfaceName(const char *aIfName) { strncpy(mIfName, aIfName, sizeof(mIfName)); }

    /**
     * This method check if wpan service is started.
     *
     * @param[in]  aNetworkName  The pointer to the network name.
     * @param[in]  aIfName  The pointer to the extended PAN ID.
     *
     * @retval kWpanStatus_OK        Has started the wpan service.
     * @retval kWpanStatus_OffLine   Has not started the the wpan service.
     * @retval kWpanStatus_Down      Has not started the wpantund.
     *
     */
    int IsStartedWpanService(std::string &aNetworkName, std::string &aExtPanId);

private:

    ot::Dbus::WpanNetworkInfo mNetworks[DBUS_MAXIMUM_NAME_LENGTH];
    int                       mNetworksCount;
    char                      mIfName[IFNAMSIZ];
    std::string               mNetworkName, mExtPanId;

    enum
    {
        kWpanStatus_OK = 0,
        kWpanStatus_OffLine,
        kWpanStatus_Down,
        kWpanStatus_ParseRequestFailed,
    };

    enum
    {
        kPropertyType_String = 0,
        kPropertyType_Data
    };

};

} //namespace Web
} //namespace ot

#endif
