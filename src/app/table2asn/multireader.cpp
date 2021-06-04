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
* Author:  Frank Ludwig, Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   Reader for selected data file formats
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include "fasta_ex.hpp"

#include <objtools/readers/gff3_reader.hpp>
#include <objtools/readers/gtf_reader.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqfeat/Variation_ref.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/general/Date.hpp>
#include <objects/biblio/Cit_sub.hpp>

#include <corelib/ncbistre.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>
#include <objects/seq/Annot_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objtools/readers/readfeat.hpp>
#include <objtools/readers/aln_reader.hpp>

#include <objtools/readers/format_guess_ex.hpp>

#include "multireader.hpp"
#include "table2asn_context.hpp"
#include "descr_apply.hpp"

#include <objtools/edit/feattable_edit.hpp>

#include <objtools/flatfile/flatfile_parser.hpp>
#include <corelib/stream_utils.hpp>

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

namespace
{

    CRef<CSeq_id> GetSeqIdWithoutVersion(const CSeq_id& id)
    {
        const CTextseq_id* text_id = id.GetTextseq_Id();

        if (text_id && text_id->IsSetVersion())
        {
            CRef<CSeq_id> new_id(new CSeq_id);
            new_id->Assign(id);
            CTextseq_id* text_id = (CTextseq_id*)new_id->GetTextseq_Id();
            text_id->ResetVersion();
            return new_id;
        }

        return CRef<CSeq_id>();
    }

    struct SCSeqidCompare
    {
        inline
            bool operator()(const CSeq_id* left, const CSeq_id* right) const
        {
            return *left < *right;
        };
    };

    CRef<CSeq_id> GetIdByKind(const CSeq_id& id, const CBioseq::TId& ids)
    {
        ITERATE(CBioseq::TId, it, ids)
        {
            if ((**it).Which() == id.Which())
                return *it;
        }
        return CRef<CSeq_id>();
    }

    void x_ModifySeqIds(CSerialObject& obj, CConstRef<CSeq_id> match, CRef<CSeq_id> new_id)
    {
#if 0
        CTypeIterator<CSeq_id> visitor(obj);

        while (visitor)
        {
            CSeq_id& id = *visitor;
            if (match.Empty() || id.Compare(*match) == CSeq_id::e_YES)
            {
                id.Assign(*new_id);
            }
            ++visitor;
        }
#else
        CTypeIterator<CSeq_loc> visitor(obj);

        CSeq_id& id = *new_id;
        while (visitor)
        {
            CSeq_loc& loc = *visitor;

            if (match.Empty() || loc.GetId()->Compare(*match) == CSeq_id::e_YES)
            {
                loc.SetId(id);
            }
            ++visitor;
        }
#endif
    }

    static const set<TTypeInfo> supported_types =
    {
        CBioseq_set::GetTypeInfo(),
        CBioseq::GetTypeInfo(),
        CSeq_entry::GetTypeInfo(),
        CSeq_submit::GetTypeInfo(),
    };
}

CRef<CSerialObject> CMultiReader::xReadASN1Binary(CObjectIStream& pObjIstrm, const CFileContentInfoGenbank& content_info)
{
    const string& content_type = content_info.mObjectType;
    if (content_type == "Bioseq-set")
    {
        auto obj = Ref(new CSeq_entry);
        auto& bioseq_set = obj->SetSet();
        pObjIstrm.Read(ObjectInfo(bioseq_set));
        return obj;
    }
    else
    if (content_type == "Seq-submit")
    {
        auto seqsubmit = Ref(new CSeq_submit);
        pObjIstrm.Read(ObjectInfo(*seqsubmit));
        return seqsubmit;
    }
    else
    if (content_type == "Seq-entry")
    {
        auto obj = Ref(new CSeq_entry);
        pObjIstrm.Read(ObjectInfo(*obj));
        return obj;
    }
    else
    if (content_type == "Bioseq")
    {
        auto obj = Ref(new CSeq_entry);
        pObjIstrm.Read(ObjectInfo(obj->SetSeq()));
        return obj;
    };

    return {};
}

