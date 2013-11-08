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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/align/util/algo_align_util_exceptions.hpp>
#include <algo/align/util/align_filter.hpp>
#include <algo/align/util/score_builder.hpp>
#include <corelib/rwstream.hpp>

#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/PIR_block.hpp>
#include <objects/seqblock/SP_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqblock/PRF_block.hpp>

#include <util/checksum.hpp>
#include <math.h>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


///////////////////////////////////////////////////////////////////////////////

void CAlignFilter::x_ParseTree_Flatten(CQueryParseTree& tree,
                                       CQueryParseTree::TNode& node)
{
    CQueryParseNode::EType type = node->GetType();
    switch (type) {
    case CQueryParseNode::eAnd:
    case CQueryParseNode::eOr:
        {{
             CQueryParseTree::TNode::TNodeList_I iter;
             size_t hoisted = 0;
             size_t count_by_complexity[] = {0, 0};
             do {
                 hoisted = 0;
                 for (iter = node.SubNodeBegin();
                      iter != node.SubNodeEnd();  ) {
                     CQueryParseTree::TNode& sub_node = **iter;
                     ++count_by_complexity[x_Complexity(sub_node)];
                     if (sub_node->GetType() == type) {
                         /// hoist this node's children
                         CQueryParseTree::TNode::TNodeList_I sub_iter =
                             sub_node.SubNodeBegin();
                         for ( ;  sub_iter != sub_node.SubNodeEnd(); ) {
                             node.AddNode(sub_node.DetachNode(*sub_iter++));
                         }

                         node.RemoveNode(iter++);
                         ++hoisted;
                     } else {
                         ++iter;
                     }
                 }
             }
             while (hoisted != 0);

             if (count_by_complexity[CScoreLookup::IScore::eEasy] &&
                 count_by_complexity[CScoreLookup::IScore::eHard])
             {
                 /// Have both easy and hard subnodes; move hard nodes to end
                 CQueryParseTree::TNode::TNodeList hard_nodes;
                 for (iter = node.SubNodeBegin(); iter != node.SubNodeEnd(); )
                 {
                     if (x_Complexity(**iter) == CScoreLookup::IScore::eHard) {
                         hard_nodes.push_back(node.DetachNode(*iter++));
                     } else {
                         ++iter;
                     }
                 }

                 ITERATE (CQueryParseTree::TNode::TNodeList, it, hard_nodes) {
                     node.AddNode(*it);
                 }
             }
         }}
        break;

    default:
        break;
    }

    CQueryParseTree::TNode::TNodeList_I iter;
    for (iter = node.SubNodeBegin();
         iter != node.SubNodeEnd();  ++iter) {
        x_ParseTree_Flatten(tree, **iter);
    }
}



//////////////////////////////////////////////////////////////////////////////

CAlignFilter::CAlignFilter()
    : m_RemoveDuplicates(false)
    , m_IsDryRun(false)
{
}


CAlignFilter::CAlignFilter(const string& query)
    : m_RemoveDuplicates(false)
    , m_IsDryRun(false)
{
    SetFilter(query);
}


void CAlignFilter::SetFilter(const string& filter)
{
    static const char *sc_Functions[] = {
        "MUL",
        "DIV",
        "ADD",
        "SUB",
        "IS_SEG_TYPE",
        "COALESCE",
        "HAS_DESC_KEYWORD",
        NULL
    };

    m_Query = filter;
    m_ParseTree.reset(new CQueryParseTree);

    vector<string> func_vec;
    for (const char** func = sc_Functions;  func  &&  *func;  ++func) {
        func_vec.push_back(*func);
    }

    m_ParseTree->Parse(m_Query.c_str(),
                       CQueryParseTree::eCaseInsensitive,
                       CQueryParseTree::eSyntaxCheck, false,
                       func_vec);

    // flatten the tree
    // this transforms the tree so that equivalent nodes are grouped more
    // effectively.  this grouping permist easier tree evaluation
    x_ParseTree_Flatten(*m_ParseTree, *m_ParseTree->GetQueryTree());

    m_Scope.Reset(new CScope(*CObjectManager::GetInstance()));
    m_Scope->AddDefaults();

    /// Dry run to null output, to get set of computed tokens in filter
#ifdef NCBI_OS_UNIX
    CNcbiOfstream null_output("/dev/null");
#else
    string scratch_file = CFile::GetTmpName(CFile::eTmpFileCreate);
    CNcbiOfstream null_output(scratch_file.c_str());
#endif
    DryRun(null_output);
}

