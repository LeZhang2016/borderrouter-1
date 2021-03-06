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
 *   This file implements the web service of border router
 */

#include "web_service.hpp"

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <server_http.hpp>

#include "common/code_utils.hpp"
#include "utils/hex.hpp"

#include "../mdns-publisher/mdns_publisher.hpp"
#include "../pskc-generator/pskc.hpp"
#include "../utils/encoding.hpp"

#define OT_ADD_PREFIX_PATH "^/add_prefix"
#define OT_AVAILABLE_NETWORK_PATH "^/available_network$"
#define OT_BOOT_MDNS_PATH "^/boot_mdns$"
#define OT_DELETE_PREFIX_PATH "^/delete_prefix"
#define OT_FORM_NETWORK_PATH "^/form_network$"
#define OT_GET_NETWORK_PATH "^/get_properties$"
#define OT_JOIN_NETWORK_PATH "^/join_network$"
#define OT_SET_NETWORK_PATH "^/settings$"
#define OT_REQUEST_METHOD_GET "GET"
#define OT_REQUEST_METHOD_POST "POST"
#define OT_RESPONSE_SUCCESS_STATUS "HTTP/1.1 200 OK\r\n"
#define OT_RESPONSE_HEADER_LENGTH "Content-Length: "
#define OT_RESPONSE_HEADER_TYPE "Content-Type: application/json\r\n charset=utf-8"
#define OT_RESPONSE_PLACEHOLD "\r\n\r\n"
#define OT_RESPONSE_FAILURE_STATUS "HTTP/1.1 400 Bad Request\r\n"

#define OT_BORDER_ROUTER_PORT 49191
#define OT_EXTENDED_PANID_LENGTH 8
#define OT_HARDWARE_ADDRESS_LENGTH 8
#define OT_NETWORK_NAME_LENGTH 16
#define OT_PANID_LENGTH 4
#define OT_PSKC_MAX_LENGTH 16
#define OT_PUBLISH_SERVICE_INTERVAL 20

