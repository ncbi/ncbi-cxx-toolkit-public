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
 *   CMaskWriterBlastDbMaskInfo class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/seqmasks_io/mask_writer_blastdb_maskinfo.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <sstream>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CMaskWriterBlastDbMaskInfo::CMaskWriterBlastDbMaskInfo
    ( CNcbiOstream& arg_os, 
      const string & format,
      int algo_id,
      EBlast_filter_program filt_program,
      const string & algo_options )
: CMaskWriter( arg_os )
{
    m_BlastDbMaskInfo.Reset(new CBlast_db_mask_info());
    m_BlastDbMaskInfo->SetAlgo_id(algo_id);
    m_BlastDbMaskInfo->SetAlgo_program((int)filt_program);
    m_BlastDbMaskInfo->SetAlgo_options(algo_options);

    if (format == "maskinfo_asn1_bin") {
        m_OutputFormat = eSerial_AsnBinary;
    } else if (format == "maskinfo_asn1_text") {
        m_OutputFormat = eSerial_AsnText;
    } else if (format == "maskinfo_xml") {
        m_OutputFormat = eSerial_Xml;
    } else {
        throw runtime_error("Invalid output format: " + format);
    }
}

template <class T>
void s_WriteObject(CRef<T> obj, CNcbiOstream& os, ESerialDataFormat fmt)
{
    switch (fmt) {
    case eSerial_AsnBinary:
        os << MSerial_AsnBinary << *obj;
        break;
    case eSerial_AsnText:
        os << MSerial_AsnText << *obj;
        break;
    case eSerial_Xml:
        os << MSerial_Xml << *obj;
        break;
    default:
        throw runtime_error("Invalid output format!");
    }
}

CMaskWriterBlastDbMaskInfo::~CMaskWriterBlastDbMaskInfo()
{
    if (m_ListOfMasks.empty()) {
        CRef<CBlast_mask_list> empty_list(new CBlast_mask_list);
        empty_list->SetMasks();
        empty_list->SetMore(false);
        m_ListOfMasks.push_back(empty_list);
    }

    x_ConsolidateListOfMasks();

    m_BlastDbMaskInfo->SetMasks(*m_ListOfMasks.front());
    s_WriteObject(m_BlastDbMaskInfo, os, m_OutputFormat);

    for (TBlastMaskLists::size_type i = 1; i < m_ListOfMasks.size(); i++) {
        s_WriteObject(m_ListOfMasks[i], os, m_OutputFormat);
    }
}

void
CMaskWriterBlastDbMaskInfo::x_ConsolidateListOfMasks()
{
    static const size_t kMaxSeqLocsPerBatch = 10000;
    TBlastMaskLists consolidated_list;
    TBlastMaskLists::size_type i = 0;       // index into m_ListOfMasks

    while (i < m_ListOfMasks.size()) {

        size_t seqlocs_read = 0;
        consolidated_list.push_back
            (TBlastMaskLists::value_type(new CBlast_mask_list));

        for (;(i < m_ListOfMasks.size() && seqlocs_read < kMaxSeqLocsPerBatch); 
              i++) {
            if (m_ListOfMasks[i]->GetMasks().empty()) {
                // We only have one list and it's empty
                _ASSERT(m_ListOfMasks.size() == 1);
                consolidated_list.swap(m_ListOfMasks);
                continue;
            }
            _ASSERT(m_ListOfMasks[i]->GetMasks().size() == 1);
            CRef<CSeq_loc> sl = m_ListOfMasks[i]->GetMasks().front();
            seqlocs_read += 
                (sl->IsPacked_int() ? sl->GetPacked_int().Get().size() : 1);
            consolidated_list.back()->SetMasks().push_back(sl);
        }
        consolidated_list.back()->SetMore(true);
    }

    m_ListOfMasks.swap(consolidated_list);
    m_ListOfMasks.back()->SetMore(false);
}

void CMaskWriterBlastDbMaskInfo::Print( const objects::CSeq_id& id, 
                                        const CSeqMasker::TMaskList & mask )
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

    CRef<CSeq_loc> seqloc
        (new CSeq_loc(const_cast<CSeq_id&>(id), masked_ranges));

    CRef<CBlast_mask_list> mask_list(new CBlast_mask_list);
    mask_list->SetMasks().push_back(seqloc);
    mask_list->SetMore(true);
    m_ListOfMasks.push_back(mask_list);
    _ASSERT( !m_ListOfMasks.back()->GetMasks().empty() );
}

void CMaskWriterBlastDbMaskInfo::Print( objects::CBioseq_Handle& bsh, 
                                        const CSeqMasker::TMaskList & mask, 
                                        bool /* match_id */ )
{
    Print(*bsh.GetSeqId(), mask);
}

void CMaskWriterBlastDbMaskInfo::Print( int gi,
                                        const CSeqMasker::TMaskList & mask )
{
    CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, gi));
    Print(*id, mask);
}

string BuildAlgorithmParametersString(const CArgs& args)
{
    ostringstream os;
    if (args.Exist("locut") &&
        args.Exist("hicut") &&
        args.Exist("window")) {
        // SEG
        os << "window=" << args["window"].AsInteger() << "; "
           << "locut=" << args["locut"].AsDouble() << "; "
           << "hicut=" << args["hicut"].AsDouble();
    } else if (args.Exist("level") && 
               args.Exist("linker") && 
               args.Exist("window")) {
        // DUST
        os << "window=" << args["window"].AsInteger() << "; "
           << "level=" << args["level"].AsInteger() << "; "
           << "linker=" << args["linker"].AsInteger();
    }
    return os.str();
}

END_NCBI_SCOPE