void CAlignFilter::SetScope(CScope& scope)
{
    m_Scope.Reset(&scope);
    m_ScoreLookup.SetScope(scope);
}


void CAlignFilter::AddBlacklistQueryId(const CSeq_id_Handle& idh)
{
    m_QueryBlacklist.insert(idh);
}


void CAlignFilter::AddWhitelistQueryId(const CSeq_id_Handle& idh)
{
    m_QueryWhitelist.insert(idh);
}

void CAlignFilter::AddExcludeNotInQueryId(const CSeq_id_Handle& idh)
{
    m_QueryExcludeNotIn.insert(idh);
}


void CAlignFilter::AddBlacklistSubjectId(const CSeq_id_Handle& idh)
{
    m_SubjectBlacklist.insert(idh);
}


void CAlignFilter::AddWhitelistSubjectId(const CSeq_id_Handle& idh)
{
    m_SubjectWhitelist.insert(idh);
}

void CAlignFilter::AddExcludeNotInSubjectId(const CSeq_id_Handle& idh)
{
    m_SubjectExcludeNotIn.insert(idh);
}


CAlignFilter& CAlignFilter::SetRemoveDuplicates(bool b)
{
    m_RemoveDuplicates = b;
    return *this;
}


void CAlignFilter::Filter(const list< CRef<CSeq_align> >& aligns_in,
                          list< CRef<CSeq_align> >& aligns_out)
{
    ITERATE (list< CRef<CSeq_align> >, iter, aligns_in) {
        if (Match(**iter)) {
            aligns_out.push_back(*iter);
        }
    }
}


void CAlignFilter::Filter(const CSeq_align_set& aligns_in,
                          CSeq_align_set&       aligns_out)
{
    Filter(aligns_in.Get(), aligns_out.Set());
}


void CAlignFilter::Filter(const CSeq_annot& aligns_in,
                          CSeq_annot&       aligns_out)
{
    Filter(aligns_in.GetData().GetAlign(), aligns_out.SetData().SetAlign());
}


bool CAlignFilter::Match(const CSeq_align& align)
{
    if (align.CheckNumRows() == 2) {
        if (m_QueryBlacklist.size()  ||  m_QueryWhitelist.size()
                                     ||  m_QueryExcludeNotIn.size())
        {
            CSeq_id_Handle query =
                CSeq_id_Handle::GetHandle(align.GetSeq_id(0));
            if (m_Scope) {
                CSeq_id_Handle idh =
                    sequence::GetId(query, *m_Scope,
                                    sequence::eGetId_Canonical);
                if (idh) {
                    query = idh;
                }
            }
            if (m_QueryBlacklist.size()  &&
                m_QueryBlacklist.find(query) != m_QueryBlacklist.end()) {
                /// reject: query sequence found in black list
                return false;
            }

            if (m_QueryExcludeNotIn.size()  &&
                m_QueryExcludeNotIn.find(query) == m_QueryExcludeNotIn.end()) {
                /// reject: query sequence not found in exclude-not-in list
                return false;
            }

            if (m_QueryWhitelist.size()  &&
                m_QueryWhitelist.find(query) != m_QueryWhitelist.end()) {
                /// accept: query sequence found in white list
                m_ScoreLookup.UpdateState(align);
                return true;
            }
        }

        if (m_SubjectBlacklist.size()  ||  m_SubjectWhitelist.size()
                                       ||  m_SubjectExcludeNotIn.size())
        {
            CSeq_id_Handle subject =
                CSeq_id_Handle::GetHandle(align.GetSeq_id(1));
            if (m_Scope) {
                CSeq_id_Handle idh =
                    sequence::GetId(subject, *m_Scope,
                                    sequence::eGetId_Canonical);
                if (idh) {
                    subject = idh;
                }
            }
            if (m_SubjectBlacklist.size()  &&
                m_SubjectBlacklist.find(subject) != m_SubjectBlacklist.end()) {
                /// reject: subject sequence found in black list
                return false;
            }

            if (m_SubjectExcludeNotIn.size()  &&
                m_SubjectExcludeNotIn.find(subject) == m_SubjectExcludeNotIn.end()) {
                /// reject: query sequence not found in exclude-not-in list
                return false;
            }

            if (m_SubjectWhitelist.size()  &&
                m_SubjectWhitelist.find(subject) != m_SubjectWhitelist.end()) {
                /// accept: subject sequence found in white list
                m_ScoreLookup.UpdateState(align);
                return true;
            }
        }
    }

    /// check to see if scores match
    bool match = true;
    if (m_ParseTree.get()) {
        match = x_Match(*m_ParseTree->GetQueryTree(), align);
        if (match) {
            m_ScoreLookup.UpdateState(align);
        }
    } else {
        if (m_QueryWhitelist.size()  ||  m_SubjectWhitelist.size()) {
            /// the user supplied inclusion criteria but no filter
            /// inclusion failed - return false
            return false;
        }
        else if (m_QueryBlacklist.size()  ||  m_SubjectBlacklist.size() ||
                 m_QueryExcludeNotIn.size()  ||  m_SubjectExcludeNotIn.size()) {
            /// the user supplied exclusion criteria but no filter
            /// exclusion failed - return true
            return true;
        }
    }

    return (match  &&  ( !m_RemoveDuplicates  ||  x_IsUnique(align) ) );
}

