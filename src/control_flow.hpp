#pragma once
#include <gsl/util>

namespace details
{
    struct finally_receiver
    {
        auto operator->*(auto&& closure_fn)
        {
            return gsl::finally(std::forward<decltype(closure_fn)>(closure_fn));
        }
    };
}

#define CFLOW_STRING_CAT(a, b) a##b
#define CFLOW_MAKE_VAR_NAME(L) CFLOW_STRING_CAT(cflow_exit_, L)
#define FINALLY                auto CFLOW_MAKE_VAR_NAME(__LINE__) = details::finally_receiver{}->*[&]