CRef<CSerialObject> CMultiReader::xReadASN1Text(CObjectIStream& pObjIstrm)
{
    CRef<CSeq_entry> entry;
    CRef<CSeq_submit> submit;

    // guess object type
    string sType;
    try
    {
        sType = pObjIstrm.ReadFileHeader();
    }
    catch (const CEofException&)
    {
        sType.clear();
        // ignore EOF exception
    }

    // do the right thing depending on the input type
    if (sType == CBioseq_set::GetTypeInfo()->GetName()) {
        entry.Reset(new CSeq_entry);
        pObjIstrm.Read(ObjectInfo(entry->SetSet()), CObjectIStream::eNoFileHeader);
    }
    else
    if (sType == CSeq_submit::GetTypeInfo()->GetName()) {
        submit.Reset(new CSeq_submit);
        pObjIstrm.Read(ObjectInfo(*submit), CObjectIStream::eNoFileHeader);

        if (submit->GetData().GetEntrys().size() > 1)
        {
            entry.Reset(new CSeq_entry);
            entry->SetSet().SetSeq_set() = submit->GetData().GetEntrys();
        }
        else
            entry = *submit->SetData().SetEntrys().begin();
    }
    else
    if (sType == CSeq_entry::GetTypeInfo()->GetName()) {
        entry.Reset(new CSeq_entry);
        pObjIstrm.Read(ObjectInfo(*entry), CObjectIStream::eNoFileHeader);
    }
    else
    if (sType == CSeq_annot::GetTypeInfo()->GetName())
    {
        entry.Reset(new CSeq_entry);
        do
        {
            CRef<CSeq_annot> annot(new CSeq_annot);
            pObjIstrm.Read(ObjectInfo(*annot), CObjectIStream::eNoFileHeader);
            entry->SetSeq().SetAnnot().push_back(annot);
            try
            {
                sType = pObjIstrm.ReadFileHeader();
            }
            catch (const CEofException&)
            {
                sType.clear();
                // ignore EOF exception
            }
        } while (sType == CSeq_annot::GetTypeInfo()->GetName());
    }
    else
    {
        return CRef<CSerialObject>();
    }

    if (m_context.m_gapNmin > 0)
    {
        CGapsEditor gap_edit(
                (CSeq_gap::EType)m_context.m_gap_type,
                m_context.m_DefaultEvidence,
                m_context.m_GapsizeToEvidence,
                m_context.m_gapNmin,
                m_context.m_gap_Unknown_length);
        gap_edit.ConvertNs2Gaps(*entry);
    }

    if (submit.Empty())
        return entry;
    else
        return submit;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_entry>
CMultiReader::ReadAlignment(CNcbiIstream& instream, const CArgs& args)
//  ----------------------------------------------------------------------------
{
    CAlnReader reader(instream);
    reader.SetAllGap(args["aln-gapchar"].AsString());
    reader.SetMissing(args["aln-gapchar"].AsString());
    if (args["aln-alphabet"].AsString() == "nuc") {
        reader.SetAlphabet(CAlnReader::eAlpha_Nucleotide);
    }
    else {
        reader.SetAlphabet(CAlnReader::eAlpha_Protein);
    }

    reader.Read(0, m_context.m_logger);
    auto pSeqEntry =
        reader.GetSeqEntry(
            CAlnReader::fGenerateLocalIDs,
            m_context.m_logger);

    if (pSeqEntry && args["a"]) {
        static const map<string, CBioseq_set::EClass>
            s_StringToClass =
            {{"s",  CBioseq_set::eClass_genbank},
             {"s1", CBioseq_set::eClass_pop_set},
             {"s2", CBioseq_set::eClass_phy_set},
             {"s3", CBioseq_set::eClass_mut_set},
             {"s4", CBioseq_set::eClass_eco_set},
             {"s9", CBioseq_set::eClass_small_genome_set}};

        auto it = s_StringToClass.find(args["a"].AsString());
        if (it != s_StringToClass.end()) {
            pSeqEntry->SetSet().SetClass(it->second);
        }
    }

    return pSeqEntry;
}


//  ----------------------------------------------------------------------------
CRef<CSeq_entry>
CMultiReader::xReadFasta(CNcbiIstream& instream)
    //  ----------------------------------------------------------------------------
{
    if (m_context.m_gapNmin > 0)
    {
        m_iFlags |= CFastaReader::fParseGaps
                 |  CFastaReader::fLetterGaps;
    }
    else
    {
        m_iFlags |= CFastaReader::fNoSplit;
//                 |  CFastaReader::fLeaveAsText;
    }

    if (m_context.m_d_fasta)
    {
        m_iFlags |= CFastaReader::fParseGaps;
    }

    m_iFlags |= CFastaReader::fIgnoreMods
             |  CFastaReader::fValidate
             |  CFastaReader::fHyphensIgnoreAndWarn
             |  CFastaReader::fDisableParseRange;

    if (m_context.m_allow_accession)
        m_iFlags |= CFastaReader::fParseRawID;


    m_iFlags |= CFastaReader::fAssumeNuc
             |  CFastaReader::fForceType;

    CStreamLineReader lr( instream );
    unique_ptr<CFastaReaderEx> pReader(new CFastaReaderEx(m_context, lr, m_iFlags));
    if (!pReader) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "File format not supported", 0);
    }
    if (m_context.m_gapNmin > 0)
    {
        pReader->SetMinGaps(m_context.m_gapNmin, m_context.m_gap_Unknown_length);
    }

    //if (m_context.m_gap_evidences.size() > 0 || m_context.m_gap_type >= 0)
    if (!m_context.m_GapsizeToEvidence.empty() ||
        !m_context.m_DefaultEvidence.empty() ||
        m_context.m_gap_type >= 0) {
        pReader->SetGapLinkageEvidence(
                (CSeq_gap::EType)m_context.m_gap_type,
                m_context.m_DefaultEvidence,
                m_context.m_GapsizeToEvidence);
    }

    int max_seqs = kMax_Int;
    CRef<CSeq_entry> result;
    if (m_context.m_di_fasta)
        result = pReader->ReadDeltaFasta(m_context.m_logger);
    else if (m_context.m_d_fasta)
        result = pReader->ReadDeltaFasta(m_context.m_logger);
    else
        result = pReader->ReadSet(max_seqs, m_context.m_logger);

    if (result.NotEmpty())
    {
        m_context.MakeGenomeCenterId(*result);
    }

    if (result->IsSet() && !m_context.m_HandleAsSet)
    {
        m_context.m_logger->PutError(*unique_ptr<CLineError>(
            CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Warning, "", 0,
            "File " + m_context.m_current_file + " contains multiple sequences")));
    }
    if (result->IsSet())
    {
        result->SetSet().SetClass(m_context.m_ClassValue);
    }

    return result;

}

CFormatGuess::EFormat CMultiReader::xInputGetFormat(CNcbiIstream& istr, CFileContentInfo* content_info) const
    //  ----------------------------------------------------------------------------
{
    CFormatGuessEx FG(istr);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eBinaryASN);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eFasta);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eTextASN);
    //FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eXml);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eGff3);
    FG.GetFormatHints().DisableAllNonpreferred();

    if (!content_info) {
        return FG.GuessFormat();
    }

    FG.SetRecognizedGenbankTypes(supported_types);
    return FG.GuessFormatAndContent(*content_info);
}

