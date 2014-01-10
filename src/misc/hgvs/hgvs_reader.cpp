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
 * Author:  Frank Ludwig
 *
 * File Description:
 *   HGVS file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/stream_utils.hpp>

#include <util/static_map.hpp>
#include <util/line_reader.hpp>

#include <serial/iterator.hpp>
#include <serial/objistrasn.hpp>

// Objects includes
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_data.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/seqfeat/VariantProperties.hpp>
#include <objects/seqfeat/Delta_item.hpp>

#include <objects/variation/VariationException.hpp>

#include <objtools/readers/read_util.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/message_listener.hpp>
#include <misc/hgvs/hgvs_reader.hpp>
#include <objtools/error_codes.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <misc/hgvs/hgvs_parser2.hpp>
#include <misc/hgvs/variation_util2.hpp>

#include <algorithm>

#define NCBI_USE_ERRCODE_X   Objtools_Rd_RepMask

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CHgvsReader::CHgvsReader(const CGC_Assembly& assembly, int flags)
    : CReaderBase(flags),
      m_Assembly(&assembly)
//  ----------------------------------------------------------------------------
{
}


//  ----------------------------------------------------------------------------
CHgvsReader::CHgvsReader(int flags)
    : CReaderBase(flags)
//  ----------------------------------------------------------------------------
{
}


//  ----------------------------------------------------------------------------
CHgvsReader::~CHgvsReader()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
CRef< CSeq_annot >
CHgvsReader::ReadSeqAnnot(
    ILineReader& lr,
    IMessageListener* pEC )
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_annot> annot(new CSeq_annot);

    // object manager
    CRef<CObjectManager> objectManager = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager( *objectManager );
    CRef<CScope> scope(new CScope(*objectManager));
    scope->AddDefaults();

    // hgvs parser
    variation::CHgvsParser hgvsParser(*scope);
    CRef<CSeq_id_Resolver> assmresolver;
    if (m_Assembly.NotNull()) {
        assmresolver.Reset(new CSeq_id_Resolver__ChrNamesFromGC(*m_Assembly, *scope));
        hgvsParser.SetSeq_id_Resolvers().push_front(assmresolver);
    }

    // helper to convert to feature
    variation::CVariationUtil varUtil( *scope );

     // parse input
    while (!lr.AtEOF()) {
        string line = *(++lr);
        m_uLineNumber++;

        // TODO split multiple hgvs names on one line (sep by whitespace, ";", ",")
        NStr::TruncateSpacesInPlace(line);
        NStr::ReplaceInPlace(line, "\r", kEmptyStr);
        NStr::ReplaceInPlace(line, "\n", kEmptyStr);

        if (NStr::IsBlank(line) || NStr::StartsWith(line, "#")) {
            continue;
        }

        try {
            CRef<CVariation> var = hgvsParser.AsVariation(line);

            CVariation::TExceptions exception_list;
            if( var->IsSetExceptions() ) {
                exception_list.insert(exception_list.end(), var->GetExceptions().begin(), var->GetExceptions().end());
            }
            if( var->IsSetPlacements() ) {
                ITERATE(CVariation::TPlacements, place_it, var->GetPlacements() ) {
                    const CVariantPlacement& placement = **place_it;
                    if( placement.IsSetExceptions() ) {
                        exception_list.insert(exception_list.end(), placement.GetExceptions().begin(), placement.GetExceptions().end());
                    }
                }
            }

            ITERATE(CVariation::TExceptions, except_it, exception_list ) {

                const CVariationException& except = **except_it;

                if( except.IsSetCode() && except.IsSetMessage() ) {

                    const string& code = 
                        CVariationException::GetTypeInfo_enum_ECode()->FindName(except.GetCode(), true);
                    auto_ptr<CObjReaderLineException> err(
                        CObjReaderLineException::Create(
                            eDiag_Warning,
                            m_uLineNumber,
                            string("CHgvsReader::ReadSeqAnnot Warning [") + code  + "] " + except.GetMessage(),
                            ILineError::eProblem_GeneralParsingError));
                    ProcessWarning(*err, pEC);
                } 
            }

            varUtil.AsVariation_feats(*var, annot->SetData().SetFtable());
        }
        catch (const variation::CHgvsParser::CHgvsParserException& e) {
            auto_ptr<CObjReaderLineException> err(
                CObjReaderLineException::Create(
                    eDiag_Error,
                    0,
                    string("CHgvsReader::ReadSeqAnnot Error [") + e.GetErrCodeString() + "] " + e.GetMsg(),
                    ILineError::eProblem_GeneralParsingError));
                ProcessError(*err, pEC);
        }
    }

    NON_CONST_ITERATE (CSeq_annot::C_Data::TFtable, itr, annot->SetData().SetFtable() ) {
        CRef<CSeq_feat> feat = *itr;
        CRef<CSeq_id> tempid(new CSeq_id());
        tempid->Assign(*(feat->GetLocation().GetId()));
        CSeq_id_Handle idh = sequence::GetId(*tempid, *scope, sequence::eGetId_ForceGi);
        tempid->SetGi(idh.GetGi());
        feat->SetLocation().SetId(*tempid);
    }
    return annot;
}

//  ---------------------------------------------------------------------------
void
CHgvsReader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    CNcbiIstream& istr,
    IMessageListener* pMessageListener )
//  ---------------------------------------------------------------------------
{
    CStreamLineReader lr(istr);
    ReadSeqAnnots(annots, lr, pMessageListener);
}

//  ---------------------------------------------------------------------------
void
CHgvsReader::ReadSeqAnnots(
    vector< CRef<CSeq_annot> >& annots,
    ILineReader& lr,
    IMessageListener* pMessageListener )
//  ----------------------------------------------------------------------------
{
    annots.push_back(ReadSeqAnnot(lr, pMessageListener));
}

//  ----------------------------------------------------------------------------
CRef< CSerialObject >
CHgvsReader::ReadObject(
    ILineReader& lr,
    IMessageListener* pMessageListener )
//  ----------------------------------------------------------------------------
{
    return Ref<CSerialObject>(ReadSeqAnnot(lr, pMessageListener).GetPointer());
}

END_objects_SCOPE
END_NCBI_SCOPE
