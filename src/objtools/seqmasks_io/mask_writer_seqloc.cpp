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
 * Author:  Christiam Camacho
 *
 * File Description:
 *   CMaskWriterSeqLoc class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/seqmasks_io/mask_writer_seqloc.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Packed_seqint.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CMaskWriterSeqLoc::CMaskWriterSeqLoc( CNcbiOstream& arg_os, 
                                      const string & format )
: CMaskWriter( arg_os )
{
    if (format == "seqloc_asn1_bin") {
        m_OutputFormat = eSerial_AsnBinary;
    } else if (format == "seqloc_asn1_text") {
        m_OutputFormat = eSerial_AsnText;
    } else if (format == "seqloc_xml") {
        m_OutputFormat = eSerial_Xml;
    } else {
        throw runtime_error("Invalid output format: " + format);
    }
}

//-------------------------------------------------------------------------
void CMaskWriterSeqLoc::Print( objects::CBioseq_Handle& bsh,
                               const CSeqMasker::TMaskList & mask,
                               bool /* match_id */ )
{
    if (mask.empty()) {
        return;
    }

    CPacked_seqint::TRanges masked_ranges;
    masked_ranges.reserve(mask.size());
    ITERATE(CSeqMasker::TMaskList, itr, mask) {
        masked_ranges.push_back
            (CPacked_seqint::TRanges::value_type(itr->first, itr->second));
    }

    CConstRef<CSeq_id> id = bsh.GetSeqId();

    CSeq_loc seqloc(const_cast<CSeq_id&>(*id), masked_ranges);
    switch (m_OutputFormat) {
    case eSerial_AsnBinary:
        os << MSerial_AsnBinary << seqloc;
        break;
    case eSerial_AsnText:
        os << MSerial_AsnText << seqloc;
        break;
    case eSerial_Xml:
        os << MSerial_Xml << seqloc;
        break;
    default:
        throw runtime_error("Invalid output format!");
    }
}


END_NCBI_SCOPE
