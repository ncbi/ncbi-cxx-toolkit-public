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

#include "asn2asn.hpp"
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbienv.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

USING_NCBI_SCOPE;

using namespace NCBI_NS_NCBI::objects;

class CNcbiDiagStream
{
public:
    CNcbiDiagStream(CNcbiOstream* afterClose = 0)
        : m_AfterClose(afterClose)
        {
            SetDiagStream(afterClose);
        }
    ~CNcbiDiagStream(void)
        {
            Reset();
        }

    void Reset(void)
        {
            SetDiagStream(m_AfterClose);
            m_CloseStream.reset(0);
        }
    void AssignTo(CNcbiOstream* out, bool deleteOnClose = false)
        {
            SetDiagStream(out);
            if ( deleteOnClose )
                m_CloseStream.reset(out);
        }
    void AssignToNull(void)
        {
            AssignTo(0, true);
        }
    bool AssignToFile(const string& fileName)
        {
            CNcbiOstream* out = new CNcbiOfstream(fileName.c_str());
            if ( !*out ) {
                delete out;
                return false;
            }
            AssignTo(out, true);
            return true;
        }

private:
    auto_ptr<CNcbiOstream> m_CloseStream;
    CNcbiOstream* m_AfterClose;
};

int main(int argc, char** argv)
{
    return CAsn2Asn().AppMain(argc, argv);
}

static
void PrintUsage(void)
{
    NcbiCout <<
        "Arguments:\n"
        "  -i  Filename for asn.1 input\n"
        "  -e  Input is a Seq-entry\n"
        "  -b  Input asnfile in binary mode\n"
        "  -X  Input XML file\n"
        "  -S  Skip value in file without reading it in memory (no write)\n"
        "  -o  Filename for asn.1 output\n"
        "  -s  Output asnfile in binary mode\n"
        "  -x  Output XML file\n"
        "  -l  Log errors to file named\n"
        "  -c <number>  Repeat action <number> times\n";
    exit(1);
}

static
void InvalidArgument(const string& arg)
{
    ERR_POST(Error << "Invalid argument: " << arg);
    PrintUsage();
}

static
const string& StringArgument(const CNcbiArguments& args, size_t index)
{
    if ( index >= args.Size() || args[index].empty() )
        InvalidArgument("Argument required");
    return args[index];
}

static
void SeqEntryProcess(CSeq_entry& sep);  /* dummy function */


/*****************************************************************************
*
*   Main program loop to read, process, write SeqEntrys
*
*****************************************************************************/
int CAsn2Asn::Run(void)
{
    string inFile;
    bool skip = false; 
    bool inSeqEntry = false;
    ESerialDataFormat inFormat = eSerial_AsnText;
    string outFile;
    ESerialDataFormat outFormat = eSerial_AsnText;
    string logFile;
    int count = 1;
    CNcbiDiagStream logStream(&NcbiCerr);
	SetDiagPostLevel(eDiag_Error);

    if ( GetArguments().Size() == 1 )
        PrintUsage();

    {
        for ( size_t i = 1; i < GetArguments().Size(); ++i ) {
            const string& arg = GetArguments()[i];
            if ( arg[0] == '-' ) {
                switch ( arg[1] ) {
                case 'i':
                    inFile = StringArgument(GetArguments(), ++i);
                    break;
                case 'e':
                    inSeqEntry = true;
                    break;
                case 'b':
                    inFormat = eSerial_AsnBinary;
                    break;
                case 'X':
                    inFormat = eSerial_Xml;
                    break;
                case 'o':
                    outFile = StringArgument(GetArguments(), ++i);
                    break;
                case 's':
                    outFormat = eSerial_AsnBinary;
                    break;
                case 'x':
                    outFormat = eSerial_Xml;
                    break;
                case 'S':
                    skip = true;
                    break;
                case 'l':
                    logFile = StringArgument(GetArguments(), ++i);
                    break;
                case 'c':
                    count = NStr::StringToInt(StringArgument(GetArguments(),
                                                             ++i));
                    break;
                default:
                    InvalidArgument(arg);
                    break;
                }
            }
            else {
                InvalidArgument(arg);
                break;
            }
        }
    }


    if ( !logFile.empty() ) {
        if ( logFile == "stderr" || logFile == "-" ) {
            logStream.AssignTo(&NcbiCerr);
        }
        else if ( logFile == "null" ) {
            logStream.AssignToNull();
        }
        else if ( !logStream.AssignToFile(logFile) ) {
            ERR_POST(Fatal << "Cannot open log file: " << logFile);
        }
    }

    for ( int i = 0; i < count; ++i ) {
        auto_ptr<CObjectIStream> in(CObjectIStream::Open(inFormat, inFile,
                                                         eSerial_StdWhenAny));
        auto_ptr<CObjectOStream> out(outFile.empty()? 0:
                                     CObjectOStream::Open(outFormat, outFile,
                                                          eSerial_StdWhenAny));

        if ( inSeqEntry ) { /* read one Seq-entry */
            if ( skip ) {
                in->Skip(CSeq_entry::GetTypeInfo());
            }
            else {
                CSeq_entry entry;
                *in >> entry;
                SeqEntryProcess(entry);     /* do any processing */
                if ( out.get() )
                    *out << entry;
            }
        }
        else {              /* read Seq-entry's from a Bioseq-set */
            if ( skip ) {
                in->Skip(CBioseq_set::GetTypeInfo());
            }
            else {
                CBioseq_set entries;
                *in >> entries;
                non_const_iterate ( CBioseq_set::TSeq_set, i,
                                    entries.SetSeq_set() ) {
                    SeqEntryProcess(**i);     /* do any processing */
                }
                if ( out.get() )
                    *out << entries;
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
void SeqEntryProcess (CSeq_entry& sep)
{
}

