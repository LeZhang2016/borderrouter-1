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
 *   This file implements starting an mDNS server, and publish border router service.
 */

#include "mdns_publisher.hpp"

#include "common/code_utils.hpp"

#define OT_HOST_NAME "OPENTHREAD"
#define OT_PERIODICAL_TIME 1000 * 7

namespace ot {
namespace Mdns {

enum
{
    kMdnsPublisher_OK = 0,
    kMdnsPublisher_FailedCreateServer,
    kMdnsPublisher_FailedCreatePoll,
    kMdnsPublisher_FailedFreePoll,
    kMdnsPublisher_FailedCreateGoup,
    kMdnsPublisher_FailedAddSevice,
    kMdnsPublisher_FailedRegisterSevice,
    kMdnsPublisher_FailedUpdateSevice,
    kMdnsPublisher_FailedCreateClient,
};

void Publisher::HandleServerStart(AvahiServer *aServer, AvahiServerState aState,
                                  AVAHI_GCC_UNUSED void *aUserData)
{
    Publisher *publisher = static_cast<Publisher *>(aUserData);

    publisher->HandleServerStart(aServer, aState);
}

void Publisher::HandleEntryGroupStart(AvahiServer *aServer,
                                      AvahiSEntryGroup *aGroup, AvahiEntryGroupState aState,
                                      AVAHI_GCC_UNUSED void *aUserData)
{
    Publisher *publisher = static_cast<Publisher *>(aUserData);

    publisher->HandleEntryGroupStart(aServer, aGroup, aState);
}

void Publisher::HandleServicePublish(AVAHI_GCC_UNUSED AvahiTimeout *aTimeout, AVAHI_GCC_UNUSED void *aUserData)
{
    Publisher *publisher = static_cast<Publisher *>(aUserData);

    publisher->HandleServicePublish(aTimeout);
}

void Publisher::HandleClientStart(AvahiClient *aClient, AvahiClientState aState,
                                  AVAHI_GCC_UNUSED void *aUserData)
{
    Publisher *publisher = static_cast<Publisher *>(aUserData);

    publisher->HandleClientStart(aClient, aState);
}

void Publisher::HandleEntryGroupStart(AvahiEntryGroup *aGroup, AvahiEntryGroupState aState,
                                      AVAHI_GCC_UNUSED void *aUserData)
{
    Publisher *publisher = static_cast<Publisher *>(aUserData);

    publisher->HandleEntryGroupStart(aGroup, aState);
}

void Publisher::Free(void)
{
    if (mServer != NULL)
    {
        avahi_server_free(mServer);
        mServer = NULL;
    }

    if (mClient != NULL)
    {
        avahi_client_free(mClient);
        mClient = NULL;
    }

    if (mSimplePoll != NULL)
    {
        avahi_simple_poll_free(mSimplePoll);
        mSimplePoll = NULL;
    }
    
    if (mServiceName)
    {
        avahi_free(mServiceName);
        mServiceName = NULL;
    }

    if (mServerGroup != NULL)
    {
        avahi_s_entry_group_free(mServerGroup);
    }

    if (mClientGroup != NULL)
    {
        avahi_entry_group_free(mClientGroup);
    }
}

Publisher::Publisher(void) :
    mServerGroup(NULL),
    mClientGroup(NULL),
    mSimplePoll(NULL),
    mServer(NULL),
    mClient(NULL),
    mPort(0),
    mServiceName(NULL),
    mNetworkNameTxt(NULL),
    mExtPanIdTxt(NULL),
    mType(NULL)
{
}

int Publisher::CreateService(AvahiClient *aClient)
{
    char *serviceName;
    int   ret = kMdnsPublisher_OK;

    assert(aClient);

    if (!mClientGroup)
    {
        mClientGroup = avahi_entry_group_new(aClient, HandleEntryGroupStart, this);
        VerifyOrExit(mClientGroup != NULL, ret = kMdnsPublisher_FailedCreateGoup);
    }

    if (avahi_entry_group_is_empty(mClientGroup))
    {
        syslog(LOG_ERR, "Adding service '%s'", mServiceName);

        VerifyOrExit(avahi_entry_group_add_service(mClientGroup, AVAHI_IF_UNSPEC,
                                                   AVAHI_PROTO_UNSPEC, (AvahiPublishFlags) 0,
                                                   mServiceName, mType, NULL, NULL, mPort,
                                                   mNetworkNameTxt, mExtPanIdTxt, NULL) == 0,
                     ret = kMdnsPublisher_FailedAddSevice);

        syslog(LOG_INFO, " Service Name: %s  Port: %d  Network Name: %s  Extended Pan ID: %s ",
               mServiceName, mPort, mNetworkNameTxt, mExtPanIdTxt);

        VerifyOrExit(avahi_entry_group_commit(mClientGroup) == 0,
                     ret = kMdnsPublisher_FailedRegisterSevice);
    }

exit:

    if (ret == kMdnsPublisher_FailedAddSevice)
    {
        serviceName = avahi_alternative_service_name(mServiceName);
        avahi_free(mServiceName);
        mServiceName = serviceName;
        syslog(LOG_INFO, "Service name collision, renaming service to '%s'", mServiceName);
        avahi_entry_group_reset(mClientGroup);
        CreateService(aClient);
    }

    if (ret == kMdnsPublisher_FailedRegisterSevice)
    {
        avahi_simple_poll_quit(mSimplePoll);
    }

    return ret;
}

int Publisher::CreateService(AvahiServer *aServer)
{
    int ret = kMdnsPublisher_OK;

    assert(aServer);

    if (!mServerGroup)
    {
        VerifyOrExit((mServerGroup = avahi_s_entry_group_new(aServer, HandleEntryGroupStart, this)) != NULL,
                     ret = kMdnsPublisher_FailedCreateGoup);
    }

    syslog(LOG_ERR, "Adding service '%s'", mServiceName);

    VerifyOrExit(avahi_server_add_service(aServer, mServerGroup, AVAHI_IF_UNSPEC,
                                          AVAHI_PROTO_UNSPEC, (AvahiPublishFlags) 0,
                                          mServiceName, mType, NULL, NULL, mPort,
                                          mNetworkNameTxt, mExtPanIdTxt, NULL) == 0,
                 ret = kMdnsPublisher_FailedAddSevice);

    syslog(LOG_INFO, " Service Name: %s Port: %d Network Name: %s Extended Pan ID: %s ",
           mServiceName, mPort, mNetworkNameTxt, mExtPanIdTxt);

    VerifyOrExit(avahi_s_entry_group_commit(mServerGroup) == 0,
                 ret = kMdnsPublisher_FailedRegisterSevice);
    return ret;

exit:
    if (ret != kMdnsPublisher_OK)
    {
        avahi_simple_poll_quit(mSimplePoll);
    }

    return ret;
}

void Publisher::HandleClientStart(AvahiClient *aClient, AvahiClientState aState)
{
    assert(aClient);

    switch (aState)
    {
    case AVAHI_CLIENT_S_RUNNING:
        CreateService(aClient);
        break;

    case AVAHI_CLIENT_FAILURE:
        syslog(LOG_ERR, "Client failure: %s", avahi_strerror(avahi_client_errno(aClient)));
        avahi_simple_poll_quit(mSimplePoll);
        break;

    case AVAHI_CLIENT_S_COLLISION:
    case AVAHI_CLIENT_S_REGISTERING:
        if (mClientGroup)
        {
            avahi_entry_group_reset(mClientGroup);
        }

        break;

    case AVAHI_CLIENT_CONNECTING:
        break;
    }
}

void Publisher::HandleServerStart(AvahiServer *aServer, AvahiServerState aState)
{
    assert(aServer);

    switch (aState)
    {

    case AVAHI_SERVER_RUNNING:
        if (mServerGroup == NULL)
        {
            CreateService(aServer);
        }

        break;

    case AVAHI_SERVER_COLLISION:
        avahi_simple_poll_quit(mSimplePoll);
        if (mServer != NULL)
        {
            avahi_server_free(mServer);
        }

        StartClient();
        break;

    case AVAHI_SERVER_REGISTERING:
        if (mServerGroup != NULL)
        {
            avahi_s_entry_group_reset(mServerGroup);
        }

        break;

    case AVAHI_SERVER_FAILURE:

        syslog(LOG_ERR, "Server failure: %s",
               avahi_strerror(avahi_server_errno(aServer)));
        avahi_simple_poll_quit(mSimplePoll);
        break;

    case AVAHI_SERVER_INVALID:
        break;
    }
}

void Publisher::HandleServicePublish(AVAHI_GCC_UNUSED AvahiTimeout *aTimeout)
{
    struct timeval tv;

    if ((mServer != NULL) && (avahi_server_get_state(mServer) == AVAHI_SERVER_RUNNING))
    {
        if (mServerGroup)
        {
            avahi_s_entry_group_reset(mServerGroup);
        }
        CreateService(mServer);

        avahi_simple_poll_get(mSimplePoll)->timeout_new(
            avahi_simple_poll_get(mSimplePoll),
            avahi_elapse_time(&tv, OT_PERIODICAL_TIME, 0),
            HandleServicePublish,
            this);
    }

    if ((mClient != NULL) && (avahi_client_get_state(mClient) == AVAHI_CLIENT_S_RUNNING))
    {
        if (mClientGroup)
        {
            avahi_entry_group_reset(mClientGroup);
        }

        CreateService(mClient);

        avahi_simple_poll_get(mSimplePoll)->timeout_new(
            avahi_simple_poll_get(mSimplePoll),
            avahi_elapse_time(&tv, OT_PERIODICAL_TIME, 0),
            HandleServicePublish,
            this);
    }
}

int Publisher::StartClient(void)
{
    int            error;
    int            ret = kMdnsPublisher_OK;
    struct timeval tv;

    VerifyOrExit((mSimplePoll = avahi_simple_poll_new()) != NULL,
                 ret = kMdnsPublisher_FailedCreatePoll);
    mClient = avahi_client_new(avahi_simple_poll_get(mSimplePoll), (AvahiClientFlags) 0,
                               HandleClientStart, this, &error);
    VerifyOrExit(mClient, ret = kMdnsPublisher_FailedCreateClient);

    avahi_simple_poll_get(mSimplePoll)->timeout_new(
        avahi_simple_poll_get(mSimplePoll),
        avahi_elapse_time(&tv, OT_PERIODICAL_TIME, 0),
        HandleServicePublish,
        this);
    avahi_simple_poll_loop(mSimplePoll);

exit:

    Free();
    return ret;
}

int Publisher::StartServer(void)
{
    int               ret = kMdnsPublisher_OK;
    AvahiServerConfig config;
    int               error;
    struct timeval    tv;

    srand(time(NULL));

    VerifyOrExit((mSimplePoll = avahi_simple_poll_new()) != NULL,
                 ret = kMdnsPublisher_FailedCreatePoll);
    avahi_server_config_init(&config);

    config.host_name = avahi_strdup(OT_HOST_NAME);
    config.publish_workstation = 0;
    config.publish_aaaa_on_ipv4 = 0;
    config.publish_a_on_ipv6 = 0;
    config.use_ipv4 = 1;
    config.use_ipv6 = 0;

    mServer = avahi_server_new(avahi_simple_poll_get(mSimplePoll), &config,
                               HandleServerStart,
                               this, &error);
    avahi_server_config_free(&config);

    VerifyOrExit(mServer != NULL, ret = kMdnsPublisher_FailedCreateServer);

    avahi_simple_poll_get(mSimplePoll)->timeout_new(
        avahi_simple_poll_get(mSimplePoll),
        avahi_elapse_time(&tv, OT_PERIODICAL_TIME, 0),
        HandleServicePublish,
        this);

    avahi_simple_poll_loop(mSimplePoll);
exit:
    Free();
    return ret;
}

void Publisher::SetServiceName(const char *aServiceName)
{
    avahi_free(mServiceName);
    mServiceName = avahi_strdup(aServiceName);
}

void Publisher::HandleEntryGroupStart(AvahiEntryGroup *aGroup, AvahiEntryGroupState aState)
{
    assert(aGroup == mClientGroup || mClientGroup == NULL);
    mClientGroup = aGroup;

    switch (aState)
    {

    case AVAHI_ENTRY_GROUP_ESTABLISHED:
        syslog(LOG_INFO, "Service '%s' successfully established.", mServiceName);
        break;

    case AVAHI_ENTRY_GROUP_COLLISION:
        char *serviceName;

        serviceName = avahi_alternative_service_name(mServiceName);
        avahi_free(mServiceName);
        mServiceName = serviceName;

        syslog(LOG_ERR, "Service name collision, renaming service to '%s'", mServiceName);

        CreateService(avahi_entry_group_get_client(aGroup));
        break;

    case AVAHI_ENTRY_GROUP_FAILURE:

        syslog(LOG_ERR, "Entry group failure: %s",
               avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(aGroup))));