void CAlignFilter::PrintDictionary(CNcbiOstream &ostr)
{
    m_ScoreLookup.PrintDictionary(ostr);
}

void CAlignFilter::DryRun(CNcbiOstream &ostr) {
    ostr << "Parse Tree:" << endl;
    m_ParseTree->Print(ostr);
    ostr << endl;

    m_IsDryRun = true;
    m_DryRunOutput = &ostr;
    CSeq_align dummy_alignment;
    x_Match(*m_ParseTree->GetQueryTree(), dummy_alignment);
    m_IsDryRun = false;
}

bool CAlignFilter::x_IsUnique(const CSeq_align& align)
{
    CChecksumStreamWriter md5(CChecksum::eMD5);
    {{
        CWStream wstr(&md5);
        wstr << MSerial_AsnBinary << align.GetSegs();
     }}

    string md5_str;
    md5.GetChecksum().GetMD5Digest(md5_str);
    return m_UniqueAligns.insert(md5_str).second;
}

double CAlignFilter::x_GetAlignmentScore(const string& score_name,
                                         const objects::CSeq_align& align,
                                         bool throw_if_not_found)
{
    ///
    /// see if we have this score
    ///
    if (m_IsDryRun) {
        (*m_DryRunOutput) << score_name << ": "
                          << m_ScoreLookup.HelpText(score_name) << endl;
        return 0;
    }

    try {
        return m_ScoreLookup.GetScore(align, score_name);
    } catch (CException &e) {
        if( throw_if_not_found)
        {
            throw;
        }
        LOG_POST(Warning << "score " << score_name << " not found");
        LOG_POST(Info << e);
        return numeric_limits<double>::quiet_NaN();
    }
}

