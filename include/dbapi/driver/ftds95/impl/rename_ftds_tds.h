#ifndef DBAPI_DRIVER_FTDS95_IMPL___RENAME_FTDS_TDS__H
#define DBAPI_DRIVER_FTDS95_IMPL___RENAME_FTDS_TDS__H

/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Macros to rename FreeTDS TDS symbols -- to avoid their clashing
 *   with other versions'
 *
 */

#define STD_DATETIME_FMT                STD_DATETIME_FMT_ver95
#define determine_adjusted_size         determine_adjusted_size_ver95
#define hmac_md5                        hmac_md5_ver95
#define tds71_submit_prepexec           tds71_submit_prepexec_ver95
#define tds7_crypt_pass                 tds7_crypt_pass_ver95
#define tds7_get_instance_port          tds7_get_instance_port_ver95
#define tds7_get_instance_ports         tds7_get_instance_ports_ver95
#define tds7_srv_charset_changed        tds7_srv_charset_changed_ver95
#define tds_addrinfo2str                tds_addrinfo2str_ver95
#define tds_alloc_bcp_column_data       tds_alloc_bcp_column_data_ver95
#define tds_alloc_client_sqlstate       tds_alloc_client_sqlstate_ver95
#define tds_alloc_compute_results       tds_alloc_compute_results_ver95
#define tds_alloc_compute_row           tds_alloc_compute_row_ver95
#define tds_alloc_connection            tds_alloc_connection_ver95
#define tds_alloc_context               tds_alloc_context_ver95
#define tds_alloc_cursor                tds_alloc_cursor_ver95
#define tds_alloc_dynamic               tds_alloc_dynamic_ver95
#define tds_alloc_locale                tds_alloc_locale_ver95
#define tds_alloc_login                 tds_alloc_login_ver95
#define tds_alloc_lookup_sqlstate       tds_alloc_lookup_sqlstate_ver95
#define tds_alloc_packet                tds_alloc_packet_ver95
#define tds_alloc_param_data            tds_alloc_param_data_ver95
#define tds_alloc_param_result          tds_alloc_param_result_ver95
#define tds_alloc_results               tds_alloc_results_ver95
#define tds_alloc_row                   tds_alloc_row_ver95
#define tds_alloc_socket                tds_alloc_socket_ver95
#define tds_bcp_done                    tds_bcp_done_ver95
#define tds_bcp_fread                   tds_bcp_fread_ver95
#define tds_bcp_init                    tds_bcp_init_ver95
#define tds_bcp_send_record             tds_bcp_send_record_ver95
#define tds_bcp_start                   tds_bcp_start_ver95
#define tds_bcp_start_copy_in           tds_bcp_start_copy_in_ver95
#define tds_canonical_charset_name      tds_canonical_charset_name_ver95
#define tds_char2hex                    tds_char2hex_ver95
#define tds_check_column_extra          tds_check_column_extra_ver95
#define tds_check_context_extra         tds_check_context_extra_ver95
#define tds_check_cursor_extra          tds_check_cursor_extra_ver95
#define tds_check_dynamic_extra         tds_check_dynamic_extra_ver95
#define tds_check_env_extra             tds_check_env_extra_ver95
#define tds_check_resultinfo_extra      tds_check_resultinfo_extra_ver95
#define tds_check_tds_extra             tds_check_tds_extra_ver95
#define tds_close_socket                tds_close_socket_ver95
#define tds_clrudt_funcs                tds_clrudt_funcs_ver95
#define tds_clrudt_get_info             tds_clrudt_get_info_ver95
#define tds_clrudt_put_info             tds_clrudt_put_info_ver95
#define tds_clrudt_row_len              tds_clrudt_row_len_ver95
#define tds_config_boolean              tds_config_boolean_ver95
#define tds_config_verstr               tds_config_verstr_ver95
#define tds_connect_and_login           tds_connect_and_login_ver95
#define tds_connection_read             tds_connection_read_ver95
#define tds_connection_write            tds_connection_write_ver95
#define tds_convert                     tds_convert_ver95
#define tds_convert_stream              tds_convert_stream_ver95
#define tds_convert_string              tds_convert_string_ver95
#if ENABLE_EXTRA_CHECKS
#  define tds_convert_string_free         tds_convert_string_free_ver95
#endif
#define tds_copy_stream                 tds_copy_stream_ver95
#define tds_count_placeholders          tds_count_placeholders_ver95
#define tds_cursor_close                tds_cursor_close_ver95
#define tds_cursor_dealloc              tds_cursor_dealloc_ver95
#define tds_cursor_deallocated          tds_cursor_deallocated_ver95
#define tds_cursor_declare              tds_cursor_declare_ver95
#define tds_cursor_fetch                tds_cursor_fetch_ver95
#define tds_cursor_get_cursor_info      tds_cursor_get_cursor_info_ver95
#define tds_cursor_open                 tds_cursor_open_ver95
#define tds_cursor_setname              tds_cursor_setname_ver95
#define tds_cursor_setrows              tds_cursor_setrows_ver95
#define tds_cursor_update               tds_cursor_update_ver95
#define tds_datain_stream_init          tds_datain_stream_init_ver95
#define tds_dataout_stream_init         tds_dataout_stream_init_ver95
#define tds_datecrack                   tds_datecrack_ver95
#define tds_debug_flags                 tds_debug_flags_ver95
#define tds_deferred_cursor_dealloc     tds_deferred_cursor_dealloc_ver95
#define tds_deferred_unprepare          tds_deferred_unprepare_ver95
#define tds_des_ecb_encrypt             tds_des_ecb_encrypt_ver95
#define tds_des_encrypt                 tds_des_encrypt_ver95
#define tds_des_set_key                 tds_des_set_key_ver95
#define tds_des_set_odd_parity          tds_des_set_odd_parity_ver95
#define tds_detach_results              tds_detach_results_ver95
#define tds_disconnect                  tds_disconnect_ver95
#define tds_dstr_alloc                  tds_dstr_alloc_ver95
#define tds_dstr_copy                   tds_dstr_copy_ver95
#define tds_dstr_copyn                  tds_dstr_copyn_ver95
#define tds_dstr_dup                    tds_dstr_dup_ver95
#define tds_dstr_free                   tds_dstr_free_ver95
#define tds_dstr_get                    tds_dstr_get_ver95
#if ENABLE_EXTRA_CHECKS
#  define tds_dstr_buf                    tds_dstr_buf_ver95
#  define tds_dstr_init                   tds_dstr_init_ver95
#  define tds_dstr_isempty                tds_dstr_isempty_ver95
#  define tds_dstr_len                    tds_dstr_len_ver95
#endif
#define tds_dstr_set                    tds_dstr_set_ver95
#define tds_dstr_setlen                 tds_dstr_setlen_ver95
#define tds_dstr_zero                   tds_dstr_zero_ver95
#define tds_dynamic_deallocated         tds_dynamic_deallocated_ver95
#define tds_dynamic_stream_init         tds_dynamic_stream_init_ver95
#define tds_fix_column_size             tds_fix_column_size_ver95
#define tds_fix_login                   tds_fix_login_ver95
#define tds_flush_packet                tds_flush_packet_ver95
#define tds_free_all_results            tds_free_all_results_ver95
#define tds_free_bcp_column_data        tds_free_bcp_column_data_ver95
#define tds_free_context                tds_free_context_ver95
#define tds_free_input_params           tds_free_input_params_ver95
#define tds_free_locale                 tds_free_locale_ver95
#define tds_free_login                  tds_free_login_ver95
#define tds_free_msg                    tds_free_msg_ver95
#define tds_free_packets                tds_free_packets_ver95
#define tds_free_param_result           tds_free_param_result_ver95
#define tds_free_param_results          tds_free_param_results_ver95
#define tds_free_results                tds_free_results_ver95
#define tds_free_row                    tds_free_row_ver95
#define tds_free_socket                 tds_free_socket_ver95
#define tds_g_append_mode               tds_g_append_mode_ver95
#define tds_generic_funcs               tds_generic_funcs_ver95
#define tds_generic_get                 tds_generic_get_ver95
#define tds_generic_get_info            tds_generic_get_info_ver95
#define tds_generic_put                 tds_generic_put_ver95
#define tds_generic_put_info            tds_generic_put_info_ver95
#define tds_generic_row_len             tds_generic_row_len_ver95
#define tds_get_byte                    tds_get_byte_ver95
#define tds_get_cardinal_type           tds_get_cardinal_type_ver95
#define tds_get_char_data               tds_get_char_data_ver95
#define tds_get_column_declaration      tds_get_column_declaration_ver95
#define tds_get_compiletime_settings    tds_get_compiletime_settings_ver95
#define tds_get_conversion_type         tds_get_conversion_type_ver95
#define tds_get_homedir                 tds_get_homedir_ver95
#define tds_get_locale                  tds_get_locale_ver95
#define tds_get_n                       tds_get_n_ver95
#define tds_get_null_type               tds_get_null_type_ver95
#define tds_get_size_by_type            tds_get_size_by_type_ver95
#define tds_get_string                  tds_get_string_ver95
#define tds_get_token_size              tds_get_token_size_ver95
#define tds_get_uint                    tds_get_uint_ver95
#define tds_get_uint8                   tds_get_uint8_ver95
#define tds_get_usmallint               tds_get_usmallint_ver95
#define tds_get_varint_size             tds_get_varint_size_ver95
#define tds_getmac                      tds_getmac_ver95
#define tds_getservice                  tds_getservice_ver95
#define tds_goodread                    tds_goodread_ver95
#define tds_goodwrite                   tds_goodwrite_ver95
#define tds_gss_get_auth                tds_gss_get_auth_ver95
#define tds_hex_digits                  tds_hex_digits_ver95
#define tds_iconv                       tds_iconv_ver95
#define tds_iconv_alloc                 tds_iconv_alloc_ver95
#define tds_iconv_close                 tds_iconv_close_ver95
#define tds_iconv_fread                 tds_iconv_fread_ver95
#define tds_iconv_free                  tds_iconv_free_ver95
#define tds_iconv_from_collate          tds_iconv_from_collate_ver95
#define tds_iconv_get                   tds_iconv_get_ver95
#define tds_iconv_open                  tds_iconv_open_ver95
#define tds_init_login                  tds_init_login_ver95
#define tds_init_write_buf              tds_init_write_buf_ver95
#define tds_localtime_r                 tds_localtime_r_ver95
#define tds_lookup_dynamic              tds_lookup_dynamic_ver95
#define tds_lookup_host                 tds_lookup_host_ver95
#define tds_lookup_host_set             tds_lookup_host_set_ver95
#define tds_money_to_string             tds_money_to_string_ver95
#define tds_msdatetime_funcs            tds_msdatetime_funcs_ver95
#define tds_msdatetime_get              tds_msdatetime_get_ver95
#define tds_msdatetime_get_info         tds_msdatetime_get_info_ver95
#define tds_msdatetime_put              tds_msdatetime_put_ver95
#define tds_msdatetime_put_info         tds_msdatetime_put_info_ver95
#define tds_msdatetime_row_len          tds_msdatetime_row_len_ver95
#define tds_multiple_done               tds_multiple_done_ver95
#define tds_multiple_execute            tds_multiple_execute_ver95
#define tds_multiple_init               tds_multiple_init_ver95
#define tds_multiple_query              tds_multiple_query_ver95
#define tds_needs_unprepare             tds_needs_unprepare_ver95
#define tds_next_placeholder            tds_next_placeholder_ver95
#define tds_ntlm_get_auth               tds_ntlm_get_auth_ver95
#define tds_numeric_bytes_per_prec      tds_numeric_bytes_per_prec_ver95
#define tds_numeric_change_prec_scale   tds_numeric_change_prec_scale_ver95
#define tds_numeric_funcs               tds_numeric_funcs_ver95
#define tds_numeric_get                 tds_numeric_get_ver95
#define tds_numeric_get_info            tds_numeric_get_info_ver95
#define tds_numeric_put                 tds_numeric_put_ver95
#define tds_numeric_put_info            tds_numeric_put_info_ver95
#define tds_numeric_row_len             tds_numeric_row_len_ver95
#define tds_numeric_to_string           tds_numeric_to_string_ver95
#define tds_open_socket                 tds_open_socket_ver95
#define tds_parse_conf_section          tds_parse_conf_section_ver95
#define tds_peek                        tds_peek_ver95
#define tds_process_cancel              tds_process_cancel_ver95
#define tds_process_login_tokens        tds_process_login_tokens_ver95
#define tds_process_simple_query        tds_process_simple_query_ver95
#define tds_process_tokens              tds_process_tokens_ver95
#define tds_prtype                      tds_prtype_ver95
#define tds_put_buf                     tds_put_buf_ver95
#define tds_put_byte                    tds_put_byte_ver95
#define tds_put_cancel                  tds_put_cancel_ver95
#define tds_put_int                     tds_put_int_ver95
#define tds_put_int8                    tds_put_int8_ver95
#define tds_put_n                       tds_put_n_ver95
#define tds_put_smallint                tds_put_smallint_ver95
#define tds_put_string                  tds_put_string_ver95
#define tds_quote_id                    tds_quote_id_ver95
#define tds_quote_string                tds_quote_string_ver95
#define tds_read_conf_file              tds_read_conf_file_ver95
#define tds_read_conf_section           tds_read_conf_section_ver95
#define tds_read_config_info            tds_read_config_info_ver95
#define tds_read_packet                 tds_read_packet_ver95
#define tds_realloc                     tds_realloc_ver95
#define tds_realloc_packet              tds_realloc_packet_ver95
#define tds_realloc_socket              tds_realloc_socket_ver95
#define tds_release_cursor              tds_release_cursor_ver95
#define tds_release_dynamic             tds_release_dynamic_ver95
#define tds_select                      tds_select_ver95
#define tds_send_cancel                 tds_send_cancel_ver95
#define tds_set_app                     tds_set_app_ver95
#define tds_set_bulk                    tds_set_bulk_ver95
#define tds_set_client_charset          tds_set_client_charset_ver95
#define tds_set_column_type             tds_set_column_type_ver95
#define tds_set_cur_dyn                 tds_set_cur_dyn_ver95
#define tds_set_current_results         tds_set_current_results_ver95
#define tds_set_host                    tds_set_host_ver95
#define tds_set_interfaces_file_loc     tds_set_interfaces_file_loc_ver95
#define tds_set_language                tds_set_language_ver95
#define tds_set_library                 tds_set_library_ver95
#define tds_set_packet                  tds_set_packet_ver95
#define tds_set_param_type              tds_set_param_type_ver95
#define tds_set_passwd                  tds_set_passwd_ver95
#define tds_set_port                    tds_set_port_ver95
#define tds_set_server                  tds_set_server_ver95
#define tds_set_state                   tds_set_state_ver95
#define tds_set_user                    tds_set_user_ver95
#define tds_set_version                 tds_set_version_ver95
#define tds_skip_comment                tds_skip_comment_ver95
#define tds_skip_quoted                 tds_skip_quoted_ver95
#define tds_sleep_ms                    tds_sleep_ms_ver95
#define tds_sleep_s                     tds_sleep_s_ver95
#define tds_srv_charset_changed         tds_srv_charset_changed_ver95
#ifdef NCBI_FTDS_ALLOW_TDS_80
#  define tds_ssl_deinit                  tds_ssl_deinit_ver95
#  define tds_ssl_init                    tds_ssl_init_ver95
#endif
#define tds_staticin_stream_init        tds_staticin_stream_init_ver95
#define tds_staticout_stream_init       tds_staticout_stream_init_ver95
#define tds_str_empty                   tds_str_empty_ver95
#define tds_strftime                    tds_strftime_ver95
#define tds_submit_begin_tran           tds_submit_begin_tran_ver95
#define tds_submit_commit               tds_submit_commit_ver95
#define tds_submit_execdirect           tds_submit_execdirect_ver95
#define tds_submit_execute              tds_submit_execute_ver95
#define tds_submit_optioncmd            tds_submit_optioncmd_ver95
#define tds_submit_prepare              tds_submit_prepare_ver95
#define tds_submit_query                tds_submit_query_ver95
#define tds_submit_query_params         tds_submit_query_params_ver95
#define tds_submit_queryf               tds_submit_queryf_ver95
#define tds_submit_rollback             tds_submit_rollback_ver95
#define tds_submit_rpc                  tds_submit_rpc_ver95
#define tds_submit_unprepare            tds_submit_unprepare_ver95
#define tds_swap_bytes                  tds_swap_bytes_ver95
#define tds_timestamp_str               tds_timestamp_str_ver95
#define tds_type_flags_ms               tds_type_flags_ms_ver95
#define tds_unget_byte                  tds_unget_byte_ver95
#define tds_variant_funcs               tds_variant_funcs_ver95
#define tds_variant_get                 tds_variant_get_ver95
#define tds_variant_put                 tds_variant_put_ver95
#define tds_variant_put_info            tds_variant_put_info_ver95
#define tds_vstrbuild                   tds_vstrbuild_ver95
#define tds_willconvert                 tds_willconvert_ver95
#define tds_write_dump                  tds_write_dump_ver95
#define tds_write_packet                tds_write_packet_ver95
#define tds_writetext_continue          tds_writetext_continue_ver95
#define tds_writetext_end               tds_writetext_end_ver95
#define tds_writetext_start             tds_writetext_start_ver95
#define tdsdump_close                   tdsdump_close_ver95
#define tdsdump_col                     tdsdump_col_ver95
#define tdsdump_do_dump_buf             tdsdump_do_dump_buf_ver95
#define tdsdump_do_log                  tdsdump_do_log_ver95
#define tdsdump_isopen                  tdsdump_isopen_ver95
#define tdsdump_off                     tdsdump_off_ver95
#define tdsdump_on                      tdsdump_on_ver95
#define tdsdump_open                    tdsdump_open_ver95
#define tdsdump_state                   tdsdump_state_ver95
#define tdserror                        tdserror_ver95

#endif  /* DBAPI_DRIVER_FTDS95_IMPL___RENAME_FTDS_TDS__H */
