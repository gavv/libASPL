// Copyright (c) libASPL authors
// Licensed under MIT

#include "aspl/Tracer.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

class MockTracer : public aspl::Tracer
{
public:
    MockTracer()
        : aspl::Tracer(Output::Stderr)
    {
        if (const char* traceEnv = getenv("ASPL_TRACE")) {
            printEnabled_ = strcmp(traceEnv, "1") == 0;
        }
    }

protected:
    void PrintImpl(Category category, const char* message) override
    {
        if (printEnabled_) {
            // Actually print only if enabled via env.
            Tracer::PrintImpl(category, message);
        }

        // Fail test on bug.
        ASSERT_FALSE(category == Category::Alert);
    }

private:
    bool printEnabled_ = false;
};

} // anonymous namespace
