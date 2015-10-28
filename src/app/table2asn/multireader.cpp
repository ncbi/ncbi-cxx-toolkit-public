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

#include <objtools/readers/fasta.hpp>
#include <objtools/readers/gff3_reader.hpp>
#include <objtools/readers/bed_reader.hpp>

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

#include <objtools/edit/gaps_edit.hpp>

#include <corelib/ncbistre.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

//#include <objtools/readers/error_container.hpp>

#include "multireader.hpp"
#include "table2asn_context.hpp"

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

namespace
{

    class CFastaReaderEx : public CFastaReader {
    public:
        CFastaReaderEx(ILineReader& reader, TFlags flags) : CFastaReader(reader, flags)
        {
        };
        virtual void AssignMolType(ILineErrorListener * pMessageListener)
        {
            CFastaReader::AssignMolType(pMessageListener);
            CSeq_inst& inst = SetCurrentSeq().SetInst();
            if (!inst.IsSetMol() || inst.GetMol() ==  CSeq_inst::eMol_na)
                inst.SetMol( CSeq_inst::eMol_dna);
        }
        virtual void AssembleSeq (ILineErrorListener * pMessageListener)
        {
            CFastaReader::AssembleSeq(pMessageListener);

            CAutoAddDesc molinfo_desc(SetCurrentSeq().SetDescr(), CSeqdesc::e_Molinfo);

            if (!molinfo_desc.Set().SetMolinfo().IsSetBiomol())
                molinfo_desc.Set().SetMolinfo().SetBiomol(CMolInfo::eBiomol_genomic);

            CAutoAddDesc create_date_desc(SetCurrentSeq().SetDescr(), CSeqdesc::e_Create_date);
            if (create_date_desc.IsNull())
            {   
                CRef<CDate> date(new CDate(CTime(CTime::eCurrent), CDate::ePrecision_day));
                create_date_desc.Set().SetCreate_date(*date);
            }

        }
    };

}
//  ============================================================================
void DumpMemory(const string& prefix)
    //  ============================================================================
{
    Uint8 totalMemory = GetPhysicalMemorySize();
    size_t usedMemory; size_t residentMemory; size_t sharedMemory;
    if (!GetMemoryUsage(&usedMemory, &residentMemory, &sharedMemory)) {
        cerr << "Unable to get memory counts!" << endl;
    }
    else {
        cerr << prefix
            << "Total:" << totalMemory
            << " Used:" << usedMemory << "("
            << (100*usedMemory)/totalMemory <<"%)"
            << " Resident:" << residentMemory << "("
            << int((100.0*residentMemory)/totalMemory) <<"%)"
            << endl;
    }
}

bool CMultiReader::xReadFile(CNcbiIstream& istr, CRef<CSeq_entry>& entry, CRef<CSeq_submit>& submit)
{
    xSetFormat(istr);
    m_iFlags = 0;
    m_iFlags |= CFastaReader::fNoUserObjs;

    switch( m_uFormat )
    {
    case CFormatGuess::eTextASN:
        return xReadASN1(m_uFormat, istr, entry, submit);
        break;
    case CFormatGuess::eGff2:
    case CFormatGuess::eGff3:
        entry = xReadGFF3(istr);
        break;
    case CFormatGuess::eBed:
        entry = xReadBed(istr);
        break;
    default:
        entry = xReadFasta(istr);
        break;
    }
    return entry.NotEmpty();
}

