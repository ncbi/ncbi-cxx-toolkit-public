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
* Author: Eugene Vasilchenko
*
* File Description:
*   asn2asn test program
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbimempool.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <memory>

BEGIN_NCBI_SCOPE

// CSEQ_ENTRY_REF_CHOICE macro to switch implementation of CSeq_entry choice
// as choice class or virtual base class.
// 0 -> generated choice class
// 1 -> virtual base class
#define CSEQ_ENTRY_REF_CHOICE 0

#if CSEQ_ENTRY_REF_CHOICE

template<typename T> const CTypeInfo* (*GetTypeRef(const T* object))(void);
template<typename T> pair<void*, const CTypeInfo*> ObjectInfo(T& object);
template<typename T> pair<const void*, const CTypeInfo*> ConstObjectInfo(const T& object);

EMPTY_TEMPLATE
inline
const CTypeInfo* (*GetTypeRef< CRef<NCBI_NS_NCBI::objects::CSeq_entry> >(const CRef<NCBI_NS_NCBI::objects::CSeq_entry>* object))(void)
{
    return &NCBI_NS_NCBI::objects::CSeq_entry::GetRefChoiceTypeInfo;
}
EMPTY_TEMPLATE
inline
pair<void*, const CTypeInfo*> ObjectInfo< CRef<NCBI_NS_NCBI::objects::CSeq_entry> >(CRef<NCBI_NS_NCBI::objects::CSeq_entry>& object)
{
    return make_pair((void*)&object, GetTypeRef(&object)());
}
EMPTY_TEMPLATE
inline
pair<const void*, const CTypeInfo*> ConstObjectInfo< CRef<NCBI_NS_NCBI::objects::CSeq_entry> >(const CRef<NCBI_NS_NCBI::objects::CSeq_entry>& object)
{
    return make_pair((const void*)&object, GetTypeRef(&object)());
}

#endif

END_NCBI_SCOPE

#include "asn2asn.hpp"
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiargs.hpp>
#include <objects/seq/Bioseq.hpp>
#include <serial/objectio.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objcopy.hpp>
#include <serial/serial.hpp>
#include <serial/objhook.hpp>
#include <serial/iterator.hpp>

// The headers for PubSeqOS access.
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/driver_mgr.hpp>
#include <dbapi/driver/drivers.hpp>
#include <corelib/rwstream.hpp>
#include <util/compress/zlib.hpp>

USING_NCBI_SCOPE;

using namespace NCBI_NS_NCBI::objects;

#if CSEQ_ENTRY_REF_CHOICE
typedef CRef<CSeq_entry> TSeqEntry;
#else
typedef CSeq_entry TSeqEntry;
#endif

int main(int argc, char** argv)
{
    return CAsn2Asn().AppMain(argc, argv, 0, eDS_Default, 0, "asn2asn");
}

static
void SeqEntryProcess(CSeq_entry& entry);  /* dummy function */

#if CSEQ_ENTRY_REF_CHOICE
static
void SeqEntryProcess(CRef<CSeq_entry>& entry)
{
    SeqEntryProcess(*entry);
}
#endif

class CCounter
{
public:
    CCounter(void)
        : m_Counter(0)
        {
        }
    ~CCounter(void)
        {
            _ASSERT(m_Counter == 0);
        }

    operator int(void) const
        {
            return m_Counter;
        }
private:
    friend class CInc;
    int m_Counter;
};

class CInc
{
public:
    CInc(CCounter& counter)
        : m_Counter(counter)
        {
            ++counter.m_Counter;
        }
    ~CInc(void)
        {
            --m_Counter.m_Counter;
        }
private:
    CCounter& m_Counter;
};

class CReadSeqSetHook : public CReadClassMemberHook
{
public:
    void ReadClassMember(CObjectIStream& in,
                         const CObjectInfo::CMemberIterator& member);

    CCounter m_Level;
};

class CWriteSeqSetHook : public CWriteClassMemberHook
{
public:
    void WriteClassMember(CObjectOStream& out,
                          const CConstObjectInfo::CMemberIterator& member);

    CCounter m_Level;
};

class CWriteSeqEntryHook : public CWriteObjectHook
{
public:
    void WriteObject(CObjectOStream& out, const CConstObjectInfo& object);

    CCounter m_Level;
};


class CFormatReadHook : public CReadObjectHook
{
public:
    CFormatReadHook()
        : m_Count(0)
        {
        }

    size_t GetCount() const { return m_Count; }

