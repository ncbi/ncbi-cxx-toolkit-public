/*
 for x in connect/mbedtls/?*.o; do echo ${x##*mbedtls?} $x; done | sort | \
 while read _ x; do nm -g --defined-only $x | sed -e 's,_ncbicxx_.*,,' | \
 sort -k3; done | \
 awk '/ / { s=substr($3, 1); print "#define", s " \\\n        " s "_ncbicxx_2_16_10" }'
 */
#define mbedtls_aes_crypt_cbc \
        mbedtls_aes_crypt_cbc_ncbicxx_2_16_10
#define mbedtls_aes_crypt_cfb128 \
        mbedtls_aes_crypt_cfb128_ncbicxx_2_16_10
#define mbedtls_aes_crypt_cfb8 \
        mbedtls_aes_crypt_cfb8_ncbicxx_2_16_10
#define mbedtls_aes_crypt_ctr \
        mbedtls_aes_crypt_ctr_ncbicxx_2_16_10
#define mbedtls_aes_crypt_ecb \
        mbedtls_aes_crypt_ecb_ncbicxx_2_16_10
#define mbedtls_aes_crypt_ofb \
        mbedtls_aes_crypt_ofb_ncbicxx_2_16_10
#define mbedtls_aes_crypt_xts \
        mbedtls_aes_crypt_xts_ncbicxx_2_16_10
#define mbedtls_aes_decrypt \
        mbedtls_aes_decrypt_ncbicxx_2_16_10
#define mbedtls_aes_encrypt \
        mbedtls_aes_encrypt_ncbicxx_2_16_10
#define mbedtls_aes_free \
        mbedtls_aes_free_ncbicxx_2_16_10
#define mbedtls_aes_init \
        mbedtls_aes_init_ncbicxx_2_16_10
#define mbedtls_aes_setkey_dec \
        mbedtls_aes_setkey_dec_ncbicxx_2_16_10
#define mbedtls_aes_setkey_enc \
        mbedtls_aes_setkey_enc_ncbicxx_2_16_10
#define mbedtls_aes_xts_free \
        mbedtls_aes_xts_free_ncbicxx_2_16_10
#define mbedtls_aes_xts_init \
        mbedtls_aes_xts_init_ncbicxx_2_16_10
#define mbedtls_aes_xts_setkey_dec \
        mbedtls_aes_xts_setkey_dec_ncbicxx_2_16_10
#define mbedtls_aes_xts_setkey_enc \
        mbedtls_aes_xts_setkey_enc_ncbicxx_2_16_10
#define mbedtls_internal_aes_decrypt \
        mbedtls_internal_aes_decrypt_ncbicxx_2_16_10
#define mbedtls_internal_aes_encrypt \
        mbedtls_internal_aes_encrypt_ncbicxx_2_16_10
#define mbedtls_aesni_crypt_ecb \
        mbedtls_aesni_crypt_ecb_ncbicxx_2_16_10
#define mbedtls_aesni_gcm_mult \
        mbedtls_aesni_gcm_mult_ncbicxx_2_16_10
#define mbedtls_aesni_has_support \
        mbedtls_aesni_has_support_ncbicxx_2_16_10
#define mbedtls_aesni_inverse_key \
        mbedtls_aesni_inverse_key_ncbicxx_2_16_10
#define mbedtls_aesni_setkey_enc \
        mbedtls_aesni_setkey_enc_ncbicxx_2_16_10
#define mbedtls_arc4_crypt \
        mbedtls_arc4_crypt_ncbicxx_2_16_10
#define mbedtls_arc4_free \
        mbedtls_arc4_free_ncbicxx_2_16_10
#define mbedtls_arc4_init \
        mbedtls_arc4_init_ncbicxx_2_16_10
#define mbedtls_arc4_setup \
        mbedtls_arc4_setup_ncbicxx_2_16_10
#define mbedtls_asn1_find_named_data \
        mbedtls_asn1_find_named_data_ncbicxx_2_16_10
#define mbedtls_asn1_free_named_data \
        mbedtls_asn1_free_named_data_ncbicxx_2_16_10
#define mbedtls_asn1_free_named_data_list \
        mbedtls_asn1_free_named_data_list_ncbicxx_2_16_10
#define mbedtls_asn1_get_alg \
        mbedtls_asn1_get_alg_ncbicxx_2_16_10
#define mbedtls_asn1_get_alg_null \
        mbedtls_asn1_get_alg_null_ncbicxx_2_16_10
#define mbedtls_asn1_get_bitstring \
        mbedtls_asn1_get_bitstring_ncbicxx_2_16_10
#define mbedtls_asn1_get_bitstring_null \
        mbedtls_asn1_get_bitstring_null_ncbicxx_2_16_10
#define mbedtls_asn1_get_bool \
        mbedtls_asn1_get_bool_ncbicxx_2_16_10
#define mbedtls_asn1_get_int \
        mbedtls_asn1_get_int_ncbicxx_2_16_10
#define mbedtls_asn1_get_len \
        mbedtls_asn1_get_len_ncbicxx_2_16_10
#define mbedtls_asn1_get_mpi \
        mbedtls_asn1_get_mpi_ncbicxx_2_16_10
#define mbedtls_asn1_get_sequence_of \
        mbedtls_asn1_get_sequence_of_ncbicxx_2_16_10
#define mbedtls_asn1_get_tag \
        mbedtls_asn1_get_tag_ncbicxx_2_16_10
#define mbedtls_asn1_store_named_data \
        mbedtls_asn1_store_named_data_ncbicxx_2_16_10
#define mbedtls_asn1_write_algorithm_identifier \
        mbedtls_asn1_write_algorithm_identifier_ncbicxx_2_16_10
#define mbedtls_asn1_write_bitstring \
        mbedtls_asn1_write_bitstring_ncbicxx_2_16_10
#define mbedtls_asn1_write_bool \
        mbedtls_asn1_write_bool_ncbicxx_2_16_10
#define mbedtls_asn1_write_ia5_string \
        mbedtls_asn1_write_ia5_string_ncbicxx_2_16_10
#define mbedtls_asn1_write_int \
        mbedtls_asn1_write_int_ncbicxx_2_16_10
#define mbedtls_asn1_write_len \
        mbedtls_asn1_write_len_ncbicxx_2_16_10
#define mbedtls_asn1_write_mpi \
        mbedtls_asn1_write_mpi_ncbicxx_2_16_10
#define mbedtls_asn1_write_null \
        mbedtls_asn1_write_null_ncbicxx_2_16_10
#define mbedtls_asn1_write_octet_string \
        mbedtls_asn1_write_octet_string_ncbicxx_2_16_10
#define mbedtls_asn1_write_oid \
        mbedtls_asn1_write_oid_ncbicxx_2_16_10
#define mbedtls_asn1_write_printable_string \
        mbedtls_asn1_write_printable_string_ncbicxx_2_16_10
#define mbedtls_asn1_write_raw_buffer \
        mbedtls_asn1_write_raw_buffer_ncbicxx_2_16_10
#define mbedtls_asn1_write_tag \
        mbedtls_asn1_write_tag_ncbicxx_2_16_10
#define mbedtls_asn1_write_tagged_string \
        mbedtls_asn1_write_tagged_string_ncbicxx_2_16_10
#define mbedtls_asn1_write_utf8_string \
        mbedtls_asn1_write_utf8_string_ncbicxx_2_16_10
#define mbedtls_base64_decode \
        mbedtls_base64_decode_ncbicxx_2_16_10
#define mbedtls_base64_encode \
        mbedtls_base64_encode_ncbicxx_2_16_10
#define mbedtls_mpi_add_abs \
        mbedtls_mpi_add_abs_ncbicxx_2_16_10
#define mbedtls_mpi_add_int \
        mbedtls_mpi_add_int_ncbicxx_2_16_10
#define mbedtls_mpi_add_mpi \
        mbedtls_mpi_add_mpi_ncbicxx_2_16_10
#define mbedtls_mpi_bitlen \
        mbedtls_mpi_bitlen_ncbicxx_2_16_10
#define mbedtls_mpi_cmp_abs \
        mbedtls_mpi_cmp_abs_ncbicxx_2_16_10
#define mbedtls_mpi_cmp_int \
        mbedtls_mpi_cmp_int_ncbicxx_2_16_10
#define mbedtls_mpi_cmp_mpi \
        mbedtls_mpi_cmp_mpi_ncbicxx_2_16_10
#define mbedtls_mpi_copy \
        mbedtls_mpi_copy_ncbicxx_2_16_10
#define mbedtls_mpi_div_int \
        mbedtls_mpi_div_int_ncbicxx_2_16_10
#define mbedtls_mpi_div_mpi \
        mbedtls_mpi_div_mpi_ncbicxx_2_16_10
#define mbedtls_mpi_exp_mod \
        mbedtls_mpi_exp_mod_ncbicxx_2_16_10
#define mbedtls_mpi_fill_random \
        mbedtls_mpi_fill_random_ncbicxx_2_16_10
#define mbedtls_mpi_free \
        mbedtls_mpi_free_ncbicxx_2_16_10
#define mbedtls_mpi_gcd \
        mbedtls_mpi_gcd_ncbicxx_2_16_10
#define mbedtls_mpi_gen_prime \
        mbedtls_mpi_gen_prime_ncbicxx_2_16_10
#define mbedtls_mpi_get_bit \
        mbedtls_mpi_get_bit_ncbicxx_2_16_10
#define mbedtls_mpi_grow \
        mbedtls_mpi_grow_ncbicxx_2_16_10
#define mbedtls_mpi_init \
        mbedtls_mpi_init_ncbicxx_2_16_10
#define mbedtls_mpi_inv_mod \
        mbedtls_mpi_inv_mod_ncbicxx_2_16_10
#define mbedtls_mpi_is_prime \
        mbedtls_mpi_is_prime_ncbicxx_2_16_10
#define mbedtls_mpi_is_prime_ext \
        mbedtls_mpi_is_prime_ext_ncbicxx_2_16_10
#define mbedtls_mpi_lsb \
        mbedtls_mpi_lsb_ncbicxx_2_16_10
#define mbedtls_mpi_lset \
        mbedtls_mpi_lset_ncbicxx_2_16_10
#define mbedtls_mpi_lt_mpi_ct \
        mbedtls_mpi_lt_mpi_ct_ncbicxx_2_16_10
#define mbedtls_mpi_mod_int \
        mbedtls_mpi_mod_int_ncbicxx_2_16_10
#define mbedtls_mpi_mod_mpi \
        mbedtls_mpi_mod_mpi_ncbicxx_2_16_10
#define mbedtls_mpi_mul_int \
        mbedtls_mpi_mul_int_ncbicxx_2_16_10
#define mbedtls_mpi_mul_mpi \
        mbedtls_mpi_mul_mpi_ncbicxx_2_16_10
#define mbedtls_mpi_read_binary \
        mbedtls_mpi_read_binary_ncbicxx_2_16_10
#define mbedtls_mpi_read_file \
        mbedtls_mpi_read_file_ncbicxx_2_16_10
#define mbedtls_mpi_read_string \
        mbedtls_mpi_read_string_ncbicxx_2_16_10
#define mbedtls_mpi_safe_cond_assign \
        mbedtls_mpi_safe_cond_assign_ncbicxx_2_16_10
#define mbedtls_mpi_safe_cond_swap \
        mbedtls_mpi_safe_cond_swap_ncbicxx_2_16_10
#define mbedtls_mpi_set_bit \
        mbedtls_mpi_set_bit_ncbicxx_2_16_10
#define mbedtls_mpi_shift_l \
        mbedtls_mpi_shift_l_ncbicxx_2_16_10
#define mbedtls_mpi_shift_r \
        mbedtls_mpi_shift_r_ncbicxx_2_16_10
#define mbedtls_mpi_shrink \
        mbedtls_mpi_shrink_ncbicxx_2_16_10
#define mbedtls_mpi_size \
        mbedtls_mpi_size_ncbicxx_2_16_10
#define mbedtls_mpi_sub_abs \
        mbedtls_mpi_sub_abs_ncbicxx_2_16_10
#define mbedtls_mpi_sub_int \
        mbedtls_mpi_sub_int_ncbicxx_2_16_10
#define mbedtls_mpi_sub_mpi \
        mbedtls_mpi_sub_mpi_ncbicxx_2_16_10
#define mbedtls_mpi_swap \
        mbedtls_mpi_swap_ncbicxx_2_16_10
#define mbedtls_mpi_write_binary \
        mbedtls_mpi_write_binary_ncbicxx_2_16_10
#define mbedtls_mpi_write_file \
        mbedtls_mpi_write_file_ncbicxx_2_16_10
#define mbedtls_mpi_write_string \
        mbedtls_mpi_write_string_ncbicxx_2_16_10
#define mbedtls_blowfish_crypt_cbc \
        mbedtls_blowfish_crypt_cbc_ncbicxx_2_16_10
#define mbedtls_blowfish_crypt_cfb64 \
        mbedtls_blowfish_crypt_cfb64_ncbicxx_2_16_10
#define mbedtls_blowfish_crypt_ctr \
        mbedtls_blowfish_crypt_ctr_ncbicxx_2_16_10
#define mbedtls_blowfish_crypt_ecb \
        mbedtls_blowfish_crypt_ecb_ncbicxx_2_16_10
#define mbedtls_blowfish_free \
        mbedtls_blowfish_free_ncbicxx_2_16_10
#define mbedtls_blowfish_init \
        mbedtls_blowfish_init_ncbicxx_2_16_10
#define mbedtls_blowfish_setkey \
        mbedtls_blowfish_setkey_ncbicxx_2_16_10
#define mbedtls_camellia_crypt_cbc \
        mbedtls_camellia_crypt_cbc_ncbicxx_2_16_10
#define mbedtls_camellia_crypt_cfb128 \
        mbedtls_camellia_crypt_cfb128_ncbicxx_2_16_10
