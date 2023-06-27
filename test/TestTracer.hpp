#include <aspl/Tracer.hpp>

#include <gtest/gtest.h>

#include <cstring>

namespace {

class TestTracer : public aspl::Tracer
{
public:
    TestTracer()
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
