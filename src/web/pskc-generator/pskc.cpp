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
 *   This file implements the generating pskc function.
 */

#include "common/code_utils.hpp"

#include "pskc.hpp"

namespace ot {
namespace Psk {

void Pskc::SetSalt(const uint8_t *aExtPanId, const char *aNetworkName)
{
    const char *saltPrefix = "Thread";
    int         cur = 0;
    int         ret = kPskcStatus_Ok;

    memset(mSalt, 0, sizeof(mSalt));
    memcpy(mSalt, saltPrefix, strlen(saltPrefix));
    cur += strlen(saltPrefix);

    memcpy(mSalt + cur, aExtPanId, EXTEND_PAN_ID_LEN);
    cur += EXTEND_PAN_ID_LEN;

    VerifyOrExit(strlen(aNetworkName) > 0, ret = kPskcStatus_InvalidArgument);
    memcpy(mSalt + cur, aNetworkName, strlen(aNetworkName));
    cur += strlen(aNetworkName);

    mSaltLen = cur;

exit:
    if (ret != kPskcStatus_Ok)
    {
        syslog(LOG_ERR, "ExtPanId or NetworkName is NULL");
    }
    return;
}

void Pskc::Pbkdf2Cmac(void)
{
    uint32_t blockCounter = 0;
    uint16_t useLen = 0;
    uint16_t prfBlockLen = MBEDTLS_CIPHER_BLKSIZE_MAX;
    uint8_t  prfInput[PBKDF2_SALT_MAX_LEN + 4];
    uint8_t  prfOutput[MBEDTLS_CIPHER_BLKSIZE_MAX];
    uint8_t  keyBlock[MBEDTLS_CIPHER_BLKSIZE_MAX];
    uint16_t keyLen = PSKC_LENGTH;
    uint8_t *pskc = mPskc;

    while (keyLen)
    {
        memcpy(prfInput, mSalt, mSaltLen);

        blockCounter++;
        prfInput[mSaltLen + 0] = (uint8_t) (blockCounter >> 24);
        prfInput[mSaltLen + 1] = (uint8_t) (blockCounter >> 16);
        prfInput[mSaltLen + 2] = (uint8_t) (blockCounter >> 8);
        prfInput[mSaltLen + 3] = (uint8_t) (blockCounter);
        // Calculate U_1
        mbedtls_aes_cmac_prf_128(mPassphrase,
                                 mPassphraseLen, prfInput,
                                 mSaltLen + 4, prfOutput);
        memcpy(keyBlock, prfOutput, prfBlockLen);

        for (uint32_t i = 1; i < ITERATION_COUNTS; i++)
        {
            memcpy(prfInput, prfOutput, prfBlockLen);

            // Calculate U_i
            mbedtls_aes_cmac_prf_128(mPassphrase,
                                     mPassphraseLen, prfInput,
                                     prfBlockLen, prfOutput);

            // xor
            for (uint32_t j = 0; j < prfBlockLen; j++)
            {
                keyBlock[j] ^= prfOutput[j];
            }
        }

        useLen = (keyLen < prfBlockLen) ? keyLen : prfBlockLen;
        memcpy(pskc, keyBlock, useLen);
        pskc += useLen;
        keyLen -= useLen;
    }

}

} //namespace Psk
} //namespace ot
