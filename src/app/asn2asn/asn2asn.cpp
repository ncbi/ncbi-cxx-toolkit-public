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
* Adde binary option to files.
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

#include <corelib/ncbistd.hpp>
#include "asn2asn.hpp"
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiargs.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/serial.hpp>
#include <serial/objhook.hpp>
#include <serial/iterator.hpp>

USING_NCBI_SCOPE;

using namespace NCBI_NS_NCBI::objects;

int main(int argc, char** argv)
{
    return CAsn2Asn().AppMain(argc, argv);
}

static
void SeqEntryProcess(CSeq_entry& sep);  /* dummy function */

class CReadSeqSetHook : public CReadClassMemberHook
{
public:
    CReadSeqSetHook(void)
        : m_Level(0)
        {
        }
    ~CReadSeqSetHook(void)
        {
            _ASSERT(m_Level == 0);
        }

    void ReadClassMember(CObjectIStream& in,
                         const CObjectInfo& object,
                         TMemberIndex memberIndex);

    class CReadSeqEntryHook : public CReadContainerElementHook
    {
    public:
        void ReadContainerElement(CObjectIStream& in,
                                  const CObjectInfo& container);
    };

    int m_Level;
};

/*****************************************************************************
*
*   Main program loop to read, process, write SeqEntrys
*
*****************************************************************************/
int CAsn2Asn::Run(void)
{
	SetDiagPostLevel(eDiag_Error);

    auto_ptr<CArgs> args;

    // read arguments
    {
        CArgDescriptions argDesc;
        argDesc.AddKey("i", "inputFile", "input data file",
                       argDesc.eInputFile, argDesc.fDelayOpen);
        argDesc.AddOptionalKey("o", "outputFile", "output data file",
                               argDesc.eOutputFile, argDesc.fDelayOpen);
        argDesc.AddFlag("e", "treat data as Seq-entry");
        argDesc.AddFlag("b", "binary ASN.1 input format");
        argDesc.AddFlag("X", "XML input format");
        argDesc.AddFlag("s", "binary ASN.1 output format");
        argDesc.AddFlag("x", "XML output format");
        argDesc.AddFlag("C", "Convert data without reading in memory");
        argDesc.AddFlag("S", "Skip data without reading in memory");
        argDesc.AddOptionalKey("l", "logFile", "log errors to <logFile>",
                               argDesc.eOutputFile);
        argDesc.AddOptionalKey("c", "count", "perform command <count> times",
                               argDesc.eInteger, 0, "1");
        
        argDesc.AddFlag("ih", "Use read hooks");
        argDesc.AddFlag("oh", "Use write hooks");

        args.reset(argDesc.CreateArgs(GetArguments()));
    }

    if ( args->Exist("l") )
        SetDiagStream(&(*args)["l"].AsOutputFile());

    string inFile = (*args)["i"].AsString();
    ESerialDataFormat inFormat = eSerial_AsnText;
    if ( args->Exist("b") )
        inFormat = eSerial_AsnBinary;
    else if ( args->Exist("X") )
        inFormat = eSerial_Xml;

    bool haveOutput = args->Exist("o");
    string outFile;
    ESerialDataFormat outFormat = eSerial_AsnText;
    if ( haveOutput ) {
        outFile = (*args)["o"].AsString();
        if ( args->Exist("s") )
            outFormat = eSerial_AsnBinary;
        else if ( args->Exist("x") )
            outFormat = eSerial_Xml;
    }

    bool inSeqEntry = args->Exist("e");
    bool skip = args->Exist("S");
    bool convert = args->Exist("C");
    bool readHook = args->Exist("ih");
    bool writeHook = args->Exist("oh");

    size_t count = (*args)["c"].AsInteger();

    for ( size_t i = 1; i <= count; ++i ) {
        bool displayMessages = count != 1;
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
                in->Skip(Type<CSeq_entry>());
            }
            else if ( convert && haveOutput ) {
                if ( displayMessages )
                    NcbiCerr << "Copying Seq-entry..." << NcbiEndl;
                CObjectStreamCopier copier(*in, *out);
                copier.Copy(Type<CSeq_entry>());
            }
            else {
                CSeq_entry entry;
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
                in->Skip(Type<CBioseq_set>());
            }
            else if ( convert && haveOutput ) {
                if ( displayMessages )
                    NcbiCerr << "Copying Bioseq-set..." << NcbiEndl;
                CObjectStreamCopier copier(*in, *out);
                copier.Copy(Type<CBioseq_set>());
            }
            else {
                CBioseq_set entries;
                if ( displayMessages )
                    NcbiCerr << "Reading Bioseq-set..." << NcbiEndl;
                if ( readHook ) {
                    CObjectTypeInfo bioseqSetType = Type<CBioseq_set>();
                    CObjectTypeInfo::CMemberIterator member =
                        bioseqSetType.FindMember("seq-set");
                    CReadSeqSetHook hook;
                    const_cast<CMemberInfo*>(member.GetMemberInfo())->SetLocalReadHook(*in, &hook);
                    *in >> entries;
                    const_cast<CMemberInfo*>(member.GetMemberInfo())->ResetLocalReadHook(*in);
                }
                else {
                    *in >> entries;
                    non_const_iterate ( CBioseq_set::TSeq_set, seqi,
                                        entries.SetSeq_set() ) {
                        SeqEntryProcess(**seqi);     /* do any processing */
                    }
                }
                if ( haveOutput ) {
                    if ( displayMessages )
                        NcbiCerr << "Writing Bioseq-set..." << NcbiEndl;
                    if ( writeHook ) {
                    }
                    else {
                        *out << entries;
                    }
                }
            }
        }
    }
	return 0;
}


/*****************************************************************************
*
*   void SeqEntryProcess (sep)
*      just a dummy routine that does nothing
*
*****************************************************************************/
static
void SeqEntryProcess (CSeq_entry& /*sep*/)
{
}

void CReadSeqSetHook::ReadClassMember(CObjectIStream& in,
                                      const CObjectInfo& object,
                                      TMemberIndex memberIndex)
{
    ++m_Level;
    //    NcbiCerr << "+Level: " << m_Level << NcbiEndl;
    if ( m_Level == 1 ) {
        CReadSeqEntryHook hook;
        (*object.GetClassMemberIterator(memberIndex)).ReadContainer(in, hook);
    }
    else {
        in.ReadObject(*object.GetClassMemberIterator(memberIndex));
    }
    //    NcbiCerr << "-Level: " << m_Level << NcbiEndl;
    --m_Level;
}

void CReadSeqSetHook::
CReadSeqEntryHook::ReadContainerElement(CObjectIStream& in,
                                        const CObjectInfo& /*cont*/)
{
    CSeq_entry entry;
    in.ReadSeparateObject(ObjectInfo(entry));
    SeqEntryProcess(entry);
}
