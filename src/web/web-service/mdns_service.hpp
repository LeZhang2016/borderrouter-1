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
 *   This file implements the mdns service
 */

#ifndef MNDS_SERVICE
#define MNDS_SERVICE

// #include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <net/if.h>

#include <jsoncpp/json/json.h>
#include <jsoncpp/json/writer.h>

#include "../mdns-publisher/mdns_publisher.hpp"
#include "../wpan-controller/wpan_controller.hpp"
#include "common/logging.hpp"

#define OT_BORDER_ROUTER_PORT 49191

namespace ot {
namespace Web {

class MdnsService
{
public:

    /**
     * This method handles mdns service http request.
     *
     * @returns The reference to the http response of mdns service.
     *
     */
    std::string &HandleMdnsRequest(const std::string &aMdnsRequest);

    /**
     * This method starts mdns service.
     *
     * @param[in]  aNetworkName  The reference to the network name.
     * @param[in]  aExtPanId     The reference to the extend PAN ID.
     *
     */
    int StartMdnsService(const std::string &aNetworkName, const std::string &aExtPanId);

    /**
     * This method updates mdns service.
     *
     * @param[in]  aNetworkName  The reference to the network name.
     * @param[in]  aExtPanId     The reference to the extend PAN ID.
     *
     */
    int UpdateMdnsService(const std::string &aNetworkName, const std::string &aExtPanId);

    /**
     * This method check if mdns service is started.
     *
     * @retval true    Has started the mdns service.
     * @retval false   Has not started the mdns service.
     *
     */
    bool IsStartedService() { return ot::Mdns::Publisher::GetInstance().IsStarted(); };

private:

    char        mIfName[IFNAMSIZ];
    std::string mNetworkName, mExtPanId;

    enum
    {
        kMndsServiceStatus_OK = 0,
        kMndsServiceStatus_ParseRequestFailed,
    };

};

} //namespace Web
} //namespace ot

#endif
