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
* Author:  David McElhany
*
* File Description:
*   See main application file.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <stack>

#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <serial/objectio.hpp>
#include <serial/objistr.hpp>
#include <serial/serialdef.hpp>

#include "seqannot_splicer_stats.hpp"
#include "seqannot_splicer_util.hpp"

USING_SCOPE(ncbi);
USING_SCOPE(objects);


///////////////////////////////////////////////////////////////////////////
// Typedefs

// TSeqRefCont _could_ be different than TIds, so give it its own type.
typedef list<TSeqRef>                       TSeqRefCont;
typedef list<CNcbiStreampos>                TPosCont;

typedef CObjectFor<TSeqRefCont>             TSeqRefContOb;
typedef CObjectFor<TPosCont>                TPosContOb;

typedef CRef<TSeqRefContOb>                 TSeqRefContRef;
typedef CRef<TPosContOb>                    TPosContRef;

typedef set<TSeqRef,PPtrLess<TSeqRef> >     TContextSeqIds;
typedef struct SContext_tag*                TContextPtr;
typedef list<TContextPtr>                   TContextList;
typedef stack<TContextPtr>                  TContextPtrStack;

typedef map<CNcbiStreampos,TSeqRefContRef>              TAnnotToSeqIdMap;
typedef map<TSeqRef,TPosContRef,PPtrLess<TSeqRef> >     TSeqIdToAnnotMap;

typedef TAnnotToSeqIdMap::const_iterator    TAnnotToSeqIdMapCIter;
typedef TAnnotToSeqIdMap::iterator          TAnnotToSeqIdMapIter;
typedef TContextList::iterator              TContextListIter;
typedef TPosCont::iterator                  TPosContIter;
typedef TSeqIdToAnnotMap::const_iterator    TSeqIdToAnnotMapCIter;
typedef TSeqIdToAnnotMap::iterator          TSeqIdToAnnotMapIter;
typedef TSeqRefCont::iterator               TSeqRefContIter;

typedef struct SContext_tag {
    EContextType    type;
    bool            has_annots;
    TContextSeqIds  seqids;
    TContextList    sub_contexts;
} SContext;


///////////////////////////////////////////////////////////////////////////
// Module static functions and objects

static void s_DeleteContext(TContextPtr context);

static void s_DumpContext(TContextPtr context);

static TSeqAnnotChoiceMaskFlags s_GetSeqAnnotChoiceMask(const CSeq_annot* annot);

static TSeqIdChoiceMaskFlags s_GetSeqIdChoiceMask(const CSeq_id* seqid);

static bool s_RemoveAnnot(TAnnotToSeqIdMapIter annot_in_map_iter);

static bool s_SpliceAnnot(auto_ptr<CObjectIStream>& sai,
                          COStreamContainer& osc,
                          TAnnotToSeqIdMapIter annot_in_map);

static void s_SpliceAnnotsForSeqId(auto_ptr<CObjectIStream>& sai,
                                   COStreamContainer& osc,
                                   TSeqIdToAnnotMapIter seqid_iter);


static TAnnotToSeqIdMap             s_AnnotToSeqIdMap;
static TSeqIdToAnnotMap             s_SeqIdToAnnotMap;

static TContextPtr                  s_CurrentContextPtr = NULL;
static TContextPtr                  s_RootContextPtr = NULL;
static TContextList                 s_ContextSequence;
static TContextListIter             s_ContextSequenceIter;
static TContextPtrStack             s_ContextPtrStack;

static TSeqAnnotChoiceMaskFlags     s_SeqAnnotChoiceMask = fSAMF_Default;
static TSeqIdChoiceMaskFlags        s_SeqIdChoiceMask = fSIMF_Default;

static CNcbiStreampos               s_AnnotPos;


///////////////////////////////////////////////////////////////////////////
// Program utility functions

// This function is called during preprocessing of the Seq-entry and adds
// a given Seq-id to the current context.
void AddSeqIdToCurrentContext(TSeqRef id)
{
    // Only add selected Seq-id CHOICE types.
    if (IsSeqIdChoiceSelected(id)) {
        s_CurrentContextPtr->seqids.insert(id);
    }
}


