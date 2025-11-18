/*
 cd $NCBI/mbedtls-3.6.5/src/Debug64MT/library/CMakeFiles && \
 ls mbed*[^c].dir/?*.o | sort -t/ -k2 | \
 while read x; do nm -g -arch all --defined-only $x 2>/dev/null | \
 grep -v '_self_test' | sed -ne '/ [A-TV-Z] /s/ _/ /p' | sort -k3 -u; done | \
 awk '/ / { s=substr($3, 1); print "#define", s " \\\n        " s "_ncbicxx_3_6_5" }'
 */
#define mbedtls_aes_crypt_cbc \
        mbedtls_aes_crypt_cbc_ncbicxx_3_6_5
#define mbedtls_aes_crypt_cfb128 \
        mbedtls_aes_crypt_cfb128_ncbicxx_3_6_5
#define mbedtls_aes_crypt_cfb8 \
        mbedtls_aes_crypt_cfb8_ncbicxx_3_6_5
#define mbedtls_aes_crypt_ctr \
        mbedtls_aes_crypt_ctr_ncbicxx_3_6_5
#define mbedtls_aes_crypt_ecb \
        mbedtls_aes_crypt_ecb_ncbicxx_3_6_5
#define mbedtls_aes_crypt_ofb \
        mbedtls_aes_crypt_ofb_ncbicxx_3_6_5
#define mbedtls_aes_crypt_xts \
        mbedtls_aes_crypt_xts_ncbicxx_3_6_5
#define mbedtls_aes_free \
        mbedtls_aes_free_ncbicxx_3_6_5
#define mbedtls_aes_init \
        mbedtls_aes_init_ncbicxx_3_6_5
#define mbedtls_aes_setkey_dec \
        mbedtls_aes_setkey_dec_ncbicxx_3_6_5
#define mbedtls_aes_setkey_enc \
        mbedtls_aes_setkey_enc_ncbicxx_3_6_5
#define mbedtls_aes_xts_free \
        mbedtls_aes_xts_free_ncbicxx_3_6_5
#define mbedtls_aes_xts_init \
        mbedtls_aes_xts_init_ncbicxx_3_6_5
#define mbedtls_aes_xts_setkey_dec \
        mbedtls_aes_xts_setkey_dec_ncbicxx_3_6_5
#define mbedtls_aes_xts_setkey_enc \
        mbedtls_aes_xts_setkey_enc_ncbicxx_3_6_5
#define mbedtls_internal_aes_decrypt \
        mbedtls_internal_aes_decrypt_ncbicxx_3_6_5
#define mbedtls_internal_aes_encrypt \
        mbedtls_internal_aes_encrypt_ncbicxx_3_6_5
#define mbedtls_aesce_crypt_ecb \
        mbedtls_aesce_crypt_ecb_ncbicxx_3_6_5
#define mbedtls_aesce_gcm_mult \
        mbedtls_aesce_gcm_mult_ncbicxx_3_6_5
#define mbedtls_aesce_inverse_key \
        mbedtls_aesce_inverse_key_ncbicxx_3_6_5
#define mbedtls_aesce_setkey_enc \
        mbedtls_aesce_setkey_enc_ncbicxx_3_6_5
#define mbedtls_aesni_crypt_ecb \
        mbedtls_aesni_crypt_ecb_ncbicxx_3_6_5
#define mbedtls_aesni_gcm_mult \
        mbedtls_aesni_gcm_mult_ncbicxx_3_6_5
#define mbedtls_aesni_has_support \
        mbedtls_aesni_has_support_ncbicxx_3_6_5
#define mbedtls_aesni_inverse_key \
        mbedtls_aesni_inverse_key_ncbicxx_3_6_5
#define mbedtls_aesni_setkey_enc \
        mbedtls_aesni_setkey_enc_ncbicxx_3_6_5
#define mbedtls_aria_crypt_cbc \
        mbedtls_aria_crypt_cbc_ncbicxx_3_6_5
#define mbedtls_aria_crypt_cfb128 \
        mbedtls_aria_crypt_cfb128_ncbicxx_3_6_5
#define mbedtls_aria_crypt_ctr \
        mbedtls_aria_crypt_ctr_ncbicxx_3_6_5
#define mbedtls_aria_crypt_ecb \
        mbedtls_aria_crypt_ecb_ncbicxx_3_6_5
#define mbedtls_aria_free \
        mbedtls_aria_free_ncbicxx_3_6_5
#define mbedtls_aria_init \
        mbedtls_aria_init_ncbicxx_3_6_5
#define mbedtls_aria_setkey_dec \
        mbedtls_aria_setkey_dec_ncbicxx_3_6_5
#define mbedtls_aria_setkey_enc \
        mbedtls_aria_setkey_enc_ncbicxx_3_6_5
#define mbedtls_asn1_find_named_data \
        mbedtls_asn1_find_named_data_ncbicxx_3_6_5
#define mbedtls_asn1_free_named_data \
        mbedtls_asn1_free_named_data_ncbicxx_3_6_5
#define mbedtls_asn1_free_named_data_list \
        mbedtls_asn1_free_named_data_list_ncbicxx_3_6_5
#define mbedtls_asn1_free_named_data_list_shallow \
        mbedtls_asn1_free_named_data_list_shallow_ncbicxx_3_6_5
#define mbedtls_asn1_get_alg \
        mbedtls_asn1_get_alg_ncbicxx_3_6_5
#define mbedtls_asn1_get_alg_null \
        mbedtls_asn1_get_alg_null_ncbicxx_3_6_5
#define mbedtls_asn1_get_bitstring \
        mbedtls_asn1_get_bitstring_ncbicxx_3_6_5
#define mbedtls_asn1_get_bitstring_null \
        mbedtls_asn1_get_bitstring_null_ncbicxx_3_6_5
#define mbedtls_asn1_get_bool \
        mbedtls_asn1_get_bool_ncbicxx_3_6_5
#define mbedtls_asn1_get_enum \
        mbedtls_asn1_get_enum_ncbicxx_3_6_5
#define mbedtls_asn1_get_int \
        mbedtls_asn1_get_int_ncbicxx_3_6_5
#define mbedtls_asn1_get_len \
        mbedtls_asn1_get_len_ncbicxx_3_6_5
#define mbedtls_asn1_get_mpi \
        mbedtls_asn1_get_mpi_ncbicxx_3_6_5
#define mbedtls_asn1_get_sequence_of \
        mbedtls_asn1_get_sequence_of_ncbicxx_3_6_5
#define mbedtls_asn1_get_tag \
        mbedtls_asn1_get_tag_ncbicxx_3_6_5
#define mbedtls_asn1_sequence_free \
        mbedtls_asn1_sequence_free_ncbicxx_3_6_5
#define mbedtls_asn1_traverse_sequence_of \
        mbedtls_asn1_traverse_sequence_of_ncbicxx_3_6_5
#define mbedtls_asn1_store_named_data \
        mbedtls_asn1_store_named_data_ncbicxx_3_6_5
#define mbedtls_asn1_write_algorithm_identifier \
        mbedtls_asn1_write_algorithm_identifier_ncbicxx_3_6_5
#define mbedtls_asn1_write_algorithm_identifier_ext \
        mbedtls_asn1_write_algorithm_identifier_ext_ncbicxx_3_6_5
#define mbedtls_asn1_write_bitstring \
        mbedtls_asn1_write_bitstring_ncbicxx_3_6_5
#define mbedtls_asn1_write_bool \
        mbedtls_asn1_write_bool_ncbicxx_3_6_5
#define mbedtls_asn1_write_enum \
        mbedtls_asn1_write_enum_ncbicxx_3_6_5
#define mbedtls_asn1_write_ia5_string \
        mbedtls_asn1_write_ia5_string_ncbicxx_3_6_5
#define mbedtls_asn1_write_int \
        mbedtls_asn1_write_int_ncbicxx_3_6_5
#define mbedtls_asn1_write_len \
        mbedtls_asn1_write_len_ncbicxx_3_6_5
#define mbedtls_asn1_write_mpi \
        mbedtls_asn1_write_mpi_ncbicxx_3_6_5
#define mbedtls_asn1_write_named_bitstring \
        mbedtls_asn1_write_named_bitstring_ncbicxx_3_6_5
#define mbedtls_asn1_write_null \
        mbedtls_asn1_write_null_ncbicxx_3_6_5
#define mbedtls_asn1_write_octet_string \
        mbedtls_asn1_write_octet_string_ncbicxx_3_6_5
#define mbedtls_asn1_write_oid \
        mbedtls_asn1_write_oid_ncbicxx_3_6_5
#define mbedtls_asn1_write_printable_string \
        mbedtls_asn1_write_printable_string_ncbicxx_3_6_5
#define mbedtls_asn1_write_raw_buffer \
        mbedtls_asn1_write_raw_buffer_ncbicxx_3_6_5
#define mbedtls_asn1_write_tag \
        mbedtls_asn1_write_tag_ncbicxx_3_6_5
#define mbedtls_asn1_write_tagged_string \
        mbedtls_asn1_write_tagged_string_ncbicxx_3_6_5
#define mbedtls_asn1_write_utf8_string \
        mbedtls_asn1_write_utf8_string_ncbicxx_3_6_5
#define mbedtls_base64_decode \
        mbedtls_base64_decode_ncbicxx_3_6_5
#define mbedtls_base64_encode \
        mbedtls_base64_encode_ncbicxx_3_6_5
#define mbedtls_mpi_add_abs \
        mbedtls_mpi_add_abs_ncbicxx_3_6_5
#define mbedtls_mpi_add_int \
        mbedtls_mpi_add_int_ncbicxx_3_6_5
#define mbedtls_mpi_add_mpi \
        mbedtls_mpi_add_mpi_ncbicxx_3_6_5
#define mbedtls_mpi_bitlen \
        mbedtls_mpi_bitlen_ncbicxx_3_6_5
#define mbedtls_mpi_cmp_abs \
        mbedtls_mpi_cmp_abs_ncbicxx_3_6_5
#define mbedtls_mpi_cmp_int \
        mbedtls_mpi_cmp_int_ncbicxx_3_6_5
#define mbedtls_mpi_cmp_mpi \
        mbedtls_mpi_cmp_mpi_ncbicxx_3_6_5
#define mbedtls_mpi_copy \
        mbedtls_mpi_copy_ncbicxx_3_6_5
#define mbedtls_mpi_div_int \
        mbedtls_mpi_div_int_ncbicxx_3_6_5
#define mbedtls_mpi_div_mpi \
        mbedtls_mpi_div_mpi_ncbicxx_3_6_5
#define mbedtls_mpi_exp_mod \
        mbedtls_mpi_exp_mod_ncbicxx_3_6_5
#define mbedtls_mpi_exp_mod_unsafe \
        mbedtls_mpi_exp_mod_unsafe_ncbicxx_3_6_5
#define mbedtls_mpi_fill_random \
        mbedtls_mpi_fill_random_ncbicxx_3_6_5
#define mbedtls_mpi_free \
        mbedtls_mpi_free_ncbicxx_3_6_5
#define mbedtls_mpi_gcd \
        mbedtls_mpi_gcd_ncbicxx_3_6_5
#define mbedtls_mpi_gcd_modinv_odd \
        mbedtls_mpi_gcd_modinv_odd_ncbicxx_3_6_5
#define mbedtls_mpi_gen_prime \
        mbedtls_mpi_gen_prime_ncbicxx_3_6_5
#define mbedtls_mpi_get_bit \
        mbedtls_mpi_get_bit_ncbicxx_3_6_5
#define mbedtls_mpi_grow \
        mbedtls_mpi_grow_ncbicxx_3_6_5
#define mbedtls_mpi_init \
        mbedtls_mpi_init_ncbicxx_3_6_5
#define mbedtls_mpi_inv_mod \
        mbedtls_mpi_inv_mod_ncbicxx_3_6_5
#define mbedtls_mpi_inv_mod_even_in_range \
        mbedtls_mpi_inv_mod_even_in_range_ncbicxx_3_6_5
#define mbedtls_mpi_inv_mod_odd \
        mbedtls_mpi_inv_mod_odd_ncbicxx_3_6_5
#define mbedtls_mpi_is_prime_ext \
        mbedtls_mpi_is_prime_ext_ncbicxx_3_6_5
#define mbedtls_mpi_lsb \
        mbedtls_mpi_lsb_ncbicxx_3_6_5
#define mbedtls_mpi_lset \
        mbedtls_mpi_lset_ncbicxx_3_6_5
#define mbedtls_mpi_lt_mpi_ct \
        mbedtls_mpi_lt_mpi_ct_ncbicxx_3_6_5
#define mbedtls_mpi_mod_int \
        mbedtls_mpi_mod_int_ncbicxx_3_6_5
#define mbedtls_mpi_mod_mpi \
        mbedtls_mpi_mod_mpi_ncbicxx_3_6_5
#define mbedtls_mpi_mul_int \
        mbedtls_mpi_mul_int_ncbicxx_3_6_5
#define mbedtls_mpi_mul_mpi \
        mbedtls_mpi_mul_mpi_ncbicxx_3_6_5
#define mbedtls_mpi_random \
        mbedtls_mpi_random_ncbicxx_3_6_5
#define mbedtls_mpi_read_binary \
        mbedtls_mpi_read_binary_ncbicxx_3_6_5
#define mbedtls_mpi_read_binary_le \
        mbedtls_mpi_read_binary_le_ncbicxx_3_6_5
#define mbedtls_mpi_read_file \
        mbedtls_mpi_read_file_ncbicxx_3_6_5
#define mbedtls_mpi_read_string \
        mbedtls_mpi_read_string_ncbicxx_3_6_5
#define mbedtls_mpi_safe_cond_assign \
        mbedtls_mpi_safe_cond_assign_ncbicxx_3_6_5
#define mbedtls_mpi_safe_cond_swap \
        mbedtls_mpi_safe_cond_swap_ncbicxx_3_6_5
#define mbedtls_mpi_set_bit \
        mbedtls_mpi_set_bit_ncbicxx_3_6_5
#define mbedtls_mpi_shift_l \
        mbedtls_mpi_shift_l_ncbicxx_3_6_5
#define mbedtls_mpi_shift_r \
        mbedtls_mpi_shift_r_ncbicxx_3_6_5
#define mbedtls_mpi_shrink \
        mbedtls_mpi_shrink_ncbicxx_3_6_5
#define mbedtls_mpi_size \
        mbedtls_mpi_size_ncbicxx_3_6_5
#define mbedtls_mpi_sub_abs \
        mbedtls_mpi_sub_abs_ncbicxx_3_6_5
#define mbedtls_mpi_sub_int \
        mbedtls_mpi_sub_int_ncbicxx_3_6_5
#define mbedtls_mpi_sub_mpi \
        mbedtls_mpi_sub_mpi_ncbicxx_3_6_5
#define mbedtls_mpi_swap \
        mbedtls_mpi_swap_ncbicxx_3_6_5
#define mbedtls_mpi_write_binary \
        mbedtls_mpi_write_binary_ncbicxx_3_6_5
#define mbedtls_mpi_write_binary_le \
        mbedtls_mpi_write_binary_le_ncbicxx_3_6_5
#define mbedtls_mpi_write_file \
        mbedtls_mpi_write_file_ncbicxx_3_6_5
#define mbedtls_mpi_write_string \
        mbedtls_mpi_write_string_ncbicxx_3_6_5
#define mbedtls_mpi_core_add \
        mbedtls_mpi_core_add_ncbicxx_3_6_5
#define mbedtls_mpi_core_add_if \
        mbedtls_mpi_core_add_if_ncbicxx_3_6_5
#define mbedtls_mpi_core_bigendian_to_host \
        mbedtls_mpi_core_bigendian_to_host_ncbicxx_3_6_5
#define mbedtls_mpi_core_bitlen \
        mbedtls_mpi_core_bitlen_ncbicxx_3_6_5
#define mbedtls_mpi_core_check_zero_ct \
        mbedtls_mpi_core_check_zero_ct_ncbicxx_3_6_5
#define mbedtls_mpi_core_clz \
        mbedtls_mpi_core_clz_ncbicxx_3_6_5
#define mbedtls_mpi_core_cond_assign \
        mbedtls_mpi_core_cond_assign_ncbicxx_3_6_5
#define mbedtls_mpi_core_cond_swap \
        mbedtls_mpi_core_cond_swap_ncbicxx_3_6_5
#define mbedtls_mpi_core_exp_mod \
        mbedtls_mpi_core_exp_mod_ncbicxx_3_6_5
#define mbedtls_mpi_core_exp_mod_unsafe \
        mbedtls_mpi_core_exp_mod_unsafe_ncbicxx_3_6_5
#define mbedtls_mpi_core_exp_mod_working_limbs \
        mbedtls_mpi_core_exp_mod_working_limbs_ncbicxx_3_6_5
#define mbedtls_mpi_core_fill_random \
        mbedtls_mpi_core_fill_random_ncbicxx_3_6_5
#define mbedtls_mpi_core_from_mont_rep \
        mbedtls_mpi_core_from_mont_rep_ncbicxx_3_6_5
#define mbedtls_mpi_core_gcd_modinv_odd \
        mbedtls_mpi_core_gcd_modinv_odd_ncbicxx_3_6_5
#define mbedtls_mpi_core_get_mont_r2_unsafe \
        mbedtls_mpi_core_get_mont_r2_unsafe_ncbicxx_3_6_5
#define mbedtls_mpi_core_lt_ct \
        mbedtls_mpi_core_lt_ct_ncbicxx_3_6_5
#define mbedtls_mpi_core_mla \
        mbedtls_mpi_core_mla_ncbicxx_3_6_5
#define mbedtls_mpi_core_montmul \
        mbedtls_mpi_core_montmul_ncbicxx_3_6_5
#define mbedtls_mpi_core_montmul_init \
        mbedtls_mpi_core_montmul_init_ncbicxx_3_6_5
#define mbedtls_mpi_core_mul \
        mbedtls_mpi_core_mul_ncbicxx_3_6_5
#define mbedtls_mpi_core_random \
        mbedtls_mpi_core_random_ncbicxx_3_6_5
#define mbedtls_mpi_core_read_be \
        mbedtls_mpi_core_read_be_ncbicxx_3_6_5
#define mbedtls_mpi_core_read_le \
        mbedtls_mpi_core_read_le_ncbicxx_3_6_5
#define mbedtls_mpi_core_shift_l \
        mbedtls_mpi_core_shift_l_ncbicxx_3_6_5
#define mbedtls_mpi_core_shift_r \
        mbedtls_mpi_core_shift_r_ncbicxx_3_6_5
#define mbedtls_mpi_core_sub \
        mbedtls_mpi_core_sub_ncbicxx_3_6_5
#define mbedtls_mpi_core_sub_int \
        mbedtls_mpi_core_sub_int_ncbicxx_3_6_5
