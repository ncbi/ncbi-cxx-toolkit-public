/* $Id$
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
 * File Name: indx_blk.h
 *
 * Author: Karl Sirotkin, Hsiu-Chuan Chen
 *
 * File Description:
 *
 */

#ifndef _INDEXBLOCK_
#define _INDEXBLOCK_

#include "ftablock.h"

BEGIN_NCBI_SCOPE

struct FinfoBlk {
    Char   str[256]; /* the current string data */
    Int4   line;     /* the current line number */
    size_t pos;      /* the current file position */

    FinfoBlk() :
        line(0),
        pos(0)
    {
        str[0] = 0;
    }
};

using TIndBlkList = forward_list<unique_ptr<Indexblk>>;

CRef<objects::CDate_std> GetUpdateDate(string_view, Parser::ESource source);

/**********************************************************/
bool XReadFileBuf(FileBuf& fileBuf, FinfoBlk& finfo);
bool SkipTitleBuf(FileBuf& fileBuf, FinfoBlk& finfo, string_view keyword);
bool FindNextEntryBuf(bool end_of_file, FileBuf& fileBuf, FinfoBlk& finfo, string_view keyword);

IndexblkPtr InitialEntry(ParserPtr pp, FinfoBlk& finfo);
bool        GetAccession(const Parser* pp, string_view str, IndexblkPtr entry, unsigned skip);

void CloseFiles(ParserPtr pp);
void MsgSkipTitleFail(const Char* flatfile, FinfoBlk& finfo);
bool FlatFileIndex(ParserPtr pp, void (*fun)(IndexblkPtr entry, char* offset, Int4 len));
void ResetParserStruct(ParserPtr pp);
bool QSIndex(ParserPtr pp, const TIndBlkList& ibl, unsigned ibl_size);

// bool IsValidAccessPrefix(char* acc, char** accpref);

void DelNoneDigitTail(char* str);
void DelNonDigitTail(string& str);
int  fta_if_wgs_acc(string_view accession);
int  CheckSTRAND(const string& str);
int  CheckTPG(const string& str);
Int2 CheckDIV(const char* str);
Int4 IsNewAccessFormat(const char* acnum);
bool IsSPROTAccession(const char* acc);
Int2 XMLCheckSTRAND(string_view str);
Int2 XMLCheckTPG(string_view str);
Int2 CheckNADDBJ(const char* str);
Int2 CheckNA(const char* str);

bool CkLocusLinePos(char* offset, Parser::ESource source, LocusContPtr lcp, bool is_mga);

const Char**               GetAccArray(Parser::ESource source);
bool                       isSupportedAccession(objects::CSeq_id::E_Choice type);
objects::CSeq_id::E_Choice GetNucAccOwner(string_view acc);
objects::CSeq_id::E_Choice GetProtAccOwner(string_view acc);
// void    FreeParser(ParserPtr pp);
END_NCBI_SCOPE

#endif
