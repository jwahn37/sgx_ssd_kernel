#ifndef ENCLAVE_U_H__
#define ENCLAVE_U_H__

#include <stdint.h>
#include <wchar.h>
#include <stddef.h>
#include <string.h>
#include "sgx_edger8r.h" /* for sgx_satus_t etc. */


#include <stdlib.h> /* for size_t */

#define SGX_CAST(type, item) ((type)(item))

#ifdef __cplusplus
extern "C" {
#endif

#ifndef OCALL_PRINT_STRING_DEFINED__
#define OCALL_PRINT_STRING_DEFINED__
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_print_string, (const char* str));
#endif
#ifndef OCALL_PASS_STRING_DEFINED__
#define OCALL_PASS_STRING_DEFINED__
void SGX_UBRIDGE(SGX_NOCONVENTION, ocall_pass_string, (unsigned char* str));
#endif

sgx_status_t printf_helloworld(sgx_enclave_id_t eid, int policy_cnt, int spm_param[4], unsigned char* newLine);
sgx_status_t print_unseal_data(sgx_enclave_id_t eid, unsigned char policy_arr[566]);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