#define mbedtls_mpi_core_to_mont_rep \
        mbedtls_mpi_core_to_mont_rep_ncbicxx_3_6_5
#define mbedtls_mpi_core_uint_le_mpi \
        mbedtls_mpi_core_uint_le_mpi_ncbicxx_3_6_5
#define mbedtls_mpi_core_write_be \
        mbedtls_mpi_core_write_be_ncbicxx_3_6_5
#define mbedtls_mpi_core_write_le \
        mbedtls_mpi_core_write_le_ncbicxx_3_6_5
#define mbedtls_camellia_crypt_cbc \
        mbedtls_camellia_crypt_cbc_ncbicxx_3_6_5
#define mbedtls_camellia_crypt_cfb128 \
        mbedtls_camellia_crypt_cfb128_ncbicxx_3_6_5
#define mbedtls_camellia_crypt_ctr \
        mbedtls_camellia_crypt_ctr_ncbicxx_3_6_5
#define mbedtls_camellia_crypt_ecb \
        mbedtls_camellia_crypt_ecb_ncbicxx_3_6_5
#define mbedtls_camellia_free \
        mbedtls_camellia_free_ncbicxx_3_6_5
#define mbedtls_camellia_init \
        mbedtls_camellia_init_ncbicxx_3_6_5
#define mbedtls_camellia_setkey_dec \
        mbedtls_camellia_setkey_dec_ncbicxx_3_6_5
#define mbedtls_camellia_setkey_enc \
        mbedtls_camellia_setkey_enc_ncbicxx_3_6_5
#define mbedtls_ccm_auth_decrypt \
        mbedtls_ccm_auth_decrypt_ncbicxx_3_6_5
#define mbedtls_ccm_encrypt_and_tag \
        mbedtls_ccm_encrypt_and_tag_ncbicxx_3_6_5
#define mbedtls_ccm_finish \
        mbedtls_ccm_finish_ncbicxx_3_6_5
#define mbedtls_ccm_free \
        mbedtls_ccm_free_ncbicxx_3_6_5
#define mbedtls_ccm_init \
        mbedtls_ccm_init_ncbicxx_3_6_5
#define mbedtls_ccm_set_lengths \
        mbedtls_ccm_set_lengths_ncbicxx_3_6_5
#define mbedtls_ccm_setkey \
        mbedtls_ccm_setkey_ncbicxx_3_6_5
#define mbedtls_ccm_star_auth_decrypt \
        mbedtls_ccm_star_auth_decrypt_ncbicxx_3_6_5
#define mbedtls_ccm_star_encrypt_and_tag \
        mbedtls_ccm_star_encrypt_and_tag_ncbicxx_3_6_5
#define mbedtls_ccm_starts \
        mbedtls_ccm_starts_ncbicxx_3_6_5
#define mbedtls_ccm_update \
        mbedtls_ccm_update_ncbicxx_3_6_5
#define mbedtls_ccm_update_ad \
        mbedtls_ccm_update_ad_ncbicxx_3_6_5
#define mbedtls_chacha20_crypt \
        mbedtls_chacha20_crypt_ncbicxx_3_6_5
#define mbedtls_chacha20_free \
        mbedtls_chacha20_free_ncbicxx_3_6_5
#define mbedtls_chacha20_init \
        mbedtls_chacha20_init_ncbicxx_3_6_5
#define mbedtls_chacha20_setkey \
        mbedtls_chacha20_setkey_ncbicxx_3_6_5
#define mbedtls_chacha20_starts \
        mbedtls_chacha20_starts_ncbicxx_3_6_5
#define mbedtls_chacha20_update \
        mbedtls_chacha20_update_ncbicxx_3_6_5
#define mbedtls_chachapoly_auth_decrypt \
        mbedtls_chachapoly_auth_decrypt_ncbicxx_3_6_5
#define mbedtls_chachapoly_encrypt_and_tag \
        mbedtls_chachapoly_encrypt_and_tag_ncbicxx_3_6_5
#define mbedtls_chachapoly_finish \
        mbedtls_chachapoly_finish_ncbicxx_3_6_5
#define mbedtls_chachapoly_free \
        mbedtls_chachapoly_free_ncbicxx_3_6_5
#define mbedtls_chachapoly_init \
        mbedtls_chachapoly_init_ncbicxx_3_6_5
#define mbedtls_chachapoly_setkey \
        mbedtls_chachapoly_setkey_ncbicxx_3_6_5
#define mbedtls_chachapoly_starts \
        mbedtls_chachapoly_starts_ncbicxx_3_6_5
#define mbedtls_chachapoly_update \
        mbedtls_chachapoly_update_ncbicxx_3_6_5
#define mbedtls_chachapoly_update_aad \
        mbedtls_chachapoly_update_aad_ncbicxx_3_6_5
#define mbedtls_cipher_auth_decrypt_ext \
        mbedtls_cipher_auth_decrypt_ext_ncbicxx_3_6_5
#define mbedtls_cipher_auth_encrypt_ext \
        mbedtls_cipher_auth_encrypt_ext_ncbicxx_3_6_5
#define mbedtls_cipher_check_tag \
        mbedtls_cipher_check_tag_ncbicxx_3_6_5
#define mbedtls_cipher_crypt \
        mbedtls_cipher_crypt_ncbicxx_3_6_5
#define mbedtls_cipher_finish \
        mbedtls_cipher_finish_ncbicxx_3_6_5
#define mbedtls_cipher_finish_padded \
        mbedtls_cipher_finish_padded_ncbicxx_3_6_5
#define mbedtls_cipher_free \
        mbedtls_cipher_free_ncbicxx_3_6_5
#define mbedtls_cipher_info_from_string \
        mbedtls_cipher_info_from_string_ncbicxx_3_6_5
#define mbedtls_cipher_info_from_type \
        mbedtls_cipher_info_from_type_ncbicxx_3_6_5
#define mbedtls_cipher_info_from_values \
        mbedtls_cipher_info_from_values_ncbicxx_3_6_5
#define mbedtls_cipher_init \
        mbedtls_cipher_init_ncbicxx_3_6_5
#define mbedtls_cipher_list \
        mbedtls_cipher_list_ncbicxx_3_6_5
#define mbedtls_cipher_reset \
        mbedtls_cipher_reset_ncbicxx_3_6_5
#define mbedtls_cipher_set_iv \
        mbedtls_cipher_set_iv_ncbicxx_3_6_5
#define mbedtls_cipher_set_padding_mode \
        mbedtls_cipher_set_padding_mode_ncbicxx_3_6_5
#define mbedtls_cipher_setkey \
        mbedtls_cipher_setkey_ncbicxx_3_6_5
#define mbedtls_cipher_setup \
        mbedtls_cipher_setup_ncbicxx_3_6_5
#define mbedtls_cipher_update \
        mbedtls_cipher_update_ncbicxx_3_6_5
#define mbedtls_cipher_update_ad \
        mbedtls_cipher_update_ad_ncbicxx_3_6_5
#define mbedtls_cipher_write_tag \
        mbedtls_cipher_write_tag_ncbicxx_3_6_5
#define mbedtls_cipher_base_lookup_table \
        mbedtls_cipher_base_lookup_table_ncbicxx_3_6_5
#define mbedtls_cipher_definitions \
        mbedtls_cipher_definitions_ncbicxx_3_6_5
#define mbedtls_cipher_supported \
        mbedtls_cipher_supported_ncbicxx_3_6_5
#define mbedtls_aes_cmac_prf_128 \
        mbedtls_aes_cmac_prf_128_ncbicxx_3_6_5
#define mbedtls_cipher_cmac \
        mbedtls_cipher_cmac_ncbicxx_3_6_5
#define mbedtls_cipher_cmac_finish \
        mbedtls_cipher_cmac_finish_ncbicxx_3_6_5
#define mbedtls_cipher_cmac_reset \
        mbedtls_cipher_cmac_reset_ncbicxx_3_6_5
#define mbedtls_cipher_cmac_starts \
        mbedtls_cipher_cmac_starts_ncbicxx_3_6_5
#define mbedtls_cipher_cmac_update \
        mbedtls_cipher_cmac_update_ncbicxx_3_6_5
#define mbedtls_ct_memcmp \
        mbedtls_ct_memcmp_ncbicxx_3_6_5
#define mbedtls_ct_memcmp_partial \
        mbedtls_ct_memcmp_partial_ncbicxx_3_6_5
#define mbedtls_ct_memcpy_if \
        mbedtls_ct_memcpy_if_ncbicxx_3_6_5
#define mbedtls_ct_memcpy_offset \
        mbedtls_ct_memcpy_offset_ncbicxx_3_6_5
#define mbedtls_ct_memmove_left \
        mbedtls_ct_memmove_left_ncbicxx_3_6_5
#define mbedtls_ct_zeroize_if \
        mbedtls_ct_zeroize_if_ncbicxx_3_6_5
#define mbedtls_ctr_drbg_free \
        mbedtls_ctr_drbg_free_ncbicxx_3_6_5
#define mbedtls_ctr_drbg_init \
        mbedtls_ctr_drbg_init_ncbicxx_3_6_5
#define mbedtls_ctr_drbg_random \
        mbedtls_ctr_drbg_random_ncbicxx_3_6_5
#define mbedtls_ctr_drbg_random_with_add \
        mbedtls_ctr_drbg_random_with_add_ncbicxx_3_6_5
#define mbedtls_ctr_drbg_reseed \
        mbedtls_ctr_drbg_reseed_ncbicxx_3_6_5
#define mbedtls_ctr_drbg_seed \
        mbedtls_ctr_drbg_seed_ncbicxx_3_6_5
#define mbedtls_ctr_drbg_set_entropy_len \
        mbedtls_ctr_drbg_set_entropy_len_ncbicxx_3_6_5
#define mbedtls_ctr_drbg_set_nonce_len \
        mbedtls_ctr_drbg_set_nonce_len_ncbicxx_3_6_5
#define mbedtls_ctr_drbg_set_prediction_resistance \
        mbedtls_ctr_drbg_set_prediction_resistance_ncbicxx_3_6_5
#define mbedtls_ctr_drbg_set_reseed_interval \
        mbedtls_ctr_drbg_set_reseed_interval_ncbicxx_3_6_5
#define mbedtls_ctr_drbg_update \
        mbedtls_ctr_drbg_update_ncbicxx_3_6_5
#define mbedtls_ctr_drbg_update_seed_file \
        mbedtls_ctr_drbg_update_seed_file_ncbicxx_3_6_5
#define mbedtls_ctr_drbg_write_seed_file \
        mbedtls_ctr_drbg_write_seed_file_ncbicxx_3_6_5
#define mbedtls_debug_print_buf \
        mbedtls_debug_print_buf_ncbicxx_3_6_5
#define mbedtls_debug_print_crt \
        mbedtls_debug_print_crt_ncbicxx_3_6_5
#define mbedtls_debug_print_ecp \
        mbedtls_debug_print_ecp_ncbicxx_3_6_5
#define mbedtls_debug_print_mpi \
        mbedtls_debug_print_mpi_ncbicxx_3_6_5
#define mbedtls_debug_print_msg \
        mbedtls_debug_print_msg_ncbicxx_3_6_5
#define mbedtls_debug_print_ret \
        mbedtls_debug_print_ret_ncbicxx_3_6_5
#define mbedtls_debug_printf_ecdh \
        mbedtls_debug_printf_ecdh_ncbicxx_3_6_5
#define mbedtls_debug_set_threshold \
        mbedtls_debug_set_threshold_ncbicxx_3_6_5
#define mbedtls_des3_crypt_cbc \
        mbedtls_des3_crypt_cbc_ncbicxx_3_6_5
#define mbedtls_des3_crypt_ecb \
        mbedtls_des3_crypt_ecb_ncbicxx_3_6_5
#define mbedtls_des3_free \
        mbedtls_des3_free_ncbicxx_3_6_5
#define mbedtls_des3_init \
        mbedtls_des3_init_ncbicxx_3_6_5
#define mbedtls_des3_set2key_dec \
        mbedtls_des3_set2key_dec_ncbicxx_3_6_5
#define mbedtls_des3_set2key_enc \
        mbedtls_des3_set2key_enc_ncbicxx_3_6_5
#define mbedtls_des3_set3key_dec \
        mbedtls_des3_set3key_dec_ncbicxx_3_6_5
#define mbedtls_des3_set3key_enc \
        mbedtls_des3_set3key_enc_ncbicxx_3_6_5
#define mbedtls_des_crypt_cbc \
        mbedtls_des_crypt_cbc_ncbicxx_3_6_5
#define mbedtls_des_crypt_ecb \
        mbedtls_des_crypt_ecb_ncbicxx_3_6_5
#define mbedtls_des_free \
        mbedtls_des_free_ncbicxx_3_6_5
#define mbedtls_des_init \
        mbedtls_des_init_ncbicxx_3_6_5
#define mbedtls_des_key_check_key_parity \
        mbedtls_des_key_check_key_parity_ncbicxx_3_6_5
#define mbedtls_des_key_check_weak \
        mbedtls_des_key_check_weak_ncbicxx_3_6_5
#define mbedtls_des_key_set_parity \
        mbedtls_des_key_set_parity_ncbicxx_3_6_5
#define mbedtls_des_setkey \
        mbedtls_des_setkey_ncbicxx_3_6_5
#define mbedtls_des_setkey_dec \
        mbedtls_des_setkey_dec_ncbicxx_3_6_5
#define mbedtls_des_setkey_enc \
        mbedtls_des_setkey_enc_ncbicxx_3_6_5
#define mbedtls_dhm_calc_secret \
        mbedtls_dhm_calc_secret_ncbicxx_3_6_5
#define mbedtls_dhm_free \
        mbedtls_dhm_free_ncbicxx_3_6_5
#define mbedtls_dhm_get_bitlen \
        mbedtls_dhm_get_bitlen_ncbicxx_3_6_5
#define mbedtls_dhm_get_len \
        mbedtls_dhm_get_len_ncbicxx_3_6_5
#define mbedtls_dhm_get_value \
        mbedtls_dhm_get_value_ncbicxx_3_6_5
#define mbedtls_dhm_init \
        mbedtls_dhm_init_ncbicxx_3_6_5
#define mbedtls_dhm_make_params \
        mbedtls_dhm_make_params_ncbicxx_3_6_5
#define mbedtls_dhm_make_public \
        mbedtls_dhm_make_public_ncbicxx_3_6_5
#define mbedtls_dhm_parse_dhm \
        mbedtls_dhm_parse_dhm_ncbicxx_3_6_5
#define mbedtls_dhm_parse_dhmfile \
        mbedtls_dhm_parse_dhmfile_ncbicxx_3_6_5
#define mbedtls_dhm_read_params \
        mbedtls_dhm_read_params_ncbicxx_3_6_5
#define mbedtls_dhm_read_public \
        mbedtls_dhm_read_public_ncbicxx_3_6_5
#define mbedtls_dhm_set_group \
        mbedtls_dhm_set_group_ncbicxx_3_6_5
#define mbedtls_ecdh_calc_secret \
        mbedtls_ecdh_calc_secret_ncbicxx_3_6_5
#define mbedtls_ecdh_can_do \
        mbedtls_ecdh_can_do_ncbicxx_3_6_5
#define mbedtls_ecdh_compute_shared \
        mbedtls_ecdh_compute_shared_ncbicxx_3_6_5
#define mbedtls_ecdh_free \
        mbedtls_ecdh_free_ncbicxx_3_6_5
#define mbedtls_ecdh_gen_public \
        mbedtls_ecdh_gen_public_ncbicxx_3_6_5
#define mbedtls_ecdh_get_grp_id \
        mbedtls_ecdh_get_grp_id_ncbicxx_3_6_5
#define mbedtls_ecdh_get_params \
        mbedtls_ecdh_get_params_ncbicxx_3_6_5
#define mbedtls_ecdh_init \
        mbedtls_ecdh_init_ncbicxx_3_6_5
#define mbedtls_ecdh_make_params \
        mbedtls_ecdh_make_params_ncbicxx_3_6_5
#define mbedtls_ecdh_make_public \
        mbedtls_ecdh_make_public_ncbicxx_3_6_5
#define mbedtls_ecdh_read_params \
        mbedtls_ecdh_read_params_ncbicxx_3_6_5
#define mbedtls_ecdh_read_public \
        mbedtls_ecdh_read_public_ncbicxx_3_6_5
#define mbedtls_ecdh_setup \
        mbedtls_ecdh_setup_ncbicxx_3_6_5
#define mbedtls_ecdsa_can_do \
        mbedtls_ecdsa_can_do_ncbicxx_3_6_5
#define mbedtls_ecdsa_free \
        mbedtls_ecdsa_free_ncbicxx_3_6_5
#define mbedtls_ecdsa_from_keypair \
        mbedtls_ecdsa_from_keypair_ncbicxx_3_6_5
#define mbedtls_ecdsa_genkey \
        mbedtls_ecdsa_genkey_ncbicxx_3_6_5
#define mbedtls_ecdsa_init \
        mbedtls_ecdsa_init_ncbicxx_3_6_5
#define mbedtls_ecdsa_read_signature \
        mbedtls_ecdsa_read_signature_ncbicxx_3_6_5
#define mbedtls_ecdsa_read_signature_restartable \
        mbedtls_ecdsa_read_signature_restartable_ncbicxx_3_6_5
#define mbedtls_ecdsa_sign \
        mbedtls_ecdsa_sign_ncbicxx_3_6_5
#define mbedtls_ecdsa_sign_det_ext \
        mbedtls_ecdsa_sign_det_ext_ncbicxx_3_6_5
#define mbedtls_ecdsa_sign_det_restartable \
        mbedtls_ecdsa_sign_det_restartable_ncbicxx_3_6_5
#define mbedtls_ecdsa_sign_restartable \
        mbedtls_ecdsa_sign_restartable_ncbicxx_3_6_5
#define mbedtls_ecdsa_verify \
        mbedtls_ecdsa_verify_ncbicxx_3_6_5
#define mbedtls_ecdsa_verify_restartable \
        mbedtls_ecdsa_verify_restartable_ncbicxx_3_6_5
#define mbedtls_ecdsa_write_signature \
        mbedtls_ecdsa_write_signature_ncbicxx_3_6_5
#define mbedtls_ecdsa_write_signature_restartable \
        mbedtls_ecdsa_write_signature_restartable_ncbicxx_3_6_5
#define mbedtls_ecjpake_check \
        mbedtls_ecjpake_check_ncbicxx_3_6_5
#define mbedtls_ecjpake_derive_secret \
        mbedtls_ecjpake_derive_secret_ncbicxx_3_6_5
#define mbedtls_ecjpake_free \
        mbedtls_ecjpake_free_ncbicxx_3_6_5
#define mbedtls_ecjpake_init \
        mbedtls_ecjpake_init_ncbicxx_3_6_5