#define mbedtls_camellia_crypt_ctr \
        mbedtls_camellia_crypt_ctr_ncbicxx_2_16_10
#define mbedtls_camellia_crypt_ecb \
        mbedtls_camellia_crypt_ecb_ncbicxx_2_16_10
#define mbedtls_camellia_free \
        mbedtls_camellia_free_ncbicxx_2_16_10
#define mbedtls_camellia_init \
        mbedtls_camellia_init_ncbicxx_2_16_10
#define mbedtls_camellia_setkey_dec \
        mbedtls_camellia_setkey_dec_ncbicxx_2_16_10
#define mbedtls_camellia_setkey_enc \
        mbedtls_camellia_setkey_enc_ncbicxx_2_16_10
#define mbedtls_ccm_auth_decrypt \
        mbedtls_ccm_auth_decrypt_ncbicxx_2_16_10
#define mbedtls_ccm_encrypt_and_tag \
        mbedtls_ccm_encrypt_and_tag_ncbicxx_2_16_10
#define mbedtls_ccm_free \
        mbedtls_ccm_free_ncbicxx_2_16_10
#define mbedtls_ccm_init \
        mbedtls_ccm_init_ncbicxx_2_16_10
#define mbedtls_ccm_setkey \
        mbedtls_ccm_setkey_ncbicxx_2_16_10
#define mbedtls_ccm_star_auth_decrypt \
        mbedtls_ccm_star_auth_decrypt_ncbicxx_2_16_10
#define mbedtls_ccm_star_encrypt_and_tag \
        mbedtls_ccm_star_encrypt_and_tag_ncbicxx_2_16_10
#define mbedtls_test_ca_crt \
        mbedtls_test_ca_crt_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_ec \
        mbedtls_test_ca_crt_ec_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_ec_der \
        mbedtls_test_ca_crt_ec_der_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_ec_der_len \
        mbedtls_test_ca_crt_ec_der_len_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_ec_len \
        mbedtls_test_ca_crt_ec_len_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_ec_pem \
        mbedtls_test_ca_crt_ec_pem_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_ec_pem_len \
        mbedtls_test_ca_crt_ec_pem_len_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_len \
        mbedtls_test_ca_crt_len_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_rsa \
        mbedtls_test_ca_crt_rsa_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_rsa_len \
        mbedtls_test_ca_crt_rsa_len_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_rsa_sha1 \
        mbedtls_test_ca_crt_rsa_sha1_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_rsa_sha1_der \
        mbedtls_test_ca_crt_rsa_sha1_der_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_rsa_sha1_der_len \
        mbedtls_test_ca_crt_rsa_sha1_der_len_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_rsa_sha1_len \
        mbedtls_test_ca_crt_rsa_sha1_len_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_rsa_sha1_pem \
        mbedtls_test_ca_crt_rsa_sha1_pem_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_rsa_sha1_pem_len \
        mbedtls_test_ca_crt_rsa_sha1_pem_len_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_rsa_sha256 \
        mbedtls_test_ca_crt_rsa_sha256_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_rsa_sha256_der \
        mbedtls_test_ca_crt_rsa_sha256_der_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_rsa_sha256_der_len \
        mbedtls_test_ca_crt_rsa_sha256_der_len_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_rsa_sha256_len \
        mbedtls_test_ca_crt_rsa_sha256_len_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_rsa_sha256_pem \
        mbedtls_test_ca_crt_rsa_sha256_pem_ncbicxx_2_16_10
#define mbedtls_test_ca_crt_rsa_sha256_pem_len \
        mbedtls_test_ca_crt_rsa_sha256_pem_len_ncbicxx_2_16_10
#define mbedtls_test_ca_key \
        mbedtls_test_ca_key_ncbicxx_2_16_10
#define mbedtls_test_ca_key_ec \
        mbedtls_test_ca_key_ec_ncbicxx_2_16_10
#define mbedtls_test_ca_key_ec_der \
        mbedtls_test_ca_key_ec_der_ncbicxx_2_16_10
#define mbedtls_test_ca_key_ec_der_len \
        mbedtls_test_ca_key_ec_der_len_ncbicxx_2_16_10
#define mbedtls_test_ca_key_ec_len \
        mbedtls_test_ca_key_ec_len_ncbicxx_2_16_10
#define mbedtls_test_ca_key_ec_pem \
        mbedtls_test_ca_key_ec_pem_ncbicxx_2_16_10
#define mbedtls_test_ca_key_ec_pem_len \
        mbedtls_test_ca_key_ec_pem_len_ncbicxx_2_16_10
#define mbedtls_test_ca_key_len \
        mbedtls_test_ca_key_len_ncbicxx_2_16_10
#define mbedtls_test_ca_key_rsa \
        mbedtls_test_ca_key_rsa_ncbicxx_2_16_10
#define mbedtls_test_ca_key_rsa_der \
        mbedtls_test_ca_key_rsa_der_ncbicxx_2_16_10
#define mbedtls_test_ca_key_rsa_der_len \
        mbedtls_test_ca_key_rsa_der_len_ncbicxx_2_16_10
#define mbedtls_test_ca_key_rsa_len \
        mbedtls_test_ca_key_rsa_len_ncbicxx_2_16_10
#define mbedtls_test_ca_key_rsa_pem \
        mbedtls_test_ca_key_rsa_pem_ncbicxx_2_16_10
#define mbedtls_test_ca_key_rsa_pem_len \
        mbedtls_test_ca_key_rsa_pem_len_ncbicxx_2_16_10
#define mbedtls_test_ca_pwd \
        mbedtls_test_ca_pwd_ncbicxx_2_16_10
#define mbedtls_test_ca_pwd_ec \
        mbedtls_test_ca_pwd_ec_ncbicxx_2_16_10
#define mbedtls_test_ca_pwd_ec_der_len \
        mbedtls_test_ca_pwd_ec_der_len_ncbicxx_2_16_10
#define mbedtls_test_ca_pwd_ec_len \
        mbedtls_test_ca_pwd_ec_len_ncbicxx_2_16_10
#define mbedtls_test_ca_pwd_ec_pem \
        mbedtls_test_ca_pwd_ec_pem_ncbicxx_2_16_10
#define mbedtls_test_ca_pwd_ec_pem_len \
        mbedtls_test_ca_pwd_ec_pem_len_ncbicxx_2_16_10
#define mbedtls_test_ca_pwd_len \
        mbedtls_test_ca_pwd_len_ncbicxx_2_16_10
#define mbedtls_test_ca_pwd_rsa \
        mbedtls_test_ca_pwd_rsa_ncbicxx_2_16_10
#define mbedtls_test_ca_pwd_rsa_der_len \
        mbedtls_test_ca_pwd_rsa_der_len_ncbicxx_2_16_10
#define mbedtls_test_ca_pwd_rsa_len \
        mbedtls_test_ca_pwd_rsa_len_ncbicxx_2_16_10
#define mbedtls_test_ca_pwd_rsa_pem \
        mbedtls_test_ca_pwd_rsa_pem_ncbicxx_2_16_10
#define mbedtls_test_ca_pwd_rsa_pem_len \
        mbedtls_test_ca_pwd_rsa_pem_len_ncbicxx_2_16_10
#define mbedtls_test_cas \
        mbedtls_test_cas_ncbicxx_2_16_10
#define mbedtls_test_cas_der \
        mbedtls_test_cas_der_ncbicxx_2_16_10
#define mbedtls_test_cas_der_len \
        mbedtls_test_cas_der_len_ncbicxx_2_16_10
#define mbedtls_test_cas_len \
        mbedtls_test_cas_len_ncbicxx_2_16_10
#define mbedtls_test_cas_pem \
        mbedtls_test_cas_pem_ncbicxx_2_16_10
#define mbedtls_test_cas_pem_len \
        mbedtls_test_cas_pem_len_ncbicxx_2_16_10
#define mbedtls_test_cli_crt \
        mbedtls_test_cli_crt_ncbicxx_2_16_10
#define mbedtls_test_cli_crt_ec \
        mbedtls_test_cli_crt_ec_ncbicxx_2_16_10
#define mbedtls_test_cli_crt_ec_der \
        mbedtls_test_cli_crt_ec_der_ncbicxx_2_16_10
#define mbedtls_test_cli_crt_ec_der_len \
        mbedtls_test_cli_crt_ec_der_len_ncbicxx_2_16_10
#define mbedtls_test_cli_crt_ec_len \
        mbedtls_test_cli_crt_ec_len_ncbicxx_2_16_10
#define mbedtls_test_cli_crt_ec_pem \
        mbedtls_test_cli_crt_ec_pem_ncbicxx_2_16_10
#define mbedtls_test_cli_crt_ec_pem_len \
        mbedtls_test_cli_crt_ec_pem_len_ncbicxx_2_16_10
#define mbedtls_test_cli_crt_len \
        mbedtls_test_cli_crt_len_ncbicxx_2_16_10
#define mbedtls_test_cli_crt_rsa \
        mbedtls_test_cli_crt_rsa_ncbicxx_2_16_10
#define mbedtls_test_cli_crt_rsa_der \
        mbedtls_test_cli_crt_rsa_der_ncbicxx_2_16_10
#define mbedtls_test_cli_crt_rsa_der_len \
        mbedtls_test_cli_crt_rsa_der_len_ncbicxx_2_16_10
#define mbedtls_test_cli_crt_rsa_len \
        mbedtls_test_cli_crt_rsa_len_ncbicxx_2_16_10
#define mbedtls_test_cli_crt_rsa_pem \
        mbedtls_test_cli_crt_rsa_pem_ncbicxx_2_16_10
#define mbedtls_test_cli_crt_rsa_pem_len \
        mbedtls_test_cli_crt_rsa_pem_len_ncbicxx_2_16_10
#define mbedtls_test_cli_key \
        mbedtls_test_cli_key_ncbicxx_2_16_10
#define mbedtls_test_cli_key_ec \
        mbedtls_test_cli_key_ec_ncbicxx_2_16_10
#define mbedtls_test_cli_key_ec_der \
        mbedtls_test_cli_key_ec_der_ncbicxx_2_16_10
#define mbedtls_test_cli_key_ec_der_len \
        mbedtls_test_cli_key_ec_der_len_ncbicxx_2_16_10
#define mbedtls_test_cli_key_ec_len \
        mbedtls_test_cli_key_ec_len_ncbicxx_2_16_10
#define mbedtls_test_cli_key_ec_pem \
        mbedtls_test_cli_key_ec_pem_ncbicxx_2_16_10
#define mbedtls_test_cli_key_ec_pem_len \
        mbedtls_test_cli_key_ec_pem_len_ncbicxx_2_16_10
#define mbedtls_test_cli_key_len \
        mbedtls_test_cli_key_len_ncbicxx_2_16_10
#define mbedtls_test_cli_key_rsa \
        mbedtls_test_cli_key_rsa_ncbicxx_2_16_10
#define mbedtls_test_cli_key_rsa_der \
        mbedtls_test_cli_key_rsa_der_ncbicxx_2_16_10
#define mbedtls_test_cli_key_rsa_der_len \
        mbedtls_test_cli_key_rsa_der_len_ncbicxx_2_16_10
#define mbedtls_test_cli_key_rsa_len \
        mbedtls_test_cli_key_rsa_len_ncbicxx_2_16_10
#define mbedtls_test_cli_key_rsa_pem \
        mbedtls_test_cli_key_rsa_pem_ncbicxx_2_16_10
#define mbedtls_test_cli_key_rsa_pem_len \
        mbedtls_test_cli_key_rsa_pem_len_ncbicxx_2_16_10
#define mbedtls_test_cli_pwd \
        mbedtls_test_cli_pwd_ncbicxx_2_16_10
#define mbedtls_test_cli_pwd_ec \
        mbedtls_test_cli_pwd_ec_ncbicxx_2_16_10
#define mbedtls_test_cli_pwd_ec_len \
        mbedtls_test_cli_pwd_ec_len_ncbicxx_2_16_10
#define mbedtls_test_cli_pwd_ec_pem \
        mbedtls_test_cli_pwd_ec_pem_ncbicxx_2_16_10
#define mbedtls_test_cli_pwd_ec_pem_len \
        mbedtls_test_cli_pwd_ec_pem_len_ncbicxx_2_16_10
#define mbedtls_test_cli_pwd_len \
        mbedtls_test_cli_pwd_len_ncbicxx_2_16_10
#define mbedtls_test_cli_pwd_rsa \
        mbedtls_test_cli_pwd_rsa_ncbicxx_2_16_10
#define mbedtls_test_cli_pwd_rsa_len \
        mbedtls_test_cli_pwd_rsa_len_ncbicxx_2_16_10
#define mbedtls_test_cli_pwd_rsa_pem \
        mbedtls_test_cli_pwd_rsa_pem_ncbicxx_2_16_10
#define mbedtls_test_cli_pwd_rsa_pem_len \
        mbedtls_test_cli_pwd_rsa_pem_len_ncbicxx_2_16_10
