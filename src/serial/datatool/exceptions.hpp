#ifndef DATATOOL_EXCEPTIONS_HPP
#define DATATOOL_EXCEPTIONS_HPP

#include <stdexcept>

class CTypeNotFound : public runtime_error
{
public:
    CTypeNotFound(const string& msg)
        : runtime_error(msg)
        {
        }
};

class CModuleNotFound : public CTypeNotFound
{
public:
    CModuleNotFound(const string& msg)
        : CTypeNotFound(msg)
        {
        }
};

class CAmbiguiousTypes : public CTypeNotFound
{
public:
    CAmbiguiousTypes(const string& msg)
        : CTypeNotFound(msg)
        {
        }
};

#endif