#define mbedtls_ecjpake_read_round_one \
        mbedtls_ecjpake_read_round_one_ncbicxx_3_6_5
#define mbedtls_ecjpake_read_round_two \
        mbedtls_ecjpake_read_round_two_ncbicxx_3_6_5
#define mbedtls_ecjpake_set_point_format \
        mbedtls_ecjpake_set_point_format_ncbicxx_3_6_5
#define mbedtls_ecjpake_setup \
        mbedtls_ecjpake_setup_ncbicxx_3_6_5
#define mbedtls_ecjpake_write_round_one \
        mbedtls_ecjpake_write_round_one_ncbicxx_3_6_5
#define mbedtls_ecjpake_write_round_two \
        mbedtls_ecjpake_write_round_two_ncbicxx_3_6_5
#define mbedtls_ecjpake_write_shared_key \
        mbedtls_ecjpake_write_shared_key_ncbicxx_3_6_5
#define mbedtls_ecp_check_privkey \
        mbedtls_ecp_check_privkey_ncbicxx_3_6_5
#define mbedtls_ecp_check_pub_priv \
        mbedtls_ecp_check_pub_priv_ncbicxx_3_6_5
#define mbedtls_ecp_check_pubkey \
        mbedtls_ecp_check_pubkey_ncbicxx_3_6_5
#define mbedtls_ecp_copy \
        mbedtls_ecp_copy_ncbicxx_3_6_5
#define mbedtls_ecp_curve_info_from_grp_id \
        mbedtls_ecp_curve_info_from_grp_id_ncbicxx_3_6_5
#define mbedtls_ecp_curve_info_from_name \
        mbedtls_ecp_curve_info_from_name_ncbicxx_3_6_5
#define mbedtls_ecp_curve_info_from_tls_id \
        mbedtls_ecp_curve_info_from_tls_id_ncbicxx_3_6_5
#define mbedtls_ecp_curve_list \
        mbedtls_ecp_curve_list_ncbicxx_3_6_5
#define mbedtls_ecp_export \
        mbedtls_ecp_export_ncbicxx_3_6_5
#define mbedtls_ecp_gen_key \
        mbedtls_ecp_gen_key_ncbicxx_3_6_5
#define mbedtls_ecp_gen_keypair \
        mbedtls_ecp_gen_keypair_ncbicxx_3_6_5
#define mbedtls_ecp_gen_keypair_base \
        mbedtls_ecp_gen_keypair_base_ncbicxx_3_6_5
#define mbedtls_ecp_gen_privkey \
        mbedtls_ecp_gen_privkey_ncbicxx_3_6_5
#define mbedtls_ecp_get_type \
        mbedtls_ecp_get_type_ncbicxx_3_6_5
#define mbedtls_ecp_group_copy \
        mbedtls_ecp_group_copy_ncbicxx_3_6_5
#define mbedtls_ecp_group_free \
        mbedtls_ecp_group_free_ncbicxx_3_6_5
#define mbedtls_ecp_group_init \
        mbedtls_ecp_group_init_ncbicxx_3_6_5
#define mbedtls_ecp_grp_id_list \
        mbedtls_ecp_grp_id_list_ncbicxx_3_6_5
#define mbedtls_ecp_is_zero \
        mbedtls_ecp_is_zero_ncbicxx_3_6_5
#define mbedtls_ecp_keypair_calc_public \
        mbedtls_ecp_keypair_calc_public_ncbicxx_3_6_5
#define mbedtls_ecp_keypair_free \
        mbedtls_ecp_keypair_free_ncbicxx_3_6_5
#define mbedtls_ecp_keypair_get_group_id \
        mbedtls_ecp_keypair_get_group_id_ncbicxx_3_6_5
#define mbedtls_ecp_keypair_init \
        mbedtls_ecp_keypair_init_ncbicxx_3_6_5
#define mbedtls_ecp_mul \
        mbedtls_ecp_mul_ncbicxx_3_6_5
#define mbedtls_ecp_mul_restartable \
        mbedtls_ecp_mul_restartable_ncbicxx_3_6_5
#define mbedtls_ecp_muladd \
        mbedtls_ecp_muladd_ncbicxx_3_6_5
#define mbedtls_ecp_muladd_restartable \
        mbedtls_ecp_muladd_restartable_ncbicxx_3_6_5
#define mbedtls_ecp_point_cmp \
        mbedtls_ecp_point_cmp_ncbicxx_3_6_5
#define mbedtls_ecp_point_free \
        mbedtls_ecp_point_free_ncbicxx_3_6_5
#define mbedtls_ecp_point_init \
        mbedtls_ecp_point_init_ncbicxx_3_6_5
#define mbedtls_ecp_point_read_binary \
        mbedtls_ecp_point_read_binary_ncbicxx_3_6_5
#define mbedtls_ecp_point_read_string \
        mbedtls_ecp_point_read_string_ncbicxx_3_6_5
#define mbedtls_ecp_point_write_binary \
        mbedtls_ecp_point_write_binary_ncbicxx_3_6_5
#define mbedtls_ecp_read_key \
        mbedtls_ecp_read_key_ncbicxx_3_6_5
#define mbedtls_ecp_set_public_key \
        mbedtls_ecp_set_public_key_ncbicxx_3_6_5
#define mbedtls_ecp_set_zero \
        mbedtls_ecp_set_zero_ncbicxx_3_6_5
#define mbedtls_ecp_tls_read_group \
        mbedtls_ecp_tls_read_group_ncbicxx_3_6_5
#define mbedtls_ecp_tls_read_group_id \
        mbedtls_ecp_tls_read_group_id_ncbicxx_3_6_5
#define mbedtls_ecp_tls_read_point \
        mbedtls_ecp_tls_read_point_ncbicxx_3_6_5
#define mbedtls_ecp_tls_write_group \
        mbedtls_ecp_tls_write_group_ncbicxx_3_6_5
#define mbedtls_ecp_tls_write_point \
        mbedtls_ecp_tls_write_point_ncbicxx_3_6_5
#define mbedtls_ecp_write_key \
        mbedtls_ecp_write_key_ncbicxx_3_6_5
#define mbedtls_ecp_write_key_ext \
        mbedtls_ecp_write_key_ext_ncbicxx_3_6_5
#define mbedtls_ecp_write_public_key \
        mbedtls_ecp_write_public_key_ncbicxx_3_6_5
#define mbedtls_ecp_group_load \
        mbedtls_ecp_group_load_ncbicxx_3_6_5
#define mbedtls_entropy_add_source \
        mbedtls_entropy_add_source_ncbicxx_3_6_5
#define mbedtls_entropy_free \
        mbedtls_entropy_free_ncbicxx_3_6_5
#define mbedtls_entropy_func \
        mbedtls_entropy_func_ncbicxx_3_6_5
#define mbedtls_entropy_gather \
        mbedtls_entropy_gather_ncbicxx_3_6_5
#define mbedtls_entropy_init \
        mbedtls_entropy_init_ncbicxx_3_6_5
#define mbedtls_entropy_update_manual \
        mbedtls_entropy_update_manual_ncbicxx_3_6_5
#define mbedtls_entropy_update_seed_file \
        mbedtls_entropy_update_seed_file_ncbicxx_3_6_5
#define mbedtls_entropy_write_seed_file \
        mbedtls_entropy_write_seed_file_ncbicxx_3_6_5
#define mbedtls_platform_entropy_poll \
        mbedtls_platform_entropy_poll_ncbicxx_3_6_5
#define mbedtls_high_level_strerr \
        mbedtls_high_level_strerr_ncbicxx_3_6_5
#define mbedtls_low_level_strerr \
        mbedtls_low_level_strerr_ncbicxx_3_6_5
#define mbedtls_strerror \
        mbedtls_strerror_ncbicxx_3_6_5
#define mbedtls_gcm_auth_decrypt \
        mbedtls_gcm_auth_decrypt_ncbicxx_3_6_5
#define mbedtls_gcm_crypt_and_tag \
        mbedtls_gcm_crypt_and_tag_ncbicxx_3_6_5
#define mbedtls_gcm_finish \
        mbedtls_gcm_finish_ncbicxx_3_6_5
#define mbedtls_gcm_free \
        mbedtls_gcm_free_ncbicxx_3_6_5
#define mbedtls_gcm_init \
        mbedtls_gcm_init_ncbicxx_3_6_5
#define mbedtls_gcm_setkey \
        mbedtls_gcm_setkey_ncbicxx_3_6_5
#define mbedtls_gcm_starts \
        mbedtls_gcm_starts_ncbicxx_3_6_5
#define mbedtls_gcm_update \
        mbedtls_gcm_update_ncbicxx_3_6_5
#define mbedtls_gcm_update_ad \
        mbedtls_gcm_update_ad_ncbicxx_3_6_5
#define mbedtls_hkdf \
        mbedtls_hkdf_ncbicxx_3_6_5
#define mbedtls_hkdf_expand \
        mbedtls_hkdf_expand_ncbicxx_3_6_5
#define mbedtls_hkdf_extract \
        mbedtls_hkdf_extract_ncbicxx_3_6_5
#define mbedtls_hmac_drbg_free \
        mbedtls_hmac_drbg_free_ncbicxx_3_6_5
#define mbedtls_hmac_drbg_init \
        mbedtls_hmac_drbg_init_ncbicxx_3_6_5
#define mbedtls_hmac_drbg_random \
        mbedtls_hmac_drbg_random_ncbicxx_3_6_5
#define mbedtls_hmac_drbg_random_with_add \
        mbedtls_hmac_drbg_random_with_add_ncbicxx_3_6_5
#define mbedtls_hmac_drbg_reseed \
        mbedtls_hmac_drbg_reseed_ncbicxx_3_6_5
#define mbedtls_hmac_drbg_seed \
        mbedtls_hmac_drbg_seed_ncbicxx_3_6_5
#define mbedtls_hmac_drbg_seed_buf \
        mbedtls_hmac_drbg_seed_buf_ncbicxx_3_6_5
#define mbedtls_hmac_drbg_set_entropy_len \
        mbedtls_hmac_drbg_set_entropy_len_ncbicxx_3_6_5
#define mbedtls_hmac_drbg_set_prediction_resistance \
        mbedtls_hmac_drbg_set_prediction_resistance_ncbicxx_3_6_5
#define mbedtls_hmac_drbg_set_reseed_interval \
        mbedtls_hmac_drbg_set_reseed_interval_ncbicxx_3_6_5
#define mbedtls_hmac_drbg_update \
        mbedtls_hmac_drbg_update_ncbicxx_3_6_5
#define mbedtls_hmac_drbg_update_seed_file \
        mbedtls_hmac_drbg_update_seed_file_ncbicxx_3_6_5
#define mbedtls_hmac_drbg_write_seed_file \
        mbedtls_hmac_drbg_write_seed_file_ncbicxx_3_6_5
#define mbedtls_lmots_calculate_public_key_candidate \
        mbedtls_lmots_calculate_public_key_candidate_ncbicxx_3_6_5
#define mbedtls_lmots_export_public_key \
        mbedtls_lmots_export_public_key_ncbicxx_3_6_5
#define mbedtls_lmots_import_public_key \
        mbedtls_lmots_import_public_key_ncbicxx_3_6_5
#define mbedtls_lmots_public_free \
        mbedtls_lmots_public_free_ncbicxx_3_6_5
#define mbedtls_lmots_public_init \
        mbedtls_lmots_public_init_ncbicxx_3_6_5
#define mbedtls_lmots_verify \
        mbedtls_lmots_verify_ncbicxx_3_6_5
#define mbedtls_lms_error_from_psa \
        mbedtls_lms_error_from_psa_ncbicxx_3_6_5
#define mbedtls_lms_export_public_key \
        mbedtls_lms_export_public_key_ncbicxx_3_6_5
#define mbedtls_lms_import_public_key \
        mbedtls_lms_import_public_key_ncbicxx_3_6_5
#define mbedtls_lms_public_free \
        mbedtls_lms_public_free_ncbicxx_3_6_5
#define mbedtls_lms_public_init \
        mbedtls_lms_public_init_ncbicxx_3_6_5
#define mbedtls_lms_verify \
        mbedtls_lms_verify_ncbicxx_3_6_5
#define mbedtls_md \
        mbedtls_md_ncbicxx_3_6_5
#define mbedtls_md_clone \
        mbedtls_md_clone_ncbicxx_3_6_5
#define mbedtls_md_error_from_psa \
        mbedtls_md_error_from_psa_ncbicxx_3_6_5
#define mbedtls_md_file \
        mbedtls_md_file_ncbicxx_3_6_5
#define mbedtls_md_finish \
        mbedtls_md_finish_ncbicxx_3_6_5
#define mbedtls_md_free \
        mbedtls_md_free_ncbicxx_3_6_5
#define mbedtls_md_get_name \
        mbedtls_md_get_name_ncbicxx_3_6_5
#define mbedtls_md_get_size \
        mbedtls_md_get_size_ncbicxx_3_6_5
#define mbedtls_md_get_type \
        mbedtls_md_get_type_ncbicxx_3_6_5
#define mbedtls_md_hmac \
        mbedtls_md_hmac_ncbicxx_3_6_5
#define mbedtls_md_hmac_finish \
        mbedtls_md_hmac_finish_ncbicxx_3_6_5
#define mbedtls_md_hmac_reset \
        mbedtls_md_hmac_reset_ncbicxx_3_6_5
#define mbedtls_md_hmac_starts \
        mbedtls_md_hmac_starts_ncbicxx_3_6_5
#define mbedtls_md_hmac_update \
        mbedtls_md_hmac_update_ncbicxx_3_6_5
#define mbedtls_md_info_from_ctx \
        mbedtls_md_info_from_ctx_ncbicxx_3_6_5
#define mbedtls_md_info_from_string \
        mbedtls_md_info_from_string_ncbicxx_3_6_5
#define mbedtls_md_info_from_type \
        mbedtls_md_info_from_type_ncbicxx_3_6_5
#define mbedtls_md_init \
        mbedtls_md_init_ncbicxx_3_6_5
#define mbedtls_md_list \
        mbedtls_md_list_ncbicxx_3_6_5
#define mbedtls_md_setup \
        mbedtls_md_setup_ncbicxx_3_6_5
#define mbedtls_md_starts \
        mbedtls_md_starts_ncbicxx_3_6_5
#define mbedtls_md_update \
        mbedtls_md_update_ncbicxx_3_6_5
#define mbedtls_internal_md5_process \
        mbedtls_internal_md5_process_ncbicxx_3_6_5
#define mbedtls_md5 \
        mbedtls_md5_ncbicxx_3_6_5
#define mbedtls_md5_clone \
        mbedtls_md5_clone_ncbicxx_3_6_5
#define mbedtls_md5_finish \
        mbedtls_md5_finish_ncbicxx_3_6_5
#define mbedtls_md5_free \
        mbedtls_md5_free_ncbicxx_3_6_5
#define mbedtls_md5_init \
        mbedtls_md5_init_ncbicxx_3_6_5
#define mbedtls_md5_starts \
        mbedtls_md5_starts_ncbicxx_3_6_5
#define mbedtls_md5_update \
        mbedtls_md5_update_ncbicxx_3_6_5
#define mbedtls_mps_reader_commit \
        mbedtls_mps_reader_commit_ncbicxx_3_6_5
#define mbedtls_mps_reader_feed \
        mbedtls_mps_reader_feed_ncbicxx_3_6_5
#define mbedtls_mps_reader_free \
        mbedtls_mps_reader_free_ncbicxx_3_6_5
#define mbedtls_mps_reader_get \
        mbedtls_mps_reader_get_ncbicxx_3_6_5
#define mbedtls_mps_reader_init \
        mbedtls_mps_reader_init_ncbicxx_3_6_5
#define mbedtls_mps_reader_reclaim \
        mbedtls_mps_reader_reclaim_ncbicxx_3_6_5
#define mbedtls_net_accept \
        mbedtls_net_accept_ncbicxx_3_6_5
#define mbedtls_net_bind \
        mbedtls_net_bind_ncbicxx_3_6_5
#define mbedtls_net_close \
        mbedtls_net_close_ncbicxx_3_6_5
#define mbedtls_net_connect \
        mbedtls_net_connect_ncbicxx_3_6_5
#define mbedtls_net_free \
        mbedtls_net_free_ncbicxx_3_6_5
#define mbedtls_net_init \
        mbedtls_net_init_ncbicxx_3_6_5
#define mbedtls_net_poll \
        mbedtls_net_poll_ncbicxx_3_6_5
#define mbedtls_net_recv \
        mbedtls_net_recv_ncbicxx_3_6_5
#define mbedtls_net_recv_timeout \
        mbedtls_net_recv_timeout_ncbicxx_3_6_5
#define mbedtls_net_send \
        mbedtls_net_send_ncbicxx_3_6_5
#define mbedtls_net_set_block \
        mbedtls_net_set_block_ncbicxx_3_6_5
#define mbedtls_net_set_nonblock \
        mbedtls_net_set_nonblock_ncbicxx_3_6_5
#define mbedtls_net_usleep \
        mbedtls_net_usleep_ncbicxx_3_6_5
#define mbedtls_nist_kw_free \
        mbedtls_nist_kw_free_ncbicxx_3_6_5
#define mbedtls_nist_kw_init \
        mbedtls_nist_kw_init_ncbicxx_3_6_5
#define mbedtls_nist_kw_setkey \
        mbedtls_nist_kw_setkey_ncbicxx_3_6_5
#define mbedtls_nist_kw_unwrap \
        mbedtls_nist_kw_unwrap_ncbicxx_3_6_5
#define mbedtls_nist_kw_wrap \
        mbedtls_nist_kw_wrap_ncbicxx_3_6_5
#define mbedtls_oid_from_numeric_string \
        mbedtls_oid_from_numeric_string_ncbicxx_3_6_5
#define mbedtls_oid_get_attr_short_name \
        mbedtls_oid_get_attr_short_name_ncbicxx_3_6_5
#define mbedtls_oid_get_certificate_policies \
        mbedtls_oid_get_certificate_policies_ncbicxx_3_6_5
#define mbedtls_oid_get_cipher_alg \
        mbedtls_oid_get_cipher_alg_ncbicxx_3_6_5
#define mbedtls_oid_get_ec_grp \
        mbedtls_oid_get_ec_grp_ncbicxx_3_6_5
#define mbedtls_oid_get_ec_grp_algid \
        mbedtls_oid_get_ec_grp_algid_ncbicxx_3_6_5
#define mbedtls_oid_get_extended_key_usage \
        mbedtls_oid_get_extended_key_usage_ncbicxx_3_6_5
#define mbedtls_oid_get_md_alg \
        mbedtls_oid_get_md_alg_ncbicxx_3_6_5
#define mbedtls_oid_get_md_hmac \
        mbedtls_oid_get_md_hmac_ncbicxx_3_6_5