#define mbedtls_test_srv_crt \
        mbedtls_test_srv_crt_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_ec \
        mbedtls_test_srv_crt_ec_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_ec_der \
        mbedtls_test_srv_crt_ec_der_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_ec_der_len \
        mbedtls_test_srv_crt_ec_der_len_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_ec_len \
        mbedtls_test_srv_crt_ec_len_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_ec_pem \
        mbedtls_test_srv_crt_ec_pem_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_ec_pem_len \
        mbedtls_test_srv_crt_ec_pem_len_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_len \
        mbedtls_test_srv_crt_len_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_rsa \
        mbedtls_test_srv_crt_rsa_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_rsa_len \
        mbedtls_test_srv_crt_rsa_len_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_rsa_sha1 \
        mbedtls_test_srv_crt_rsa_sha1_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_rsa_sha1_der \
        mbedtls_test_srv_crt_rsa_sha1_der_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_rsa_sha1_der_len \
        mbedtls_test_srv_crt_rsa_sha1_der_len_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_rsa_sha1_len \
        mbedtls_test_srv_crt_rsa_sha1_len_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_rsa_sha1_pem \
        mbedtls_test_srv_crt_rsa_sha1_pem_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_rsa_sha1_pem_len \
        mbedtls_test_srv_crt_rsa_sha1_pem_len_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_rsa_sha256 \
        mbedtls_test_srv_crt_rsa_sha256_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_rsa_sha256_der \
        mbedtls_test_srv_crt_rsa_sha256_der_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_rsa_sha256_der_len \
        mbedtls_test_srv_crt_rsa_sha256_der_len_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_rsa_sha256_len \
        mbedtls_test_srv_crt_rsa_sha256_len_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_rsa_sha256_pem \
        mbedtls_test_srv_crt_rsa_sha256_pem_ncbicxx_2_16_10
#define mbedtls_test_srv_crt_rsa_sha256_pem_len \
        mbedtls_test_srv_crt_rsa_sha256_pem_len_ncbicxx_2_16_10
#define mbedtls_test_srv_key \
        mbedtls_test_srv_key_ncbicxx_2_16_10
#define mbedtls_test_srv_key_ec \
        mbedtls_test_srv_key_ec_ncbicxx_2_16_10
#define mbedtls_test_srv_key_ec_der \
        mbedtls_test_srv_key_ec_der_ncbicxx_2_16_10
#define mbedtls_test_srv_key_ec_der_len \
        mbedtls_test_srv_key_ec_der_len_ncbicxx_2_16_10
#define mbedtls_test_srv_key_ec_len \
        mbedtls_test_srv_key_ec_len_ncbicxx_2_16_10
#define mbedtls_test_srv_key_ec_pem \
        mbedtls_test_srv_key_ec_pem_ncbicxx_2_16_10
#define mbedtls_test_srv_key_ec_pem_len \
        mbedtls_test_srv_key_ec_pem_len_ncbicxx_2_16_10
#define mbedtls_test_srv_key_len \
        mbedtls_test_srv_key_len_ncbicxx_2_16_10
#define mbedtls_test_srv_key_rsa \
        mbedtls_test_srv_key_rsa_ncbicxx_2_16_10
#define mbedtls_test_srv_key_rsa_der \
        mbedtls_test_srv_key_rsa_der_ncbicxx_2_16_10
#define mbedtls_test_srv_key_rsa_der_len \
        mbedtls_test_srv_key_rsa_der_len_ncbicxx_2_16_10
#define mbedtls_test_srv_key_rsa_len \
        mbedtls_test_srv_key_rsa_len_ncbicxx_2_16_10
#define mbedtls_test_srv_key_rsa_pem \
        mbedtls_test_srv_key_rsa_pem_ncbicxx_2_16_10
#define mbedtls_test_srv_key_rsa_pem_len \
        mbedtls_test_srv_key_rsa_pem_len_ncbicxx_2_16_10
#define mbedtls_test_srv_pwd \
        mbedtls_test_srv_pwd_ncbicxx_2_16_10
#define mbedtls_test_srv_pwd_ec \
        mbedtls_test_srv_pwd_ec_ncbicxx_2_16_10
#define mbedtls_test_srv_pwd_ec_der_len \
        mbedtls_test_srv_pwd_ec_der_len_ncbicxx_2_16_10
#define mbedtls_test_srv_pwd_ec_len \
        mbedtls_test_srv_pwd_ec_len_ncbicxx_2_16_10
#define mbedtls_test_srv_pwd_ec_pem \
        mbedtls_test_srv_pwd_ec_pem_ncbicxx_2_16_10
#define mbedtls_test_srv_pwd_ec_pem_len \
        mbedtls_test_srv_pwd_ec_pem_len_ncbicxx_2_16_10
#define mbedtls_test_srv_pwd_len \
        mbedtls_test_srv_pwd_len_ncbicxx_2_16_10
#define mbedtls_test_srv_pwd_rsa \
        mbedtls_test_srv_pwd_rsa_ncbicxx_2_16_10
#define mbedtls_test_srv_pwd_rsa_der_len \
        mbedtls_test_srv_pwd_rsa_der_len_ncbicxx_2_16_10
#define mbedtls_test_srv_pwd_rsa_len \
        mbedtls_test_srv_pwd_rsa_len_ncbicxx_2_16_10
#define mbedtls_test_srv_pwd_rsa_pem \
        mbedtls_test_srv_pwd_rsa_pem_ncbicxx_2_16_10
#define mbedtls_test_srv_pwd_rsa_pem_len \
        mbedtls_test_srv_pwd_rsa_pem_len_ncbicxx_2_16_10
#define mbedtls_chacha20_crypt \
        mbedtls_chacha20_crypt_ncbicxx_2_16_10
#define mbedtls_chacha20_free \
        mbedtls_chacha20_free_ncbicxx_2_16_10
#define mbedtls_chacha20_init \
        mbedtls_chacha20_init_ncbicxx_2_16_10
#define mbedtls_chacha20_setkey \
        mbedtls_chacha20_setkey_ncbicxx_2_16_10
#define mbedtls_chacha20_starts \
        mbedtls_chacha20_starts_ncbicxx_2_16_10
#define mbedtls_chacha20_update \
        mbedtls_chacha20_update_ncbicxx_2_16_10
#define mbedtls_chachapoly_auth_decrypt \
        mbedtls_chachapoly_auth_decrypt_ncbicxx_2_16_10
#define mbedtls_chachapoly_encrypt_and_tag \
        mbedtls_chachapoly_encrypt_and_tag_ncbicxx_2_16_10
#define mbedtls_chachapoly_finish \
        mbedtls_chachapoly_finish_ncbicxx_2_16_10
#define mbedtls_chachapoly_free \
        mbedtls_chachapoly_free_ncbicxx_2_16_10
#define mbedtls_chachapoly_init \
        mbedtls_chachapoly_init_ncbicxx_2_16_10
#define mbedtls_chachapoly_setkey \
        mbedtls_chachapoly_setkey_ncbicxx_2_16_10
#define mbedtls_chachapoly_starts \
        mbedtls_chachapoly_starts_ncbicxx_2_16_10
#define mbedtls_chachapoly_update \
        mbedtls_chachapoly_update_ncbicxx_2_16_10
#define mbedtls_chachapoly_update_aad \
        mbedtls_chachapoly_update_aad_ncbicxx_2_16_10
#define mbedtls_cipher_auth_decrypt \
        mbedtls_cipher_auth_decrypt_ncbicxx_2_16_10
#define mbedtls_cipher_auth_encrypt \
        mbedtls_cipher_auth_encrypt_ncbicxx_2_16_10
#define mbedtls_cipher_check_tag \
        mbedtls_cipher_check_tag_ncbicxx_2_16_10
#define mbedtls_cipher_crypt \
        mbedtls_cipher_crypt_ncbicxx_2_16_10
#define mbedtls_cipher_finish \
        mbedtls_cipher_finish_ncbicxx_2_16_10
#define mbedtls_cipher_free \
        mbedtls_cipher_free_ncbicxx_2_16_10
#define mbedtls_cipher_info_from_string \
        mbedtls_cipher_info_from_string_ncbicxx_2_16_10
#define mbedtls_cipher_info_from_type \
        mbedtls_cipher_info_from_type_ncbicxx_2_16_10
#define mbedtls_cipher_info_from_values \
        mbedtls_cipher_info_from_values_ncbicxx_2_16_10
#define mbedtls_cipher_init \
        mbedtls_cipher_init_ncbicxx_2_16_10
#define mbedtls_cipher_list \
        mbedtls_cipher_list_ncbicxx_2_16_10
#define mbedtls_cipher_reset \
        mbedtls_cipher_reset_ncbicxx_2_16_10
#define mbedtls_cipher_set_iv \
        mbedtls_cipher_set_iv_ncbicxx_2_16_10
#define mbedtls_cipher_set_padding_mode \
        mbedtls_cipher_set_padding_mode_ncbicxx_2_16_10
#define mbedtls_cipher_setkey \
        mbedtls_cipher_setkey_ncbicxx_2_16_10
#define mbedtls_cipher_setup \
        mbedtls_cipher_setup_ncbicxx_2_16_10
#define mbedtls_cipher_update \
        mbedtls_cipher_update_ncbicxx_2_16_10
#define mbedtls_cipher_update_ad \
        mbedtls_cipher_update_ad_ncbicxx_2_16_10
#define mbedtls_cipher_write_tag \
        mbedtls_cipher_write_tag_ncbicxx_2_16_10
#define mbedtls_cipher_definitions \
        mbedtls_cipher_definitions_ncbicxx_2_16_10
#define mbedtls_cipher_supported \
        mbedtls_cipher_supported_ncbicxx_2_16_10
#define mbedtls_ctr_drbg_free \
        mbedtls_ctr_drbg_free_ncbicxx_2_16_10
#define mbedtls_ctr_drbg_init \
        mbedtls_ctr_drbg_init_ncbicxx_2_16_10
#define mbedtls_ctr_drbg_random \
        mbedtls_ctr_drbg_random_ncbicxx_2_16_10
#define mbedtls_ctr_drbg_random_with_add \
        mbedtls_ctr_drbg_random_with_add_ncbicxx_2_16_10
#define mbedtls_ctr_drbg_reseed \
        mbedtls_ctr_drbg_reseed_ncbicxx_2_16_10
#define mbedtls_ctr_drbg_seed \
        mbedtls_ctr_drbg_seed_ncbicxx_2_16_10
#define mbedtls_ctr_drbg_seed_entropy_len \
        mbedtls_ctr_drbg_seed_entropy_len_ncbicxx_2_16_10
#define mbedtls_ctr_drbg_set_entropy_len \
        mbedtls_ctr_drbg_set_entropy_len_ncbicxx_2_16_10
#define mbedtls_ctr_drbg_set_prediction_resistance \
        mbedtls_ctr_drbg_set_prediction_resistance_ncbicxx_2_16_10
#define mbedtls_ctr_drbg_set_reseed_interval \
        mbedtls_ctr_drbg_set_reseed_interval_ncbicxx_2_16_10
#define mbedtls_ctr_drbg_update \
        mbedtls_ctr_drbg_update_ncbicxx_2_16_10
#define mbedtls_ctr_drbg_update_ret \
        mbedtls_ctr_drbg_update_ret_ncbicxx_2_16_10
#define mbedtls_ctr_drbg_update_seed_file \
        mbedtls_ctr_drbg_update_seed_file_ncbicxx_2_16_10
#define mbedtls_ctr_drbg_write_seed_file \
        mbedtls_ctr_drbg_write_seed_file_ncbicxx_2_16_10
#define mbedtls_debug_print_buf \
        mbedtls_debug_print_buf_ncbicxx_2_16_10
#define mbedtls_debug_print_crt \
        mbedtls_debug_print_crt_ncbicxx_2_16_10
#define mbedtls_debug_print_ecp \
        mbedtls_debug_print_ecp_ncbicxx_2_16_10
#define mbedtls_debug_print_mpi \
        mbedtls_debug_print_mpi_ncbicxx_2_16_10
#define mbedtls_debug_print_msg \
        mbedtls_debug_print_msg_ncbicxx_2_16_10
#define mbedtls_debug_print_ret \
        mbedtls_debug_print_ret_ncbicxx_2_16_10
#define mbedtls_debug_printf_ecdh \
        mbedtls_debug_printf_ecdh_ncbicxx_2_16_10
#define mbedtls_debug_set_threshold \
        mbedtls_debug_set_threshold_ncbicxx_2_16_10
#define mbedtls_des3_crypt_cbc \
        mbedtls_des3_crypt_cbc_ncbicxx_2_16_10
#define mbedtls_des3_crypt_ecb \
        mbedtls_des3_crypt_ecb_ncbicxx_2_16_10
#define mbedtls_des3_free \
        mbedtls_des3_free_ncbicxx_2_16_10
#define mbedtls_des3_init \
        mbedtls_des3_init_ncbicxx_2_16_10
#define mbedtls_des3_set2key_dec \
        mbedtls_des3_set2key_dec_ncbicxx_2_16_10
#define mbedtls_des3_set2key_enc \
        mbedtls_des3_set2key_enc_ncbicxx_2_16_10
#define mbedtls_des3_set3key_dec \
        mbedtls_des3_set3key_dec_ncbicxx_2_16_10
#define mbedtls_des3_set3key_enc \
        mbedtls_des3_set3key_enc_ncbicxx_2_16_10
#define mbedtls_des_crypt_cbc \
        mbedtls_des_crypt_cbc_ncbicxx_2_16_10
#define mbedtls_des_crypt_ecb \
        mbedtls_des_crypt_ecb_ncbicxx_2_16_10
#define mbedtls_des_free \
        mbedtls_des_free_ncbicxx_2_16_10
#define mbedtls_des_init \
        mbedtls_des_init_ncbicxx_2_16_10
#define mbedtls_des_key_check_key_parity \
        mbedtls_des_key_check_key_parity_ncbicxx_2_16_10
#define mbedtls_des_key_check_weak \
        mbedtls_des_key_check_weak_ncbicxx_2_16_10
#define mbedtls_des_key_set_parity \
        mbedtls_des_key_set_parity_ncbicxx_2_16_10
#define mbedtls_des_setkey \
        mbedtls_des_setkey_ncbicxx_2_16_10
#define mbedtls_des_setkey_dec \
        mbedtls_des_setkey_dec_ncbicxx_2_16_10
#define mbedtls_des_setkey_enc \
        mbedtls_des_setkey_enc_ncbicxx_2_16_10
#define mbedtls_dhm_calc_secret \
        mbedtls_dhm_calc_secret_ncbicxx_2_16_10
