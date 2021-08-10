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

#include "ftablock.h"

BEGIN_NCBI_SCOPE

struct FinfoBlk {
    Char   str[256];                    /* the current string data */
    Int4   line;                        /* the current line number */
    size_t pos;                         /* the current file position */
};

using FinfoBlkPtr = FinfoBlk*;





typedef struct ind_blk_next {
    IndexblkPtr              ibp;
    struct ind_blk_next *next;
} IndBlkNext, *IndBlkNextPtr;

CRef<objects::CDate_std> GetUpdateDate(char* ptr, Parser::ESource source);

/**********************************************************/
bool        XReadFileBuf(FileBuf& fileBuf, FinfoBlkPtr finfo);
bool        SkipTitleBuf(FileBuf& fileBuf, FinfoBlkPtr finfo, const Char *str, Int2 len);
bool        SkipTitleBuf(FileBuf& fileBuf, FinfoBlkPtr finfo, const CTempString& keyword);
bool        FindNextEntryBuf(bool end_of_file, FileBuf& fileBuf,
                             FinfoBlkPtr finfo, const char* str, Int2 len);
bool        FindNextEntryBuf(bool end_of_file, FileBuf& fileBuf, 
                             FinfoBlkPtr finfo, const CTempString& keyword);

IndexblkPtr InitialEntry(ParserPtr pp, FinfoBlkPtr finfo);
bool        GetAccession(ParserPtr pp, char* str, IndexblkPtr entry, Int4 skip);
//bool        GetAccession(const Parser& parseInfo, const CTempString& str, IndexblkPtr entry, int skip);
void        CloseFiles(ParserPtr pp);
void        MsgSkipTitleFail(const Char *flatfile, FinfoBlkPtr finfo);
bool        FlatFileIndex(ParserPtr pp, void(*fun)(IndexblkPtr entry, char* offset, Int4 len));
void        ResetParserStruct(ParserPtr pp);
bool        QSIndex(ParserPtr pp, IndBlkNextPtr ibnp);

//bool  IsValidAccessPrefix(char* acc, char** accpref);

void  DelNoneDigitTail(char* str);
int  fta_if_wgs_acc(const CTempString& accession);
Int2  CheckSTRAND(const char* str);
Int2  CheckTPG(char* str);
Int2  CheckDIV(char* str);
Int4  IsNewAccessFormat(const char* acnum);
bool  IsSPROTAccession(const char* acc);
Int2  XMLCheckSTRAND(char* str);
Int2  XMLCheckTPG(char* str);
Int2  CheckNADDBJ(char* str);
Int2  CheckNA(char* str);

bool CkLocusLinePos(char* offset, Parser::ESource source, LocusContPtr lcp, bool is_mga);

const Char **GetAccArray(Parser::ESource source);
objects::CSeq_id::E_Choice  GetNucAccOwner(const char* accession, bool is_tpa);
Uint1        GetProtAccOwner(const char* acc);
//void    FreeParser(ParserPtr pp);
END_NCBI_SCOPE

#endif