// This function is called during preprocessing of the Seq-entry, at the end
// of the Bioseq or Bioseq-set, and helps create the context tree.
void ContextEnd(void)
{
    s_ContextPtrStack.pop();
    if (s_ContextPtrStack.empty()) {
        s_CurrentContextPtr = NULL;
    } else {
        s_CurrentContextPtr = s_ContextPtrStack.top();
    }
}

// This function is called during preprocessing of the Seq-entry, at the start
// of the Bioseq or Bioseq-set, and helps create the context tree.
void ContextStart(CObjectIStream& in, EContextType type)
{
    // Create and populate new context.
    TContextPtr context = new SContext;
    context->type = type;
    context->has_annots = false;
    context->seqids.clear();
    context->sub_contexts.clear();

    // Insert the new context into the full context tree.
    if (NULL == s_RootContextPtr) {
        s_RootContextPtr = context;
    } else {
        s_CurrentContextPtr->sub_contexts.push_back(context);
    }

    // Use a stack to track the history of current context pointers.
    s_ContextPtrStack.push(context);

    // Keep a full chronological sequence of context pointers for use when
    // copying.
    s_ContextSequence.push_back(context);

    // Use a convenience variable to track the current context.
    s_CurrentContextPtr = context;
}


// This function is called during copying of the Seq-entry, at the start
// of the Bioseq or Bioseq-set, and merely progresses through the sequence of
// contexts.
void ContextEnter(void)
{
    if (s_ContextSequence.end() == s_ContextSequenceIter) {
        s_ContextSequenceIter = s_ContextSequence.begin();
    } else {
        ++s_ContextSequenceIter;
    }
    s_CurrentContextPtr = *s_ContextSequenceIter;
}

// This function is called during copying of the Seq-entry, at the end
// of the Bioseq or Bioseq-set.
// It is empty in this implementation, but could be implemented for a more
// advanced splicing algorithm and is therefore provided for symmetry.
void ContextLeave(void)
{
}


// This function is called after preprocessing of the Seq-entry, and prepares
// for copying.
void ContextInit(void)
{
    // Indicate that no context has been entered yet.
    s_ContextSequenceIter = s_ContextSequence.end();
    s_CurrentContextPtr = NULL;
}


// This function just records that the current context contains Seq-annot's.
void CurrentContextContainsSeqAnnots(void)
{
    s_CurrentContextPtr->has_annots = true;
}


// This function translates format names to enum values.
ESerialDataFormat GetFormat(const string& name)
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


// These functions determine if the given Seq-annot choice type matches the
// user selection.
bool IsSeqAnnotChoiceSelected(TSeqAnnotChoiceMaskFlags flags)
{
    return (flags & s_SeqAnnotChoiceMask) != 0;
}
bool IsSeqAnnotChoiceSelected(const CSeq_annot* annot)
{
    return (s_GetSeqAnnotChoiceMask(annot) & s_SeqAnnotChoiceMask) != 0;
}


// These functions determine if the given Seq-id choice type matches the
// user selection.
bool IsSeqIdChoiceSelected(TSeqIdChoiceMaskFlags flags)
{
    return (flags & s_SeqIdChoiceMask) != 0;
}
bool IsSeqIdChoiceSelected(const CSeq_id* seqid)
{
    return (s_GetSeqIdChoiceMask(seqid) & s_SeqIdChoiceMask) != 0;
}


// This function is called during the copying of a Seq-annot, and (after
// all existing Seq-annot's are copied) splices in any applicable new
// Seq-annot's.
void ProcessSeqEntryAnnot(auto_ptr<CObjectIStream>& sai,
                          COStreamContainer& osc)
{
    // Loop through all Seq-id's for this context, and splice appropriate
    // Seq-annot's.
    ITERATE (TContextSeqIds, seqid_from_se_iter, s_CurrentContextPtr->seqids) {
        // See if the Seq-id from the Seq-entry was also in
        // the Seq-annot's.
        TSeqIdToAnnotMapIter
            seqid_in_map_iter = s_SeqIdToAnnotMap.find(*seqid_from_se_iter);
        if (seqid_in_map_iter != s_SeqIdToAnnotMap.end()) {
            // The current Seq-id for the current Seq-annot for
            // the current Seq-entry being read may be contained
            // by a number of in the Seq-annot file.
            // Find all such Seq-annot's and splice them.
            s_SpliceAnnotsForSeqId(sai, osc, seqid_in_map_iter);

            // Track stats.
            g_Stats->SeqEntry_Changed();
        } else {
            // No Seq-annot's need to be spliced for this Seq-id.
            // Either the Seq-annot file contained no Seq-annot's
            // that contained this Seq-id, or some Seq-annot that
            // contains this Seq-id was already spliced, and so
            // this Seq-id has been removed from the mapping.
        }
    }
}