    virtual void ReadObject(CObjectIStream& in,
                            const CObjectInfo& info)
    {
        ++m_Count;
        if ( m_Count % 1000 == 0 )
            NcbiCout << "Bioseq "<<m_Count<<NcbiEndl;

        try {
            ///
            /// bioseq is in here
            ///
            CRef<CBioseq> bioseq(new CBioseq);
            info.GetTypeInfo()->DefaultReadData(in, bioseq);
            //in.ReadExternalObject(&*bioseq, CType<CBioseq>().GetTypeInfo());
            in.SetDiscardCurrObject();

            ///
            /// do some nifty formatting
            ///
                //Process(*bioseq);
        }
        catch (CException& e) {
            LOG_POST(Error << "error processing bioseq: " << e.what());
        }
    }

private:
    size_t m_Count;
};


class CSkipReadClassMemberHook : public CReadClassMemberHook
{
public:
    virtual void ReadClassMember(CObjectIStream& in,
                                 const CObjectInfo::CMemberIterator& member)
    {
        in.SkipObject(member.GetMemberType().GetTypeInfo());
    }
};


template<typename Member>
class CReadInSkipClassMemberHook : public CSkipClassMemberHook
{
public:
    typedef Member TObject;
    CReadInSkipClassMemberHook() { object = TObject(); }
    
    void SkipClassMember(CObjectIStream& in, const CObjectTypeInfoMI& member)
        {
            _ASSERT((*member).GetTypeInfo()->GetSize() == sizeof(object));
            CObjectInfo info(&object, (*member).GetTypeInfo());
            in.ReadObject(info);
            NcbiCout << "Skipped class member: " <<
                member.GetClassType().GetTypeInfo()->GetName() << '.' <<
                member.GetMemberInfo()->GetId().GetName() << ": " <<
                MSerial_AsnText << info << NcbiEndl;
        }
    
private:
    TObject object;
};

template<typename Object>
class CReadInSkipObjectHook : public CSkipObjectHook
{
public:
    typedef Object TObject;

    CReadInSkipObjectHook() : object() {}

    int m_Dummy;

    void SkipObject(CObjectIStream& in, const CObjectTypeInfo& type)
        {
            _ASSERT(type.GetTypeInfo()->GetSize() == sizeof(object));
            CObjectInfo info(&object, type.GetTypeInfo());
            in.ReadObject(info);

            const CSeq_descr& descr = object;
            string title;
            string genome;
            int subtype = 0;
            string taxname;
            string taxid;
            int biomol = 0;
            int complete = 0;
            int tech = 0;

            NcbiCout << "Skipped object: " <<
                type.GetTypeInfo()->GetName() << ": " <<
                MSerial_AsnText << info << NcbiEndl;
            ITERATE(list< CRef< CSeqdesc > >,it,descr.Get()) {
                CConstRef<CSeqdesc> desc = *it;
                if (desc->IsTitle()) {
                    title = desc->GetTitle();
                    string lt = NStr::ToLower(title); 
                }
                else if (desc->IsSource()) { 
                    const CBioSource & source = desc->GetSource();
                    if (source.IsSetGenome())
                        genome = source.GetGenome();					
                    if (source.IsSetSubtype()) {
                        ITERATE(list< CRef< CSubSource > >,it,source.GetSubtype()) {
                            string name = (*it)->GetName();
                            int st = (*it)->GetSubtype();
                            if (st == 19) subtype = st;
                        }
                    }
                    if (source.IsSetOrg()) {
                        const COrg_ref & orgref= source.GetOrg();
                        taxname = orgref.GetTaxname();
                        ITERATE(vector< CRef< CDbtag > > ,it,orgref.GetDb()) {
                            if (CDbtag::eDbtagType_taxon == (*it)->GetType()) {
                                taxid = (*it)->GetTag().GetId();
                                break;
                            }
                        }
                    }
                }
                else if (desc->IsMolinfo()) {
                    const CMolInfo & molinfo = desc->GetMolinfo();
                    if  (molinfo.IsSetBiomol())
                        biomol = molinfo.GetBiomol();
                    if (molinfo.IsSetCompleteness())
                        complete = molinfo.GetCompleteness();
                    if (molinfo.IsSetTech())
                        tech = molinfo.GetTech();
                }
            }
            m_Dummy += subtype+biomol+complete+tech;
        }

private:
    TObject object;
};