CFormatGuess::EFormat CMultiReader::xAnnotGetFormat(CNcbiIstream& istr) const
    //  ----------------------------------------------------------------------------
{
    CFormatGuess FG(istr);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eBinaryASN);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eTextASN);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eGffAugustus);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eGff3);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eGff2);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eGtf);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eFiveColFeatureTable);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eFlatFileGenbank);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eFlatFileEna);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eFlatFileUniProt);
    FG.GetFormatHints().DisableAllNonpreferred();

    return FG.GuessFormat();
}

//  ----------------------------------------------------------------------------
void CMultiReader::WriteObject(
    const CSerialObject& object,
    ostream& ostr)
    //  ----------------------------------------------------------------------------
{
    ostr << MSerial_Format{m_context.m_binary_asn1_output?eSerial_AsnBinary:eSerial_AsnText}
         //<< MSerial_VerifyNo
         << object;
    ostr.flush();
}

CMultiReader::CMultiReader(CTable2AsnContext& context)
    :m_context(context),
    mAtSequenceData(false)
{
}

/*
void CMultiReader::ApplyAdditionalProperties(CSeq_entry& entry)
{
    switch(entry.Which())
    {
    case CSeq_entry::e_Seq:
        if (!m_context.m_OrganismName.empty() || m_context.m_taxid != 0)
        {
            CBioSource::TOrg& org(CAutoAddDesc(entry.SetDescr(), CSeqdesc::e_Source).Set().SetSource().SetOrg());
            // we should reset taxid in case new name is different
            if (org.IsSetTaxname() && org.GetTaxId() >0 && org.GetTaxname() != m_context.m_OrganismName)
            {
                org.SetTaxId(0);
            }

            if (!m_context.m_OrganismName.empty())
                org.SetTaxname(m_context.m_OrganismName);
            if (m_context.m_taxid != 0)
                org.SetTaxId(m_context.m_taxid);
        }
        break;

    case CSeq_entry::e_Set:
        {
            if (!entry.GetSet().IsSetClass())
                entry.SetSet().SetClass(CBioseq_set::eClass_genbank);

            NON_CONST_ITERATE(CBioseq_set_Base::TSeq_set, it, entry.SetSet().SetSeq_set())
            {
                ApplyAdditionalProperties(**it);
            }
        }
        break;
    default:
        break;
    }
}
*/

void CMultiReader::LoadDescriptors(const string& ifname, CRef<CSeq_descr> & out_desc)
{
    out_desc.Reset(new CSeq_descr);

    unique_ptr<CObjectIStream> pObjIstrm = xCreateASNStream(ifname);

    // guess object type
    //const string sType = pObjIstrm->ReadFileHeader();

    // do the right thing depending on the input type
    while (true) {
        try {
            const string sType = pObjIstrm->ReadFileHeader();
            if (sType == CSeq_descr::GetTypeInfo()->GetName())
            {
                CRef<CSeq_descr> descr(new CSeq_descr);
                pObjIstrm->Read(ObjectInfo(*descr),
                    CObjectIStream::eNoFileHeader);
                out_desc->Set().insert(out_desc->Set().end(), descr->Get().begin(), descr->Get().end());
            }
            else if (sType == CSeqdesc::GetTypeInfo()->GetName())
            {
                CRef<CSeqdesc> desc(new CSeqdesc);
                pObjIstrm->Read(ObjectInfo(*desc),
                    CObjectIStream::eNoFileHeader);
                out_desc->Set().push_back(desc);
            }
            else if (sType == CPubdesc::GetTypeInfo()->GetName())
            {
                CRef<CSeqdesc> desc(new CSeqdesc);
                pObjIstrm->Read(ObjectInfo(desc->SetPub()),
                    CObjectIStream::eNoFileHeader);
                out_desc->Set().push_back(desc);
            }
            else
            {
                throw runtime_error("Descriptor file must contain "
                    "either Seq_descr or Seqdesc elements");
            }
        } catch (CException& ex) {
            if (!NStr::EqualNocase(ex.GetMsg(), "end of file")) {
                throw runtime_error("Unable to read descriptor from file:" + ex.GetMsg());
            }
            break;
        }
    }
}