#define mbedtls_dhm_free \
        mbedtls_dhm_free_ncbicxx_2_16_10
#define mbedtls_dhm_init \
        mbedtls_dhm_init_ncbicxx_2_16_10
#define mbedtls_dhm_make_params \
        mbedtls_dhm_make_params_ncbicxx_2_16_10
#define mbedtls_dhm_make_public \
        mbedtls_dhm_make_public_ncbicxx_2_16_10
#define mbedtls_dhm_parse_dhm \
        mbedtls_dhm_parse_dhm_ncbicxx_2_16_10
#define mbedtls_dhm_parse_dhmfile \
        mbedtls_dhm_parse_dhmfile_ncbicxx_2_16_10
#define mbedtls_dhm_read_params \
        mbedtls_dhm_read_params_ncbicxx_2_16_10
#define mbedtls_dhm_read_public \
        mbedtls_dhm_read_public_ncbicxx_2_16_10
#define mbedtls_dhm_set_group \
        mbedtls_dhm_set_group_ncbicxx_2_16_10
#define mbedtls_ecdh_calc_secret \
        mbedtls_ecdh_calc_secret_ncbicxx_2_16_10
#define mbedtls_ecdh_compute_shared \
        mbedtls_ecdh_compute_shared_ncbicxx_2_16_10
#define mbedtls_ecdh_free \
        mbedtls_ecdh_free_ncbicxx_2_16_10
#define mbedtls_ecdh_gen_public \
        mbedtls_ecdh_gen_public_ncbicxx_2_16_10
#define mbedtls_ecdh_get_params \
        mbedtls_ecdh_get_params_ncbicxx_2_16_10
#define mbedtls_ecdh_init \
        mbedtls_ecdh_init_ncbicxx_2_16_10
#define mbedtls_ecdh_make_params \
        mbedtls_ecdh_make_params_ncbicxx_2_16_10
#define mbedtls_ecdh_make_public \
        mbedtls_ecdh_make_public_ncbicxx_2_16_10
#define mbedtls_ecdh_read_params \
        mbedtls_ecdh_read_params_ncbicxx_2_16_10
#define mbedtls_ecdh_read_public \
        mbedtls_ecdh_read_public_ncbicxx_2_16_10
#define mbedtls_ecdh_setup \
        mbedtls_ecdh_setup_ncbicxx_2_16_10
#define mbedtls_ecdsa_free \
        mbedtls_ecdsa_free_ncbicxx_2_16_10
#define mbedtls_ecdsa_from_keypair \
        mbedtls_ecdsa_from_keypair_ncbicxx_2_16_10
#define mbedtls_ecdsa_genkey \
        mbedtls_ecdsa_genkey_ncbicxx_2_16_10
#define mbedtls_ecdsa_init \
        mbedtls_ecdsa_init_ncbicxx_2_16_10
#define mbedtls_ecdsa_read_signature \
        mbedtls_ecdsa_read_signature_ncbicxx_2_16_10
#define mbedtls_ecdsa_read_signature_restartable \
        mbedtls_ecdsa_read_signature_restartable_ncbicxx_2_16_10
#define mbedtls_ecdsa_sign \
        mbedtls_ecdsa_sign_ncbicxx_2_16_10
#define mbedtls_ecdsa_sign_det \
        mbedtls_ecdsa_sign_det_ncbicxx_2_16_10
#define mbedtls_ecdsa_sign_det_ext \
        mbedtls_ecdsa_sign_det_ext_ncbicxx_2_16_10
#define mbedtls_ecdsa_verify \
        mbedtls_ecdsa_verify_ncbicxx_2_16_10
#define mbedtls_ecdsa_write_signature \
        mbedtls_ecdsa_write_signature_ncbicxx_2_16_10
#define mbedtls_ecdsa_write_signature_det \
        mbedtls_ecdsa_write_signature_det_ncbicxx_2_16_10
#define mbedtls_ecdsa_write_signature_restartable \
        mbedtls_ecdsa_write_signature_restartable_ncbicxx_2_16_10
#define mbedtls_ecp_check_privkey \
        mbedtls_ecp_check_privkey_ncbicxx_2_16_10
#define mbedtls_ecp_check_pub_priv \
        mbedtls_ecp_check_pub_priv_ncbicxx_2_16_10
#define mbedtls_ecp_check_pubkey \
        mbedtls_ecp_check_pubkey_ncbicxx_2_16_10
#define mbedtls_ecp_copy \
        mbedtls_ecp_copy_ncbicxx_2_16_10
#define mbedtls_ecp_curve_info_from_grp_id \
        mbedtls_ecp_curve_info_from_grp_id_ncbicxx_2_16_10
#define mbedtls_ecp_curve_info_from_name \
        mbedtls_ecp_curve_info_from_name_ncbicxx_2_16_10
#define mbedtls_ecp_curve_info_from_tls_id \
        mbedtls_ecp_curve_info_from_tls_id_ncbicxx_2_16_10
#define mbedtls_ecp_curve_list \
        mbedtls_ecp_curve_list_ncbicxx_2_16_10
#define mbedtls_ecp_gen_key \
        mbedtls_ecp_gen_key_ncbicxx_2_16_10
#define mbedtls_ecp_gen_keypair \
        mbedtls_ecp_gen_keypair_ncbicxx_2_16_10
#define mbedtls_ecp_gen_keypair_base \
        mbedtls_ecp_gen_keypair_base_ncbicxx_2_16_10
#define mbedtls_ecp_gen_privkey \
        mbedtls_ecp_gen_privkey_ncbicxx_2_16_10
#define mbedtls_ecp_group_copy \
        mbedtls_ecp_group_copy_ncbicxx_2_16_10
#define mbedtls_ecp_group_free \
        mbedtls_ecp_group_free_ncbicxx_2_16_10
#define mbedtls_ecp_group_init \
        mbedtls_ecp_group_init_ncbicxx_2_16_10
#define mbedtls_ecp_grp_id_list \
        mbedtls_ecp_grp_id_list_ncbicxx_2_16_10
#define mbedtls_ecp_is_zero \
        mbedtls_ecp_is_zero_ncbicxx_2_16_10
#define mbedtls_ecp_keypair_free \
        mbedtls_ecp_keypair_free_ncbicxx_2_16_10
#define mbedtls_ecp_keypair_init \
        mbedtls_ecp_keypair_init_ncbicxx_2_16_10
#define mbedtls_ecp_mul \
        mbedtls_ecp_mul_ncbicxx_2_16_10
#define mbedtls_ecp_mul_restartable \
        mbedtls_ecp_mul_restartable_ncbicxx_2_16_10
#define mbedtls_ecp_muladd \
        mbedtls_ecp_muladd_ncbicxx_2_16_10
#define mbedtls_ecp_muladd_restartable \
        mbedtls_ecp_muladd_restartable_ncbicxx_2_16_10
#define mbedtls_ecp_point_cmp \
        mbedtls_ecp_point_cmp_ncbicxx_2_16_10
#define mbedtls_ecp_point_free \
        mbedtls_ecp_point_free_ncbicxx_2_16_10
#define mbedtls_ecp_point_init \
        mbedtls_ecp_point_init_ncbicxx_2_16_10
#define mbedtls_ecp_point_read_binary \
        mbedtls_ecp_point_read_binary_ncbicxx_2_16_10
#define mbedtls_ecp_point_read_string \
        mbedtls_ecp_point_read_string_ncbicxx_2_16_10
#define mbedtls_ecp_point_write_binary \
        mbedtls_ecp_point_write_binary_ncbicxx_2_16_10
#define mbedtls_ecp_set_zero \
        mbedtls_ecp_set_zero_ncbicxx_2_16_10
#define mbedtls_ecp_tls_read_group \
        mbedtls_ecp_tls_read_group_ncbicxx_2_16_10
#define mbedtls_ecp_tls_read_group_id \
        mbedtls_ecp_tls_read_group_id_ncbicxx_2_16_10
#define mbedtls_ecp_tls_read_point \
        mbedtls_ecp_tls_read_point_ncbicxx_2_16_10
#define mbedtls_ecp_tls_write_group \
        mbedtls_ecp_tls_write_group_ncbicxx_2_16_10
#define mbedtls_ecp_tls_write_point \
        mbedtls_ecp_tls_write_point_ncbicxx_2_16_10
#define mbedtls_ecp_group_load \
        mbedtls_ecp_group_load_ncbicxx_2_16_10
#define mbedtls_entropy_add_source \
        mbedtls_entropy_add_source_ncbicxx_2_16_10
#define mbedtls_entropy_free \
        mbedtls_entropy_free_ncbicxx_2_16_10
#define mbedtls_entropy_func \
        mbedtls_entropy_func_ncbicxx_2_16_10
#define mbedtls_entropy_gather \
        mbedtls_entropy_gather_ncbicxx_2_16_10
#define mbedtls_entropy_init \
        mbedtls_entropy_init_ncbicxx_2_16_10
#define mbedtls_entropy_update_manual \
        mbedtls_entropy_update_manual_ncbicxx_2_16_10
#define mbedtls_entropy_update_seed_file \
        mbedtls_entropy_update_seed_file_ncbicxx_2_16_10
#define mbedtls_entropy_write_seed_file \
        mbedtls_entropy_write_seed_file_ncbicxx_2_16_10
#define mbedtls_hardclock_poll \
        mbedtls_hardclock_poll_ncbicxx_2_16_10
#define mbedtls_platform_entropy_poll \
        mbedtls_platform_entropy_poll_ncbicxx_2_16_10
#define mbedtls_strerror \
        mbedtls_strerror_ncbicxx_2_16_10
#define mbedtls_gcm_auth_decrypt \
        mbedtls_gcm_auth_decrypt_ncbicxx_2_16_10
#define mbedtls_gcm_crypt_and_tag \
        mbedtls_gcm_crypt_and_tag_ncbicxx_2_16_10
#define mbedtls_gcm_finish \
        mbedtls_gcm_finish_ncbicxx_2_16_10
#define mbedtls_gcm_free \
        mbedtls_gcm_free_ncbicxx_2_16_10
#define mbedtls_gcm_init \
        mbedtls_gcm_init_ncbicxx_2_16_10
#define mbedtls_gcm_setkey \
        mbedtls_gcm_setkey_ncbicxx_2_16_10
#define mbedtls_gcm_starts \
        mbedtls_gcm_starts_ncbicxx_2_16_10
#define mbedtls_gcm_update \
        mbedtls_gcm_update_ncbicxx_2_16_10
#define mbedtls_hkdf \
        mbedtls_hkdf_ncbicxx_2_16_10
#define mbedtls_hkdf_expand \
        mbedtls_hkdf_expand_ncbicxx_2_16_10
#define mbedtls_hkdf_extract \
        mbedtls_hkdf_extract_ncbicxx_2_16_10
#define mbedtls_hmac_drbg_free \
        mbedtls_hmac_drbg_free_ncbicxx_2_16_10
#define mbedtls_hmac_drbg_init \
        mbedtls_hmac_drbg_init_ncbicxx_2_16_10
#define mbedtls_hmac_drbg_random \
        mbedtls_hmac_drbg_random_ncbicxx_2_16_10
#define mbedtls_hmac_drbg_random_with_add \
        mbedtls_hmac_drbg_random_with_add_ncbicxx_2_16_10
#define mbedtls_hmac_drbg_reseed \
        mbedtls_hmac_drbg_reseed_ncbicxx_2_16_10
#define mbedtls_hmac_drbg_seed \
        mbedtls_hmac_drbg_seed_ncbicxx_2_16_10
#define mbedtls_hmac_drbg_seed_buf \
        mbedtls_hmac_drbg_seed_buf_ncbicxx_2_16_10
#define mbedtls_hmac_drbg_set_entropy_len \
        mbedtls_hmac_drbg_set_entropy_len_ncbicxx_2_16_10
#define mbedtls_hmac_drbg_set_prediction_resistance \
        mbedtls_hmac_drbg_set_prediction_resistance_ncbicxx_2_16_10
#define mbedtls_hmac_drbg_set_reseed_interval \
        mbedtls_hmac_drbg_set_reseed_interval_ncbicxx_2_16_10
#define mbedtls_hmac_drbg_update \
        mbedtls_hmac_drbg_update_ncbicxx_2_16_10
#define mbedtls_hmac_drbg_update_ret \
        mbedtls_hmac_drbg_update_ret_ncbicxx_2_16_10
#define mbedtls_hmac_drbg_update_seed_file \
        mbedtls_hmac_drbg_update_seed_file_ncbicxx_2_16_10
#define mbedtls_hmac_drbg_write_seed_file \
        mbedtls_hmac_drbg_write_seed_file_ncbicxx_2_16_10
#define mbedtls_md \
        mbedtls_md_ncbicxx_2_16_10
#define mbedtls_md_clone \
        mbedtls_md_clone_ncbicxx_2_16_10
#define mbedtls_md_file \
        mbedtls_md_file_ncbicxx_2_16_10
#define mbedtls_md_finish \
        mbedtls_md_finish_ncbicxx_2_16_10
#define mbedtls_md_free \
        mbedtls_md_free_ncbicxx_2_16_10
#define mbedtls_md_get_name \
        mbedtls_md_get_name_ncbicxx_2_16_10
#define mbedtls_md_get_size \
        mbedtls_md_get_size_ncbicxx_2_16_10
#define mbedtls_md_get_type \
        mbedtls_md_get_type_ncbicxx_2_16_10
#define mbedtls_md_hmac \
        mbedtls_md_hmac_ncbicxx_2_16_10
#define mbedtls_md_hmac_finish \
        mbedtls_md_hmac_finish_ncbicxx_2_16_10
#define mbedtls_md_hmac_reset \
        mbedtls_md_hmac_reset_ncbicxx_2_16_10
#define mbedtls_md_hmac_starts \
        mbedtls_md_hmac_starts_ncbicxx_2_16_10
