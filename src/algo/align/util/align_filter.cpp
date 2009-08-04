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
#include <gpipe/common/align_filter.hpp>
#include <gpipe/common/buffer_writer.hpp>

#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <util/checksum.hpp>
#include <math.h>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class CMD5StreamWriter : public IWriter
{
public:
    CMD5StreamWriter()
        : m_Checksum(CChecksum::eMD5)
        {
        }

    virtual ERW_Result Write(const void* buf,
                             size_t count,
                             size_t* bytes_written = 0)
    {
        if (bytes_written) {
            *bytes_written = count;
        }

        m_Checksum.AddChars((const char*)buf, count);
        return eRW_Success;
    }

    virtual ERW_Result Flush()
    {
        return eRW_Success;
    }

    void GetMD5Sum(string& s) const
    {
        unsigned char buf[16];
        m_Checksum.GetMD5Digest(buf);

        s.clear();
        s.insert(s.end(), (const char*)buf, (const char*)buf + 16);
    }

private:
    CChecksum m_Checksum;
};

static
char *functions[] = { "MUL", NULL };

/////////////////////////////////////////////////////////////////////////////

CAlignFilter::CAlignFilter(const string& query)
    : m_RemoveDuplicates(false)
    , m_Query(query)
{
    m_ParseTree.reset(new CQueryParseTree);

    vector<string> func_vec;
    for (char** func = functions; *func; ++func) {
        func_vec.push_back(*func);
    }

    m_ParseTree->Parse(query.c_str(),
                       CQueryParseTree::eCaseInsensitive,
                       CQueryParseTree::eSyntaxCheck, false,
                       func_vec);

    m_Scope.Reset(new CScope(*CObjectManager::GetInstance()));
    m_Scope->AddDefaults();
}

void CAlignFilter::SetScope(CScope& scope)
{
    m_Scope.Reset(&scope);
}


CScope& CAlignFilter::SetScope()
{
    return *m_Scope;
}

void CAlignFilter::AddBlacklistQueryId(const CSeq_id_Handle& idh)
{
    m_QueryBlacklist.insert(idh);
}


void CAlignFilter::AddWhitelistQueryId(const CSeq_id_Handle& idh)
{
    m_QueryWhitelist.insert(idh);
}


void CAlignFilter::AddBlacklistSubjectId(const CSeq_id_Handle& idh)
{
    m_SubjectBlacklist.insert(idh);
}


void CAlignFilter::AddWhitelistSubjectId(const CSeq_id_Handle& idh)
{
    m_SubjectWhitelist.insert(idh);
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
        CSeq_id_Handle query =
            CSeq_id_Handle::GetHandle(align.GetSeq_id(0));
        query = sequence::GetId(query, *m_Scope,
                                sequence::eGetId_Canonical);
        if (m_QueryBlacklist.size()  &&
            m_QueryBlacklist.find(query) != m_QueryBlacklist.end()) {
            /// reject: query sequence found in black list
            return false;
        }

        if (m_QueryWhitelist.size()  &&
            m_QueryWhitelist.find(query) != m_QueryWhitelist.end()) {
            /// accept: query sequence found in white list
            return true;
        }

        CSeq_id_Handle subject =
            CSeq_id_Handle::GetHandle(align.GetSeq_id(1));
        subject = sequence::GetId(subject, *m_Scope,
                                sequence::eGetId_Canonical);
        if (m_SubjectBlacklist.size()  &&
            m_SubjectBlacklist.find(subject) != m_SubjectBlacklist.end()) {
            /// reject: subject sequence found in black list
            return false;
        }

        if (m_SubjectWhitelist.size()  &&
            m_SubjectWhitelist.find(subject) != m_SubjectWhitelist.end()) {
            /// accept: subject sequence found in white list
            return true;
        }
    }

    /// check to see if scores match
    return x_Match(*m_ParseTree->GetQueryTree(), align) &&
            ( !m_RemoveDuplicates  ||  x_IsUnique(align) );
}


bool CAlignFilter::x_IsUnique(const CSeq_align& align)
{
    CMD5StreamWriter md5;
    {{
        CWStream wstr(&md5);
        wstr << MSerial_AsnBinary << align;
     }}

    string md5_str;
    md5.GetMD5Sum(md5_str);
    return m_UniqueAligns.insert(md5_str).second;
}

double CAlignFilter::x_GetAlignmentScore(const string& score_name, const objects::CSeq_align& align)
{
    ///
    /// see if we have this score
    ///
    double score_value = numeric_limits<double>::quiet_NaN();
    if (NStr::EqualNocase(score_name, "align_length")) {
        score_value = align.GetSeqRange(0).GetLength();
    } else if (align.IsSetScore()) {
        ITERATE (CSeq_align::TScore, iter, align.GetScore()) {
            const CScore& score = **iter;
            if ( !score.IsSetId()  ||
                 !score.GetId().IsStr() ) {
                continue;
            }

            if (score.GetId().GetStr() != score_name) {
                continue;
            }

            if (score.GetValue().IsInt()) {
                score_value = score.GetValue().GetInt();
            } else {
                score_value = score.GetValue().GetReal();
            }
            break;
        }
    }
    return score_value;
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
    return this_val;
}


double CAlignFilter::x_TermValue(const CQueryParseTree::TNode& term_node,
                                 const CSeq_align& align)
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
             try {
                 double val =  NStr::StringToDouble(str);
                 return val;
             } catch (...) {
             }
             return x_GetAlignmentScore(str, align);
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
             for ( ;  iter != node.SubNodeEnd()  &&  res;  ++iter) {
                 res &= x_Match(**iter, align);
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
             for ( ;  iter != node.SubNodeEnd();  ++iter) {
                 res |= x_Match(**iter, align);
             }

             return res;
         }}


    default:
        LOG_POST(Warning << "unhandled parse node in tree");
        break;
    }

    return false;
}



END_NCBI_SCOPE

