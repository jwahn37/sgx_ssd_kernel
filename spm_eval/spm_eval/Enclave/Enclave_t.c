#include "Enclave_t.h"

#include "sgx_trts.h" /* for sgx_ocalloc, sgx_is_outside_enclave */
#include "sgx_lfence.h" /* for sgx_lfence */

#include <errno.h>
#include <mbusafecrt.h> /* for memcpy_s etc */
#include <stdlib.h> /* for malloc/free etc */

#define CHECK_REF_POINTER(ptr, siz) do {	\
	if (!(ptr) || ! sgx_is_outside_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define CHECK_UNIQUE_POINTER(ptr, siz) do {	\
	if ((ptr) && ! sgx_is_outside_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)

#define CHECK_ENCLAVE_POINTER(ptr, siz) do {	\
	if ((ptr) && ! sgx_is_within_enclave((ptr), (siz)))	\
		return SGX_ERROR_INVALID_PARAMETER;\
} while (0)


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

static sgx_status_t SGX_CDECL sgx_printf_helloworld(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_printf_helloworld_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_printf_helloworld_t* ms = SGX_CAST(ms_printf_helloworld_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	int* _tmp_spm_param = ms->ms_spm_param;
	unsigned char* _tmp_newLine = ms->ms_newLine;



	printf_helloworld(ms->ms_policy_cnt, _tmp_spm_param, _tmp_newLine);


	return status;
}

static sgx_status_t SGX_CDECL sgx_print_unseal_data(void* pms)
{
	CHECK_REF_POINTER(pms, sizeof(ms_print_unseal_data_t));
	//
	// fence after pointer checks
	//
	sgx_lfence();
	ms_print_unseal_data_t* ms = SGX_CAST(ms_print_unseal_data_t*, pms);
	sgx_status_t status = SGX_SUCCESS;
	unsigned char* _tmp_policy_arr = ms->ms_policy_arr;



	print_unseal_data(_tmp_policy_arr);


	return status;
}

SGX_EXTERNC const struct {
	size_t nr_ecall;
	struct {void* ecall_addr; uint8_t is_priv;} ecall_table[2];
} g_ecall_table = {
	2,
	{
		{(void*)(uintptr_t)sgx_printf_helloworld, 0},
		{(void*)(uintptr_t)sgx_print_unseal_data, 0},
	}
};

SGX_EXTERNC const struct {
	size_t nr_ocall;
	uint8_t entry_table[2][2];
} g_dyn_entry_table = {
	2,
	{
		{0, 0, },
		{0, 0, },
	}
};


sgx_status_t SGX_CDECL ocall_print_string(const char* str)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_str = str ? strlen(str) + 1 : 0;

	ms_ocall_print_string_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_ocall_print_string_t);
	void *__tmp = NULL;


	CHECK_ENCLAVE_POINTER(str, _len_str);

	ocalloc_size += (str != NULL) ? _len_str : 0;

	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_ocall_print_string_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_ocall_print_string_t));
	ocalloc_size -= sizeof(ms_ocall_print_string_t);

	if (str != NULL) {
		ms->ms_str = (const char*)__tmp;
		if (memcpy_s(__tmp, ocalloc_size, str, _len_str)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_str);
		ocalloc_size -= _len_str;
	} else {
		ms->ms_str = NULL;
	}
	
	status = sgx_ocall(0, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}

sgx_status_t SGX_CDECL ocall_pass_string(unsigned char* str)
{
	sgx_status_t status = SGX_SUCCESS;
	size_t _len_str = 566;

	ms_ocall_pass_string_t* ms = NULL;
	size_t ocalloc_size = sizeof(ms_ocall_pass_string_t);
	void *__tmp = NULL;


	CHECK_ENCLAVE_POINTER(str, _len_str);

	ocalloc_size += (str != NULL) ? _len_str : 0;

	__tmp = sgx_ocalloc(ocalloc_size);
	if (__tmp == NULL) {
		sgx_ocfree();
		return SGX_ERROR_UNEXPECTED;
	}
	ms = (ms_ocall_pass_string_t*)__tmp;
	__tmp = (void *)((size_t)__tmp + sizeof(ms_ocall_pass_string_t));
	ocalloc_size -= sizeof(ms_ocall_pass_string_t);

	if (str != NULL) {
		ms->ms_str = (unsigned char*)__tmp;
		if (memcpy_s(__tmp, ocalloc_size, str, _len_str)) {
			sgx_ocfree();
			return SGX_ERROR_UNEXPECTED;
		}
		__tmp = (void *)((size_t)__tmp + _len_str);
		ocalloc_size -= _len_str;
	} else {
		ms->ms_str = NULL;
	}
	
	status = sgx_ocall(1, ms);

	if (status == SGX_SUCCESS) {
	}
	sgx_ocfree();
	return status;
}

