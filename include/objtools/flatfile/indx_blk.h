/* indx_blk.h
 *
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
 * File Name:  indx_blk.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 * -----------------
 */

#ifndef _INDEXBLOCK_
#define _INDEXBLOCK_

typedef struct file_infomation_block {
    Char   str[256];                    /* the current string data */
    Int4   line;                        /* the current line number */
    size_t pos;                         /* the current file position */
} FinfoBlk, PNTR FinfoBlkPtr;

typedef struct ind_blk_next {
    IndexblkPtr              ibp;
    struct ind_blk_next PNTR next;
} IndBlkNext, PNTR IndBlkNextPtr;

ncbi::CRef<ncbi::objects::CDate_std> GetUpdateDate PROTO((CharPtr ptr, Int2 source));

/**********************************************************/
bool        XReadFile PROTO((FILE PNTR fp, FinfoBlkPtr finfo));
bool        XReadFileBuf PROTO((FileBufPtr fpbuf, FinfoBlkPtr finfo));
bool        SkipTitle PROTO((FILE PNTR fp, FinfoBlkPtr finfo, const Char *str, Int2 len));
bool        SkipTitleBuf PROTO((FileBufPtr fpbuf, FinfoBlkPtr finfo, const Char *str, Int2 len));
bool        FindNextEntry PROTO((bool end_of_file, FILE PNTR ifp,
                                FinfoBlkPtr finfo, const Char *str, Int2 len));
bool        FindNextEntryBuf PROTO((bool end_of_file, FileBufPtr ifpbuf,
                                   FinfoBlkPtr finfo, const Char *str, Int2 len));
IndexblkPtr InitialEntry PROTO((ParserPtr pp, FinfoBlkPtr finfo));
bool        GetAccession PROTO((ParserPtr pp, CharPtr str, IndexblkPtr entry, Int4 skip));
void        CloseFiles PROTO((ParserPtr pp));
void        MsgSkipTitleFail PROTO((const Char *flatfile, FinfoBlkPtr finfo));
bool        FlatFileIndex PROTO((ParserPtr pp, void(*fun)(IndexblkPtr entry, CharPtr offset, Int4 len)));
void        ResetParserStruct PROTO((ParserPtr pp));
bool        QSIndex PROTO((ParserPtr pp, IndBlkNextPtr ibnp));

bool  IsValidAccessPrefix PROTO((CharPtr acc, CharPtr* accpref));

void  DelNoneDigitTail PROTO((CharPtr str));
Int4  fta_if_wgs_acc PROTO((const Char* acc));
Int2  CheckSTRAND PROTO((CharPtr str));
Int2  CheckTPG PROTO((CharPtr str));
Int2  CheckDIV PROTO((CharPtr str));
Int4  IsNewAccessFormat PROTO((const Char* acnum));
bool  IsSPROTAccession PROTO((const Char PNTR acc));
Int2  XMLCheckSTRAND PROTO ((CharPtr str));
Int2  XMLCheckTPG PROTO((CharPtr str)); 
Int2  CheckNADDBJ PROTO((CharPtr str));
Int2  CheckNA PROTO((CharPtr str));

bool CkLocusLinePos PROTO((CharPtr offset, Int2 source, LocusContPtr lcp, bool is_mga));

#endif