#define mbedtls_oid_get_numeric_string \
        mbedtls_oid_get_numeric_string_ncbicxx_3_6_5
#define mbedtls_oid_get_oid_by_ec_grp \
        mbedtls_oid_get_oid_by_ec_grp_ncbicxx_3_6_5
#define mbedtls_oid_get_oid_by_ec_grp_algid \
        mbedtls_oid_get_oid_by_ec_grp_algid_ncbicxx_3_6_5
#define mbedtls_oid_get_oid_by_md \
        mbedtls_oid_get_oid_by_md_ncbicxx_3_6_5
#define mbedtls_oid_get_oid_by_pk_alg \
        mbedtls_oid_get_oid_by_pk_alg_ncbicxx_3_6_5
#define mbedtls_oid_get_oid_by_sig_alg \
        mbedtls_oid_get_oid_by_sig_alg_ncbicxx_3_6_5
#define mbedtls_oid_get_pk_alg \
        mbedtls_oid_get_pk_alg_ncbicxx_3_6_5
#define mbedtls_oid_get_pkcs12_pbe_alg \
        mbedtls_oid_get_pkcs12_pbe_alg_ncbicxx_3_6_5
#define mbedtls_oid_get_sig_alg \
        mbedtls_oid_get_sig_alg_ncbicxx_3_6_5
#define mbedtls_oid_get_sig_alg_desc \
        mbedtls_oid_get_sig_alg_desc_ncbicxx_3_6_5
#define mbedtls_oid_get_x509_ext_type \
        mbedtls_oid_get_x509_ext_type_ncbicxx_3_6_5
#define mbedtls_pem_free \
        mbedtls_pem_free_ncbicxx_3_6_5
#define mbedtls_pem_init \
        mbedtls_pem_init_ncbicxx_3_6_5
#define mbedtls_pem_read_buffer \
        mbedtls_pem_read_buffer_ncbicxx_3_6_5
#define mbedtls_pem_write_buffer \
        mbedtls_pem_write_buffer_ncbicxx_3_6_5
#define mbedtls_pk_can_do \
        mbedtls_pk_can_do_ncbicxx_3_6_5
#define mbedtls_pk_check_pair \
        mbedtls_pk_check_pair_ncbicxx_3_6_5
#define mbedtls_pk_copy_from_psa \
        mbedtls_pk_copy_from_psa_ncbicxx_3_6_5
#define mbedtls_pk_copy_public_from_psa \
        mbedtls_pk_copy_public_from_psa_ncbicxx_3_6_5
#define mbedtls_pk_debug \
        mbedtls_pk_debug_ncbicxx_3_6_5
#define mbedtls_pk_decrypt \
        mbedtls_pk_decrypt_ncbicxx_3_6_5
#define mbedtls_pk_encrypt \
        mbedtls_pk_encrypt_ncbicxx_3_6_5
#define mbedtls_pk_free \
        mbedtls_pk_free_ncbicxx_3_6_5
#define mbedtls_pk_get_bitlen \
        mbedtls_pk_get_bitlen_ncbicxx_3_6_5
#define mbedtls_pk_get_name \
        mbedtls_pk_get_name_ncbicxx_3_6_5
#define mbedtls_pk_get_psa_attributes \
        mbedtls_pk_get_psa_attributes_ncbicxx_3_6_5
#define mbedtls_pk_get_type \
        mbedtls_pk_get_type_ncbicxx_3_6_5
#define mbedtls_pk_import_into_psa \
        mbedtls_pk_import_into_psa_ncbicxx_3_6_5
#define mbedtls_pk_info_from_type \
        mbedtls_pk_info_from_type_ncbicxx_3_6_5
#define mbedtls_pk_init \
        mbedtls_pk_init_ncbicxx_3_6_5
#define mbedtls_pk_setup \
        mbedtls_pk_setup_ncbicxx_3_6_5
#define mbedtls_pk_setup_rsa_alt \
        mbedtls_pk_setup_rsa_alt_ncbicxx_3_6_5
#define mbedtls_pk_sign \
        mbedtls_pk_sign_ncbicxx_3_6_5
#define mbedtls_pk_sign_ext \
        mbedtls_pk_sign_ext_ncbicxx_3_6_5
#define mbedtls_pk_sign_restartable \
        mbedtls_pk_sign_restartable_ncbicxx_3_6_5
#define mbedtls_pk_verify \
        mbedtls_pk_verify_ncbicxx_3_6_5
#define mbedtls_pk_verify_ext \
        mbedtls_pk_verify_ext_ncbicxx_3_6_5
#define mbedtls_pk_verify_restartable \
        mbedtls_pk_verify_restartable_ncbicxx_3_6_5
#define mbedtls_pk_ecc_set_group \
        mbedtls_pk_ecc_set_group_ncbicxx_3_6_5
#define mbedtls_pk_ecc_set_key \
        mbedtls_pk_ecc_set_key_ncbicxx_3_6_5
#define mbedtls_pk_ecc_set_pubkey \
        mbedtls_pk_ecc_set_pubkey_ncbicxx_3_6_5
#define mbedtls_pk_ecc_set_pubkey_from_prv \
        mbedtls_pk_ecc_set_pubkey_from_prv_ncbicxx_3_6_5
#define mbedtls_ecdsa_info \
        mbedtls_ecdsa_info_ncbicxx_3_6_5
#define mbedtls_eckey_info \
        mbedtls_eckey_info_ncbicxx_3_6_5
#define mbedtls_eckeydh_info \
        mbedtls_eckeydh_info_ncbicxx_3_6_5
#define mbedtls_rsa_alt_info \
        mbedtls_rsa_alt_info_ncbicxx_3_6_5
#define mbedtls_rsa_info \
        mbedtls_rsa_info_ncbicxx_3_6_5
#define mbedtls_pkcs12_derivation \
        mbedtls_pkcs12_derivation_ncbicxx_3_6_5
#define mbedtls_pkcs12_pbe \
        mbedtls_pkcs12_pbe_ncbicxx_3_6_5
#define mbedtls_pkcs12_pbe_ext \
        mbedtls_pkcs12_pbe_ext_ncbicxx_3_6_5
#define mbedtls_pkcs5_pbes2 \
        mbedtls_pkcs5_pbes2_ncbicxx_3_6_5
#define mbedtls_pkcs5_pbes2_ext \
        mbedtls_pkcs5_pbes2_ext_ncbicxx_3_6_5
#define mbedtls_pkcs5_pbkdf2_hmac \
        mbedtls_pkcs5_pbkdf2_hmac_ncbicxx_3_6_5
#define mbedtls_pkcs5_pbkdf2_hmac_ext \
        mbedtls_pkcs5_pbkdf2_hmac_ext_ncbicxx_3_6_5
#define mbedtls_pkcs7_free \
        mbedtls_pkcs7_free_ncbicxx_3_6_5
#define mbedtls_pkcs7_init \
        mbedtls_pkcs7_init_ncbicxx_3_6_5
#define mbedtls_pkcs7_parse_der \
        mbedtls_pkcs7_parse_der_ncbicxx_3_6_5
#define mbedtls_pkcs7_signed_data_verify \
        mbedtls_pkcs7_signed_data_verify_ncbicxx_3_6_5
#define mbedtls_pkcs7_signed_hash_verify \
        mbedtls_pkcs7_signed_hash_verify_ncbicxx_3_6_5
#define mbedtls_pk_load_file \
        mbedtls_pk_load_file_ncbicxx_3_6_5
#define mbedtls_pk_parse_key \
        mbedtls_pk_parse_key_ncbicxx_3_6_5
#define mbedtls_pk_parse_keyfile \
        mbedtls_pk_parse_keyfile_ncbicxx_3_6_5
#define mbedtls_pk_parse_public_key \
        mbedtls_pk_parse_public_key_ncbicxx_3_6_5
#define mbedtls_pk_parse_public_keyfile \
        mbedtls_pk_parse_public_keyfile_ncbicxx_3_6_5
#define mbedtls_pk_parse_subpubkey \
        mbedtls_pk_parse_subpubkey_ncbicxx_3_6_5
#define mbedtls_pk_write_key_der \
        mbedtls_pk_write_key_der_ncbicxx_3_6_5
#define mbedtls_pk_write_key_pem \
        mbedtls_pk_write_key_pem_ncbicxx_3_6_5
#define mbedtls_pk_write_pubkey \
        mbedtls_pk_write_pubkey_ncbicxx_3_6_5
#define mbedtls_pk_write_pubkey_der \
        mbedtls_pk_write_pubkey_der_ncbicxx_3_6_5
#define mbedtls_pk_write_pubkey_pem \
        mbedtls_pk_write_pubkey_pem_ncbicxx_3_6_5
#define mbedtls_platform_setup \
        mbedtls_platform_setup_ncbicxx_3_6_5
#define mbedtls_platform_teardown \
        mbedtls_platform_teardown_ncbicxx_3_6_5
#define mbedtls_ms_time \
        mbedtls_ms_time_ncbicxx_3_6_5
#define mbedtls_platform_gmtime_r \
        mbedtls_platform_gmtime_r_ncbicxx_3_6_5
#define mbedtls_platform_zeroize \
        mbedtls_platform_zeroize_ncbicxx_3_6_5
#define mbedtls_zeroize_and_free \
        mbedtls_zeroize_and_free_ncbicxx_3_6_5
#define mbedtls_poly1305_finish \
        mbedtls_poly1305_finish_ncbicxx_3_6_5
#define mbedtls_poly1305_free \
        mbedtls_poly1305_free_ncbicxx_3_6_5
#define mbedtls_poly1305_init \
        mbedtls_poly1305_init_ncbicxx_3_6_5
#define mbedtls_poly1305_mac \
        mbedtls_poly1305_mac_ncbicxx_3_6_5
#define mbedtls_poly1305_starts \
        mbedtls_poly1305_starts_ncbicxx_3_6_5
#define mbedtls_poly1305_update \
        mbedtls_poly1305_update_ncbicxx_3_6_5
#define mbedtls_psa_crypto_configure_entropy_sources \
        mbedtls_psa_crypto_configure_entropy_sources_ncbicxx_3_6_5
#define mbedtls_psa_crypto_free \
        mbedtls_psa_crypto_free_ncbicxx_3_6_5
#define mbedtls_psa_interruptible_set_max_ops \
        mbedtls_psa_interruptible_set_max_ops_ncbicxx_3_6_5
#define mbedtls_psa_sign_hash_abort \
        mbedtls_psa_sign_hash_abort_ncbicxx_3_6_5
#define mbedtls_psa_sign_hash_complete \
        mbedtls_psa_sign_hash_complete_ncbicxx_3_6_5
#define mbedtls_psa_sign_hash_get_num_ops \
        mbedtls_psa_sign_hash_get_num_ops_ncbicxx_3_6_5
#define mbedtls_psa_sign_hash_start \
        mbedtls_psa_sign_hash_start_ncbicxx_3_6_5
#define mbedtls_psa_verify_hash_abort \
        mbedtls_psa_verify_hash_abort_ncbicxx_3_6_5
#define mbedtls_psa_verify_hash_complete \
        mbedtls_psa_verify_hash_complete_ncbicxx_3_6_5
#define mbedtls_psa_verify_hash_get_num_ops \
        mbedtls_psa_verify_hash_get_num_ops_ncbicxx_3_6_5
#define mbedtls_psa_verify_hash_start \
        mbedtls_psa_verify_hash_start_ncbicxx_3_6_5
#define mbedtls_to_psa_error \
        mbedtls_to_psa_error_ncbicxx_3_6_5
#define psa_aead_abort \
        psa_aead_abort_ncbicxx_3_6_5
#define psa_aead_decrypt \
        psa_aead_decrypt_ncbicxx_3_6_5
#define psa_aead_decrypt_setup \
        psa_aead_decrypt_setup_ncbicxx_3_6_5
#define psa_aead_encrypt \
        psa_aead_encrypt_ncbicxx_3_6_5
#define psa_aead_encrypt_setup \
        psa_aead_encrypt_setup_ncbicxx_3_6_5
#define psa_aead_finish \
        psa_aead_finish_ncbicxx_3_6_5
#define psa_aead_generate_nonce \
        psa_aead_generate_nonce_ncbicxx_3_6_5
#define psa_aead_set_lengths \
        psa_aead_set_lengths_ncbicxx_3_6_5
#define psa_aead_set_nonce \
        psa_aead_set_nonce_ncbicxx_3_6_5
#define psa_aead_update \
        psa_aead_update_ncbicxx_3_6_5
#define psa_aead_update_ad \
        psa_aead_update_ad_ncbicxx_3_6_5
#define psa_aead_verify \
        psa_aead_verify_ncbicxx_3_6_5
#define psa_allocate_buffer_to_slot \
        psa_allocate_buffer_to_slot_ncbicxx_3_6_5
#define psa_asymmetric_decrypt \
        psa_asymmetric_decrypt_ncbicxx_3_6_5
#define psa_asymmetric_encrypt \
        psa_asymmetric_encrypt_ncbicxx_3_6_5
#define psa_can_do_cipher \
        psa_can_do_cipher_ncbicxx_3_6_5
#define psa_can_do_hash \
        psa_can_do_hash_ncbicxx_3_6_5
#define psa_cipher_abort \
        psa_cipher_abort_ncbicxx_3_6_5
#define psa_cipher_decrypt \
        psa_cipher_decrypt_ncbicxx_3_6_5
#define psa_cipher_decrypt_setup \
        psa_cipher_decrypt_setup_ncbicxx_3_6_5
#define psa_cipher_encrypt \
        psa_cipher_encrypt_ncbicxx_3_6_5
#define psa_cipher_encrypt_setup \
        psa_cipher_encrypt_setup_ncbicxx_3_6_5
#define psa_cipher_finish \
        psa_cipher_finish_ncbicxx_3_6_5
#define psa_cipher_generate_iv \
        psa_cipher_generate_iv_ncbicxx_3_6_5
#define psa_cipher_set_iv \
        psa_cipher_set_iv_ncbicxx_3_6_5
#define psa_cipher_update \
        psa_cipher_update_ncbicxx_3_6_5
#define psa_copy_key \
        psa_copy_key_ncbicxx_3_6_5
#define psa_copy_key_material_into_slot \
        psa_copy_key_material_into_slot_ncbicxx_3_6_5
#define psa_crypto_driver_pake_get_cipher_suite \
        psa_crypto_driver_pake_get_cipher_suite_ncbicxx_3_6_5
#define psa_crypto_driver_pake_get_password \
        psa_crypto_driver_pake_get_password_ncbicxx_3_6_5
#define psa_crypto_driver_pake_get_password_len \
        psa_crypto_driver_pake_get_password_len_ncbicxx_3_6_5
#define psa_crypto_driver_pake_get_peer \
        psa_crypto_driver_pake_get_peer_ncbicxx_3_6_5
#define psa_crypto_driver_pake_get_peer_len \
        psa_crypto_driver_pake_get_peer_len_ncbicxx_3_6_5
#define psa_crypto_driver_pake_get_user \
        psa_crypto_driver_pake_get_user_ncbicxx_3_6_5
#define psa_crypto_driver_pake_get_user_len \
        psa_crypto_driver_pake_get_user_len_ncbicxx_3_6_5
#define psa_crypto_init \
        psa_crypto_init_ncbicxx_3_6_5
#define psa_crypto_local_input_alloc \
        psa_crypto_local_input_alloc_ncbicxx_3_6_5
#define psa_crypto_local_input_free \
        psa_crypto_local_input_free_ncbicxx_3_6_5
#define psa_crypto_local_output_alloc \
        psa_crypto_local_output_alloc_ncbicxx_3_6_5
#define psa_crypto_local_output_free \
        psa_crypto_local_output_free_ncbicxx_3_6_5
#define psa_custom_key_parameters_are_default \
        psa_custom_key_parameters_are_default_ncbicxx_3_6_5
#define psa_destroy_key \
        psa_destroy_key_ncbicxx_3_6_5
#define psa_export_key \
        psa_export_key_ncbicxx_3_6_5
#define psa_export_key_internal \
        psa_export_key_internal_ncbicxx_3_6_5
#define psa_export_public_key \
        psa_export_public_key_ncbicxx_3_6_5
#define psa_export_public_key_internal \
        psa_export_public_key_internal_ncbicxx_3_6_5
#define psa_generate_key \
        psa_generate_key_ncbicxx_3_6_5
#define psa_generate_key_custom \
        psa_generate_key_custom_ncbicxx_3_6_5
#define psa_generate_key_ext \
        psa_generate_key_ext_ncbicxx_3_6_5
#define psa_generate_key_internal \
        psa_generate_key_internal_ncbicxx_3_6_5
#define psa_generate_random \
        psa_generate_random_ncbicxx_3_6_5
#define psa_get_key_attributes \
        psa_get_key_attributes_ncbicxx_3_6_5
#define psa_hash_abort \
        psa_hash_abort_ncbicxx_3_6_5
#define psa_hash_clone \
        psa_hash_clone_ncbicxx_3_6_5
#define psa_hash_compare \
        psa_hash_compare_ncbicxx_3_6_5
#define psa_hash_compute \
        psa_hash_compute_ncbicxx_3_6_5
#define psa_hash_finish \
        psa_hash_finish_ncbicxx_3_6_5
#define psa_hash_setup \
        psa_hash_setup_ncbicxx_3_6_5
#define psa_hash_update \
        psa_hash_update_ncbicxx_3_6_5
#define psa_hash_verify \
        psa_hash_verify_ncbicxx_3_6_5
#define psa_import_key \
        psa_import_key_ncbicxx_3_6_5
#define psa_import_key_into_slot \
        psa_import_key_into_slot_ncbicxx_3_6_5
#define psa_interruptible_get_max_ops \
        psa_interruptible_get_max_ops_ncbicxx_3_6_5
#define psa_interruptible_set_max_ops \
        psa_interruptible_set_max_ops_ncbicxx_3_6_5
#define psa_key_agreement_raw_builtin \
        psa_key_agreement_raw_builtin_ncbicxx_3_6_5
#define psa_key_derivation_abort \
        psa_key_derivation_abort_ncbicxx_3_6_5
#define psa_key_derivation_get_capacity \
        psa_key_derivation_get_capacity_ncbicxx_3_6_5
#define psa_key_derivation_input_bytes \
        psa_key_derivation_input_bytes_ncbicxx_3_6_5
#define psa_key_derivation_input_integer \
        psa_key_derivation_input_integer_ncbicxx_3_6_5
#define psa_key_derivation_input_key \
        psa_key_derivation_input_key_ncbicxx_3_6_5
#define psa_key_derivation_key_agreement \
        psa_key_derivation_key_agreement_ncbicxx_3_6_5
#define psa_key_derivation_output_bytes \
        psa_key_derivation_output_bytes_ncbicxx_3_6_5
#define psa_key_derivation_output_key \
        psa_key_derivation_output_key_ncbicxx_3_6_5
