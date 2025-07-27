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
    void PrintImpl(const char* message) override
    {
        if (printEnabled_) {
            Tracer::PrintImpl(message);
        }

        // Detect unpaired operations errors reported by Tracer.
        if (strstr(message, "Tracer")) {
            FAIL() << message;
        }
    }

private:
    bool printEnabled_ = false;
};

} // anonymous namespace
