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
 * Author:  Sergey Satskiy, NCBI
 * Credits: Denis Vakatov, NCBI (API design)
 *
 */

/** @file
 * This file contains the implementation of the xslt::xpath_object class.
**/


// xmlwrapp includes
#include <misc/xmlwrapp/xpath_object.hpp>
#include <misc/xmlwrapp/xslt_exception.hpp>
#include <misc/xmlwrapp/node.hpp>
#include <misc/xmlwrapp/xslt_init.hpp>

#include "node_set_impl.hpp"


// libxml2
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

// For isnan(...)
#if defined(_MSC_VER)
#  include <float.h>
#  if !defined(isnan)
#    define isnan _isnan
#  endif
#else
#  include <math.h>
#endif

// for INT_MIN and INT_MAX
#include <limits.h>


namespace xslt {

    namespace impl
    {
        // Private implementation of the xpath_object class.
        // It holds a libxml2 pointer to xmlXPathObject structure and
        // provides reference counting
        struct xpath_obj_impl
        {
            xmlXPathObjectPtr   obj_;
            mutable bool        owner_;
            bool                from_xslt_;

            xpath_obj_impl(void*  obj) :
                obj_(reinterpret_cast<xmlXPathObjectPtr>(obj)),
                owner_(true), from_xslt_(false), refcnt_(1)
            {}
            void inc_ref() { ++refcnt_; }
            void dec_ref() {
                if (--refcnt_ == 0) {
                    if (obj_ && owner_)
                        xmlXPathFreeObject(obj_);
                    delete this;
                }
            }

        protected:
            ~xpath_obj_impl() {}

        private:
            size_t  refcnt_;        // reference counter
        };
    } // impl namespace



    //
    // xpath_object
    //

    const char*     kCouldNotCopyNode           = "Could not copy node";
    const char*     kCouldNotCreateNodeSet      = "Could not create a new node set";
    const char*     kCouldNotCreateXpathNodeSet = "Could not create new xpath nodeset";
    const char*     kUninitialisedObject        = "Uninitialised xpath_object";
    const char*     kCannotCopyXpathObject      = "Cannot copy xpath_object";
    const char*     kConverToNumberFailed       = "XPath conversion to number failed";
    const char*     kUnexpectedXpathObjectType  = "Unexpected xpath_object type";


    xpath_object::xpath_object() :
        pimpl_(NULL)
    {
        /* Avoid compiler warnings */
        pimpl_ = new impl::xpath_obj_impl(0);
    }

    xpath_object::~xpath_object()
    {
        if (pimpl_ != NULL)
            pimpl_->dec_ref();
    }

    xpath_object::xpath_object(const xpath_object&  other) :
        pimpl_(other.pimpl_)
    {
        pimpl_->inc_ref();
    }

    xpath_object& xpath_object::operator= (const xpath_object& other)
    {
        if (this != &other) {
            pimpl_->dec_ref();
            pimpl_ = other.pimpl_;
            pimpl_->inc_ref();
        }
        return *this;
    }

    xpath_object::xpath_object(xpath_object &&  other) :
        pimpl_(other.pimpl_)
    {
        other.pimpl_ = NULL;
    }

    xpath_object &  xpath_object::operator= (xpath_object &&  other)
    {
        if (this != &other) {
            if (pimpl_ != NULL)
                pimpl_->dec_ref();
            pimpl_ = other.pimpl_;
            other.pimpl_ = NULL;
        }
        return *this;
    }

    xpath_object::xpath_object(const char* value) :
        pimpl_(NULL)
    {
        xmlXPathObjectPtr   new_obj = NULL;
        if (value)
            new_obj = xmlXPathNewString(
                                reinterpret_cast<const xmlChar*>(value));
        else
            new_obj = xmlXPathNewString(
                                reinterpret_cast<const xmlChar*>(""));

        if (new_obj == NULL)
            throw xslt::exception("Could not create new xpath string");

        try {
            pimpl_ = new impl::xpath_obj_impl(new_obj);
        } catch (...) {
            xmlXPathFreeObject(new_obj);
            throw;
        }
    }

    xpath_object::xpath_object(bool value) :
        pimpl_(NULL)
    {
        xmlXPathObjectPtr   new_obj = xmlXPathNewBoolean(value);
        if (new_obj == NULL)
            throw xslt::exception("Could not create new xpath boolean");

        try {
            pimpl_ = new impl::xpath_obj_impl(new_obj);
        } catch (...) {
            xmlXPathFreeObject(new_obj);
            throw;
        }
    }