/*****************************************************************************
*
*   Main program loop to read, process, write SeqEntrys
*
*****************************************************************************/
void CAsn2Asn::Init(void)
{
    auto_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->SetUsageContext("asn2asn", "convert Seq-entry or Bioseq-set data");

    d->AddKey("i", "inputFile",
              "input data file",
              CArgDescriptions::eInputFile);
    d->AddOptionalKey("o", "outputFile",
                      "output data file",
                      CArgDescriptions::eOutputFile);
    d->AddFlag("e",
               "treat data as Seq-entry");
    d->AddFlag("sub",
               "treat data as Seq-submit");
    d->SetDependency("e", CArgDescriptions::eExcludes, "sub");
    d->AddFlag("b",
               "binary ASN.1 input format");
    d->AddFlag("X",
               "XML input format");
    d->AddFlag("s",
               "binary ASN.1 output format");
    d->AddFlag("x",
               "XML output format");
    d->AddFlag("C",
               "Convert data without reading in memory");
    d->AddFlag("S",
               "Skip data without reading in memory");
    d->AddOptionalKey("Mgi", "MergeAnnotGi",
                      "Merge Seq-annot to GI",
                      CArgDescriptions::eInteger);
    d->AddOptionalKey("Min", "MergeAnnotInput",
                      "Binary ASN.1 Seq-entry file with Seq-annot for merging",
                      CArgDescriptions::eInputFile);
    d->AddOptionalKey("Mext", "MergeAnnotExternal",
                      "Merge external annotation from PubSeqOS",
                      CArgDescriptions::eInteger);
    d->AddFlag("P",
               "Use memory pool for deserialization");
    d->AddOptionalKey("l", "logFile",
                      "log errors to <logFile>",
                      CArgDescriptions::eOutputFile);
    d->AddDefaultKey("c", "count",
                      "perform command <count> times",
                      CArgDescriptions::eInteger, "1");
    d->AddDefaultKey("tc", "threadCount",
                      "perform command in <threadCount> thread",
                      CArgDescriptions::eInteger, "1");
    d->AddFlag("m",
               "Input file contains multiple objects");
    
    d->AddFlag("ih",
               "Use read hooks");
    d->AddFlag("oh",
               "Use write hooks");

    d->AddFlag("q",
               "Quiet execution");

    SetupArgDescriptions(d.release());
}

class CAsn2AsnThread : public CThread
{
public:
    CAsn2AsnThread(int index, CAsn2Asn* asn2asn)
        : m_Index(index), m_Asn2Asn(asn2asn), m_DoneOk(false)
    {
    }

    void* Main()
    {
        string suffix = '.'+NStr::IntToString(m_Index);
        try {
            m_Asn2Asn->RunAsn2Asn(suffix);
        }
        catch (exception& e) {
            CNcbiDiag() << Error << "[asn2asn thread " << m_Index << "]" <<
                "Exception: " << e.what();
            return 0;
        }
        m_DoneOk = true;
        return 0;
    }

    bool DoneOk() const
    {
        return m_DoneOk;
    }

private:
    int m_Index;
    CAsn2Asn* m_Asn2Asn;
    bool m_DoneOk;
};