#define psa_key_derivation_output_key_custom \
        psa_key_derivation_output_key_custom_ncbicxx_3_6_5
#define psa_key_derivation_output_key_ext \
        psa_key_derivation_output_key_ext_ncbicxx_3_6_5
#define psa_key_derivation_set_capacity \
        psa_key_derivation_set_capacity_ncbicxx_3_6_5
#define psa_key_derivation_setup \
        psa_key_derivation_setup_ncbicxx_3_6_5
#define psa_mac_abort \
        psa_mac_abort_ncbicxx_3_6_5
#define psa_mac_compute \
        psa_mac_compute_ncbicxx_3_6_5
#define psa_mac_sign_finish \
        psa_mac_sign_finish_ncbicxx_3_6_5
#define psa_mac_sign_setup \
        psa_mac_sign_setup_ncbicxx_3_6_5
#define psa_mac_update \
        psa_mac_update_ncbicxx_3_6_5
#define psa_mac_verify \
        psa_mac_verify_ncbicxx_3_6_5
#define psa_mac_verify_finish \
        psa_mac_verify_finish_ncbicxx_3_6_5
#define psa_mac_verify_setup \
        psa_mac_verify_setup_ncbicxx_3_6_5
#define psa_pake_abort \
        psa_pake_abort_ncbicxx_3_6_5
#define psa_pake_get_implicit_key \
        psa_pake_get_implicit_key_ncbicxx_3_6_5
#define psa_pake_input \
        psa_pake_input_ncbicxx_3_6_5
#define psa_pake_output \
        psa_pake_output_ncbicxx_3_6_5
#define psa_pake_set_password_key \
        psa_pake_set_password_key_ncbicxx_3_6_5
#define psa_pake_set_peer \
        psa_pake_set_peer_ncbicxx_3_6_5
#define psa_pake_set_role \
        psa_pake_set_role_ncbicxx_3_6_5
#define psa_pake_set_user \
        psa_pake_set_user_ncbicxx_3_6_5
#define psa_pake_setup \
        psa_pake_setup_ncbicxx_3_6_5
#define psa_raw_key_agreement \
        psa_raw_key_agreement_ncbicxx_3_6_5
#define psa_remove_key_data_from_memory \
        psa_remove_key_data_from_memory_ncbicxx_3_6_5
#define psa_sign_hash \
        psa_sign_hash_ncbicxx_3_6_5
#define psa_sign_hash_abort \
        psa_sign_hash_abort_ncbicxx_3_6_5
#define psa_sign_hash_builtin \
        psa_sign_hash_builtin_ncbicxx_3_6_5
#define psa_sign_hash_complete \
        psa_sign_hash_complete_ncbicxx_3_6_5
#define psa_sign_hash_get_num_ops \
        psa_sign_hash_get_num_ops_ncbicxx_3_6_5
#define psa_sign_hash_start \
        psa_sign_hash_start_ncbicxx_3_6_5
#define psa_sign_message \
        psa_sign_message_ncbicxx_3_6_5
#define psa_sign_message_builtin \
        psa_sign_message_builtin_ncbicxx_3_6_5
#define psa_validate_unstructured_key_bit_size \
        psa_validate_unstructured_key_bit_size_ncbicxx_3_6_5
#define psa_verify_hash \
        psa_verify_hash_ncbicxx_3_6_5
#define psa_verify_hash_abort \
        psa_verify_hash_abort_ncbicxx_3_6_5
#define psa_verify_hash_builtin \
        psa_verify_hash_builtin_ncbicxx_3_6_5
#define psa_verify_hash_complete \
        psa_verify_hash_complete_ncbicxx_3_6_5
#define psa_verify_hash_get_num_ops \
        psa_verify_hash_get_num_ops_ncbicxx_3_6_5
#define psa_verify_hash_start \
        psa_verify_hash_start_ncbicxx_3_6_5
#define psa_verify_message \
        psa_verify_message_ncbicxx_3_6_5
#define psa_verify_message_builtin \
        psa_verify_message_builtin_ncbicxx_3_6_5
#define psa_wipe_key_slot \
        psa_wipe_key_slot_ncbicxx_3_6_5
#define mbedtls_psa_aead_abort \
        mbedtls_psa_aead_abort_ncbicxx_3_6_5
#define mbedtls_psa_aead_decrypt \
        mbedtls_psa_aead_decrypt_ncbicxx_3_6_5
#define mbedtls_psa_aead_decrypt_setup \
        mbedtls_psa_aead_decrypt_setup_ncbicxx_3_6_5
#define mbedtls_psa_aead_encrypt \
        mbedtls_psa_aead_encrypt_ncbicxx_3_6_5
#define mbedtls_psa_aead_encrypt_setup \
        mbedtls_psa_aead_encrypt_setup_ncbicxx_3_6_5
#define mbedtls_psa_aead_finish \
        mbedtls_psa_aead_finish_ncbicxx_3_6_5
#define mbedtls_psa_aead_set_lengths \
        mbedtls_psa_aead_set_lengths_ncbicxx_3_6_5
#define mbedtls_psa_aead_set_nonce \
        mbedtls_psa_aead_set_nonce_ncbicxx_3_6_5
#define mbedtls_psa_aead_update \
        mbedtls_psa_aead_update_ncbicxx_3_6_5
#define mbedtls_psa_aead_update_ad \
        mbedtls_psa_aead_update_ad_ncbicxx_3_6_5
#define mbedtls_cipher_info_from_psa \
        mbedtls_cipher_info_from_psa_ncbicxx_3_6_5
#define mbedtls_cipher_values_from_psa \
        mbedtls_cipher_values_from_psa_ncbicxx_3_6_5
#define mbedtls_psa_cipher_abort \
        mbedtls_psa_cipher_abort_ncbicxx_3_6_5
#define mbedtls_psa_cipher_decrypt \
        mbedtls_psa_cipher_decrypt_ncbicxx_3_6_5
#define mbedtls_psa_cipher_decrypt_setup \
        mbedtls_psa_cipher_decrypt_setup_ncbicxx_3_6_5
#define mbedtls_psa_cipher_encrypt \
        mbedtls_psa_cipher_encrypt_ncbicxx_3_6_5
#define mbedtls_psa_cipher_encrypt_setup \
        mbedtls_psa_cipher_encrypt_setup_ncbicxx_3_6_5
#define mbedtls_psa_cipher_finish \
        mbedtls_psa_cipher_finish_ncbicxx_3_6_5
#define mbedtls_psa_cipher_set_iv \
        mbedtls_psa_cipher_set_iv_ncbicxx_3_6_5
#define mbedtls_psa_cipher_update \
        mbedtls_psa_cipher_update_ncbicxx_3_6_5
#define psa_reset_key_attributes \
        psa_reset_key_attributes_ncbicxx_3_6_5
#define psa_driver_wrapper_export_public_key \
        psa_driver_wrapper_export_public_key_ncbicxx_3_6_5
#define psa_driver_wrapper_get_builtin_key \
        psa_driver_wrapper_get_builtin_key_ncbicxx_3_6_5
#define psa_driver_wrapper_get_key_buffer_size \
        psa_driver_wrapper_get_key_buffer_size_ncbicxx_3_6_5
#define mbedtls_psa_ecdsa_sign_hash \
        mbedtls_psa_ecdsa_sign_hash_ncbicxx_3_6_5
#define mbedtls_psa_ecdsa_verify_hash \
        mbedtls_psa_ecdsa_verify_hash_ncbicxx_3_6_5
#define mbedtls_psa_ecp_export_key \
        mbedtls_psa_ecp_export_key_ncbicxx_3_6_5
#define mbedtls_psa_ecp_export_public_key \
        mbedtls_psa_ecp_export_public_key_ncbicxx_3_6_5
#define mbedtls_psa_ecp_generate_key \
        mbedtls_psa_ecp_generate_key_ncbicxx_3_6_5
#define mbedtls_psa_ecp_import_key \
        mbedtls_psa_ecp_import_key_ncbicxx_3_6_5
#define mbedtls_psa_ecp_load_public_part \
        mbedtls_psa_ecp_load_public_part_ncbicxx_3_6_5
#define mbedtls_psa_ecp_load_representation \
        mbedtls_psa_ecp_load_representation_ncbicxx_3_6_5
#define mbedtls_psa_key_agreement_ecdh \
        mbedtls_psa_key_agreement_ecdh_ncbicxx_3_6_5
#define mbedtls_psa_ffdh_export_public_key \
        mbedtls_psa_ffdh_export_public_key_ncbicxx_3_6_5
#define mbedtls_psa_ffdh_generate_key \
        mbedtls_psa_ffdh_generate_key_ncbicxx_3_6_5
#define mbedtls_psa_ffdh_import_key \
        mbedtls_psa_ffdh_import_key_ncbicxx_3_6_5
#define mbedtls_psa_ffdh_key_agreement \
        mbedtls_psa_ffdh_key_agreement_ncbicxx_3_6_5
#define mbedtls_psa_hash_abort \
        mbedtls_psa_hash_abort_ncbicxx_3_6_5
#define mbedtls_psa_hash_clone \
        mbedtls_psa_hash_clone_ncbicxx_3_6_5
#define mbedtls_psa_hash_compute \
        mbedtls_psa_hash_compute_ncbicxx_3_6_5
#define mbedtls_psa_hash_finish \
        mbedtls_psa_hash_finish_ncbicxx_3_6_5
#define mbedtls_psa_hash_setup \
        mbedtls_psa_hash_setup_ncbicxx_3_6_5
#define mbedtls_psa_hash_update \
        mbedtls_psa_hash_update_ncbicxx_3_6_5
#define mbedtls_psa_mac_abort \
        mbedtls_psa_mac_abort_ncbicxx_3_6_5
#define mbedtls_psa_mac_compute \
        mbedtls_psa_mac_compute_ncbicxx_3_6_5
#define mbedtls_psa_mac_sign_finish \
        mbedtls_psa_mac_sign_finish_ncbicxx_3_6_5
#define mbedtls_psa_mac_sign_setup \
        mbedtls_psa_mac_sign_setup_ncbicxx_3_6_5
#define mbedtls_psa_mac_update \
        mbedtls_psa_mac_update_ncbicxx_3_6_5
#define mbedtls_psa_mac_verify_finish \
        mbedtls_psa_mac_verify_finish_ncbicxx_3_6_5
#define mbedtls_psa_mac_verify_setup \
        mbedtls_psa_mac_verify_setup_ncbicxx_3_6_5
#define mbedtls_psa_pake_abort \
        mbedtls_psa_pake_abort_ncbicxx_3_6_5
#define mbedtls_psa_pake_get_implicit_key \
        mbedtls_psa_pake_get_implicit_key_ncbicxx_3_6_5
#define mbedtls_psa_pake_input \
        mbedtls_psa_pake_input_ncbicxx_3_6_5
#define mbedtls_psa_pake_output \
        mbedtls_psa_pake_output_ncbicxx_3_6_5
#define mbedtls_psa_pake_setup \
        mbedtls_psa_pake_setup_ncbicxx_3_6_5
#define mbedtls_psa_asymmetric_decrypt \
        mbedtls_psa_asymmetric_decrypt_ncbicxx_3_6_5
#define mbedtls_psa_asymmetric_encrypt \
        mbedtls_psa_asymmetric_encrypt_ncbicxx_3_6_5
#define mbedtls_psa_rsa_export_key \
        mbedtls_psa_rsa_export_key_ncbicxx_3_6_5
#define mbedtls_psa_rsa_export_public_key \
        mbedtls_psa_rsa_export_public_key_ncbicxx_3_6_5
#define mbedtls_psa_rsa_generate_key \
        mbedtls_psa_rsa_generate_key_ncbicxx_3_6_5
#define mbedtls_psa_rsa_import_key \
        mbedtls_psa_rsa_import_key_ncbicxx_3_6_5
#define mbedtls_psa_rsa_load_representation \
        mbedtls_psa_rsa_load_representation_ncbicxx_3_6_5
#define mbedtls_psa_rsa_sign_hash \
        mbedtls_psa_rsa_sign_hash_ncbicxx_3_6_5
#define mbedtls_psa_rsa_verify_hash \
        mbedtls_psa_rsa_verify_hash_ncbicxx_3_6_5
#define mbedtls_psa_get_stats \
        mbedtls_psa_get_stats_ncbicxx_3_6_5
#define psa_close_key \
        psa_close_key_ncbicxx_3_6_5
#define psa_free_key_slot \
        psa_free_key_slot_ncbicxx_3_6_5
#define psa_get_and_lock_key_slot \
        psa_get_and_lock_key_slot_ncbicxx_3_6_5
#define psa_initialize_key_slots \
        psa_initialize_key_slots_ncbicxx_3_6_5
#define psa_is_valid_key_id \
        psa_is_valid_key_id_ncbicxx_3_6_5
#define psa_open_key \
        psa_open_key_ncbicxx_3_6_5
#define psa_purge_key \
        psa_purge_key_ncbicxx_3_6_5
#define psa_reserve_free_key_slot \
        psa_reserve_free_key_slot_ncbicxx_3_6_5
#define psa_unregister_read \
        psa_unregister_read_ncbicxx_3_6_5
#define psa_unregister_read_under_mutex \
        psa_unregister_read_under_mutex_ncbicxx_3_6_5
#define psa_validate_key_location \
        psa_validate_key_location_ncbicxx_3_6_5
#define psa_validate_key_persistence \
        psa_validate_key_persistence_ncbicxx_3_6_5
#define psa_wipe_all_key_slots \
        psa_wipe_all_key_slots_ncbicxx_3_6_5
#define psa_destroy_persistent_key \
        psa_destroy_persistent_key_ncbicxx_3_6_5
#define psa_format_key_data_for_storage \
        psa_format_key_data_for_storage_ncbicxx_3_6_5
#define psa_free_persistent_key_data \
        psa_free_persistent_key_data_ncbicxx_3_6_5
#define psa_is_key_present_in_storage \
        psa_is_key_present_in_storage_ncbicxx_3_6_5
#define psa_load_persistent_key \
        psa_load_persistent_key_ncbicxx_3_6_5
#define psa_parse_key_data_from_storage \
        psa_parse_key_data_from_storage_ncbicxx_3_6_5
#define psa_save_persistent_key \
        psa_save_persistent_key_ncbicxx_3_6_5
#define psa_its_get \
        psa_its_get_ncbicxx_3_6_5
#define psa_its_get_info \
        psa_its_get_info_ncbicxx_3_6_5
#define psa_its_remove \
        psa_its_remove_ncbicxx_3_6_5
#define psa_its_set \
        psa_its_set_ncbicxx_3_6_5
#define mbedtls_ecc_group_from_psa \
        mbedtls_ecc_group_from_psa_ncbicxx_3_6_5
#define mbedtls_ecc_group_to_psa \
        mbedtls_ecc_group_to_psa_ncbicxx_3_6_5
#define mbedtls_ecdsa_der_to_raw \
        mbedtls_ecdsa_der_to_raw_ncbicxx_3_6_5
#define mbedtls_ecdsa_raw_to_der \
        mbedtls_ecdsa_raw_to_der_ncbicxx_3_6_5
#define mbedtls_psa_get_random \
        mbedtls_psa_get_random_ncbicxx_3_6_5
#define psa_generic_status_to_mbedtls \
        psa_generic_status_to_mbedtls_ncbicxx_3_6_5
#define psa_pk_status_to_mbedtls \
        psa_pk_status_to_mbedtls_ncbicxx_3_6_5
#define psa_status_to_mbedtls \
        psa_status_to_mbedtls_ncbicxx_3_6_5
#define psa_to_lms_errors \
        psa_to_lms_errors_ncbicxx_3_6_5
#define psa_to_md_errors \
        psa_to_md_errors_ncbicxx_3_6_5
#define psa_to_pk_rsa_errors \
        psa_to_pk_rsa_errors_ncbicxx_3_6_5
#define psa_to_ssl_errors \
        psa_to_ssl_errors_ncbicxx_3_6_5
#define mbedtls_internal_ripemd160_process \
        mbedtls_internal_ripemd160_process_ncbicxx_3_6_5
#define mbedtls_ripemd160 \
        mbedtls_ripemd160_ncbicxx_3_6_5
#define mbedtls_ripemd160_clone \
        mbedtls_ripemd160_clone_ncbicxx_3_6_5
#define mbedtls_ripemd160_finish \
        mbedtls_ripemd160_finish_ncbicxx_3_6_5
#define mbedtls_ripemd160_free \
        mbedtls_ripemd160_free_ncbicxx_3_6_5
#define mbedtls_ripemd160_init \
        mbedtls_ripemd160_init_ncbicxx_3_6_5
#define mbedtls_ripemd160_starts \
        mbedtls_ripemd160_starts_ncbicxx_3_6_5
#define mbedtls_ripemd160_update \
        mbedtls_ripemd160_update_ncbicxx_3_6_5
#define mbedtls_rsa_check_privkey \
        mbedtls_rsa_check_privkey_ncbicxx_3_6_5
#define mbedtls_rsa_check_pub_priv \
        mbedtls_rsa_check_pub_priv_ncbicxx_3_6_5
#define mbedtls_rsa_check_pubkey \
        mbedtls_rsa_check_pubkey_ncbicxx_3_6_5
#define mbedtls_rsa_complete \
        mbedtls_rsa_complete_ncbicxx_3_6_5
#define mbedtls_rsa_copy \
        mbedtls_rsa_copy_ncbicxx_3_6_5
#define mbedtls_rsa_export \
        mbedtls_rsa_export_ncbicxx_3_6_5
#define mbedtls_rsa_export_crt \
        mbedtls_rsa_export_crt_ncbicxx_3_6_5
#define mbedtls_rsa_export_raw \
        mbedtls_rsa_export_raw_ncbicxx_3_6_5
#define mbedtls_rsa_free \
        mbedtls_rsa_free_ncbicxx_3_6_5
#define mbedtls_rsa_gen_key \
        mbedtls_rsa_gen_key_ncbicxx_3_6_5
#define mbedtls_rsa_get_bitlen \
        mbedtls_rsa_get_bitlen_ncbicxx_3_6_5
#define mbedtls_rsa_get_len \
        mbedtls_rsa_get_len_ncbicxx_3_6_5
#define mbedtls_rsa_get_md_alg \
        mbedtls_rsa_get_md_alg_ncbicxx_3_6_5
#define mbedtls_rsa_get_padding_mode \
        mbedtls_rsa_get_padding_mode_ncbicxx_3_6_5
#define mbedtls_rsa_import \
        mbedtls_rsa_import_ncbicxx_3_6_5
#define mbedtls_rsa_import_raw \
        mbedtls_rsa_import_raw_ncbicxx_3_6_5
#define mbedtls_rsa_init \
        mbedtls_rsa_init_ncbicxx_3_6_5