void CMultiReader::LoadTemplate(CTable2AsnContext& context, const string& ifname)
{
#if 0
    // check if the location doesn't exist, and see if we can
    // consider it some kind of sequence identifier
    if( ! CDirEntry(ifname).IsFile() ) {
        // see if this is blatantly not a sequence identifier
        if( ! CRegexpUtil(sTemplateLocation).Exists("^[A-Za-z0-9_|]+(\\.[0-9]+)?$") ) {
            throw runtime_error("This is not a valid sequence identifier: " + sTemplateLocation);
        }

        // try to load from genbank
        CGBDataLoader::RegisterInObjectManager(*CObjectManager::GetInstance());
        CRef<CScope> pScope(new CScope(*CObjectManager::GetInstance()));
        pScope->AddDefaults();

        CRef<CSeq_id> pTemplateId( new CSeq_id(sTemplateLocation) );
        CBioseq_Handle bsh = pScope->GetBioseqHandle( *pTemplateId );

        if ( ! bsh ) {
            throw runtime_error("Invalid sequence identifier: " + sTemplateLocation);
        }
        CSeq_entry_Handle entry_h = bsh.GetParentEntry();

        context.m_entry_template->Assign( *entry_h.GetCompleteSeq_entry() );
        return;
    }
#endif

    unique_ptr<CObjectIStream> pObjIstrm = xCreateASNStream(ifname);

    // guess object type
    string sType = pObjIstrm->ReadFileHeader();

    // do the right thing depending on the input type
    if( sType == CSeq_entry::GetTypeInfo()->GetName() ) {
        context.m_entry_template.Reset( new CSeq_entry );
        pObjIstrm->Read(ObjectInfo(*context.m_entry_template), CObjectIStream::eNoFileHeader);
    } else if( sType == CBioseq::GetTypeInfo()->GetName() ) {
        CRef<CBioseq> pBioseq( new CBioseq );
        pObjIstrm->Read(ObjectInfo(*pBioseq), CObjectIStream::eNoFileHeader);
        context.m_entry_template.Reset( new CSeq_entry );
        context.m_entry_template->SetSeq( *pBioseq );
    } else if( sType == CSeq_submit::GetTypeInfo()->GetName() ) {

        context.m_submit_template.Reset( new CSeq_submit );
        pObjIstrm->Read(ObjectInfo(*context.m_submit_template), CObjectIStream::eNoFileHeader);
        if (!context.m_submit_template->GetData().IsEntrys()
            || context.m_submit_template->GetData().GetEntrys().size() != 1)
        {
            throw runtime_error("Seq-submit template must contain "
                "exactly one Seq-entry");
        }
    } else if( sType == CSubmit_block::GetTypeInfo()->GetName() ) {

        // a Submit-block
        CRef<CSubmit_block> submit_block(new CSubmit_block);
        pObjIstrm->Read(ObjectInfo(*submit_block),
            CObjectIStream::eNoFileHeader);

        // Build a Seq-submit containing this plus a bogus Seq-entry
        context.m_submit_template.Reset( new CSeq_submit );
        context.m_submit_template->SetSub(*submit_block);
        CRef<CSeq_entry> ent(new CSeq_entry);
        CRef<CSeq_id> dummy_id(new CSeq_id("lcl|dummy_id"));
        ent->SetSeq().SetId().push_back(dummy_id);
        ent->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
        ent->SetSeq().SetInst().SetMol(CSeq_inst::eMol_dna);
        context.m_submit_template->SetData().SetEntrys().push_back(ent);
    } else if ( sType == CSeqdesc::GetTypeInfo()->GetName()) {
        // it's OK
    } else {
        NCBI_USER_THROW_FMT("Template must be Seq-entry, Seq-submit, Bioseq or "
            "Submit-block.  Object seems to be of type: " << sType);
    }

    // for submit types, pull out the seq-entry inside and remember it
    if( context.m_submit_template.NotEmpty() && context.m_submit_template->IsEntrys() ) {
        context.m_entry_template = context.m_submit_template->SetData().SetEntrys().front();
    }

    // The template may contain a set rather than a seq.
    // That's OK if it contains only one na entry, which we'll use.
    if (context.m_entry_template.NotEmpty() && context.m_entry_template->IsSet())
    {
        CRef<CSeq_entry> tmp(new CSeq_entry);
        ITERATE(CBioseq_set::TSeq_set, ent_iter, context.m_entry_template->GetSet().GetSeq_set())
        {
            const CSeq_descr* descr = nullptr;
            if ((*ent_iter)->IsSetDescr())
            {
                descr = &(*ent_iter)->GetDescr();
            }
            if (descr)
            {
                //tmp->Assign(**ent_iter);
                tmp->SetSeq().SetInst();
                // Copy any descriptors from the set to the sequence
                ITERATE(CBioseq_set::TDescr::Tdata, desc_iter, descr->Get())
                {
                    switch ((*desc_iter)->Which())
                    {
                    case CSeqdesc::e_Pub:
                    case CSeqdesc::e_Source:
                       break;
                    default:
                       continue;
                    }
                    CRef<CSeqdesc> desc(new CSeqdesc);
                    desc->Assign(**desc_iter);
                    tmp->SetSeq().SetDescr().Set().push_back(desc);
                }
                break;
            }
        }

        if (tmp->IsSetDescr() && !tmp->GetDescr().Get().empty())
            context.m_entry_template = tmp;

    }

    // incorporate any Seqdesc's that follow in the file
    if (!pObjIstrm->EndOfData())
    {
        if (sType != CSeqdesc::GetTypeInfo()->GetName())
            sType = pObjIstrm->ReadFileHeader();

        while (sType == CSeqdesc::GetTypeInfo()->GetName()) {
            CRef<CSeqdesc> desc(new CSeqdesc);
            pObjIstrm->Read(ObjectInfo(*desc), CObjectIStream::eNoFileHeader);

            if  (context.m_entry_template.Empty())
                context.m_entry_template.Reset(new CSeq_entry);

            {
                if (desc->IsUser() && desc->GetUser().IsDBLink())
                {
                    CUser_object& user_obj = desc->SetUser();
                    if (user_obj.IsDBLink())
                    {
                        user_obj.SetData();
                    }
                }
            }

            context.m_entry_template->SetSeq().SetDescr().Set().push_back(desc);

            if (pObjIstrm->EndOfData())
                break;

            try {
                sType = pObjIstrm->ReadFileHeader();
            }
            catch(CEofException&) {
                break;
            }
        }
    }

#if 0
    if ( context.m_submit_template->IsEntrys() ) {
        // Take Seq-submit.sub.cit and put it in the Bioseq
        CRef<CPub> pub(new CPub);
        pub->SetSub().Assign(context.m_submit_template->GetSub().GetCit());
        CRef<CSeqdesc> pub_desc(new CSeqdesc);
        pub_desc->SetPub().SetPub().Set().push_back(pub);
        context.m_entry_template->SetSeq().SetDescr().Set().push_back(pub_desc);
    }
#endif

    if( context.m_entry_template.NotEmpty() && ! context.m_entry_template->IsSeq() ) {
        throw runtime_error("The Seq-entry must be a Bioseq not a Bioseq-set.");
    }

    if (context.m_submit_template.NotEmpty())
    {
        if (context.m_submit_template->IsSetSub() &&
            context.m_submit_template->GetSub().IsSetCit())
        {
        CRef<CDate> date(new CDate(CTime(CTime::eCurrent), CDate::ePrecision_day));
        context.m_submit_template->SetSub().SetCit().SetDate(*date);
        }
    }

#if 0
    if( args["output-type"].AsString() == "Seq-entry" ) {
        // force Seq-entry by throwing out the Seq-submit
        context.m_submit_template.Reset( new CSeq_submit );
    }
#endif
}