#define mbedtls_md_hmac_update \
        mbedtls_md_hmac_update_ncbicxx_2_16_10
#define mbedtls_md_info_from_string \
        mbedtls_md_info_from_string_ncbicxx_2_16_10
#define mbedtls_md_info_from_type \
        mbedtls_md_info_from_type_ncbicxx_2_16_10
#define mbedtls_md_init \
        mbedtls_md_init_ncbicxx_2_16_10
#define mbedtls_md_init_ctx \
        mbedtls_md_init_ctx_ncbicxx_2_16_10
#define mbedtls_md_list \
        mbedtls_md_list_ncbicxx_2_16_10
#define mbedtls_md_process \
        mbedtls_md_process_ncbicxx_2_16_10
#define mbedtls_md_setup \
        mbedtls_md_setup_ncbicxx_2_16_10
#define mbedtls_md_starts \
        mbedtls_md_starts_ncbicxx_2_16_10
#define mbedtls_md_update \
        mbedtls_md_update_ncbicxx_2_16_10
#define mbedtls_internal_md5_process \
        mbedtls_internal_md5_process_ncbicxx_2_16_10
#define mbedtls_md5 \
        mbedtls_md5_ncbicxx_2_16_10
#define mbedtls_md5_clone \
        mbedtls_md5_clone_ncbicxx_2_16_10
#define mbedtls_md5_finish \
        mbedtls_md5_finish_ncbicxx_2_16_10
#define mbedtls_md5_finish_ret \
        mbedtls_md5_finish_ret_ncbicxx_2_16_10
#define mbedtls_md5_free \
        mbedtls_md5_free_ncbicxx_2_16_10
#define mbedtls_md5_init \
        mbedtls_md5_init_ncbicxx_2_16_10
#define mbedtls_md5_process \
        mbedtls_md5_process_ncbicxx_2_16_10
#define mbedtls_md5_ret \
        mbedtls_md5_ret_ncbicxx_2_16_10
#define mbedtls_md5_starts \
        mbedtls_md5_starts_ncbicxx_2_16_10
#define mbedtls_md5_starts_ret \
        mbedtls_md5_starts_ret_ncbicxx_2_16_10
#define mbedtls_md5_update \
        mbedtls_md5_update_ncbicxx_2_16_10
#define mbedtls_md5_update_ret \
        mbedtls_md5_update_ret_ncbicxx_2_16_10
#define mbedtls_md5_info \
        mbedtls_md5_info_ncbicxx_2_16_10
#define mbedtls_ripemd160_info \
        mbedtls_ripemd160_info_ncbicxx_2_16_10
#define mbedtls_sha1_info \
        mbedtls_sha1_info_ncbicxx_2_16_10
#define mbedtls_sha224_info \
        mbedtls_sha224_info_ncbicxx_2_16_10
#define mbedtls_sha256_info \
        mbedtls_sha256_info_ncbicxx_2_16_10
#define mbedtls_sha384_info \
        mbedtls_sha384_info_ncbicxx_2_16_10
#define mbedtls_sha512_info \
        mbedtls_sha512_info_ncbicxx_2_16_10
#define mbedtls_net_accept \
        mbedtls_net_accept_ncbicxx_2_16_10
#define mbedtls_net_bind \
        mbedtls_net_bind_ncbicxx_2_16_10
#define mbedtls_net_connect \
        mbedtls_net_connect_ncbicxx_2_16_10
#define mbedtls_net_free \
        mbedtls_net_free_ncbicxx_2_16_10
#define mbedtls_net_init \
        mbedtls_net_init_ncbicxx_2_16_10
#define mbedtls_net_poll \
        mbedtls_net_poll_ncbicxx_2_16_10
#define mbedtls_net_recv \
        mbedtls_net_recv_ncbicxx_2_16_10
#define mbedtls_net_recv_timeout \
        mbedtls_net_recv_timeout_ncbicxx_2_16_10
#define mbedtls_net_send \
        mbedtls_net_send_ncbicxx_2_16_10
#define mbedtls_net_set_block \
        mbedtls_net_set_block_ncbicxx_2_16_10
#define mbedtls_net_set_nonblock \
        mbedtls_net_set_nonblock_ncbicxx_2_16_10
#define mbedtls_net_usleep \
        mbedtls_net_usleep_ncbicxx_2_16_10
#define mbedtls_oid_get_attr_short_name \
        mbedtls_oid_get_attr_short_name_ncbicxx_2_16_10
#define mbedtls_oid_get_cipher_alg \
        mbedtls_oid_get_cipher_alg_ncbicxx_2_16_10
#define mbedtls_oid_get_ec_grp \
        mbedtls_oid_get_ec_grp_ncbicxx_2_16_10
#define mbedtls_oid_get_extended_key_usage \
        mbedtls_oid_get_extended_key_usage_ncbicxx_2_16_10
#define mbedtls_oid_get_md_alg \
        mbedtls_oid_get_md_alg_ncbicxx_2_16_10
#define mbedtls_oid_get_md_hmac \
        mbedtls_oid_get_md_hmac_ncbicxx_2_16_10
#define mbedtls_oid_get_numeric_string \
        mbedtls_oid_get_numeric_string_ncbicxx_2_16_10
#define mbedtls_oid_get_oid_by_ec_grp \
        mbedtls_oid_get_oid_by_ec_grp_ncbicxx_2_16_10
#define mbedtls_oid_get_oid_by_md \
        mbedtls_oid_get_oid_by_md_ncbicxx_2_16_10
#define mbedtls_oid_get_oid_by_pk_alg \
        mbedtls_oid_get_oid_by_pk_alg_ncbicxx_2_16_10
#define mbedtls_oid_get_oid_by_sig_alg \
        mbedtls_oid_get_oid_by_sig_alg_ncbicxx_2_16_10
#define mbedtls_oid_get_pk_alg \
        mbedtls_oid_get_pk_alg_ncbicxx_2_16_10
#define mbedtls_oid_get_pkcs12_pbe_alg \
        mbedtls_oid_get_pkcs12_pbe_alg_ncbicxx_2_16_10
#define mbedtls_oid_get_sig_alg \
        mbedtls_oid_get_sig_alg_ncbicxx_2_16_10
#define mbedtls_oid_get_sig_alg_desc \
        mbedtls_oid_get_sig_alg_desc_ncbicxx_2_16_10
#define mbedtls_oid_get_x509_ext_type \
        mbedtls_oid_get_x509_ext_type_ncbicxx_2_16_10
#define mbedtls_pem_free \
        mbedtls_pem_free_ncbicxx_2_16_10
#define mbedtls_pem_init \
        mbedtls_pem_init_ncbicxx_2_16_10
#define mbedtls_pem_read_buffer \
        mbedtls_pem_read_buffer_ncbicxx_2_16_10
#define mbedtls_pem_write_buffer \
        mbedtls_pem_write_buffer_ncbicxx_2_16_10
#define mbedtls_pk_can_do \
        mbedtls_pk_can_do_ncbicxx_2_16_10
#define mbedtls_pk_check_pair \
        mbedtls_pk_check_pair_ncbicxx_2_16_10
#define mbedtls_pk_debug \
        mbedtls_pk_debug_ncbicxx_2_16_10
#define mbedtls_pk_decrypt \
        mbedtls_pk_decrypt_ncbicxx_2_16_10
#define mbedtls_pk_encrypt \
        mbedtls_pk_encrypt_ncbicxx_2_16_10
#define mbedtls_pk_free \
        mbedtls_pk_free_ncbicxx_2_16_10
#define mbedtls_pk_get_bitlen \
        mbedtls_pk_get_bitlen_ncbicxx_2_16_10
#define mbedtls_pk_get_name \
        mbedtls_pk_get_name_ncbicxx_2_16_10
#define mbedtls_pk_get_type \
        mbedtls_pk_get_type_ncbicxx_2_16_10
#define mbedtls_pk_info_from_type \
        mbedtls_pk_info_from_type_ncbicxx_2_16_10
#define mbedtls_pk_init \
        mbedtls_pk_init_ncbicxx_2_16_10
#define mbedtls_pk_setup \
        mbedtls_pk_setup_ncbicxx_2_16_10
#define mbedtls_pk_setup_rsa_alt \
        mbedtls_pk_setup_rsa_alt_ncbicxx_2_16_10
#define mbedtls_pk_sign \
        mbedtls_pk_sign_ncbicxx_2_16_10
#define mbedtls_pk_sign_restartable \
        mbedtls_pk_sign_restartable_ncbicxx_2_16_10
#define mbedtls_pk_verify \
        mbedtls_pk_verify_ncbicxx_2_16_10
#define mbedtls_pk_verify_ext \
        mbedtls_pk_verify_ext_ncbicxx_2_16_10
#define mbedtls_pk_verify_restartable \
        mbedtls_pk_verify_restartable_ncbicxx_2_16_10
#define mbedtls_ecdsa_info \
        mbedtls_ecdsa_info_ncbicxx_2_16_10
#define mbedtls_eckey_info \
        mbedtls_eckey_info_ncbicxx_2_16_10
#define mbedtls_eckeydh_info \
        mbedtls_eckeydh_info_ncbicxx_2_16_10
#define mbedtls_rsa_alt_info \
        mbedtls_rsa_alt_info_ncbicxx_2_16_10
#define mbedtls_rsa_info \
        mbedtls_rsa_info_ncbicxx_2_16_10
#define mbedtls_pkcs12_derivation \
        mbedtls_pkcs12_derivation_ncbicxx_2_16_10
#define mbedtls_pkcs12_pbe \
        mbedtls_pkcs12_pbe_ncbicxx_2_16_10
#define mbedtls_pkcs12_pbe_sha1_rc4_128 \
        mbedtls_pkcs12_pbe_sha1_rc4_128_ncbicxx_2_16_10
#define mbedtls_pkcs5_pbes2 \
        mbedtls_pkcs5_pbes2_ncbicxx_2_16_10
#define mbedtls_pkcs5_pbkdf2_hmac \
        mbedtls_pkcs5_pbkdf2_hmac_ncbicxx_2_16_10
#define mbedtls_pk_load_file \
        mbedtls_pk_load_file_ncbicxx_2_16_10
#define mbedtls_pk_parse_key \
        mbedtls_pk_parse_key_ncbicxx_2_16_10
#define mbedtls_pk_parse_keyfile \
        mbedtls_pk_parse_keyfile_ncbicxx_2_16_10
#define mbedtls_pk_parse_public_key \
        mbedtls_pk_parse_public_key_ncbicxx_2_16_10
#define mbedtls_pk_parse_public_keyfile \
        mbedtls_pk_parse_public_keyfile_ncbicxx_2_16_10
#define mbedtls_pk_parse_subpubkey \
        mbedtls_pk_parse_subpubkey_ncbicxx_2_16_10
#define mbedtls_pk_write_key_der \
        mbedtls_pk_write_key_der_ncbicxx_2_16_10
#define mbedtls_pk_write_key_pem \
        mbedtls_pk_write_key_pem_ncbicxx_2_16_10
#define mbedtls_pk_write_pubkey \
        mbedtls_pk_write_pubkey_ncbicxx_2_16_10
#define mbedtls_pk_write_pubkey_der \
        mbedtls_pk_write_pubkey_der_ncbicxx_2_16_10
#define mbedtls_pk_write_pubkey_pem \
        mbedtls_pk_write_pubkey_pem_ncbicxx_2_16_10
#define mbedtls_platform_setup \
        mbedtls_platform_setup_ncbicxx_2_16_10
#define mbedtls_platform_teardown \
        mbedtls_platform_teardown_ncbicxx_2_16_10
#define mbedtls_platform_gmtime_r \
        mbedtls_platform_gmtime_r_ncbicxx_2_16_10
#define mbedtls_platform_zeroize \
        mbedtls_platform_zeroize_ncbicxx_2_16_10
#define mbedtls_poly1305_finish \
        mbedtls_poly1305_finish_ncbicxx_2_16_10
#define mbedtls_poly1305_free \
        mbedtls_poly1305_free_ncbicxx_2_16_10
#define mbedtls_poly1305_init \
        mbedtls_poly1305_init_ncbicxx_2_16_10
#define mbedtls_poly1305_mac \
        mbedtls_poly1305_mac_ncbicxx_2_16_10
#define mbedtls_poly1305_starts \
        mbedtls_poly1305_starts_ncbicxx_2_16_10
#define mbedtls_poly1305_update \
        mbedtls_poly1305_update_ncbicxx_2_16_10
#define mbedtls_internal_ripemd160_process \
        mbedtls_internal_ripemd160_process_ncbicxx_2_16_10
#define mbedtls_ripemd160 \
        mbedtls_ripemd160_ncbicxx_2_16_10
#define mbedtls_ripemd160_clone \
        mbedtls_ripemd160_clone_ncbicxx_2_16_10
#define mbedtls_ripemd160_finish \
        mbedtls_ripemd160_finish_ncbicxx_2_16_10
#define mbedtls_ripemd160_finish_ret \
        mbedtls_ripemd160_finish_ret_ncbicxx_2_16_10
#define mbedtls_ripemd160_free \
        mbedtls_ripemd160_free_ncbicxx_2_16_10
#define mbedtls_ripemd160_init \
        mbedtls_ripemd160_init_ncbicxx_2_16_10
#define mbedtls_ripemd160_process \
        mbedtls_ripemd160_process_ncbicxx_2_16_10
#define mbedtls_ripemd160_ret \
        mbedtls_ripemd160_ret_ncbicxx_2_16_10
#define mbedtls_ripemd160_starts \
        mbedtls_ripemd160_starts_ncbicxx_2_16_10
#define mbedtls_ripemd160_starts_ret \
        mbedtls_ripemd160_starts_ret_ncbicxx_2_16_10