#define mbedtls_rsa_parse_key \
        mbedtls_rsa_parse_key_ncbicxx_3_6_5
#define mbedtls_rsa_parse_pubkey \
        mbedtls_rsa_parse_pubkey_ncbicxx_3_6_5
#define mbedtls_rsa_pkcs1_decrypt \
        mbedtls_rsa_pkcs1_decrypt_ncbicxx_3_6_5
#define mbedtls_rsa_pkcs1_encrypt \
        mbedtls_rsa_pkcs1_encrypt_ncbicxx_3_6_5
#define mbedtls_rsa_pkcs1_sign \
        mbedtls_rsa_pkcs1_sign_ncbicxx_3_6_5
#define mbedtls_rsa_pkcs1_verify \
        mbedtls_rsa_pkcs1_verify_ncbicxx_3_6_5
#define mbedtls_rsa_private \
        mbedtls_rsa_private_ncbicxx_3_6_5
#define mbedtls_rsa_public \
        mbedtls_rsa_public_ncbicxx_3_6_5
#define mbedtls_rsa_rsaes_oaep_decrypt \
        mbedtls_rsa_rsaes_oaep_decrypt_ncbicxx_3_6_5
#define mbedtls_rsa_rsaes_oaep_encrypt \
        mbedtls_rsa_rsaes_oaep_encrypt_ncbicxx_3_6_5
#define mbedtls_rsa_rsaes_pkcs1_v15_decrypt \
        mbedtls_rsa_rsaes_pkcs1_v15_decrypt_ncbicxx_3_6_5
#define mbedtls_rsa_rsaes_pkcs1_v15_encrypt \
        mbedtls_rsa_rsaes_pkcs1_v15_encrypt_ncbicxx_3_6_5
#define mbedtls_rsa_rsassa_pkcs1_v15_sign \
        mbedtls_rsa_rsassa_pkcs1_v15_sign_ncbicxx_3_6_5
#define mbedtls_rsa_rsassa_pkcs1_v15_verify \
        mbedtls_rsa_rsassa_pkcs1_v15_verify_ncbicxx_3_6_5
#define mbedtls_rsa_rsassa_pss_sign \
        mbedtls_rsa_rsassa_pss_sign_ncbicxx_3_6_5
#define mbedtls_rsa_rsassa_pss_sign_ext \
        mbedtls_rsa_rsassa_pss_sign_ext_ncbicxx_3_6_5
#define mbedtls_rsa_rsassa_pss_sign_no_mode_check \
        mbedtls_rsa_rsassa_pss_sign_no_mode_check_ncbicxx_3_6_5
#define mbedtls_rsa_rsassa_pss_verify \
        mbedtls_rsa_rsassa_pss_verify_ncbicxx_3_6_5
#define mbedtls_rsa_rsassa_pss_verify_ext \
        mbedtls_rsa_rsassa_pss_verify_ext_ncbicxx_3_6_5
#define mbedtls_rsa_set_padding \
        mbedtls_rsa_set_padding_ncbicxx_3_6_5
#define mbedtls_rsa_write_key \
        mbedtls_rsa_write_key_ncbicxx_3_6_5
#define mbedtls_rsa_write_pubkey \
        mbedtls_rsa_write_pubkey_ncbicxx_3_6_5
#define mbedtls_rsa_deduce_crt \
        mbedtls_rsa_deduce_crt_ncbicxx_3_6_5
#define mbedtls_rsa_deduce_primes \
        mbedtls_rsa_deduce_primes_ncbicxx_3_6_5
#define mbedtls_rsa_deduce_private_exponent \
        mbedtls_rsa_deduce_private_exponent_ncbicxx_3_6_5
#define mbedtls_rsa_validate_crt \
        mbedtls_rsa_validate_crt_ncbicxx_3_6_5
#define mbedtls_rsa_validate_params \
        mbedtls_rsa_validate_params_ncbicxx_3_6_5
#define mbedtls_internal_sha1_process \
        mbedtls_internal_sha1_process_ncbicxx_3_6_5
#define mbedtls_sha1 \
        mbedtls_sha1_ncbicxx_3_6_5
#define mbedtls_sha1_clone \
        mbedtls_sha1_clone_ncbicxx_3_6_5
#define mbedtls_sha1_finish \
        mbedtls_sha1_finish_ncbicxx_3_6_5
#define mbedtls_sha1_free \
        mbedtls_sha1_free_ncbicxx_3_6_5
#define mbedtls_sha1_init \
        mbedtls_sha1_init_ncbicxx_3_6_5
#define mbedtls_sha1_starts \
        mbedtls_sha1_starts_ncbicxx_3_6_5
#define mbedtls_sha1_update \
        mbedtls_sha1_update_ncbicxx_3_6_5
#define mbedtls_internal_sha256_process \
        mbedtls_internal_sha256_process_ncbicxx_3_6_5
#define mbedtls_sha256 \
        mbedtls_sha256_ncbicxx_3_6_5
#define mbedtls_sha256_clone \
        mbedtls_sha256_clone_ncbicxx_3_6_5
#define mbedtls_sha256_finish \
        mbedtls_sha256_finish_ncbicxx_3_6_5
#define mbedtls_sha256_free \
        mbedtls_sha256_free_ncbicxx_3_6_5
#define mbedtls_sha256_init \
        mbedtls_sha256_init_ncbicxx_3_6_5
#define mbedtls_sha256_starts \
        mbedtls_sha256_starts_ncbicxx_3_6_5
#define mbedtls_sha256_update \
        mbedtls_sha256_update_ncbicxx_3_6_5
#define mbedtls_sha3 \
        mbedtls_sha3_ncbicxx_3_6_5
#define mbedtls_sha3_clone \
        mbedtls_sha3_clone_ncbicxx_3_6_5
#define mbedtls_sha3_finish \
        mbedtls_sha3_finish_ncbicxx_3_6_5
#define mbedtls_sha3_free \
        mbedtls_sha3_free_ncbicxx_3_6_5
#define mbedtls_sha3_init \
        mbedtls_sha3_init_ncbicxx_3_6_5
#define mbedtls_sha3_starts \
        mbedtls_sha3_starts_ncbicxx_3_6_5
#define mbedtls_sha3_update \
        mbedtls_sha3_update_ncbicxx_3_6_5
#define mbedtls_internal_sha512_process \
        mbedtls_internal_sha512_process_ncbicxx_3_6_5
#define mbedtls_sha512 \
        mbedtls_sha512_ncbicxx_3_6_5
#define mbedtls_sha512_clone \
        mbedtls_sha512_clone_ncbicxx_3_6_5
#define mbedtls_sha512_finish \
        mbedtls_sha512_finish_ncbicxx_3_6_5
#define mbedtls_sha512_free \
        mbedtls_sha512_free_ncbicxx_3_6_5
#define mbedtls_sha512_init \
        mbedtls_sha512_init_ncbicxx_3_6_5
#define mbedtls_sha512_starts \
        mbedtls_sha512_starts_ncbicxx_3_6_5
#define mbedtls_sha512_update \
        mbedtls_sha512_update_ncbicxx_3_6_5
#define mbedtls_ssl_cache_free \
        mbedtls_ssl_cache_free_ncbicxx_3_6_5
#define mbedtls_ssl_cache_get \
        mbedtls_ssl_cache_get_ncbicxx_3_6_5
#define mbedtls_ssl_cache_init \
        mbedtls_ssl_cache_init_ncbicxx_3_6_5
#define mbedtls_ssl_cache_remove \
        mbedtls_ssl_cache_remove_ncbicxx_3_6_5
#define mbedtls_ssl_cache_set \
        mbedtls_ssl_cache_set_ncbicxx_3_6_5
#define mbedtls_ssl_cache_set_max_entries \
        mbedtls_ssl_cache_set_max_entries_ncbicxx_3_6_5
#define mbedtls_ssl_cache_set_timeout \
        mbedtls_ssl_cache_set_timeout_ncbicxx_3_6_5
#define mbedtls_ssl_ciphersuite_from_id \
        mbedtls_ssl_ciphersuite_from_id_ncbicxx_3_6_5
#define mbedtls_ssl_ciphersuite_from_string \
        mbedtls_ssl_ciphersuite_from_string_ncbicxx_3_6_5
#define mbedtls_ssl_ciphersuite_get_cipher_key_bitlen \
        mbedtls_ssl_ciphersuite_get_cipher_key_bitlen_ncbicxx_3_6_5
#define mbedtls_ssl_ciphersuite_uses_ec \
        mbedtls_ssl_ciphersuite_uses_ec_ncbicxx_3_6_5
#define mbedtls_ssl_ciphersuite_uses_psk \
        mbedtls_ssl_ciphersuite_uses_psk_ncbicxx_3_6_5
#define mbedtls_ssl_get_ciphersuite_id \
        mbedtls_ssl_get_ciphersuite_id_ncbicxx_3_6_5
#define mbedtls_ssl_get_ciphersuite_name \
        mbedtls_ssl_get_ciphersuite_name_ncbicxx_3_6_5
#define mbedtls_ssl_get_ciphersuite_sig_alg \
        mbedtls_ssl_get_ciphersuite_sig_alg_ncbicxx_3_6_5
#define mbedtls_ssl_get_ciphersuite_sig_pk_alg \
        mbedtls_ssl_get_ciphersuite_sig_pk_alg_ncbicxx_3_6_5
#define mbedtls_ssl_list_ciphersuites \
        mbedtls_ssl_list_ciphersuites_ncbicxx_3_6_5
#define mbedtls_ssl_write_client_hello \
        mbedtls_ssl_write_client_hello_ncbicxx_3_6_5
#define mbedtls_ssl_cookie_check \
        mbedtls_ssl_cookie_check_ncbicxx_3_6_5
#define mbedtls_ssl_cookie_free \
        mbedtls_ssl_cookie_free_ncbicxx_3_6_5
#define mbedtls_ssl_cookie_init \
        mbedtls_ssl_cookie_init_ncbicxx_3_6_5
#define mbedtls_ssl_cookie_set_timeout \
        mbedtls_ssl_cookie_set_timeout_ncbicxx_3_6_5
#define mbedtls_ssl_cookie_setup \
        mbedtls_ssl_cookie_setup_ncbicxx_3_6_5
#define mbedtls_ssl_cookie_write \
        mbedtls_ssl_cookie_write_ncbicxx_3_6_5
#define mbedtls_ssl_key_export_type_str \
        mbedtls_ssl_key_export_type_str_ncbicxx_3_6_5
#define mbedtls_ssl_named_group_to_str \
        mbedtls_ssl_named_group_to_str_ncbicxx_3_6_5
#define mbedtls_ssl_protocol_version_str \
        mbedtls_ssl_protocol_version_str_ncbicxx_3_6_5
#define mbedtls_ssl_sig_alg_to_str \
        mbedtls_ssl_sig_alg_to_str_ncbicxx_3_6_5
#define mbedtls_ssl_states_str \
        mbedtls_ssl_states_str_ncbicxx_3_6_5
#define mbedtls_tls_prf_types_str \
        mbedtls_tls_prf_types_str_ncbicxx_3_6_5
#define mbedtls_ssl_buffering_free \
        mbedtls_ssl_buffering_free_ncbicxx_3_6_5
#define mbedtls_ssl_check_pending \
        mbedtls_ssl_check_pending_ncbicxx_3_6_5
#define mbedtls_ssl_check_record \
        mbedtls_ssl_check_record_ncbicxx_3_6_5
#define mbedtls_ssl_check_timer \
        mbedtls_ssl_check_timer_ncbicxx_3_6_5
#define mbedtls_ssl_close_notify \
        mbedtls_ssl_close_notify_ncbicxx_3_6_5
#define mbedtls_ssl_decrypt_buf \
        mbedtls_ssl_decrypt_buf_ncbicxx_3_6_5
#define mbedtls_ssl_dtls_replay_check \
        mbedtls_ssl_dtls_replay_check_ncbicxx_3_6_5
#define mbedtls_ssl_dtls_replay_reset \
        mbedtls_ssl_dtls_replay_reset_ncbicxx_3_6_5
#define mbedtls_ssl_dtls_replay_update \
        mbedtls_ssl_dtls_replay_update_ncbicxx_3_6_5
#define mbedtls_ssl_encrypt_buf \
        mbedtls_ssl_encrypt_buf_ncbicxx_3_6_5
#define mbedtls_ssl_fetch_input \
        mbedtls_ssl_fetch_input_ncbicxx_3_6_5
#define mbedtls_ssl_finish_handshake_msg \
        mbedtls_ssl_finish_handshake_msg_ncbicxx_3_6_5
#define mbedtls_ssl_flight_free \
        mbedtls_ssl_flight_free_ncbicxx_3_6_5
#define mbedtls_ssl_flight_transmit \
        mbedtls_ssl_flight_transmit_ncbicxx_3_6_5
#define mbedtls_ssl_flush_output \
        mbedtls_ssl_flush_output_ncbicxx_3_6_5
#define mbedtls_ssl_get_bytes_avail \
        mbedtls_ssl_get_bytes_avail_ncbicxx_3_6_5
#define mbedtls_ssl_get_record_expansion \
        mbedtls_ssl_get_record_expansion_ncbicxx_3_6_5
#define mbedtls_ssl_handle_message_type \
        mbedtls_ssl_handle_message_type_ncbicxx_3_6_5
#define mbedtls_ssl_handle_pending_alert \
        mbedtls_ssl_handle_pending_alert_ncbicxx_3_6_5
#define mbedtls_ssl_parse_change_cipher_spec \
        mbedtls_ssl_parse_change_cipher_spec_ncbicxx_3_6_5
#define mbedtls_ssl_pend_fatal_alert \
        mbedtls_ssl_pend_fatal_alert_ncbicxx_3_6_5
#define mbedtls_ssl_prepare_handshake_record \
        mbedtls_ssl_prepare_handshake_record_ncbicxx_3_6_5
#define mbedtls_ssl_read \
        mbedtls_ssl_read_ncbicxx_3_6_5
#define mbedtls_ssl_read_record \
        mbedtls_ssl_read_record_ncbicxx_3_6_5
#define mbedtls_ssl_read_version \
        mbedtls_ssl_read_version_ncbicxx_3_6_5
#define mbedtls_ssl_recv_flight_completed \
        mbedtls_ssl_recv_flight_completed_ncbicxx_3_6_5
#define mbedtls_ssl_resend \
        mbedtls_ssl_resend_ncbicxx_3_6_5
#define mbedtls_ssl_reset_in_pointers \
        mbedtls_ssl_reset_in_pointers_ncbicxx_3_6_5
#define mbedtls_ssl_reset_out_pointers \
        mbedtls_ssl_reset_out_pointers_ncbicxx_3_6_5
#define mbedtls_ssl_send_alert_message \
        mbedtls_ssl_send_alert_message_ncbicxx_3_6_5
#define mbedtls_ssl_send_fatal_handshake_failure \
        mbedtls_ssl_send_fatal_handshake_failure_ncbicxx_3_6_5
#define mbedtls_ssl_send_flight_completed \
        mbedtls_ssl_send_flight_completed_ncbicxx_3_6_5
#define mbedtls_ssl_set_inbound_transform \
        mbedtls_ssl_set_inbound_transform_ncbicxx_3_6_5
#define mbedtls_ssl_set_outbound_transform \
        mbedtls_ssl_set_outbound_transform_ncbicxx_3_6_5
#define mbedtls_ssl_set_timer \
        mbedtls_ssl_set_timer_ncbicxx_3_6_5
#define mbedtls_ssl_start_handshake_msg \
        mbedtls_ssl_start_handshake_msg_ncbicxx_3_6_5
#define mbedtls_ssl_transform_free \
        mbedtls_ssl_transform_free_ncbicxx_3_6_5
#define mbedtls_ssl_update_handshake_status \
        mbedtls_ssl_update_handshake_status_ncbicxx_3_6_5
#define mbedtls_ssl_update_in_pointers \
        mbedtls_ssl_update_in_pointers_ncbicxx_3_6_5
#define mbedtls_ssl_update_out_pointers \
        mbedtls_ssl_update_out_pointers_ncbicxx_3_6_5
#define mbedtls_ssl_write \
        mbedtls_ssl_write_ncbicxx_3_6_5
#define mbedtls_ssl_write_change_cipher_spec \
        mbedtls_ssl_write_change_cipher_spec_ncbicxx_3_6_5
#define mbedtls_ssl_write_handshake_msg_ext \
        mbedtls_ssl_write_handshake_msg_ext_ncbicxx_3_6_5
#define mbedtls_ssl_write_record \
        mbedtls_ssl_write_record_ncbicxx_3_6_5
#define mbedtls_ssl_write_version \
        mbedtls_ssl_write_version_ncbicxx_3_6_5
#define mbedtls_ssl_ticket_free \
        mbedtls_ssl_ticket_free_ncbicxx_3_6_5
#define mbedtls_ssl_ticket_init \
        mbedtls_ssl_ticket_init_ncbicxx_3_6_5
#define mbedtls_ssl_ticket_parse \
        mbedtls_ssl_ticket_parse_ncbicxx_3_6_5
#define mbedtls_ssl_ticket_rotate \
        mbedtls_ssl_ticket_rotate_ncbicxx_3_6_5
#define mbedtls_ssl_ticket_setup \
        mbedtls_ssl_ticket_setup_ncbicxx_3_6_5
#define mbedtls_ssl_ticket_write \
        mbedtls_ssl_ticket_write_ncbicxx_3_6_5
#define mbedtls_ssl_add_hs_hdr_to_checksum \
        mbedtls_ssl_add_hs_hdr_to_checksum_ncbicxx_3_6_5
#define mbedtls_ssl_add_hs_msg_to_checksum \
        mbedtls_ssl_add_hs_msg_to_checksum_ncbicxx_3_6_5
#define mbedtls_ssl_check_cert_usage \
        mbedtls_ssl_check_cert_usage_ncbicxx_3_6_5
#define mbedtls_ssl_check_curve \
        mbedtls_ssl_check_curve_ncbicxx_3_6_5
#define mbedtls_ssl_check_curve_tls_id \
        mbedtls_ssl_check_curve_tls_id_ncbicxx_3_6_5
#define mbedtls_ssl_cipher_to_psa \
        mbedtls_ssl_cipher_to_psa_ncbicxx_3_6_5
#define mbedtls_ssl_conf_alpn_protocols \
        mbedtls_ssl_conf_alpn_protocols_ncbicxx_3_6_5
#define mbedtls_ssl_conf_authmode \
        mbedtls_ssl_conf_authmode_ncbicxx_3_6_5
#define mbedtls_ssl_conf_ca_chain \
        mbedtls_ssl_conf_ca_chain_ncbicxx_3_6_5
#define mbedtls_ssl_conf_cert_profile \
        mbedtls_ssl_conf_cert_profile_ncbicxx_3_6_5