namespace
{
    class AllowedDuplicates: public set<CSeqdesc_Base::E_Choice>
    {
    public:
        AllowedDuplicates()
        {
            insert(CSeqdesc_Base::e_User);
        }
    };
    AllowedDuplicates m_allowed_duplicates;

    template<typename _which>
    struct LocateWhich
    {
        typename _which::E_Choice compare_to;
        bool operator() (_which l)  const
        {
            return l.Which() == compare_to;
        }
        bool operator() (const CRef<_which>& l)  const
        {
            return l->Which() == compare_to;
        }
    };
}

void CMultiReader::MergeDescriptors(CSeq_descr & dest, const CSeq_descr & source)
{
    ITERATE(CSeq_descr::Tdata, it, source.Get())
    {
        MergeDescriptors(dest, **it);
    }
}

void CMultiReader::MergeDescriptors(CSeq_descr & dest, const CSeqdesc & source)
{
    bool duplicates = (m_allowed_duplicates.find(source.Which()) == m_allowed_duplicates.end());

    CAutoAddDesc desc(dest, source.Which());
    desc.Set(duplicates).Assign(source);
}

void CMultiReader::ApplyDescriptors(CSeq_entry& entry, const CSeq_descr& source)
{
    MergeDescriptors(entry.SetDescr(), source);
    //g_ApplyDescriptors(source.Get(), entry);
}

namespace
{
    void CopyDescr(CSeq_entry& dest, const CSeq_entry& src)
    {
        if (src.IsSetDescr() && !src.GetDescr().Get().empty())
        {
            dest.SetDescr().Set().insert(dest.SetDescr().Set().end(),
                src.GetDescr().Get().begin(),
                src.GetDescr().Get().end());
        }
    }
    void CopyAnnot(CSeq_entry& dest, const CSeq_entry& src)
    {
        if (src.IsSetAnnot() && !src.GetAnnot().empty())
        {
            dest.SetAnnot().insert(dest.SetAnnot().end(),
                src.GetAnnot().begin(),
                src.GetAnnot().end());
        }
    }
}

CFormatGuess::EFormat CMultiReader::OpenFile(const string& filename, CRef<CSerialObject>& input_sequence, TAnnots& annots)
{
    CFormatGuess::EFormat format;
    CFileContentInfo content_info;
    {
        unique_ptr<istream> istream(new CNcbiIfstream(filename));
        format = xInputGetFormat(*istream, &content_info);
    }

    switch (format)
    {
        case CFormatGuess::eBinaryASN:
            m_obj_stream.reset(CObjectIStream::Open(eSerial_AsnBinary, filename));
            input_sequence = xReadASN1Binary(*m_obj_stream, content_info.mInfoGenbank);
            break;
        case CFormatGuess::eTextASN:
            m_obj_stream.reset(CObjectIStream::Open(eSerial_AsnText, filename));
            input_sequence = xReadASN1Text(*m_obj_stream);
            break;
        case CFormatGuess::eGff3:
            {
            LOG_POST("Recognized input file as format: " << CFormatGuess::GetFormatName(format));
            unique_ptr<istream> in(new CNcbiIfstream(filename));
            bool post_process = false;
            annots = xReadGFF3(*in, post_process);
            if (!AtSeqenceData()) {
                NCBI_THROW2(CObjReaderParseException, eFormat,
                    "Specified GFF3 file does not include any sequence data", 0);
            }
            x_PostProcessAnnots(annots);
            m_iFlags = 0;
            m_iFlags |= CFastaReader::fNoUserObjs;
            input_sequence = xReadFasta(*in);
            }
            break;
        default: // RW-616 - Assume FASTA
            {
            m_iFlags = 0;
            m_iFlags |= CFastaReader::fNoUserObjs;

            CBufferedInput istream;
            istream.get().open(filename);
            input_sequence = xReadFasta(istream);
            }
            break;
    }
    if (input_sequence.Empty())
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "File format not recognized", 0);

    //rw-617: apply template descriptors only if input is *not* ASN1:
    bool merge_template_descriptors = (format != CFormatGuess::eTextASN);
    input_sequence = xApplyTemplate(input_sequence, merge_template_descriptors);

    return format;
}

void CMultiReader::GetSeqEntry(CRef<CSeq_entry>& entry, CRef<CSeq_submit>& submit, CRef<CSerialObject> obj)
{
    if (obj->GetThisTypeInfo() == CSeq_submit::GetTypeInfo())
    {
        submit.Reset(static_cast<CSeq_submit*>(obj.GetPointer()));
        entry = submit->SetData().SetEntrys().front();
    }
    else
    if (obj->GetThisTypeInfo() == CSeq_entry::GetTypeInfo()) {
        entry.Reset(static_cast<CSeq_entry*>(obj.GetPointer()));
    }
}

