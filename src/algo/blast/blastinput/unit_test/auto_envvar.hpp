#ifndef __AUTO_ENVVAR__HPP
#define __AUTO_ENVVAR__HPP

#include <corelib/ncbienv.hpp>

/// Auxiliary class to modify the environment for the duration of a unit test
/// and restore it afterwards
class CAutoEnvironmentVariable
{
public:
    /// Initializes the environment variable passed as an argument to "1"
    /// @param var_name environment variable name [in]
    CAutoEnvironmentVariable(const char* var_name) 
        : m_VariableName(var_name)
    {
        _ASSERT(var_name);
        std::string var(m_VariableName);
        std::string value("1");
        ncbi::CNcbiEnvironment env(0);
        m_PrevValue = env.Get(var);
        env.Set(var, value);
    }

    /// Initializes the environment variable passed as an argument to the
    /// corresponding value
    /// @param var_name environment variable name [in]
    /// @param value value to set the environment variable to [in]
    CAutoEnvironmentVariable(const char* var_name, const std::string& value) 
        : m_VariableName(var_name)
    {
        _ASSERT(var_name);
        std::string var(m_VariableName);
        ncbi::CNcbiEnvironment env(0);
        m_PrevValue = env.Get(var);
        if (value == kEmptyStr) {
            env.Unset(var);
        } else {
            env.Set(var, value);
        }
    }

    /// Destructor which restores the modifications made in the environment by
    /// this class
    ~CAutoEnvironmentVariable() {
        std::string var(m_VariableName);
        ncbi::CNcbiEnvironment env(0);
        env.Set(var, m_PrevValue);
    }
private:
    /// Name of the environment variable manipulated
    const char* m_VariableName;
    /// Previous value of the environment variable manipulated
    std::string m_PrevValue;
};

#endif /* __AUTO_ENVVAR__HPP */
