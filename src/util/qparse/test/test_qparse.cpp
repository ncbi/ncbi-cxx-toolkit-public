/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *   This software/database is a "United States Government Work" under the
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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:
 *   Test/demo for query parser
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <util/qparse/query_parse.hpp>
#include <stdio.h>

#include <test/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


/// Integer calculator for static query evaluation
///
/// @internal
///
/*
class CTestQueryEvalFunc
{
public: 
    CTestQueryEvalFunc()
    {}

    
    void PrintUnary(const CQueryParseNode& qnode)
    {
        CQueryParseNode::EUnaryOp op = qnode.GetUnaryOp();
        switch (op) {
        case CQueryParseNode::eNot:
            m_OStream << "NOT";
            break;
        default:
            m_OStream << "UNK";
        }
    }
    void PrintBinary(const CQueryParseNode& qnode)
    {
        CQueryParseNode::EBinaryOp op = qnode.GetBinaryOp();
        switch (op) {
        case CQueryParseNode::eAnd:
            m_OStream << "AND";
            break;
        case CQueryParseNode::eOr:
            m_OStream << "OR";
            break;
        case CQueryParseNode::eSub:
            m_OStream << "MINUS";
            break;
        case CQueryParseNode::eNot2:
            m_OStream << "NOT";
            break;
        case CQueryParseNode::eXor:
            m_OStream << "XOR";
            break;
        case CQueryParseNode::eEQ:
            m_OStream << "==";
            break;
        case CQueryParseNode::eGT:
            m_OStream << ">";
            break;
        case CQueryParseNode::eGE:
            m_OStream << ">=";
            break;
        case CQueryParseNode::eLT:
            m_OStream << "<";
            break;
        case CQueryParseNode::eLE:
            m_OStream << "<=";
            break;
        default:
            m_OStream << "UNK";
            break;
        }
    }
  
    ETreeTraverseCode 
    operator()(const CTreeNode<CQueryParseNode>& tr, int delta) 
    {
        const CQueryParseNode& qnode = tr.GetValue();

        if (delta < 0)
            return eTreeTraverse;

        
        if (qnode.IsNot()) {
            m_OStream << "NOT ";        
        }

        switch (qnode.GetType()) {
        case CQueryParseNode::eIdentifier:
            m_OStream << qnode.GetStrValue();
            break;
        case CQueryParseNode::eString:
            m_OStream << '"' << qnode.GetStrValue() << '"';
            break;
        case CQueryParseNode::eIntConst:
            m_OStream << qnode.GetInt() << "L";
            break;
        case CQueryParseNode::eFloatConst:
            m_OStream << qnode.GetDouble() << "F";
            break;
        case CQueryParseNode::eBoolConst:
            m_OStream << qnode.GetBool();
            break;
        case CQueryParseNode::eUnaryOp:
            PrintUnary(qnode);
            break;
        case CQueryParseNode::eBinaryOp:
            PrintBinary(qnode);
            break;
        default:
            m_OStream << "UNK";
            break;
        } // switch


        return eTreeTraverse;
    }

};
*/








/// @internal
class CTestQParse : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTestQParse::Init(void)
{

    SetDiagPostLevel(eDiag_Warning);

    auto_ptr<CArgDescriptions> d(new CArgDescriptions);

    d->SetUsageContext("test_qparse",
                       "test query parse");
    SetupArgDescriptions(d.release());
}

void TestExpression(const char* q)
{
    CQueryParseTree qtree;
    NcbiCout << "---------------------------------------------------" << endl;
    NcbiCout << "Query:" << "'" << q << "'" << endl << endl;    
    qtree.Parse(q);
    qtree.Print(NcbiCout);
    NcbiCout << "---------------------------------------------------" << endl;
}




int CTestQParse::Run(void)
{
    const char* queries[] = {    
        "\n\r\n(asdf != 2)",
        "    asdf != 2",
        " 1 AND 0 ",
        " 1 OR 0 ",
        " 1 XOR 0 ",
        " 1 SUB 1",
        " 1 MINUS 1",
        " 1 && 1",
        " 1 || 1",
        " 13 NOT 1",
        " NOT 1",
        " 1 < 2",
        " 1 <= 1",
        " 1 > 1",
        " 200>=1002",
        " 200==1002",
        " 200 <> 1002",
        " 200 != 1002",
        " 100 OR \nVitamin C ",
        "(1 and 1)or 0",
        "((1 and 1)or 0)((1 && 1) or 0)",
        "\"Vitamin C\" \"aspirin\"\"2\"and \"D\"",  
        "a AND P[2]",
        "\"Pseudarthrosis\"[MeSH] OR \"Pseudarthrosis\"[MeSH] AND systematic neurons[Text word]",
        "\"J Sci Food Agric\"[Journal:__jrid5260] 1974",
        "vitamin 1993:1995 [dp]",
         "\"2005/04/17 12.23\"[MHDA]:\"2005/05/10 10.56\"[MHDA]"
    };
    int l = sizeof (queries) / sizeof(queries[0]);
    for (int i = 0; i < l; ++i) {
        TestExpression(queries[i]);
    } // for
    return 0;
}


int main(int argc, const char* argv[])
{
    CTestQParse app; 
    return app.AppMain(argc, argv);
}

/*
 * ===========================================================================
 * $Log: test_qparse.cpp,v $
 * Revision 1.2  2007/01/11 14:49:52  kuznets
 * Many cosmetic fixes and functional development
 *
 * Revision 1.1  2007/01/10 17:06:31  kuznets
 * initial revision
 *
 * ===========================================================================
 */
