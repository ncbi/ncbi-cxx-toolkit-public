/*
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
* Author:
*
* File Description:
*
* ===========================================================================
*/

#ifndef __process_null__hpp__
#define __process_null__hpp__

#include <objtools/edit/struc_comm_field.hpp>
#include <objtools/edit/gb_block_field.hpp>
#include <objects/valid/Comment_rule.hpp>

//  ============================================================================
class CNullProcess
//  ============================================================================
    : public CSeqEntryProcess
{
public:
    //  ------------------------------------------------------------------------
    CNullProcess()
    //  ------------------------------------------------------------------------
        : CSeqEntryProcess()
    {};

    //  ------------------------------------------------------------------------
    ~CNullProcess()
    //  ------------------------------------------------------------------------
    {
    };

    //  ------------------------------------------------------------------------
    void ProcessInitialize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        CSeqEntryProcess::ProcessInitialize( args );
    };

    static bool x_UserFieldCompare (const CRef<CUser_field>& f1, const CRef<CUser_field>& f2)
    {
        if (!f1->IsSetLabel()) return true;
        if (!f2->IsSetLabel()) return false;
        return f1->GetLabel().Compare(f2->GetLabel()) < 0;
    }

    //  ------------------------------------------------------------------------
    void SeqEntryProcess()
    //  ------------------------------------------------------------------------
    {
        // empty method body
        try {

            vector<string> keywords = edit::CStructuredCommentField::GetKeywordList();
            EDIT_EACH_SEQDESC_ON_SEQENTRY ( itr, *m_entry ) {
                CSeqdesc& desc = **itr;
                if ( desc.Which() != CSeqdesc::e_Genbank ) continue;
                CGB_block& gb_block = desc.SetGenbank();
                FOR_EACH_STRING_IN_VECTOR ( s_itr, keywords ) {
                    EDIT_EACH_KEYWORD_ON_GENBANKBLOCK (k_itr, gb_block) {
                        if (NStr::EqualNocase (*k_itr, *s_itr)) {
                            ERASE_KEYWORD_ON_GENBANKBLOCK (k_itr, gb_block);
                        }
                    }
                }
                if (gb_block.IsSetKeywords() && gb_block.GetKeywords().size() == 0) {
                    gb_block.ResetKeywords();
                }
                edit::CGBBlockField gb_block_field(edit::CGBBlockField::eGBBlockFieldType_Keyword);
                if (gb_block_field.IsEmpty(desc)) {
                    ERASE_SEQDESC_ON_SEQENTRY ( itr, *m_entry );
                }
            }

            vector<string> new_keywords;
            FOR_EACH_SEQDESC_ON_SEQENTRY ( itr, *m_entry ) {
                const CSeqdesc& desc = **itr;
                if ( desc.Which() != CSeqdesc::e_User ) continue;
                const CUser_object& usr = desc.GetUser();
                if ( ! edit::CStructuredCommentField::IsStructuredComment (usr) ) continue;
                try {
                    string prefix = edit::CStructuredCommentField::GetPrefix (usr);
                    CConstRef<CComment_set> comment_rules = CComment_set::GetCommentRules();
                    try {
                        const CComment_rule& rule = comment_rules->FindCommentRule(prefix);
                        CUser_object tmp;
                        tmp.Assign(usr);
                        CUser_object::TData& fields = tmp.SetData();
                        if (! rule.GetRequire_order()) {
                            stable_sort (fields.begin(), fields.end(), x_UserFieldCompare);
                        }
                        CComment_rule::TErrorList errors = rule.IsValid(tmp);
                        if (errors.size() == 0) {
                            string kywd = edit::CStructuredCommentField::KeywordForPrefix( prefix );
                            new_keywords.push_back(kywd);
                        }
                    } catch (CException) {
                    }
                } catch (CException) {
                }
            }
            if (new_keywords.size() > 0) {
                CGB_block *gb_block;
                EDIT_EACH_SEQDESC_ON_SEQENTRY ( itr, *m_entry ) {
                    CSeqdesc& desc = **itr;
                    if ( desc.Which() != CSeqdesc::e_Genbank ) continue;
                    gb_block = &desc.SetGenbank();
                }
                if (! gb_block) {
                    CRef<CSeqdesc> new_desc ( new CSeqdesc );
                    new_desc->SetGenbank();
                    m_entry->SetDescr().Set().push_back( new_desc );
                }
                EDIT_EACH_SEQDESC_ON_SEQENTRY ( itr, *m_entry ) {
                    CSeqdesc& desc = **itr;
                    if ( desc.Which() != CSeqdesc::e_Genbank ) continue;
                    CGB_block& gb_block = desc.SetGenbank();
                    FOR_EACH_STRING_IN_VECTOR ( n_itr, new_keywords ) {
                        ADD_KEYWORD_TO_GENBANKBLOCK (gb_block, *n_itr);
                    }
                }
            }

        }
        catch (CException& e) {
            LOG_POST(Error << "error processing seqentry: " << e.what());
        }
    };

};

#endif
