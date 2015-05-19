#ifndef DBAPI_DRIVER_CTLIB___NCBI_DBAPI_RENAME_CTLIB__H
#define DBAPI_DRIVER_CTLIB___NCBI_DBAPI_RENAME_CTLIB__H

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
 * Author:  Sergey Sikorskiy
 *
 * File Description:
 *   Macros to rename FreeTDS CTLIB symbols -- to avoid their clashing
 *   with the Sybase CTLIB ones
 *
 */


#if defined(NCBI_DBAPI_RENAME_CTLIB)

#  define cs_convert        cs_convert_ver64
#  define cs_ctx_alloc      cs_ctx_alloc_ver64
#  define cs_ctx_global     cs_ctx_global_ver64
#  define cs_ctx_drop       cs_ctx_drop_ver64
#  define cs_config         cs_config_ver64
#  define cs_strbuild       cs_strbuild_ver64
#  define cs_dt_crack       cs_dt_crack_ver64
#  define cs_loc_alloc      cs_loc_alloc_ver64
#  define cs_loc_drop       cs_loc_drop_ver64
#  define cs_locale         cs_locale_ver64
#  define cs_dt_info        cs_dt_info_ver64
#  define cs_calc           cs_calc_ver64
#  define cs_cmp            cs_cmp_ver64
#  define cs_conv_mult      cs_conv_mult_ver64
#  define cs_diag           cs_diag_ver64
#  define cs_manage_convert cs_manage_convert_ver64
#  define cs_objects        cs_objects_ver64
#  define cs_set_convert    cs_set_convert_ver64
#  define cs_setnull        cs_setnull_ver64
#  define cs_strcmp         cs_strcmp_ver64
#  define cs_time           cs_time_ver64
#  define cs_will_convert   cs_will_convert_ver64
#  define ct_init           ct_init_ver64
#  define ct_con_alloc      ct_con_alloc_ver64
#  define ct_con_props      ct_con_props_ver64
#  define ct_connect        ct_connect_ver64
#  define ct_cmd_alloc      ct_cmd_alloc_ver64
#  define ct_cancel         ct_cancel_ver64
#  define ct_cmd_drop       ct_cmd_drop_ver64
#  define ct_close          ct_close_ver64
#  define ct_con_drop       ct_con_drop_ver64
#  define ct_exit           ct_exit_ver64
#  define ct_command        ct_command_ver64
#  define ct_send           ct_send_ver64
#  define ct_results        ct_results_ver64
#  define ct_bind           ct_bind_ver64
#  define ct_fetch          ct_fetch_ver64
#  define ct_res_info_dyn   ct_res_info_dyn_ver64
#  define ct_res_info       ct_res_info_ver64
#  define ct_describe       ct_describe_ver64
#  define ct_callback       ct_callback_ver64
#  define ct_send_dyn       ct_send_dyn_ver64
#  define ct_results_dyn    ct_results_dyn_ver64
#  define ct_config         ct_config_ver64
#  define ct_cmd_props      ct_cmd_props_ver64
#  define ct_compute_info   ct_compute_info_ver64
#  define ct_get_data       ct_get_data_ver64
#  define ct_send_data      ct_send_data_ver64
#  define ct_data_info      ct_data_info_ver64
#  define ct_capability     ct_capability_ver64
#  define ct_dynamic        ct_dynamic_ver64
#  define ct_param          ct_param_ver64
#  define ct_setparam       ct_setparam_ver64
#  define ct_options        ct_options_ver64
#  define ct_poll           ct_poll_ver64
#  define ct_cursor         ct_cursor_ver64
#  define ct_diag           ct_diag_ver64
#  define blk_alloc         blk_alloc_ver64
#  define blk_bind          blk_bind_ver64
#  define blk_colval        blk_colval_ver64
#  define blk_default       blk_default_ver64
#  define blk_describe      blk_describe_ver64
#  define blk_done          blk_done_ver64
#  define blk_drop          blk_drop_ver64
#  define blk_getrow        blk_getrow_ver64
#  define blk_gettext       blk_gettext_ver64
#  define blk_init          blk_init_ver64
#  define blk_props         blk_props_ver64
#  define blk_sethints      blk_sethints_ver64
#  define blk_rowalloc      blk_rowalloc_ver64
#  define blk_rowdrop       blk_rowdrop_ver64
#  define blk_rowxfer       blk_rowxfer_ver64
#  define blk_rowxfer_mult  blk_rowxfer_mult_ver64
#  define blk_sendrow       blk_sendrow_ver64
#  define blk_sendtext      blk_sendtext_ver64
#  define blk_srvinit       blk_srvinit_ver64
#  define blk_textxfer      blk_textxfer_ver64

/* Also cover type names that appear as template parameters */
#  define _cs_datafmt _cs_datafmt_ver64
#  define _cs_varchar _cs_varchar_ver64

#endif  /* NCBI_DBAPI_RENAME_CTLIB */


#endif  /* DBAPI_DRIVER_CTLIB___NCBI_DBAPI_RENAME_CTLIB__H */
