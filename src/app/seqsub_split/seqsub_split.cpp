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
* Author: Justin Foley
*
* File Description:
*   seq-submit splitter application
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <corelib/ncbidiag.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqloc/Seq_id.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

class CSeqSubSplitter : public CNcbiApplication
{

public:
    void Init();
    int Run();
    CObjectIStream* xInitInputStream(const CArgs& args) const; // Why CObjectIStream instead of NcbiIstream here?

    CObjectOStream* xInitOutputStream(const string& output_stub, 
                                      const TSeqPos output_index,
                                      const TSeqPos pad_width,
                                      const bool binary) const;


    bool xTryReadInputFile(const CArgs& args, 
                           CRef<CSeq_submit>& seq_sub) const;
    
    bool xTryProcessSeqSubmit(CRef<CSeq_submit>& seq_sub,
                              TSeqPos sort_order,
                              TSeqPos bundle_size,
                              list<CRef<CSeq_submit> >& output_array) const;
    
    void xFlattenSeqEntrys(CSeq_submit::TData::TEntrys& entries,
                           vector<CRef<CBioseq> >& seq_array) const;

    void xFlattenBioseqSet(CBioseq_set& bss, 
                           vector<CRef<CBioseq> >& seq_array) const;
};


void CSeqSubSplitter::Init()
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(
            GetArguments().GetProgramBasename(),
            "Split a single large instance of Seq-submit into smaller instances",
            false);
    // input
    {
        arg_desc->AddOptionalKey("i", "InputFile",
                                 "Filename for asn.1 input", 
                                 CArgDescriptions::eInputFile);
    }
 

    {
        arg_desc->AddFlag("b",
                          "Input asn.1 file in binary mode",
                           true);
    }

    // output
    {
        arg_desc->AddKey("o", "OutputFile",
                         "Filename for asn.1 outputs. \ 
                         Will append consecutative numbers \"_NNN\" to this name", 
                         CArgDescriptions::eOutputFile);
    }

    // flags
    {
        arg_desc->AddFlag("s",
                          "Output asn.1 files in binary mode",
                          true);
    }

    // parameters 
    {
        arg_desc->AddDefaultKey("n",
                                "POSINT",
                                "Number of records in output Seq-submits",
                                CArgDescriptions::eInteger,
                                "1");

        arg_desc->AddDefaultKey("r",
                                "INTEGER",
                                "Sorting order",
                                CArgDescriptions::eInteger,
                                "0");
        arg_desc->SetConstraint("r",
                               &(*new CArgAllow_Strings,
                                 "0", "1", "2", "3"));
    }
    
    SetupArgDescriptions(arg_desc.release());
}


int CSeqSubSplitter::Run()
{
    const CArgs& args = GetArgs();
    const TSeqPos bundle_size = args["n"].AsInteger();
    const TSeqPos sort_order = args["r"].AsInteger();

    CRef<CSeq_submit> input_sub;
    xTryReadInputFile(args, input_sub);

    list<CRef<CSeq_submit> > output_array;
    xTryProcessSeqSubmit(input_sub, 
                         sort_order, 
                         bundle_size,
                         output_array);


    const string output_stub = args["o"].AsString();
    int output_index = 0;
    auto_ptr<CObjectOStream> ostr;
    bool binary = false;


    const TSeqPos pad_width = log10(output_array.size()) + 1;

    NON_CONST_ITERATE(list<CRef<CSeq_submit> >, it, output_array) {
        ++output_index;
        ostr.reset(xInitOutputStream(output_stub, output_index, pad_width, binary));
        *ostr << **it;
    }

    return 0;
}



CObjectOStream* CSeqSubSplitter:: xInitOutputStream(
        const string& output_stub,
        const TSeqPos output_index,
        const TSeqPos pad_width,
        const bool binary) const
{
    if (output_stub.empty()) {
        NCBI_THROW(CException, eUnknown, "Output stub not specified");
    }

    string padded_index;
    {
        const string padding = string(pad_width, '0');
        padded_index = NStr::IntToString(output_index);

        if (padded_index.size() < pad_width) {
            padded_index = padding.substr(0, pad_width - padded_index.size()) + padded_index;
        } 
    }

    string filename = output_stub + "_" + padded_index;

    ESerialDataFormat serial = eSerial_AsnText;
    if (binary) {
        serial = eSerial_AsnBinary;
    }
    CObjectOStream* pOstr = CObjectOStream::Open(filename, serial);

    if (!pOstr) {
        NCBI_THROW(CException, 
                   eUnknown, 
                   "Unable to open output file:" + filename);
    }
    return pOstr;
}



bool CSeqSubSplitter::xTryReadInputFile(const CArgs& args,
                                        CRef<CSeq_submit>& seq_sub) const
{
    auto_ptr<CObjectIStream> istr;
    istr.reset(xInitInputStream(args));
    
    CRef<CSeq_submit> input_sub = Ref(new CSeq_submit());
    try {
        istr->Read(ObjectInfo(*input_sub));
    }
    catch (CException&) {
        return false;
    }

    seq_sub = input_sub;
 
    return true;
}


static bool s_ShortestFirstCompare(const CRef<CBioseq>& b1, const CRef<CBioseq>& b2)
{
    if (b1.IsNull() || b2.IsNull() ||
        !b1->IsSetInst() || !b2->IsSetInst())
    {
        return true;
    }

    if (!b1->GetInst().IsSetLength() ||
        !b2->GetInst().IsSetLength()) 
    {
        return true;
    }

   return (b1->GetInst().GetLength() < b2->GetInst().GetLength());
}


