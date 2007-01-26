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
    typedef vector<unsigned char> TBuffer;
    typedef BV                    TBitVector;
    
    CQueryEval_BV_Value() {}
    virtual void Reset() 
    { 
        m_BV_Buffer.reset(0);m_BV.reset(0);
    }
    TBuffer*    GetBuffer() { return m_BV_Buffer.get(); }
    TBitVector* GetBV()     { return m_BV.get(); }
    
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
        
        typename TParent::TBVContainer* bv_cont;        
        bv_cont = this->MakeContainer(qnode);
        BV* bv_res;
        bv_cont->SetBV(bv_res = new BV);
        
        for (size_t i = 0; i < args.size(); ++i) {
            CQueryParseTree::TNode* arg_node = args[i];
            typename TParent::TBVContainer* arg_cont = 
                                    this->GetContainer(*arg_node);
            if (!arg_cont) {
                continue;
            }
            
            typename TParent::TBVContainer::TBuffer *arg_buf = 
                                                arg_cont->GetBuffer();
            typename TParent::TBVContainer::TBitVector* arg_bv = 
                                                arg_cont->GetBV();
            
            _ASSERT(arg_buf == 0 || arg_bv == 0);
            _ASSERT(arg_buf != 0 || arg_bv != 0);
            
            if (arg_bv) {
                bv_res->combine_operation(*arg_bv, (bm::operation) m_OpCode);
            }
            if (arg_buf) {
               bm::operation_deserializer<BV>::deserialize(*bv_res,
                                                  &((*arg_buf)[0]),
                                                  0,
                                                  m_OpCode);
            }
        } // for        
    }

protected:
    const bm::set_operation  m_OpCode;
};




/* @} */

END_NCBI_SCOPE


#endif  // UTIL__QUERY_EXEC_FUNC_BV_HPP__