namespace ot {
namespace Web {

ot::Dbus::WpanNetworkInfo sNetworks[DBUS_MAXIMUM_NAME_LENGTH];
int                       sNetworksCount = 0;
std::string               sNetworkName = "";
std::string               sExtPanId = "";
bool                      sIsStarted = false;

void DefaultResourceSend(const HttpServer &aServer, const std::shared_ptr<HttpServer::Response> &aResponse,
                         const std::shared_ptr<std::ifstream> &aIfStream);

static std::string HttpReponse(uint8_t error)
{
    boost::property_tree::ptree root;
    std::stringstream           ss;

    root.put("error", error);
    if (error == 0)
    {
        root.put("result", "successful");
    }
    else
    {
        root.put("result", "failed");
    }
    write_json(ss, root, false);
    return ss.str();
}

static void SetNetworkInfo(const char *networkName, const char *extPanId)
{
    sNetworkName = networkName;
    sExtPanId = extPanId;
}

static std::string OnJoinNetworkRequest(boost::property_tree::ptree &aJoinRequest, const char *aIfName)
{
    char extPanId[OT_EXTENDED_PANID_LENGTH * 2 + 1];
    int  ret = ot::Dbus::kWpantundStatus_Ok;
    int  index = aJoinRequest.get<int>("index");

    std::string              networkKey = aJoinRequest.get<std::string>("networkKey");
    std::string              prefix = aJoinRequest.get<std::string>("prefix");
    bool                     defaultRoute = aJoinRequest.get<bool>("defaultRoute");
    ot::Dbus::WPANController wpanController;

    wpanController.SetInterfaceName(aIfName);
    VerifyOrExit(wpanController.Leave() == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_LeaveFailed);
    VerifyOrExit(wpanController.Set(WebServer::kPropertyType_Data,
                                    "NetworkKey",
                                    networkKey.c_str()) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(wpanController.Join(sNetworks[index].mNetworkName,
                                     sNetworks[index].mChannel,
                                     sNetworks[index].mExtPanId,
                                     sNetworks[index].mPanId) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_JoinFailed);
    VerifyOrExit(wpanController.AddGateway(prefix.c_str(), defaultRoute) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetGatewayFailed);

    ot::Utils::Long2Hex(sNetworks[index].mExtPanId, extPanId);
    SetNetworkInfo(sNetworks[index].mNetworkName, extPanId);
exit:
    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "Error is %d", ret);
    }
    return HttpReponse(ret);
}

static std::string OnFormNetworkRequest(boost::property_tree::ptree &aFormRequest, const char *aIfName)
{
    int ret = ot::Dbus::kWpantundStatus_Ok;

    std::string              networkKey = aFormRequest.get<std::string>("networkKey");
    std::string              prefix = aFormRequest.get<std::string>("prefix");
    uint16_t                 channel = aFormRequest.get<uint16_t>("channel");
    std::string              networkName = aFormRequest.get<std::string>("networkName");
    std::string              passphrase = aFormRequest.get<std::string>("passphrase");
    std::string              panId = aFormRequest.get<std::string>("panId");
    std::string              extPanId = aFormRequest.get<std::string>("extPanId");
    bool                     defaultRoute = aFormRequest.get<bool>("defaultRoute");
    ot::Psk::Pskc            psk;
    char                     pskcStr[OT_PSKC_MAX_LENGTH * 2];
    uint8_t                  extPanIdBytes[OT_EXTENDED_PANID_LENGTH];
    ot::Dbus::WPANController wpanController;

    wpanController.SetInterfaceName(aIfName);
    VerifyOrExit(wpanController.Leave() == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_LeaveFailed);

    VerifyOrExit(wpanController.Set(WebServer::kPropertyType_Data,
                                    kWPANTUNDProperty_NetworkKey,
                                    networkKey.c_str()) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);

    VerifyOrExit(wpanController.Set(WebServer::kPropertyType_String,
                                    kWPANTUNDProperty_NetworkPANID,
                                    panId.c_str()) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);
    VerifyOrExit(wpanController.Set(WebServer::kPropertyType_Data,
                                    kWPANTUNDProperty_NetworkXPANID,
                                    extPanId.c_str()) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);
    ot::Utils::Hex2Bytes(extPanId.c_str(), extPanIdBytes, OT_EXTENDED_PANID_LENGTH);
    ot::Utils::Bytes2Hex(psk.ComputePskc(extPanIdBytes, networkName.c_str(),
                                         passphrase.c_str()), OT_PSKC_MAX_LENGTH, pskcStr);
    VerifyOrExit(wpanController.Set(WebServer::kPropertyType_Data,
                                    kWPANTUNDProperty_NetworkPSKc,
                                    pskcStr) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetFailed);

    VerifyOrExit(wpanController.Form(networkName.c_str(), channel) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_FormFailed);

    VerifyOrExit(wpanController.AddGateway(prefix.c_str(), defaultRoute) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetGatewayFailed);
    SetNetworkInfo(networkName.c_str(), extPanId.c_str());
exit:
    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "Error is %d", ret);
    }
    return HttpReponse(ret);
}

static std::string OnAddPrefixRequest(boost::property_tree::ptree &aAddPrefixRequest, const char *aIfName)
{
    int ret = ot::Dbus::kWpantundStatus_Ok;

    std::string              prefix = aAddPrefixRequest.get<std::string>("prefix");
    bool                     defaultRoute = aAddPrefixRequest.get<bool>("defaultRoute");
    ot::Dbus::WPANController wpanController;

    wpanController.SetInterfaceName(aIfName);
    VerifyOrExit(wpanController.AddGateway(prefix.c_str(), defaultRoute) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetGatewayFailed);
exit:
    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "Error is %d", ret);
    }
    return HttpReponse(ret);

}

static std::string OnDeletePrefixRequest(boost::property_tree::ptree &aDeleteRequest, const char *aIfName)
{
    int ret = ot::Dbus::kWpantundStatus_Ok;

    std::string              prefix = aDeleteRequest.get<std::string>("prefix");
    ot::Dbus::WPANController wpanController;

    wpanController.SetInterfaceName(aIfName);
    VerifyOrExit(wpanController.RemoveGateway(prefix.c_str()) == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_SetGatewayFailed);
exit:
    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        syslog(LOG_ERR, "Error is %d", ret);
    }
    return HttpReponse(ret);
}