CRef<CSerialObject> CMultiReader::xApplyTemplate(CRef<CSerialObject> obj, bool merge_template_descriptors)
{
    CRef<CSeq_entry> entry;
    CRef<CSeq_submit> submit;

    GetSeqEntry(entry, submit, obj);

    if (entry.NotEmpty()) // &&
    {
        if (submit.Empty())
        if (entry->IsSet() && entry->GetSet().GetSeq_set().size() < 2 &&
            entry->GetSet().GetSeq_set().front()->IsSeq())
        {
            CRef<CSeq_entry> seq = entry->SetSet().SetSeq_set().front();
            CopyDescr(*seq, *entry);
            CopyAnnot(*seq, *entry);
            entry = seq;
        }
        entry->ResetParentEntry();
        entry->Parentize();

        if (merge_template_descriptors) {
            m_context.MergeWithTemplate(*entry);
        }
        else {
            if (m_context.m_t && m_context.m_logger) {
                string msg(
                    "Template file descriptors are ignored if input is ASN.1");
                m_context.m_logger->PutError(
                    *unique_ptr<CLineError>(
                        CLineError::Create(ILineError::eProblem_Unset,
                            eDiag_Warning, "", 0, msg)));
            }
        }
    }

    if (submit.Empty())
        return entry;
    else
        return submit;
}

CRef<CSerialObject> CMultiReader::ReadNextEntry()
{
    if (m_obj_stream)
        return xReadASN1Text(*m_obj_stream);
    else
        return CRef<CSerialObject>();
}

CMultiReader::TAnnots CMultiReader::xReadGFF3(CNcbiIstream& instream, bool post_process)
{
    int flags = 0;
    flags |= CGff3Reader::fGenbankMode;
    flags |= CGff3Reader::fRetainLocusIds;
    flags |= CGff3Reader::fGeneXrefs;
    flags |= CGff3Reader::fAllIdsAsLocal;

    CGff3Reader reader(flags, m_AnnotName, m_AnnotTitle);
    CStreamLineReader lr(instream);
    TAnnots annots;
    reader.ReadSeqAnnots(annots, lr, m_context.m_logger);
    mAtSequenceData = reader.AtSequenceData();
    if (post_process) {
        x_PostProcessAnnots(annots, reader.SequenceSize());
    }
    return annots;
}


void CMultiReader::x_PostProcessAnnots(TAnnots& annots, unsigned int sequenceSize)
{
    unsigned int startingLocusTagNumber = 1;
    unsigned int startingFeatureId = 1;
    for (auto it = annots.begin(); it != annots.end(); ++it) {

        edit::CFeatTableEdit fte(
            **it, sequenceSize, m_context.m_locus_tag_prefix, startingLocusTagNumber, startingFeatureId, m_context.m_logger);
        //fte.InferPartials();
        fte.GenerateMissingParentFeatures(m_context.m_eukaryote);
        if (m_context.m_locus_tags_needed) {
            if (m_context.m_locus_tag_prefix.empty() && !fte.AnnotHasAllLocusTags()) {
                NCBI_THROW(CArgException, eNoArg,
                    "GFF annotation requires locus tag, which is missing from arguments");
            }
            fte.GenerateLocusTags();
        }
        fte.GenerateProteinAndTranscriptIds();
        //fte.InstantiateProducts();
        fte.ProcessCodonRecognized();
        fte.EliminateBadQualifiers();
        fte.SubmitFixProducts();

        startingLocusTagNumber = fte.PendingLocusTagNumber();
        startingFeatureId = fte.PendingFeatureId();
    }
}




unique_ptr<CObjectIStream> CMultiReader::xCreateASNStream(const string& filename)
{
    unique_ptr<istream> instream(new CNcbiIfstream(filename));
    return xCreateASNStream(CFormatGuess::eUnknown, instream);
}

unique_ptr<CObjectIStream> CMultiReader::xCreateASNStream(CFormatGuess::EFormat format, unique_ptr<istream>& instream)
{
    // guess format
    ESerialDataFormat eSerialDataFormat = eSerial_None;
    {{
        if (format == CFormatGuess::eUnknown)
            format = xInputGetFormat(*instream);

        switch(format) {
        case CFormatGuess::eBinaryASN:
            eSerialDataFormat = eSerial_AsnBinary;
            break;
        case CFormatGuess::eUnknown:
        case CFormatGuess::eTextASN:
            eSerialDataFormat = eSerial_AsnText;
            break;
        case CFormatGuess::eXml:
            eSerialDataFormat = eSerial_Xml;
            break;
        default:
            NCBI_USER_THROW_FMT(
                "Descriptor file seems to be in an unsupported format: "
                << CFormatGuess::GetFormatName(format) );
            break;
        }

        //instream.seekg(0);
    }}

    unique_ptr<CObjectIStream> pObjIstrm(
        CObjectIStream::Open(eSerialDataFormat, *instream.release(), eTakeOwnership));

    return pObjIstrm;
}

CMultiReader::~CMultiReader()
{
}

class CAnnotationLoader
{
public:
    using TAnnots = CMultiReader::TAnnots;


    bool Init(const TAnnots& annots) {
        if (annots.empty()) {
            return false;
        }

        m_Annots = annots;
        m_annot_iterator = m_Annots.begin();
        return true;
    }

    bool Init(const string& seqid_prefix, unique_ptr<istream>& instream, ILineErrorListener* logger)
    {
        m_seqid_prefix = seqid_prefix;
        m_line_reader = ILineReader::New(*instream, eTakeOwnership);
        instream.release();
        m_logger = logger;
        return true;
    }

