/*
 * Copyright (C) 2011-2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <stdarg.h>
#include <stdio.h>      /* vsnprintf */
#include <stdlib.h>

#include "sgx_trts.h"
#include "sgx_tseal.h"

#include "Enclave.h"
#include "Enclave_t.h"  /* print_string */

//new
#include "sgx_tseal.h"
#include <stdlib.h>
#include <string.h>
#include "sgx_trts.h"
#include "sgx_report.h"
#include "sgx_utils.h"
#include "sgx_attributes.h"
 #include <stdint.h>

 
/* Set the bits which have no security implications to 0 for sealed data migration */
/* Bits which have no security implications in attributes.flags:
 *    Reserved bit[55:6]  - 0xFFFFFFFFFFFFC0ULL
 *    SGX_FLAGS_MODE64BIT
 *    SGX_FLAGS_PROVISION_KEY
 *    SGX_FLAGS_EINITTOKEN_KEY */
#define FLAGS_NON_SECURITY_BITS     (0xFFFFFFFFFFFFC0ULL | SGX_FLAGS_MODE64BIT | SGX_FLAGS_PROVISION_KEY| SGX_FLAGS_EINITTOKEN_KEY)
#define TSEAL_DEFAULT_FLAGSMASK     (~FLAGS_NON_SECURITY_BITS)
#define FLAGS_SECURITY_BITS_RESERVED (~(FLAGS_NON_SECURITY_BITS | SGX_FLAGS_INITTED | SGX_FLAGS_DEBUG | SGX_FLAGS_KSS))


#define MISC_NON_SECURITY_BITS      0x0FFFFFFF  /* bit[27:0]: have no security implications */
#define TSEAL_DEFAULT_MISCMASK      (~MISC_NON_SECURITY_BITS)
 
#define KEY_POLICY_KSS  (SGX_KEYPOLICY_CONFIGID | SGX_KEYPOLICY_ISVFAMILYID | SGX_KEYPOLICY_ISVEXTPRODID)

//

#define original_len 6
#define encrypt_len 566

/* 
 * printf: 
 *   Invokes OCALL to display the enclave buffer to the terminal.
 */
void printf(const char *fmt, ...)
{
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print_string(buf);
}

void printf_helloworld(int policy_cnt, int spm_param[4], unsigned char*newLine)
{
    //실험용printf("Hello World\n");
    //실험용printf("spm_param : %d %d %d\n", spm_param[0], spm_param[1], spm_param[3]);
    
    //make original policy
    unsigned char tmp_policy[original_len];
    tmp_policy[0] = spm_param[0] + '0';
    tmp_policy[1] = ' ';
    tmp_policy[2] = spm_param[1] + '0';
    tmp_policy[3] = ' ';
    tmp_policy[4] = '0';
    tmp_policy[5] = 0;
    
    //to do : make encrypt msg and MAC headed for ssd
    uint8_t msg1[5] = {'l', 'a', 's','s', 0};
    uint32_t MAC_len1 = 4;
    
    uint32_t plaintext_len2 = original_len;
    uint32_t sealed_size2 = sgx_calc_sealed_data_size(NULL, original_len);
    unsigned char sealed_data2[sealed_size2];
    sgx_seal_data(4, msg1, plaintext_len2, (uint8_t*)tmp_policy, sealed_size2, (sgx_sealed_data_t*)sealed_data2);
    //but no pass to app
    
    //to do : make encrypted new line for policy list
    uint32_t plaintext_len = original_len;
    uint32_t sealed_size = sgx_calc_sealed_data_size(NULL, original_len);
    unsigned char sealed_data[sealed_size];
    sgx_seal_data(0, NULL,plaintext_len, (uint8_t*)tmp_policy, sealed_size, (sgx_sealed_data_t*)sealed_data);
    
    ocall_pass_string(sealed_data);
}

//spm_send_cmd 에서 암호화가 됐다고 가정하고 걍 마샬링 잘해서 보내면 될듯

void print_unseal_data(unsigned char policy_arr[566]){
    char plaintext[original_len];
    uint32_t plaintext_len = original_len;
    unsigned char sealed_data[566];
    for(int i = 0; i < 566; i++){
        sealed_data[i] = policy_arr[i];
    }
    sgx_unseal_data((sgx_sealed_data_t*)sealed_data, NULL, NULL, (uint8_t*)plaintext, &plaintext_len);
    plaintext[5] = 0;
    //실험printf("%s\n", plaintext);
}
