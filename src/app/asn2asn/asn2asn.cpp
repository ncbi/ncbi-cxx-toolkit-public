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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.46  2004/05/21 21:41:38  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.45  2003/03/11 15:30:29  kuznets
* iterate -> ITERATE
*
* Revision 1.44  2002/09/19 20:05:44  vasilche
* Safe initialization of static mutexes
*
* Revision 1.43  2002/09/17 22:27:09  grichenk
* Type<> -> CType<>
*
* Revision 1.42  2002/09/05 21:23:22  vasilche
* Added mutex for arguments
*
* Revision 1.41  2002/08/30 16:22:47  vasilche
* Added MT mode to asn2asn
*
* Revision 1.40  2001/08/31 20:05:44  ucko
* Fix ICC build.
*
* Revision 1.39  2001/05/17 15:05:45  lavr
* Typos corrected
*
* Revision 1.38  2001/02/01 19:53:17  vasilche
* Reading program arguments from file moved to CNcbiApplication::AppMain.
*
* Revision 1.37  2001/01/30 21:42:29  vasilche
* Added passing arguments via file.
*
* Revision 1.36  2001/01/23 00:12:02  vakatov
* Added comments and usage directions
*
* Revision 1.35  2000/12/24 00:14:16  vakatov
* Minor fix due to the changed NCBIARGS API
*
* Revision 1.34  2000/12/12 14:28:31  vasilche
* Changed the way arguments are processed.
*
* Revision 1.33  2000/11/17 22:04:33  vakatov
* CArgDescriptions::  Switch the order of optional args in methods
* AddOptionalKey() and AddPlain(). Also, enforce the default value to
* match arg. description (and constraints, if any) at all times.
*
* Revision 1.32  2000/11/14 21:40:08  vasilche
* New NCBIArgs API.
*
* Revision 1.31  2000/11/01 20:38:28  vasilche
* Removed ECanDelete enum and related constructors.
*
* Revision 1.30  2000/10/27 14:42:59  ostell
* removed extra CreateArguments call so that usage shows properly
*
* Revision 1.29  2000/10/20 19:29:52  vasilche
* Adapted for MSVC which doesn't like explicit operator templates.
*
* Revision 1.28  2000/10/20 15:51:52  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.27  2000/10/18 13:07:17  ostell
* added proper program name to usage
*
* Revision 1.26  2000/10/17 18:46:25  vasilche
* Added print usage to asn2asn
* Remove unnecessary warning about missing config file.
*
* Revision 1.25  2000/10/13 16:29:40  vasilche
* Fixed processing of optional -o argument.
*
* Revision 1.24  2000/10/03 17:23:50  vasilche
* Added code for CSeq_entry as choice pointer.
*
* Revision 1.23  2000/09/29 14:38:54  vasilche
* Updated for changes in library.
*
* Revision 1.22  2000/09/26 19:29:17  vasilche
* Removed experimental choice pointer stuff.
*
* Revision 1.21  2000/09/26 17:39:02  vasilche
* Updated hooks implementation.
*
* Revision 1.20  2000/09/18 20:47:27  vasilche
* Updated to new headers.
*
* Revision 1.19  2000/09/01 13:16:41  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.18  2000/08/15 19:46:57  vasilche
* Added Read/Write hooks.
*
* Revision 1.17  2000/06/16 16:32:06  vasilche
* Fixed 'unused variable' warnings.
*
* Revision 1.16  2000/06/01 19:07:08  vasilche
* Added parsing of XML data.
*
* Revision 1.15  2000/05/24 20:09:54  vasilche
* Implemented XML dump.
*
* Revision 1.14  2000/04/28 16:59:39  vasilche
* Fixed call to CObjectIStream::Open().
*
* Revision 1.13  2000/04/13 14:50:59  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.12  2000/04/07 19:27:34  vasilche
* Generated objects now are placed in NCBI_NS_NCBI::objects namespace.
*
* Revision 1.11  2000/04/06 16:12:14  vasilche
* Added -c option.
*
* Revision 1.10  2000/03/17 16:47:48  vasilche
* Added copyright message to generated files.
* All objects pointers in choices now share the only CObject pointer.
*
* Revision 1.9  2000/03/07 14:10:52  vasilche
* Fixed for reference counting.
*
* Revision 1.8  2000/02/17 20:07:18  vasilche
* Generated class names now have 'C' prefix.
*
* Revision 1.7  2000/02/04 18:09:57  vasilche
* Added binary option to files.
*
* Revision 1.6  2000/02/04 17:57:45  vasilche
* Fixed for new generated classes interface.
*
* Revision 1.5  2000/01/20 21:51:28  vakatov
* Fixed to follow changes of the "CNcbiApplication" interface
*
* Revision 1.4  2000/01/10 19:47:20  vasilche
* Member type typedef now generated in _Base class.
*
* Revision 1.3  2000/01/10 14:17:47  vasilche
* Fixed usage of different types in ?: statement.
*
* Revision 1.2  2000/01/06 21:28:04  vasilche
* Fixed for variable scope.
*
* Revision 1.1  2000/01/05 19:46:58  vasilche
* Added asn2asn test application.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbithr.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
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
#include <serial/object.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/serial.hpp>
#include <serial/objhook.hpp>
#include <serial/iterator.hpp>

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
    d->AddOptionalKey("l", "logFile",
                      "log errors to <logFile>",
                      CArgDescriptions::eOutputFile);
    d->AddDefaultKey("c", "count",
                      "perform command <count> times",
                      CArgDescriptions::eInteger, "1");
    d->AddDefaultKey("tc", "threadCount",
                      "perform command in <threadCount> thread",
                      CArgDescriptions::eInteger, "1");
    
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


    size_t threadCount = args["tc"].AsInteger();
    vector< CRef<CAsn2AsnThread> > threads(threadCount);
    for ( size_t i = 1; i < threadCount; ++i ) {
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

    for ( size_t i = 1; i < threadCount; ++i ) {
        threads[i]->Join();
        if ( !threads[i]->DoneOk() ) {
            NcbiCerr << "Error in thread: " << i << NcbiEndl;
            return 1;
        }
    }

    return 0;
}

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

    bool inSeqEntry = args["e"];
    bool skip = args["S"];
    bool convert = args["C"];
    bool readHook = args["ih"];
    bool writeHook = args["oh"];

    bool quiet = args["q"];

    size_t count = args["c"].AsInteger();

    GUARD.Release();
    
    for ( size_t i = 1; i <= count; ++i ) {
        bool displayMessages = count != 1 && !quiet;
        if ( displayMessages )
            NcbiCerr << "Step " << i << ':' << NcbiEndl;
        auto_ptr<CObjectIStream> in(CObjectIStream::Open(inFormat, inFile,
                                                         eSerial_StdWhenAny));
        auto_ptr<CObjectOStream> out(!haveOutput? 0:
                                     CObjectOStream::Open(outFormat, outFile,
                                                          eSerial_StdWhenAny));

        if ( inSeqEntry ) { /* read one Seq-entry */
            if ( skip ) {
                if ( displayMessages )
                    NcbiCerr << "Skipping Seq-entry..." << NcbiEndl;
                in->Skip(CType<CSeq_entry>());
            }
            else if ( convert && haveOutput ) {
                if ( displayMessages )
                    NcbiCerr << "Copying Seq-entry..." << NcbiEndl;
                CObjectStreamCopier copier(*in, *out);
                copier.Copy(CType<CSeq_entry>());
            }
            else {
                TSeqEntry entry;
                //entry.DoNotDeleteThisObject();
                if ( displayMessages )
                    NcbiCerr << "Reading Seq-entry..." << NcbiEndl;
                *in >> entry;
                SeqEntryProcess(entry);     /* do any processing */
                if ( haveOutput ) {
                    if ( displayMessages )
                        NcbiCerr << "Writing Seq-entry..." << NcbiEndl;
                    *out << entry;
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
                CBioseq_set entries;
                //entries.DoNotDeleteThisObject();
                if ( displayMessages )
                    NcbiCerr << "Reading Bioseq-set..." << NcbiEndl;
                if ( readHook ) {
                    CObjectTypeInfo bioseqSetType = CType<CBioseq_set>();
                    bioseqSetType.FindMember("seq-set")
                        .SetLocalReadHook(*in, new CReadSeqSetHook);
                    *in >> entries;
                }
                else {
                    *in >> entries;
                    NON_CONST_ITERATE ( CBioseq_set::TSeq_set, seqi,
                                        entries.SetSeq_set() ) {
                        SeqEntryProcess(**seqi);     /* do any processing */
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
                        *out << entries;
                    }
                    else {
                        *out << entries;
                    }
                }
            }
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