static std::string OnGetNetworkRequest(boost::property_tree::ptree &aGetNetworkRequest, const char *aIfName)
{
    boost::property_tree::ptree root, networkInfo;
    int                         ret = ot::Dbus::kWpantundStatus_Ok;
    ot::Dbus::WPANController    wpanController;

    wpanController.SetInterfaceName(aIfName);
    networkInfo.put(kWPANTUNDProperty_NCPState, wpanController.Get(kWPANTUNDProperty_NCPState));
    networkInfo.put(kWPANTUNDProperty_DaemonEnabled, wpanController.Get(kWPANTUNDProperty_DaemonEnabled));
    networkInfo.put(kWPANTUNDProperty_NCPVersion, wpanController.Get(kWPANTUNDProperty_NCPVersion));
    networkInfo.put(kWPANTUNDProperty_DaemonVersion, wpanController.Get(kWPANTUNDProperty_DaemonVersion));
    networkInfo.put(kWPANTUNDProperty_ConfigNCPDriverName, wpanController.Get(kWPANTUNDProperty_ConfigNCPDriverName));
    networkInfo.put(kWPANTUNDProperty_NCPHardwareAddress, wpanController.Get(kWPANTUNDProperty_NCPHardwareAddress));
    networkInfo.put(kWPANTUNDProperty_NCPChannel, wpanController.Get(kWPANTUNDProperty_NCPChannel));
    networkInfo.put(kWPANTUNDProperty_NetworkNodeType, wpanController.Get(kWPANTUNDProperty_NetworkNodeType));
    networkInfo.put(kWPANTUNDProperty_NetworkName, wpanController.Get(kWPANTUNDProperty_NetworkName));
    networkInfo.put(kWPANTUNDProperty_NetworkXPANID, wpanController.Get(kWPANTUNDProperty_NetworkXPANID));
    networkInfo.put(kWPANTUNDProperty_NetworkPANID, wpanController.Get(kWPANTUNDProperty_NetworkPANID));
    networkInfo.put(kWPANTUNDProperty_IPv6LinkLocalAddress, wpanController.Get(kWPANTUNDProperty_IPv6LinkLocalAddress));
    networkInfo.put(kWPANTUNDProperty_IPv6MeshLocalAddress, wpanController.Get(kWPANTUNDProperty_IPv6MeshLocalAddress));
    networkInfo.put(kWPANTUNDProperty_IPv6MeshLocalPrefix, wpanController.Get(kWPANTUNDProperty_IPv6MeshLocalPrefix));
    root.add_child("result", networkInfo);
    root.put("error", ret);
    std::stringstream ss;
    write_json(ss, root, false);

    (void)aIfName;
    (void)aGetNetworkRequest;

    return ss.str();
}

static std::string OnGetAvailableNetworkResponse(boost::property_tree::ptree &aGetAvailableNetworkRequest,
                                                 const char *aIfName)
{
    boost::property_tree::ptree root, networks, networkInfo;
    int                         ret = ot::Dbus::kWpantundStatus_Ok;
    ot::Dbus::WPANController    wpanController;

    wpanController.SetInterfaceName(aIfName);
    VerifyOrExit(wpanController.Leave() == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_LeaveFailed);
    VerifyOrExit(wpanController.Scan() == ot::Dbus::kWpantundStatus_Ok,
                 ret = ot::Dbus::kWpantundStatus_ScanFailed);
    sNetworksCount = wpanController.GetScanNetworksInfoCount();
    VerifyOrExit(sNetworksCount > 0, ret = ot::Dbus::kWpantundStatus_NetworkNotFound);
    memcpy(sNetworks, wpanController.GetScanNetworksInfo(),
           sNetworksCount * sizeof(ot::Dbus::WpanNetworkInfo));

    for (int i = 0; i < sNetworksCount; i++)
    {
        char extPanId[OT_EXTENDED_PANID_LENGTH * 2 + 1], panId[OT_PANID_LENGTH + 3],
             hardwareAddress[OT_HARDWARE_ADDRESS_LENGTH * 2 + 1];
        ot::Utils::Long2Hex(Thread::Encoding::BigEndian::HostSwap64(sNetworks[i].mExtPanId), extPanId);
        ot::Utils::Bytes2Hex(sNetworks[i].mHardwareAddress, OT_HARDWARE_ADDRESS_LENGTH, hardwareAddress);
        sprintf(panId, "0x%X", sNetworks[i].mPanId);
        networkInfo.put("nn", sNetworks[i].mNetworkName);
        networkInfo.put("xp", extPanId);
        networkInfo.put("pi", panId);
        networkInfo.put("ch", sNetworks[i].mChannel);
        networkInfo.put("ha", hardwareAddress);
        networks.push_back(std::make_pair("", networkInfo));
    }
    root.add_child("result", networks);
exit:
    if (ret != ot::Dbus::kWpantundStatus_Ok)
    {
        root.put("result", "failed");
        syslog(LOG_ERR, "Error is %d", ret);
    }
    root.put("error", ret);
    std::stringstream ss;
    write_json(ss, root, false);

    (void)aGetAvailableNetworkRequest;

    return ss.str();
}

