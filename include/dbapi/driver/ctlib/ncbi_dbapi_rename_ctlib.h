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

#  define cs_convert        NCBI_FTDS_cs_convert
#  define cs_ctx_alloc      NCBI_FTDS_cs_ctx_alloc
#  define cs_ctx_global     NCBI_FTDS_cs_ctx_global
#  define cs_ctx_drop       NCBI_FTDS_cs_ctx_drop
#  define cs_config         NCBI_FTDS_cs_config
#  define cs_strbuild       NCBI_FTDS_cs_strbuild
#  define cs_dt_crack       NCBI_FTDS_cs_dt_crack
#  define cs_loc_alloc      NCBI_FTDS_cs_loc_alloc
#  define cs_loc_drop       NCBI_FTDS_cs_loc_drop
#  define cs_locale         NCBI_FTDS_cs_locale
#  define cs_dt_info        NCBI_FTDS_cs_dt_info
#  define cs_calc           NCBI_FTDS_cs_calc
#  define cs_cmp            NCBI_FTDS_cs_cmp
#  define cs_conv_mult      NCBI_FTDS_cs_conv_mult
#  define cs_diag           NCBI_FTDS_cs_diag
#  define cs_manage_convert NCBI_FTDS_cs_manage_convert
#  define cs_objects        NCBI_FTDS_cs_objects
#  define cs_set_convert    NCBI_FTDS_cs_set_convert
#  define cs_setnull        NCBI_FTDS_cs_setnull
#  define cs_strcmp         NCBI_FTDS_cs_strcmp
#  define cs_time           NCBI_FTDS_cs_time
#  define cs_will_convert   NCBI_FTDS_cs_will_convert
#  define ct_init           NCBI_FTDS_ct_init
#  define ct_con_alloc      NCBI_FTDS_ct_con_alloc
#  define ct_con_props      NCBI_FTDS_ct_con_props
#  define ct_connect        NCBI_FTDS_ct_connect
#  define ct_cmd_alloc      NCBI_FTDS_ct_cmd_alloc
#  define ct_cancel         NCBI_FTDS_ct_cancel
#  define ct_cmd_drop       NCBI_FTDS_ct_cmd_drop
#  define ct_close          NCBI_FTDS_ct_close
#  define ct_con_drop       NCBI_FTDS_ct_con_drop
#  define ct_exit           NCBI_FTDS_ct_exit
#  define ct_command        NCBI_FTDS_ct_command
#  define ct_send           NCBI_FTDS_ct_send
#  define ct_results        NCBI_FTDS_ct_results
#  define ct_bind           NCBI_FTDS_ct_bind
#  define ct_fetch          NCBI_FTDS_ct_fetch
#  define ct_res_info_dyn   NCBI_FTDS_ct_res_info_dyn
#  define ct_res_info       NCBI_FTDS_ct_res_info
#  define ct_describe       NCBI_FTDS_ct_describe
#  define ct_callback       NCBI_FTDS_ct_callback
#  define ct_send_dyn       NCBI_FTDS_ct_send_dyn
#  define ct_results_dyn    NCBI_FTDS_ct_results_dyn
#  define ct_config         NCBI_FTDS_ct_config
#  define ct_cmd_props      NCBI_FTDS_ct_cmd_props
#  define ct_compute_info   NCBI_FTDS_ct_compute_info
#  define ct_get_data       NCBI_FTDS_ct_get_data
#  define ct_send_data      NCBI_FTDS_ct_send_data
#  define ct_data_info      NCBI_FTDS_ct_data_info
#  define ct_capability     NCBI_FTDS_ct_capability
#  define ct_dynamic        NCBI_FTDS_ct_dynamic
#  define ct_param          NCBI_FTDS_ct_param
#  define ct_setparam       NCBI_FTDS_ct_setparam
#  define ct_options        NCBI_FTDS_ct_options
#  define ct_poll           NCBI_FTDS_ct_poll
#  define ct_cursor         NCBI_FTDS_ct_cursor
#  define ct_diag           NCBI_FTDS_ct_diag
#  define blk_alloc         NCBI_FTDS_blk_alloc
#  define blk_bind          NCBI_FTDS_blk_bind
#  define blk_colval        NCBI_FTDS_blk_colval
#  define blk_default       NCBI_FTDS_blk_default
#  define blk_describe      NCBI_FTDS_blk_describe
#  define blk_done          NCBI_FTDS_blk_done
#  define blk_drop          NCBI_FTDS_blk_drop
#  define blk_getrow        NCBI_FTDS_blk_getrow
#  define blk_gettext       NCBI_FTDS_blk_gettext
#  define blk_init          NCBI_FTDS_blk_init
#  define blk_props         NCBI_FTDS_blk_props
#  define blk_rowalloc      NCBI_FTDS_blk_rowalloc
#  define blk_rowdrop       NCBI_FTDS_blk_rowdrop
#  define blk_rowxfer       NCBI_FTDS_blk_rowxfer
#  define blk_rowxfer_mult  NCBI_FTDS_blk_rowxfer_mult
#  define blk_sendrow       NCBI_FTDS_blk_sendrow
#  define blk_sendtext      NCBI_FTDS_blk_sendtext
#  define blk_srvinit       NCBI_FTDS_blk_srvinit
#  define blk_textxfer      NCBI_FTDS_blk_textxfer

#endif  /* NCBI_DBAPI_RENAME_CTLIB */


#endif  /* DBAPI_DRIVER_CTLIB___NCBI_DBAPI_RENAME_CTLIB__H */
