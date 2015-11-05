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
#include <objects/seq/Seq_descr.hpp>
#include "splitter_exception.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

class CSeqSubSplitter : public CNcbiApplication
{

public:
    void Init();
    int Run();

private:
    CObjectIStream* xInitInputStream(const CArgs& args) const; // Why CObjectIStream instead of NcbiIstream here?

    CObjectOStream* xInitOutputStream(const string& output_stub, 
                                      const TSeqPos output_index,
                                      const TSeqPos pad_width,
                                      const string& output_extension,
                                      const bool binary) const;


    bool xTryReadInputFile(const CArgs& args, 
                           CRef<CSeq_submit>& seq_sub) const;
    
    bool xTryProcessSeqSubmit(CRef<CSeq_submit>& seq_sub,
                              TSeqPos sort_order,
                              TSeqPos bundle_size,
                              bool wrap_entries,
                              list<CRef<CSeq_submit> >& output_array) const;
   
    void xWrapSeqEntries(vector<CRef<CSeq_entry> >& seq_entry_array, 
                         const TSeqPos& bundle_size,
                         vector<CRef<CSeq_entry> >& wrapped_entry_array) const;

    void xFlattenSeqEntrys(CSeq_submit::TData::TEntrys& entries,
                           vector<CRef<CSeq_entry> >& seq_entry_array) const;

    void xFlattenSeqEntry(CSeq_entry& seq_entry, 
                          const CSeq_descr& seq_descr,
                          vector<CRef<CSeq_entry> >& seq_entry_array) const;

    void xMergeSeqDescr(const CSeq_descr& src, CSeq_descr& dst) const; 


