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
        : aspl::Tracer(Mode::Custom, Style::Hierarchical)
    {
        if (const char* traceEnv = getenv("ASPL_TRACE")) {
            consoleEnabled_ = strcmp(traceEnv, "1") == 0;
        }
    }

protected:
    void Print(const char* message) override
    {
        if (consoleEnabled_) {
            fprintf(stderr, "%s\n", message);
        }

        // Detect unpaired operations errors reported by Tracer.
        if (strstr(message, "Tracer")) {
            FAIL() << message;
        }
    }

private:
    bool consoleEnabled_ = false;
};

} // anonymous namespace
