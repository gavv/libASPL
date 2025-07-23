// Copyright (c) libASPL authors
// Licensed under MIT

#include "aspl/Tracer.hpp"

#include <gtest/gtest.h>

#include <cstring>

namespace {

class MockTracer : public aspl::Tracer
{
public:
    MockTracer()
        : aspl::Tracer(aspl::Tracer::Mode::Custom)
    {
    }

protected:
    void Print(const char* message) override
    {
        // Detect unpaired operations errors reported by Tracer.
        if (strstr(message, "Tracer")) {
            FAIL() << message;
        }
    }
};

} // anonymous namespace