static bool s_LongestFirstCompare(const CRef<CBioseq>& b1, const CRef<CBioseq>& b2)
{
    if (b1.IsNull() || b2.IsNull() ||
        !b1->IsSetInst() || !b2->IsSetInst())
    {
        return true;
    }

    if (!b1->GetInst().IsSetLength() ||
        !b2->GetInst().IsSetLength()) 
    {
        return true;
    }

    return (b1->GetInst().GetLength() > b2->GetInst().GetLength());
}


static bool s_LocalIdCompare(const CRef<CBioseq>& b1, const CRef<CBioseq>& b2) 
{
    if (b1.IsNull() || b2.IsNull() ||
        !b1->IsSetId() || !b2->IsSetId()) 
    {
        return true;
    }

    const CSeq_id* id1 = b1->GetLocalId();
    const CSeq_id* id2 = b2->GetLocalId();

    if (!id1 || !id2)
    {
        return true;
    }

    return (id1->CompareOrdered(*id2) < 0);
}



bool CSeqSubSplitter::xTryProcessSeqSubmit(CRef<CSeq_submit>& input_sub,
                                           const TSeqPos sort_order,
                                           const TSeqPos bundle_size,
                                           list<CRef<CSeq_submit> >& output_array) const
{

    vector<CRef<CBioseq> > seq_array;
    xFlattenSeqEntrys(input_sub->SetData().SetEntrys(), seq_array);

    switch(sort_order) {
        default:
            NCBI_THROW(CException, eUnknown, "Unrecognized sequence ordering");
        case 0:
            break;
        case 1:
            stable_sort(seq_array.begin(), seq_array.end(), s_LongestFirstCompare);
            break;
        case 2:
            stable_sort(seq_array.begin(), seq_array.end(), s_ShortestFirstCompare);
            break;
        case 3:
            stable_sort(seq_array.begin(), seq_array.end(), s_LocalIdCompare);
            break;
    } 

    if (seq_array.size() <= bundle_size) {
        output_array.push_back(input_sub);
        return true;
    }

    vector<CRef<CBioseq> >::iterator seq_it = seq_array.begin();
    while (seq_it != seq_array.end()) {
        CRef<CSeq_submit> seqsub = Ref(new CSeq_submit());
        seqsub->SetSub(input_sub->SetSub()); // Not sure if this is the best thing to do
        for(TSeqPos i=0; i<bundle_size; ++i) {
            CRef<CSeq_entry> seq_entry(new CSeq_entry());
            seq_entry->SetSeq(**seq_it);
            seqsub->SetData().SetEntrys().push_back(seq_entry);
            ++seq_it;
            if (seq_it == seq_array.end()) {
                break;
            }
        }
        output_array.push_back(seqsub);
    }

    return true;
}




CObjectIStream* CSeqSubSplitter::xInitInputStream(const CArgs& args) const
{
    ESerialDataFormat serial = eSerial_AsnText;
    if (args["b"]) {
        serial = eSerial_AsnBinary;
    }
    CNcbiIstream* pInputStream = &NcbiCin;
 
    bool bDeleteOnClose = false;
    if (args["i"]) {
        const char* infile = args["i"].AsString().c_str();
        pInputStream = new CNcbiIfstream(infile, ios::binary);
        bDeleteOnClose = true;
    }
    CObjectIStream* p_istream = CObjectIStream::Open(
            serial, *pInputStream, (bDeleteOnClose ? eTakeOwnership : eNoOwnership));

    if (!p_istream) {
        NCBI_THROW(CException, eUnknown,
                "seqsub_split: Unable to open input file");
    }
    return p_istream;
}



void CSeqSubSplitter::xFlattenSeqEntrys(CSeq_submit::TData::TEntrys& entries,
                                        vector< CRef<CBioseq> >& seq_array) const
{
    NON_CONST_ITERATE(CSeq_submit::TData::TEntrys, it, entries) {
        CSeq_entry& seq_entry = **it;
        if (seq_entry.IsSet()) {
            xFlattenBioseqSet(seq_entry.SetSet(), seq_array); // Copied from blast
        } else {
            _ASSERT(seq_entry.IsSeq());
            CRef<CBioseq> bioseq = Ref(&(seq_entry.SetSeq()));
            seq_array.push_back(bioseq);
        }
    }
}


void CSeqSubSplitter::xFlattenBioseqSet(CBioseq_set& bss, vector< CRef<CBioseq> >& seq_array) const 
{ // Copied from src/algo/blast/api/remote_blast.cpp
    if (bss.IsSetSeq_set()) {
        NON_CONST_ITERATE(CBioseq_set::TSeq_set, it, bss.SetSeq_set()) {
            CSeq_entry& seq_entry = **it;

            if (seq_entry.IsSeq()) {
                CRef<CBioseq> bioseq = Ref(&(seq_entry.SetSeq()));
                seq_array.push_back(bioseq);
            }
            else {
                _ASSERT(seq_entry.IsSet());
                xFlattenBioseqSet(seq_entry.SetSet(), seq_array);
            }
        }
    }
}


END_NCBI_SCOPE


USING_NCBI_SCOPE;
int main(int argc, const char** argv)
{
    return CSeqSubSplitter().AppMain(argc, argv, 0, eDS_ToStderr, 0);
    return 0;
}

