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
* Author:  David McElhany, with thanks to Eugene Vasilchenko and Andrei
*   Gourianov for patiently answering my many questions.
*
* File Description:
*   A demo program to answer the question:
*   - I've got 1,000,000 separate Seq-annots. How do I splice them into
*     Seq-entries I have without reading the seq-entries in?
*
*   NOTE: This question was given in CXX-1382 in the context of demonstrating
*     serial hooks, and the program is therefore designed primarily to demo
*     serial hooks, not to demo an optimal splicing algorithm.
*
* Implementation Approach:
*
*  1. Scan the Seq-annot file and create two cross-reference maps.  One maps
*     from Seq-id to stream positions of Seq-Annot's that reference the Seq-id.
*     The other maps from Seq-annot stream positions to all the Seq-id's that
*     the Seq-annot at that stream position references.
*
*     The scanning is done by setting skip hooks on the Seq-annot and Seq-id
*     object types and skipping through the file.  Inside the Seq-annot hook,
*     the stream position is noted.  Inside the Seq-id hook, the Seq-id is read
*     and cross references between the Seq-annot and the Seq-id are mapped.
*
*     The first map is necessary because Seq-annot's will be spliced based on
*     the Seq-id's they reference.  However, there is no way to know ahead of
*     time which Seq-id from the Seq-entry files will be used to trigger the
*     splicing of a given Seq-annot.  So this map eliminates a linear search
*     through all Seq-annot's looking for Seq-annot's that reference each
*     Seq-id by providing random access to the Seq-annot's keyed on Seq-id.
*
*     The second map was added to eliminate a linear search through all
*     Seq-id's looking for those that a given Seq-annot references.  This
*     will eliminate the search through all Seq-id's every time a Seq-annot is
*     spliced - nominally a million times.  However, optimizing this may or
*     may not be required, and is probably highly dependent on the splicing
*     algorithm.  It is expected that a real splicing application would use
*     a different algorithm and would need to revamp the data structures.
*     Some things to consider:
*     (a) There are initially a million Seq-annot's, but each Seq-annot will
*         only be spliced once so it makes sense to remove it from searches
*         once it has been spliced.  This is most easily accomplished by
*         having a map keyed on Seq-annot stream position.
*     (b) Seq-annot's might reference many Seq-id's, so once a Seq-annot is
*         spliced, it makes sense to not look for it again for any of the
*         Seq-id's that it references.
*     (c) The Seq-entry files may contain a large number of references to the
*         same Seq-id.  So this map provides random access to all the Seq-id's
*         that a given Seq-annot references.
*
*  2. For each Seq-entry file, scan the file and build a tree of contexts
*     within which Seq-annot's can be spliced.  Seq-annot's can only occur in
*     Bioseq or Bioseq-set objects, so for this purpose a context basically
*     just means the nested location of these objects in the Seq-entry file.
*
*     The scanning is done by setting skip hooks for Bioseq and Bioseq-set
*     objects, which build the context tree, and by setting skip hooks on
*     Seq-annot and Seq-id objects, which update the current context node.
*     The Seq-annot hook simply notes that the context contains annotations,
*     while the Seq-id hook just reads in a Seq-id and adds it to the set of
*     Seq-id's referenced in that context.
*
*  3. With the Seq-entry file just scanned, now copy it to the output file,
*     splicing Seq-annot's as appropriate.  The question of where to splice
*     is not the focus of this program, so it uses the simple approach of
*     (a) setting copy hooks on Bioseq and Bioseq-set objects to traverse
*     the context tree, and (b) setting a copy hook on Seq-annot objects to
*     splice any Seq-annot's that contain Seq-id's also referenced in the
*     current context.  Basically, the first time a Seq-id is encountered in
*     the Seq-entry, all Seq-annot's referencing that Seq-id are spliced
*     into the 'annot' member of the enclosing Bioseq or Bioseq-set.
*
* Some Assumptions:
*
*  1. There should be linear complexity in time and memory with respect to
*     the number of Seq-annot's.
*  2. There should be linear complexity in time and memory with respect to
*     the number of Seq-entry's.
*  3. It's OK to use a test file having less than 1,000,000 Seq-annots,
*     provided the program scales well without changing the complexity.
*
*  4. All Seq-annot's are concatenated in one file with no top-level type.
*     Note that due to an apparent bug in CObjectIStream::{G|S}etStreamPos(),
*     this file should be in ASN.1 binary format.
*  5. All Seq-entry's are stored one per file in a dedicated directory and
*     have the same format.
*  6. There are not so many Seq-entry's that storing them all in a single
*     directory will be hard for the file system to manage.
*  7. There's sufficient storage to write the Seq-entry files (with spliced
*     Seq-annot's) to a new directory.
*  8. It's OK to delete all contents of the Seq-entry output directory prior
*     to writing the new spliced Seq-entry files.
*
*  9. All Seq-annot's will reference at least one Seq-id.
* 10. Individual Seq-annot's will typically reference relatively few Seq-id's -
*     that is, optimizing performance won't be necessary for accessing the
*     Seq-id's referenced by a Seq-annot.
* 11. Seq-id's are not unique across all the Seq-annot's.
* 12. Seq-id's are not unique across all the Seq-entry files.
*
* 13. Every Seq-annot should be spliced into exactly one Seq-entry.
* 14. It's OK to splice a Seq-annot into the first Seq-entry containing any
*     Seq-id also in the Seq-annot, regardless of the other Seq-id's contained
*     in either the Seq-annot or Seq-entry.
* 15. If none of the Seq-id's in a Seq-annot are contained in any of the
*     Seq-entry's, the Seq-annot could be spliced into a new "catch-all"
*     Seq-entry (but this is not implemented).
* 16. Any Seq-id's contained in the Seq-entry's but not in any Seq-annot's
*     do not require any processing.
*
* 17. Reporting should go to STDOUT and should include:
*       a. The number of Seq-annot's processed.
*       b. The number of Seq-annot's spliced into existing Seq-entry's.
*       c. The number of Seq-annot's not spliced (or spliced into a new
*          catch-all Seq-entry).
*       d. The number of Seq-entry's processed.
*       e. The number of Seq-entry's copied with spliced Seq-annot's.
*       f. The number of Seq-entry's copied unchanged.
*
* Program Organization:
*   seqannot_splicer.cpp
*       Main application and serial hooks logic.
*   seqannot_splicer_stats.[ch]pp
*       For tracking and reporting program statistics.
*   seqannot_splicer_util.[ch]pp
*       Supporting code not specifically related to serial
*       hooks - notably, a simplistic method of splicing.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistre.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <serial/iterator.hpp>
#include <serial/objcopy.hpp>
#include <serial/objectio.hpp>
#include <serial/objhook.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include "seqannot_splicer_stats.hpp"
#include "seqannot_splicer_util.hpp"