double CAlignFilter::x_FuncCall(const CQueryParseTree::TNode& node, const CSeq_align& align)
{
    double this_val = numeric_limits<double>::quiet_NaN();
    string function = node.GetValue().GetStrValue();
    if (NStr::EqualNocase(function, "MUL")) {
        CQueryParseTree::TNode::TNodeList_CI iter =
            node.SubNodeBegin();
        CQueryParseTree::TNode::TNodeList_CI end =
            node.SubNodeEnd();
        const CQueryParseTree::TNode& node1 = **iter;
        ++iter;
        if (iter == end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: expected 2, got 1");
        }
        const CQueryParseTree::TNode& node2 = **iter;
        ++iter;
        if (iter != end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: "
                       "expected 2, got more than 2");
        }

        double val1 = x_TermValue(node1, align);
        double val2 = x_TermValue(node2, align);

        this_val = val1 * val2;

    }
    else if (NStr::EqualNocase(function, "DIV")) {
        CQueryParseTree::TNode::TNodeList_CI iter =
            node.SubNodeBegin();
        CQueryParseTree::TNode::TNodeList_CI end =
            node.SubNodeEnd();
        const CQueryParseTree::TNode& node1 = **iter;
        ++iter;
        if (iter == end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: expected 2, got 1");
        }
        const CQueryParseTree::TNode& node2 = **iter;
        ++iter;
        if (iter != end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: "
                       "expected 2, got more than 2");
        }

        double val1 = x_TermValue(node1, align);
        double val2 = x_TermValue(node2, align);

        this_val = val1 / val2;
    }
    else if (NStr::EqualNocase(function, "ADD")) {
        CQueryParseTree::TNode::TNodeList_CI iter =
            node.SubNodeBegin();
        CQueryParseTree::TNode::TNodeList_CI end =
            node.SubNodeEnd();
        const CQueryParseTree::TNode& node1 = **iter;
        ++iter;
        if (iter == end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: expected 2, got 1");
        }
        const CQueryParseTree::TNode& node2 = **iter;
        ++iter;
        if (iter != end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: "
                       "expected 2, got more than 2");
        }

        double val1 = x_TermValue(node1, align);
        double val2 = x_TermValue(node2, align);

        this_val = val1 + val2;
    }
    else if (NStr::EqualNocase(function, "SUB")) {
        CQueryParseTree::TNode::TNodeList_CI iter =
            node.SubNodeBegin();
        CQueryParseTree::TNode::TNodeList_CI end =
            node.SubNodeEnd();
        const CQueryParseTree::TNode& node1 = **iter;
        ++iter;
        if (iter == end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: expected 2, got 1");
        }
        const CQueryParseTree::TNode& node2 = **iter;
        ++iter;
        if (iter != end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: "
                       "expected 2, got more than 2");
        }

        double val1 = x_TermValue(node1, align);
        double val2 = x_TermValue(node2, align);

        this_val = val1 - val2;
    }
    else if (NStr::EqualNocase(function, "COALESCE")) {
        ///
        /// standard SQL coalesce-if-null
        ///
        CQueryParseTree::TNode::TNodeList_CI iter =
            node.SubNodeBegin();
        CQueryParseTree::TNode::TNodeList_CI end =
            node.SubNodeEnd();
        for ( ;  iter != end;  ++iter) {
            try {
                this_val = x_TermValue(**iter, align, true);
                break;
            }
            catch (CException&) {
                /// In a real run, any exception we get are likely to be from terms not
                /// found, and should be ignored. In a dry run, if we get an exception it
                /// must be the result of some other problem which has to be reported
                if (m_IsDryRun) {
                    throw;
                }
            }
        }
    }
    else if (NStr::EqualNocase(function, "IS_SEG_TYPE")) {
        CQueryParseTree::TNode::TNodeList_CI iter =
            node.SubNodeBegin();
        CQueryParseTree::TNode::TNodeList_CI end =
            node.SubNodeEnd();
        const CQueryParseTree::TNode& node1 = **iter;
        ++iter;
        if (iter != end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: expected 1, got more than 1");
        }

        /// this is a bit different - we expect a string type here
        if (node1.GetValue().GetType() != CQueryParseNode::eString) {
            NCBI_THROW(CException, eUnknown,
                       "invalid seg type - expected string");
        }
        const string& s = node1.GetValue().GetStrValue();
        if (NStr::EqualNocase(s, "denseg")) {
            if (!m_IsDryRun) {
                this_val = align.GetSegs().IsDenseg();
            }
        }
        else if (NStr::EqualNocase(s, "spliced")) {
            if (!m_IsDryRun) {
                this_val = align.GetSegs().IsSpliced();
            }
        }
        else if (NStr::EqualNocase(s, "disc")) {
            if (!m_IsDryRun) {
                this_val = align.GetSegs().IsDisc();
            }
        }
        else if (NStr::EqualNocase(s, "std")) {
            if (!m_IsDryRun) {
                this_val = align.GetSegs().IsStd();
            }
        }
        else if (NStr::EqualNocase(s, "sparse")) {
            if (!m_IsDryRun) {
                this_val = align.GetSegs().IsSparse();
            }
        }
        else if (NStr::EqualNocase(s, "dendiag")) {
            if (!m_IsDryRun) {
                this_val = align.GetSegs().IsDendiag();
            }
        }
        else if (NStr::EqualNocase(s, "packed")) {
            if (!m_IsDryRun) {
                this_val = align.GetSegs().IsPacked();
            }
        }
        else {
            NCBI_THROW(CException, eUnknown,
                       "invalid seg type: " + s);
        }
    }
    else if (NStr::EqualNocase(function, "HAS_DESC_KEYWORD")) {
        CQueryParseTree::TNode::TNodeList_CI iter =
            node.SubNodeBegin();
        CQueryParseTree::TNode::TNodeList_CI end =
            node.SubNodeEnd();
        const CQueryParseTree::TNode& node1 = **iter;
        const string& which = node1.GetValue().GetStrValue();
        if (which != "query" && which != "subject") {
            NCBI_THROW(CException, eUnknown,
                       "Invalid first argument to HAS_DESC_KEYWORD; must be "
                       "either 'query' or 'subject'");
        }

        ++iter;
        if (iter == end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: expected 2, got 1");
        }
        const CQueryParseTree::TNode& node2 = **iter;
        const string& keyword = node2.GetValue().GetStrValue();
        ++iter;
        if (iter != end) {
            NCBI_THROW(CException, eUnknown,
                       "invalid number of nodes: "
                       "expected 2, got more than 2");
        }

        const CSeq_id &seq_id = align.GetSeq_id(
            NStr::EqualNocase(which, "query") ? 0 : 1);
        CSeq_id_Handle idh =
            sequence::GetId(seq_id, *m_Scope,
                            sequence::eGetId_Best);
        bool found_keyword = false;
        CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq_id);
        for (CSeqdesc_CI desc_ci(bsh);
             desc_ci && !found_keyword; ++desc_ci)
        {
            const CGB_block::TKeywords *keywords = NULL;
            switch (desc_ci->Which()) {
            case CSeqdesc::e_Genbank:
                if (desc_ci->GetGenbank().IsSetKeywords()) {
                    keywords = &desc_ci->GetGenbank().GetKeywords();
                }
                break;

            case CSeqdesc::e_Pir:
                if (desc_ci->GetPir().IsSetKeywords()) {
                    keywords = &desc_ci->GetPir().GetKeywords();
                }
                break;

            case CSeqdesc::e_Sp:
                if (desc_ci->GetSp().IsSetKeywords()) {
                    keywords = &desc_ci->GetSp().GetKeywords();
                }
                break;

            case CSeqdesc::e_Embl:
                if (desc_ci->GetEmbl().IsSetKeywords()) {
                    keywords = &desc_ci->GetEmbl().GetKeywords();
                }
                break;

            case CSeqdesc::e_Prf:
                if (desc_ci->GetPrf().IsSetKeywords()) {
                    keywords = &desc_ci->GetPrf().GetKeywords();
                }
                break;

            default:
                break;
            }

            if (keywords) {
                ITERATE (CGB_block::TKeywords, keyword_it, *keywords) {
                    if (NStr::EqualNocase(keyword, *keyword_it)) {
                        found_keyword = true;
                        break;
                    }
                }
            }
        }
    
        return found_keyword ? 1 : 0;
    }
    else {
        NCBI_THROW(CException, eUnknown,
                   "function not understood: " + function);
    }

    return this_val;
}