#define mbedtls_ssl_conf_cert_req_ca_list \
        mbedtls_ssl_conf_cert_req_ca_list_ncbicxx_3_6_5
#define mbedtls_ssl_conf_cid \
        mbedtls_ssl_conf_cid_ncbicxx_3_6_5
#define mbedtls_ssl_conf_ciphersuites \
        mbedtls_ssl_conf_ciphersuites_ncbicxx_3_6_5
#define mbedtls_ssl_conf_curves \
        mbedtls_ssl_conf_curves_ncbicxx_3_6_5
#define mbedtls_ssl_conf_dbg \
        mbedtls_ssl_conf_dbg_ncbicxx_3_6_5
#define mbedtls_ssl_conf_dh_param_bin \
        mbedtls_ssl_conf_dh_param_bin_ncbicxx_3_6_5
#define mbedtls_ssl_conf_dh_param_ctx \
        mbedtls_ssl_conf_dh_param_ctx_ncbicxx_3_6_5
#define mbedtls_ssl_conf_dhm_min_bitlen \
        mbedtls_ssl_conf_dhm_min_bitlen_ncbicxx_3_6_5
#define mbedtls_ssl_conf_dtls_anti_replay \
        mbedtls_ssl_conf_dtls_anti_replay_ncbicxx_3_6_5
#define mbedtls_ssl_conf_dtls_badmac_limit \
        mbedtls_ssl_conf_dtls_badmac_limit_ncbicxx_3_6_5
#define mbedtls_ssl_conf_encrypt_then_mac \
        mbedtls_ssl_conf_encrypt_then_mac_ncbicxx_3_6_5
#define mbedtls_ssl_conf_endpoint \
        mbedtls_ssl_conf_endpoint_ncbicxx_3_6_5
#define mbedtls_ssl_conf_extended_master_secret \
        mbedtls_ssl_conf_extended_master_secret_ncbicxx_3_6_5
#define mbedtls_ssl_conf_groups \
        mbedtls_ssl_conf_groups_ncbicxx_3_6_5
#define mbedtls_ssl_conf_handshake_timeout \
        mbedtls_ssl_conf_handshake_timeout_ncbicxx_3_6_5
#define mbedtls_ssl_conf_has_static_psk \
        mbedtls_ssl_conf_has_static_psk_ncbicxx_3_6_5
#define mbedtls_ssl_conf_legacy_renegotiation \
        mbedtls_ssl_conf_legacy_renegotiation_ncbicxx_3_6_5
#define mbedtls_ssl_conf_max_frag_len \
        mbedtls_ssl_conf_max_frag_len_ncbicxx_3_6_5
#define mbedtls_ssl_conf_max_version \
        mbedtls_ssl_conf_max_version_ncbicxx_3_6_5
#define mbedtls_ssl_conf_min_version \
        mbedtls_ssl_conf_min_version_ncbicxx_3_6_5
#define mbedtls_ssl_conf_new_session_tickets \
        mbedtls_ssl_conf_new_session_tickets_ncbicxx_3_6_5
#define mbedtls_ssl_conf_own_cert \
        mbedtls_ssl_conf_own_cert_ncbicxx_3_6_5
#define mbedtls_ssl_conf_psk \
        mbedtls_ssl_conf_psk_ncbicxx_3_6_5
#define mbedtls_ssl_conf_psk_cb \
        mbedtls_ssl_conf_psk_cb_ncbicxx_3_6_5
#define mbedtls_ssl_conf_read_timeout \
        mbedtls_ssl_conf_read_timeout_ncbicxx_3_6_5
#define mbedtls_ssl_conf_renegotiation \
        mbedtls_ssl_conf_renegotiation_ncbicxx_3_6_5
#define mbedtls_ssl_conf_renegotiation_enforced \
        mbedtls_ssl_conf_renegotiation_enforced_ncbicxx_3_6_5
#define mbedtls_ssl_conf_renegotiation_period \
        mbedtls_ssl_conf_renegotiation_period_ncbicxx_3_6_5
#define mbedtls_ssl_conf_rng \
        mbedtls_ssl_conf_rng_ncbicxx_3_6_5
#define mbedtls_ssl_conf_session_cache \
        mbedtls_ssl_conf_session_cache_ncbicxx_3_6_5
#define mbedtls_ssl_conf_session_tickets \
        mbedtls_ssl_conf_session_tickets_ncbicxx_3_6_5
#define mbedtls_ssl_conf_session_tickets_cb \
        mbedtls_ssl_conf_session_tickets_cb_ncbicxx_3_6_5
#define mbedtls_ssl_conf_sig_algs \
        mbedtls_ssl_conf_sig_algs_ncbicxx_3_6_5
#define mbedtls_ssl_conf_sig_hashes \
        mbedtls_ssl_conf_sig_hashes_ncbicxx_3_6_5
#define mbedtls_ssl_conf_sni \
        mbedtls_ssl_conf_sni_ncbicxx_3_6_5
#define mbedtls_ssl_conf_tls13_enable_signal_new_session_tickets \
        mbedtls_ssl_conf_tls13_enable_signal_new_session_tickets_ncbicxx_3_6_5
#define mbedtls_ssl_conf_tls13_key_exchange_modes \
        mbedtls_ssl_conf_tls13_key_exchange_modes_ncbicxx_3_6_5
#define mbedtls_ssl_conf_transport \
        mbedtls_ssl_conf_transport_ncbicxx_3_6_5
#define mbedtls_ssl_conf_verify \
        mbedtls_ssl_conf_verify_ncbicxx_3_6_5
#define mbedtls_ssl_config_defaults \
        mbedtls_ssl_config_defaults_ncbicxx_3_6_5
#define mbedtls_ssl_config_free \
        mbedtls_ssl_config_free_ncbicxx_3_6_5
#define mbedtls_ssl_config_init \
        mbedtls_ssl_config_init_ncbicxx_3_6_5
#define mbedtls_ssl_context_load \
        mbedtls_ssl_context_load_ncbicxx_3_6_5
#define mbedtls_ssl_context_save \
        mbedtls_ssl_context_save_ncbicxx_3_6_5
#define mbedtls_ssl_derive_keys \
        mbedtls_ssl_derive_keys_ncbicxx_3_6_5
#define mbedtls_ssl_export_keying_material \
        mbedtls_ssl_export_keying_material_ncbicxx_3_6_5
#define mbedtls_ssl_free \
        mbedtls_ssl_free_ncbicxx_3_6_5
#define mbedtls_ssl_get_alpn_protocol \
        mbedtls_ssl_get_alpn_protocol_ncbicxx_3_6_5
#define mbedtls_ssl_get_ciphersuite \
        mbedtls_ssl_get_ciphersuite_ncbicxx_3_6_5
#define mbedtls_ssl_get_ciphersuite_id_from_ssl \
        mbedtls_ssl_get_ciphersuite_id_from_ssl_ncbicxx_3_6_5
#define mbedtls_ssl_get_current_mtu \
        mbedtls_ssl_get_current_mtu_ncbicxx_3_6_5
#define mbedtls_ssl_get_curve_name_from_tls_id \
        mbedtls_ssl_get_curve_name_from_tls_id_ncbicxx_3_6_5
#define mbedtls_ssl_get_ecp_group_id_from_tls_id \
        mbedtls_ssl_get_ecp_group_id_from_tls_id_ncbicxx_3_6_5
#define mbedtls_ssl_get_extension_id \
        mbedtls_ssl_get_extension_id_ncbicxx_3_6_5
#define mbedtls_ssl_get_extension_mask \
        mbedtls_ssl_get_extension_mask_ncbicxx_3_6_5
#define mbedtls_ssl_get_extension_name \
        mbedtls_ssl_get_extension_name_ncbicxx_3_6_5
#define mbedtls_ssl_get_handshake_transcript \
        mbedtls_ssl_get_handshake_transcript_ncbicxx_3_6_5
#define mbedtls_ssl_get_hostname_pointer \
        mbedtls_ssl_get_hostname_pointer_ncbicxx_3_6_5
#define mbedtls_ssl_get_hs_sni \
        mbedtls_ssl_get_hs_sni_ncbicxx_3_6_5
#define mbedtls_ssl_get_input_max_frag_len \
        mbedtls_ssl_get_input_max_frag_len_ncbicxx_3_6_5
#define mbedtls_ssl_get_key_exchange_md_tls1_2 \
        mbedtls_ssl_get_key_exchange_md_tls1_2_ncbicxx_3_6_5
#define mbedtls_ssl_get_max_in_record_payload \
        mbedtls_ssl_get_max_in_record_payload_ncbicxx_3_6_5
#define mbedtls_ssl_get_max_out_record_payload \
        mbedtls_ssl_get_max_out_record_payload_ncbicxx_3_6_5
#define mbedtls_ssl_get_mode_from_ciphersuite \
        mbedtls_ssl_get_mode_from_ciphersuite_ncbicxx_3_6_5
#define mbedtls_ssl_get_mode_from_transform \
        mbedtls_ssl_get_mode_from_transform_ncbicxx_3_6_5
#define mbedtls_ssl_get_output_max_frag_len \
        mbedtls_ssl_get_output_max_frag_len_ncbicxx_3_6_5
#define mbedtls_ssl_get_own_cid \
        mbedtls_ssl_get_own_cid_ncbicxx_3_6_5
#define mbedtls_ssl_get_peer_cert \
        mbedtls_ssl_get_peer_cert_ncbicxx_3_6_5
#define mbedtls_ssl_get_peer_cid \
        mbedtls_ssl_get_peer_cid_ncbicxx_3_6_5
#define mbedtls_ssl_get_psa_curve_info_from_tls_id \
        mbedtls_ssl_get_psa_curve_info_from_tls_id_ncbicxx_3_6_5
#define mbedtls_ssl_get_session \
        mbedtls_ssl_get_session_ncbicxx_3_6_5
#define mbedtls_ssl_get_tls_id_from_ecp_group_id \
        mbedtls_ssl_get_tls_id_from_ecp_group_id_ncbicxx_3_6_5
#define mbedtls_ssl_get_verify_result \
        mbedtls_ssl_get_verify_result_ncbicxx_3_6_5
#define mbedtls_ssl_get_version \
        mbedtls_ssl_get_version_ncbicxx_3_6_5
#define mbedtls_ssl_handshake \
        mbedtls_ssl_handshake_ncbicxx_3_6_5
#define mbedtls_ssl_handshake_free \
        mbedtls_ssl_handshake_free_ncbicxx_3_6_5
#define mbedtls_ssl_handshake_step \
        mbedtls_ssl_handshake_step_ncbicxx_3_6_5
#define mbedtls_ssl_handshake_wrapup \
        mbedtls_ssl_handshake_wrapup_ncbicxx_3_6_5
#define mbedtls_ssl_handshake_wrapup_free_hs_transform \
        mbedtls_ssl_handshake_wrapup_free_hs_transform_ncbicxx_3_6_5
#define mbedtls_ssl_hash_from_md_alg \
        mbedtls_ssl_hash_from_md_alg_ncbicxx_3_6_5
#define mbedtls_ssl_init \
        mbedtls_ssl_init_ncbicxx_3_6_5
#define mbedtls_ssl_md_alg_from_hash \
        mbedtls_ssl_md_alg_from_hash_ncbicxx_3_6_5
#define mbedtls_ssl_optimize_checksum \
        mbedtls_ssl_optimize_checksum_ncbicxx_3_6_5
#define mbedtls_ssl_parse_alpn_ext \
        mbedtls_ssl_parse_alpn_ext_ncbicxx_3_6_5
#define mbedtls_ssl_parse_certificate \
        mbedtls_ssl_parse_certificate_ncbicxx_3_6_5
#define mbedtls_ssl_parse_finished \
        mbedtls_ssl_parse_finished_ncbicxx_3_6_5
#define mbedtls_ssl_parse_server_name_ext \
        mbedtls_ssl_parse_server_name_ext_ncbicxx_3_6_5
#define mbedtls_ssl_parse_sig_alg_ext \
        mbedtls_ssl_parse_sig_alg_ext_ncbicxx_3_6_5
#define mbedtls_ssl_pk_alg_from_sig \
        mbedtls_ssl_pk_alg_from_sig_ncbicxx_3_6_5
#define mbedtls_ssl_print_extension \
        mbedtls_ssl_print_extension_ncbicxx_3_6_5
#define mbedtls_ssl_print_extensions \
        mbedtls_ssl_print_extensions_ncbicxx_3_6_5
#define mbedtls_ssl_print_ticket_flags \
        mbedtls_ssl_print_ticket_flags_ncbicxx_3_6_5
#define mbedtls_ssl_psk_derive_premaster \
        mbedtls_ssl_psk_derive_premaster_ncbicxx_3_6_5
#define mbedtls_ssl_renegotiate \
        mbedtls_ssl_renegotiate_ncbicxx_3_6_5
#define mbedtls_ssl_resend_hello_request \
        mbedtls_ssl_resend_hello_request_ncbicxx_3_6_5
#define mbedtls_ssl_reset_checksum \
        mbedtls_ssl_reset_checksum_ncbicxx_3_6_5
#define mbedtls_ssl_session_copy \
        mbedtls_ssl_session_copy_ncbicxx_3_6_5
#define mbedtls_ssl_session_free \
        mbedtls_ssl_session_free_ncbicxx_3_6_5
#define mbedtls_ssl_session_init \
        mbedtls_ssl_session_init_ncbicxx_3_6_5
#define mbedtls_ssl_session_load \
        mbedtls_ssl_session_load_ncbicxx_3_6_5
#define mbedtls_ssl_session_reset \
        mbedtls_ssl_session_reset_ncbicxx_3_6_5
#define mbedtls_ssl_session_reset_int \
        mbedtls_ssl_session_reset_int_ncbicxx_3_6_5
#define mbedtls_ssl_session_reset_msg_layer \
        mbedtls_ssl_session_reset_msg_layer_ncbicxx_3_6_5
#define mbedtls_ssl_session_save \
        mbedtls_ssl_session_save_ncbicxx_3_6_5
#define mbedtls_ssl_session_set_hostname \
        mbedtls_ssl_session_set_hostname_ncbicxx_3_6_5
#define mbedtls_ssl_set_bio \
        mbedtls_ssl_set_bio_ncbicxx_3_6_5
#define mbedtls_ssl_set_calc_verify_md \
        mbedtls_ssl_set_calc_verify_md_ncbicxx_3_6_5
#define mbedtls_ssl_set_cid \
        mbedtls_ssl_set_cid_ncbicxx_3_6_5
#define mbedtls_ssl_set_datagram_packing \
        mbedtls_ssl_set_datagram_packing_ncbicxx_3_6_5
#define mbedtls_ssl_set_export_keys_cb \
        mbedtls_ssl_set_export_keys_cb_ncbicxx_3_6_5
#define mbedtls_ssl_set_hostname \
        mbedtls_ssl_set_hostname_ncbicxx_3_6_5
#define mbedtls_ssl_set_hs_authmode \
        mbedtls_ssl_set_hs_authmode_ncbicxx_3_6_5
#define mbedtls_ssl_set_hs_ca_chain \
        mbedtls_ssl_set_hs_ca_chain_ncbicxx_3_6_5
#define mbedtls_ssl_set_hs_dn_hints \
        mbedtls_ssl_set_hs_dn_hints_ncbicxx_3_6_5
#define mbedtls_ssl_set_hs_own_cert \
        mbedtls_ssl_set_hs_own_cert_ncbicxx_3_6_5
#define mbedtls_ssl_set_hs_psk \
        mbedtls_ssl_set_hs_psk_ncbicxx_3_6_5
#define mbedtls_ssl_set_mtu \
        mbedtls_ssl_set_mtu_ncbicxx_3_6_5
#define mbedtls_ssl_set_session \
        mbedtls_ssl_set_session_ncbicxx_3_6_5
#define mbedtls_ssl_set_timer_cb \
        mbedtls_ssl_set_timer_cb_ncbicxx_3_6_5
#define mbedtls_ssl_set_verify \
        mbedtls_ssl_set_verify_ncbicxx_3_6_5
#define mbedtls_ssl_setup \
        mbedtls_ssl_setup_ncbicxx_3_6_5
#define mbedtls_ssl_sig_from_pk \
        mbedtls_ssl_sig_from_pk_ncbicxx_3_6_5
#define mbedtls_ssl_sig_from_pk_alg \
        mbedtls_ssl_sig_from_pk_alg_ncbicxx_3_6_5
#define mbedtls_ssl_start_renegotiation \
        mbedtls_ssl_start_renegotiation_ncbicxx_3_6_5
#define mbedtls_ssl_tls12_get_preferred_hash_for_sig_alg \
        mbedtls_ssl_tls12_get_preferred_hash_for_sig_alg_ncbicxx_3_6_5
#define mbedtls_ssl_tls_prf \
        mbedtls_ssl_tls_prf_ncbicxx_3_6_5
#define mbedtls_ssl_transform_init \
        mbedtls_ssl_transform_init_ncbicxx_3_6_5
#define mbedtls_ssl_validate_ciphersuite \
        mbedtls_ssl_validate_ciphersuite_ncbicxx_3_6_5
#define mbedtls_ssl_verify_certificate \
        mbedtls_ssl_verify_certificate_ncbicxx_3_6_5
#define mbedtls_ssl_write_alpn_ext \
        mbedtls_ssl_write_alpn_ext_ncbicxx_3_6_5
#define mbedtls_ssl_write_certificate \
        mbedtls_ssl_write_certificate_ncbicxx_3_6_5
#define mbedtls_ssl_write_finished \
        mbedtls_ssl_write_finished_ncbicxx_3_6_5
#define mbedtls_ssl_write_sig_alg_ext \
        mbedtls_ssl_write_sig_alg_ext_ncbicxx_3_6_5
#define mbedtls_ssl_handshake_client_step \
        mbedtls_ssl_handshake_client_step_ncbicxx_3_6_5
#define mbedtls_ssl_tls12_write_client_hello_exts \
        mbedtls_ssl_tls12_write_client_hello_exts_ncbicxx_3_6_5
#define mbedtls_ssl_conf_dtls_cookies \
        mbedtls_ssl_conf_dtls_cookies_ncbicxx_3_6_5
#define mbedtls_ssl_conf_preference_order \
        mbedtls_ssl_conf_preference_order_ncbicxx_3_6_5
#define mbedtls_ssl_handshake_server_step \
        mbedtls_ssl_handshake_server_step_ncbicxx_3_6_5
#define mbedtls_ssl_set_client_transport_id \
        mbedtls_ssl_set_client_transport_id_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_finalize_client_hello \
        mbedtls_ssl_tls13_finalize_client_hello_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_handshake_client_step \
        mbedtls_ssl_tls13_handshake_client_step_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_write_binders_of_pre_shared_key_ext \
        mbedtls_ssl_tls13_write_binders_of_pre_shared_key_ext_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_write_client_hello_exts \
        mbedtls_ssl_tls13_write_client_hello_exts_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_write_identities_of_pre_shared_key_ext \
        mbedtls_ssl_tls13_write_identities_of_pre_shared_key_ext_ncbicxx_3_6_5