    xpath_object::xpath_object(int value) :
        pimpl_(NULL)
    {
        xmlXPathObjectPtr   new_obj = xmlXPathNewFloat(value);
        if (new_obj == NULL)
            throw xslt::exception("Could not create new xpath integer");

        try {
            pimpl_ = new impl::xpath_obj_impl(new_obj);
        } catch (...) {
            xmlXPathFreeObject(new_obj);
            throw;
        }
    }

    xpath_object::xpath_object(double value) :
        pimpl_(NULL)
    {
        xmlXPathObjectPtr   new_obj = xmlXPathNewFloat(value);
        if (new_obj == NULL)
            throw xslt::exception("Could not create new xpath double");

        try {
            pimpl_ = new impl::xpath_obj_impl(new_obj);
        } catch (...) {
            xmlXPathFreeObject(new_obj);
            throw;
        }
    }

    xpath_object::xpath_object (const xml::node &  node_) :
        pimpl_(NULL)
    {
        // Do recursive copy
        xmlNodePtr      new_node = xmlCopyNode(
                            reinterpret_cast<xmlNodePtr>(
                                                    node_.get_node_data()), 1);
        if (new_node == NULL)
            throw xslt::exception(kCouldNotCopyNode);

        xmlXPathObjectPtr   new_obj = xmlXPathNewNodeSet(new_node);
        if (new_obj == NULL) {
            xmlFreeNode(new_node);
            throw xslt::exception("Could not create new xpath node");
        }

        // This is a part of a hack of leak-less handling nodeset return values
        // from XSLT extension functions.
        // libxml2 has undocumented behavior of how to free xmlXPathObject.
        // It depends on the boolval value. If it is 0 then the only
        // xmlXPathObject structure is freed. If it is 1 then the structure is
        // freed and all the contained nodes are freed as well. Here I have to
        // set boolval to 1 however if this object is passed to XSLT as a
        // return value, it should be reset due to a bug (?) in libxslt which
        // frees the nodes too early so that xslt does not work as expected.
        // See extension_function.cpp and stylesheet.cpp as well.
        if (init::get_allow_extension_functions_leak() == false)
            new_obj->boolval = 1;

        try {
            pimpl_ = new impl::xpath_obj_impl(new_obj);
        } catch (...) {
            xmlXPathFreeObject(new_obj);
            throw;
        }
    }

    xpath_object::xpath_object (const xml::node_set &  nset) :
        pimpl_(NULL)
    {
        xmlNodeSetPtr   new_node_set = xmlXPathNodeSetCreate(NULL);
        if (new_node_set == NULL)
            throw xslt::exception(kCouldNotCreateNodeSet);

        // Create a temporary copy of all nodes
        std::vector<xmlNodePtr>             node_copies;
        xml::node_set::const_iterator       k = nset.begin();
        for ( ; k != nset.end(); ++k) {
            void *      raw_node = k->get_node_data();
            xmlNodePtr  new_node = xmlCopyNode(
                                        reinterpret_cast<xmlNodePtr>(
                                            raw_node), 1);
            if (new_node == NULL) {
                for (std::vector<xmlNodePtr>::iterator  j = node_copies.begin();
                     j != node_copies.end(); ++j)
                    xmlFreeNode(*j);
                xmlXPathFreeNodeSet(new_node_set);
                throw xslt::exception(kCouldNotCopyNode);
            }
            node_copies.push_back(new_node);
        }

        // Add new nodes to the set
        for (std::vector<xmlNodePtr>::iterator  j = node_copies.begin();
             j != node_copies.end(); ++j) {
            // Sick! libxml2 does not give a chance to know if there was an
            // allocation problem
            xmlXPathNodeSetAdd(new_node_set, *j);
        }

        // Wrap the nodes into XPathObject
        xmlXPathObjectPtr   new_obj = xmlXPathNewNodeSetList(new_node_set);
        xmlXPathFreeNodeSet(new_node_set);  // xmlNodeSet not needed any more
        if (new_obj == NULL) {
            for (std::vector<xmlNodePtr>::iterator  j = node_copies.begin();
                 j != node_copies.end(); ++j)
                xmlFreeNode(*j);
            throw xslt::exception(kCouldNotCreateXpathNodeSet);
        }

        // This is a part of a hack of leak-less handling nodeset return values
        // from XSLT extension functions.
        // See detailed comment about boolval above.
        if (init::get_allow_extension_functions_leak() == false)
            new_obj->boolval = 1;

        try {
            pimpl_ = new impl::xpath_obj_impl(new_obj);
        } catch (...) {
            xmlXPathFreeObject(new_obj);
            throw;
        }
    }

