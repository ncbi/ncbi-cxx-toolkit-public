#ifndef HUGE_FILE_CLEANUP_HPP
#define HUGE_FILE_CLEANUP_HPP

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
 * File Description:
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objtools/edit/huge_asn_reader.hpp>
#include <objtools/edit/huge_file_process.hpp>
#include <objtools/cleanup/cleanup_change.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Feat_id.hpp>

#include <map>
#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CFeat_id;

class CCleanupHugeAsnReader: public edit::CHugeAsnReader
{
public:
    enum EOptions {
        eExtendedCleanup       = 1,
        eNoNcbiUserObjects     = 1<<1,
        eEnableSmallGenomeSets = 1<<2,
        eRemoveNcbiUserObjects = 1<<3,
    };

    using TOptions = int;

    CCleanupHugeAsnReader(TOptions options);
    virtual ~CCleanupHugeAsnReader() = default;
    using TParent = edit::CHugeAsnReader;

    void FlattenGenbankSet() override;
    CRef<CSeq_entry> LoadSeqEntry(const TBioseqSetInfo& info,
            eAddTopEntry add_top_entry = eAddTopEntry::yes) const override;
    const CCleanupChangeCore& GetChanges() const;

    using TFeatId = CFeat_id::TLocal::TId;
private:
    bool x_IsExtendedCleanup() const;
    bool x_NeedsNcbiUserObject() const;
    void x_SetHooks(CObjectIStream& objStream, TContext& context) override;
    void x_SetBioseqHooks(CObjectIStream& objStream, TContext& context) override;
    void x_SetBioseqSetHooks(CObjectIStream& objStream, TContext& context) override;
    void x_SetSeqFeatHooks(CObjectIStream& objStream);

    void x_RecordFeatureId(const CFeat_id& featId);

    void x_CreateSmallGenomeSets();
    void x_PruneIfFeatsIncomplete();
    void x_PruneAndReorderTopIds();
    void x_PruneIfSegsMissing(const string& fluLabel, const set<size_t>& segsFound);

    void x_CleanupTopLevelDescriptors();
    bool x_LooksLikeNucProtSet() const;
    void x_AddTopLevelDescriptors(CSeq_entry& entry) const;
    list<CRef<CSeqdesc>> m_TopLevelBiosources;
    CRef<CSeqdesc> m_pTopLevelMolInfo;
    const TOptions m_CleanupOptions;
    mutable CCleanupChangeCore m_Changes;
    using TIdToFluLabel = map<CConstRef<CSeq_id>,string,CRefLess>;
    TIdToFluLabel m_IdToFluLabel;  
    TIdToFluLabel::iterator x_GetFluLabel(const CConstRef<CSeq_id>& pId);
    map<string, list<TBioseqSetInfo>> m_FluLabelToSetInfo;
    map<TFileSize, string> m_SetPosToFluLabel;
    set<CConstRef<CSeq_id>, CRefLess> m_HasIncompleteFeats;
    
    
    struct TFeatIdInfo {
        using TFeatIdMap = map<TFeatId, TFeatId>;
        using TPosToFeatIds = map<CHugeAsnReader::TStreamPos, TFeatIdMap>;

        set<TFeatId>  ExistingIds;
        set<TFeatId>  NewIds;
        set<TFeatId>  NewExistingIds;
        TFeatIdMap    RemappedIds;
        TFeatId       IdOffset{0};
        TPosToFeatIds PosToIdMap;
    };

    TFeatIdInfo   m_FeatIdInfo;
};

END_SCOPE(object)
END_NCBI_SCOPE

#endif  ///  HUGE_FILE_CLEANUP