// This function is called between Seq-entry's, so all context info is reset.
void ResetSeqEntryProcessing(void)
{
    if (NULL != s_RootContextPtr) {
        s_DeleteContext(s_RootContextPtr);
    }
    _ASSERT (s_ContextPtrStack.empty());
    s_ContextSequence.clear();
    s_CurrentContextPtr = NULL;
    s_RootContextPtr = NULL;
}


// This function associates this Seq-id with the containing Seq-annot.
// The mappings created are highly dependent on the splicing algorithm
// and may need to be changed for a real Seq-annot splicing application.
void SeqAnnotMapSeqId(TSeqRef seqid_in_annot)
{
    // Only map selected Seq-id CHOICE types.
    if ( ! IsSeqIdChoiceSelected(&*seqid_in_annot) ) {
        return;
    }

    /////////////////////////////////////////////////////////
    // Forward mapping

    // For this Seq-id, see if it has been mapped to any
    // annotations yet.  If yes, then make sure this position
    // is included in the list.  If not, then insert a map
    // entry with a new vector containing this position.
    {{
        // mapentry finds mapping, if one exists:
        TSeqIdToAnnotMapIter
            mapentry = s_SeqIdToAnnotMap.find(seqid_in_annot);

        if (mapentry != s_SeqIdToAnnotMap.end()) {
            // a mapping exists, so see if the current
            // position is in the position list
            TPosContIter
                pos = find(mapentry->second->GetData().begin(),
                           mapentry->second->GetData().end(),
                           s_AnnotPos);
            if (pos == mapentry->second->GetData().end()) {
                // current position not found in list, so add it
                mapentry->second->GetData().push_back(s_AnnotPos);
            } else {
                //do nothing since position already in list
            }
        } else {
            // this Seq-id hasn't been mapped yet, so add a new
            // map entry with just this position
            TPosContRef newcont(new TPosContOb);
            newcont->GetData().push_back(s_AnnotPos);
            s_SeqIdToAnnotMap.insert(
                pair<TSeqRef, TPosContRef>(seqid_in_annot, newcont));
        }
    }}

    /////////////////////////////////////////////////////////
    // Backward mapping

    // For this Seq-annot, see if it has been mapped to any
    // Seq-id's yet.  If yes, then make sure this Seq-id
    // is included in the list.  If not, then insert a map
    // entry with a new vector containing this Seq-id.
    {{
        // mapentry finds mapping, if one exists:
        TAnnotToSeqIdMapIter
            mapentry = s_AnnotToSeqIdMap.find(s_AnnotPos);

        if (mapentry != s_AnnotToSeqIdMap.end()) {
            // a mapping exists, so see if the current Seq-id is
            // in the Seq-id list
            TSeqRefContIter
                seqid = find(mapentry->second->GetData().begin(),
                             mapentry->second->GetData().end(),
                             seqid_in_annot);
            if (seqid == mapentry->second->GetData().end()) {
                // current Seq-id not found in list, so add it
                mapentry->second->GetData().push_back(seqid_in_annot);
            } // else do nothing since Seq-id already in list
        } else {
            // this position hasn't been mapped yet, so add a new
            // map entry with just this Seq-id
            TSeqRefContRef newvec(new TSeqRefContOb);
            newvec->GetData().push_back(seqid_in_annot);
            s_AnnotToSeqIdMap.insert(
                pair<CNcbiStreampos, TSeqRefContRef>(s_AnnotPos, newvec));
        }
    }}
}