#define mbedtls_ripemd160_update \
        mbedtls_ripemd160_update_ncbicxx_2_16_10
#define mbedtls_ripemd160_update_ret \
        mbedtls_ripemd160_update_ret_ncbicxx_2_16_10
#define mbedtls_rsa_check_privkey \
        mbedtls_rsa_check_privkey_ncbicxx_2_16_10
#define mbedtls_rsa_check_pub_priv \
        mbedtls_rsa_check_pub_priv_ncbicxx_2_16_10
#define mbedtls_rsa_check_pubkey \
        mbedtls_rsa_check_pubkey_ncbicxx_2_16_10
#define mbedtls_rsa_complete \
        mbedtls_rsa_complete_ncbicxx_2_16_10
#define mbedtls_rsa_copy \
        mbedtls_rsa_copy_ncbicxx_2_16_10
#define mbedtls_rsa_export \
        mbedtls_rsa_export_ncbicxx_2_16_10
#define mbedtls_rsa_export_crt \
        mbedtls_rsa_export_crt_ncbicxx_2_16_10
#define mbedtls_rsa_export_raw \
        mbedtls_rsa_export_raw_ncbicxx_2_16_10
#define mbedtls_rsa_free \
        mbedtls_rsa_free_ncbicxx_2_16_10
#define mbedtls_rsa_gen_key \
        mbedtls_rsa_gen_key_ncbicxx_2_16_10
#define mbedtls_rsa_get_len \
        mbedtls_rsa_get_len_ncbicxx_2_16_10
#define mbedtls_rsa_import \
        mbedtls_rsa_import_ncbicxx_2_16_10
#define mbedtls_rsa_import_raw \
        mbedtls_rsa_import_raw_ncbicxx_2_16_10
#define mbedtls_rsa_init \
        mbedtls_rsa_init_ncbicxx_2_16_10
#define mbedtls_rsa_pkcs1_decrypt \
        mbedtls_rsa_pkcs1_decrypt_ncbicxx_2_16_10
#define mbedtls_rsa_pkcs1_encrypt \
        mbedtls_rsa_pkcs1_encrypt_ncbicxx_2_16_10
#define mbedtls_rsa_pkcs1_sign \
        mbedtls_rsa_pkcs1_sign_ncbicxx_2_16_10
#define mbedtls_rsa_pkcs1_verify \
        mbedtls_rsa_pkcs1_verify_ncbicxx_2_16_10
#define mbedtls_rsa_private \
        mbedtls_rsa_private_ncbicxx_2_16_10
#define mbedtls_rsa_public \
        mbedtls_rsa_public_ncbicxx_2_16_10
#define mbedtls_rsa_rsaes_oaep_decrypt \
        mbedtls_rsa_rsaes_oaep_decrypt_ncbicxx_2_16_10
#define mbedtls_rsa_rsaes_oaep_encrypt \
        mbedtls_rsa_rsaes_oaep_encrypt_ncbicxx_2_16_10
#define mbedtls_rsa_rsaes_pkcs1_v15_decrypt \
        mbedtls_rsa_rsaes_pkcs1_v15_decrypt_ncbicxx_2_16_10
#define mbedtls_rsa_rsaes_pkcs1_v15_encrypt \
        mbedtls_rsa_rsaes_pkcs1_v15_encrypt_ncbicxx_2_16_10
#define mbedtls_rsa_rsassa_pkcs1_v15_sign \
        mbedtls_rsa_rsassa_pkcs1_v15_sign_ncbicxx_2_16_10
#define mbedtls_rsa_rsassa_pkcs1_v15_verify \
        mbedtls_rsa_rsassa_pkcs1_v15_verify_ncbicxx_2_16_10
#define mbedtls_rsa_rsassa_pss_sign \
        mbedtls_rsa_rsassa_pss_sign_ncbicxx_2_16_10
#define mbedtls_rsa_rsassa_pss_verify \
        mbedtls_rsa_rsassa_pss_verify_ncbicxx_2_16_10
#define mbedtls_rsa_rsassa_pss_verify_ext \
        mbedtls_rsa_rsassa_pss_verify_ext_ncbicxx_2_16_10
#define mbedtls_rsa_set_padding \
        mbedtls_rsa_set_padding_ncbicxx_2_16_10
#define mbedtls_rsa_deduce_crt \
        mbedtls_rsa_deduce_crt_ncbicxx_2_16_10
#define mbedtls_rsa_deduce_primes \
        mbedtls_rsa_deduce_primes_ncbicxx_2_16_10
#define mbedtls_rsa_deduce_private_exponent \
        mbedtls_rsa_deduce_private_exponent_ncbicxx_2_16_10
#define mbedtls_rsa_validate_crt \
        mbedtls_rsa_validate_crt_ncbicxx_2_16_10
#define mbedtls_rsa_validate_params \
        mbedtls_rsa_validate_params_ncbicxx_2_16_10
#define mbedtls_internal_sha1_process \
        mbedtls_internal_sha1_process_ncbicxx_2_16_10
#define mbedtls_sha1 \
        mbedtls_sha1_ncbicxx_2_16_10
#define mbedtls_sha1_clone \
        mbedtls_sha1_clone_ncbicxx_2_16_10
#define mbedtls_sha1_finish \
        mbedtls_sha1_finish_ncbicxx_2_16_10
#define mbedtls_sha1_finish_ret \
        mbedtls_sha1_finish_ret_ncbicxx_2_16_10
#define mbedtls_sha1_free \
        mbedtls_sha1_free_ncbicxx_2_16_10
#define mbedtls_sha1_init \
        mbedtls_sha1_init_ncbicxx_2_16_10
#define mbedtls_sha1_process \
        mbedtls_sha1_process_ncbicxx_2_16_10
#define mbedtls_sha1_ret \
        mbedtls_sha1_ret_ncbicxx_2_16_10
#define mbedtls_sha1_starts \
        mbedtls_sha1_starts_ncbicxx_2_16_10
#define mbedtls_sha1_starts_ret \
        mbedtls_sha1_starts_ret_ncbicxx_2_16_10
#define mbedtls_sha1_update \
        mbedtls_sha1_update_ncbicxx_2_16_10
#define mbedtls_sha1_update_ret \
        mbedtls_sha1_update_ret_ncbicxx_2_16_10
#define mbedtls_internal_sha256_process \
        mbedtls_internal_sha256_process_ncbicxx_2_16_10
#define mbedtls_sha256 \
        mbedtls_sha256_ncbicxx_2_16_10
#define mbedtls_sha256_clone \
        mbedtls_sha256_clone_ncbicxx_2_16_10
#define mbedtls_sha256_finish \
        mbedtls_sha256_finish_ncbicxx_2_16_10
#define mbedtls_sha256_finish_ret \
        mbedtls_sha256_finish_ret_ncbicxx_2_16_10
#define mbedtls_sha256_free \
        mbedtls_sha256_free_ncbicxx_2_16_10
#define mbedtls_sha256_init \
        mbedtls_sha256_init_ncbicxx_2_16_10
#define mbedtls_sha256_process \
        mbedtls_sha256_process_ncbicxx_2_16_10
#define mbedtls_sha256_ret \
        mbedtls_sha256_ret_ncbicxx_2_16_10
#define mbedtls_sha256_starts \
        mbedtls_sha256_starts_ncbicxx_2_16_10
#define mbedtls_sha256_starts_ret \
        mbedtls_sha256_starts_ret_ncbicxx_2_16_10
#define mbedtls_sha256_update \
        mbedtls_sha256_update_ncbicxx_2_16_10
#define mbedtls_sha256_update_ret \
        mbedtls_sha256_update_ret_ncbicxx_2_16_10
#define mbedtls_internal_sha512_process \
        mbedtls_internal_sha512_process_ncbicxx_2_16_10
#define mbedtls_sha512 \
        mbedtls_sha512_ncbicxx_2_16_10
#define mbedtls_sha512_clone \
        mbedtls_sha512_clone_ncbicxx_2_16_10
#define mbedtls_sha512_finish \
        mbedtls_sha512_finish_ncbicxx_2_16_10
#define mbedtls_sha512_finish_ret \
        mbedtls_sha512_finish_ret_ncbicxx_2_16_10
#define mbedtls_sha512_free \
        mbedtls_sha512_free_ncbicxx_2_16_10
#define mbedtls_sha512_init \
        mbedtls_sha512_init_ncbicxx_2_16_10
#define mbedtls_sha512_process \
        mbedtls_sha512_process_ncbicxx_2_16_10
#define mbedtls_sha512_ret \
        mbedtls_sha512_ret_ncbicxx_2_16_10
#define mbedtls_sha512_starts \
        mbedtls_sha512_starts_ncbicxx_2_16_10
#define mbedtls_sha512_starts_ret \
        mbedtls_sha512_starts_ret_ncbicxx_2_16_10
#define mbedtls_sha512_update \
        mbedtls_sha512_update_ncbicxx_2_16_10
#define mbedtls_sha512_update_ret \
        mbedtls_sha512_update_ret_ncbicxx_2_16_10
#define mbedtls_ssl_cache_free \
        mbedtls_ssl_cache_free_ncbicxx_2_16_10
#define mbedtls_ssl_cache_get \
        mbedtls_ssl_cache_get_ncbicxx_2_16_10
#define mbedtls_ssl_cache_init \
        mbedtls_ssl_cache_init_ncbicxx_2_16_10
#define mbedtls_ssl_cache_set \
        mbedtls_ssl_cache_set_ncbicxx_2_16_10
#define mbedtls_ssl_cache_set_max_entries \
        mbedtls_ssl_cache_set_max_entries_ncbicxx_2_16_10
#define mbedtls_ssl_cache_set_timeout \
        mbedtls_ssl_cache_set_timeout_ncbicxx_2_16_10
#define mbedtls_ssl_ciphersuite_from_id \
        mbedtls_ssl_ciphersuite_from_id_ncbicxx_2_16_10
#define mbedtls_ssl_ciphersuite_from_string \
        mbedtls_ssl_ciphersuite_from_string_ncbicxx_2_16_10
#define mbedtls_ssl_ciphersuite_uses_ec \
        mbedtls_ssl_ciphersuite_uses_ec_ncbicxx_2_16_10
#define mbedtls_ssl_ciphersuite_uses_psk \
        mbedtls_ssl_ciphersuite_uses_psk_ncbicxx_2_16_10
#define mbedtls_ssl_get_ciphersuite_id \
        mbedtls_ssl_get_ciphersuite_id_ncbicxx_2_16_10
#define mbedtls_ssl_get_ciphersuite_name \
        mbedtls_ssl_get_ciphersuite_name_ncbicxx_2_16_10
#define mbedtls_ssl_get_ciphersuite_sig_alg \
        mbedtls_ssl_get_ciphersuite_sig_alg_ncbicxx_2_16_10
#define mbedtls_ssl_get_ciphersuite_sig_pk_alg \
        mbedtls_ssl_get_ciphersuite_sig_pk_alg_ncbicxx_2_16_10
#define mbedtls_ssl_list_ciphersuites \
        mbedtls_ssl_list_ciphersuites_ncbicxx_2_16_10
#define mbedtls_ssl_handshake_client_step \
        mbedtls_ssl_handshake_client_step_ncbicxx_2_16_10
#define mbedtls_ssl_cookie_check \
        mbedtls_ssl_cookie_check_ncbicxx_2_16_10
#define mbedtls_ssl_cookie_free \
        mbedtls_ssl_cookie_free_ncbicxx_2_16_10
#define mbedtls_ssl_cookie_init \
        mbedtls_ssl_cookie_init_ncbicxx_2_16_10
#define mbedtls_ssl_cookie_set_timeout \
        mbedtls_ssl_cookie_set_timeout_ncbicxx_2_16_10
#define mbedtls_ssl_cookie_setup \
        mbedtls_ssl_cookie_setup_ncbicxx_2_16_10
#define mbedtls_ssl_cookie_write \
        mbedtls_ssl_cookie_write_ncbicxx_2_16_10
#define mbedtls_ssl_conf_dtls_cookies \
        mbedtls_ssl_conf_dtls_cookies_ncbicxx_2_16_10
#define mbedtls_ssl_handshake_server_step \
        mbedtls_ssl_handshake_server_step_ncbicxx_2_16_10
#define mbedtls_ssl_set_client_transport_id \
        mbedtls_ssl_set_client_transport_id_ncbicxx_2_16_10
#define mbedtls_ssl_ticket_free \
        mbedtls_ssl_ticket_free_ncbicxx_2_16_10
#define mbedtls_ssl_ticket_init \
        mbedtls_ssl_ticket_init_ncbicxx_2_16_10
#define mbedtls_ssl_ticket_parse \
        mbedtls_ssl_ticket_parse_ncbicxx_2_16_10
#define mbedtls_ssl_ticket_setup \
        mbedtls_ssl_ticket_setup_ncbicxx_2_16_10
#define mbedtls_ssl_ticket_write \
        mbedtls_ssl_ticket_write_ncbicxx_2_16_10
#define mbedtls_ssl_cf_hmac \
        mbedtls_ssl_cf_hmac_ncbicxx_2_16_10
#define mbedtls_ssl_cf_memcpy_offset \
        mbedtls_ssl_cf_memcpy_offset_ncbicxx_2_16_10
#define mbedtls_ssl_check_cert_usage \
        mbedtls_ssl_check_cert_usage_ncbicxx_2_16_10
#define mbedtls_ssl_check_curve \
        mbedtls_ssl_check_curve_ncbicxx_2_16_10
#define mbedtls_ssl_check_pending \
        mbedtls_ssl_check_pending_ncbicxx_2_16_10
#define mbedtls_ssl_check_sig_hash \
        mbedtls_ssl_check_sig_hash_ncbicxx_2_16_10
#define mbedtls_ssl_close_notify \
        mbedtls_ssl_close_notify_ncbicxx_2_16_10