static bool s_IsDouble(const string& str)
{
    ITERATE(string, iter, str) {
        if( !isdigit(*iter) &&
		    *iter != '+' &&
			*iter != '-' &&
			*iter != '.' &&
			*iter != ' ') {
            return false;
	    }
	}
	return true;
}

double CAlignFilter::x_TermValue(const CQueryParseTree::TNode& term_node,
                                 const CSeq_align& align,
                                 bool throw_if_not_found)
{
    CQueryParseNode::EType type = term_node.GetValue().GetType();
    switch (type) {
    case CQueryParseNode::eIntConst:
        return term_node.GetValue().GetInt();
    case CQueryParseNode::eFloatConst:
        return term_node.GetValue().GetDouble();
    case CQueryParseNode::eString:
        {{
             string str = term_node.GetValue().GetStrValue();
             double val;
			 if(s_IsDouble(str)) {
			     try {
			         val = NStr::StringToDouble(str);
                 }
                 catch (CException&) {
                     val = x_GetAlignmentScore(str, align, throw_if_not_found);
				 }
		     } else {
                 val = x_GetAlignmentScore(str, align, throw_if_not_found);
			 }
             return val;
         }}
    case CQueryParseNode::eFunction:
        return x_FuncCall(term_node, align);
    default:
        NCBI_THROW(CException, eUnknown,
                   "unexpected expression");
    }
}


