#ifndef UTIL__QUERY_EXEC_FUNC_BV_HPP__
#define UTIL__QUERY_EXEC_FUNC_BV_HPP__

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
 * Authors: Anatoliy Kuznetsov
 *
 * File Description: BitVector specific query execution components 
 *
 */

#include <util/qparse/query_exec.hpp>
#include <util/bitset/ncbi_bitset.hpp>
#include <util/bitset/bmserial.h>
#include <util/simple_buffer.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup QParser
 *
 * @{
 */


/// Query tree user object for storing bit-vectors
///
/// Stores either bitvector as is or serialized(compressed) buffer
///
template<class BV>
class CQueryEval_BV_Value : public IQueryParseUserObject
{
public:
    typedef CSimpleBuffer         TBuffer;
    typedef BV                    TBitVector;
    
    CQueryEval_BV_Value() {}
    virtual void Reset() 
    { 
        m_BV_Buffer.reset(0);m_BV.reset(0);
    }
    TBuffer*    GetBuffer() { return m_BV_Buffer.get(); }
    TBitVector* GetBV()     { return m_BV.get(); }

    TBuffer*    ReleaseBuffer() { return m_BV_Buffer.release(); }
    TBitVector* ReleaseBV()     { return m_BV.release(); }
    
    /// Attach buffer (with ownership transfer)
    void SetBuffer(TBuffer* buf) { m_BV_Buffer.reset(buf); m_BV.reset(0); }
    /// Attach bitvector (with ownership transfer)
    void SetBV(TBitVector* bv) { m_BV.reset(bv); m_BV_Buffer.reset(0); }
    
private:
    CQueryEval_BV_Value(const CQueryEval_BV_Value&);
    CQueryEval_BV_Value& operator=(const CQueryEval_BV_Value&);
private:
    auto_ptr<TBuffer>   m_BV_Buffer; ///< serialized bitvector
    auto_ptr<BV>        m_BV;        ///< bitvector
};


/// Base class for all bit-vector oriented function implementations
///
template<class BV>
class CQueryFunction_BV_Base : public CQueryFunctionBase
{
public:
    typedef CQueryEval_BV_Value<BV> TBVContainer;
        
protected:

    /// Extract user object out of the current node
    ///
    TBVContainer* GetContainer(CQueryParseTree::TNode& qnode)
    {
        IQueryParseUserObject* uo = qnode.GetValue().GetUserObject();
        return dynamic_cast<TBVContainer*>(uo);
    }
    
    /// Create container if it does not exist
    ///
    TBVContainer* MakeContainer(CQueryParseTree::TNode& qnode)
    {
        TBVContainer* c = this->GetContainer(qnode);
        if (!c) {
            qnode.GetValue().SetUserObject((c = new TBVContainer()));
        }
        return c;
    }
    
    /// Unpack argument to result bitvector 
    /// (use assignment for deserialization)
    ///
    BV* ArgToRes(CQueryParseTree::TNode& qnode,
                 CQueryParseTree::TNode* arg_node)
    {
        TBVContainer* bv_cont;
        BV* bv_res = 0;
        TBVContainer* arg_cont = this->GetContainer(*arg_node);
        if (arg_cont) {
            bv_cont = this->MakeContainer(qnode);        
            typename TBVContainer::TBuffer *arg_buf = arg_cont->GetBuffer();
            typename TBVContainer::TBitVector* arg_bv = arg_cont->GetBV();
            if (arg_bv) {
                bv_cont->SetBV(bv_res = new BV(*arg_bv));
            } else
            if (arg_buf) {
               bv_cont->SetBV(bv_res = new BV);                
               bm::operation_deserializer<BV>::deserialize(*bv_res,
                                                  &((*arg_buf)[0]),
                                                  0,
                                                  bm::set_ASSIGN);
            }
        }
        return bv_res;
    }
    
    void ProcessArgVector(CQueryParseTree::TNode&         qnode, 
                          BV*                             bv_res,
                          CQueryFunctionBase::TArgVector& args,
                          const bm::set_operation         op_code)
    {
        for (size_t i = 1; i < args.size(); ++i) {
            CQueryParseTree::TNode* arg_node = args[i];
            TBVContainer* arg_cont = this->GetContainer(*arg_node);
            if (!arg_cont) {
                // no argument... 
                if (op_code == bm::set_AND) {
                    return; // no-op for an AND => 0
                }
                continue; // OR, SUB, etc is ok to continue
            }
            typename TBVContainer::TBuffer *arg_buf = arg_cont->GetBuffer();
            typename TBVContainer::TBitVector* arg_bv = arg_cont->GetBV();
            
            if (!bv_res) { // lazy init
                TBVContainer* bv_cont = this->MakeContainer(qnode);
                if (arg_bv) {
                    bv_cont->SetBV(bv_res = new BV(*arg_bv));
                    continue;
                }
                bv_cont->SetBV(bv_res = new BV);
            }
            
            
            if (arg_bv) {
                bv_res->combine_operation(*arg_bv, (bm::operation) op_code);
            } else
            if (arg_buf) {
               bm::operation_deserializer<BV>::deserialize(*bv_res,
                                                  &((*arg_buf)[0]),
                                                  0,
                                                  op_code);
            }
        } // for        
    
    }
};


/// Implementation of logical functions
///
template<class BV>
class CQueryFunction_BV_Logic : public CQueryFunction_BV_Base<BV>
{
public:
    typedef CQueryFunction_BV_Base<BV> TParent;
public:
    CQueryFunction_BV_Logic(bm::set_operation op)
        : m_OpCode(op)
    {}
    
    virtual void Evaluate(CQueryParseTree::TNode& qnode)
    {
        CQueryFunctionBase::TArgVector args;
        this->MakeArgVector(qnode, args);
        _ASSERT(args.size() > 1);
        
        BV* bv_res = 0;
        
        if (args.size() == 0) {
            // No arg logical function...strange... TO DO:add diagnostics
            return;   
        }
        
        // first argument becomes the default result
        bv_res = this->ArgToRes(qnode, args[0]);
        
        // all other arguments are logically combined with arg[0] 
        this->ProcessArgVector(qnode, bv_res, args, m_OpCode);
    }

protected:
    const bm::set_operation  m_OpCode;
};


/// Implementation of logical NOT 
///
template<class BV>
class CQueryFunction_BV_Not : public CQueryFunction_BV_Base<BV>
{
public:
    typedef CQueryFunction_BV_Base<BV> TParent;
public:
    CQueryFunction_BV_Not()
    {}
    
    virtual void Evaluate(CQueryParseTree::TNode& qnode)
    {
        CQueryFunctionBase::TArgVector args;
        this->MakeArgVector(qnode, args);
        
        BV* bv_res = 0;
        if (args.size() == 0) {
            // No arg logical function...strange... TO DO:add diagnostics
            return;   
        }
        bv_res = this->ArgToRes(qnode, args[0]);
        if (args.size() == 1) { // unary NOT 
            if (bv_res) {
                bv_res->invert();
            } else {
                typename TParent::TBVContainer* bv_cont 
                                        = this->MakeContainer(qnode);
                bv_cont->SetBV(bv_res = new BV);
                bv_res->invert();
            }
            return;
        }
        // muli-argument NOT...  
        // needs to be run as AND NOT or MINUS
        this->ProcessArgVector(qnode, bv_res, args, bm::set_SUB);
    }
};


/// Implementation of IN as OR-ed EQ search
///
/// Based on presumption that:
///     a IN (1, 2, 3) is equivalent of (a=1 OR a=2 OR a=3)
///
template<class BV>
class CQueryFunction_BV_In_Or : public CQueryFunction_BV_Base<BV>
{
public:
    typedef CQueryFunction_BV_Base<BV> TParent;
public:
    CQueryFunction_BV_In_Or()
    {}
    
    virtual void Evaluate(CQueryParseTree::TNode& qnode)
    {
        CQueryFunctionBase* func = this->m_QExec->GetFunc(CQueryParseNode::eEQ);
        if (!func) { // EQ not registered
            NCBI_THROW(CQueryParseException, eUnknownFunction, 
               "Query execution failed. Unknown function: =");
        }
    
        CQueryFunctionBase::TArgVector args;
        this->MakeArgVector(qnode, args);
        _ASSERT(args.size() > 1);
                
        // get first argument (used as left side EQ)
        CQueryParseTree::TNode* arg_node0 = args[0];
        BV* bv_res = 0;
        
        for (size_t i = 1; i < args.size(); ++i) {
            CQueryParseTree::TNode* arg_node = args[i];
            
            // make a temp fake mini tree for EQ operation
            //
            auto_ptr<CQueryParseTree::TNode> eq_node;
            try {
                eq_node.reset(
                    this->GetQueryTree().CreateNode(CQueryParseNode::eEQ, 
                                                    arg_node0, 
                                                    arg_node));
                func->Evaluate(*eq_node);
            } 
            catch (CException&)
            {
                eq_node->RemoveAllSubNodes(CQueryParseTree::TNode::eNoDelete);
                throw;
            }
            // no need to recursively delete the temp tree
            eq_node->RemoveAllSubNodes(CQueryParseTree::TNode::eNoDelete);
            
            
            typename TParent::TBVContainer* cont = this->GetContainer(*eq_node);
            if (!cont) continue;
            
            typename 
                TParent::TBVContainer::TBuffer *arg_buf = cont->GetBuffer();
            typename 
                TParent::TBVContainer::TBitVector* arg_bv = cont->GetBV();
            
            
            if (!bv_res) { // first OR
                typename TParent::TBVContainer* bv_cont = 
                                                this->MakeContainer(qnode);
                if (arg_bv) {
                    bv_cont->SetBV(bv_res = new BV(*arg_bv));
                    continue;
                }
                bv_cont->SetBV(bv_res = new BV);
            }
            
            if (arg_bv) {
                bv_res->combine_operation(*arg_bv, (bm::operation) bm::set_OR);
            } else
            if (arg_buf) {
               bm::operation_deserializer<BV>::deserialize(*bv_res,
                                                           &((*arg_buf)[0]),
                                                           0,
                                                           bm::set_OR);
            }
            
        } // for
        
        if (qnode.GetValue().IsNot()) {
           if (bv_res) {
                bv_res->invert(); 
           } 
           else {
                typename TParent::TBVContainer* bv_cont 
                                        = this->MakeContainer(qnode);
                bv_cont->SetBV(bv_res = new BV);
                bv_res->invert();
           }
        }
    }
};


/* @} */

END_NCBI_SCOPE


#endif  // UTIL__QUERY_EXEC_FUNC_BV_HPP__