#define mbedtls_ssl_conf_alpn_protocols \
        mbedtls_ssl_conf_alpn_protocols_ncbicxx_2_16_10
#define mbedtls_ssl_conf_arc4_support \
        mbedtls_ssl_conf_arc4_support_ncbicxx_2_16_10
#define mbedtls_ssl_conf_authmode \
        mbedtls_ssl_conf_authmode_ncbicxx_2_16_10
#define mbedtls_ssl_conf_ca_chain \
        mbedtls_ssl_conf_ca_chain_ncbicxx_2_16_10
#define mbedtls_ssl_conf_cbc_record_splitting \
        mbedtls_ssl_conf_cbc_record_splitting_ncbicxx_2_16_10
#define mbedtls_ssl_conf_cert_profile \
        mbedtls_ssl_conf_cert_profile_ncbicxx_2_16_10
#define mbedtls_ssl_conf_cert_req_ca_list \
        mbedtls_ssl_conf_cert_req_ca_list_ncbicxx_2_16_10
#define mbedtls_ssl_conf_ciphersuites \
        mbedtls_ssl_conf_ciphersuites_ncbicxx_2_16_10
#define mbedtls_ssl_conf_ciphersuites_for_version \
        mbedtls_ssl_conf_ciphersuites_for_version_ncbicxx_2_16_10
#define mbedtls_ssl_conf_curves \
        mbedtls_ssl_conf_curves_ncbicxx_2_16_10
#define mbedtls_ssl_conf_dbg \
        mbedtls_ssl_conf_dbg_ncbicxx_2_16_10
#define mbedtls_ssl_conf_dh_param \
        mbedtls_ssl_conf_dh_param_ncbicxx_2_16_10
#define mbedtls_ssl_conf_dh_param_bin \
        mbedtls_ssl_conf_dh_param_bin_ncbicxx_2_16_10
#define mbedtls_ssl_conf_dh_param_ctx \
        mbedtls_ssl_conf_dh_param_ctx_ncbicxx_2_16_10
#define mbedtls_ssl_conf_dhm_min_bitlen \
        mbedtls_ssl_conf_dhm_min_bitlen_ncbicxx_2_16_10
#define mbedtls_ssl_conf_dtls_anti_replay \
        mbedtls_ssl_conf_dtls_anti_replay_ncbicxx_2_16_10
#define mbedtls_ssl_conf_dtls_badmac_limit \
        mbedtls_ssl_conf_dtls_badmac_limit_ncbicxx_2_16_10
#define mbedtls_ssl_conf_encrypt_then_mac \
        mbedtls_ssl_conf_encrypt_then_mac_ncbicxx_2_16_10
#define mbedtls_ssl_conf_endpoint \
        mbedtls_ssl_conf_endpoint_ncbicxx_2_16_10
#define mbedtls_ssl_conf_export_keys_cb \
        mbedtls_ssl_conf_export_keys_cb_ncbicxx_2_16_10
#define mbedtls_ssl_conf_extended_master_secret \
        mbedtls_ssl_conf_extended_master_secret_ncbicxx_2_16_10
#define mbedtls_ssl_conf_fallback \
        mbedtls_ssl_conf_fallback_ncbicxx_2_16_10
#define mbedtls_ssl_conf_handshake_timeout \
        mbedtls_ssl_conf_handshake_timeout_ncbicxx_2_16_10
#define mbedtls_ssl_conf_legacy_renegotiation \
        mbedtls_ssl_conf_legacy_renegotiation_ncbicxx_2_16_10
#define mbedtls_ssl_conf_max_frag_len \
        mbedtls_ssl_conf_max_frag_len_ncbicxx_2_16_10
#define mbedtls_ssl_conf_max_version \
        mbedtls_ssl_conf_max_version_ncbicxx_2_16_10
#define mbedtls_ssl_conf_min_version \
        mbedtls_ssl_conf_min_version_ncbicxx_2_16_10
#define mbedtls_ssl_conf_own_cert \
        mbedtls_ssl_conf_own_cert_ncbicxx_2_16_10
#define mbedtls_ssl_conf_psk \
        mbedtls_ssl_conf_psk_ncbicxx_2_16_10
#define mbedtls_ssl_conf_psk_cb \
        mbedtls_ssl_conf_psk_cb_ncbicxx_2_16_10
#define mbedtls_ssl_conf_read_timeout \
        mbedtls_ssl_conf_read_timeout_ncbicxx_2_16_10
#define mbedtls_ssl_conf_renegotiation \
        mbedtls_ssl_conf_renegotiation_ncbicxx_2_16_10
#define mbedtls_ssl_conf_renegotiation_enforced \
        mbedtls_ssl_conf_renegotiation_enforced_ncbicxx_2_16_10
#define mbedtls_ssl_conf_renegotiation_period \
        mbedtls_ssl_conf_renegotiation_period_ncbicxx_2_16_10
#define mbedtls_ssl_conf_rng \
        mbedtls_ssl_conf_rng_ncbicxx_2_16_10
#define mbedtls_ssl_conf_session_cache \
        mbedtls_ssl_conf_session_cache_ncbicxx_2_16_10
#define mbedtls_ssl_conf_session_tickets \
        mbedtls_ssl_conf_session_tickets_ncbicxx_2_16_10
#define mbedtls_ssl_conf_session_tickets_cb \
        mbedtls_ssl_conf_session_tickets_cb_ncbicxx_2_16_10
#define mbedtls_ssl_conf_sig_hashes \
        mbedtls_ssl_conf_sig_hashes_ncbicxx_2_16_10
#define mbedtls_ssl_conf_sni \
        mbedtls_ssl_conf_sni_ncbicxx_2_16_10
#define mbedtls_ssl_conf_transport \
        mbedtls_ssl_conf_transport_ncbicxx_2_16_10
#define mbedtls_ssl_conf_truncated_hmac \
        mbedtls_ssl_conf_truncated_hmac_ncbicxx_2_16_10
#define mbedtls_ssl_conf_verify \
        mbedtls_ssl_conf_verify_ncbicxx_2_16_10
#define mbedtls_ssl_config_defaults \
        mbedtls_ssl_config_defaults_ncbicxx_2_16_10
#define mbedtls_ssl_config_free \
        mbedtls_ssl_config_free_ncbicxx_2_16_10
#define mbedtls_ssl_config_init \
        mbedtls_ssl_config_init_ncbicxx_2_16_10
#define mbedtls_ssl_derive_keys \
        mbedtls_ssl_derive_keys_ncbicxx_2_16_10
#define mbedtls_ssl_dtls_replay_check \
        mbedtls_ssl_dtls_replay_check_ncbicxx_2_16_10
#define mbedtls_ssl_dtls_replay_update \
        mbedtls_ssl_dtls_replay_update_ncbicxx_2_16_10
#define mbedtls_ssl_fetch_input \
        mbedtls_ssl_fetch_input_ncbicxx_2_16_10
#define mbedtls_ssl_flight_transmit \
        mbedtls_ssl_flight_transmit_ncbicxx_2_16_10
#define mbedtls_ssl_flush_output \
        mbedtls_ssl_flush_output_ncbicxx_2_16_10
#define mbedtls_ssl_free \
        mbedtls_ssl_free_ncbicxx_2_16_10
#define mbedtls_ssl_get_alpn_protocol \
        mbedtls_ssl_get_alpn_protocol_ncbicxx_2_16_10
#define mbedtls_ssl_get_bytes_avail \
        mbedtls_ssl_get_bytes_avail_ncbicxx_2_16_10
#define mbedtls_ssl_get_ciphersuite \
        mbedtls_ssl_get_ciphersuite_ncbicxx_2_16_10
#define mbedtls_ssl_get_key_exchange_md_ssl_tls \
        mbedtls_ssl_get_key_exchange_md_ssl_tls_ncbicxx_2_16_10
#define mbedtls_ssl_get_key_exchange_md_tls1_2 \
        mbedtls_ssl_get_key_exchange_md_tls1_2_ncbicxx_2_16_10
#define mbedtls_ssl_get_max_frag_len \
        mbedtls_ssl_get_max_frag_len_ncbicxx_2_16_10
#define mbedtls_ssl_get_max_out_record_payload \
        mbedtls_ssl_get_max_out_record_payload_ncbicxx_2_16_10
#define mbedtls_ssl_get_peer_cert \
        mbedtls_ssl_get_peer_cert_ncbicxx_2_16_10
#define mbedtls_ssl_get_record_expansion \
        mbedtls_ssl_get_record_expansion_ncbicxx_2_16_10
#define mbedtls_ssl_get_session \
        mbedtls_ssl_get_session_ncbicxx_2_16_10
#define mbedtls_ssl_get_verify_result \
        mbedtls_ssl_get_verify_result_ncbicxx_2_16_10
#define mbedtls_ssl_get_version \
        mbedtls_ssl_get_version_ncbicxx_2_16_10
#define mbedtls_ssl_handle_message_type \
        mbedtls_ssl_handle_message_type_ncbicxx_2_16_10
#define mbedtls_ssl_handshake \
        mbedtls_ssl_handshake_ncbicxx_2_16_10
#define mbedtls_ssl_handshake_free \
        mbedtls_ssl_handshake_free_ncbicxx_2_16_10
#define mbedtls_ssl_handshake_step \
        mbedtls_ssl_handshake_step_ncbicxx_2_16_10
#define mbedtls_ssl_handshake_wrapup \
        mbedtls_ssl_handshake_wrapup_ncbicxx_2_16_10
#define mbedtls_ssl_hash_from_md_alg \
        mbedtls_ssl_hash_from_md_alg_ncbicxx_2_16_10
#define mbedtls_ssl_init \
        mbedtls_ssl_init_ncbicxx_2_16_10
#define mbedtls_ssl_md_alg_from_hash \
        mbedtls_ssl_md_alg_from_hash_ncbicxx_2_16_10
#define mbedtls_ssl_optimize_checksum \
        mbedtls_ssl_optimize_checksum_ncbicxx_2_16_10
#define mbedtls_ssl_parse_certificate \
        mbedtls_ssl_parse_certificate_ncbicxx_2_16_10
#define mbedtls_ssl_parse_change_cipher_spec \
        mbedtls_ssl_parse_change_cipher_spec_ncbicxx_2_16_10
#define mbedtls_ssl_parse_finished \
        mbedtls_ssl_parse_finished_ncbicxx_2_16_10
#define mbedtls_ssl_pk_alg_from_sig \
        mbedtls_ssl_pk_alg_from_sig_ncbicxx_2_16_10
#define mbedtls_ssl_prepare_handshake_record \
        mbedtls_ssl_prepare_handshake_record_ncbicxx_2_16_10
#define mbedtls_ssl_psk_derive_premaster \
        mbedtls_ssl_psk_derive_premaster_ncbicxx_2_16_10
#define mbedtls_ssl_read \
        mbedtls_ssl_read_ncbicxx_2_16_10
#define mbedtls_ssl_read_record \
        mbedtls_ssl_read_record_ncbicxx_2_16_10
#define mbedtls_ssl_read_version \
        mbedtls_ssl_read_version_ncbicxx_2_16_10
#define mbedtls_ssl_recv_flight_completed \
        mbedtls_ssl_recv_flight_completed_ncbicxx_2_16_10
#define mbedtls_ssl_renegotiate \
        mbedtls_ssl_renegotiate_ncbicxx_2_16_10
#define mbedtls_ssl_resend \
        mbedtls_ssl_resend_ncbicxx_2_16_10
#define mbedtls_ssl_reset_checksum \
        mbedtls_ssl_reset_checksum_ncbicxx_2_16_10
#define mbedtls_ssl_send_alert_message \
        mbedtls_ssl_send_alert_message_ncbicxx_2_16_10
#define mbedtls_ssl_send_fatal_handshake_failure \
        mbedtls_ssl_send_fatal_handshake_failure_ncbicxx_2_16_10
#define mbedtls_ssl_send_flight_completed \
        mbedtls_ssl_send_flight_completed_ncbicxx_2_16_10
#define mbedtls_ssl_session_free \
        mbedtls_ssl_session_free_ncbicxx_2_16_10
#define mbedtls_ssl_session_init \
        mbedtls_ssl_session_init_ncbicxx_2_16_10
#define mbedtls_ssl_session_reset \
        mbedtls_ssl_session_reset_ncbicxx_2_16_10
#define mbedtls_ssl_set_bio \
        mbedtls_ssl_set_bio_ncbicxx_2_16_10
#define mbedtls_ssl_set_calc_verify_md \
        mbedtls_ssl_set_calc_verify_md_ncbicxx_2_16_10
#define mbedtls_ssl_set_datagram_packing \
        mbedtls_ssl_set_datagram_packing_ncbicxx_2_16_10
#define mbedtls_ssl_set_hostname \
        mbedtls_ssl_set_hostname_ncbicxx_2_16_10
#define mbedtls_ssl_set_hs_authmode \
        mbedtls_ssl_set_hs_authmode_ncbicxx_2_16_10
#define mbedtls_ssl_set_hs_ca_chain \
        mbedtls_ssl_set_hs_ca_chain_ncbicxx_2_16_10
#define mbedtls_ssl_set_hs_own_cert \
        mbedtls_ssl_set_hs_own_cert_ncbicxx_2_16_10
#define mbedtls_ssl_set_hs_psk \
        mbedtls_ssl_set_hs_psk_ncbicxx_2_16_10
#define mbedtls_ssl_set_mtu \
        mbedtls_ssl_set_mtu_ncbicxx_2_16_10
#define mbedtls_ssl_set_session \
        mbedtls_ssl_set_session_ncbicxx_2_16_10
#define mbedtls_ssl_set_timer_cb \
        mbedtls_ssl_set_timer_cb_ncbicxx_2_16_10
#define mbedtls_ssl_setup \
        mbedtls_ssl_setup_ncbicxx_2_16_10
