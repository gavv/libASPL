// Copyright (c) libASPL authors
// Licensed under MIT

#include <aspl/Tracer.hpp>

#include "Strings.hpp"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <sstream>
#include <vector>

#include <pthread.h>
#include <syslog.h>

namespace aspl {

namespace {

enum
{
    DepthSoftLimit = 10,
    DepthHardLimit = 1000,
};

UInt64 GetThreadID()
{
    UInt64 tid = 0;
    pthread_threadid_np(nullptr, &tid);

    return tid;
}

UInt64 MakeThreadIndex()
{
    static std::atomic<UInt64> threadCounter = 0;

    return threadCounter++;
}

void FormatThreadColor(UInt64 threadIndex, char* beginColor, char* endColor)
{
    // clang-format off
    static const unsigned colors[] = {
        39, 192, 49, 210, 27, 172, 46, 201, 99, 184,
        87, 213, 34, 230, 56, 154, 141, 37, 195, 147,
    };
    // clang-format on

    unsigned colorCode = colors[threadIndex % std::size(colors)];

    snprintf(beginColor, 16, "\033[38;5;%um", colorCode);
    strcpy(endColor, "\033[0m");
}

UInt32 DeduceDefaultStyle(Tracer::Output output)
{
    UInt32 style = Tracer::Style::Hierarchical;

    if (output == Tracer::Output::Stderr && isatty(STDERR_FILENO)) {
        style |= Tracer::Style::Colored;
    }

    return style;
}

} // namespace

Tracer::Tracer(Output output)
    : Tracer(output, DeduceDefaultStyle(output))
{
}

Tracer::Tracer(Output output, UInt32 style)
    : output_(output)
    , style_(style)
{
    pthread_key_create(&threadKey_, DestroyThreadLocalState);
}

void* Tracer::CreateThreadLocalState(UInt32 style)
{
    auto* state = new ThreadLocalState();

    state->ThreadIndex = MakeThreadIndex();

    if (style & Style::Colored) {
        FormatThreadColor(state->ThreadIndex, state->BeginColor, state->EndColor);
    }

    return state;
}

void Tracer::DestroyThreadLocalState(void* ptr)
{
    delete static_cast<ThreadLocalState*>(ptr);
}

Tracer::ThreadLocalState& Tracer::GetThreadLocalState()
{
    void* ptr = pthread_getspecific(threadKey_);

    if (!ptr) {
        ptr = CreateThreadLocalState(style_);
        pthread_setspecific(threadKey_, ptr);
    }

    return *static_cast<ThreadLocalState*>(ptr);
}

void Tracer::OperationBegin(const Operation& op)
{
    if (output_ == Output::Null) {
        return;
    }

    auto& threadState = GetThreadLocalState();

    if (threadState.DepthCounter < DepthHardLimit) {
        threadState.DepthCounter++;
    } else {
        Print(
            "Tracer: detected unpaired OperationBegin/OperationEnd"
            " or infinite recursion");
    }

    if (threadState.IgnoreCounter != 0 &&
        threadState.DepthCounter >= threadState.IgnoreCounter) {
        return;
    }

    if (ShouldIgnore(op)) {
        threadState.IgnoreCounter = threadState.DepthCounter;
        return;
    }

    const auto str = FormatOperationBegin(op, threadState.DepthCounter);

    Print(str.c_str());
}

void Tracer::Message(const char* format, ...)
{
    if (output_ == Output::Null) {
        return;
    }

    auto& threadState = GetThreadLocalState();

    if (threadState.IgnoreCounter != 0 &&
        threadState.DepthCounter >= threadState.IgnoreCounter) {
        return;
    }

    char message[MaxMessageLen] = {};

    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    const auto str = FormatMessage(message, threadState.DepthCounter);

    Print(str.c_str());
}

void Tracer::OperationEnd(const Operation& op, OSStatus status)
{
    if (output_ == Output::Null) {
        return;
    }

    auto& threadState = GetThreadLocalState();

    if (threadState.IgnoreCounter != 0 &&
        threadState.DepthCounter >= threadState.IgnoreCounter) {
        if (threadState.DepthCounter == threadState.IgnoreCounter) {
            threadState.IgnoreCounter = 0;
        }
        threadState.DepthCounter--;
        return;
    }

    const auto str = FormatOperationEnd(op, status, threadState.DepthCounter);

    Print(str.c_str());

    if (threadState.DepthCounter != 0) {
        threadState.DepthCounter--;
    } else {
        Print("Tracer: detected unpaired OperationBegin/OperationEnd");
    }
}

std::string Tracer::FormatOperationBegin(const Operation& op, UInt32 depth)
{
    if (depth > DepthSoftLimit) {
        depth = DepthSoftLimit;
    }

    std::ostringstream ss;

    const auto& threadState = GetThreadLocalState();
    const auto threadID = GetThreadID();

    ss << threadState.BeginColor << "T" << threadID << " ";

    if (style_ & Style::Hierarchical) {
        ss << "|";
        for (UInt32 i = 0; i < depth; i++) {
            ss << "-";
        }
        ss << " ";
    }

    ss << threadState.EndColor;

    ss << op.Name << " begin";

    if (op.PropertyAddress) {
        ss << " " << PropertySelectorToString(op.PropertyAddress->mSelector);
    }

    if (op.ClientPID != 0) {
        ss << " clientPID=" << op.ClientPID;
    }

    ss << " objectID=" << op.ObjectID;

    if (op.PropertyAddress) {
        ss << " scope=" << PropertyScopeToString(op.PropertyAddress->mScope);
    }

    if (op.InDataSize != 0 || op.InData != nullptr) {
        ss << " inSize=" << op.InDataSize;
    }

    if (op.QualifierDataSize != 0 || op.QualifierData != nullptr) {
        ss << " qualSize=" << op.QualifierDataSize;
    }

    return ss.str();
}

std::string Tracer::FormatMessage(const char* message, UInt32 depth)
{
    if (depth > DepthSoftLimit) {
        depth = DepthSoftLimit;
    }

    std::ostringstream ss;

    const auto& threadState = GetThreadLocalState();
    const auto threadID = GetThreadID();

    ss << threadState.BeginColor << "T" << threadID << " ";

    if (style_ & Style::Hierarchical) {
        ss << "|";
        for (UInt32 i = 0; i <= depth; i++) {
            ss << "-";
        }
        ss << " ";
    }

    ss << threadState.EndColor;

    ss << message;

    return ss.str();
}

std::string Tracer::FormatOperationEnd(const Operation& op, OSStatus status, UInt32 depth)
{
    if (depth > DepthSoftLimit) {
        depth = DepthSoftLimit;
    }

    std::ostringstream ss;

    const auto& threadState = GetThreadLocalState();
    const auto threadID = GetThreadID();

    ss << threadState.BeginColor << "T" << threadID << " ";

    if (style_ & Style::Hierarchical) {
        ss << "|";
        for (UInt32 i = 0; i < depth; i++) {
            ss << "-";
        }
        ss << " ";
    }

    ss << threadState.EndColor;

    ss << op.Name << " end";

    ss << " status=" << StatusToString(status);

    if (status == kAudioHardwareNoError && op.OutDataSize != nullptr) {
        ss << " outSize=" << *op.OutDataSize;
    }

    return ss.str();
}

void Tracer::Print(const char* message)
{
    switch (output_) {
    case Output::Null:
        return;

    case Output::Stderr:
        fprintf(stderr, "[aspl] %s\n", message);
        return;

    case Output::Syslog:
        syslog(LOG_NOTICE, "[aspl] %s", message);
        return;

    case Output::Custom:
        return;
    }
}

bool Tracer::ShouldIgnore(const Operation& operation)
{
    return false;
}

} // namespace aspl
