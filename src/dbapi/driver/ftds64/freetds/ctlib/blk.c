/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-2004, 2005  Brian Bruns, Bill Thompson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "tdsstring.h"

#include "bkpublic.h"

#include "ctpublic.h"
#include "ctlib.h"
#include "replacements.h"

#include <assert.h>
#include <stdlib.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

typedef struct _pbcb
{
	char *pb;
	int cb;
} TDS_PBCB;

TDS_RCSID(var, "$Id$");

static CS_RETCODE _blk_get_col_data(CS_BLKDESC *, TDSCOLUMN *, int );
static int _blk_add_variable_columns(CS_BLKDESC * blkdesc, int offset, unsigned char * rowbuffer, int start, int *var_cols);
static CS_RETCODE _blk_add_fixed_columns(CS_BLKDESC * blkdesc, int offset, unsigned char * rowbuffer, int start);
static CS_RETCODE _blk_build_bcp_record(CS_BLKDESC *blkdesc, CS_INT offset);
static CS_RETCODE _blk_send_colmetadata(CS_BLKDESC * blkdesc);
static CS_RETCODE _blk_build_bulk_insert_stmt(TDS_PBCB * clause, TDSCOLUMN * bcpcol, int first);
static CS_RETCODE _rowxfer_in_init(CS_BLKDESC * blkdesc);
static CS_RETCODE _blk_rowxfer_in(CS_BLKDESC * blkdesc, CS_INT rows_to_xfer, CS_INT * rows_xferred);
static CS_RETCODE _blk_rowxfer_out(CS_BLKDESC * blkdesc, CS_INT rows_to_xfer, CS_INT * rows_xferred);
static CS_RETCODE _blk_start_new_batch(CS_BLKDESC * blkdesc);
static int _blk_is_binded(TDSCOLUMN *);
static void _blk_clean_desc(CS_BLKDESC * blkdesc);
static int _blk_read_column_defaults(CS_BLKDESC * blkdesc);
static int _blk_send_blob_columns(CS_BLKDESC *blkdesc, CS_INT offset);
static int _blk5_send_blob_columns(CS_BLKDESC *blkdesc, CS_INT offset);

#undef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

CS_RETCODE
blk_alloc(CS_CONNECTION * connection, CS_INT version, CS_BLKDESC ** blk_pointer)
{
    tdsdump_log(TDS_DBG_FUNC, "blk_alloc()\n");

    *blk_pointer = (CS_BLKDESC *) malloc(sizeof(CS_BLKDESC));
    memset(*blk_pointer, '\0', sizeof(CS_BLKDESC));

    /* so we know who we belong to */
    (*blk_pointer)->con = connection;
    tds_dstr_init(&(*blk_pointer)->hints);

    return CS_SUCCEED;
}


CS_RETCODE
blk_bind(CS_BLKDESC * blkdesc, CS_INT item, CS_DATAFMT * datafmt, CS_VOID * buffer, CS_INT * datalen, CS_SMALLINT * indicator)
{
    TDSCOLUMN *colinfo;
    CS_CONNECTION *con;
    CS_INT bind_count;
    int i;

    tdsdump_log(TDS_DBG_FUNC, "blk_bind()\n");

    if (!blkdesc) {
        return CS_FAIL;
    }
    con = blkdesc->con;

    if (item == CS_UNUSED) {
        /* clear all bindings */
        if (datafmt == NULL && buffer == NULL && datalen == NULL && indicator == NULL ) {
            blkdesc->bind_count = CS_UNUSED;
            for (i = 0; i < blkdesc->bindinfo->num_cols; i++ ) {
                colinfo = blkdesc->bindinfo->columns[i];
                colinfo->column_varaddr  = NULL;
                colinfo->column_bindtype = 0;
                colinfo->column_bindfmt  = 0;
                colinfo->column_bindlen  = 0;
                colinfo->column_nullbind = NULL;
                colinfo->column_lenbind  = NULL;
            }
        }
        return CS_SUCCEED;
    }

    /* check item value */

    if (item < 1 || item > blkdesc->bindinfo->num_cols) {
        _ctclient_msg(con, "blk_bind", 2, 5, 1, 141, "%s, %d", "colnum", item);
        return CS_FAIL;
    }

    /* clear bindings for this column */

    if (datafmt == NULL && buffer == NULL && datalen == NULL && indicator == NULL ) {

        colinfo = blkdesc->bindinfo->columns[item - 1];
        colinfo->column_varaddr  = NULL;
        colinfo->column_bindtype = 0;
        colinfo->column_bindfmt  = 0;
        colinfo->column_bindlen  = 0;
        colinfo->column_nullbind = NULL;
        colinfo->column_lenbind  = NULL;

        return CS_SUCCEED;
    }

    /*
     * check whether the request is for array binding and ensure that user
     * supplies the same datafmt->count to the subsequent ct_bind calls
     */

    bind_count = (datafmt->count == 0) ? 1 : datafmt->count;

    /* first bind for this result set */

    if (blkdesc->bind_count == CS_UNUSED) {
        blkdesc->bind_count = bind_count;
    } else {
        /* all subsequent binds for this result set - the bind counts must be the same */
        if (blkdesc->bind_count != bind_count) {
            _ctclient_msg(con, "blk_bind", 1, 1, 1, 137, "%d, %d", bind_count, blkdesc->bind_count);
            return CS_FAIL;
        }
    }

    /* bind the column_varaddr to the address of the buffer */

    colinfo = blkdesc->bindinfo->columns[item - 1];

    colinfo->column_varaddr = (char *) buffer;
    colinfo->column_bindtype = datafmt->datatype;
    colinfo->column_bindfmt = datafmt->format;
    colinfo->column_bindlen = datafmt->maxlength;
    if (indicator) {
        colinfo->column_nullbind = indicator;
    }
    if (datalen) {
        colinfo->column_lenbind = datalen;
    }
    return CS_SUCCEED;
}

