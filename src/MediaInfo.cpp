#include <cstdlib>
#include "MediaInfo.h"

using namespace std;

namespace MediaCore
{
Ratio::Ratio(const string& ratstr)
{
    auto pos = ratstr.find('/');
    if (pos == string::npos)
        pos = ratstr.find(':');
    if (pos == string::npos)
    {
        num = atoi(ratstr.c_str());
        den = 1;
    }
    else
    {
        num = atoi(ratstr.substr(0, pos).c_str());
        if (pos == ratstr.size()-1)
            den = 1;
        else
            den = atoi(ratstr.substr(pos+1).c_str());
    }
}

ostream& operator<<(ostream& os, const Value& val)
{
    if (val.type == Value::VT_INT)
        os << val.numval.i64;
    else if (val.type == Value::VT_DOUBLE)
        os << val.numval.dbl;
    else if (val.type == Value::VT_BOOL)
        os << val.numval.bln;
    else if (val.type == Value::VT_STRING)
        os << val.strval;
    else if (val.type == Value::VT_FLAGS)
        os << val.numval.i64;
    else if (val.type == Value::VT_RATIO)
    {
        if (val.strval.empty())
            os << "0";
        else
            os << val.strval;
    }
    return os;
}
}