bool CAlignFilter::x_Query_Op(const CQueryParseTree::TNode& l_node,
                              CQueryParseNode::EType type,
                              bool is_not,
                              const CQueryParseTree::TNode& r_node,
                              const CSeq_align& align)
{
    ///
    /// screen for simple sequence and strand match first
    ///
    if (l_node.GetValue().GetType() == CQueryParseNode::eString) {
        string s = l_node.GetValue().GetStrValue();
        if (NStr::EqualNocase(s, "query")  ||
            NStr::EqualNocase(s, "subject")) {

            string val = r_node.GetValue().GetStrValue();
            bool constant_seqid;
            CSeq_id_Handle idh;
            if (NStr::EqualNocase(val, "query")) {
                if (!m_IsDryRun) {
                    idh = CSeq_id_Handle::GetHandle(align.GetSeq_id(0));
                }
                constant_seqid = false;
            } else if (NStr::EqualNocase(val, "subject")) {
                if (!m_IsDryRun) {
                    idh = CSeq_id_Handle::GetHandle(align.GetSeq_id(1));
                }
                constant_seqid = false;
            } else {
                idh = CSeq_id_Handle::GetHandle(val);
                constant_seqid = true;
            }

            if (m_IsDryRun) {
                if (constant_seqid) {
                    (*m_DryRunOutput) << val << ": SeqId, ";
                    CConstRef<CSynonymsSet> syns = m_Scope->GetSynonyms(idh);
                    if (!syns) {
                        (*m_DryRunOutput) << "No synonyms";
                    } else {
                        (*m_DryRunOutput) << "synonyms ";
                        ITERATE (CSynonymsSet::TIdSet, syn_it, *syns) {
                            if (syn_it != syns->begin()) {
                                (*m_DryRunOutput) << ",";
                            }
                            (*m_DryRunOutput) << (*syn_it)->first;
                        }
                    }
                    (*m_DryRunOutput) << endl;
                }
                return false;
            } else {
                CSeq_id_Handle other_idh;
                if (NStr::EqualNocase(s, "query")) {
                    other_idh = CSeq_id_Handle::GetHandle(align.GetSeq_id(0));
                } else {
                    other_idh = CSeq_id_Handle::GetHandle(align.GetSeq_id(1));
                }
    
                switch (type) {
                case CQueryParseNode::eEQ:
                    if (other_idh == idh) {
                        return !is_not;
                    }
                    break;

                case CQueryParseNode::eLT:
                    return ((other_idh.AsString() <  idh.AsString())
                                ==  !is_not);

                case CQueryParseNode::eLE:
                    return ((other_idh.AsString() <= idh.AsString())
                                ==  !is_not);

                case CQueryParseNode::eGT:
                    return ((other_idh.AsString() >  idh.AsString())
                                ==  !is_not);

                case CQueryParseNode::eGE:
                    return ((other_idh.AsString() >= idh.AsString())
                                ==  !is_not);

                default:
                    LOG_POST(Warning << "unhandled parse node in expression");
                    break;
                }

                NCBI_ASSERT(type == CQueryParseNode::eEQ,
                            "Should reach this point only with equality");

                CConstRef<CSynonymsSet> syns = m_Scope->GetSynonyms(idh);
                if ( !syns  ||  syns->empty() ) {
                    return false;
                }
                return (syns->ContainsSynonym(other_idh) == !is_not);
            }
        } else if (NStr::EqualNocase(s, "query_strand")  ||
                   NStr::EqualNocase(s, "subject_strand")) {
            string val = r_node.GetValue().GetStrValue();
            ENa_strand strand = eNa_strand_unknown;
            if (NStr::EqualNocase(val, "query_strand")) {
                if (!m_IsDryRun) {
                    strand = align.GetSeqStrand(0);
                }
            } else if (NStr::EqualNocase(val, "subject_strand")) {
                if (!m_IsDryRun) {
                    strand = align.GetSeqStrand(1);
                }
            } else if (val.size() != 1) {
                NCBI_THROW(CException, eUnknown,
                           "Valid strand values are '+', '-', 'b' and '?'");
                
            } else {
                switch (val[0]) {
                case '+':
                    strand = eNa_strand_plus;
                    break;

                case '-':
                    strand = eNa_strand_minus;
                    break;

                case 'b':
                    strand = eNa_strand_both;
                    break;

                case '?':
                    strand = eNa_strand_unknown;
                    break;

                default:
                    NCBI_THROW(CException, eUnknown,
                               "Valid strand values are '+', '-, 'b'' and '?'");
                }
            }

            if (m_IsDryRun) {
                return false;
            } else {
                ENa_strand other_strand;
                if (NStr::EqualNocase(s, "query_strand")) {
                    other_strand = align.GetSeqStrand(0);
                } else {
                    other_strand = align.GetSeqStrand(1);
                }
    
                if (type == CQueryParseNode::eEQ) {
                    return other_strand == strand ? !is_not : is_not;
                } else {
                    NCBI_THROW(CException, eUnknown,
                               "unhandled parse node in expression");
                }
            }
        } else if (NStr::EqualNocase(s, "query_refseq_status")  ||
                   NStr::EqualNocase(s, "subject_refseq_status")) {
            string val = r_node.GetValue().GetStrValue();

            if (m_IsDryRun) {
                return false;
            } else {
                const CSeq_id &seq_id = align.GetSeq_id(
                    NStr::EqualNocase(s, "query_refseq_status") ? 0 : 1);
                CSeq_id_Handle idh =
                    sequence::GetId(seq_id, *m_Scope,
                                    sequence::eGetId_Best);
                if (!idh.GetSeqId()->IsOther()) {
                    /// Not RefSeq
                    return is_not;
                }
                string status_string = "MODEL";
                CBioseq_Handle bsh = m_Scope->GetBioseqHandle(seq_id);
                for (CSeqdesc_CI desc_ci(bsh, CSeqdesc::e_User); desc_ci; ++desc_ci) {
                     const CUser_object &uo = desc_ci->GetUser();
                     if (uo.GetType().IsStr() &&
                         uo.GetType().GetStr() == "RefGeneTracking")
                     {
                         status_string =
                            uo.GetField("Status").GetData().GetStr();
                         break;
                     }
                }
    
                if (type == CQueryParseNode::eEQ) {
                    return NStr::EqualNocase(val, status_string)
                        ? !is_not : is_not;
                } else {
                    NCBI_THROW(CException, eUnknown,
                               "unhandled parse node in expression");
                }
            }
        }
    }

    /// fall-through: not query or subject

    double l_val = x_TermValue(l_node, align);
    double r_val = x_TermValue(r_node, align);

    if (isnan(l_val) || isnan(r_val)) {
        return false;
    }

    switch (type) {
    case CQueryParseNode::eEQ:
        return ((l_val == r_val)  ==  !is_not);

    case CQueryParseNode::eLT:
        return ((l_val <  r_val)  ==  !is_not);

    case CQueryParseNode::eLE:
        return ((l_val <= r_val)  ==  !is_not);

    case CQueryParseNode::eGT:
        return ((l_val >  r_val)  ==  !is_not);

    case CQueryParseNode::eGE:
        return ((l_val >= r_val)  ==  !is_not);

    default:
        LOG_POST(Warning << "unhandled parse node in expression");
        break;
    }

    return false;
}