USING_SCOPE(ncbi);
USING_SCOPE(objects);


///////////////////////////////////////////////////////////////////////////
// Static Function Prototypes

static void s_PreprocessSeqAnnots(auto_ptr<CObjectIStream>& sai);

static void s_PreprocessSeqEntry(auto_ptr<CObjectIStream>& sei);


///////////////////////////////////////////////////////////////////////////
// Hook Classes

// This class is used when skipping Seq-annot's.  The entire Seq-annot is
// hooked so that any necessary wrapper functionality can be implemented.
class CHookSeq_annot__Seq_annot : public CSkipObjectHook
{
public:
    virtual void SkipObject(CObjectIStream& in,
                            const CObjectTypeInfo& passed_info)
    {
        // Do any needed processing before skipping object.
        SeqAnnotSet_Pre(in);

        // Skip the Seq-annot (triggering other hooks).
        DefaultSkip(in, passed_info);
    }
};


// This class is used when skipping Seq-annot's.  The Seq-id is
// hooked so that Seq-id's can be associated with the Seq-annot's.
class CHookSeq_annot__Seq_id : public CSkipObjectHook
{
public:
    virtual void SkipObject(CObjectIStream& in,
                            const CObjectTypeInfo& passed_info)
    {
        // Read Seq-id locally (not saved outside this scope).
        CRef<CSeq_id> seqid(new CSeq_id);
        in.Read(&*seqid, CType<CSeq_id>().GetTypeInfo(),
                CObjectIStream::eNoFileHeader);

        // Associate this Seq-id with the containing Seq-annot.
        SeqAnnotMapSeqId(seqid);
    }
};


// This class is used when copying Seq-entry's.  Bioseq and Bioseq-set's are
// hooked so that the context can be tracked, and so that splicing can be
// done within the proper context.
class CHookSeq_entry__Copy : public CCopyObjectHook
{
public:
    virtual void CopyObject(CObjectStreamCopier& copier,
                            const CObjectTypeInfo& passed_info)
    {
        // This hook simply follows the context (Bioseq and Bioseq-set),
        // and the copy hook for the Seq-annot then checks the current
        // context to see if it should splice any Seq-annot's.
        ContextEnter();
        DefaultCopy(copier, passed_info);
        ContextLeave();
    }
};