bool 
CMultiReader::xReadASN1(CFormatGuess::EFormat format, CNcbiIstream& instream, CRef<CSeq_entry>& entry, CRef<CSeq_submit>& submit)
{
    auto_ptr<CObjectIStream> pObjIstrm = xCreateASNStream(format, instream);

    // guess object type
    const string sType = pObjIstrm->ReadFileHeader();

    // do the right thing depending on the input type
    if( sType == CBioseq_set::GetTypeInfo()->GetName() ) {
        entry.Reset(new CSeq_entry);
        pObjIstrm->Read(ObjectInfo(entry->SetSet()), CObjectIStream::eNoFileHeader);

        return entry.NotNull();
    }
    else
    if( sType == CSeq_submit::GetTypeInfo()->GetName() ) {
        submit.Reset(new CSeq_submit);
        pObjIstrm->Read(ObjectInfo(*submit), CObjectIStream::eNoFileHeader);

        if (submit->GetData().GetEntrys().size() > 1)
        {
            entry.Reset(new CSeq_entry);
            entry->SetSet().SetSeq_set() = submit->GetData().GetEntrys();
        }
        else
            entry = *submit->SetData().SetEntrys().begin();
    }
    else
    if( sType == CSeq_entry::GetTypeInfo()->GetName() ) {
        entry.Reset(new CSeq_entry);
        pObjIstrm->Read(ObjectInfo(*entry), CObjectIStream::eNoFileHeader);
    }
    else
    {
        return false;
    }

    if (m_context.m_gapNmin > 0)
        CGapsEditor::ConvertNs2Gaps(*entry, m_context.m_gapNmin, m_context.m_gap_Unknown_length, 
        (CSeq_gap::EType)m_context.m_gap_type,
        //(CLinkage_evidence::EType)
        m_context.m_gap_evidences);

    return entry.NotNull();
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
    m_iFlags |= CFastaReader::fAddMods
             |  CFastaReader::fValidate;

    if (m_context.m_handle_as_aa)
    {
        m_iFlags |= CFastaReader::fAssumeProt 
                 |  CFastaReader::fForceType;
                   
    }
    else
    if (m_context.m_handle_as_nuc)
    {
        m_iFlags |= CFastaReader::fAssumeNuc
                 |  CFastaReader::fForceType;
    }
    else
    {
        m_iFlags |= CFastaReader::fAssumeNuc;
    }


    CStreamLineReader lr( instream );
#ifdef NEW_VERSION
    auto_ptr<CBaseFastaReader> pReader(new CBaseFastaReader(m_iFlags));
//    auto_ptr<CBaseFastaReader> pReader(new CFastaReaderEx(lr, m_iFlags));
#else
    auto_ptr<CFastaReader> pReader(new CFastaReaderEx(lr, m_iFlags));
#endif
    if (!pReader.get()) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
            "File format not supported", 0);
    }
#ifdef NEW_VERSION
    CRef<CSeq_entry> result = pReader->ReadSeqEntry(lr , m_context.m_logger);
#else
    if (m_context.m_gapNmin > 0)
    {
        pReader->SetMinGaps(m_context.m_gapNmin, m_context.m_gap_Unknown_length);
        if (m_context.m_gap_evidences.size() >0 || m_context.m_gap_type>=0)
            pReader->SetGapLinkageEvidences((CSeq_gap::EType)m_context.m_gap_type, m_context.m_gap_evidences);
    }

    int max_seqs = kMax_Int;
    CRef<CSeq_entry> result = pReader->ReadSet(max_seqs, m_context.m_logger);
    if (result->IsSet() && !m_context.m_HandleAsSet)
    {
        m_context.m_logger->PutError(*auto_ptr<CLineError>(
            CLineError::Create(ILineError::eProblem_GeneralParsingError, eDiag_Warning, "", 0,
            "File " + m_context.m_current_file + " contains multiple sequences")));
    }
#endif
    return result;

}

//  ----------------------------------------------------------------------------
CFormatGuess::EFormat CMultiReader::GetFormat(CNcbiIstream& in)
{
    xSetFormat(in);
    return m_uFormat;
}

