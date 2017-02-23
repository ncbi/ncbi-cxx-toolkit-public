
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
* Author:  Justin Foley
*
* File Description:
*   Code to assign accessions to new protein sequences
*
* ===========================================================================
*/



#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistd.hpp>

#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <util/line_reader.hpp>
#include <objtools/edit/protein_match/prot_match_exception.hpp>



BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class CCopySeqIdHook : public CCopyObjectHook
{
public:

    typedef map<string, CRef<CSeq_id>> TIdMap;
    CCopySeqIdHook(TIdMap& id_map) : m_IdMap(id_map) {}

    virtual void CopyObject(CObjectStreamCopier& copier,
        const CObjectTypeInfo& passed_info);

    void x_UpdateSeqId(const TIdMap& id_map, 
        CSeq_id& id) const;

private:
    TIdMap m_IdMap; 
};


void CCopySeqIdHook::CopyObject(CObjectStreamCopier& copier,
    const CObjectTypeInfo& passed_info) 
{
    CSeq_id seq_id;
    copier.In().ReadObject(&seq_id, CSeq_id::GetTypeInfo());

    if (seq_id.IsLocal()) {
        x_UpdateSeqId(m_IdMap, seq_id);
    }

    copier.Out().WriteObject(&seq_id, CSeq_id::GetTypeInfo());
}


void CCopySeqIdHook::x_UpdateSeqId(const TIdMap& id_map,
    CSeq_id& id) const
{
    if (!id.IsLocal()) {
        return;
    }

    const string id_string = id.GetLocal().IsStr() ?
        id.GetLocal().GetStr() :
        NStr::NumericToString(id.GetLocal().GetId());

    const auto match = id_map.find(id_string);

    if (match != id_map.end()) {
        const CSeq_id& new_id = *(match->second);
        id.Assign(new_id); // Reference - Sergiy G. 
    }
    return;
}

class CProtIdUpdateApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:

    typedef map<string, CRef<CSeq_id>> TIdMap;

    bool x_ProcessInputTable(CNcbiIstream& istr, 
                             TIdMap& id_map);
};


void CProtIdUpdateApp::Init(void) 
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());

    arg_desc->AddKey("i",
            "InputFile",
            "Path to Seq-entry input file",
            CArgDescriptions::eInputFile);

    arg_desc->AddKey("t",
            "Table",
            "Path to file containing protein match table",
            CArgDescriptions::eInputFile);

    arg_desc->AddKey("o", "OutputFile",
            "Path to Seq-entry output file. Defaults to stdout",
            CArgDescriptions::eOutputFile);

    SetupArgDescriptions(arg_desc.release());

    return;
}


int CProtIdUpdateApp::Run(void) 
{
    const CArgs& args = GetArgs();

    CNcbiIstream& table_str = args["t"].AsInputFile();
    map<string, CRef<CSeq_id>> id_map;
    x_ProcessInputTable(table_str, id_map);

    unique_ptr<CObjectIStream> istr(CObjectIStream::Open(eSerial_AsnText, args["i"].AsString()));
    unique_ptr<CObjectOStream> ostr(CObjectOStream::Open(eSerial_AsnText, args["o"].AsString()));

    CObjectStreamCopier copier(*istr, *ostr);
    CObjectTypeInfo(CType<CSeq_id>()) 
        .SetLocalCopyHook(copier, new CCopySeqIdHook(id_map));

    set<TTypeInfo> knownTypes, matchingTypes;
    knownTypes.insert(CSeq_entry::GetTypeInfo());
    knownTypes.insert(CBioseq_set::GetTypeInfo());
    matchingTypes = istr->GuessDataType(knownTypes);

    if (matchingTypes.empty()) {
        NCBI_THROW(CProteinMatchException,
            eInputError,
            "Unrecognized input");
    }

    const TTypeInfo typeInfo = *matchingTypes.begin();

    if (typeInfo == CSeq_entry::GetTypeInfo()) {
        copier.Copy(CType<CSeq_entry>());
    } else {
        copier.Copy(CType<CBioseq_set>());
    }

    return 0;
}


bool CProtIdUpdateApp::x_ProcessInputTable(CNcbiIstream& istr, TIdMap& id_map) 
{
    CStreamLineReader lr(istr);

    while ( !lr.AtEOF() ) {
        string line = *++lr;

        if (line.empty() ||
            NStr::StartsWith(line, "#")) {
            continue;
        }

        vector<string> entries;
        string delim = " \t";
        NStr::Split(line, delim, entries, NStr::fSplit_MergeDelimiters);
        if (!entries.empty() &&
            entries[0] == "NA_ACCESSION") {
            continue;
        } 

        if (entries.size() < 5) { 
            continue;             
        }

        if (entries[2] == "---" ||  // No local ID
            entries[3] != "PROT" || // Not a protein
            (entries[4] != "Same" && entries[4] != "New")) {
            continue;
        }

        if (entries[1] == "___") {
            ERR_POST(Warning << entries[1] << ": Protein accession unspecified");
            continue;
        }

        CRef<CSeq_id> gb_id = Ref(new CSeq_id());
        gb_id->SetGenbank().Set(entries[1]);

        string local = entries[2];
        id_map[local] = gb_id;
    }

    return true;
}


END_NCBI_SCOPE
USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CProtIdUpdateApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