int CAsn2Asn::Run(void)
{
    SetDiagPostLevel(eDiag_Error);

    const CArgs& args = GetArgs();

    if ( const CArgValue& l = args["l"] )
        SetDiagStream(&l.AsOutputFile());


    int threadCount = args["tc"].AsInteger();
    vector< CRef<CAsn2AsnThread> > threads(threadCount);
    for ( int i = 1; i < threadCount; ++i ) {
        threads[i] = new CAsn2AsnThread(i, this);
        threads[i]->Run();
    }
    
    try {
        RunAsn2Asn("");
    }
    catch (exception& e) {
        CNcbiDiag() << Error << "[asn2asn]" << "Exception: " << e.what();
        return 1;
    }

    for ( int i = 1; i < threadCount; ++i ) {
        threads[i]->Join();
        if ( !threads[i]->DoneOk() ) {
            NcbiCerr << "Error in thread: " << i << NcbiEndl;
            return 1;
        }
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
// Seq-annot merging code
/////////////////////////////////////////////////////////////////////////////

// All the code is enclosed into namespace merge_annot to avoid conflicts.
BEGIN_SCOPE(merge_annot)

// This class contains all necessary information for various hooks.
class CInsertAnnotManager
{
public:
    CInsertAnnotManager(TGi target_gi, CConstRef<CSeq_annot> annot)
        : target_gi(target_gi), do_insert(false), annot(annot), annot_in(0) {
    }
    CInsertAnnotManager(TGi target_gi, CObjectIStream& annot_in)
        : target_gi(target_gi), do_insert(false), annot_in(&annot_in) {
    }

    // Returns true if there is any annot to insert.
    bool HasAnnot(void) const {
        return annot || annot_in;
    }

    // Returns true if current sequence is to be copied without modification.
    bool SkipInsert(void) const {
        return !do_insert;
    }

    // Marks current annot as inserted.
    void InsertDone(void) {
        annot = null;
        annot_in = 0;
    }

    // Notifies manager about Seq-id of the current sequence.
    void AddId(const CSeq_id& id) {
        if ( id.IsGi() && id.GetGi() == target_gi && HasAnnot() ) {
            do_insert = true;
        }
    }

    TGi target_gi; // GI of the sequence to insert annots in.
    bool do_insert; // Flag is true if currently sequence is the target.
    CConstRef<CSeq_annot> annot; // Explicit annot object to insert.
    CObjectIStream* annot_in; // Input object stream source of annot to insert.
};

// This hook is used to determine if current sequence is the target.
// It should be set on Bioseq.id field which is SEQUENCE OF Seq-id.
class CInsertAnnotHookId : public CCopyClassMemberHook
{
public:
    CInsertAnnotManager& manager;
    CInsertAnnotHookId(CInsertAnnotManager& manager)
        : manager(manager) {
    }
    void CopyClassMember(CObjectStreamCopier& copier,
                         const CObjectTypeInfoMI& member) {
        // When the Bioseq.id field is found we copy Seq-ids one by one
        // and notify the manager about them.

        // Temporary Seq-id object.
        CSeq_id id;

        // Open object output iterator.
        COStreamContainer out(copier.Out(), member);

        // Scan by object input iterator.
        for ( CIStreamContainerIterator in(copier.In(), member); in; ++in ) {
            in >> id; // read next Seq-id
            manager.AddId(id); // register it
            out << id; // write it into output
        }
    }
};

// This hook is used to extract Seq-annot from the external annot input stream
// and copy it into main output.
// It should be set on Seq-annot object.
// The external annot input stream is 'skipped', and when a Seq-annot type
// is detected we copy it into main output instead of skipping.
class CInsertAnnotHookCopy : public CSkipObjectHook
{
public:
    CObjectStreamCopier& copier; // Object copier to use.
    COStreamContainer& out; // Output stream object iterator to use.
    CInsertAnnotHookCopy(CObjectStreamCopier& copier,
                         COStreamContainer& out)
        : copier(copier), out(out) {
    }

    void SkipObject(CObjectIStream& in,
                    const CObjectTypeInfo& type) {
        // Just copy the Seq-annot.
        out.WriteElement(copier, in);
    }
};

// This hook is used for main merging operation.
// It should be set on Bioseq.annot field which is SEQUENCE OF Seq-annot.
// It will first copy all existing Seq-annots if any,
// and then insert Seq-annots from external source.
class CInsertAnnotHookAnnot : public CCopyClassMemberHook
{
public:
    CInsertAnnotManager& manager;
    CInsertAnnotHookAnnot(CInsertAnnotManager& manager)
        : manager(manager) {
    }
    // Inserts Seq-annot from external source.
    void InsertAnnot(CObjectStreamCopier& copier,
                     COStreamContainer& out) {
        if ( manager.annot ) {
            // If there is an explicit Seq-annot object, just write it.
            out << *manager.annot;
        }
        else {
            // Otherwise, prepare the copier, setup necessary hook
            // for insertion, and the invoke 'skip' operation on
            // external object source.
            // The hook will itercept Seq-annot object in the skipped data
            // and copy it into the main output instead of skipping.
            CObjectIStream& in = *manager.annot_in;
            CObjectStreamCopier copier2(in, copier.Out());
            CObjectHookGuard<CSeq_annot> guard
                (*new CInsertAnnotHookCopy(copier2, out), &in);
            in.Skip(CType<CSeq_entry>());
        }
        // Notify the manager that the Seq-annot was inserted
        // to avoid duplication.
        manager.InsertDone();
    }

    // This hook method is called by the serialization library when the field
    // is copied.
    // We'll copy all existing annots, and append extra from external source.
    void CopyClassMember(CObjectStreamCopier& copier,
                         const CObjectTypeInfoMI& member) {
        if ( manager.SkipInsert() ) {
            // If no work to be done, just invoke default method.
            DefaultCopy(copier, member);
            return;
        }

        // Open output object iterator.
        COStreamContainer out(copier.Out(), member);
        // Scan input object iterator with copying.
        for ( CIStreamContainerIterator in(copier.In(), member); in; ++in ) {
            out.WriteElement(copier, in);
        }
        // Insert external annots.
        InsertAnnot(copier, out);
    }

    // This hook method is called by the serialization library when the field
    // is missing in input stream.
    // We'll create the field in the output stream and copy external annots.
    void CopyMissingClassMember(CObjectStreamCopier& copier,
                                const CObjectTypeInfoMI& member) {
        if ( manager.SkipInsert() ) {
            // If no work to be done, just do nothing.
            return;
        }

        // Open output object iterator.
        COStreamContainer out(copier.Out(), member);
        // Insert external annots.
        InsertAnnot(copier, out);
    }
};

// Main API function for merging.
// CObjectIStream& in - object stream of main input with Seq-entry.
// CObjectOStream& out - object stream of main output for modified Seq-entry.
// int gi - GI of target sequence.
// CObjectIStream& annot_in - external object stream input with Seq-entry
void MergeAnnot(CObjectIStream& in, CObjectOStream& out, TGi gi,
                CObjectIStream& annot_in)
{
    // Merge manager.
    CInsertAnnotManager manager(gi, annot_in);
    // Main object copier.
    CObjectStreamCopier copier(in, out);
    // Setup hook on Bioseq.id to detect target sequence.
    CObjectHookGuard<CBioseq> guard1("id",
                                     *new CInsertAnnotHookId(manager),
                                     &copier);
    // Setup hook on Bioseq.annot for Seq-annot insertion.
    CObjectHookGuard<CBioseq> guard2("annot",
                                     *new CInsertAnnotHookAnnot(manager),
                                     &copier);
    // These options will speed up strings processing a bit.
    // The methods aren't declared in CObjectIStream, so we have to cast.
    if ( CObjectIStreamAsnBinary* bin_in =
         dynamic_cast<CObjectIStreamAsnBinary*>(&annot_in) ) {
        bin_in->FixNonPrint(eFNP_Allow);
    }
    if ( CObjectIStreamAsnBinary* bin_in =
         dynamic_cast<CObjectIStreamAsnBinary*>(&copier.In()) ) {
        bin_in->FixNonPrint(eFNP_Allow);
    }
    if ( CObjectOStreamAsnBinary* bin_out =
         dynamic_cast<CObjectOStreamAsnBinary*>(&copier.Out()) ) {
        bin_out->FixNonPrint(eFNP_Allow);
    }

    // Do the copy.
    copier.Copy(CType<CSeq_entry>());
}

// Merging function with external annot source from file.
// CObjectIStream& in - object stream of main input with Seq-entry.
// CObjectOStream& out - object stream of main output for modified Seq-entry.
// int gi - GI of target sequence.
// const string& in_file - the name of file with ANS.1 binary Seq-entry.
void MergeFromFile(CObjectIStream& in, CObjectOStream& out, TGi gi,
                   const string& in_file)
{
    auto_ptr<CObjectIStream> annot_in(CObjectIStream::Open(eSerial_AsnBinary,
                                                           in_file));
    MergeAnnot(in, out, gi, *annot_in);
}

BEGIN_SCOPE(PubSeqOS)

// Utility class for reading data from CDB_Result with blob data from PubSeqOS.
class CDB_Result_Reader : public IReader
{
public:
    CDB_Result_Reader(CDB_Result* db_result)
        : m_DB_Result(db_result) {
    }
    
    ERW_Result Read(void*   buf,
                    size_t  count,
                    size_t* bytes_read) {
        if ( !count ) {
            if ( bytes_read ) {
                *bytes_read = 0;
            }
            return eRW_Success;
        }
        size_t ret;
        while ( (ret = m_DB_Result->ReadItem(buf, count)) == 0 ) {
            if ( !m_DB_Result->Fetch() )
                break;
        }
        if ( bytes_read ) {
            *bytes_read = ret;
        }
        return ret? eRW_Success: eRW_Eof;
    }
    ERW_Result PendingCount(size_t* /*count*/) {
        return eRW_NotImplemented;
    }

private:
    CDB_Result* m_DB_Result;
};

END_SCOPE(PubSeqOS)

// Merging function with external annot to be read from PubSeqOS.
// CObjectIStream& in - object stream of main input with Seq-entry.
// CObjectOStream& out - object stream of main output for modified Seq-entry.
// int gi - GI of target sequence.
// int add_ext_feat - bit mask of external annots to be read from PubSeqOS.
void MergeExternal(CObjectIStream& in, CObjectOStream& out, TGi gi,
                   int add_ext_feat)
{
    DBAPI_RegisterDriver_FTDS();

    // Prepare PubSeqOS connection.
    C_DriverMgr drvMgr;
    map<string,string> args;
    args["packet"]="3584"; // 7*512
    args["version"]="125"; // for correct connection to OpenServer
    string err;
    I_DriverContext* context = drvMgr.GetDriverContext("ftds", &err, &args);

    AutoPtr<CDB_Connection> conn
        (context->Connect("PUBSEQ_OS_PUBLIC", "anyone", "allowed", 0));
    if ( 1 ) {
        // I'm not sure what this option does...
        AutoPtr<CDB_LangCmd> cmd(conn->LangCmd("set blob_stream on"));
        cmd->Send();
    }
    if ( 1 ) {
        // Allow the PubSeqOS server to send gzipped data.
        AutoPtr<CDB_LangCmd> cmd(conn->LangCmd("set accept gzip"));
        cmd->Send();
    }

    // Request the external annotations blob.
    AutoPtr<CDB_RPCCmd> cmd(conn->RPC("id_get_asn"));
    // external annotations sat is 26 (all except CDD).
    int sat = add_ext_feat == 8? 10: 26;
    CDB_SmallInt satIn(sat); 
    CDB_Int satKeyIn(GI_TO(int, gi)); // sat_key is equal to GI.
    CDB_Int ext_feat(add_ext_feat); // ext_feat mask.

    cmd->SetParam("@sat_key", &satKeyIn);
    cmd->SetParam("@sat", &satIn);
    cmd->SetParam("@ext_feat", &ext_feat);
    cmd->Send();

    AutoPtr<CDB_Result> result; // The result with external annotations blob.
    int zip_type = 0; // Compression of the blob.

    // Wait for result with external annotations blob.
    while( !result && cmd->HasMoreResults() ) {
        if ( cmd->HasFailed() ) {
            break;
        }
        
        AutoPtr<CDB_Result> dbr(cmd->Result());
        if ( !dbr.get() || dbr->ResultType() != eDB_RowResult ) {
            continue;
        }
        
        while ( !result && dbr->Fetch() ) {
            for ( unsigned pos = 0; pos < dbr->NofItems(); ++pos ) {
                const string& name = dbr->ItemName(pos);
                if ( name == "zip_type" ) {
                    // zip_type column contains compression type.
                    CDB_Int v;
                    dbr->GetItem(&v);
                    zip_type = v.Value();
                }
                else if ( name == "asn1" ) {
                    // asn1 column contains the blob data.
                    result = dbr;
                    break;
                }
                else {
                    // We are not interested in all other columns.
                    dbr->SkipItem();
                }
            }
        }
    }

    // Here the error check should be done if there is no result...
    if ( !result.get() ) {
        ERR_POST("No external annot found.");
        // Main object copier.
        CObjectStreamCopier copier(in, out);
        // Do the copy.
        copier.Copy(CType<CSeq_entry>());
        return;
    }

    // Setup CByteSourceReader for reading data from CDB_Result.
    PubSeqOS::CDB_Result_Reader reader(result.get());
    // Setup input stream.
    CRStream stream(&reader);

    if ( zip_type & 2 ) { // gzip
        // If there is a compression we add an extra piped stream
        // for uncompressed data.
        CZipStreamDecompressor decompressor;
        CCompressionIStream unzip(stream, &decompressor);
        // Open object stream for external annotations.
        auto_ptr<CObjectIStream> annot_in
            (CObjectIStream::Open(eSerial_AsnBinary, unzip));
        // Do the merging.
        MergeAnnot(in, out, gi, *annot_in);
    }
    else {
        // Open object stream for external annotations.
        auto_ptr<CObjectIStream> annot_in
            (CObjectIStream::Open(eSerial_AsnBinary, stream));
        // Do the merging.
        MergeAnnot(in, out, gi, *annot_in);
    }
}

END_SCOPE(merge_annot)

/////////////////////////////////////////////////////////////////////////////
// End of Seq-annot merging code
/////////////////////////////////////////////////////////////////////////////


DEFINE_STATIC_FAST_MUTEX(s_ArgsMutex);

void CAsn2Asn::RunAsn2Asn(const string& outFileSuffix)
{
    CFastMutexGuard GUARD(s_ArgsMutex);

    const CArgs& args = GetArgs();

    string inFile = args["i"].AsString();
    ESerialDataFormat inFormat = eSerial_AsnText;
    if ( args["b"] )
        inFormat = eSerial_AsnBinary;
    else if ( args["X"] )
        inFormat = eSerial_Xml;

    const CArgValue& o = args["o"];
    bool haveOutput = o;
    string outFile;
    ESerialDataFormat outFormat = eSerial_AsnText;
    if ( haveOutput ) {
        outFile = o.AsString();
        if ( args["s"] )
            outFormat = eSerial_AsnBinary;
        else if ( args["x"] )
            outFormat = eSerial_Xml;
    }
    outFile += outFileSuffix;

    enum EDataType {
        eDataType_BioseqSet, // the default
        eDataType_SeqEntry,
        eDataType_SeqSubmit
    };
    EDataType eDataType = eDataType_BioseqSet;
    if( args["e"] ) {
        eDataType = eDataType_SeqEntry;
    } else if( args["sub"] ) {
        eDataType = eDataType_SeqSubmit;
    }
    bool skip = args["S"];
    bool convert = args["C"];
    bool merge = args["Mgi"] && (args["Min"] || args["Mext"]);
    if ( args["Mext"] ) {
        int ext = args["Mext"].AsInteger();
        if ( ext == 0 || (ext & (ext-1)) != 0 ) {
            ERR_FATAL("Only single external annotation bit is allowed.");
        }
    }
    bool readHook = args["ih"];
    bool writeHook = args["oh"];
    bool usePool = args["P"];

    bool quiet = args["q"];
    bool multi = args["m"];

    size_t count = args["c"].AsInteger();

    GUARD.Release();
    
    for ( size_t i = 1; i <= count; ++i ) {
        bool displayMessages = count != 1 && !quiet;
        if ( displayMessages )
            NcbiCerr << "Step " << i << ':' << NcbiEndl;
        auto_ptr<CObjectIStream> in(CObjectIStream::Open(inFormat, inFile,
                                                         eSerial_StdWhenAny));
        if ( usePool ) {
            in->UseMemoryPool();
        }
        auto_ptr<CObjectOStream> out(!haveOutput? 0:
                                     CObjectOStream::Open(outFormat, outFile,
                                                          eSerial_StdWhenAny));

        for ( ;; ) {
            /* read one Seq-entry or Seq-submit */
            if ( eDataType == eDataType_SeqEntry ||
                 eDataType == eDataType_SeqSubmit ) 
            {
                const CObjectTypeInfo objectTypeInfo = (
                    eDataType == eDataType_SeqEntry ?
                    CType<CSeq_entry>().operator ncbi::CObjectTypeInfo() :
                    CType<CSeq_submit>().operator ncbi::CObjectTypeInfo() );
                if ( skip ) {
                    if ( displayMessages )
                        NcbiCerr << "Skipping " << objectTypeInfo.GetName() << "..." << NcbiEndl;
                    if ( readHook ) {
                        {{
                            CObjectTypeInfo type = CType<CSeq_descr>();
                            type.SetLocalSkipHook
                                (*in, new CReadInSkipObjectHook<CSeq_descr>);
                        }}
                        {{
                            CObjectTypeInfo type = CType<CBioseq_set>();
                            type.FindMember("class").SetLocalSkipHook
                                (*in, new CReadInSkipClassMemberHook<CBioseq_set::TClass>);
                        }}
                        {{
                            CObjectTypeInfo type = CType<CSeq_inst>();
                            type.FindMember("topology").SetLocalSkipHook
                                (*in, new CReadInSkipClassMemberHook<CSeq_inst::TTopology>);
                        }}
                        in->Skip(objectTypeInfo);
                    }
                    else {
                        in->Skip(objectTypeInfo);
                    }
                }
                else if ( convert && haveOutput ) {
                    if ( displayMessages )
                        NcbiCerr << "Copying " << objectTypeInfo.GetName() << "..." << NcbiEndl;

                    CObjectStreamCopier copier(*in, *out);
                    copier.Copy(CType<CSeq_entry>());
                }
                else if ( eDataType == eDataType_SeqEntry &&
                    merge && haveOutput ) 
                {
                    if ( displayMessages )
                        NcbiCerr << "Merging Seq-annot..." << NcbiEndl;

                    if ( args["Min"] ) {
                        merge_annot::MergeFromFile(*in, *out,
                                                   GI_FROM(int, args["Mgi"].AsInteger()),
                                                   args["Min"].AsString());
                    }
                    else {
                        merge_annot::MergeExternal(*in, *out,
                                                   GI_FROM(int, args["Mgi"].AsInteger()),
                                                   args["Mext"].AsInteger());
                    }
                }
                else {
                    CRef<CSerialObject> pObjectFromIn;
                    //entry.DoNotDeleteThisObject();
                    if ( displayMessages )
                        NcbiCerr << "Reading " << objectTypeInfo.GetName() << "..." << NcbiEndl;

                    // read in the CSerialObject, then
                    // extract the Seq-entry inside there for processing
                    CRef<CSeq_entry> pInnerSeqEntry;
                    if( eDataType == eDataType_SeqEntry ) {
                        CRef<CSeq_entry> pEntry( new CSeq_entry );
                        *in >> *pEntry;
                        pObjectFromIn = pEntry;
                        // input *is* a seq-entry, so no digging necessary
                        pInnerSeqEntry = static_cast<CSeq_entry*>(pObjectFromIn.GetPointer());
                    } else {
                        CRef<CSeq_submit> pSeqSubmit( new CSeq_submit );
                        *in >> *pSeqSubmit;
                        pObjectFromIn = pSeqSubmit;
                        // get the one Seq-entry from the Seq-submit
                        if( pSeqSubmit->GetData().GetEntrys().size() == 1 ) {
                            pInnerSeqEntry = pSeqSubmit->GetData().GetEntrys().front();
                        }
                    }

                    /* do any processing */
                    if( pInnerSeqEntry ) {
                        SeqEntryProcess(*pInnerSeqEntry);
                    }

                    if ( haveOutput ) {
                        if ( displayMessages )
                            NcbiCerr << "Writing " << objectTypeInfo.GetName() << "..." << NcbiEndl;
                        *out << *pObjectFromIn;
                    }
                }
            }
            else {              /* read Seq-entry's from a Bioseq-set */
                if ( skip ) {
                    if ( displayMessages )
                        NcbiCerr << "Skipping Bioseq-set..." << NcbiEndl;
                    in->Skip(CType<CBioseq_set>());
                }
                else if ( convert && haveOutput ) {
                    if ( displayMessages )
                        NcbiCerr << "Copying Bioseq-set..." << NcbiEndl;
                    CObjectStreamCopier copier(*in, *out);
                    copier.Copy(CType<CBioseq_set>());
                }
                else {
                    CRef<CBioseq_set> entries(new CBioseq_set);
                    //entries.DoNotDeleteThisObject();
                    if ( displayMessages )
                        NcbiCerr << "Reading Bioseq-set..." << NcbiEndl;

                    if ( readHook ) {
                        CObjectTypeInfo bioseqSetType = CType<CBioseq_set>();
                        bioseqSetType.FindMember("seq-set")
                            .SetLocalReadHook(*in, new CReadSeqSetHook);
                        *in >> *entries;
                    }
                    else {
                        *in >> *entries;
                        
                        NON_CONST_ITERATE ( CBioseq_set::TSeq_set, seqi,
                                            entries->SetSeq_set() ) {
                            SeqEntryProcess(**seqi);    /* do any processing */
                        }
                    }
                    if ( haveOutput ) {
                        if ( displayMessages )
                            NcbiCerr << "Writing Bioseq-set..." << NcbiEndl;
                        if ( writeHook ) {
#if 0
                            CObjectTypeInfo bioseqSetType = CType<CBioseq_set>();
                            bioseqSetType.FindMember("seq-set")
                                .SetLocalWriteHook(*out, new CWriteSeqSetHook);
#else
                            CObjectTypeInfo seqEntryType = CType<CSeq_entry>();
                            seqEntryType
                                .SetLocalWriteHook(*out, new CWriteSeqEntryHook);
#endif
                            *out << *entries;
                        }
                        else {
                            *out << *entries;
                        }
                    }
                }
            }
            if ( !multi || in->EndOfData() )
                break;
        }
    }
}


/*****************************************************************************
*
*   void SeqEntryProcess (sep)
*      just a dummy routine that does nothing
*
*****************************************************************************/
static
void SeqEntryProcess(CSeq_entry& /* seqEntry */)
{
}

void CReadSeqSetHook::ReadClassMember(CObjectIStream& in,
                                      const CObjectInfo::CMemberIterator& member)
{
    CInc inc(m_Level);
    if ( m_Level == 1 ) {
        // (do not have to read member open/close tag, it's done by this time)

        // Read each element separately to a local TSeqEntry,
        // process it somehow, and... not store it in the container.
        for ( CIStreamContainerIterator i(in, member); i; ++i ) {
            TSeqEntry entry;
            //entry.DoNotDeleteThisObject();
            i >> entry;
            SeqEntryProcess(entry);
        }

        // MemberIterators are DANGEROUS!  -- do not use the following
        // unless...

        // The same trick can be done with CIStreamClassMember -- to traverse
        // through the class members rather than container elements.
        // CObjectInfo object = member;
        // for ( CIStreamClassMemberIterator i(in, object); i; ++i ) {
        //    // CObjectTypeInfo::CMemberIterator nextMember = *i;
        //    in.ReadObject(object.GetMember(*i));
        // }
    }
    else {
        // standard read
        in.ReadClassMember(member);
    }
}

void CWriteSeqEntryHook::WriteObject(CObjectOStream& out,
                                     const CConstObjectInfo& object)
{
    CInc inc(m_Level);
    if ( m_Level == 1 ) {
        NcbiCerr << "entry" << NcbiEndl;
        // const CSeq_entry& entry = *CType<CSeq_entry>::Get(object);
        object.GetTypeInfo()->DefaultWriteData(out, object.GetObjectPtr());
    }
    else {
        // const CSeq_entry& entry = *CType<CSeq_entry>::Get(object);
        object.GetTypeInfo()->DefaultWriteData(out, object.GetObjectPtr());
    }
}

void CWriteSeqSetHook::WriteClassMember(CObjectOStream& out,
                                        const CConstObjectInfo::CMemberIterator& member)
{
    // keep track of the level of write recursion
    CInc inc(m_Level);

    // just for fun -- do the hook only on the first level of write recursion,
    if ( m_Level == 1 ) {
        // provide opening and closing(automagic, in destr) tags for the member
        COStreamClassMember m(out, member);

        // out.WriteObject(*member);  or, with just the same effect:

        // provide opening and closing(automagic) tags for the container
        COStreamContainer o(out, member);

        typedef CBioseq_set::TSeq_set TSeq_set;
        // const TSeq_set& cnt = *CType<TSeq_set>::Get(*member);
        // but as soon as we know for sure that it *is* TSeq_set, so:
        const TSeq_set& cnt = *CType<TSeq_set>::GetUnchecked(*member);

        // write elem-by-elem
        for ( TSeq_set::const_iterator i = cnt.begin(); i != cnt.end(); ++i ) {
            const TSeqEntry& entry = **i;
            // COStreamContainer is smart enough to automagically put
            // open/close tags for each element written
            o << entry;

            // here, we could e.g. write each elem twice:
            // o << entry;  o << entry;
            // we cannot change the element content as everything is const in
            // the write hooks.
        }
    }
    else {
        // on all other levels -- use standard write func for this member
        out.WriteClassMember(member);
    }
}