static std::string OnBootMdnsRequest(boost::property_tree::ptree &aBootMdnsRequest, const char *aIfName)
{
    std::thread mdnsPublisherThread([]() {
                ot::Mdns::Publisher::GetInstance().SetServiceName(sNetworkName.c_str());
                ot::Mdns::Publisher::GetInstance().SetType("_meshcop._udp");
                ot::Mdns::Publisher::GetInstance().SetPort(OT_BORDER_ROUTER_PORT);
                sNetworkName = "nn=" + sNetworkName;
                sExtPanId = "xp=" + sExtPanId;
                ot::Mdns::Publisher::GetInstance().SetNetworkNameTxt(sNetworkName.c_str());
                ot::Mdns::Publisher::GetInstance().SetExtPanIdTxt(sExtPanId.c_str());
                if (sIsStarted)
                {
                    ot::Mdns::Publisher::GetInstance().UpdateService();
                }
                else
                {
                    sIsStarted = true;
                    ot::Mdns::Publisher::GetInstance().StartClient();
                }
            });

    mdnsPublisherThread.detach();

    (void)aBootMdnsRequest;
    (void)aIfName;

    return HttpReponse(ot::Dbus::kWpantundStatus_Ok);
}

WebServer::WebServer(void) : mServer(new HttpServer()) {}

WebServer::~WebServer(void)
{
    delete &mServer;
}

void WebServer::StartWebServer(const char *aIfName)
{
    mServer->config.port = 80;
    strncpy(mIfName, aIfName, sizeof(mIfName));
    JoinNetworkResponse();
    FormNetworkResponse();
    AddOnMeshPrefix();
    DeleteOnMeshPrefix();
    GetNetworkResponse();
    AvailableNetworkResponse();
    DefaultHttpResponse();
    BootMdnsPublisher();
    std::thread ServerThread([this]() {
                mServer->start();
            });
    ServerThread.join();
}

void WebServer::JoinNetworkResponse(void)
{
    HandleHttpRequest(OT_JOIN_NETWORK_PATH, OT_REQUEST_METHOD_POST, OnJoinNetworkRequest, mIfName);
}

void WebServer::FormNetworkResponse(void)
{
    HandleHttpRequest(OT_FORM_NETWORK_PATH, OT_REQUEST_METHOD_POST, OnFormNetworkRequest, mIfName);
}

void WebServer::AddOnMeshPrefix(void)
{
    HandleHttpRequest(OT_ADD_PREFIX_PATH, OT_REQUEST_METHOD_POST, OnAddPrefixRequest, mIfName);
}

void WebServer::DeleteOnMeshPrefix(void)
{
    HandleHttpRequest(OT_DELETE_PREFIX_PATH, OT_REQUEST_METHOD_POST, OnDeletePrefixRequest, mIfName);
}

void WebServer::GetNetworkResponse(void)
{
    HandleHttpRequest(OT_GET_NETWORK_PATH, OT_REQUEST_METHOD_GET, OnGetNetworkRequest, mIfName);
}

void WebServer::AvailableNetworkResponse(void)
{
    HandleHttpRequest(OT_AVAILABLE_NETWORK_PATH, OT_REQUEST_METHOD_GET, OnGetAvailableNetworkResponse, mIfName);
}

