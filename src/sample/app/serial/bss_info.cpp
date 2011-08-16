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
*   A demo program to address the problem statement:
*   - Please extract the seq-id, tax-id, and defline for each of some very
*     large number of sequences packed in a Bioseq-set.
*
*   NOTE: This question was given in CXX-1382 in the context of demonstrating
*     serial hooks, and the program is therefore designed primarily to demo
*     serial hooks, not necessarily navigating object hierarchies.
*
* Some Assumptions:
*
*  1. The input will be a Seq-entry file containing the Bioseq-set.
*  2. The Seq-id to be reported will simply be the Bioseq.id.
*  3. The Object Manager shouldn't be used to retrieve the Tax-id or Defline
*     (CXX-1382 specifically requested that hooks would be used to answer the
*     question).
*  4. The Tax-id to be reported will be that of the Bioseq itself or, if it
*     doesn't specify one, that of the most nested enclosing Bioseq or
*     Bioseq-set that defines a Tax-id.  Every Bioseq (or one of its enclosing
*     Bioseq's or Bioseq-set's) must define a Tax-id.
*  5. The Defline to be reported will be found in the Bioseq.descr.title or,
*     if that isn't present, in the descr.title of the most nested enclosing
*     Bioseq or Bioseq-set that defines a descr.title.  Every Bioseq (or one
*     of its enclosing Bioseq's or Bioseq-set's) must define a Defline.
*
* Implementation Approach:
*
*  1. Set up a class member skip hook on Bioseq.id.  This hook will record the
*     current Bioseq.id, and when the information for the current Bioseq is
*     reported, this is the Seq-id that will be used.
*  2. Set up stack path skip hooks for "*.descr.source.org.db" and
*     "*.descr.org.db".  These hooks will set the current Tax-id.  Stack path
*     hooks are used to ensure that Tax-id's are parsed only within the proper
*     structural context.
*  3. Set up an object skip hook for Bioseq-set.  This hook will udpate a stack
*     containing the applicable Tax-id.
*  4. Set up an object skip hook for Bioseq.  This hook will udpate a stack
*     containing the applicable Tax-id, and will also determine the Defline and
*     report the desired information.
*  5. Skip through the file, triggering the hooks.
* 
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <stack>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbistre.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <serial/objcopy.hpp>
#include <serial/objectio.hpp>
#include <serial/objhook.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);

///////////////////////////////////////////////////////////////////////////
// Local Types

enum EBioseqContext {
    eBioseqContextBioseq,
    eBioseqContextBioseqSet
};

enum ETaxPath {
    eTaxPathOrg,
    eTaxPathSourceOrg
};


// Keep info about a Bioseq.  Note that this info could be inherited from a
// Bioseq-set.descr.

struct SBioseqInfo
{
    SBioseqInfo()
        : seqid_str(""), taxid_org(-1), taxid_source_org(-1), defline("")
    {
    }

    string  seqid_str;
    int     taxid_org;
    int     taxid_source_org;
    string  defline;
};


///////////////////////////////////////////////////////////////////////////
// Module Static Functions and Data

static ESerialDataFormat    s_GetFormat(const string& name);
static void                 s_Report(void);

static stack<SBioseqInfo>   s_BioseqInfoStack;


///////////////////////////////////////////////////////////////////////////
// Hook Classes

// This class finds Bioseq's and Bioseq-set's when skipping, gathers info
// about the context, and reports the info (for Bioseq's).
class CHookBioseqContext : public CSkipObjectHook
{
public:
    CHookBioseqContext(EBioseqContext context)
        : m_Context(context)
    {
    }

    virtual void SkipObject(CObjectIStream& stream,
                            const CObjectTypeInfo& info)
    {
        // Push info to be used for this Bioseq.  Note: this is initially
        // invalid, but the info will be overwritten (in other hooks) before
        // it gets used.
        if (s_BioseqInfoStack.empty()) {
            // Push a new empty object for the first time.
            SBioseqInfo  bs_info;
            s_BioseqInfoStack.push(bs_info);
        } else {
            // Push a copy of the last object for this Bioseq.
            // This facilitates inheriting info through nesting.
            s_BioseqInfoStack.push(s_BioseqInfoStack.top());
        }

        // Skip the Bioseq, triggering other hooks which in turn retrieve
        // relevant data.
        DefaultSkip(stream, info);

        // Report the required information (if this is a Bioseq).
        if (m_Context == eBioseqContextBioseq) {
            s_Report();
        }

        // We're done with this Bioseq info.
        s_BioseqInfoStack.pop();
    }

private:
    EBioseqContext  m_Context;
};


