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
* Author:  Kevin Bealer
*
*/

/** @file multisource_util.cpp
* Utility functions and classes for multisource app.
*/

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <serial/typeinfo.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objtools/blast/seqdb_writer/multisource_util.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
#endif

void ReadTextFile(CNcbiIstream   & f,
                  vector<string> & lines)
{
    // Arbitrary number, to avoid excess realloc()s.  A smarter
    // version might use string::swap() to avoid copying strings, but
    // in the case of gcc, strings should be reference counted, so
    // copying is already cheap.

    lines.reserve(128);

    while(f && (! f.eof())) {
        string s;
        NcbiGetlineEOL(f, s);

        if (s.size()) {
            lines.push_back(s);
        }
    }
}

/// Read a set of GI lists, each a vector of strings, and combine the
/// bits into the resulting linkbits map.

void MapToLMBits(const TLinkoutMap & gilist, TIdToBits & id2links)
{
    ITERATE(TLinkoutMap, iter1, gilist) {
        int bitnum = iter1->first;
        const vector<string> & v = iter1->second;

        ITERATE(vector<string>, iter2, v) {
            string key = AccessionToKey(*iter2);

            if (key.size()) {
                id2links[key] |= bitnum;
            }
        }
    }
}

string AccessionToKey(const string & acc)
{
    TGi gi(ZERO_GI);
    CRef<CSeq_id> seqid;
    bool specific(false);

    string str;

    if (CheckAccession(acc, gi, seqid, specific)) {
        if (seqid.Empty()) {
            if (gi != ZERO_GI) {
                str = "gi|";
                str += NStr::IntToString(GI_TO(int, gi));
            }
        } else {
            GetSeqIdKey(*seqid, str);
        }
    }

    return str;
}

bool CheckAccession(const string  & acc,
                    TGi           & gi,
                    CRef<objects::CSeq_id> & seqid,
                    bool          & specific)
{
    specific = true;
    gi = ZERO_GI;
    seqid.Reset();

    // Case 1: Numeric GI

    bool digits = !! acc.size();

    for(unsigned i = 0; i < acc.size(); i++) {
        if (! isdigit(acc[i])) {
            digits = false;
            break;
        }
    }

    if (digits) {
        gi = GI_FROM(int, NStr::StringToInt(acc));
        return true;
    }

    try {
        seqid.Reset(new CSeq_id(acc));
    }
    catch(CException &) {
        return false;
    }

    if (seqid->IsGi()) {
        gi = seqid->GetGi();
        seqid.Reset();
        return true;
    }

    // Case 2: Other Seq-ids.

    const CTextseq_id * tsi = seqid->GetTextseq_Id();

    if (tsi != NULL) {
        specific = tsi->IsSetVersion();
    }

    return true;
}

void GetSeqIdKey(const objects::CSeq_id & id, string & key)
{
    id.GetLabel(& key, CSeq_id::eBoth, CSeq_id::fLabel_GeneralDbIsContent);
}

void GetDeflineKeys(const objects::CBlast_def_line & defline,
                    vector<string>        & keys)
{
    keys.clear();

    ITERATE(CBlast_def_line::TSeqid, iter, defline.GetSeqid()) {
        string key;
        GetSeqIdKey(**iter, key);
        keys.push_back(key);
    }
}

END_NCBI_SCOPE