    xpath_object::xpath_object (const type_node_source &  value) :
        pimpl_(NULL)
    {
        xmlNodeSetPtr   new_node_set = xmlXPathNodeSetCreate(NULL);
        if (new_node_set == NULL)
            throw xslt::exception(kCouldNotCreateNodeSet);

        // Create a temporary copy of all nodes
        std::vector<xmlNodePtr>             node_copies;
        type_node_source::const_iterator    k = value.begin();
        for ( ; k != value.end(); ++k) {
            void *      raw_node = k->get_node_data();
            xmlNodePtr  new_node = xmlCopyNode(
                                        reinterpret_cast<xmlNodePtr>(
                                            raw_node), 1);
            if (new_node == NULL) {
                for (std::vector<xmlNodePtr>::iterator  j = node_copies.begin();
                     j != node_copies.end(); ++j)
                    xmlFreeNode(*j);
                xmlXPathFreeNodeSet(new_node_set);
                throw xslt::exception(kCouldNotCopyNode);
            }
            node_copies.push_back(new_node);
        }

        // Add new nodes to the set
        for (std::vector<xmlNodePtr>::iterator  j = node_copies.begin();
             j != node_copies.end(); ++j) {
            // Sick! libxml2 does not give a chance to know if there was an
            // allocation problem
            xmlXPathNodeSetAdd(new_node_set, *j);
        }

        // Wrap the nodes into XPathObject
        xmlXPathObjectPtr   new_obj = xmlXPathNewNodeSetList(new_node_set);
        xmlXPathFreeNodeSet(new_node_set);  // xmlNodeSet not needed any more
        if (new_obj == NULL) {
            for (std::vector<xmlNodePtr>::iterator  j = node_copies.begin();
                 j != node_copies.end(); ++j)

                xmlFreeNode(*j);
            throw xslt::exception(kCouldNotCreateXpathNodeSet);
        }

        // This is a part of a hack of leak-less handling nodeset return values
        // from XSLT extension functions.
        // See detailed comment about boolval above.
        if (init::get_allow_extension_functions_leak() == false)
            new_obj->boolval = 1;

        try {
            pimpl_ = new impl::xpath_obj_impl(new_obj);
        } catch (...) {
            xmlXPathFreeObject(new_obj);
            throw;
        }
    }

    std::string xpath_object::get_as_string(void) const
    {
        if (pimpl_->obj_ == NULL)
            throw xslt::exception(kUninitialisedObject);

        if (pimpl_->obj_->type == XPATH_STRING)
            return reinterpret_cast<const char *>(pimpl_->obj_->stringval);

        // Type differs, try to convert
        xmlXPathObjectPtr   copy = xmlXPathObjectCopy(pimpl_->obj_);
        if (copy == NULL)
            throw xslt::exception(kCannotCopyXpathObject);
        copy = xmlXPathConvertString(copy);
        if (copy == NULL)
            throw xslt::exception("XPath conversion to string failed");

        std::string     retval = reinterpret_cast<const char *>(copy->stringval);
        xmlXPathFreeObject(copy);
        return retval;
    }

    bool xpath_object::get_as_bool(void) const
    {
        if (pimpl_->obj_ == NULL)
            throw xslt::exception(kUninitialisedObject);

        if (pimpl_->obj_->type == XPATH_BOOLEAN)
            return pimpl_->obj_->boolval != 0;

        // Type differs, try to convert
        xmlXPathObjectPtr   copy = xmlXPathObjectCopy(pimpl_->obj_);
        if (copy == NULL)
            throw xslt::exception(kCannotCopyXpathObject);
        copy = xmlXPathConvertBoolean(copy);
        if (copy == NULL)
            throw xslt::exception("XPath conversion to boolean failed");

        bool    retval = copy->boolval !=0;
        xmlXPathFreeObject(copy);
        return retval;
    }