    CRef<CSeq_annot> GetNextAnnot()
    {
        if (!m_Annots.empty())
        {
            if (m_annot_iterator != m_Annots.end())
            {
                return *m_annot_iterator++;
            }
        }
        else
        if (m_line_reader.NotEmpty())
        {
            while (!m_line_reader->AtEOF()) {
                CRef<CSeq_annot> annot = CFeature_table_reader::ReadSequinFeatureTable(
                    *m_line_reader,
                    CFeature_table_reader::fReportBadKey |
                    CFeature_table_reader::fLeaveProteinIds |
                    CFeature_table_reader::fCreateGenesFromCDSs |
                    CFeature_table_reader::fAllIdsAsLocal |
                    CFeature_table_reader::fPreferGenbankId,
                    m_logger, nullptr/*filter*/, m_seqid_prefix);

                if (annot.NotEmpty() && annot->IsSetData() && annot->GetData().IsFtable() &&
                    !annot->GetData().GetFtable().empty()) {
                    return annot;
                }
            }
        }
        return CRef<CSeq_annot>();
    }

private:
    TAnnots m_Annots;
    TAnnots::iterator m_annot_iterator;
    string m_seqid_prefix;
    CRef<ILineReader> m_line_reader;
    ILineErrorListener* m_logger;
};

bool CMultiReader::xGetAnnotLoader(CAnnotationLoader& loader, const string& filename)
{
    unique_ptr<istream> in(new CNcbiIfstream(filename));

    CFormatGuess::EFormat uFormat = xAnnotGetFormat(*in);

    if (uFormat == CFormatGuess::eUnknown)
    {
        string ext;
        CDirEntry::SplitPath(filename, nullptr, nullptr, &ext);
        NStr::ToLower(ext);
        if (ext == ".gff" || ext == ".gff3")
            uFormat = CFormatGuess::eGff3;
        else
        if (ext == ".gff2")
            uFormat = CFormatGuess::eGff2;
        else
        if (ext == ".gtf")
            uFormat = CFormatGuess::eGtf;
        else
        if (ext == ".tbl")
            uFormat = CFormatGuess::eFiveColFeatureTable;
        else
        if (ext == ".asn" || ext == ".sqn" || ext == ".sap")
            uFormat = CFormatGuess::eTextASN;

        if (uFormat != CFormatGuess::eUnknown)
        {
            LOG_POST("Presuming annotation format by filename suffix: "
                 << CFormatGuess::GetFormatName(uFormat));
        }
    }
    else
    {
        LOG_POST("Recognized annotation format: " << CFormatGuess::GetFormatName(uFormat));
    }

    TAnnots annots;
    switch (uFormat)
    {
    case CFormatGuess::eFiveColFeatureTable:
    {
        string seqid_prefix;
        if (!m_context.m_genome_center_id.empty())
            seqid_prefix = "gnl|" + m_context.m_genome_center_id + "|";
        return loader.Init(seqid_prefix, in, m_context.m_logger);
    }
    break;
    case CFormatGuess::eTextASN:
    {
        auto obj_stream = xCreateASNStream(uFormat, in);
        CRef<CSerialObject> obj = xReadASN1Text(*obj_stream);
        CRef<CSeq_submit> unused;
        CRef<CSeq_entry> pEntry;
        GetSeqEntry(pEntry, unused, obj);
        if (pEntry && pEntry->IsSetAnnot()) {
            annots = pEntry->GetAnnot();
        }    
    }
        break;
    case CFormatGuess::eGff2:
    case CFormatGuess::eGff3:
        annots = xReadGFF3(*in);
        break;
    case CFormatGuess::eGtf:
    case CFormatGuess::eGffAugustus:
        annots = xReadGTF(*in);
        break;
    case CFormatGuess::eFlatFileGenbank:
    case CFormatGuess::eFlatFileEna:
    case CFormatGuess::eFlatFileUniProt:
        in.reset();
    {
        auto pEntry = xReadFlatfile(uFormat, filename);
        if (pEntry && pEntry->IsSetAnnot()) {
            annots = pEntry->GetAnnot();
        }
    }
        break;
    default:
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "Annotation file format not recognized. Run format validator on your annotation file", 1);
    }
    
    if (!annots.empty()) {
        loader.Init(annots);
        return true;
    }
    return false;
}


bool CMultiReader::LoadAnnot(CScope& scope, const string& filename)
{
    CAnnotationLoader annot_loader;

    if (!xGetAnnotLoader(annot_loader, filename)) {
        return false;
    }

    CRef<CSeq_annot> annot_it;
    while ((annot_it = annot_loader.GetNextAnnot()).NotEmpty())
    {
        xFixupAnnot(scope, annot_it);
    }
    return true;
}


bool CMultiReader::LoadAnnots(const string& filename, list<CRef<CSeq_annot>>& annots)
{   
    CAnnotationLoader annot_loader;
    if (!xGetAnnotLoader(annot_loader, filename)) {
        return false;
    }

    CRef<CSeq_annot> pAnnot;
    while ((pAnnot = annot_loader.GetNextAnnot()).NotEmpty()) {
        annots.push_back(pAnnot);
    }
    return true;
}


void CMultiReader::AddAnnots(const list<CRef<CSeq_annot>>& annots, CScope& scope) 
{
    for (auto pAnnot : annots) {
        xFixupAnnot(scope, pAnnot);
    }
}