void WebServer::BootMdnsPublisher(void)
{
    HandleHttpRequest(OT_BOOT_MDNS_PATH, OT_REQUEST_METHOD_GET, OnBootMdnsRequest, mIfName);
}

void WebServer::HandleHttpRequest(const char *aUrl, const char *aMethod, HttpRequestCallback aCallback,
                                  const char *aIfName)
{
    mServer->resource[aUrl][aMethod] =
        [aCallback, aIfName](std::shared_ptr<HttpServer::Response> response,
                             std::shared_ptr<HttpServer::Request> request)
        {
            try
            {
                boost::property_tree::ptree pt;
                std::string                 httpResponse;
                if (aCallback != NULL)
                {
                    if (request->content.size() > 0)
                    {
                        read_json(request->content, pt);
                    }
                    httpResponse = aCallback(pt, aIfName);
                }

                *response << OT_RESPONSE_SUCCESS_STATUS
                          << OT_RESPONSE_HEADER_LENGTH
                          << httpResponse.length()
                          << OT_RESPONSE_PLACEHOLD
                          << httpResponse;
            }
            catch (std::exception & e)
            {

                *response << OT_RESPONSE_FAILURE_STATUS
                          << OT_RESPONSE_HEADER_LENGTH
                          << strlen(e.what())
                          << OT_RESPONSE_PLACEHOLD
                          << e.what();
            }
        };
}

void WebServer::DefaultHttpResponse(void)
{
    mServer->default_resource[OT_REQUEST_METHOD_GET] =
        [this](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request)
        {
            try
            {
                auto webRootPath =
                    boost::filesystem::canonical(WEB_FILE_PATH);
                auto path = boost::filesystem::canonical(
                    webRootPath / request->path);
                //Check if path is within webRootPath
                if (std::distance(webRootPath.begin(),
                                  webRootPath.end()) > std::distance(path.begin(), path.end()) ||
                    !std::equal(webRootPath.begin(), webRootPath.end(),
                                path.begin()))
                {
                    throw std::invalid_argument("path must be within root path");
                }
                if (boost::filesystem::is_directory(path))
                {
                    path /= "index.html";
                }
                if (!(boost::filesystem::exists(path) &&
                      boost::filesystem::is_regular_file(path)))
                {
                    throw std::invalid_argument("file does not exist");
                }

                std::string cacheControl, etag;

                auto ifs = std::make_shared<std::ifstream>();
                ifs->open(
                    path.string(), std::ifstream::in | std::ios::binary | std::ios::ate);

                if (*ifs)
                {
                    auto length = ifs->tellg();
                    ifs->seekg(0, std::ios::beg);

                    *response << OT_RESPONSE_SUCCESS_STATUS
                              << cacheControl << etag
                              << OT_RESPONSE_HEADER_LENGTH
                              << length
                              << OT_RESPONSE_PLACEHOLD;

                    DefaultResourceSend(*mServer, response, ifs);
                }
                else
                {
                    throw std::invalid_argument("could not read file");
                }

            }
            catch (const std::exception &e)
            {
                std::string content = "Could not open path " + request->path + ": " +
                                      e.what();
                *response << OT_RESPONSE_FAILURE_STATUS
                          << OT_RESPONSE_HEADER_LENGTH
                          << content.length()
                          << OT_RESPONSE_PLACEHOLD
                          << content;
            }
        };
}

void DefaultResourceSend(const HttpServer &aServer, const std::shared_ptr<HttpServer::Response> &aResponse,
                         const std::shared_ptr<std::ifstream> &aIfStream)
{
    static std::vector<char> buffer(131072); // Safe when server is running on one thread

    std::streamsize readLength;

    if ((readLength = aIfStream->read(&buffer[0], buffer.size()).gcount()) > 0)
    {
        aResponse->write(&buffer[0], readLength);
        if (readLength == static_cast<std::streamsize>(buffer.size()))
        {
            aServer.send(aResponse, [&aServer, aResponse, aIfStream](const boost::system::error_code &ec)
                    {
                        if (!ec)
                        {
                            DefaultResourceSend(aServer, aResponse, aIfStream);
                        }
                        else
                        {
                            std::cerr << "Connection interrupted" << std::endl;
                        }
                    });
        }
    }
}

} //namespace Web
} //namespace ot
