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
#include <math.h>

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

namespace seqsubmit_split
{

typedef vector<CRef<CSeq_entry> > TSeqEntryArray;
typedef list<CRef<CSeq_entry> > TSeqEntryList;

class CObjectHelper
{
public:
    virtual CRef<CSerialObject> BuildObject() const = 0;
    virtual TSeqEntryList& GetListOfEntries(CSerialObject& obj) const = 0;
};

class CSeqSubSplitter : public CNcbiApplication
{

public:
    void Init();
    int Run();

private:
    CObjectIStream* xInitInputStream() const; // Why CObjectIStream instead of NcbiIstream here?

    CObjectOStream* xInitOutputStream(const string& output_stub, 
                                      const TSeqPos output_index,
                                      const TSeqPos pad_width,
                                      const string& output_extension,
                                      const bool binary) const;


    CRef<CSerialObject> xGetInputObject() const;

    bool xTryReadInputFile(CRef<CSerialObject>& obj) const;
    
    bool xTryProcessSeqEntries(const CObjectHelper& builder, TSeqEntryArray& seq_entry_array,
                               list<CRef<CSerialObject>>& output_array) const;
   
    bool xTryProcessSeqSubmit(CRef<CSerialObject>& obj,
                              list<CRef<CSerialObject>>& output_array) const;
    bool xTryProcessSeqEntry(CRef<CSerialObject>& obj,
                             list<CRef<CSerialObject>>& output_array) const;

    void xWrapSeqEntries(TSeqEntryArray& seq_entry_array, 
                         const TSeqPos& bundle_size,
                         TSeqEntryArray& wrapped_entry_array) const;

    void xFlattenSeqEntrys(CSeq_submit::TData::TEntrys& entries,
                           TSeqEntryArray& seq_entry_array) const;

    void xFlattenSeqEntry(CSeq_entry& seq_entry, 
                          const CSeq_descr& seq_descr,
                          TSeqEntryArray& seq_entry_array,
                          bool process_set_of_any_type = false) const;

    void xMergeSeqDescr(const CSeq_descr& src, CSeq_descr& dst) const; 


    string xGetFileExtension(const string& filename) const;
};


void CSeqSubSplitter::Init()
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
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
    
    // treat input as Seq-entry
    {
        arg_desc->AddFlag("e", "Treat input as Seq-entry");
    }

    SetupArgDescriptions(arg_desc.release());
}


int CSeqSubSplitter::Run()
{
    const CArgs& args = GetArgs();
    if (args["e"].AsBoolean() && args["w"].AsBoolean()) {
        string err_msg = "Incompatible parameters: do not use -w when -e is used";
        ERR_POST(err_msg);
        return 1;
    }

    CRef<CSerialObject> input_obj;
    if (!xTryReadInputFile(input_obj)) {
        string err_msg = "Could not read input file";
        ERR_POST(err_msg);
        return 1;
    }


    bool input_as_seq_entry = args["e"].AsBoolean();
    list<CRef<CSerialObject>> output_array;

    bool good = input_as_seq_entry ? xTryProcessSeqEntry(input_obj, output_array) : xTryProcessSeqSubmit(input_obj, output_array);

    if (!good) {
        string err_msg = "Could not process input file";
        ERR_POST(err_msg);
        return 1;
    }

    const string output_stub = args["o"].AsString();


    // Output files should have the same extension as the input file
    string output_extension = "";
    if (args["i"]) {
        output_extension = xGetFileExtension(args["i"].AsString());
    }

    int output_index = 0;
    unique_ptr<CObjectOStream> ostr;
    bool binary = args["s"].AsBoolean();

    const TSeqPos pad_width = static_cast<TSeqPos>(log10(output_array.size())) + 1;

    try {
        for (auto& it: output_array) {
            ++output_index;
            ostr.reset(xInitOutputStream(output_stub, 
                        output_index, 
                        pad_width, 
                        output_extension,
                        binary
                        ));
            *ostr << *it;
        }
    }
    catch (const CException& e) {
        string err_msg = "Error while output results. ";
        err_msg += e.what();
        ERR_POST(err_msg);
        return 1;
    }

    return 0;
}