bool CMultiReader::xFixupAnnot(CScope& scope, CRef<CSeq_annot>& annot_it)
{
    CRef<CSeq_id> annot_id;
    if (annot_it->IsSetId())
    {
        annot_id.Reset(new CSeq_id);
        const CAnnot_id& tmp_annot_id = *(*annot_it).GetId().front();
        if (tmp_annot_id.IsLocal())
            annot_id->SetLocal().Assign(tmp_annot_id.GetLocal());
        else
            if (tmp_annot_id.IsGeneral())
            {
                annot_id->SetGeneral().Assign(tmp_annot_id.GetGeneral());
            }
            else
            {
                //cerr << "Unknown id type:" << annot_id.Which() << endl;
                return false;
            }
    }
    else
        if (!annot_it->GetData().GetFtable().empty())
        {
            // get a reference to CSeq_id instance, we'd need to update it recently
            // 5 column feature reader has a single shared instance for all features
            // update one at once would change all the features
            annot_id.Reset((CSeq_id*)annot_it->GetData().GetFtable().front()->GetLocation().GetId());
        }


    CBioseq::TId ids;
    CBioseq_Handle bioseq_h = scope.GetBioseqHandle(*annot_id);
    if (!bioseq_h && annot_id->IsLocal() && annot_id->GetLocal().IsStr())
    {
        CSeq_id::ParseIDs(ids, annot_id->GetLocal().GetStr());
        if (ids.size() == 1)
        {
            bioseq_h = scope.GetBioseqHandle(*ids.front());
        }
        if (!bioseq_h && !m_context.m_genome_center_id.empty())
        {
            string id = "gnl|" + m_context.m_genome_center_id + "|" + annot_id->GetLocal().GetStr();
            ids.clear();
            CSeq_id::ParseIDs(ids, id);
            if (ids.size() == 1)
            {
                bioseq_h = scope.GetBioseqHandle(*ids.front());
            }
        }
    }
    if (!bioseq_h)
    {
        CRef<CSeq_id> alt_id = GetSeqIdWithoutVersion(*annot_id);
        if (alt_id)
        {
            bioseq_h = scope.GetBioseqHandle(*alt_id);
        }
    }
    if (bioseq_h)
    {
        // update ids
        CBioseq_EditHandle edit_handle = bioseq_h.GetEditHandle();
        CBioseq& bioseq = (CBioseq&)*edit_handle.GetBioseqCore();
        CRef<CSeq_id> matching_id = GetIdByKind(*annot_id, bioseq.GetId());

        if (matching_id.Empty())
            matching_id = ids.front();

        if (matching_id && !annot_id->Equals(*matching_id))
        {
            x_ModifySeqIds(*annot_it, annot_id, matching_id);
        }

        CRef<CSeq_annot> existing;
        for (auto feat_it : bioseq.SetAnnot())
        {
            if (feat_it->IsFtable())
            {
                existing = feat_it;
                break;
            }
        }

        if (existing.Empty())
            bioseq.SetAnnot().push_back(annot_it);
        else {
            objects::edit::CFeatTableEdit featEdit(*existing);
            featEdit.MergeFeatures(annot_it->SetData().SetFtable());
        }
    }
#ifdef _DEBUG
    else
    {
        cerr << MSerial_AsnText << "Found unmatched annot: " << *annot_id << endl;
    }
    if (false)
    {
        CNcbiOfstream debug_annot("annot.sqn");
        debug_annot << MSerial_AsnText
            << MSerial_VerifyNo
            << *annot_it;
    }
#endif
    return true;
}

CMultiReader::TAnnots CMultiReader::xReadGTF(CNcbiIstream& instream)
{
    int flags = 0;
    flags |= CGtfReader::fGenbankMode;
    flags |= CGtfReader::fAllIdsAsLocal;
    flags |= CGtfReader::fGenerateChildXrefs;

    CGtfReader reader(flags, m_AnnotName, m_AnnotTitle);
    CStreamLineReader lr(instream);
    TAnnots annots;
    reader.ReadSeqAnnots(annots, lr, m_context.m_logger);

    x_PostProcessAnnots(annots);

    return annots;
}

CRef<CSeq_entry> CMultiReader::xReadFlatfile(CFormatGuess::EFormat format, const string& filename)
{
    unique_ptr<Parser> pp(new Parser);
    switch (format)
    {
        case CFormatGuess::eFlatFileGenbank:
            pp->format = Parser::EFormat::GenBank;
            pp->source = Parser::ESource::GenBank;
            pp->seqtype = CSeq_id::e_Genbank;
            break;
        case CFormatGuess::eFlatFileEna:
            pp->format = Parser::EFormat::EMBL;
            pp->source = Parser::ESource::EMBL;
            pp->acprefix = ParFlat_EMBL_AC;
            pp->seqtype = CSeq_id::e_Embl;
            break;
        case CFormatGuess::eFlatFileUniProt:
            pp->format = Parser::EFormat::SPROT;
            pp->source = Parser::ESource::SPROT;
            pp->seqtype = CSeq_id::e_Swissprot;
            break;
        default:
            NCBI_THROW2(CObjReaderParseException, eFormat,
                "This flat file format is not supported: " + filename, 0);
            break;
    }
#ifdef WIN32
    pp->ifp = fopen(filename.c_str(), "rb");
#else
    pp->ifp = fopen(filename.c_str(), "r");
#endif
    pp->output_format = Parser::EOutput::BioseqSet;

    CFlatFileParser ffparser(m_context.m_logger);
    auto obj = ffparser.Parse(*pp);
    if (obj.NotEmpty())
    {
        if (obj->GetThisTypeInfo() == CBioseq_set::GetTypeInfo())
        {
            auto bioseq_set = Ref(CTypeConverter<CBioseq_set>::SafeCast(obj.GetPointerOrNull()));
            auto entry = Ref(new CSeq_entry);
            entry->SetSeq();
            auto& annot = entry->SetAnnot();
            for (auto& bioseq : bioseq_set->SetSeq_set())
            {
                if (bioseq->IsSetAnnot())
                   annot.splice(annot.end(), bioseq->SetAnnot());
            }
            if (entry->IsSetAnnot())
                return entry;
        }
    }
    return {};
}

END_NCBI_SCOPE