// This class finds Bioseq.id's when skipping.  The hook reads and records
// the last Bioseq.id encountered.
class CHookBioseq__Seq_id : public CSkipClassMemberHook
{
public:
    virtual void SkipClassMember(CObjectIStream& stream,
                                 const CObjectTypeInfoMI& info)
    {
        // The relevant ASN.1 is:
        //     Bioseq ::= SEQUENCE {
        //         id SET OF Seq-id
        //
        // This hook is on the 'id' class member of Bioseq, which means it's
        // the whole 'SET OF Seq-id' that's hooked, not individual Seq-id's.
        //
        // Therefore, we will iterate through the set and: (1) read into a
        // local Seq-id object, and (2) append that Seq-id's info to the
        // Seq-id string that represents the Bioseq's id.

        string seqid_str("");
        CIStreamContainerIterator isc(stream, info.GetMemberType());
        for ( ; isc; ++isc ) {
            // Read the Seq-id locally.
            CSeq_id seqid;
            isc >> seqid; // Read from the container iterator, not the stream.

            // Append this Seq-id to the Seq-id string.
            if (seqid_str != "") {
                seqid_str += "|";
            }
            seqid_str += seqid.AsFastaString();
        }

        // Update the current Bioseq info with the new Seq-id string.
        s_BioseqInfoStack.top().seqid_str = seqid_str;
    }
};


// This class finds Tax-id's when skipping, and sets the current Tax-id.
class CHookTax_id : public CSkipClassMemberHook
{
public:
    CHookTax_id(ETaxPath tax_path)
        : m_TaxPath(tax_path)
    {
    }

    virtual void SkipClassMember(CObjectIStream& stream,
                                 const CObjectTypeInfoMI& info)
    {
        // The relevant ASN.1 is:
        //     Seqdesc ::= CHOICE {
        //         org Org-ref ,
        //         source BioSource ,
        //     BioSource ::= SEQUENCE {
        //         org Org-ref ,
        //     Org-ref ::= SEQUENCE {
        //         db SET OF Dbtag OPTIONAL ,
        //     Dbtag ::= SEQUENCE {
        //         db VisibleString ,
        //         tag Object-id }
        //     Object-id ::= CHOICE {
        //         id INTEGER ,
        //         str VisibleString }
        //
        // This hook is on the 'db' class member of Org-ref (as set via stack
        // path hooks), which means it's the whole 'SET OF Dbtag' that's
        // hooked, not individual Dbtag's.
        //
        // Therefore, we will iterate through the set and: (1) read into a
        // local Dbtag object, and (2) parse that Dbtag to see if it's a
        // Tax-id and if so, update the current Bioseq info.

        // Set up the container iterator based on the hooked object info.
        CIStreamContainerIterator isc(stream, info.GetMemberType());
        for ( ; isc; ++isc ) {
            // Read the Dbtag locally.
            CDbtag  dbtag;
            isc >> dbtag; // Read from the container iterator, not the stream.

            // Get access to the Dbtag.db class member.
            CObjectInfo     obj = ObjectInfo<CDbtag>(dbtag);
            CObjectInfo     db_member = obj.SetClassMember
                                (obj.FindMemberIndex("db"));

            // Get the value of the Dbtag.db class member.
            string          db_str = db_member.GetPrimitiveValueString();

            // Only continue for taxonomy db entries.
            if (db_str == "taxon") {
                // Get access to the Dbtag.tag class member.
                CObjectInfo     tag_cont = obj.SetClassMember
                                    (obj.FindMemberIndex("tag"));

                // Get access to the Dbtag.tag class member (the Object-id).
                CObjectInfo     tag_pt = tag_cont.GetPointedObject();

                // Get access to the selected Object-id choice variant.
                CObjectInfoCV   tag_var = tag_pt.GetCurrentChoiceVariant();
                CObjectInfo     tag_choice = tag_var.GetVariant();

                // Get the value of the selected Object-id choice variant.
                int taxid;
                if (tag_var.GetVariantInfo()->GetId().GetName() == "id") {
                    taxid = tag_choice.GetPrimitiveValueInt();
                } else {
                    taxid = NStr::StringToInt
                        (tag_choice.GetPrimitiveValueString(),
                         NStr::fConvErr_NoThrow);
                }

                // Only keep the last Tax-id found (for each path).
                if (m_TaxPath == eTaxPathOrg) {
                    s_BioseqInfoStack.top().taxid_org = taxid;
                } else {
                    s_BioseqInfoStack.top().taxid_source_org = taxid;
                }
            }
        }
    }

private:
    ETaxPath  m_TaxPath;
};