// This class is used when copying Seq-entry's.  Seq-annot's are hooked so
// that splicing new Seq-annot's can be done in conjunction with copying the
// existing Seq-annot's.
class CHookSeq_entry__Copy__Seq_annot : public CCopyClassMemberHook
{
public:
    CHookSeq_entry__Copy__Seq_annot(auto_ptr<CObjectIStream>& sai)
        : m_sai(sai)
    {
    }
    virtual void CopyClassMember(CObjectStreamCopier& copier,
                                 const CObjectTypeInfoMI& member)
    {
        // The relevant ASN.1 is:
        //     Bioseq ::= {
        //         ...
        //         annot SET OF Seq-annot
        // or:
        //     Bioseq-set ::= {
        //         ...
        //         annot SET OF Seq-annot
        //
        // Hooks are on the 'annot' class member of the both objects.
        //
        // However, we want to process individual Seq-annot's, so we will
        // iterate through the set and: (1) read into a local Seq-annot object,
        // (2) write that object, and (3) check if any new Seq-annot's need
        // to be spliced in with that Seq-annot - if so they then get added.
        //
        // The set is initiated in the output stream (e.g. writing '{' for
        // ASN.1 text format) when the CIStreamContainerIterator is created,
        // and terminated when the CIStreamContainerIterator goes out of scope.

        // Create a temporary Seq-annot object.
        CSeq_annot annot;

        // Create an object output iterator.
        COStreamContainer osc(copier.Out(), member.GetMemberType());

        // Iterate through the existing set of Seq-annot's.
        CIStreamContainerIterator isc(copier.In(), member.GetMemberType());
        for ( ; isc; ++isc ) {
            isc >> annot; // Read existing Seq-annot.
            osc << annot; // Write existing Seq-annot.
        }

        // Splice in any applicable annotations.
        ProcessSeqEntryAnnot(m_sai, osc);
    }
private:
    auto_ptr<CObjectIStream>& m_sai;
};


// This class is used when skipping Seq-entry's.  Bioseq and Bioseq-set's are
// hooked so that a context tree can be created for reference when the
// Seq-entry is copied.
class CHookSeq_entry__Skip : public CSkipObjectHook
{
public:
    CHookSeq_entry__Skip(const EContextType context)
        : m_context(context)
    {
    }

    virtual void SkipObject(CObjectIStream& in,
                            const CObjectTypeInfo& passed_info)
    {
        // This hook just helps establish context for other hooks by
        // recording the context (nesting of Bioseq and Bioseq-set).
        // So just update the context and Skip the object.
        ContextStart(in, m_context);
        DefaultSkip(in, passed_info);
        ContextEnd();
    }
private:
    EContextType    m_context;
};


// This class is used when skipping Seq-entry's.  Seq-annot's are
// hooked because the splicing code needs to know whether the context
// being copied contains a Seq-annot.
class CHookSeq_entry__Skip__Seq_annot : public CSkipObjectHook
{
public:
    virtual void SkipObject(CObjectIStream& in,
                            const CObjectTypeInfo& passed_info)
    {
        // This hook just records that the current context contains
        // Seq-annot's and then skips over the annotation.
        CurrentContextContainsSeqAnnots();
        DefaultSkip(in, passed_info);
    }
};


// This class is used when skipping Seq-entry's.  Seq-id's are
// hooked so that all the Seq-id's referenced in a given context will be
// known when it's time to splice.
class CHookSeq_entry__Skip__Seq_id : public CSkipObjectHook
{
public:
    virtual void SkipObject(CObjectIStream& in,
                            const CObjectTypeInfo& passed_info)
    {
        // Read Seq-id locally (not saved outside this scope).
        CRef<CSeq_id> id(new CSeq_id);
        in.Read(&*id, CType<CSeq_id>().GetTypeInfo(),
                CObjectIStream::eNoFileHeader);

        // Add this Seq-id to the current context.
        AddSeqIdToCurrentContext(id);
    }
};


///////////////////////////////////////////////////////////////////////////
// Static Function Definitions