bool CAlignFilter::x_Query_Range(const CQueryParseTree::TNode& key_node,
                                 bool is_not,
                                 const CQueryParseTree::TNode& val1_node,
                                 const CQueryParseTree::TNode& val2_node,
                                 const CSeq_align& align)
{
    double this_val = x_TermValue(key_node, align);
    double val1 = x_TermValue(val1_node, align);
    double val2 = x_TermValue(val2_node, align);
    if (val1 > val2) {
        swap(val1, val2);
    }

    if ( !isnan(this_val)  &&  !isnan(val1)  &&  !isnan(val2)) {
        bool between = (val1 <= this_val)  &&  (this_val <= val2);
        return (between == !is_not);
    }

    return false;
}

CScoreLookup::IScore::EComplexity
CAlignFilter::x_Complexity(const CQueryParseTree::TNode& node)
{
    if (node.GetValue().GetType() == CQueryParseNode::eString) {
        try {
            return m_ScoreLookup.Complexity(node.GetValue().GetStrValue());
        } catch (CAlgoAlignUtilException &) {
            /// Score not found, so this is an align_filter's built-in string
            return CScoreLookup::IScore::eEasy;
        }
    }

    for (CQueryParseTree::TNode::TNodeList_CI iter = node.SubNodeBegin();
         iter != node.SubNodeEnd(); ++iter)
    {
        if (x_Complexity(**iter) == CScoreLookup::IScore::eHard) {
            return CScoreLookup::IScore::eHard;
        }
    }

    /// None of the subnodes are hard
    return CScoreLookup::IScore::eEasy;
}