    string xGetFileExtension(const string& filename) const;
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
        arg_desc->AddKey("i", "InputFile",
                         "Filename for asn.1 input", 
                         CArgDescriptions::eInputFile);
    }
 
    {
        arg_desc->AddDefaultKey("b",
                                "BOOLEAN",
                                "Input asn.1 file in binary mode [T/F]",
                                CArgDescriptions::eBoolean,
                                "F");

   //     arg_desc->SetConstraint("b", &(*new CArgAllow_Strings, "T", "F"));
    }

    // output
    {
        string description = "Filename stub for asn.1 outputs.\n";
        description.append("Will append consecutive numbers and a file-type extension to this stub");

        arg_desc->AddKey("o", "OutputFile",
                         description,
                         CArgDescriptions::eOutputFile);
    }

    {
        arg_desc->AddDefaultKey("s",
                                "BOOLEAN",
                                "Output asn.1 files in binary mode [T/F]",
                                CArgDescriptions::eBoolean,
                                "F");

    }

    { 
        arg_desc->AddDefaultKey("w",
                                "BOOLEAN",
                                "Wrap output Seq-entries within Seq-submits with Genbank set [T/F]",
                                CArgDescriptions::eBoolean,
                                "F");
    }

    // logfile alias 
    { 
        arg_desc->AddAlias("l", "logfile"); 
    }

    // parameters 
    {
        arg_desc->AddDefaultKey("n",
                                "POSINT",
                                "Number of records in output Seq-submits",
                                CArgDescriptions::eInteger,
                                "1");

        string description = "Generate output in sorted order \n";
        description.append("  0 - unsorted (in order of appearance in input file;\n");
        description.append("  1 - by sequence length from longest to shortest;\n");
        description.append("  2 - by sequence length from shortest to longest;\n");
        description.append("  3 - by contig/scaffold id.");

        arg_desc->AddDefaultKey("r",
                                "INTEGER",
                                description,
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
    if (!xTryReadInputFile(args, input_sub)) {
        string err_msg = "Could not read input Seq-submit";
        ERR_POST(err_msg);
        return 0;
    }

    list<CRef<CSeq_submit> > output_array;

    const bool wrap_entries = args["w"].AsBoolean(); // Wrap the output Seq-entries 
                                                     // within a Seq-submit in a Genbank set

    if(!xTryProcessSeqSubmit(input_sub, 
                             sort_order, 
                             bundle_size,
                             wrap_entries,
                             output_array)) {
        string err_msg = "Could not process input Seq-submit";
        ERR_POST(err_msg);
        return 0;
    }

    const string output_stub = args["o"].AsString();


    // Output files should have the same extension as the input file
    string output_extension = "";
    if (args["i"]) {
        output_extension = xGetFileExtension(args["i"].AsString());
    }

    int output_index = 0;
    auto_ptr<CObjectOStream> ostr;
    bool binary = args["s"].AsBoolean();

    const TSeqPos pad_width = log10(output_array.size()) + 1;

    NON_CONST_ITERATE(list<CRef<CSeq_submit> >, it, output_array) {
        ++output_index;
        ostr.reset(xInitOutputStream(output_stub, 
                    output_index, 
                    pad_width, 
                    output_extension,
                    binary
                    ));
        *ostr << **it;
    }

    return 0;
}


string CSeqSubSplitter::xGetFileExtension(const string& filename) const
{
   string extension = "";
   vector<string> arr;
   NStr::Tokenize(filename,".",arr);
   if (arr.size() > 1) {
       extension = arr.back();
   }

   return extension; 
}


CObjectOStream* CSeqSubSplitter:: xInitOutputStream(
        const string& output_stub,
        const TSeqPos output_index,
        const TSeqPos pad_width,
        const string& output_extension,
        const bool binary) const
{
    if (output_stub.empty()) {
        NCBI_THROW(CSeqSubSplitException, 
                   eEmptyOutputStub, 
                   "Output stub not specified");
    }

    string padded_index;
    {
        const string padding = string(pad_width, '0');
        padded_index = NStr::IntToString(output_index);

        if (padded_index.size() < pad_width) {
            padded_index = padding.substr(0, pad_width - padded_index.size()) + padded_index;
        } 
    }

    string filename = output_stub + "_" + padded_index + "." + output_extension;

    ESerialDataFormat serial = eSerial_AsnText;
    if (binary) {
        serial = eSerial_AsnBinary;
    }
    CObjectOStream* pOstr = CObjectOStream::Open(filename, serial);

    if (!pOstr) {
        NCBI_THROW(CSeqSubSplitException, 
                   eOutputError, 
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


// I guess that I could make Comparison classes subclasses of CSeqSubSplitter 
template<class Derived>
struct SCompare
{
    bool operator()(const CRef<CSeq_entry>& e1, const CRef<CSeq_entry>& e2) const
    {
        const CBioseq& b1 = e1->IsSeq() ? e1->GetSeq() : e1->GetSet().GetNucFromNucProtSet();

        const CBioseq& b2 = e2->IsSeq() ? e2->GetSeq() : e2->GetSet().GetNucFromNucProtSet();

        return static_cast<const Derived*>(this)->compare_seq(b1, b2);
    }

    bool compare_seq(const CBioseq& b1, const CBioseq& b2) const
    {
        return true;
    }
};


struct SLongestFirstCompare : public SCompare<SLongestFirstCompare>
{
    bool compare_seq(const CBioseq& b1, const CBioseq& b2) const
    {
        if (!b1.IsSetInst() || !b2.IsSetInst()) {
            NCBI_THROW(CSeqSubSplitException, eInvalidSeqinst, "Bioseq inst not set");
        }

        if (!b1.GetInst().IsSetLength() || // Length must be set
            !b2.GetInst().IsSetLength())
        {
            return true;
        }

        return (b1.GetInst().GetLength() > b2.GetInst().GetLength());
    }
};


struct SShortestFirstCompare : public SCompare<SShortestFirstCompare>
{
    bool compare_seq(const CBioseq& b1, const CBioseq& b2) const 
    {
        if (!b1.IsSetInst() || !b2.IsSetInst()) {
            NCBI_THROW(CSeqSubSplitException, eInvalidSeqinst, "Bioseq inst not set");
        }

        if (!b1.GetInst().IsSetLength() || // Length must be set
            !b2.GetInst().IsSetLength())
        {
            return true;
        }
        return (b1.GetInst().GetLength() < b2.GetInst().GetLength());
    }
};





struct SIdCompare : public SCompare<SIdCompare>
{

    CConstRef<CSeq_id> xGetGeneralId(const CBioseq& bioseq) const 
    {
        const CBioseq::TId& ids = bioseq.GetId();

        ITERATE(CBioseq::TId, id_itr, ids) { 
            CConstRef<CSeq_id> id = *id_itr;

            if (id && id->IsGeneral()) {
                return id;
            }
        }

        return CConstRef<CSeq_id>();
    }

    CConstRef<CSeq_id> xGetId(const CBioseq& bioseq) const 
    {
        if (bioseq.GetLocalId()) {
           return CConstRef<CSeq_id>(bioseq.GetLocalId());
        }
        return xGetGeneralId(bioseq);
    }


    bool compare_seq(const CBioseq& b1, const CBioseq& b2) const 
    {
        if (!b1.IsSetId() || !b2.IsSetId()) {
            NCBI_THROW(CSeqSubSplitException, 
                       eInvalidSeqid, 
                       "Bioseq id not set");
        }

        CConstRef<CSeq_id> id1 = xGetId(b1);
        CConstRef<CSeq_id> id2 = xGetId(b2);

        if (id1.IsNull() || id2.IsNull()) {
            NCBI_THROW(CSeqSubSplitException, 
                       eSeqIdError, 
                       "Cannot access bioseq id");
        }

        if (id1->IsGeneral() != id2->IsGeneral()) {
            NCBI_THROW(CSeqSubSplitException, 
                       eSeqIdError, 
                       "Inconsistent bioseq ids");
        }

        return (id1->CompareOrdered(*id2) < 0);
    }
};



void CSeqSubSplitter::xWrapSeqEntries(vector<CRef<CSeq_entry> >& seq_entry_array, 
                                      const TSeqPos& bundle_size,
                                      vector<CRef<CSeq_entry> >& wrapped_entry_array) const
{
    vector<CRef<CSeq_entry> >::iterator seq_entry_it = seq_entry_array.begin();
    while (seq_entry_it != seq_entry_array.end()) {
        CRef<CSeq_entry> seq_entry = Ref(new CSeq_entry());
        seq_entry->SetSet().SetClass(CBioseq_set::eClass_genbank);
        for (TSeqPos i=0; i<bundle_size; ++i) {
           seq_entry->SetSet().SetSeq_set().push_back(*seq_entry_it);   
           ++seq_entry_it;
           if (seq_entry_it == seq_entry_array.end()) {
               break;
           }
        }
        wrapped_entry_array.push_back(seq_entry);
    }
}



bool CSeqSubSplitter::xTryProcessSeqSubmit(CRef<CSeq_submit>& input_sub,
                                           const TSeqPos sort_order,
                                           const TSeqPos bundle_size,
                                           bool wrap_entries,
                                           list<CRef<CSeq_submit> >& output_array) const
{
    if (!input_sub->IsEntrys()) {
        ERR_POST("Seq-submit does not contain any entries");
        return false;
    }

    vector<CRef<CSeq_entry> > seq_entry_array;

    CRef<CSeq_descr> seq_descr = Ref(new CSeq_descr());
    xFlattenSeqEntrys(input_sub->SetData().SetEntrys(), seq_entry_array);

    switch(sort_order) {
        default:
            NCBI_THROW(CSeqSubSplitException,  
                       eInvalidSortOrder,      
                       "Unrecognized sort order: " + NStr::IntToString(sort_order));
        case 0:
            break;
        case 1:
            stable_sort(seq_entry_array.begin(), seq_entry_array.end(), SLongestFirstCompare());
            break;
        case 2:
            stable_sort(seq_entry_array.begin(), seq_entry_array.end(), SShortestFirstCompare());
            break;
        case 3:
            stable_sort(seq_entry_array.begin(), seq_entry_array.end(), SIdCompare());
            break;
    } 

    if (wrap_entries) { // wrap the entries inside a genbank set
        vector<CRef<CSeq_entry> > wrapped_entry_array;
        xWrapSeqEntries(seq_entry_array, bundle_size, wrapped_entry_array);
        for(int i=0; i<wrapped_entry_array.size(); ++i) {
            CRef<CSeq_submit> seqsub = Ref(new CSeq_submit());
            seqsub->SetSub(input_sub->SetSub());
            seqsub->SetData().SetEntrys().push_back(wrapped_entry_array[i]);
            output_array.push_back(seqsub);
        }
    } else {
        vector<CRef<CSeq_entry> >::iterator seq_entry_it = seq_entry_array.begin();
        while (seq_entry_it != seq_entry_array.end()) {
            CRef<CSeq_submit> seqsub = Ref(new CSeq_submit());
            seqsub->SetSub(input_sub->SetSub());
            for(TSeqPos i=0; i<bundle_size; ++i) {
                seqsub->SetData().SetEntrys().push_back(*seq_entry_it);
                ++seq_entry_it;
                if (seq_entry_it == seq_entry_array.end()) {
                    break;
                }
            }
            output_array.push_back(seqsub);
        }
    }
    return true;
}



CObjectIStream* CSeqSubSplitter::xInitInputStream(const CArgs& args) const
{
    if (!args["i"]) {
        NCBI_THROW(CSeqSubSplitException, 
                   eInputError,
                   "Input file unspecified");
    }

    ESerialDataFormat serial = eSerial_AsnText;
    if (args["b"].AsBoolean()) {
        serial = eSerial_AsnBinary;
    }

    string infile_str = args["i"].AsString();
    CNcbiIstream* pInputStream = new CNcbiIfstream(infile_str.c_str(), ios::in | ios::binary);

    if (pInputStream->fail()) 
    {
       NCBI_THROW(CSeqSubSplitException,
                  eInputError,
                  "Could not create input stream for \"" + infile_str + "\"");
    }


    CObjectIStream* p_istream = CObjectIStream::Open(serial, 
                                                     *pInputStream, 
                                                     eTakeOwnership);
    
    if (!p_istream) {
        NCBI_THROW(CSeqSubSplitException, 
                   eInputError,
                   "Unable to open input file \"" + infile_str + "\"");

    }

    return p_istream;
}


void CSeqSubSplitter::xMergeSeqDescr(const CSeq_descr& src, CSeq_descr& dst) const
{
    if (!src.IsSet()) {
        return;
    }

    ITERATE(CSeq_descr::Tdata, it, src.Get()) 
    {
        // Need to insert a check to make sure there aren't conflicting entries in the 
        // src and destination.
        CRef<CSeqdesc> desc = CAutoAddDesc::LocateDesc(dst, (**it).Which());
        if (desc.Empty()) { 
            desc.Reset(new CSeqdesc());
            desc->Assign(**it);
            dst.Set().push_back(desc);
        }
    }

    return;
}


void CSeqSubSplitter::xFlattenSeqEntrys(CSeq_submit::TData::TEntrys& entries,
                                       vector<CRef<CSeq_entry> >& seq_entry_array) const 
{
    NON_CONST_ITERATE(CSeq_submit::TData::TEntrys, it, entries) {
        CSeq_entry& seq_entry = **it;
        CSeq_descr seq_descr;
        xFlattenSeqEntry(seq_entry, seq_descr, seq_entry_array);     
    }
}


void CSeqSubSplitter::xFlattenSeqEntry(CSeq_entry& entry,
                                       const CSeq_descr& seq_descr,
                                       vector<CRef<CSeq_entry> >& seq_entry_array) const 
{
    if (entry.IsSeq() ||
        (entry.IsSet() &&
         (!entry.GetSet().IsSetClass() ||
          (entry.GetSet().IsSetClass() && 
           (entry.GetSet().GetClass() != CBioseq_set::eClass_genbank) &&
           (entry.GetSet().GetClass() != CBioseq_set::eClass_pub_set))
          ))) 
    {

        CRef<CSeq_entry> new_entry = Ref(&entry);

        if (seq_descr.IsSet()) {
            CSeq_descr&  entry_descr = (entry.IsSeq()) 
                                ? entry.SetSeq().SetDescr() 
                                : entry.SetSet().SetDescr();


            xMergeSeqDescr(seq_descr, entry_descr);
        }

        seq_entry_array.push_back(new_entry);
        return;
    }

    // Class has to be either genbank or pub_set
    // These sets should not have annotations
    const CBioseq_set& seq_set = entry.GetSet();

    if (seq_set.IsSetAnnot()) {

        string class_string = (seq_set.GetClass() == CBioseq_set::eClass_genbank) 
                            ? "Genbank set" : "Pub-set";

        string err_msg = "Wrapper " + class_string + "has non-empty annotation.";

        NCBI_THROW(CSeqSubSplitException, eInvalidAnnot, err_msg);
    }

    CSeq_descr new_descr;
    ITERATE(CSeq_descr::Tdata, it, seq_descr.Get()) {
        new_descr.Set().push_back(*it);
    }

    if (entry.GetSet().IsSetDescr()) {
        xMergeSeqDescr(entry.GetSet().GetDescr(), new_descr); 
    }

    NON_CONST_ITERATE(CBioseq_set::TSeq_set, it, entry.SetSet().SetSeq_set()) {
        xFlattenSeqEntry(**it, new_descr, seq_entry_array);
    }
}



END_NCBI_SCOPE


USING_NCBI_SCOPE;
int main(int argc, const char** argv)
{
    return CSeqSubSplitter().AppMain(argc, argv, 0, eDS_ToStderr, 0);
    return 0;
}