// This class finds Defline's when skipping, and sets the current Defline.
class CHookDefline : public CSkipObjectHook
{
public:
    virtual void SkipObject(CObjectIStream& stream,
                            const CObjectTypeInfo& info)
    {
        // Get a reference to the current Bioseq info.
        SBioseqInfo& bs_info(s_BioseqInfoStack.top());

        // Read the Defline into the current Bioseq info.
        stream.Read(&bs_info.defline,
                    CStdTypeInfo<string>::GetTypeInfo(),
                    CObjectIStream::eNoFileHeader);
    }
};


///////////////////////////////////////////////////////////////////////////
// Static Function Definitions


// This function translates format names to enum values.
ESerialDataFormat s_GetFormat(const string& name)
{
    if (name == "asn") {
        return eSerial_AsnText;
    } else if (name == "asnb") {
        return eSerial_AsnBinary;
    } else if (name == "xml") {
        return eSerial_Xml;
    } else if (name == "json") {
        return eSerial_Json;
    } else {
        // Should be caught by argument processing, but in case of a
        // programming error...
        NCBI_THROW(CException, eUnknown, "Bad serial format name " + name);
    }
}

// This function will print the required information for the current Bioseq.
static void s_Report(void)
{
    // Get the Seq-id string.
    string seqid_str = s_BioseqInfoStack.top().seqid_str;

    // Get the Tax-id, preferring Tax-id's found with stack path
    // *.descr.source.org.db over those with path *.descr.org.db --
    // see ncbi::objects::CBioseq_Info::GetTaxId() and
    // ncbi::objects::sequence::GetTaxId().
    int taxid = s_BioseqInfoStack.top().taxid_source_org;
    // If not found in source.org, taxid will be < 1 so try just org.
    if (taxid < 1) {
        taxid = s_BioseqInfoStack.top().taxid_org;
    }

    // Get the Defline.
    string defline = s_BioseqInfoStack.top().defline;

    // Report the required information.
    NcbiCout << ">" 
        << seqid_str << " " 
        << defline << " [" 
        << taxid << "]" << NcbiEndl;
}


///////////////////////////////////////////////////////////////////////////
// Main Application Functionality

class CBssInfoApp : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
};


void CBssInfoApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Bioseq-set info extractor");

    // Describe the expected command-line arguments

    arg_desc->AddDefaultKey
        ("i", "InputFile",
         "name of input file",
         CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey("ifmt", "InputFormat", "format of input file",
                            CArgDescriptions::eString, "asn");
    arg_desc->SetConstraint
        ("ifmt", &(*new CArgAllow_Strings, "asn", "asnb", "xml", "json"));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

int CBssInfoApp::Run(void)
{
    // Get arguments
    CArgs args = GetArgs();

    // Set up the input stream.
    auto_ptr<CObjectIStream> in(CObjectIStream::Open
        (s_GetFormat(args["ifmt"].AsString()),
         args["i"].AsInputFile()));

    // Set up an object skip hook for Bioseq.  This hook will udpate a stack
    // containing the required info and report the required info.
    CObjectTypeInfo(CType<CBioseq>())
        .SetLocalSkipHook(*in, new CHookBioseqContext(eBioseqContextBioseq));

    // Set up an object skip hook for Bioseq-set.  This hook will udpate a stack
    // containing the required info.
    CObjectTypeInfo(CType<CBioseq_set>())
        .SetLocalSkipHook(*in, new CHookBioseqContext(eBioseqContextBioseqSet));

    // Set up a class member skip hook on Bioseq.id.  This hook will record the
    // current Bioseq.id, and when the information for the current Bioseq is
    // reported, this is the Seq-id that will be used.
    CObjectTypeInfo(CType<CBioseq>())
        .FindMember("id")
        .SetLocalSkipHook(*in, new CHookBioseq__Seq_id());

    // Set up stack path skip hooks to capture Tax-id's.  Stack path
    // hooks are used to ensure that Tax-id's are parsed only within the
    // proper structural context.
    in->SetPathSkipMemberHook("*.descr.source.org.db",
                              new CHookTax_id(eTaxPathSourceOrg));
    in->SetPathSkipMemberHook("*.descr.org.db",
                              new CHookTax_id(eTaxPathOrg));

    // Set up a stack path skip hook to capture the Defline.  A stack path
    // hook is used to ensure that Defline's are parsed only within the
    // proper structural context.
    in->SetPathSkipObjectHook("*.descr.title", new CHookDefline());

    // Skip through the Seq-entry in the input file.  This will trigger the
    // hooks, which will extract and report the desired Bioseq information.
    in->Skip(CType<CSeq_entry>());

    return 0;
}


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CBssInfoApp().AppMain(argc, argv);
}