CS_RETCODE
blk_colval(SRV_PROC * srvproc, CS_BLKDESC * blkdescp, CS_BLK_ROW * rowp, CS_INT colnum, CS_VOID * valuep, CS_INT valuelen,
	   CS_INT * outlenp)
{

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED blk_colval()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_default(CS_BLKDESC * blkdesc, CS_INT colnum, CS_VOID * buffer, CS_INT buflen, CS_INT * outlen)
{

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED blk_default()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_describe(CS_BLKDESC * blkdesc, CS_INT item, CS_DATAFMT * datafmt)
{
    TDSCOLUMN *curcol;
    int len;

    tdsdump_log(TDS_DBG_FUNC, "blk_describe()\n");

    if (item < 1 || item > blkdesc->bindinfo->num_cols) {
        _ctclient_msg(blkdesc->con, "blk_describe", 2, 5, 1, 141, "%s, %d", "colnum", item);
        return CS_FAIL;
    }

    curcol = blkdesc->bindinfo->columns[item - 1];
    len = curcol->column_namelen;
    if (len >= CS_MAX_NAME)
        len = CS_MAX_NAME - 1;
    strncpy(datafmt->name, curcol->column_name, len);
    /* name is always null terminated */
    datafmt->name[len] = 0;
    datafmt->namelen = len;
    /* need to turn the SYBxxx into a CS_xxx_TYPE */
    datafmt->datatype = _ct_get_client_type(blkdesc->con->ctx, curcol->column_type, curcol->column_usertype, curcol->column_size);
    tdsdump_log(TDS_DBG_INFO1, "blk_describe() datafmt->datatype = %d server type %d\n", datafmt->datatype,
            curcol->column_type);
    /* FIXME is ok this value for numeric/decimal? */
    datafmt->maxlength = curcol->column_size;
    datafmt->usertype = curcol->column_usertype;
    datafmt->precision = curcol->column_prec;
    datafmt->scale = curcol->column_scale;

    /*
     * There are other options that can be returned, but these are the
     * only two being noted via the TDS layer.
     */
    datafmt->status = 0;
    if (curcol->column_nullable)
        datafmt->status |= CS_CANBENULL;
    if (curcol->column_identity)
        datafmt->status |= CS_IDENTITY;

    datafmt->count = 1;
    datafmt->locale = NULL;

    return CS_SUCCEED;
}

CS_RETCODE
blk_done(CS_BLKDESC * blkdesc, CS_INT type, CS_INT * outrow)
{
    TDSSOCKET *tds;

    tdsdump_log(TDS_DBG_FUNC, "blk_done()\n");
    tds = blkdesc->con->tds_socket;

    switch (type) {
        case CS_BLK_BATCH:

            tds_flush_packet(tds);
            /* TODO correct ?? */
            tds_set_state(tds, TDS_PENDING);
            if (tds_process_simple_query(tds) != TDS_SUCCEED) {
                _ctclient_msg(blkdesc->con, "blk_done", 2, 5, 1, 140, "");
                return CS_FAIL;
            }

            if (outrow)
                *outrow = tds->rows_affected;

            if (_blk_start_new_batch(blkdesc) == CS_FAIL)
                return (CS_FAIL);

            break;

        case CS_BLK_ALL:

            tds_flush_packet(tds);
            /* TODO correct ?? */
            tds_set_state(tds, TDS_PENDING);
            if (tds_process_simple_query(tds) != TDS_SUCCEED) {
                _ctclient_msg(blkdesc->con, "blk_done", 2, 5, 1, 140, "");
                return CS_FAIL;
            }

            if (outrow)
                *outrow = tds->rows_affected;

            /* free allocated storage in blkdesc & initialise flags, etc. */
            _blk_clean_desc(blkdesc);

            break;

        case CS_BLK_CANCEL:

            tds_send_cancel(tds);

            tds_set_state(tds, TDS_PENDING);
            if (tds_process_cancel(tds) != TDS_SUCCEED) {
                _ctclient_msg(blkdesc->con, "blk_done", 2, 5, 1, 140, "");
                return CS_FAIL;
            }

            if (outrow)
                *outrow = 0;

            /* free allocated storage in blkdesc & initialise flags, etc. */
            _blk_clean_desc(blkdesc);

            break;

    }

    return CS_SUCCEED;
}

CS_RETCODE
blk_drop(CS_BLKDESC * blkdesc)
{
    if (!blkdesc)
        return CS_SUCCEED;

    _blk_clean_desc(blkdesc);
    free(blkdesc);

    return CS_SUCCEED;
}

static void
_blk_clean_desc(CS_BLKDESC * blkdesc)
{
    if (blkdesc->tablename)
        TDS_ZERO_FREE(blkdesc->tablename);

    if (blkdesc->insert_stmt)
        TDS_ZERO_FREE(blkdesc->insert_stmt);

    if (blkdesc->bindinfo) {
        /* In TDS 5.0 structure of row is different, so free it
           before entering tds_free_results */
        if (IS_TDS50(blkdesc->con->tds_socket)  &&  blkdesc->bindinfo->current_row) {
            free(blkdesc->bindinfo->current_row);
            blkdesc->bindinfo->current_row = NULL;
        }
        tds_free_results(blkdesc->bindinfo);
        blkdesc->bindinfo = NULL;
    }

    blkdesc->direction = 0;
    blkdesc->bind_count = CS_UNUSED;
    blkdesc->xfer_init = 0;
    blkdesc->var_cols = 0;
    blkdesc->text_sent = 0;
    blkdesc->current_col = 0;
    blkdesc->blob_cols = 0;

    tds_dstr_free(&blkdesc->hints);
}

CS_RETCODE
blk_getrow(SRV_PROC * srvproc, CS_BLKDESC * blkdescp, CS_BLK_ROW * rowp)
{

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED blk_getrow()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_gettext(SRV_PROC * srvproc, CS_BLKDESC * blkdescp, CS_BLK_ROW * rowp, CS_INT bufsize, CS_INT * outlenp)
{

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED blk_gettext()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_init(CS_BLKDESC * blkdesc, CS_INT direction, CS_CHAR * tablename, CS_INT tnamelen)
{
    TDSCOLUMN *curcol;

    TDSSOCKET *tds;
    TDSRESULTINFO *resinfo;
    TDSRESULTINFO *bindinfo;
    TDS_INT result_type;
    int i, rc;

    tdsdump_log(TDS_DBG_FUNC, "blk_init()\n");

    if (!blkdesc) {
        return CS_FAIL;
    }

    if (direction != CS_BLK_IN && direction != CS_BLK_OUT ) {
        _ctclient_msg(blkdesc->con, "blk_init", 2, 6, 1, 138, "");
        return CS_FAIL;
    }

    if (!tablename) {
        _ctclient_msg(blkdesc->con, "blk_init", 2, 6, 1, 139, "");
        return CS_FAIL;
    }
    if (tnamelen == CS_NULLTERM)
        tnamelen = strlen(tablename);

    /* free allocated storage in blkdesc & initialise flags, etc. */

    if (blkdesc->tablename) {
        tdsdump_log(TDS_DBG_FUNC, "blk_init() freeing tablename\n");
        free(blkdesc->tablename);
    }

    if (blkdesc->insert_stmt) {
        tdsdump_log(TDS_DBG_FUNC, "blk_init() freeing insert_stmt\n");
        TDS_ZERO_FREE(blkdesc->insert_stmt);
    }

    if (blkdesc->bindinfo) {
        tdsdump_log(TDS_DBG_FUNC, "blk_init() freeing results\n");
        tds_free_results(blkdesc->bindinfo);
        blkdesc->bindinfo = NULL;
    }

    /* string can be no-nul terminated so copy with memcpy */
    blkdesc->tablename = (char *) malloc(tnamelen + 1);
    /* FIXME malloc can fail */
    memcpy(blkdesc->tablename, tablename, tnamelen);
    blkdesc->tablename[tnamelen] = 0;

    blkdesc->direction = direction;
    blkdesc->bind_count = CS_UNUSED;
    blkdesc->xfer_init = 0;
    blkdesc->var_cols = 0;

    tds = blkdesc->con->tds_socket;

    /* TODO quote tablename if needed */
    if (tds_submit_queryf(tds, "select * from %s where 0 = 1", blkdesc->tablename) == TDS_FAIL) {
        _ctclient_msg(blkdesc->con, "blk_init", 2, 5, 1, 140, "");
        return CS_FAIL;
    }

    while ((rc = tds_process_tokens(tds, &result_type, NULL, TDS_TOKEN_RESULTS))
            == TDS_SUCCEED) {
    }
    if (rc != TDS_NO_MORE_RESULTS) {
        _ctclient_msg(blkdesc->con, "blk_init", 2, 5, 1, 140, "");
        return CS_FAIL;
    }

    /* copy the results info from the TDS socket into CS_BLKDESC structure */

    if (!tds->res_info) {
        _ctclient_msg(blkdesc->con, "blk_init", 2, 5, 1, 140, "");
        return CS_FAIL;
    }

    resinfo = tds->res_info;

    if ((bindinfo = tds_alloc_results(resinfo->num_cols)) == NULL) {
        _ctclient_msg(blkdesc->con, "blk_init", 2, 5, 1, 140, "");
        return CS_FAIL;
    }


    bindinfo->row_size = resinfo->row_size;

    for (i = 0; i < bindinfo->num_cols; i++) {

        curcol = bindinfo->columns[i];

        curcol->column_type = resinfo->columns[i]->column_type;
        curcol->column_usertype = resinfo->columns[i]->column_usertype;
        curcol->column_flags = resinfo->columns[i]->column_flags;
        curcol->column_size = resinfo->columns[i]->column_size;
        curcol->column_varint_size = resinfo->columns[i]->column_varint_size;
        curcol->column_prec = resinfo->columns[i]->column_prec;
        curcol->column_scale = resinfo->columns[i]->column_scale;
        curcol->column_namelen = resinfo->columns[i]->column_namelen;
        curcol->on_server.column_type = resinfo->columns[i]->on_server.column_type;
        curcol->on_server.column_size = resinfo->columns[i]->on_server.column_size;
        curcol->char_conv = resinfo->columns[i]->char_conv;
        memcpy(curcol->column_name, resinfo->columns[i]->column_name, resinfo->columns[i]->column_namelen);
        curcol->column_nullable = resinfo->columns[i]->column_nullable;
        curcol->column_identity = resinfo->columns[i]->column_identity;
        curcol->column_timestamp = resinfo->columns[i]->column_timestamp;
        curcol->column_offset = resinfo->columns[i]->column_offset;

        memcpy(curcol->column_collation, resinfo->columns[i]->column_collation, 5);

        if (is_numeric_type(curcol->column_type)) {
            curcol->bcp_column_data = tds_alloc_bcp_column_data(sizeof(TDS_NUMERIC));
            ((TDS_NUMERIC *) curcol->bcp_column_data->data)->precision = curcol->column_prec;
            ((TDS_NUMERIC *) curcol->bcp_column_data->data)->scale = curcol->column_scale;
        } else {
            if (is_blob_type(curcol->column_type)) {
                curcol->bcp_column_data = tds_alloc_bcp_column_data(0);
            } else {
                curcol->bcp_column_data = tds_alloc_bcp_column_data(curcol->on_server.column_size);
            }
        }
    }

    bindinfo->current_row = tds_alloc_row(bindinfo);

    blkdesc->bindinfo = bindinfo;
    blkdesc->bind_count = CS_UNUSED;

    if (blkdesc->identity_insert_on) {

        if (tds_submit_queryf(tds, "set identity_insert %s on", blkdesc->tablename) == TDS_FAIL) {
            _ctclient_msg(blkdesc->con, "blk_init", 2, 5, 1, 140, "");
            return CS_FAIL;
        }

        while ((rc = tds_process_tokens(tds, &result_type, NULL, TDS_TOKEN_RESULTS))
                == TDS_SUCCEED) {
        }
        if (rc != TDS_NO_MORE_RESULTS) {
            _ctclient_msg(blkdesc->con, "blk_init", 2, 5, 1, 140, "");
            return CS_FAIL;
        }
    }

    return CS_SUCCEED;
}

CS_RETCODE
blk_props(CS_BLKDESC * blkdesc, CS_INT action, CS_INT property, CS_VOID * buffer, CS_INT buflen, CS_INT * outlen)
{

    int retval;
    int intval;

    switch (property) {
        case BLK_IDENTITY:
            switch (action) {
                case CS_SET:
                    if (buffer) {
                        memcpy(&intval, buffer, sizeof(intval));
                        if (intval == CS_TRUE)
                            blkdesc->identity_insert_on = 1;
                        if (intval == CS_FALSE)
                            blkdesc->identity_insert_on = 0;
                    }
                    return CS_SUCCEED;
                    break;
                case CS_GET:
                    retval = blkdesc->identity_insert_on == 1 ? CS_TRUE : CS_FALSE ;
                    if (buffer) {
                        memcpy (buffer, &retval, sizeof(retval));
                        if (outlen)
                            *outlen = sizeof(retval);
                    }
                    return CS_SUCCEED;
                    break;
                default:
                    _ctclient_msg(blkdesc->con, "blk_props", 2, 5, 1, 141, "%s, %d", "action", action);
                    break;
            }
            break;

        default:
            _ctclient_msg(blkdesc->con, "blk_props", 2, 5, 1, 141, "%s, %d", "property", property);
            break;
    }
    return CS_FAIL;
}

CS_RETCODE
blk_sethints(CS_BLKDESC* blkdesc, CS_CHAR* hints, CS_INT hintslen)
{
    if (!blkdesc)
        return CS_FAIL;

    tds_dstr_copyn(&blkdesc->hints, hints, hintslen);

    return CS_SUCCEED;
}

CS_RETCODE
blk_rowalloc(SRV_PROC * srvproc, CS_BLK_ROW ** row)
{

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED blk_rowalloc()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_rowdrop(SRV_PROC * srvproc, CS_BLK_ROW * row)
{

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED blk_rowdrop()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_rowxfer(CS_BLKDESC * blkdesc)
{
	CS_INT row_count = 1;

	return blk_rowxfer_mult(blkdesc, &row_count);
}

CS_RETCODE
blk_rowxfer_mult(CS_BLKDESC * blkdesc, CS_INT * row_count)
{

	int rows_to_xfer = 0;
	int rows_xferred = 0;
	CS_RETCODE ret;

	tdsdump_log(TDS_DBG_FUNC, "blk_rowxfer_mult()\n");

	if (!row_count || *row_count == 0 )
		rows_to_xfer = blkdesc->bind_count;
	else
		rows_to_xfer = *row_count;

	if (blkdesc->direction == CS_BLK_IN) {
		ret = _blk_rowxfer_in(blkdesc, rows_to_xfer, &rows_xferred);
	} else {
		ret = _blk_rowxfer_out(blkdesc, rows_to_xfer, &rows_xferred);
	}
	if (row_count)
		*row_count = rows_xferred;
	return ret;

}

CS_RETCODE
blk_sendrow(CS_BLKDESC * blkdesc, CS_BLK_ROW * row)
{

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED blk_sendrow()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_sendtext(CS_BLKDESC * blkdesc, CS_BLK_ROW * row, CS_BYTE * buffer, CS_INT buflen)
{

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED blk_sendtext()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_srvinit(SRV_PROC * srvproc, CS_BLKDESC * blkdescp)
{

	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED blk_srvinit()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_textxfer(CS_BLKDESC * blkdesc, CS_BYTE * buffer, CS_INT buflen, CS_INT * outlen)
{

    TDSSOCKET  *tds = blkdesc->con->tds_socket;
    TDSCOLUMN  *bindcol;
    TDS_INT    offset = 0;

    static const unsigned char textptr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };
    static const unsigned char timestamp[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

    static const TDS_TINYINT textptr_size = 16;

    unsigned char *record;
    TDS_INT	  old_record_size;
    TDS_INT	  new_record_size;

    TDS_INT	 varint_4;

    CS_RETCODE rc = CS_SUCCEED;

    int         result = 0;
    CS_INT      null_column = 0;
    CS_INT      srclen  = 0;
    CS_INT      destlen  = 0;
    CS_INT      srctype = 0;
    CS_SMALLINT *nullind = NULL;
    CS_INT      *datalen = NULL;
    CS_CONTEXT  *ctx = blkdesc->con->ctx;
    unsigned char* current_row = blkdesc->bindinfo->current_row;

    CS_DATAFMT srcfmt;
    CS_DATAFMT destfmt;

    CS_INT need_to_send;

    tdsdump_log(TDS_DBG_FUNC, "blk_textxfer(blkdesc, buflen %d, outlen)\n", buflen);

    record = blkdesc->bindinfo->current_row;
    old_record_size = blkdesc->bindinfo->row_size;
    new_record_size = 0;

    if(blkdesc->current_col >= blkdesc->bindinfo->num_cols)
        return CS_FAIL;

    bindcol = blkdesc->bindinfo->columns[blkdesc->current_col];

    if (IS_TDS7_PLUS(tds)) {
        /* Added by ssikorsk */
        current_row += bindcol->column_offset;
        record += bindcol->column_offset;

        if ((!blkdesc->identity_insert_on && bindcol->column_identity) ||
                bindcol->column_timestamp ||
                !is_blob_type(bindcol->on_server.column_type)) {
            return CS_FAIL;
        }

        /* Calculate data shared between two stages */
        /* Similar to _blk_get_col_data */

        if (bindcol->column_varaddr != NULL) {
            return CS_FAIL;
        }

        if (bindcol->column_nullbind) {
            nullind = bindcol->column_nullbind;
        }

        if (bindcol->column_lenbind) {
            datalen = bindcol->column_lenbind;
        }

        srctype = bindcol->column_bindtype; 		/* used to pass to cs_convert */

        if (buflen == CS_UNUSED) {
            switch (srctype) {
                case CS_LONG_TYPE:	    srclen = 8; break;
                case CS_FLOAT_TYPE:	    srclen = 8; break;
                case CS_MONEY_TYPE:	    srclen = 8; break;
                case CS_DATETIME_TYPE:  srclen = 8; break;
                case CS_INT_TYPE:	    srclen = 4; break;
                case CS_REAL_TYPE:	    srclen = 4; break;
                case CS_MONEY4_TYPE:	srclen = 4; break;
                case CS_DATETIME4_TYPE: srclen = 4; break;
                case CS_SMALLINT_TYPE:  srclen = 2; break;
                case CS_TINYINT_TYPE:   srclen = 1; break;
                default:
                                        printf("error not fixed length type (%d) and datalen not specified\n", bindcol->column_bindtype);
                                        return CS_FAIL;
            }

        } else {
            srclen = buflen;
        }

        if (blkdesc->text_sent == 0) {

            null_column = (srclen == 0 && *nullind == -1) ? 1: 0;

            bindcol->bcp_column_data->null_column = null_column;

            /* Put a data header */

#if ENABLE_EXTRA_CHECKS
            assert(!bindcol->bcp_column_data->null_column);
            assert(bindcol->column_varint_size == 4);
#endif
            /* This is not NULL */

            if (buffer == NULL /*|| *buffer == 0 || buflen == 0*/) {
                return CS_FAIL;
            }

            /* ssikorsk */
            /* TDSBLOB */
            /* *record = textptr_size; record++; */
            *((TDS_CHAR**) record) = 0;
            tds_put_n(tds, &textptr_size, sizeof(textptr_size));
            record += sizeof(TDS_CHAR*);
            current_row += sizeof(TDS_CHAR*);
            memcpy(record, textptr, 16); record += 16;
            memcpy(record, timestamp, 8); record += 8;
            /* new_record_size += 25; */
            new_record_size += 24;

            // varint_4 = bindcol->bcp_column_data->datalen;
            varint_4 = bindcol->column_bindlen;
#if WORDS_BIGENDIAN
            tds_swap_datatype(SYBINT4, (unsigned char *)&varint_4);
#endif
            memcpy(record, &varint_4, 4); record += 4; new_record_size +=4;

#if WORDS_BIGENDIAN
            tds_swap_datatype(
                    tds_get_conversion_type(
                        bindcol->on_server.column_type, 
                        bindcol->on_server.column_size 
                        ),
                    bindcol->bcp_column_data->data
                    );
#endif
        }

        tds_put_n(tds, current_row, new_record_size);

        /* Put actual data */
        /* Encode data */

        need_to_send = (CS_INT)bindcol->column_bindlen - blkdesc->text_sent;
        buflen = (buflen > need_to_send) ? need_to_send : buflen;

        srcfmt.datatype = srctype;
        srcfmt.maxlength = srclen;

        destfmt.datatype  = _ct_get_client_type(blkdesc->con->ctx, bindcol->column_type, bindcol->column_usertype, bindcol->column_size);
        destfmt.maxlength = buflen;
        destfmt.precision = bindcol->column_prec;
        destfmt.scale     = bindcol->column_scale;

        destfmt.format  = CS_FMT_UNUSED;

        buflen = MIN(buflen, (CS_INT)bindcol->column_bindlen - blkdesc->text_sent);

        /*if (buflen) {*/
            tds_put_n(tds, buffer, buflen);

            blkdesc->text_sent += buflen;
            if ((CS_INT)bindcol->column_bindlen == blkdesc->text_sent) {
                blkdesc->text_sent = 0;
                blkdesc->current_col++;
                rc = _blk_send_blob_columns(blkdesc, 0);
                if (rc == CS_SUCCEED)
                    rc = CS_END_DATA;
            }
        /*}*/
    }  /* IS_TDS7_PLUS */
    else {
        /* handle any text/image data */

        if (is_blob_type(bindcol->column_type)) {
            if (blkdesc->text_sent == 0) {
                /* unknown but zero */
                tds_put_smallint(tds, 0);
                tds_put_byte(tds, (unsigned char)bindcol->column_type);
                tds_put_byte(tds, 0xff - blkdesc->blob_cols);
                /*
                 * offset of txptr we stashed during variable
                 * column processing
                 */
                tds_put_smallint(tds, bindcol->column_textpos);
                tds_put_int(tds, bindcol->column_bindlen);
            }

            buflen = MIN(buflen, (CS_INT)bindcol->column_bindlen - blkdesc->text_sent);

            /*if (buflen) {*/
                tds_put_n(tds, buffer, buflen);

                blkdesc->text_sent += buflen;
                if ((CS_INT)bindcol->column_bindlen == blkdesc->text_sent) {
                    blkdesc->text_sent = 0;
                    blkdesc->blob_cols++;
                    blkdesc->current_col++;
                    rc = _blk5_send_blob_columns(blkdesc, 0);
                    if (rc == CS_SUCCEED)
                        rc = CS_END_DATA;
                }
            /*}*/
        }
        else {
            assert(0);
        }
    }

    return rc;
}

static CS_RETCODE
_blk_rowxfer_out(CS_BLKDESC * blkdesc, CS_INT rows_to_xfer, CS_INT * rows_xferred)
{

    TDSSOCKET *tds;
    TDS_INT result_type;
    TDS_INT ret;
    TDS_INT temp_count;
    TDS_INT row_of_query;
    TDS_INT rows_written;

    tdsdump_log(TDS_DBG_FUNC, "blk_rowxfer_out()\n");

    if (!blkdesc || !blkdesc->con)
        return CS_FAIL;

    tds = blkdesc->con->tds_socket;

    /*
     * the first time blk_xfer called after blk_init()
     * do the query and get to the row data...
     */

    if (blkdesc->xfer_init == 0) {

        if (tds_submit_queryf(tds, "select * from %s", blkdesc->tablename)
                == TDS_FAIL) {
            _ctclient_msg(blkdesc->con, "blk_rowxfer", 2, 5, 1, 140, "");
            return CS_FAIL;
        }

        while ((ret = tds_process_tokens(tds, &result_type, NULL, TDS_TOKEN_RESULTS)) == TDS_SUCCEED) {
            if (result_type == TDS_ROW_RESULT)
                break;
        }

        if (ret != TDS_SUCCEED || result_type != TDS_ROW_RESULT) {
            _ctclient_msg(blkdesc->con, "blk_rowxfer", 2, 5, 1, 140, "");
            return CS_FAIL;
        }

        blkdesc->xfer_init = 1;
    }

    row_of_query = 0;
    rows_written = 0;

    if (rows_xferred)
        *rows_xferred = 0;

    for (temp_count = 0; temp_count < rows_to_xfer; temp_count++) {

        ret = tds_process_tokens(tds, &result_type, NULL, TDS_STOPAT_ROWFMT|TDS_STOPAT_DONE|TDS_RETURN_ROW|TDS_RETURN_COMPUTE);

        tdsdump_log(TDS_DBG_FUNC, "blk_rowxfer_out() process_row_tokens returned %d\n", ret);

        switch (ret) {
            case TDS_SUCCEED:
                if (result_type == TDS_ROW_RESULT || result_type == TDS_COMPUTE_RESULT) {
                    if (result_type == TDS_ROW_RESULT) {
                        if (_ct_bind_data( blkdesc->con->ctx, tds->current_results, blkdesc->bindinfo, temp_count))
                            return CS_ROW_FAIL;
                        if (rows_xferred)
                            *rows_xferred = *rows_xferred + 1;
                    }
                    break;
                }
            case TDS_NO_MORE_RESULTS:
                return CS_END_DATA;
                break;

            default:
                _ctclient_msg(blkdesc->con, "blk_rowxfer", 2, 5, 1, 140, "");
                return CS_FAIL;
                break;
        }
    }

    return CS_SUCCEED;
}

static CS_RETCODE
_blk_rowxfer_in(CS_BLKDESC * blkdesc, CS_INT rows_to_xfer, CS_INT * rows_xferred)
{

    TDSSOCKET *tds;
    TDS_INT each_row;
    CS_RETCODE rc = CS_SUCCEED;

    if (!blkdesc)
        return CS_FAIL;

    tds = blkdesc->con->tds_socket;

    /*
     * the first time blk_xfer called after blk_init()
     * do the query and get to the row data...
     */

    if (blkdesc->xfer_init == 0) {

        /*
         * first call the start_copy function, which will
         * retrieve details of the database table columns
         */

        if (_rowxfer_in_init(blkdesc) == CS_FAIL)
            return (CS_FAIL);


        if (_blk_start_new_batch(blkdesc) == CS_FAIL)
            return (CS_FAIL);

        blkdesc->xfer_init = 1;
    }

    for (each_row = 0; each_row < rows_to_xfer; each_row++ ) {

        rc = _blk_build_bcp_record(blkdesc, each_row);

        if (rc != CS_SUCCEED) {
            return rc;
        }
    }

    return CS_SUCCEED;
}


static CS_RETCODE
_blk_start_new_batch(CS_BLKDESC * blkdesc)
{
    int rc;
    TDSSOCKET *tds = blkdesc->con->tds_socket;

    tds_submit_query(tds, blkdesc->insert_stmt);

    if (IS_TDS7_PLUS(tds)) {
        rc = tds_process_simple_query(tds);
    }
    else {
        /*
         * In TDS 5 we get the column information as a result set from the "insert bulk" command.
         * We need to get information about default values from it.
         */
        rc = _blk_read_column_defaults(blkdesc);
    }
    if (rc != TDS_SUCCEED) {
        _ctclient_msg(blkdesc->con, "blk_rowxfer", 2, 5, 1, 140, "");
        return CS_FAIL;
    }

    /* set packet type to send bulk data */
    tds->out_flag = TDS_BULK;

    if (IS_TDS7_PLUS(tds)) {
        _blk_send_colmetadata(blkdesc);
    }

    if (tds_set_state(tds, TDS_QUERYING) != TDS_QUERYING) {
        return CS_FAIL;
    }

    return CS_SUCCEED;
}


static CS_RETCODE
_rowxfer_in_init(CS_BLKDESC * blkdesc)
{

    TDSSOCKET *tds = blkdesc->con->tds_socket;
    TDSCOLUMN *bcpcol;

    int i, n;
    int firstcol;

    int fixed_col_len_tot     = 0;
    int variable_col_len_tot  = 0;
    int column_bcp_data_size  = 0;
    int bcp_record_size       = 0;

    char *query;
    char clause_buffer[4096] = { 0 };

    TDS_PBCB colclause;

    colclause.pb = clause_buffer;
    colclause.cb = sizeof(clause_buffer);

    if (IS_TDS7_PLUS(tds)) {
        int erc;
        char *hint;

        firstcol = 1;

        for (n = 2; n--; ) {
            for (i = 0; i < blkdesc->bindinfo->num_cols; i++) {
                bcpcol = blkdesc->bindinfo->columns[i];
                if (n  &&  is_blob_type(bcpcol->on_server.column_type))
                    continue;
                else if (!n  &&  !is_blob_type(bcpcol->on_server.column_type))
                    continue;

                if (blkdesc->identity_insert_on) {
                    /* ssikorsk */
                    if (!bcpcol->column_timestamp && _blk_is_binded(bcpcol)) {
                        _blk_build_bulk_insert_stmt(&colclause, bcpcol, firstcol);
                        firstcol = 0;
                    }
                } else {
                    /* ssikorsk */
                    if (!bcpcol->column_identity && !bcpcol->column_timestamp && _blk_is_binded(bcpcol)) {
                        _blk_build_bulk_insert_stmt(&colclause, bcpcol, firstcol);
                        firstcol = 0;
                    }
                }
            }
        }

        if (tds_dstr_isempty(&blkdesc->hints)) {
            hint = strdup("");
        } else {
            if (asprintf(&hint, " with (%s)", tds_dstr_cstr(&blkdesc->hints)) < 0)
                hint = NULL;
        }
        if (!hint) {
            if (colclause.pb != clause_buffer)
                TDS_ZERO_FREE(colclause.pb);
            return TDS_FAIL;
        }

        erc = asprintf(&query, "insert bulk %s (%s)%s", blkdesc->tablename, colclause.pb, hint);

        free(hint);
        if (colclause.pb != clause_buffer)
            TDS_ZERO_FREE(colclause.pb);	/* just for good measure; not used beyond this point */

        if (erc < 0) {
            return CS_FAIL;
        }

    } else {
        /* NOTE: if we use "with nodescribe" for following inserts server do not send describe */
        if (asprintf(&query, "insert bulk %s", blkdesc->tablename) < 0) {
            return CS_FAIL;
        }
    }

    /* save the statement for later... */

    blkdesc->insert_stmt = query;

    /*
     * Work out the number of "variable" columns.  These are either nullable or of
     * varying length type e.g. varchar.
     */
    blkdesc->var_cols = 0;

    if (IS_TDS50(tds)) {
        for (i = 0; i < blkdesc->bindinfo->num_cols; i++) {

            bcpcol = blkdesc->bindinfo->columns[i];

            /*
             * work out storage required for this datatype
             * blobs always require 16, numerics vary, the
             * rest can be taken from the server
             */

            if (is_blob_type(bcpcol->on_server.column_type))
                column_bcp_data_size  = 16;
            else if (is_numeric_type(bcpcol->on_server.column_type))
                column_bcp_data_size  = tds_numeric_bytes_per_prec[bcpcol->column_prec];
            else
                column_bcp_data_size  = bcpcol->column_size;

            /*
             * now add that size into either fixed or variable
             * column totals...
             */

            if (is_nullable_type(bcpcol->on_server.column_type) || bcpcol->column_nullable) {
                blkdesc->var_cols++;
                variable_col_len_tot += column_bcp_data_size;
            }
            else {
                fixed_col_len_tot += column_bcp_data_size;
            }
        }

        /* this formula taken from sybase manual... */

        bcp_record_size =  	4 +
            fixed_col_len_tot +
            variable_col_len_tot +
            ( (int)(variable_col_len_tot / 256 ) + 1 ) +
            (blkdesc->var_cols + 1) +
            2;

        tdsdump_log(TDS_DBG_FUNC, "current_record_size = %d\n", blkdesc->bindinfo->row_size);
        tdsdump_log(TDS_DBG_FUNC, "bcp_record_size     = %d\n", bcp_record_size);

        if (bcp_record_size > blkdesc->bindinfo->row_size) {
            blkdesc->bindinfo->current_row = realloc(blkdesc->bindinfo->current_row, bcp_record_size);
            if (blkdesc->bindinfo->current_row == NULL) {
                tdsdump_log(TDS_DBG_FUNC, "could not realloc current_row\n");
                return CS_FAIL;
            }
            blkdesc->bindinfo->row_size = bcp_record_size;
        }
    }

    return CS_SUCCEED;
}

static CS_RETCODE
_blk_build_bulk_insert_stmt(TDS_PBCB * clause, TDSCOLUMN * bcpcol, int first)
{
    char buffer[32];
    char *column_type = buffer;

    switch (bcpcol->on_server.column_type) {
        case SYBINT1:
            column_type = "tinyint";
            break;
        case SYBBIT:
            column_type = "bit";
            break;
        case SYBINT2:
            column_type = "smallint";
            break;
        case SYBINT4:
            column_type = "int";
            break;
        case SYBINT8:
            column_type = "bigint";
            break;
        case SYBDATETIME:
            column_type = "datetime";
            break;
        case SYBDATETIME4:
            column_type = "smalldatetime";
            break;
        case SYBREAL:
            column_type = "real";
            break;
        case SYBMONEY:
            column_type = "money";
            break;
        case SYBMONEY4:
            column_type = "smallmoney";
            break;
        case SYBFLT8:
            column_type = "float";
            break;

        case SYBINTN:
            switch (bcpcol->column_size) {
                case 1:
                    column_type = "tinyint";
                    break;
                case 2:
                    column_type = "smallint";
                    break;
                case 4:
                    column_type = "int";
                    break;
                case 8:
                    column_type = "bigint";
                    break;
            }
            break;

        case SYBBITN:
            column_type = "bit";
            break;
        case SYBFLTN:
            switch (bcpcol->column_size) {
                case 4:
                    column_type = "real";
                    break;
                case 8:
                    column_type = "float";
                    break;
            }
            break;
        case SYBMONEYN:
            switch (bcpcol->column_size) {
                case 4:
                    column_type = "smallmoney";
                    break;
                case 8:
                    column_type = "money";
                    break;
            }
            break;
        case SYBDATETIMN:
            switch (bcpcol->column_size) {
                case 4:
                    column_type = "smalldatetime";
                    break;
                case 8:
                    column_type = "datetime";
                    break;
            }
            break;
        case SYBDECIMAL:
            sprintf(column_type, "decimal(%d,%d)", bcpcol->column_prec, bcpcol->column_scale);
            break;
        case SYBNUMERIC:
            sprintf(column_type, "numeric(%d,%d)", bcpcol->column_prec, bcpcol->column_scale);
            break;

        case XSYBVARBINARY:
            sprintf(column_type, "varbinary(%d)", bcpcol->column_size);
            break;
        case XSYBVARCHAR:
            /* ssikorsk */
            sprintf(column_type, "varchar(%d)", bcpcol->on_server.column_size);
            break;
            /* case SYBLONGCHAR: */
        case XSYBBINARY:
            sprintf(column_type, "binary(%d)", bcpcol->column_size);
            break;
            /* case SYBLONGBINARY: */
        case XSYBCHAR:
            /* ssikorsk */
            sprintf(column_type, "char(%d)", bcpcol->on_server.column_size);
            break;
        case SYBTEXT:
            sprintf(column_type, "text");
            break;
        case SYBIMAGE:
            sprintf(column_type, "image");
            break;
        case XSYBNVARCHAR:
            /* ssikorsk */
            sprintf(column_type, "nvarchar(%d)", bcpcol->on_server.column_size);
            break;
        case XSYBNCHAR:
            /* ssikorsk */
            sprintf(column_type, "nchar(%d)", bcpcol->on_server.column_size);
            break;
        case SYBNTEXT:
            sprintf(column_type, "ntext");
            break;
        case SYBUNIQUE:
            sprintf(column_type, "uniqueidentifier");
            break;
        default:
            tdsdump_log(TDS_DBG_FUNC, "error: cannot build bulk insert statement. unrecognized server datatype %d\n",
                    bcpcol->on_server.column_type);
            return CS_FAIL;
    }

    if ((size_t)clause->cb < strlen(clause->pb) + strlen(bcpcol->column_name) + strlen(column_type) + ((first) ? 2 : 4)) {
        char *temp = malloc(2 * clause->cb);

        if (!temp)
            return CS_FAIL;
        strcpy(temp, clause->pb);
        clause->pb = temp;
        clause->cb *= 2;
    }

    if (!first)
        strcat(clause->pb, ", ");

    strcat(clause->pb, bcpcol->column_name);
    strcat(clause->pb, " ");
    strcat(clause->pb, column_type);

    return CS_SUCCEED;
}

static CS_RETCODE
_blk_send_colmetadata(CS_BLKDESC * blkdesc)
{

    TDSSOCKET *tds = blkdesc->con->tds_socket;
    unsigned char colmetadata_token = 0x81;
    TDSCOLUMN *bcpcol;
    int i, n;
    TDS_SMALLINT num_cols;

    /*
     * Deep joy! For TDS 8 we have to send a colmetadata message followed by row data
     */
    tds_put_byte(tds, colmetadata_token);	/* 0x81 */

    num_cols = 0;
    for (i = 0; i < blkdesc->bindinfo->num_cols; i++) {
        bcpcol = blkdesc->bindinfo->columns[i];
        /* ssikorsk */
        if ((!blkdesc->identity_insert_on && bcpcol->column_identity) ||
                bcpcol->column_timestamp || 
                !_blk_is_binded(bcpcol)) {
            continue;
        }
        num_cols++;
    }

    tds_put_smallint(tds, num_cols);

    for (n = 2; n--; ) {
        for (i = 0; i < blkdesc->bindinfo->num_cols; i++) {
            bcpcol = blkdesc->bindinfo->columns[i];

            if (n  &&  is_blob_type(bcpcol->on_server.column_type))
                continue;
            else if (!n  &&  !is_blob_type(bcpcol->on_server.column_type))
                continue;

            /*
             * dont send the (meta)data for timestamp columns, or
             * identity columns (unless indentity_insert is enabled
             */

            /* ssikorsk */
            if ((!blkdesc->identity_insert_on && bcpcol->column_identity) ||
                    bcpcol->column_timestamp ||
                    !_blk_is_binded(bcpcol)) {
                continue;
            }

            tds_put_smallint(tds, bcpcol->column_usertype);
            tds_put_smallint(tds, bcpcol->column_flags);
            tds_put_byte(tds, (unsigned char)bcpcol->on_server.column_type);

            switch (bcpcol->column_varint_size) {
                case 4:
                    tds_put_int(tds, bcpcol->column_size);
                    break;
                case 2:
                    tds_put_smallint(tds, bcpcol->column_size);
                    break;
                case 1:
                    tds_put_byte(tds, bcpcol->column_size);
                    break;
                case 0:
                    break;
                default:
                    break;
            }

            if (is_numeric_type(bcpcol->on_server.column_type)) {
                tds_put_byte(tds, bcpcol->column_prec);
                tds_put_byte(tds, bcpcol->column_scale);
            }
            if (IS_TDS80(tds)
                    && is_collate_type(bcpcol->on_server.column_type)) {
                tds_put_n(tds, bcpcol->column_collation, 5);
            }
            if (is_blob_type(bcpcol->on_server.column_type)) {
                tds_put_smallint(tds, strlen(blkdesc->tablename));
                tds_put_string(tds, blkdesc->tablename, strlen(blkdesc->tablename));
            }
            /* FIXME support multibyte string */
            tds_put_byte(tds, (unsigned char)bcpcol->column_namelen);
            tds_put_string(tds, bcpcol->column_name, bcpcol->column_namelen);

        }
    }
    return CS_SUCCEED;
}

static CS_RETCODE
_blk_build_bcp_record(CS_BLKDESC *blkdesc, CS_INT offset)
{
    TDSSOCKET  *tds = blkdesc->con->tds_socket;
    TDSCOLUMN  *bindcol;

    static const unsigned char CHARBIN_NULL[] = { 0xff, 0xff };
    static const unsigned char GEN_NULL = 0x00;
    static const unsigned char textptr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    };
    static const unsigned char timestamp[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

    static const TDS_TINYINT textptr_size = 16;
    const unsigned char row_token = 0xd1;

    unsigned char *record;
    TDS_INT	 old_record_size;
    TDS_INT	 new_record_size;

    TDS_INT	     varint_4;
    TDS_SMALLINT varint_2;
    TDS_TINYINT  varint_1;

    int row_pos;
    int row_sz_pos;
    TDS_SMALLINT row_size;

    int var_cols_written = 0;

    CS_RETCODE rc = CS_SUCCEED;

    tdsdump_log(TDS_DBG_FUNC, "_blk_build_bcp_record(offset %d)\n", offset);

    record = blkdesc->bindinfo->current_row;
    old_record_size = blkdesc->bindinfo->row_size;
    new_record_size = 0;

    if (IS_TDS7_PLUS(tds)) {
        for (blkdesc->current_col = 0; blkdesc->current_col < blkdesc->bindinfo->num_cols; ++blkdesc->current_col) {

            bindcol = blkdesc->bindinfo->columns[blkdesc->current_col];

            /*
             * dont send the (meta)data for timestamp columns, or
             * identity columns (unless indentity_insert is enabled
             */

            /* ssikorsk */
            if ((!blkdesc->identity_insert_on && bindcol->column_identity) ||
                    bindcol->column_timestamp ||
                    !_blk_is_binded(bindcol) ||
                    is_blob_type(bindcol->on_server.column_type))
            {
                continue;
            }

            rc = _blk_get_col_data(blkdesc, bindcol, offset);

            if (rc == CS_FAIL) {
                tdsdump_log(TDS_DBG_INFO1, "blk_get_colData (column %d) failed\n", blkdesc->current_col + 1);
                return CS_FAIL;
            }

            tdsdump_log(TDS_DBG_INFO1, "gotten column %d length %d null %d\n",
                    blkdesc->current_col + 1, bindcol->bcp_column_data->datalen, bindcol->bcp_column_data->null_column);

            if (bindcol->bcp_column_data->null_column) {
                if (bindcol->column_nullable) {
                    switch (bindcol->on_server.column_type) {
                        case XSYBCHAR:
                        case XSYBVARCHAR:
                        case XSYBBINARY:
                        case XSYBVARBINARY:
                        case XSYBNCHAR:
                        case XSYBNVARCHAR:
                            memcpy(record, CHARBIN_NULL, 2);
                            record +=2;
                            new_record_size +=2;
                            break;
                        default:
                            *record = GEN_NULL;
                            record++;
                            new_record_size ++;
                            break;
                    }
                } else {
                    /* No value or default value available and NULL not allowed. col = %d row = %d. */
                    _ctclient_msg(blkdesc->con, "blk_rowxfer", 2, 7, 1, 142, "%d, %d",  blkdesc->current_col + 1, offset + 1);
                    return CS_FAIL;
                }
            } else {

                switch (bindcol->column_varint_size) {
                    case 4:
                        if (is_blob_type(bindcol->on_server.column_type)) {
                            *record = textptr_size; record++;
                            memcpy(record, textptr, 16); record += 16;
                            memcpy(record, timestamp, 8); record += 8;
                            new_record_size += 25;
                        }
                        varint_4 = bindcol->bcp_column_data->datalen;
#if WORDS_BIGENDIAN
                        tds_swap_datatype(SYBINT4, (unsigned char *)&varint_4);
#endif
                        memcpy(record, &varint_4, 4); record += 4; new_record_size +=4;
                        break;
                    case 2:
                        varint_2 = bindcol->bcp_column_data->datalen;
#if WORDS_BIGENDIAN
                        tds_swap_datatype(SYBINT2, (unsigned char *)&varint_2);
#endif
                        memcpy(record, &varint_2, 2); record += 2; new_record_size +=2;
                        break;
                    case 1:
                        varint_1 = bindcol->bcp_column_data->datalen;
                        if (is_numeric_type(bindcol->on_server.column_type))
                            varint_1 = tds_numeric_bytes_per_prec[bindcol->column_prec];
                        else
                            varint_1 = bindcol->bcp_column_data->datalen;
                        *record = varint_1; record++; new_record_size++;
                        break;
                    case 0:
                        break;
                }

#if WORDS_BIGENDIAN
                tds_swap_datatype(
                        tds_get_conversion_type(
                            bindcol->on_server.column_type, 
                            bindcol->on_server.column_size 
                            ),
                        bindcol->bcp_column_data->data
                        );
#endif

                if (is_numeric_type(bindcol->on_server.column_type)) {
                    CS_NUMERIC *num = (CS_NUMERIC *) bindcol->bcp_column_data->data;
                    if (IS_TDS7_PLUS(tds))
                        tds_swap_numeric((TDS_NUMERIC *) num);
                    memcpy(record, num->array, tds_numeric_bytes_per_prec[num->precision]);
                    record += tds_numeric_bytes_per_prec[num->precision];
                    new_record_size += tds_numeric_bytes_per_prec[num->precision];
                } else {
                    memcpy(record, bindcol->bcp_column_data->data, bindcol->bcp_column_data->datalen);
                    record += bindcol->bcp_column_data->datalen; 
                    new_record_size += bindcol->bcp_column_data->datalen;
                }

            }
            tdsdump_log(TDS_DBG_INFO1, "old_record_size = %d new size = %d \n",
                    old_record_size, new_record_size);
        }

        tds_put_byte(tds, row_token);   /* 0xd1 */
        tds_put_n(tds, blkdesc->bindinfo->current_row, new_record_size);

        blkdesc->current_col = 0;

        rc = _blk_send_blob_columns(blkdesc, offset);
    }  /* IS_TDS7_PLUS */
    else {
        memset(record, '\0', old_record_size);	/* zero the rowbuffer */

        /*
         * offset 0 = number of var columns
         * offset 1 = row number.  zeroed (datasever assigns)
         */
        row_pos = 2;

        blkdesc->bit_field_pos = 0;
        blkdesc->cur_bit_num = 0;

        if ((row_pos = _blk_add_fixed_columns(blkdesc, offset, record, row_pos)) == CS_FAIL)
            return CS_FAIL;

        row_sz_pos = row_pos;

        /* potential variable columns to write */

        if (blkdesc->var_cols) {
            row_pos = _blk_add_variable_columns(blkdesc, offset, record, row_pos, &var_cols_written);
            if (row_pos == CS_FAIL) {
                return CS_FAIL;
            }
        }

        row_size = row_pos;

        if (var_cols_written) {
            memcpy(&record[row_sz_pos], &row_size, sizeof(row_size));
            record[0] = var_cols_written;
        }

        tdsdump_log(TDS_DBG_INFO1, "old_record_size = %d new size = %d \n",
                old_record_size, row_size);

        tds_put_smallint(tds, row_size);
        tds_put_n(tds, record, row_size);

        /* row is done, now handle any text/image data */

        blkdesc->blob_cols = 0;
        blkdesc->current_col = 0;

        rc = _blk5_send_blob_columns(blkdesc, offset);
    }

    return rc;
}

static int
_blk_send_blob_columns(CS_BLKDESC *blkdesc, CS_INT offset)
{
    TDSSOCKET  *tds = blkdesc->con->tds_socket;
    TDSCOLUMN  *bindcol;
    int rc = CS_SUCCEED;

    static const unsigned char CHARBIN_NULL[] = { 0xff, 0xff };
    static const unsigned char GEN_NULL = 0x00;

    for (; blkdesc->current_col < blkdesc->bindinfo->num_cols; blkdesc->current_col++) {
        bindcol = blkdesc->bindinfo->columns[blkdesc->current_col];
        if (is_blob_type(bindcol->column_type)) {
            rc = _blk_get_col_data(blkdesc, bindcol, offset);

            if (rc == CS_FAIL) {
                tdsdump_log(TDS_DBG_INFO1, "blk_get_col_data (column %d) failed\n", blkdesc->current_col + 1);
                return CS_FAIL;
            }

            if (rc == CS_BLK_HAS_TEXT) {
                break;
            }

            if (bindcol->bcp_column_data->null_column) {
                if (bindcol->column_nullable) {
                    switch (bindcol->on_server.column_type) {
                        case XSYBCHAR:
                        case XSYBVARCHAR:
                        case XSYBBINARY:
                        case XSYBVARBINARY:
                        case XSYBNCHAR:
                        case XSYBNVARCHAR:
                            tds_put_n(tds, CHARBIN_NULL, 2);
                            break;
                        default:
                            tds_put_byte(tds, GEN_NULL);
                            break;
                    }
                } else {
                    /* No value or default value available and NULL not allowed. col = %d row = %d. */
                    _ctclient_msg(blkdesc->con, "blk_rowxfer", 2, 7, 1, 142, "%d, %d",  blkdesc->current_col + 1, offset + 1);
                    rc = CS_FAIL;
                }
            }
            else {
                /* Here we need to send blob binded directly (not via blk_textxfer).
                   But in DBAPI this functionality will not be used, so it is not
                   implemented. */
                _ctclient_msg(blkdesc->con, "blk_rowxfer", 2, 6, 1, 16843164, "%s", bindcol->column_name);
                rc = CS_FAIL;
            }
        }
    }

    return rc;
}

static int
_blk5_send_blob_columns(CS_BLKDESC *blkdesc, CS_INT offset)
{
    TDSSOCKET  *tds = blkdesc->con->tds_socket;
    TDSCOLUMN  *bindcol;
    int rc = CS_SUCCEED;

    for (; blkdesc->current_col < blkdesc->bindinfo->num_cols; blkdesc->current_col++) {
        bindcol = blkdesc->bindinfo->columns[blkdesc->current_col];
        if (is_blob_type(bindcol->column_type)) {
            rc = _blk_get_col_data(blkdesc, bindcol, offset);

            if (rc == CS_FAIL) {
                tdsdump_log(TDS_DBG_INFO1, "blk_get_col_data (column %d) failed\n", blkdesc->current_col + 1);
                return CS_FAIL;
            }

            if (rc == CS_BLK_HAS_TEXT) {
                break;
            }

            if (!bindcol->bcp_column_data->null_column) {
                /* unknown but zero */
                tds_put_smallint(tds, 0);
                tds_put_byte(tds, (unsigned char)bindcol->column_type);
                tds_put_byte(tds, 0xff - blkdesc->blob_cols);
                /*
                 * offset of txptr we stashed during variable
                 * column processing
                 */
                tds_put_smallint(tds, bindcol->column_textpos);
                tds_put_int(tds, bindcol->bcp_column_data->datalen);
                tds_put_n(tds, bindcol->bcp_column_data->data, bindcol->bcp_column_data->datalen);
            }
            blkdesc->blob_cols++;
        }
    }

    return rc;
}

static CS_RETCODE
_blk_add_fixed_columns(CS_BLKDESC * blkdesc, int offset, unsigned char * rowbuffer, int start)
{
    TDS_NUMERIC *num;
    int row_pos = start;
    TDSCOLUMN *bcpcol;
    int cpbytes;

    int i, j;

    tdsdump_log(TDS_DBG_FUNC, "_blk_add_fixed_columns (offset %d)\n", offset);

    for (i = 0; i < blkdesc->bindinfo->num_cols; i++) {

        bcpcol = blkdesc->bindinfo->columns[i];

        if (!is_nullable_type(bcpcol->on_server.column_type) && !(bcpcol->column_nullable)) {

            tdsdump_log(TDS_DBG_FUNC, "_blk_add_fixed_columns column %d is a fixed column\n", i + 1);

            if (( _blk_get_col_data(blkdesc, bcpcol, offset)) != CS_SUCCEED) {
                return CS_FAIL;
            }

            if (bcpcol->bcp_column_data->null_column) {
                /* No value or default value available and NULL not allowed. col = %d row = %d. */
                _ctclient_msg(blkdesc->con, "blk_rowxfer", 2, 7, 1, 142, "%d, %d",  i + 1, offset + 1);
                return CS_FAIL;
            }

            if (is_numeric_type(bcpcol->column_type)) {
                num = (TDS_NUMERIC *) bcpcol->bcp_column_data->data;
                cpbytes = tds_numeric_bytes_per_prec[num->precision];
                memcpy(&rowbuffer[row_pos], num->array, cpbytes);
            }
            else if (bcpcol->on_server.column_type == SYBBIT) {
                if (blkdesc->bit_field_pos == 0) {
                    memcpy(&rowbuffer[row_pos], bcpcol->bcp_column_data->data, 1);
                    blkdesc->bit_field_pos = row_pos;
                }
                else {
                    if (*bcpcol->bcp_column_data->data)
                        rowbuffer[blkdesc->bit_field_pos] |= (1 << blkdesc->cur_bit_num);
                    /* row_pos shouldn't change, but it's increased below */
                    --row_pos;
                }

                if (++blkdesc->cur_bit_num == 8) {
                    blkdesc->cur_bit_num = 0;
                    blkdesc->bit_field_pos = 0;
                }
            }
            else {
                cpbytes = bcpcol->bcp_column_data->datalen > bcpcol->column_size ? bcpcol->column_size : bcpcol->bcp_column_data->datalen;
                memcpy(&rowbuffer[row_pos], bcpcol->bcp_column_data->data, cpbytes);

                /* CHAR data may need padding out to the database length with blanks */

                if (bcpcol->column_type == SYBCHAR && cpbytes < bcpcol->column_size) {
                    for (j = cpbytes; j <  bcpcol->column_size; j++)
                        rowbuffer[row_pos + j] = ' ';
                }
            }

            row_pos += bcpcol->column_size;

        }
    }
    return row_pos;
}

/**
 * Add variable size columns to the row
 */
static int
_blk_add_variable_columns(CS_BLKDESC * blkdesc, int offset, unsigned char * rowbuffer, int start, int *var_cols)
{
    TDSCOLUMN   *bcpcol;
    TDS_NUMERIC *num;
    int row_pos;
    int cpbytes;

    unsigned char offset_table[256];
    unsigned char adjust_table[256];

    int offset_pos     = 0;
    int adjust_pos     = 0;
    int num_cols       = 0;
    int last_adjustment_increment = 0;
    int this_adjustment_increment = 0;

    int adjust_table_entries_required;
    int i;
    CS_RETCODE rc;

    /*
     * Skip over two bytes. These will be used to hold the entire record length
     * once the record has been completely built.
     */

    row_pos = start + 2;

    /* for each column in the target table */

    tdsdump_log(TDS_DBG_FUNC, "_blk_add_variable_columns (offset %d)\n", offset);

    for (blkdesc->current_col = 0; blkdesc->current_col < blkdesc->bindinfo->num_cols; blkdesc->current_col++) {

        bcpcol = blkdesc->bindinfo->columns[blkdesc->current_col];

        /*
         * is this column of "variable" type, i.e. NULLable
         * or naturally variable length e.g. VARCHAR
         */

        if (is_nullable_type(bcpcol->on_server.column_type) || bcpcol->column_nullable) {

            tdsdump_log(TDS_DBG_FUNC, "_blk_add_variable_columns column %d is a variable column\n", blkdesc->current_col + 1);

            rc = _blk_get_col_data(blkdesc, bcpcol, offset);

            if (rc == CS_FAIL) {
                tdsdump_log(TDS_DBG_INFO1, "_blk_add_variable_columns (column %d) failed\n", blkdesc->current_col + 1);
                return CS_FAIL;
            }

            /*
             * but if its a NOT NULL column, and we have no data
             * throw an error
             */

            if (!(bcpcol->column_nullable) && bcpcol->bcp_column_data->null_column) {
                /* No value or default value available and NULL not allowed. col = %d row = %d. */
                _ctclient_msg(blkdesc->con, "blk_rowxfer", 2, 7, 1, 142, "%d, %d",  blkdesc->current_col + 1, offset + 1);
                return CS_FAIL;
            }

            if (is_blob_type(bcpcol->column_type)) {
                cpbytes = bcpcol->bcp_column_data->null_column? 0: 16;
                bcpcol->column_textpos = row_pos;               /* save for data write */
            } else if (is_numeric_type(bcpcol->column_type)) {
                num = (TDS_NUMERIC *) bcpcol->bcp_column_data->data;
                cpbytes = tds_numeric_bytes_per_prec[num->precision];
                memcpy(&rowbuffer[row_pos], num->array, cpbytes);
            } else {
                /* compute the length to copy to the row ** buffer */
                if (bcpcol->bcp_column_data->null_column) {
                    cpbytes = 0;
                } else {
                    cpbytes = bcpcol->bcp_column_data->datalen > bcpcol->column_size ? bcpcol->column_size : bcpcol->bcp_column_data->datalen;
                    memcpy(&rowbuffer[row_pos], bcpcol->bcp_column_data->data, cpbytes);
                }
            }

            /*
             * update offset table. Each entry in the offset table is a single byte
             * so can only hold a maximum value of 255. If the real offset is more
             * than 255 we will have to add one or more entries in the adjust table
             */

            offset_table[offset_pos++] = row_pos % 256;

            /* increment count of variable columns added to the record */

            num_cols++;

            /*
             * how many times does 256 have to be added to the one byte offset to
             * calculate the REAL offset...
             */

            this_adjustment_increment = row_pos / 256;

            /* has this changed since we did the last column...      */

            if (this_adjustment_increment > last_adjustment_increment) {

                /*
                 * add n entries to the adjust table. each entry represents
                 * an adjustment of 256 bytes, and each entry holds the
                 * column number for which the adjustment needs to be made
                 */

                for ( adjust_table_entries_required = this_adjustment_increment - last_adjustment_increment;
                        adjust_table_entries_required > 0;
                        adjust_table_entries_required-- ) {
                    adjust_table[adjust_pos++] = num_cols;
                }
                last_adjustment_increment = this_adjustment_increment;
            }

            row_pos += cpbytes;
        }
    }

    if (num_cols) {	
        /*
         * If we have written any variable columns to the record, add entries
         * to the offset and adjust tables for the end of data offset (as above).
         */

        offset_table[offset_pos++] = row_pos % 256;

        /*
         * Write the offset data etc. to the end of the record, starting with
         * a count of variable columns (plus 1 for the eod offset)
         */

        rowbuffer[row_pos++] = num_cols + 1;

        /* write the adjust table (right to left) */
        for (i = adjust_pos - 1; i >= 0; i--) {
            rowbuffer[row_pos++] = adjust_table[i];
        }

        /* write the offset table (right to left) */
        for (i = offset_pos - 1; i >= 0; i--) {
            rowbuffer[row_pos++] = offset_table[i];
        }
    }

    *var_cols = num_cols;

    if (num_cols == 0) /* we haven't written anything */
        return start;
    else
        return row_pos;
}

static CS_RETCODE
_blk_get_col_data(CS_BLKDESC *blkdesc, TDSCOLUMN *bindcol, int offset)
{
    int result = 0;

    CS_INT null_column = 0;

    unsigned char *src = NULL;

    CS_INT      srctype = 0;
    CS_INT      srclen  = 0;
    CS_INT      destlen  = 0;
    CS_SMALLINT *nullind = NULL;
    CS_INT      *datalen = NULL;
    CS_CONTEXT *ctx = blkdesc->con->ctx;
    CS_DATAFMT srcfmt, destfmt;

    /*
     * retrieve the initial bound column_varaddress
     * and increment it if offset specified
     */

    /* ssikorsk
    if (is_blob_type(bindcol->column_type) && bindcol->column_varaddr == NULL) {
        return CS_BLK_HAS_TEXT;
    }
    */

    src = (unsigned char *) bindcol->column_varaddr;
    src += offset * bindcol->column_bindlen;
    srctype = bindcol->column_bindtype; 		/* used to pass to cs_convert */

    if (bindcol->column_nullbind) {
        nullind = bindcol->column_nullbind;
        nullind += offset;
    }
    if (bindcol->column_lenbind) {
        datalen = bindcol->column_lenbind;
        datalen += offset;
    }

    if (!src) {
        if (nullind  &&  *nullind == -1) {
            bindcol->bcp_column_data->datalen = 0;
            bindcol->bcp_column_data->null_column = 1;
        }
        else if (bindcol->column_hasdefault) {
            src = bindcol->column_default;
            srctype = _ct_get_client_type(blkdesc->con->ctx, bindcol->column_type, bindcol->column_usertype, bindcol->on_server.column_size);
            datalen = &bindcol->column_def_size;
        }
        else {
            bindcol->bcp_column_data->datalen = 0;

            if (is_blob_type(bindcol->column_type)) {
                bindcol->bcp_column_data->null_column = 0;
                return CS_BLK_HAS_TEXT;
            }

            bindcol->bcp_column_data->null_column = 1;
        }
    }

    if (src) {
        tdsdump_log(TDS_DBG_INFO1, "blk_get_col_data srctype = %d \n", srctype);
        tdsdump_log(TDS_DBG_INFO1, "blk_get_col_data datalen = %d \n", *datalen);

        if (*datalen) {
            if (*datalen == CS_UNUSED) {
                switch (srctype) {
                    case CS_LONG_TYPE:	    srclen = 8; break;
                    case CS_FLOAT_TYPE:	    srclen = 8; break;
                    case CS_MONEY_TYPE:	    srclen = 8; break;
                    case CS_DATETIME_TYPE:  srclen = 8; break;
                    case CS_INT_TYPE:	    srclen = 4; break;
                    case CS_REAL_TYPE:	    srclen = 4; break;
                    case CS_MONEY4_TYPE:	srclen = 4; break;
                    case CS_DATETIME4_TYPE: srclen = 4; break;
                    case CS_SMALLINT_TYPE:  srclen = 2; break;
                    case CS_TINYINT_TYPE:   srclen = 1; break;
                    default:
                                            printf("error not fixed length type (%d) and datalen not specified\n", bindcol->column_bindtype);
                                            return CS_FAIL;
                }

            } else {
                srclen = *datalen;
            }
        }
        if (srclen == 0) {
            if (nullind &&  *nullind == -1) {
                null_column = 1;
            }
        }

        if (!null_column) {

            srcfmt.datatype = srctype;
            srcfmt.maxlength = srclen;

            destfmt.datatype  = _ct_get_client_type(blkdesc->con->ctx, bindcol->column_type, bindcol->column_usertype, bindcol->on_server.column_size);
            destfmt.maxlength = bindcol->on_server.column_size;
            destfmt.precision = bindcol->column_prec;
            destfmt.scale     = bindcol->column_scale;

            destfmt.format	= CS_FMT_UNUSED;

            /* if convert return FAIL mark error but process other columns */
            if ((result = cs_convert(ctx, &srcfmt, (CS_VOID *) src,
                            &destfmt, (CS_VOID *) bindcol->bcp_column_data->data, &destlen) != CS_SUCCEED)) {
                tdsdump_log(TDS_DBG_INFO1, "convert failed for %d \n", srcfmt.datatype);
                return CS_FAIL;
            }
        }

        bindcol->bcp_column_data->datalen = destlen;
        bindcol->bcp_column_data->null_column = null_column;
    }

    return CS_SUCCEED;
}


static int
_blk_is_binded(TDSCOLUMN * colinfo)
{
    if (colinfo) {
        int result = 0;

        if (colinfo->column_varaddr == NULL) {
            if (colinfo->column_lenbind != NULL) {
                if (*colinfo->column_lenbind == 0) {
                    if (colinfo->column_nullbind != NULL) {
                        /*if (*colinfo->column_nullbind == -1) {*/
                            /* null-value for blk_textxfer ...*/
                            result = 1;
                        /*}*/
                    } 
                } else {
                    result = 1;
                }
            } 
        } else {
            if (colinfo->column_lenbind != NULL) {
                if (*colinfo->column_lenbind == 0) {
                    if (colinfo->column_nullbind != NULL) {
                        /*if (*colinfo->column_nullbind == -1) {*/
                            /* null-value for blk_textxfer ...*/
                            result = 1;
                        /*}*/
                    } 
                } else {
                    result = 1;
                }
            }
        }

        if (result) {
            return 1;
        } else {
            return 0;
        }
    }

    return 0;
}

static int
_blk_read_column_defaults(CS_BLKDESC * blkdesc)
{
    TDS_INT res_type;
    TDS_INT done_flags;
    int     rc;
    int     ret = TDS_SUCCEED;

    int num_defs = 0;
    int colnum, res_colnum;
    TDS_TINYINT byte_val;
    unsigned char* src;
    TDSRESULTINFO* resinfo;
    TDSRESULTINFO* bindinfo = blkdesc->bindinfo;
    TDSCOLUMN* curcol;
    TDSCOLUMN* bindcol;

    TDSSOCKET *tds = blkdesc->con->tds_socket;

    /* Read metainformation about all columns */
    for (colnum = 0; ; ++colnum) {
        rc = tds_process_tokens(tds, &res_type, &done_flags,
                                    TDS_RETURN_ROW | TDS_RETURN_DONE);
        if (rc != TDS_SUCCEED) {
            ret = TDS_FAIL;
            break;
        }

        if (res_type == TDS_DONE_RESULT) {
            if ((done_flags & TDS_DONE_ERROR) != 0)
                ret = TDS_FAIL;
            /* First recordset with columnsmetainfo is done - go to the next */
            break;
        }

        if (res_type != TDS_ROW_RESULT)
            continue;

        resinfo = tds->current_results;

#if ENABLE_EXTRA_CHECKS
        /* Most probably the format of result is fixed, but couple of asserts
           are still needed to be sure */
        assert(resinfo->num_cols == 12);
        assert(colnum < bindinfo->num_cols);
#endif

        bindcol = bindinfo->columns[colnum];

/*
There should be some way to check that we are not messing with different types which need conversion,
for example. But I didn't find any clear way to do it. This code fails to compare as equal
LONGBINARY and VARBINARY.

#if ENABLE_EXTRA_CHECKS
        curcol = resinfo->columns[5];
        assert(curcol->column_type == SYBINT1);
        src = &(resinfo->current_row[curcol->column_offset]);
        byte_val = *((TDS_TINYINT*) src);
        assert(_ct_get_client_type(blkdesc->con->ctx, byte_val, 0, bindcol->column_size)
                == _ct_get_client_type(blkdesc->con->ctx, bindcol->column_type, 0, bindcol->column_size));
#endif
*/

        /* Read "default" flag of the column */
        curcol = resinfo->columns[9];

#if ENABLE_EXTRA_CHECKS
        assert(curcol->column_type == SYBINT1);
#endif

        src = &(resinfo->current_row[curcol->column_offset]);
        byte_val = *((TDS_TINYINT*) src);

        if (byte_val == 1) {
            ++num_defs;
            bindcol->column_hasdefault = 1;
        }
    }

    if (ret == TDS_SUCCEED  &&  num_defs > 0
        &&  tds_process_tokens(tds, &res_type, &done_flags, TDS_RETURN_ROW) == TDS_SUCCEED)
    {
        resinfo = tds->current_results;

#if ENABLE_EXTRA_CHECKS
        assert(resinfo->num_cols == num_defs);
#endif

        /* Now read all default values */
        res_colnum = 0;
        for (colnum = 0; colnum < bindinfo->num_cols; ++colnum) {
            bindcol = bindinfo->columns[colnum];
            if (!bindcol->column_hasdefault)
                continue;

            curcol = resinfo->columns[res_colnum];
            ++res_colnum;
            src = &(resinfo->current_row[curcol->column_offset]);
            /* Maybe it's better to ignore all requests of defaults after first request */
            if (bindcol->column_default) {
                bindcol->column_default = realloc(bindcol->column_default, curcol->column_size);
            }
            else {
                bindcol->column_default = malloc(curcol->column_size);
            }
            if (!bindcol->column_default) {
                ret = TDS_FAIL;
                break;
            }

            bindcol->column_def_size = curcol->column_size;
            memcpy(bindcol->column_default, src, bindcol->column_def_size);
        }
    }

    if (ret == TDS_SUCCEED) {
        ret = tds_process_simple_query(tds);
    }

    return ret;
}