string CSeqSubSplitter::xGetFileExtension(const string& filename) const
{
   string extension = "";
   vector<string> arr;
   NStr::Split(filename,".",arr);
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

CRef<CSerialObject> CSeqSubSplitter::xGetInputObject() const
{
    CRef<CSerialObject> ret;
    if (GetArgs()["e"].AsBoolean()) {
        ret.Reset(new CSeq_entry);
    }
    else {
        ret.Reset(new CSeq_submit);
    }

    return ret;
}

bool CSeqSubSplitter::xTryReadInputFile(CRef<CSerialObject>& obj) const
{
    unique_ptr<CObjectIStream> istr;
    istr.reset(xInitInputStream());
    
    CRef<CSerialObject> input_obj = xGetInputObject();

    bool ret = true;
    try {
        istr->Read( { input_obj, input_obj->GetThisTypeInfo() } );
        obj = input_obj;
    }
    catch (CException&) {
        ret = false;
    }

    return ret;
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



void CSeqSubSplitter::xWrapSeqEntries(TSeqEntryArray& seq_entry_array, 
                                      const TSeqPos& bundle_size,
                                      TSeqEntryArray& wrapped_entry_array) const
{
    TSeqEntryArray::iterator seq_entry_it = seq_entry_array.begin();
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


bool CSeqSubSplitter::xTryProcessSeqEntries(const CObjectHelper& helper, TSeqEntryArray& seq_entry_array,
                                           list<CRef<CSerialObject>>& output_array) const
{
    const CArgs& args = GetArgs();

    TSeqPos bundle_size = args["n"].AsInteger();
    TSeqPos sort_order = args["r"].AsInteger();
    bool wrap_entries = args["w"].AsBoolean(); // Wrap the output Seq-entries 
                                               // within a Seq-submit in a Genbank set

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
        TSeqEntryArray wrapped_entry_array;
        xWrapSeqEntries(seq_entry_array, bundle_size, wrapped_entry_array);
        for(size_t i=0; i<wrapped_entry_array.size(); ++i) {
            CRef<CSerialObject> seqsub = helper.BuildObject();
            helper.GetListOfEntries(*seqsub).push_back(wrapped_entry_array[i]);
            output_array.push_back(seqsub);
        }
    } else {
        TSeqEntryArray::iterator seq_entry_it = seq_entry_array.begin();
        while (seq_entry_it != seq_entry_array.end()) {

            CRef<CSerialObject> seqsub = helper.BuildObject();
            for(TSeqPos i=0; i<bundle_size; ++i) {
                helper.GetListOfEntries(*seqsub).push_back(*seq_entry_it);
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


class CSeqSubmitHelper : public CObjectHelper
{
public:
    CSeqSubmitHelper(CSeq_submit& seq_submit) :
        m_seq_submit(seq_submit)
    {}

    virtual CRef<CSerialObject> BuildObject() const
    {
        CRef<CSeq_submit> seqsub = Ref(new CSeq_submit());
        seqsub->SetSub(m_seq_submit.SetSub());

        return seqsub;
    }

    virtual TSeqEntryList& GetListOfEntries(CSerialObject& obj) const
    {
        CSeq_submit& sub = dynamic_cast<CSeq_submit&>(obj);
        return sub.SetData().SetEntrys();
    }

private:
    CSeq_submit& m_seq_submit;
};


bool CSeqSubSplitter::xTryProcessSeqSubmit(CRef<CSerialObject>& obj, list<CRef<CSerialObject>>& output_array) const
{
    CSeq_submit* input_sub = dynamic_cast<CSeq_submit*>(obj.GetPointer());
    if (input_sub == nullptr || !input_sub->IsEntrys()) {
        ERR_POST("Seq-submit does not contain any entries");
        return false;
    }

    TSeqEntryArray seq_entry_array;

    xFlattenSeqEntrys(input_sub->SetData().SetEntrys(), seq_entry_array);
    return xTryProcessSeqEntries(CSeqSubmitHelper(*input_sub), seq_entry_array, output_array);
}


class CSeqEntryHelper : public CObjectHelper
{
public:
    CSeqEntryHelper(CSeq_entry& seq_entry) :
        m_seq_entry(seq_entry)
    {}

    virtual CRef<CSerialObject> BuildObject() const
    {
        CRef<CSeq_entry> seqentry = Ref(new CSeq_entry());
        seqentry->SetSet().SetClass(m_seq_entry.GetSet().GetClass());

        if (m_seq_entry.IsSetDescr()) {
            seqentry->SetDescr().Assign(m_seq_entry.GetDescr());
        }
        return seqentry;
    }

    virtual TSeqEntryList& GetListOfEntries(CSerialObject& obj) const
    {
        CSeq_entry& entry = dynamic_cast<CSeq_entry&>(obj);
        return entry.SetSet().SetSeq_set();
    }

private:
    CSeq_entry& m_seq_entry;
};


bool CSeqSubSplitter::xTryProcessSeqEntry(CRef<CSerialObject>& obj, list<CRef<CSerialObject>>& output_array) const
{
    CSeq_entry* input_entry = dynamic_cast<CSeq_entry*>(obj.GetPointer());
    if (input_entry == nullptr || !input_entry->IsSet()) {
        ERR_POST("Seq-entry does not contain any entries");
        return false;
    }

    TSeqEntryArray seq_entry_array;
    CSeq_descr seq_descr;

    CRef<CSeq_descr> upper_level_descr;
    if (input_entry->IsSetDescr()) {
        upper_level_descr.Reset(new CSeq_descr);
        upper_level_descr->Assign(input_entry->GetDescr());
        input_entry->SetDescr().Reset();
    }

    xFlattenSeqEntry(*input_entry, seq_descr, seq_entry_array, true);
    if (upper_level_descr.NotEmpty()) {
        input_entry->SetDescr().Assign(*upper_level_descr);
        upper_level_descr.Reset();
    }

    return xTryProcessSeqEntries(CSeqEntryHelper(*input_entry), seq_entry_array, output_array);
}


CObjectIStream* CSeqSubSplitter::xInitInputStream() const
{
    const CArgs& args = GetArgs();
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

static bool MultipleAllowed(CSeqdesc::E_Choice choice)
{
    static constexpr CSeqdesc::E_Choice MULTIPLE_ALLOWED[] = {
        CSeqdesc::e_Name,
        CSeqdesc::e_Comment,
        CSeqdesc::e_Num,
        CSeqdesc::e_Maploc,
        CSeqdesc::e_Pub,
        CSeqdesc::e_Region,
        CSeqdesc::e_User,
        CSeqdesc::e_Dbxref,
        CSeqdesc::e_Het,
        CSeqdesc::e_Modelev
    };

    return find(begin(MULTIPLE_ALLOWED), end(MULTIPLE_ALLOWED), choice) != end(MULTIPLE_ALLOWED);
}

static bool NeedToInclude(const CSeqdesc& descr, const CSeq_descr& dst)
{
    bool ret = true;

    if (dst.IsSet()) {

        CSeqdesc::E_Choice choice = descr.Which();

        for (auto& dst_descr: dst.Get()) {
            if (dst_descr->Which() == choice) {

                if (dst_descr->Equals(descr)) {
                    ret = false;
                    break;
                }
            }
        }
    }
    return ret;
}

void CSeqSubSplitter::xMergeSeqDescr(const CSeq_descr& src, CSeq_descr& dst) const
{
    if (!src.IsSet()) {
        return;
    }

    for (auto& descr: src.Get()) 
    {
        CSeqdesc::E_Choice choice = descr->Which();
        if (MultipleAllowed(choice)) {

            if (NeedToInclude(*descr, dst)) {
                dst.Set().push_back(descr);
            }
        }
        else {

            if (find_if(dst.Set().begin(), dst.Set().end(), [choice](const CRef<CSeqdesc>& cur_descr) { return cur_descr->Which() == choice; } ) == dst.Set().end()) {
                dst.Set().push_back(descr);
            }
        }
    }
}


void CSeqSubSplitter::xFlattenSeqEntrys(CSeq_submit::TData::TEntrys& entries,
                                        TSeqEntryArray& seq_entry_array) const 
{
    NON_CONST_ITERATE(CSeq_submit::TData::TEntrys, it, entries) {
        CSeq_entry& seq_entry = **it;
        CSeq_descr seq_descr;
        xFlattenSeqEntry(seq_entry, seq_descr, seq_entry_array);     
    }
}

static bool NeedToProcess(const CSeq_entry& entry, bool set_of_any_type_allowed)
{
    if (entry.IsSeq()) {
        return false;
    }

    bool set_of_allowed_type = set_of_any_type_allowed;
    if (!set_of_any_type_allowed) {
        set_of_allowed_type = entry.GetSet().IsSetClass() &&
            (entry.GetSet().GetClass() == CBioseq_set::eClass_genbank || entry.GetSet().GetClass() == CBioseq_set::eClass_pub_set);
    }

    return set_of_allowed_type;
}

void CSeqSubSplitter::xFlattenSeqEntry(CSeq_entry& entry,
                                       const CSeq_descr& seq_descr,
                                       TSeqEntryArray& seq_entry_array,
                                       bool process_set_of_any_type) const
{
    if (NeedToProcess(entry, process_set_of_any_type)) {

        // Class has to be either genbank or pub_set,
        // or it can be a top level Seq-entry containing set of any type.
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
    else {
        CRef<CSeq_entry> new_entry = Ref(&entry);

        if (seq_descr.IsSet()) {
            CSeq_descr&  entry_descr = (entry.IsSeq())
                ? entry.SetSeq().SetDescr()
                : entry.SetSet().SetDescr();


            xMergeSeqDescr(seq_descr, entry_descr);
        }

        seq_entry_array.push_back(new_entry);
    }
}

} // namespace

END_NCBI_SCOPE


USING_NCBI_SCOPE;

int main(int argc, const char** argv)
{
    return seqsubmit_split::CSeqSubSplitter().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}

