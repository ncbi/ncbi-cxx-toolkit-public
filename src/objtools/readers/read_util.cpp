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
 * Authors:  Frank Ludwig
 *
 * File Description: Common file reader utility functions.
 *
 */

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objtools/readers/read_util.hpp>
#include <objtools/readers/reader_base.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
void CReadUtil::Tokenize(
    const string& str,
    const string& delim,
    vector< string >& parts )
//  ----------------------------------------------------------------------------
{
    string temp;
    bool inQuote(false);
    const char joiner('#');

    for (size_t i=0; i < str.size(); ++i) {
        switch(str[i]) {

        default:
            break;
        case '\"':
            inQuote = inQuote ^ true;
            break;
        case ' ':
            if (inQuote) {
                if (temp.empty())
                    temp = str;
                temp[i] = joiner;
            }
            break;
        }
    }
    if (temp.empty()) {
        NStr::Tokenize(str, delim, parts, NStr::eMergeDelims);
        return;
    }
    NStr::Tokenize(temp, delim, parts, NStr::eMergeDelims);
    for (size_t j=0; j < parts.size(); ++j) {
        for (size_t i=0; i < parts[j].size(); ++i) {
            if (parts[j][i] == joiner) {
                parts[j][i] = ' ';
            }
        }
    }
}


//  -----------------------------------------------------------------
CRef<CSeq_id> CReadUtil::AsSeqId(
    const string& rawId,
    unsigned int flags)
//  -----------------------------------------------------------------
{
    CRef<CSeq_id> pId;
    if (flags & CReaderBase::fAllIdsAsLocal) {
        pId.Reset(new CSeq_id(CSeq_id::e_Local, rawId));
        return pId;
    }

    try {
        pId.Reset(new CSeq_id(rawId));
        if (!pId) {
            pId.Reset(new CSeq_id(CSeq_id::e_Local, rawId));
            return pId;
        }
        if (pId->IsGi()) {
            if (flags & CReaderBase::fNumericIdsAsLocal) {
                pId.Reset(new CSeq_id(CSeq_id::e_Local, rawId));
                return pId;
            }
            if (pId->GetGi() < 500) {
                pId.Reset(new CSeq_id(CSeq_id::e_Local, rawId));
                return pId;
            }
        }
        return pId;
    }
    catch(...) {
        pId.Reset(new CSeq_id(CSeq_id::e_Local, rawId));
    }
    return pId;
}

END_objects_SCOPE
END_NCBI_SCOPE