        avahi_simple_poll_quit(mSimplePoll);
        break;

    case AVAHI_ENTRY_GROUP_UNCOMMITED:
    case AVAHI_ENTRY_GROUP_REGISTERING:
        break;
    }
}

void Publisher::HandleEntryGroupStart(AvahiServer *aServer,
                                      AvahiSEntryGroup *aGroup, AvahiEntryGroupState aState)
{
    assert(aServer);
    assert(aGroup == mServerGroup);

    switch (aState)
    {

    case AVAHI_ENTRY_GROUP_ESTABLISHED:
        syslog(LOG_ERR, "Service '%s' successfully established.",
               mServiceName);
        break;

    case AVAHI_ENTRY_GROUP_COLLISION:
        char *alternativeName;
        alternativeName = avahi_alternative_service_name(mServiceName);
        avahi_free(mServiceName);
        mServiceName = alternativeName;
        syslog(LOG_WARNING, "Service name collision, renaming service to '%s'",
               mServiceName);
        CreateService(aServer);
        break;

    case AVAHI_ENTRY_GROUP_FAILURE:

        syslog(LOG_ERR, "Entry mServerGroup failure: %s",
               avahi_strerror(avahi_server_errno(aServer)));
        avahi_simple_poll_quit(mSimplePoll);
        break;

    case AVAHI_ENTRY_GROUP_UNCOMMITED:
    case AVAHI_ENTRY_GROUP_REGISTERING:
        break;
    }
}