    int xpath_object::get_as_int(void) const
    {
        if (pimpl_->obj_ == NULL)
            throw xslt::exception(kUninitialisedObject);

        if (pimpl_->obj_->type == XPATH_NUMBER) {
            test_int_convertability(pimpl_->obj_->floatval);
            return static_cast<int>(pimpl_->obj_->floatval);
        }

        // Type differs, try to convert
        xmlXPathObjectPtr   copy = xmlXPathObjectCopy(pimpl_->obj_);
        if (copy == NULL)
            throw xslt::exception(kCannotCopyXpathObject);
        copy = xmlXPathConvertNumber(copy);
        if (copy == NULL)
            throw xslt::exception(kConverToNumberFailed);

        try {
            test_int_convertability(copy->floatval);
        } catch (...) {
            xmlXPathFreeObject(copy);
            throw;
        }

        int  retval = static_cast<int>(copy->floatval);
        xmlXPathFreeObject(copy);
        return retval;
    }

    double xpath_object::get_as_float(void) const
    {
        if (pimpl_->obj_ == NULL)
            throw xslt::exception(kUninitialisedObject);

        if (pimpl_->obj_->type == XPATH_NUMBER)
            return pimpl_->obj_->floatval;

        // Type differs, try to convert
        xmlXPathObjectPtr   copy = xmlXPathObjectCopy(pimpl_->obj_);
        if (copy == NULL)
            throw xslt::exception(kCannotCopyXpathObject);
        copy = xmlXPathConvertNumber(copy);
        if (copy == NULL)
            throw xslt::exception(kConverToNumberFailed);

        double  retval = copy->floatval;
        xmlXPathFreeObject(copy);
        return retval;
    }

    xml::node &  xpath_object::get_as_node (void) const
    {
        if (pimpl_->obj_ == NULL)
            throw xslt::exception(kUninitialisedObject);

        if (pimpl_->obj_->type != XPATH_NODESET)
            throw xslt::exception(kUnexpectedXpathObjectType);

        if (pimpl_->obj_->nodesetval->nodeNr <= 0)
            throw xslt::exception("There are no nodes in the set");

        // Wrap up the nodeset as an xmlwrapp class and make sure
        // the wrapper has no ownership on the object.
        xml::node_set   nset(pimpl_->obj_);
        nset.pimpl_->set_ownership(false);

        return *nset.begin();
    }

    xml::node_set  xpath_object::get_as_node_set (void) const
    {
        if (pimpl_->obj_ == NULL)
            throw xslt::exception(kUninitialisedObject);

        if (pimpl_->obj_->type != XPATH_NODESET)
            throw xslt::exception(kUnexpectedXpathObjectType);

        // Wrap up the nodeset as an xmlwrapp class and make sure
        // the wrapper has no ownership on the object.
        xml::node_set   nset(pimpl_->obj_);
        nset.pimpl_->set_ownership(false);
        return nset;
    }

    xpath_object_type xpath_object::get_type(void) const
    {
        if (pimpl_->obj_ == NULL)
            throw xslt::exception(kUninitialisedObject);

        switch (pimpl_->obj_->type) {
            case XPATH_UNDEFINED:   return type_undefined;
            case XPATH_NODESET:     return type_nodeset;
            case XPATH_BOOLEAN:     return type_boolean;
            case XPATH_NUMBER:      return type_number;
            case XPATH_STRING:      return type_string;

            case XPATH_POINT:       return type_not_implemented;
            case XPATH_RANGE:       return type_not_implemented;
            case XPATH_LOCATIONSET: return type_not_implemented;
            case XPATH_USERS:       return type_not_implemented;
            case XPATH_XSLT_TREE:   return type_not_implemented;
            default:
                ;
        }
        throw xslt::exception("Unknown xpath_object type");
    }

    void *  xpath_object::get_object(void) const
    {
        return pimpl_->obj_;
    }

    void xpath_object::revoke_ownership(void) const
    {
        pimpl_->owner_ = false;
    }

    void xpath_object::set_from_xslt(void) const
    {
        pimpl_->from_xslt_ = true;
    }

    bool xpath_object::get_from_xslt(void) const
    {
        return pimpl_->from_xslt_;
    }

    void xpath_object::test_int_convertability(double val) const
    {
        if (isnan(val) != 0)
            throw xslt::exception("NaN cannot be converted to int");
        if (val < INT_MIN)
            throw xslt::exception("Value is too small to be converted to int");
        if (val > INT_MAX)
            throw xslt::exception("Value is too large to be converted to int");
    }

    xpath_object::xpath_object(void *  raw_object) :
        pimpl_(new impl::xpath_obj_impl(
                        reinterpret_cast<xmlXPathObjectPtr>(raw_object)))
    {}

} // xslt namespace

