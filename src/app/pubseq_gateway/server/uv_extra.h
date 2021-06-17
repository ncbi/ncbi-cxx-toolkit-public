#ifndef UV_EXTRA__H
#define UV_EXTRA__H

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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */

#include <uv.h>

#ifdef __cplusplus
extern "C" {
#endif

struct uv_export_t;

int uv_export_start(uv_loop_t *  loop, uv_stream_t *  handle,
                    const char *  ipc_name, unsigned short  count,
                    struct uv_export_t **  exp);
int uv_export_finish(struct uv_export_t *  exp);
int uv_import(uv_loop_t *  loop, uv_stream_t *  handle,
              struct uv_export_t *  exp);
int uv_export_close(struct uv_export_t *  exp);

#ifdef __cplusplus
}
#endif

#endif
