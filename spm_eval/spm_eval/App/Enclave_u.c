#include "Enclave_u.h"
#include <errno.h>

typedef struct ms_printf_helloworld_t {
	int ms_policy_cnt;
	int* ms_spm_param;
	unsigned char* ms_newLine;
} ms_printf_helloworld_t;

typedef struct ms_print_unseal_data_t {
	unsigned char* ms_policy_arr;
} ms_print_unseal_data_t;

typedef struct ms_ocall_print_string_t {
	const char* ms_str;
} ms_ocall_print_string_t;

typedef struct ms_ocall_pass_string_t {
	unsigned char* ms_str;
} ms_ocall_pass_string_t;

static sgx_status_t SGX_CDECL Enclave_ocall_print_string(void* pms)
{
	ms_ocall_print_string_t* ms = SGX_CAST(ms_ocall_print_string_t*, pms);
	ocall_print_string(ms->ms_str);

	return SGX_SUCCESS;
}

static sgx_status_t SGX_CDECL Enclave_ocall_pass_string(void* pms)
{
	ms_ocall_pass_string_t* ms = SGX_CAST(ms_ocall_pass_string_t*, pms);
	ocall_pass_string(ms->ms_str);

	return SGX_SUCCESS;
}

static const struct {
	size_t nr_ocall;
	void * table[2];
} ocall_table_Enclave = {
	2,
	{
		(void*)Enclave_ocall_print_string,
		(void*)Enclave_ocall_pass_string,
	}
};
sgx_status_t printf_helloworld(sgx_enclave_id_t eid, int policy_cnt, int spm_param[4], unsigned char* newLine)
{
	sgx_status_t status;
	ms_printf_helloworld_t ms;
	ms.ms_policy_cnt = policy_cnt;
	ms.ms_spm_param = (int*)spm_param;
	ms.ms_newLine = newLine;
	status = sgx_ecall(eid, 0, &ocall_table_Enclave, &ms);
	return status;
}

sgx_status_t print_unseal_data(sgx_enclave_id_t eid, unsigned char policy_arr[566])
{
	sgx_status_t status;
	ms_print_unseal_data_t ms;
	ms.ms_policy_arr = (unsigned char*)policy_arr;
	status = sgx_ecall(eid, 1, &ocall_table_Enclave, &ms);
	return status;
}