#define mbedtls_ssl_sig_from_pk \
        mbedtls_ssl_sig_from_pk_ncbicxx_2_16_10
#define mbedtls_ssl_sig_from_pk_alg \
        mbedtls_ssl_sig_from_pk_alg_ncbicxx_2_16_10
#define mbedtls_ssl_sig_hash_set_add \
        mbedtls_ssl_sig_hash_set_add_ncbicxx_2_16_10
#define mbedtls_ssl_sig_hash_set_const_hash \
        mbedtls_ssl_sig_hash_set_const_hash_ncbicxx_2_16_10
#define mbedtls_ssl_sig_hash_set_find \
        mbedtls_ssl_sig_hash_set_find_ncbicxx_2_16_10
#define mbedtls_ssl_transform_free \
        mbedtls_ssl_transform_free_ncbicxx_2_16_10
#define mbedtls_ssl_update_handshake_status \
        mbedtls_ssl_update_handshake_status_ncbicxx_2_16_10
#define mbedtls_ssl_write \
        mbedtls_ssl_write_ncbicxx_2_16_10
#define mbedtls_ssl_write_certificate \
        mbedtls_ssl_write_certificate_ncbicxx_2_16_10
#define mbedtls_ssl_write_change_cipher_spec \
        mbedtls_ssl_write_change_cipher_spec_ncbicxx_2_16_10
#define mbedtls_ssl_write_finished \
        mbedtls_ssl_write_finished_ncbicxx_2_16_10
#define mbedtls_ssl_write_handshake_msg \
        mbedtls_ssl_write_handshake_msg_ncbicxx_2_16_10
#define mbedtls_ssl_write_record \
        mbedtls_ssl_write_record_ncbicxx_2_16_10
#define mbedtls_ssl_write_version \
        mbedtls_ssl_write_version_ncbicxx_2_16_10
#define mbedtls_mutex_free \
        mbedtls_mutex_free_ncbicxx_2_16_10
#define mbedtls_mutex_init \
        mbedtls_mutex_init_ncbicxx_2_16_10
#define mbedtls_mutex_lock \
        mbedtls_mutex_lock_ncbicxx_2_16_10
#define mbedtls_mutex_unlock \
        mbedtls_mutex_unlock_ncbicxx_2_16_10
#define mbedtls_threading_free_alt \
        mbedtls_threading_free_alt_ncbicxx_2_16_10
#define mbedtls_threading_readdir_mutex \
        mbedtls_threading_readdir_mutex_ncbicxx_2_16_10
#define mbedtls_threading_set_alt \
        mbedtls_threading_set_alt_ncbicxx_2_16_10
#define mbedtls_set_alarm \
        mbedtls_set_alarm_ncbicxx_2_16_10
#define mbedtls_timing_alarmed \
        mbedtls_timing_alarmed_ncbicxx_2_16_10
#define mbedtls_timing_get_delay \
        mbedtls_timing_get_delay_ncbicxx_2_16_10
#define mbedtls_timing_get_timer \
        mbedtls_timing_get_timer_ncbicxx_2_16_10
#define mbedtls_timing_hardclock \
        mbedtls_timing_hardclock_ncbicxx_2_16_10
#define mbedtls_timing_set_delay \
        mbedtls_timing_set_delay_ncbicxx_2_16_10
#define mbedtls_version_get_number \
        mbedtls_version_get_number_ncbicxx_2_16_10
#define mbedtls_version_get_string \
        mbedtls_version_get_string_ncbicxx_2_16_10
#define mbedtls_version_get_string_full \
        mbedtls_version_get_string_full_ncbicxx_2_16_10
#define mbedtls_version_check_feature \
        mbedtls_version_check_feature_ncbicxx_2_16_10
#define mbedtls_x509_dn_gets \
        mbedtls_x509_dn_gets_ncbicxx_2_16_10
#define mbedtls_x509_get_alg \
        mbedtls_x509_get_alg_ncbicxx_2_16_10
#define mbedtls_x509_get_alg_null \
        mbedtls_x509_get_alg_null_ncbicxx_2_16_10
#define mbedtls_x509_get_ext \
        mbedtls_x509_get_ext_ncbicxx_2_16_10
#define mbedtls_x509_get_name \
        mbedtls_x509_get_name_ncbicxx_2_16_10
#define mbedtls_x509_get_rsassa_pss_params \
        mbedtls_x509_get_rsassa_pss_params_ncbicxx_2_16_10
#define mbedtls_x509_get_serial \
        mbedtls_x509_get_serial_ncbicxx_2_16_10
#define mbedtls_x509_get_sig \
        mbedtls_x509_get_sig_ncbicxx_2_16_10
#define mbedtls_x509_get_sig_alg \
        mbedtls_x509_get_sig_alg_ncbicxx_2_16_10
#define mbedtls_x509_get_time \
        mbedtls_x509_get_time_ncbicxx_2_16_10
#define mbedtls_x509_key_size_helper \
        mbedtls_x509_key_size_helper_ncbicxx_2_16_10
#define mbedtls_x509_serial_gets \
        mbedtls_x509_serial_gets_ncbicxx_2_16_10
#define mbedtls_x509_sig_alg_gets \
        mbedtls_x509_sig_alg_gets_ncbicxx_2_16_10
#define mbedtls_x509_time_is_future \
        mbedtls_x509_time_is_future_ncbicxx_2_16_10
#define mbedtls_x509_time_is_past \
        mbedtls_x509_time_is_past_ncbicxx_2_16_10
#define mbedtls_x509_set_extension \
        mbedtls_x509_set_extension_ncbicxx_2_16_10
#define mbedtls_x509_string_to_names \
        mbedtls_x509_string_to_names_ncbicxx_2_16_10
#define mbedtls_x509_write_extensions \
        mbedtls_x509_write_extensions_ncbicxx_2_16_10
#define mbedtls_x509_write_names \
        mbedtls_x509_write_names_ncbicxx_2_16_10
#define mbedtls_x509_write_sig \
        mbedtls_x509_write_sig_ncbicxx_2_16_10
#define mbedtls_x509_crl_free \
        mbedtls_x509_crl_free_ncbicxx_2_16_10
#define mbedtls_x509_crl_info \
        mbedtls_x509_crl_info_ncbicxx_2_16_10
#define mbedtls_x509_crl_init \
        mbedtls_x509_crl_init_ncbicxx_2_16_10
#define mbedtls_x509_crl_parse \
        mbedtls_x509_crl_parse_ncbicxx_2_16_10
#define mbedtls_x509_crl_parse_der \
        mbedtls_x509_crl_parse_der_ncbicxx_2_16_10
#define mbedtls_x509_crl_parse_file \
        mbedtls_x509_crl_parse_file_ncbicxx_2_16_10
#define mbedtls_x509_crt_check_extended_key_usage \
        mbedtls_x509_crt_check_extended_key_usage_ncbicxx_2_16_10
#define mbedtls_x509_crt_check_key_usage \
        mbedtls_x509_crt_check_key_usage_ncbicxx_2_16_10
#define mbedtls_x509_crt_free \
        mbedtls_x509_crt_free_ncbicxx_2_16_10
#define mbedtls_x509_crt_info \
        mbedtls_x509_crt_info_ncbicxx_2_16_10
#define mbedtls_x509_crt_init \
        mbedtls_x509_crt_init_ncbicxx_2_16_10
#define mbedtls_x509_crt_is_revoked \
        mbedtls_x509_crt_is_revoked_ncbicxx_2_16_10
#define mbedtls_x509_crt_parse \
        mbedtls_x509_crt_parse_ncbicxx_2_16_10
#define mbedtls_x509_crt_parse_der \
        mbedtls_x509_crt_parse_der_ncbicxx_2_16_10
#define mbedtls_x509_crt_parse_file \
        mbedtls_x509_crt_parse_file_ncbicxx_2_16_10
#define mbedtls_x509_crt_parse_path \
        mbedtls_x509_crt_parse_path_ncbicxx_2_16_10
#define mbedtls_x509_crt_profile_default \
        mbedtls_x509_crt_profile_default_ncbicxx_2_16_10
#define mbedtls_x509_crt_profile_next \
        mbedtls_x509_crt_profile_next_ncbicxx_2_16_10
#define mbedtls_x509_crt_profile_suiteb \
        mbedtls_x509_crt_profile_suiteb_ncbicxx_2_16_10
#define mbedtls_x509_crt_verify \
        mbedtls_x509_crt_verify_ncbicxx_2_16_10
#define mbedtls_x509_crt_verify_info \
        mbedtls_x509_crt_verify_info_ncbicxx_2_16_10
#define mbedtls_x509_crt_verify_restartable \
        mbedtls_x509_crt_verify_restartable_ncbicxx_2_16_10
#define mbedtls_x509_crt_verify_with_profile \
        mbedtls_x509_crt_verify_with_profile_ncbicxx_2_16_10
#define mbedtls_x509_csr_free \
        mbedtls_x509_csr_free_ncbicxx_2_16_10
#define mbedtls_x509_csr_info \
        mbedtls_x509_csr_info_ncbicxx_2_16_10
#define mbedtls_x509_csr_init \
        mbedtls_x509_csr_init_ncbicxx_2_16_10
#define mbedtls_x509_csr_parse \
        mbedtls_x509_csr_parse_ncbicxx_2_16_10
#define mbedtls_x509_csr_parse_der \
        mbedtls_x509_csr_parse_der_ncbicxx_2_16_10
#define mbedtls_x509_csr_parse_file \
        mbedtls_x509_csr_parse_file_ncbicxx_2_16_10
#define mbedtls_x509write_crt_der \
        mbedtls_x509write_crt_der_ncbicxx_2_16_10
#define mbedtls_x509write_crt_free \
        mbedtls_x509write_crt_free_ncbicxx_2_16_10
#define mbedtls_x509write_crt_init \
        mbedtls_x509write_crt_init_ncbicxx_2_16_10
#define mbedtls_x509write_crt_pem \
        mbedtls_x509write_crt_pem_ncbicxx_2_16_10
#define mbedtls_x509write_crt_set_authority_key_identifier \
        mbedtls_x509write_crt_set_authority_key_identifier_ncbicxx_2_16_10
#define mbedtls_x509write_crt_set_basic_constraints \
        mbedtls_x509write_crt_set_basic_constraints_ncbicxx_2_16_10
#define mbedtls_x509write_crt_set_extension \
        mbedtls_x509write_crt_set_extension_ncbicxx_2_16_10
#define mbedtls_x509write_crt_set_issuer_key \
        mbedtls_x509write_crt_set_issuer_key_ncbicxx_2_16_10
#define mbedtls_x509write_crt_set_issuer_name \
        mbedtls_x509write_crt_set_issuer_name_ncbicxx_2_16_10
#define mbedtls_x509write_crt_set_key_usage \
        mbedtls_x509write_crt_set_key_usage_ncbicxx_2_16_10
#define mbedtls_x509write_crt_set_md_alg \
        mbedtls_x509write_crt_set_md_alg_ncbicxx_2_16_10
#define mbedtls_x509write_crt_set_ns_cert_type \
        mbedtls_x509write_crt_set_ns_cert_type_ncbicxx_2_16_10
#define mbedtls_x509write_crt_set_serial \
        mbedtls_x509write_crt_set_serial_ncbicxx_2_16_10
#define mbedtls_x509write_crt_set_subject_key \
        mbedtls_x509write_crt_set_subject_key_ncbicxx_2_16_10
#define mbedtls_x509write_crt_set_subject_key_identifier \
        mbedtls_x509write_crt_set_subject_key_identifier_ncbicxx_2_16_10
#define mbedtls_x509write_crt_set_subject_name \
        mbedtls_x509write_crt_set_subject_name_ncbicxx_2_16_10
#define mbedtls_x509write_crt_set_validity \
        mbedtls_x509write_crt_set_validity_ncbicxx_2_16_10
#define mbedtls_x509write_crt_set_version \
        mbedtls_x509write_crt_set_version_ncbicxx_2_16_10
#define mbedtls_x509write_csr_der \
        mbedtls_x509write_csr_der_ncbicxx_2_16_10
#define mbedtls_x509write_csr_free \
        mbedtls_x509write_csr_free_ncbicxx_2_16_10
#define mbedtls_x509write_csr_init \
        mbedtls_x509write_csr_init_ncbicxx_2_16_10
#define mbedtls_x509write_csr_pem \
        mbedtls_x509write_csr_pem_ncbicxx_2_16_10
#define mbedtls_x509write_csr_set_extension \
        mbedtls_x509write_csr_set_extension_ncbicxx_2_16_10
#define mbedtls_x509write_csr_set_key \
        mbedtls_x509write_csr_set_key_ncbicxx_2_16_10
#define mbedtls_x509write_csr_set_key_usage \
        mbedtls_x509write_csr_set_key_usage_ncbicxx_2_16_10
#define mbedtls_x509write_csr_set_md_alg \
        mbedtls_x509write_csr_set_md_alg_ncbicxx_2_16_10
#define mbedtls_x509write_csr_set_ns_cert_type \
        mbedtls_x509write_csr_set_ns_cert_type_ncbicxx_2_16_10
#define mbedtls_x509write_csr_set_subject_name \
        mbedtls_x509write_csr_set_subject_name_ncbicxx_2_16_10
#define mbedtls_xtea_crypt_cbc \
        mbedtls_xtea_crypt_cbc_ncbicxx_2_16_10
#define mbedtls_xtea_crypt_ecb \
        mbedtls_xtea_crypt_ecb_ncbicxx_2_16_10
#define mbedtls_xtea_free \
        mbedtls_xtea_free_ncbicxx_2_16_10
#define mbedtls_xtea_init \
        mbedtls_xtea_init_ncbicxx_2_16_10
#define mbedtls_xtea_setup \
        mbedtls_xtea_setup_ncbicxx_2_16_10