bool CAlignFilter::x_Match(const CQueryParseTree::TNode& node,
                           const CSeq_align& align)
{
    switch (node.GetValue().GetType()) {
    case CQueryParseNode::eEQ:
    case CQueryParseNode::eGT:
    case CQueryParseNode::eGE:
    case CQueryParseNode::eLT:
    case CQueryParseNode::eLE:
        {{
             CQueryParseTree::TNode::TNodeList_CI iter =
                 node.SubNodeBegin();
             CQueryParseTree::TNode::TNodeList_CI end =
                 node.SubNodeEnd();
             const CQueryParseTree::TNode& node1 = **iter;
             ++iter;
             if (iter == end) {
                 NCBI_THROW(CException, eUnknown,
                            "invalid number of nodes: expected 2, got 1");
             }
             const CQueryParseTree::TNode& node2 = **iter;
             ++iter;
             if (iter != end) {
                 NCBI_THROW(CException, eUnknown,
                            "invalid number of nodes: "
                            "expected 2, got more than 2");
             }

             return x_Query_Op(node1,
                               node.GetValue().GetType(),
                               node.GetValue().IsNot(),
                               node2, align);
         }}
        break;

    case CQueryParseNode::eBetween:
        {{
             CQueryParseTree::TNode::TNodeList_CI iter =
                 node.SubNodeBegin();
             CQueryParseTree::TNode::TNodeList_CI end =
                 node.SubNodeEnd();
             const CQueryParseTree::TNode& node1 = **iter;
             ++iter;
             if (iter == end) {
                 NCBI_THROW(CException, eUnknown,
                            "invalid number of nodes: expected 3, got 1");
             }
             const CQueryParseTree::TNode& node2 = **iter;
             ++iter;
             if (iter == end) {
                 NCBI_THROW(CException, eUnknown,
                            "invalid number of nodes: "
                            "expected 3, got 2");
             }

             const CQueryParseTree::TNode& node3 = **iter;
             ++iter;
             if (iter != end) {
                 NCBI_THROW(CException, eUnknown,
                            "invalid number of nodes: "
                            "expected 3, got more than 3");
             }

             if (node1.GetValue().GetType() != CQueryParseNode::eString) {
                 NCBI_THROW(CException, eUnknown,
                            "unexpected expression");
             }
             return x_Query_Range(node1,
                                  node.GetValue().IsNot(),
                                  node2, node3, align);
         }}
        break;

    case CQueryParseNode::eAnd:
        {{
             bool res = false;
             CQueryParseTree::TNode::TNodeList_CI iter =
                 node.SubNodeBegin();
             if (iter != node.SubNodeEnd()) {
                 res = x_Match(**iter, align);
                 ++iter;
             }
             /// In a real run, use short-circuit logic; in a dry run, always interpret
             /// both sides
             for ( ;  iter != node.SubNodeEnd()  &&
                      (res || m_IsDryRun);
                      ++iter)
             {
                 res &= x_Match(**iter, align);
             }

             return res;
         }}

    case CQueryParseNode::eNot:
        {{
             bool res = false;
             CQueryParseTree::TNode::TNodeList_CI iter =
                 node.SubNodeBegin();
             if (iter != node.SubNodeEnd()) {
                 res = !x_Match(**iter, align);
             }

             return res;
         }}

    case CQueryParseNode::eOr:
        {{
             bool res = false;
             CQueryParseTree::TNode::TNodeList_CI iter =
                 node.SubNodeBegin();
             if (iter != node.SubNodeEnd()) {
                 res = x_Match(**iter, align);
                 ++iter;
             }
             /// In a real run, use short-circuit logic; in a dry run, always interpret
             /// both sides
             for ( ;  iter != node.SubNodeEnd()  &&
                      (!res || m_IsDryRun);
                      ++iter)
             {
                 res |= x_Match(**iter, align);
             }

             return res;
         }}


    default:
        try {
            return x_TermValue(node, align);
        } catch (CException &e) {
        }
    }

    return false;
}



END_NCBI_SCOPE