void Publisher::SetNetworkNameTxt(const char *aNetworkNameTxt)
{
    avahi_free(mNetworkNameTxt);
    mNetworkNameTxt = avahi_strdup(aNetworkNameTxt);
}

void Publisher::SetExtPanIdTxt(const char *aExtPanIdTxt)
{
    avahi_free(mExtPanIdTxt);
    mExtPanIdTxt = avahi_strdup(aExtPanIdTxt);
}

void Publisher::SetType(const char *aType)
{
    avahi_free(mType);
    mType = avahi_strdup(aType);
}

void Publisher::SetPort(uint16_t aPort)
{
    mPort = aPort;
}

int Publisher::UpdateService(void)
{
    int ret = kMdnsPublisher_OK;

    if (mServerGroup != NULL)
    {
        ret = avahi_server_update_service_txt(mServer, mServerGroup, AVAHI_IF_UNSPEC,
                                              AVAHI_PROTO_UNSPEC, (AvahiPublishFlags) 0,
                                              mServiceName, mType, NULL, NULL, mPort,
                                              mNetworkNameTxt, mExtPanIdTxt, NULL);
    }
    else if (mClientGroup != NULL)
    {
        ret = avahi_entry_group_update_service_txt(mClientGroup, AVAHI_IF_UNSPEC,
                                                   AVAHI_PROTO_UNSPEC, (AvahiPublishFlags) 0,
                                                   mServiceName, mType, NULL, NULL, mPort,
                                                   mNetworkNameTxt, mExtPanIdTxt, NULL);
    }
    VerifyOrExit(ret == kMdnsPublisher_OK, ret = kMdnsPublisher_FailedUpdateSevice);
exit:
    return ret;
}

} //namespace Mdns
} //namespace ot