// This function pre-processes the Seq-annot stream to record the stream
// positions of the Seq-annot's and to associate Seq-annot's with the Seq-id's
// they reference.
static void s_PreprocessSeqAnnots(auto_ptr<CObjectIStream>& sai)
{
    // Set a hook to handle any processing necessary for the full Seq-annot.
    CObjectTypeInfo(CType<CSeq_annot>())
        .SetLocalSkipHook(*sai, new CHookSeq_annot__Seq_annot);

    // Set a hook to process each Seq-id within the Seq-annot's to get info
    // on how to maps Seq-Id's to Seq-annot's and vice versa.
    CObjectTypeInfo(CType<CSeq_id>())
        .SetLocalSkipHook(*sai, new CHookSeq_annot__Seq_id);

    // Skip through all Seq-annot's (triggering the hooks).
    // This will throw when all are skipped and no data is left in the file.
    try {
        while (true) {
            g_Stats->SeqAnnot_Scan_Pre();
            sai->SkipObject(CType<CSeq_annot>());
            g_Stats->SeqAnnot_Scan_Post();
        }
    } catch(CEofException&) {
        // This is expected - no more data to skip.
    } catch(...) {
        // This is not.
        NCBI_THROW(CException, eUnknown,
            "Unexpected exception while preprocessing Seq-annot file.");
    }

    // Remove the hook.
    sai->ResetLocalHooks();
}


// This function figures out the context for each Seq-id so the best place to
// splice Seq-annot's can be determined.
static void s_PreprocessSeqEntry(auto_ptr<CObjectIStream>& sei)
{
    // Set hooks to find the context for each Seq-id.
    CObjectTypeInfo(CType<CBioseq>())
        .SetLocalSkipHook(*sei, new CHookSeq_entry__Skip(eBioseq));

    CObjectTypeInfo(CType<CBioseq_set>())
        .SetLocalSkipHook(*sei, new CHookSeq_entry__Skip(eBioseq_set));

    CObjectTypeInfo(CType<CSeq_annot>())
        .SetLocalSkipHook(*sei, new CHookSeq_entry__Skip__Seq_annot());

    CObjectTypeInfo(CType<CSeq_id>())
        .SetLocalSkipHook(*sei, new CHookSeq_entry__Skip__Seq_id());

    // Ignore header line.
    sei->ReadFileHeader();

    // Skip through the Seq-entry (triggering hooks).
    sei->SkipObject(CType<CSeq_entry>());

    // Remove the skip hooks - they won't be used anymore.
    sei->ResetLocalHooks();

    // Rewind the file.
    sei->SetStreamPos(0);

    // Initialize the current context.
    ContextInit();
}


///////////////////////////////////////////////////////////////////////////
// Main Application Functionality

class CSeqAnnotSplicerApp : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
};


void CSeqAnnotSplicerApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Seq-annot splicer");

    // Describe the expected command-line arguments

    arg_desc->AddDefaultKey
        ("sa", "SeqAnnotFile",
         "name of file containing Seq-annot's",
         CArgDescriptions::eInputFile, "", CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey("safmt", "SeqAnnotFormat", "format of Seq-annot file",
                            CArgDescriptions::eString, "asnb");
    arg_desc->SetConstraint
        ("safmt", &(*new CArgAllow_Strings, "asn", "asnb", "xml", "json"));

    arg_desc->AddDefaultKey
        ("sei", "SeqEntryInputDir",
         "the directory containing the input Seq-Entry files",
         CArgDescriptions::eString, "");
    arg_desc->AddDefaultKey("seifmt", "SeqEntryInputFormat", "format of input Seq-entry files",
                            CArgDescriptions::eString, "asnb");
    arg_desc->SetConstraint
        ("seifmt", &(*new CArgAllow_Strings, "asn", "asnb", "xml", "json"));

    arg_desc->AddDefaultKey
        ("seo", "SeqEntryOutputDir",
         "the directory containing the output Seq-Entry files",
         CArgDescriptions::eString, "");
    arg_desc->AddDefaultKey("seofmt", "SeqEntryOutputFormat", "format of output Seq-entry files",
                            CArgDescriptions::eString, "asnb");
    arg_desc->SetConstraint
        ("seofmt", &(*new CArgAllow_Strings, "asn", "asnb", "xml", "json"));

    arg_desc->AddOptionalKey
        ("sasm", "SeqAnnotChoiceMask",
         "Seq-annot choice mask",
         CArgDescriptions::eString);

    arg_desc->AddOptionalKey
        ("sism", "SeqIdChoiceMask",
         "Seq-id choice mask",
         CArgDescriptions::eString);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int CSeqAnnotSplicerApp::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    // Set up Seq-annot selection mask.
    if (args["sasm"].HasValue()) {
        SetSeqAnnotChoiceMask(args["sasm"].AsString());
    }
    //
    // NOTE: There appears to be a bug in CObjectIStream::{G|S}etStreamPos()
    // for ASN.1 text format, so don't allow that format.
    if (IsSeqAnnotChoiceSelected(fSAMF_Seq_table)) {
        NcbiCout << "I'm sorry, the Seq_table CHOICE mask for Seq-annot's "
                    "is not currently supported \n"
                    "due to an apparent bug in parsing seq-table CHOICE values.\n"
                    "Please try an alternate CHOICE." << NcbiEndl;
        return 0;
    }

    // Set up Seq-id selection mask.
    if (args["sism"].HasValue()) {
        SetSeqIdChoiceMask(args["sism"].AsString());
    }

    // Set up Seq-annot stream.
    auto_ptr<CObjectIStream> sai(CObjectIStream::Open
        (GetFormat(args["safmt"].AsString()),
         args["sa"].AsInputFile()));
    //
    // NOTE: There appears to be a bug in CObjectIStream::{G|S}etStreamPos()
    // for ASN.1 text format, so don't allow that format.
    if (eSerial_AsnText == GetFormat(args["safmt"].AsString())) {
        NcbiCout << "I'm sorry, ASN.1 text is not currently "
                    "supported for the concatenated \n"
                    "Seq-Annot file due to an apparent bug in "
                    "CObjectIStream::{G|S}etStreamPos().\n"
                    "Please try an alternate format." << NcbiEndl;
        return 0;
    }

    // Pre-process the Seq-annot stream to create map from Seq-id to Seq-annot.
    s_PreprocessSeqAnnots(sai);

    // For handling Seq-entry input and output file formats and directories:
    ESerialDataFormat seifmt = GetFormat(args["seifmt"].AsString());
    ESerialDataFormat seofmt = GetFormat(args["seofmt"].AsString());
    string seidir = args["sei"].AsString();
    string seodir = args["seo"].AsString();

    // Clear the Seq-entry output directory.
    CDir(seodir).Remove();
    CDir(seodir).Create();

    // Loop through all the Seq-entry files.
    CDir::TEntries dir_entries(CDir(seidir).GetEntries());
    ITERATE (CDir::TEntries, file, dir_entries) {
        // Get the current (non-trivial) directory entry.
        string name = (*file)->GetName();
        if (name.compare(".")  && name.compare("..")) {
            string seiname = (*file)->GetPath();
            string seoname = seodir + CDir::GetPathSeparator() + name;

            // Start the statistics for this Seq-entry.
            g_Stats->SeqEntry_Start();

            // Set up input and output streams for copying Seq-entry's.
            auto_ptr<CObjectIStream> sei(CObjectIStream::Open(seifmt, seiname));
            auto_ptr<CObjectOStream> seo(CObjectOStream::Open(seofmt, seoname));
            CObjectStreamCopier copier(*sei, *seo);

            // Do any preprocessing necessary.
            NcbiCout << "Processing Seq-entry: " << name << NcbiEndl;
            s_PreprocessSeqEntry(sei);

            // Set up hooks to follow the Bioseq / Bioseq-set context.
            CObjectTypeInfo(CType<CBioseq>())
                .SetLocalCopyHook(copier, new CHookSeq_entry__Copy());
            CObjectTypeInfo(CType<CBioseq_set>())
                .SetLocalCopyHook(copier, new CHookSeq_entry__Copy());

            // Set up hooks to copy existing and possibly splice new
            // Seq-annot's.  Note that the hooks must be set on the 'annot'
            // member, because it's a SET OF, and we want to iterate through
            // the set in the hook.  Therefore, must set a hook for both
            // Bioseq and Bioseq-set (both hooks call the same code).
            CObjectTypeInfo(CType<CBioseq>())
                .FindMember("annot")
                .SetLocalCopyHook(copier, new CHookSeq_entry__Copy__Seq_annot(sai));
            CObjectTypeInfo(CType<CBioseq_set>())
                .FindMember("annot")
                .SetLocalCopyHook(copier, new CHookSeq_entry__Copy__Seq_annot(sai));

            // Copy input Seq-entry to output Seq-entry.  This will trigger the
            // hooks, which will copy the existing Seq-entry's Seq-annot's, then
            // splice any applicable new Seq-annot's.
            copier.Copy(CType<CSeq_entry>());

            // Reset Seq-entry data structures.
            ResetSeqEntryProcessing();

            // Stop the statistics for this Seq-entry.
            g_Stats->SeqEntry_End();
        }
    }

    // Report on program statistics.
    g_Stats->Report();

    return 0;
}


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CSeqAnnotSplicerApp().AppMain(argc, argv);
}