void CMultiReader::xSetFormat(CNcbiIstream& istr )
    //  ----------------------------------------------------------------------------
{
    m_uFormat = CFormatGuess::eUnknown;
#if 0
    const string& strProgramName = "GetProgramDisplayName";

    if (NStr::StartsWith(strProgramName, "wig") || format == "wig" ||
        format == "wiggle") {
            m_uFormat = CFormatGuess::eWiggle;
    }
    if (NStr::StartsWith(strProgramName, "bed") || format == "bed") {
        m_uFormat = CFormatGuess::eBed;
    }
    if (NStr::StartsWith(strProgramName, "b15") || format == "bed15" ||
        format == "microarray") {
            m_uFormat = CFormatGuess::eBed15;
    }
    if (NStr::StartsWith(strProgramName, "gtf") || format == "gtf") {
        m_uFormat = CFormatGuess::eGtf;
    }
    if (NStr::StartsWith(strProgramName, "gff") ||
        format == "gff3" || format == "gff2") {
            m_uFormat = CFormatGuess::eGff3;
    }
    if (NStr::StartsWith(strProgramName, "agp")) {
        m_uFormat = CFormatGuess::eAgp;
    }

    if (NStr::StartsWith(strProgramName, "newick") ||
        format == "newick" || format == "tree" || format == "tre") {
            m_uFormat = CFormatGuess::eNewick;
    }
    if (NStr::StartsWith(strProgramName, "gvf") || format == "gvf") {
        m_uFormat = CFormatGuess::eGtf;
    }
    if (NStr::StartsWith(strProgramName, "aln") || format == "align" ||
        format == "aln") {
            m_uFormat = CFormatGuess::eAlignment;
    }
    if (NStr::StartsWith(strProgramName, "hgvs") ||
        format == "hgvs") {
            m_uFormat = CFormatGuess::eHgvs;
    }
    if( NStr::StartsWith(strProgramName, "fasta") ||
        format == "fasta" ) {
            m_uFormat = CFormatGuess::eFasta;
    }
    if( NStr::StartsWith(strProgramName, "feattbl") ||
        format == "5colftbl" ) {
            m_uFormat = CFormatGuess::eFiveColFeatureTable;
    }
#endif
    CFormatGuess FG(istr);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eBinaryASN);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eFasta);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eTextASN);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eGff3);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eGff2);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eBed);
    FG.GetFormatHints().DisableAllNonpreferred();

    m_uFormat = FG.GuessFormat();
}

//  ----------------------------------------------------------------------------
void CMultiReader::WriteObject(
    const CSerialObject& object,
    ostream& ostr)
    //  ----------------------------------------------------------------------------
{
    ostr << MSerial_AsnText
         //<< MSerial_VerifyNo
         << object;
    ostr.flush();
}