#define mbedtls_ssl_reset_transcript_for_hrr \
        mbedtls_ssl_reset_transcript_for_hrr_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_check_received_extension \
        mbedtls_ssl_tls13_check_received_extension_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_check_sig_alg_cert_key_match \
        mbedtls_ssl_tls13_check_sig_alg_cert_key_match_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_crypto_init \
        mbedtls_ssl_tls13_crypto_init_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_fetch_handshake_msg \
        mbedtls_ssl_tls13_fetch_handshake_msg_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_generate_and_write_xxdh_key_exchange \
        mbedtls_ssl_tls13_generate_and_write_xxdh_key_exchange_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_handshake_wrapup \
        mbedtls_ssl_tls13_handshake_wrapup_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_hello_retry_request_magic \
        mbedtls_ssl_tls13_hello_retry_request_magic_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_is_supported_versions_ext_present_in_exts \
        mbedtls_ssl_tls13_is_supported_versions_ext_present_in_exts_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_process_certificate \
        mbedtls_ssl_tls13_process_certificate_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_process_certificate_verify \
        mbedtls_ssl_tls13_process_certificate_verify_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_process_finished_message \
        mbedtls_ssl_tls13_process_finished_message_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_read_public_xxdhe_share \
        mbedtls_ssl_tls13_read_public_xxdhe_share_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_write_certificate \
        mbedtls_ssl_tls13_write_certificate_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_write_certificate_verify \
        mbedtls_ssl_tls13_write_certificate_verify_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_write_change_cipher_spec \
        mbedtls_ssl_tls13_write_change_cipher_spec_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_write_finished_message \
        mbedtls_ssl_tls13_write_finished_message_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_calculate_verify_data \
        mbedtls_ssl_tls13_calculate_verify_data_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_compute_application_transform \
        mbedtls_ssl_tls13_compute_application_transform_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_compute_handshake_transform \
        mbedtls_ssl_tls13_compute_handshake_transform_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_compute_resumption_master_secret \
        mbedtls_ssl_tls13_compute_resumption_master_secret_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_create_psk_binder \
        mbedtls_ssl_tls13_create_psk_binder_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_derive_application_secrets \
        mbedtls_ssl_tls13_derive_application_secrets_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_derive_early_secrets \
        mbedtls_ssl_tls13_derive_early_secrets_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_derive_handshake_secrets \
        mbedtls_ssl_tls13_derive_handshake_secrets_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_derive_resumption_master_secret \
        mbedtls_ssl_tls13_derive_resumption_master_secret_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_derive_secret \
        mbedtls_ssl_tls13_derive_secret_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_evolve_secret \
        mbedtls_ssl_tls13_evolve_secret_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_export_handshake_psk \
        mbedtls_ssl_tls13_export_handshake_psk_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_exporter \
        mbedtls_ssl_tls13_exporter_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_hkdf_expand_label \
        mbedtls_ssl_tls13_hkdf_expand_label_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_key_schedule_stage_early \
        mbedtls_ssl_tls13_key_schedule_stage_early_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_labels \
        mbedtls_ssl_tls13_labels_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_make_traffic_keys \
        mbedtls_ssl_tls13_make_traffic_keys_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_populate_transform \
        mbedtls_ssl_tls13_populate_transform_ncbicxx_3_6_5
#define mbedtls_ssl_tls13_handshake_server_step \
        mbedtls_ssl_tls13_handshake_server_step_ncbicxx_3_6_5
#define mbedtls_mutex_free \
        mbedtls_mutex_free_ncbicxx_3_6_5
#define mbedtls_mutex_init \
        mbedtls_mutex_init_ncbicxx_3_6_5
#define mbedtls_mutex_lock \
        mbedtls_mutex_lock_ncbicxx_3_6_5
#define mbedtls_mutex_unlock \
        mbedtls_mutex_unlock_ncbicxx_3_6_5
#define mbedtls_threading_key_slot_mutex \
        mbedtls_threading_key_slot_mutex_ncbicxx_3_6_5
#define mbedtls_threading_psa_globaldata_mutex \
        mbedtls_threading_psa_globaldata_mutex_ncbicxx_3_6_5
#define mbedtls_threading_psa_rngdata_mutex \
        mbedtls_threading_psa_rngdata_mutex_ncbicxx_3_6_5
#define mbedtls_threading_readdir_mutex \
        mbedtls_threading_readdir_mutex_ncbicxx_3_6_5
#define mbedtls_timing_get_delay \
        mbedtls_timing_get_delay_ncbicxx_3_6_5
#define mbedtls_timing_get_final_delay \
        mbedtls_timing_get_final_delay_ncbicxx_3_6_5
#define mbedtls_timing_get_timer \
        mbedtls_timing_get_timer_ncbicxx_3_6_5
#define mbedtls_timing_set_delay \
        mbedtls_timing_set_delay_ncbicxx_3_6_5
#define mbedtls_version_get_number \
        mbedtls_version_get_number_ncbicxx_3_6_5
#define mbedtls_version_get_string \
        mbedtls_version_get_string_ncbicxx_3_6_5
#define mbedtls_version_get_string_full \
        mbedtls_version_get_string_full_ncbicxx_3_6_5
#define mbedtls_version_check_feature \
        mbedtls_version_check_feature_ncbicxx_3_6_5
#define mbedtls_x509_dn_gets \
        mbedtls_x509_dn_gets_ncbicxx_3_6_5
#define mbedtls_x509_free_subject_alt_name \
        mbedtls_x509_free_subject_alt_name_ncbicxx_3_6_5
#define mbedtls_x509_get_alg \
        mbedtls_x509_get_alg_ncbicxx_3_6_5
#define mbedtls_x509_get_alg_null \
        mbedtls_x509_get_alg_null_ncbicxx_3_6_5
#define mbedtls_x509_get_ext \
        mbedtls_x509_get_ext_ncbicxx_3_6_5
#define mbedtls_x509_get_key_usage \
        mbedtls_x509_get_key_usage_ncbicxx_3_6_5
#define mbedtls_x509_get_name \
        mbedtls_x509_get_name_ncbicxx_3_6_5
#define mbedtls_x509_get_ns_cert_type \
        mbedtls_x509_get_ns_cert_type_ncbicxx_3_6_5
#define mbedtls_x509_get_rsassa_pss_params \
        mbedtls_x509_get_rsassa_pss_params_ncbicxx_3_6_5
#define mbedtls_x509_get_serial \
        mbedtls_x509_get_serial_ncbicxx_3_6_5
#define mbedtls_x509_get_sig \
        mbedtls_x509_get_sig_ncbicxx_3_6_5
#define mbedtls_x509_get_sig_alg \
        mbedtls_x509_get_sig_alg_ncbicxx_3_6_5
#define mbedtls_x509_get_subject_alt_name \
        mbedtls_x509_get_subject_alt_name_ncbicxx_3_6_5
#define mbedtls_x509_get_subject_alt_name_ext \
        mbedtls_x509_get_subject_alt_name_ext_ncbicxx_3_6_5
#define mbedtls_x509_get_time \
        mbedtls_x509_get_time_ncbicxx_3_6_5
#define mbedtls_x509_info_cert_type \
        mbedtls_x509_info_cert_type_ncbicxx_3_6_5
#define mbedtls_x509_info_key_usage \
        mbedtls_x509_info_key_usage_ncbicxx_3_6_5
#define mbedtls_x509_info_subject_alt_name \
        mbedtls_x509_info_subject_alt_name_ncbicxx_3_6_5
#define mbedtls_x509_key_size_helper \
        mbedtls_x509_key_size_helper_ncbicxx_3_6_5
#define mbedtls_x509_parse_subject_alt_name \
        mbedtls_x509_parse_subject_alt_name_ncbicxx_3_6_5
#define mbedtls_x509_serial_gets \
        mbedtls_x509_serial_gets_ncbicxx_3_6_5
#define mbedtls_x509_sig_alg_gets \
        mbedtls_x509_sig_alg_gets_ncbicxx_3_6_5
#define mbedtls_x509_time_cmp \
        mbedtls_x509_time_cmp_ncbicxx_3_6_5
#define mbedtls_x509_time_gmtime \
        mbedtls_x509_time_gmtime_ncbicxx_3_6_5
#define mbedtls_x509_time_is_future \
        mbedtls_x509_time_is_future_ncbicxx_3_6_5
#define mbedtls_x509_time_is_past \
        mbedtls_x509_time_is_past_ncbicxx_3_6_5
#define mbedtls_x509_set_extension \
        mbedtls_x509_set_extension_ncbicxx_3_6_5
#define mbedtls_x509_string_to_names \
        mbedtls_x509_string_to_names_ncbicxx_3_6_5
#define mbedtls_x509_write_extensions \
        mbedtls_x509_write_extensions_ncbicxx_3_6_5
#define mbedtls_x509_write_names \
        mbedtls_x509_write_names_ncbicxx_3_6_5
#define mbedtls_x509_write_sig \
        mbedtls_x509_write_sig_ncbicxx_3_6_5
#define mbedtls_x509_crl_free \
        mbedtls_x509_crl_free_ncbicxx_3_6_5
#define mbedtls_x509_crl_info \
        mbedtls_x509_crl_info_ncbicxx_3_6_5
#define mbedtls_x509_crl_init \
        mbedtls_x509_crl_init_ncbicxx_3_6_5
#define mbedtls_x509_crl_parse \
        mbedtls_x509_crl_parse_ncbicxx_3_6_5
#define mbedtls_x509_crl_parse_der \
        mbedtls_x509_crl_parse_der_ncbicxx_3_6_5
#define mbedtls_x509_crl_parse_file \
        mbedtls_x509_crl_parse_file_ncbicxx_3_6_5
#define mbedtls_x509_crt_check_extended_key_usage \
        mbedtls_x509_crt_check_extended_key_usage_ncbicxx_3_6_5
#define mbedtls_x509_crt_check_key_usage \
        mbedtls_x509_crt_check_key_usage_ncbicxx_3_6_5
#define mbedtls_x509_crt_free \
        mbedtls_x509_crt_free_ncbicxx_3_6_5
#define mbedtls_x509_crt_get_ca_istrue \
        mbedtls_x509_crt_get_ca_istrue_ncbicxx_3_6_5
#define mbedtls_x509_crt_info \
        mbedtls_x509_crt_info_ncbicxx_3_6_5
#define mbedtls_x509_crt_init \
        mbedtls_x509_crt_init_ncbicxx_3_6_5
#define mbedtls_x509_crt_is_revoked \
        mbedtls_x509_crt_is_revoked_ncbicxx_3_6_5
#define mbedtls_x509_crt_parse \
        mbedtls_x509_crt_parse_ncbicxx_3_6_5
#define mbedtls_x509_crt_parse_cn_inet_pton \
        mbedtls_x509_crt_parse_cn_inet_pton_ncbicxx_3_6_5
#define mbedtls_x509_crt_parse_der \
        mbedtls_x509_crt_parse_der_ncbicxx_3_6_5
#define mbedtls_x509_crt_parse_der_nocopy \
        mbedtls_x509_crt_parse_der_nocopy_ncbicxx_3_6_5
#define mbedtls_x509_crt_parse_der_with_ext_cb \
        mbedtls_x509_crt_parse_der_with_ext_cb_ncbicxx_3_6_5
#define mbedtls_x509_crt_parse_file \
        mbedtls_x509_crt_parse_file_ncbicxx_3_6_5
#define mbedtls_x509_crt_parse_path \
        mbedtls_x509_crt_parse_path_ncbicxx_3_6_5
#define mbedtls_x509_crt_profile_default \
        mbedtls_x509_crt_profile_default_ncbicxx_3_6_5
#define mbedtls_x509_crt_profile_next \
        mbedtls_x509_crt_profile_next_ncbicxx_3_6_5
#define mbedtls_x509_crt_profile_none \
        mbedtls_x509_crt_profile_none_ncbicxx_3_6_5
#define mbedtls_x509_crt_profile_suiteb \
        mbedtls_x509_crt_profile_suiteb_ncbicxx_3_6_5
#define mbedtls_x509_crt_verify \
        mbedtls_x509_crt_verify_ncbicxx_3_6_5
#define mbedtls_x509_crt_verify_info \
        mbedtls_x509_crt_verify_info_ncbicxx_3_6_5
#define mbedtls_x509_crt_verify_restartable \
        mbedtls_x509_crt_verify_restartable_ncbicxx_3_6_5
#define mbedtls_x509_crt_verify_with_profile \
        mbedtls_x509_crt_verify_with_profile_ncbicxx_3_6_5
#define mbedtls_x509_csr_free \
        mbedtls_x509_csr_free_ncbicxx_3_6_5
#define mbedtls_x509_csr_info \
        mbedtls_x509_csr_info_ncbicxx_3_6_5
#define mbedtls_x509_csr_init \
        mbedtls_x509_csr_init_ncbicxx_3_6_5
#define mbedtls_x509_csr_parse \
        mbedtls_x509_csr_parse_ncbicxx_3_6_5
#define mbedtls_x509_csr_parse_der \
        mbedtls_x509_csr_parse_der_ncbicxx_3_6_5
#define mbedtls_x509_csr_parse_der_with_ext_cb \
        mbedtls_x509_csr_parse_der_with_ext_cb_ncbicxx_3_6_5
#define mbedtls_x509_csr_parse_file \
        mbedtls_x509_csr_parse_file_ncbicxx_3_6_5
#define mbedtls_x509_write_set_san_common \
        mbedtls_x509_write_set_san_common_ncbicxx_3_6_5
#define mbedtls_x509write_crt_der \
        mbedtls_x509write_crt_der_ncbicxx_3_6_5
#define mbedtls_x509write_crt_free \
        mbedtls_x509write_crt_free_ncbicxx_3_6_5
#define mbedtls_x509write_crt_init \
        mbedtls_x509write_crt_init_ncbicxx_3_6_5
#define mbedtls_x509write_crt_pem \
        mbedtls_x509write_crt_pem_ncbicxx_3_6_5
#define mbedtls_x509write_crt_set_authority_key_identifier \
        mbedtls_x509write_crt_set_authority_key_identifier_ncbicxx_3_6_5
#define mbedtls_x509write_crt_set_basic_constraints \
        mbedtls_x509write_crt_set_basic_constraints_ncbicxx_3_6_5
#define mbedtls_x509write_crt_set_ext_key_usage \
        mbedtls_x509write_crt_set_ext_key_usage_ncbicxx_3_6_5
#define mbedtls_x509write_crt_set_extension \
        mbedtls_x509write_crt_set_extension_ncbicxx_3_6_5
#define mbedtls_x509write_crt_set_issuer_key \
        mbedtls_x509write_crt_set_issuer_key_ncbicxx_3_6_5
#define mbedtls_x509write_crt_set_issuer_name \
        mbedtls_x509write_crt_set_issuer_name_ncbicxx_3_6_5
#define mbedtls_x509write_crt_set_key_usage \
        mbedtls_x509write_crt_set_key_usage_ncbicxx_3_6_5
#define mbedtls_x509write_crt_set_md_alg \
        mbedtls_x509write_crt_set_md_alg_ncbicxx_3_6_5
#define mbedtls_x509write_crt_set_ns_cert_type \
        mbedtls_x509write_crt_set_ns_cert_type_ncbicxx_3_6_5
#define mbedtls_x509write_crt_set_serial \
        mbedtls_x509write_crt_set_serial_ncbicxx_3_6_5
#define mbedtls_x509write_crt_set_serial_raw \
        mbedtls_x509write_crt_set_serial_raw_ncbicxx_3_6_5
#define mbedtls_x509write_crt_set_subject_alternative_name \
        mbedtls_x509write_crt_set_subject_alternative_name_ncbicxx_3_6_5
#define mbedtls_x509write_crt_set_subject_key \
        mbedtls_x509write_crt_set_subject_key_ncbicxx_3_6_5
#define mbedtls_x509write_crt_set_subject_key_identifier \
        mbedtls_x509write_crt_set_subject_key_identifier_ncbicxx_3_6_5
#define mbedtls_x509write_crt_set_subject_name \
        mbedtls_x509write_crt_set_subject_name_ncbicxx_3_6_5
#define mbedtls_x509write_crt_set_validity \
        mbedtls_x509write_crt_set_validity_ncbicxx_3_6_5
#define mbedtls_x509write_crt_set_version \
        mbedtls_x509write_crt_set_version_ncbicxx_3_6_5
#define mbedtls_x509write_csr_der \
        mbedtls_x509write_csr_der_ncbicxx_3_6_5
#define mbedtls_x509write_csr_free \
        mbedtls_x509write_csr_free_ncbicxx_3_6_5
#define mbedtls_x509write_csr_init \
        mbedtls_x509write_csr_init_ncbicxx_3_6_5
#define mbedtls_x509write_csr_pem \
        mbedtls_x509write_csr_pem_ncbicxx_3_6_5
#define mbedtls_x509write_csr_set_extension \
        mbedtls_x509write_csr_set_extension_ncbicxx_3_6_5
#define mbedtls_x509write_csr_set_key \
        mbedtls_x509write_csr_set_key_ncbicxx_3_6_5
#define mbedtls_x509write_csr_set_key_usage \
        mbedtls_x509write_csr_set_key_usage_ncbicxx_3_6_5
#define mbedtls_x509write_csr_set_md_alg \
        mbedtls_x509write_csr_set_md_alg_ncbicxx_3_6_5
#define mbedtls_x509write_csr_set_ns_cert_type \
        mbedtls_x509write_csr_set_ns_cert_type_ncbicxx_3_6_5
#define mbedtls_x509write_csr_set_subject_alternative_name \
        mbedtls_x509write_csr_set_subject_alternative_name_ncbicxx_3_6_5
#define mbedtls_x509write_csr_set_subject_name \
        mbedtls_x509write_csr_set_subject_name_ncbicxx_3_6_5
/* platform-/configuration-specific additions */
#define mbedtls_ct_zero \
        mbedtls_ct_zero_ncbicxx_3_6_5
#define mbedtls_platform_set_snprintf \
        mbedtls_platform_set_snprintf_ncbicxx_3_6_5
#define mbedtls_platform_set_vsnprintf \
        mbedtls_platform_set_vsnprintf_ncbicxx_3_6_5
#define mbedtls_snprintf \
        mbedtls_snprintf_ncbicxx_3_6_5
#define mbedtls_vsnprintf \
        mbedtls_vsnprintf_ncbicxx_3_6_5
#define mbedtls_threading_free_alt \
        mbedtls_threading_free_alt_ncbicxx_3_6_5
#define mbedtls_threading_set_alt \
        mbedtls_threading_set_alt_ncbicxx_3_6_5
