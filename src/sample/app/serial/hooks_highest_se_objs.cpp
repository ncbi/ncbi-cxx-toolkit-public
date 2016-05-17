/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology gfmtion
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
* Author:  David McElhany
*
* File Description:
*   Show how to process the highest level Seq-entry objects in a data stream,
*   using serial hooks when necessary.
*
*   This sample program simply outputs the label for each processed Seq-entry,
*   but it is done in a way that could be easily extended to more sophisticated
*   processing of the entire Seq-entry.
*
*   Constraints for the sample program:
*   -   The input will be streamed from STDIN, as ASN.1 text.
*   -   The input data type must be specified in the command line.  The
*       supported input data types are Bioseq-set, Seq-entry, and Seq-submit.
*       Note that hooks are not needed if the input type is Seq-entry.
*   -   The input data could be a single object, or multiple concatenated
*       objects of a single type.
*   -   The output will be streamed to STDOUT.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/submit/Seq_submit.hpp>

#include <serial/objectio.hpp>
#include <serial/objhook.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


///////////////////////////////////////////////////////////////////////////
// Static Functions

static void s_Process(const CSeq_entry& entry)
{
    // This sample program simply outputs the Seq-entry's label, but this is
    // where you would put your custom processing code.
    string label;
    entry.GetLabel(&label, CSeq_entry::eBoth);
    NcbiCout << label << NcbiEndl;
}


///////////////////////////////////////////////////////////////////////////
// Hook Classes

// This class processes top-level Seq-entry's when skipping through a
// Bioseq-set.
class CSkipMemberHook__Bioseq_set : public CSkipClassMemberHook
{
public:
    virtual void SkipClassMember(CObjectIStream& stream,
                                    const CObjectTypeInfoMI& passed_info)
    {
        // The relevant ASN.1 is:
        //     Bioseq-set ::= SEQUENCE {
        //         seq-set SEQUENCE OF Seq-entry
        //
        // This hook is on the 'seq-set' class member of Bioseq-set, which
        // means it's the whole 'SEQUENCE OF Seq-entry' that's hooked, not
        // individual Seq-entry's.
        //
        // Therefore, we will iterate through the sequence and: (1) read
        // into a local Seq-entry object, and (2) process that Seq-entry.
        CIStreamContainerIterator isc(stream, passed_info.GetMemberType());
        for ( ; isc; ++isc ) {
            CSeq_entry entry;
            isc >> entry;
            s_Process(entry);
        }
    }
};


// This class processes top-level Seq-entry's when skipping through a
// Seq-submit.
class CSkipVariantHook__Seq_submit : public CSkipChoiceVariantHook
{
public:
    virtual void SkipChoiceVariant(CObjectIStream& stream,
                                    const CObjectTypeInfoCV& passed_info)
    {
        // The relevant ASN.1 is:
        //     Seq-submit ::= SEQUENCE {
        //         data CHOICE {
        //             entrys  SET OF Seq-entry
        //
        // This hook is on the 'entrys' choice variant of Seq-submit.data,
        // which means it's the whole 'SET OF Seq-entry' that's hooked, not
        // individual Seq-entry's.
        //
        // Therefore, we will iterate through the set and: (1) read into a
        // local Seq-entry object, and (2) process that Seq-entry.
        CIStreamContainerIterator isc(stream, passed_info.GetVariantType());
        for ( ; isc; ++isc ) {
            CSeq_entry entry;
            isc >> entry;
            s_Process(entry);
        }
    }
};


///////////////////////////////////////////////////////////////////////////
// Main Application Functionality

class CProcessHighestSeObjs : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
};


void CProcessHighestSeObjs::Init(void)
{
    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
        "Bioseq-set info extractor");

    // Describe the expected command-line arguments
    arg_desc->AddKey("type", "InputType", "type of input data object",
        CArgDescriptions::eString);
    arg_desc->SetConstraint("type",
        &(*new CArgAllow_Strings, "Bioseq-set", "Seq-entry", "Seq-submit"));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CProcessHighestSeObjs::Run(void)
{
    // Get object stream.
    unique_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, NcbiCin));

    // Set hooks.  No hooks are necessary for Seq-entry because in that case
    // all highest-level (Seq-entry) objects are read.
    string type_str = GetArgs()["type"].AsString();
    if (type_str == "Bioseq-set") {
        in->SetPathSkipMemberHook("Bioseq-set.seq-set",
            new CSkipMemberHook__Bioseq_set());
    } else if (type_str == "Seq-submit") {
        in->SetPathSkipVariantHook("Seq-submit.data.entrys",
            new CSkipVariantHook__Seq_submit());
    }

    // Repeat processing for each concatenated object in input stream.
    while ( ! in->EndOfData()) {
        if (type_str == "Bioseq-set") {
            in->Skip(CType<CBioseq_set>());
        } else if (type_str == "Seq-submit") {
            in->Skip(CType<CSeq_submit>());
        } else if (type_str == "Seq-entry") {
            // Just read and process each object - no skipping is needed.
            CSeq_entry entry;
            *in >> entry;
            s_Process(entry);
        }
    }

    return 0;
}


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CProcessHighestSeObjs().AppMain(argc, argv);
}