CMultiReader::CMultiReader(const CTable2AsnContext& context)
    :m_context(context)
{
}

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
        if (!m_context.m_Comment.empty())
        {
            CRef<CSeqdesc> comment_desc(new CSeqdesc());
            comment_desc->SetComment(m_context.m_Comment);
            entry.SetDescr().Set().push_back(comment_desc);
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

void CMultiReader::LoadDescriptors(const string& ifname, CRef<CSeq_descr> & out_desc)
{
    out_desc.Reset(new CSeq_descr);

    CNcbiIfstream istrm(ifname.c_str());

    auto_ptr<CObjectIStream> pObjIstrm = xCreateASNStream(CFormatGuess::eUnknown, istrm);

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
            else
                if (sType == CSeqdesc::GetTypeInfo()->GetName())
                {
                    CRef<CSeqdesc> desc(new CSeqdesc);
                    pObjIstrm->Read(ObjectInfo(*desc),
                        CObjectIStream::eNoFileHeader);
                    out_desc->Set().push_back(desc);
                }
                else
                {
                    throw runtime_error("Descriptor file must contain "
                        "either Seq_descr or Seqdesc elements");
                }
        } catch (...) {
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


    CNcbiIfstream istrm(ifname.c_str());

    auto_ptr<CObjectIStream> pObjIstrm = xCreateASNStream(CFormatGuess::eUnknown, istrm);

    // guess object type
    string sType = pObjIstrm->ReadFileHeader();

    // do the right thing depending on the input type
    if( sType == CSeq_entry::GetTypeInfo()->GetName() ) {
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
            const CSeq_descr* descr = 0;
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
}

CRef<CSeq_entry> CMultiReader::CreateNewSeqFromTemplate(const CTable2AsnContext& context, CBioseq& bioseq) const
{
    CRef<CSeq_entry> result(new CSeq_entry);
    if (context.m_entry_template.NotEmpty())
       result->Assign(*context.m_entry_template);
    result->SetSeq(bioseq);

    return result;
}

CFormatGuess::EFormat CMultiReader::LoadFile(CNcbiIstream& istream, CRef<CSeq_entry>& entry, CRef<CSeq_submit>& submit)
{
    xReadFile(istream, entry, submit);
    if (entry.NotEmpty())
    {
        if (entry->IsSet() && entry->GetSet().GetSeq_set().size() < 2 &&
            entry->GetSet().GetSeq_set().front()->IsSeq())
        {           
            CRef<CSeq_entry> seq = entry->SetSet().SetSeq_set().front();
            seq->SetDescr().Set().insert(seq->SetDescr().Set().end(),
                entry->SetDescr().Set().begin(),
                entry->SetDescr().Set().end());
            seq->SetAnnot().insert(seq->SetAnnot().end(),
                entry->SetAnnot().begin(),
                entry->SetAnnot().end());
            entry = seq;
        }
        entry->ResetParentEntry();
        entry->Parentize();
        m_context.MergeWithTemplate(*entry);
    }
    return m_uFormat;
}



void CMultiReader::Cleanup(CRef<CSeq_entry> entry)
{
    /*
    CCleanup cleanup;
    CConstRef<CCleanupChange> changes;
    changes = cleanup.ExtendedCleanup(*entry);
    */
}


#if 0
//  ----------------------------------------------------------------------------
int CMultiReader::RunOld(const CTable2AsnContext& args, const string& ifname, CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    //  const CArgs& args = GetArgs();
    //CNcbiIstream& istr = args["input"].AsInputFile();
    //CNcbiOstream& ostr = args["output"].AsOutputFile();
    //CNcbiIstream& istr = *args.m_input;
    //CNcbiOstream& ostr = *args.m_output;
    CNcbiIfstream istr(ifname.c_str());
    //CNcbiOfstream ostr(ofname);


    xSetFormat(args, istr);
    xSetFlags(args, istr);
    xSetMapper(args);
    xSetErrorContainer(args);

    //CRef< CSerialObject> object;
    //vector< CRef< CSeq_annot > > annots;
    switch( m_uFormat ) {
    default:
        xProcessDefault(args, istr);
        //ApplyAdditionalProperties(args);
        WriteObject(*m_pObject, ostr);
        break;
    case CFormatGuess::eWiggle:
        if (m_iFlags & CReaderBase::fAsRaw) {
            xProcessWiggleRaw(args, istr, ostr);
        }
        else {
            xProcessWiggle(args, istr, ostr);
        }
        break;
    case CFormatGuess::eBed:
        if (m_iFlags & CReaderBase::fAsRaw) {
            xProcessBedRaw(args, istr, ostr);
        }
        else {
            xProcessBed(args, istr, ostr);
        }
        break;
    case CFormatGuess::eGtf:
    case CFormatGuess::eGtf_POISENED:
        xProcessGtf(args, istr, ostr);
        break;
    case CFormatGuess::eVcf:
        xProcessVcf(args, istr, ostr);
        break;
    case CFormatGuess::eNewick:
        xProcessNewick(args, istr, ostr);
        break;
    case CFormatGuess::eGff3:
        xProcessGff3(args, istr, ostr);
        break;
    case CFormatGuess::eGff2:
        xProcessGff2(args, istr, ostr);
        break;
    case CFormatGuess::eGvf:
        xProcessGvf(args, istr, ostr);
        break;
    case CFormatGuess::eAgp:
        xProcessAgp(args, istr, ostr);
        break;
    case CFormatGuess::eAlignment:
        xProcessAlignment(args, istr, ostr);
        break;
    }

    xDumpErrors( cerr );
    return 0;
}

//  ----------------------------------------------------------------------------
void CMultiReader::xProcessWiggle(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CSeq_annot> > ANNOTS;
    ANNOTS annots;

    CWiggleReader reader(m_iFlags);
    CStreamLineReader lr(istr);
    reader.ReadSeqAnnots(annots, istr, m_pErrors);
    for (ANNOTS::iterator cit = annots.begin(); cit != annots.end(); ++cit){
        xWriteObject(**cit, ostr);
    }
}


//  ----------------------------------------------------------------------------
void CMultiReader::xProcessWiggleRaw(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    CWiggleReader reader(m_iFlags);
    CStreamLineReader lr(istr);
    CRawWiggleTrack raw;
    while (reader.ReadTrackData(lr, raw)) {
        raw.Dump(cerr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReader::xProcessBed(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    //  Use ReadSeqAnnot() over ReadSeqAnnots() to keep memory footprint down.
    CBedReader reader(m_iFlags);
    CStreamLineReader lr(istr);
    CRef<CSeq_annot> pAnnot = reader.ReadSeqAnnot(lr, m_pErrors);
    while(pAnnot) {
        xWriteObject(*pAnnot, ostr);
        pAnnot.Reset();
        pAnnot = reader.ReadSeqAnnot(lr, m_pErrors);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReader::xProcessBedRaw(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    CBedReader reader(m_iFlags);
    CStreamLineReader lr(istr);
    CRawBedTrack raw;
    while (reader.ReadTrackData(lr, raw)) {
        raw.Dump(cerr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReader::xProcessGtf(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CSeq_annot> > ANNOTS;
    ANNOTS annots;

    if (args.m_format == "gff2") { // process as plain GFF2
        return xProcessGff2(args, istr, ostr);
    }
    CGtfReader reader(m_iFlags, m_AnnotName, m_AnnotTitle);
    reader.ReadSeqAnnots(annots, istr, m_pErrors);
    for (ANNOTS::iterator cit = annots.begin(); cit != annots.end(); ++cit){
        xWriteObject(**cit, ostr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReader::xProcessGff2(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CSeq_annot> > ANNOTS;
    ANNOTS annots;

    CGff2Reader reader(m_iFlags, m_AnnotName, m_AnnotTitle);
    reader.ReadSeqAnnots(annots, istr, m_pErrors);
    for (ANNOTS::iterator cit = annots.begin(); cit != annots.end(); ++cit){
        xWriteObject(**cit, ostr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReader::xProcessGvf(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CSeq_annot> > ANNOTS;
    ANNOTS annots;

    if (args.m_format == "gff2") { // process as plain GFF2
        return xProcessGff2(args, istr, ostr);
    }
    if (args.m_format == "gff3") { // process as plain GFF2
        return xProcessGff3(args, istr, ostr);
    }
    CGvfReader reader(m_iFlags, m_AnnotName, m_AnnotTitle);
    reader.ReadSeqAnnots(annots, istr, m_pErrors);
    for (ANNOTS::iterator cit = annots.begin(); cit != annots.end(); ++cit){
        xWriteObject(**cit, ostr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReader::xProcessVcf(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CSeq_annot> > ANNOTS;
    ANNOTS annots;

    CVcfReader reader( m_iFlags );
    reader.ReadSeqAnnots(annots, istr, m_pErrors);
    for (ANNOTS::iterator cit = annots.begin(); cit != annots.end(); ++cit){
        xWriteObject(**cit, ostr);
    }
}

//  ----------------------------------------------------------------------------
void CMultiReader::xProcessNewick(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    string strTree;
    char c = istr.get();
    while (!istr.eof()) {
        strTree += c;
        if (c == ';') {
            CNcbiIstrstream istrstr(strTree.c_str());
            auto_ptr<TPhyTreeNode>  pTree(ReadNewickTree(istrstr) );
            CRef<CBioTreeContainer> btc = MakeBioTreeContainer(pTree.get());
            xWriteObject(*btc, ostr);
            strTree.clear();
        }
        c = istr.get();
    }
}


//  ----------------------------------------------------------------------------
void CMultiReader::xProcessAgp(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    typedef vector<CRef<CSeq_entry> > TEntries;
    TEntries entries;
    AgpRead(istr, entries);
    NON_CONST_ITERATE (TEntries, it, entries) {
        xWriteObject(**it, ostr);
    }
}


//  ----------------------------------------------------------------------------
void CMultiReader::xProcessAlignment(
    const CTable2AsnContext& args,
    CNcbiIstream& istr,
    CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    CAlnReader reader(istr);
    reader.SetAllGap(args["aln-gapchar"].AsString());
    reader.SetAlphabet(CAlnReader::eAlpha_Nucleotide);
    if (args["aln-alphabet"].AsString() == "prot") {
        reader.SetAlphabet(CAlnReader::eAlpha_Protein);
    }
    reader.Read(false, args["force-local-ids"]);
    CRef<CSeq_align> pAlign = reader.GetSeqAlign();
    xWriteObject(*pAlign, ostr);
}

void CMultiReader::xDumpErrors(CNcbiOstream& ostr)
    //  ----------------------------------------------------------------------------
{
    if (m_pErrors) {
        m_pErrors->Dump(ostr);
    }
}


#endif

CRef<CSeq_entry> CMultiReader::xReadGFF3(CNcbiIstream& instream)
{
    int flags = 0;
    flags |= CGff3Reader::fAllIdsAsLocal;
    flags |= CGff3Reader::fNewCode;
    flags |= CGff3Reader::fGenbankMode;

    CGff3Reader reader(flags, m_AnnotName, m_AnnotTitle);
    CStreamLineReader lr(instream);
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSeq();
    reader.ReadSeqAnnotsNew(entry->SetAnnot(), lr, m_context.m_logger);

    return entry;
}

auto_ptr<CObjectIStream> CMultiReader::xCreateASNStream(CFormatGuess::EFormat format, CNcbiIstream& instream)
{
    // guess format
    ESerialDataFormat eSerialDataFormat = eSerial_None;
    {{
        if (format == CFormatGuess::eUnknown)
            format = CFormatGuess::Format(instream);

        switch(format) {
        case CFormatGuess::eBinaryASN:
            eSerialDataFormat = eSerial_AsnBinary;
            break;
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

    auto_ptr<CObjectIStream> pObjIstrm(
        CObjectIStream::Open(eSerialDataFormat, instream, eNoOwnership) );

    return pObjIstrm;
}

CMultiReader::~CMultiReader()
{
}

bool CMultiReader::LoadAnnot(objects::CSeq_entry& obj, CNcbiIstream& in)
{
    CRef<CSeq_entry> entry;
    CRef<CSeq_submit> submit;
    if (xReadFile(in, entry, submit)
        && entry->IsSetAnnot() && !entry->GetAnnot().empty())
    {
        obj.SetAnnot().insert(obj.SetAnnot().end(),
            entry->SetAnnot().begin(), entry->SetAnnot().end());

        return true;
    }
    return false;
    }

CRef<CSeq_entry> CMultiReader::xReadBed(CNcbiIstream& instream)
{
    //  Use ReadSeqAnnot() over ReadSeqAnnots() to keep memory footprint down.
    CBedReader reader(m_iFlags);
    CStreamLineReader lr(instream);
    CRef<CSeq_annot> pAnnot = reader.ReadSeqAnnot(lr, m_context.m_logger);
    CRef<CSeq_entry> entry(new CSeq_entry);
    while(pAnnot) {
        entry->SetSeq().SetAnnot().push_back(pAnnot);
        pAnnot.Reset();
        pAnnot = reader.ReadSeqAnnot(lr, m_context.m_logger);
    }
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_virtual);
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_other);
    //entry->SetSeq().ResetInst();
    return entry;
}

END_NCBI_SCOPE