// This function does any needed preprocessing before skipping a Seq-annot.
void SeqAnnotSet_Pre(CObjectIStream& in)
{
    // The file contains full Seq-annot's (including header) but we don't
    // need to read the header in subsequent operations, so just skip it.
    in.SkipFileHeader(CType<CSeq_annot>().GetTypeInfo());

    // Record where in the input stream the data for this Seq-annot starts.
    s_AnnotPos = in.GetStreamPos();
}


// These functions record the user selection for the Seq-annot choice type.
void SetSeqAnnotChoiceMask(const string& mask)
{
    if (mask == "Default") {
        SetSeqAnnotChoiceMask(fSAMF_Default);
        return;
    } else if (mask == "All") {
        SetSeqAnnotChoiceMask(fSAMF_All);
        return;
    }

    list<string>    flag_list;
    NStr::Split(mask, "|", flag_list);
    list<string>::iterator  fbegin(flag_list.begin());
    list<string>::iterator  fend(flag_list.end());
    list<string>::iterator  found_flag;
    TSeqAnnotChoiceMaskFlags flags = fSAMF_NotSet;

#define ADDFLAG(flag) \
    if ((found_flag = find(fbegin,fend,#flag)) != fend) flags |= fSAMF_##flag;

    ADDFLAG(Ftable);
    ADDFLAG(Align);
    ADDFLAG(Graph);
    ADDFLAG(Ids);
    ADDFLAG(Locs);
    ADDFLAG(Seq_table);

#undef ADDFLAG

    SetSeqAnnotChoiceMask(flags);
}
void SetSeqAnnotChoiceMask(const TSeqAnnotChoiceMaskFlags mask)
{
    s_SeqAnnotChoiceMask = mask;
}


// These functions record the user selection for the Seq-id choice type.
void SetSeqIdChoiceMask(const string& mask)
{
    if (mask == "Default") {
        SetSeqIdChoiceMask(fSIMF_Default);
        return;
    } else if (mask == "All") {
        SetSeqIdChoiceMask(fSIMF_All);
        return;
    } else if (mask == "AllButLocal") {
        SetSeqIdChoiceMask(fSIMF_AllButLocal);
        return;
    }

    list<string>    flag_list;
    NStr::Split(mask, "|", flag_list);
    list<string>::iterator  fbegin(flag_list.begin());
    list<string>::iterator  fend(flag_list.end());
    list<string>::iterator  found_flag;
    TSeqIdChoiceMaskFlags flags = fSIMF_NotSet;

#define ADDFLAG(flag) \
    if ((found_flag = find(fbegin,fend,#flag)) != fend) flags |= fSIMF_##flag;

    ADDFLAG(Local);
    ADDFLAG(Gibbsq);
    ADDFLAG(Gibbmt);
    ADDFLAG(Giim);
    ADDFLAG(Genbank);
    ADDFLAG(Embl);
    ADDFLAG(Pir);
    ADDFLAG(Swissprot);
    ADDFLAG(Patent);
    ADDFLAG(Other);
    ADDFLAG(General);
    ADDFLAG(Gi);
    ADDFLAG(Ddbj);
    ADDFLAG(Prf);
    ADDFLAG(Pdb);
    ADDFLAG(Tpg);
    ADDFLAG(Tpe);
    ADDFLAG(Tpd);
    ADDFLAG(Gpipe);
    ADDFLAG(Named_annot_track);

#undef ADDFLAG

    SetSeqIdChoiceMask(flags);
}
void SetSeqIdChoiceMask(const TSeqIdChoiceMaskFlags mask)
{
    s_SeqIdChoiceMask = mask;
}


///////////////////////////////////////////////////////////////////////////
// Module Static functions

// This function deletes a context.
static void s_DeleteContext(TContextPtr context)
{
    while ( ! context->sub_contexts.empty()) {
        s_DeleteContext(context->sub_contexts.back());
        context->sub_contexts.pop_back();
    }
    delete context;
}


// The following function could be used for debugging.
#if 0
// This function prints out the context tree.
static void s_DumpContext(TContextPtr context)
{
    static int indent = 0;

    string s1 = string(4 * indent, ' ');
    string s2 = string(4 * (indent+1), ' ');
    string s3 = string(4 * (indent+2), ' ');
    NcbiCout << s1 << "Context {" << NcbiEndl;
    NcbiCout << s2 << "type: " << context->type << NcbiEndl;
    NcbiCout << s2 << "has_annots: " << context->has_annots << NcbiEndl;
    if (context->seqids.size() > 0) {
        NcbiCout << s2 << "seqids: " << NcbiEndl;
        ITERATE (TContextSeqIds, id, context->seqids) {
            NcbiCout << s3 << "seqid: " << (*id)->GetSeqIdString(true) << NcbiEndl;
        }
    } else {
        NcbiCout << s2 << "seqids: (none)" << NcbiEndl;
    }

    ++indent;
    ITERATE (TContextList, sub, context->sub_contexts) {
        s_DumpContext(*sub);
    }
    --indent;

    NcbiCout << s1 << "}" << NcbiEndl;
}
#endif


// This function returns the user selection for the Seq-annot choice type.
static TSeqAnnotChoiceMaskFlags s_GetSeqAnnotChoiceMask(const CSeq_annot* annot)
{
    if ( ! annot->CanGetData() || ! annot->IsSetData() ) {
        return fSAMF_NotSet;
    }

    switch (annot->GetData().Which()) {
        case CSeq_annot_Base::C_Data::e_not_set:    return fSAMF_NotSet;
        case CSeq_annot_Base::C_Data::e_Ftable:     return fSAMF_Ftable;
        case CSeq_annot_Base::C_Data::e_Align:      return fSAMF_Align;
        case CSeq_annot_Base::C_Data::e_Graph:      return fSAMF_Graph;
        case CSeq_annot_Base::C_Data::e_Ids:        return fSAMF_Ids;
        case CSeq_annot_Base::C_Data::e_Locs:       return fSAMF_Locs;
        case CSeq_annot_Base::C_Data::e_Seq_table:  return fSAMF_Seq_table;
        default: {
            NCBI_THROW(CException, eUnknown, "Unexpected Seq-annot mask");
        }
    }
}


// This function returns the user selection for the Seq-id choice type.
static TSeqIdChoiceMaskFlags s_GetSeqIdChoiceMask(const CSeq_id* seqid)
{
    switch (seqid->Which()) {
        case CSeq_id_Base::e_not_set:           return fSIMF_NotSet;
        case CSeq_id_Base::e_Local:             return fSIMF_Local;
        case CSeq_id_Base::e_Gibbsq:            return fSIMF_Gibbsq;
        case CSeq_id_Base::e_Gibbmt:            return fSIMF_Gibbmt;
        case CSeq_id_Base::e_Giim:              return fSIMF_Giim;
        case CSeq_id_Base::e_Genbank:           return fSIMF_Genbank;
        case CSeq_id_Base::e_Embl:              return fSIMF_Embl;
        case CSeq_id_Base::e_Pir:               return fSIMF_Pir;
        case CSeq_id_Base::e_Swissprot:         return fSIMF_Swissprot;
        case CSeq_id_Base::e_Patent:            return fSIMF_Patent;
        case CSeq_id_Base::e_Other:             return fSIMF_Other;
        case CSeq_id_Base::e_General:           return fSIMF_General;
        case CSeq_id_Base::e_Gi:                return fSIMF_Gi;
        case CSeq_id_Base::e_Ddbj:              return fSIMF_Ddbj;
        case CSeq_id_Base::e_Prf:               return fSIMF_Prf;
        case CSeq_id_Base::e_Pdb:               return fSIMF_Pdb;
        case CSeq_id_Base::e_Tpg:               return fSIMF_Tpg;
        case CSeq_id_Base::e_Tpe:               return fSIMF_Tpe;
        case CSeq_id_Base::e_Tpd:               return fSIMF_Tpd;
        case CSeq_id_Base::e_Gpipe:             return fSIMF_Gpipe;
        case CSeq_id_Base::e_Named_annot_track: return fSIMF_Named_annot_track;
        default: {
            NCBI_THROW(CException, eUnknown, "Unexpected Seq-id mask");
        }
    }
}


// This function removes a Seq-annot from both maps.
static bool s_RemoveAnnot(TAnnotToSeqIdMapIter annot_in_map_iter)
{
    // Track whether or not the original container was removed.
    bool container_removed = false;

    // Loop through all Seq-id's for this Seq-annot and remove their
    // link back to this Seq-annot.
    // Note: This loops through Seq-id's for this particular Seq-annot,
    // not the Seq-id's in the global map.  It then looks up the
    // corresponding Seq-id in the global map.
    // This will erase elements from Seq-annot's container.
    TSeqRefCont& seq_list(annot_in_map_iter->second->GetData());
    NON_CONST_ITERATE (TSeqRefCont, seq_iter, seq_list) {
        // Find the Seq-id in the global map that corresponds to the
        // Seq-id in this Seq-annot's list.
        TSeqIdToAnnotMapIter seqid_in_map = s_SeqIdToAnnotMap.find(*seq_iter);
        _ASSERT(seqid_in_map != s_SeqIdToAnnotMap.end());

        // Find the link to this Seq-annot in this Seq-id's list.
        TPosCont&       annot_list(seqid_in_map->second->GetData());
        TPosContIter    seqannot = find(annot_list.begin(), annot_list.end(),
                                        annot_in_map_iter->first);
        _ASSERT(seqannot != annot_list.end());

        // Erase the link to this Seq-annot from this Seq-id.
        // If this was the initial Seq-id, make sure the iterator doesn't
        // get trashed.
        annot_list.erase(seqannot);

        // Erase this Seq-id from the map if it no longer has any Seq-annot's.
        if (annot_list.empty()) {
            s_SeqIdToAnnotMap.erase(seqid_in_map);
            container_removed = true;
        }
    }

    // Remove this Seq-annot from the map.
    s_AnnotToSeqIdMap.erase(annot_in_map_iter);

    return container_removed;
}


// This function splices a Seq-annot from the Seq-annot stream to the
// new Seq-entry file, and calls the function that removes the Seq-annot
// from the mappings.
static bool s_SpliceAnnot(auto_ptr<CObjectIStream>& sai,
                          COStreamContainer& osc,
                          TAnnotToSeqIdMapIter annot_in_map)
{
    // Seek to to the start of this Seq-annot in the Seq-annot stream.
    sai->SetStreamPos(annot_in_map->first);

    // Read Seq-annot locally (not saved outside this scope).
    CRef<CSeq_annot> annot(new CSeq_annot);
    sai->Read(&*annot, CType<CSeq_annot>().GetTypeInfo(),
             CObjectIStream::eNoFileHeader);

    // Splice the Seq-annot.
    osc << *annot;

    // Track stats.
    g_Stats->SeqAnnot_Spliced();

    // Now we don't need to splice this Seq-annot for any other Seq-id's,
    // so remove it from the maps.
    return s_RemoveAnnot(annot_in_map);
}


// This function splices all the Seq-annot's for a given Seq-id.
static void s_SpliceAnnotsForSeqId(auto_ptr<CObjectIStream>& sai,
                                   COStreamContainer& osc,
                                   TSeqIdToAnnotMapIter seqid_iter)
{
    // Loop through all Seq-annot's for this Seq-id.
    TPosCont&       pos_list = seqid_iter->second->GetData();
    TPosContIter    pos, next_pos;
    for ( pos = pos_list.begin(); pos != pos_list.end(); pos = next_pos ) {
        // Save next iterator, in case this one gets removed by splicing.
        next_pos = pos;
        ++next_pos;

        // Find the Seq-annot in the map that corresponds to the Seq-annot
        // in this Seq-id's list.
        TAnnotToSeqIdMapIter annot_in_map = s_AnnotToSeqIdMap.find(*pos);
        _ASSERT(annot_in_map != s_AnnotToSeqIdMap.end());

        // Splice (and remove) this Seq-annot.
        // Removal will advance the iterator.  After the last Seq-annot for
        // this Seq-id is removed, the Seq-id itself will be removed from
        // the map (invalidating the local iterator), so the loop will be
        // broken out of.
        if (s_SpliceAnnot(sai, osc, annot_in_map)) {
            break;
        }
    }
}
