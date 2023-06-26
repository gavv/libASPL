#include <aspl/Tracer.hpp>

#include <gtest/gtest.h>

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
    }

    void Error(const char* message) override
    {
        FAIL() << message;
    }
};

} // anonymous namespace
