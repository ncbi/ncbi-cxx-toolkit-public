#include <aparser.hpp>

USING_NCBI_SCOPE;

AbstractParser::AbstractParser(AbstractLexer& lexer)
    : m_Lexer(lexer)
{
}

AbstractParser::~AbstractParser(void)
{
}

void AbstractParser::ParseError(const char* error, const char* expected,
                                const AbstractToken& token)
{
    throw runtime_error(NStr::IntToString(token.GetLine()) +
                        ": Parse error: " + error + ": " +
                        expected + " expected");
}

string AbstractParser::Location(void) const
{
    const AbstractToken& token = NextToken();
    return NStr::IntToString(token.GetLine()) + ':';
}
