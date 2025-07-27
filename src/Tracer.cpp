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

void __attribute__((format(printf, 4, 5)))
Appendf(char* buf, size_t bufsz, size_t* off, const char* format, ...)
{
    if (*off >= bufsz) {
        return;
    }

    va_list args;
    va_start(args, format);
    size_t avail = bufsz - *off;
    int written = vsnprintf(buf + *off, avail, format, args);
    va_end(args);

    if (written > 0) {
        if (static_cast<size_t>(written) < avail) {
            *off += static_cast<size_t>(written);
        } else {
            *off = bufsz - 1;
        }
    }

    buf[*off] = '\0';
}

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

UInt32 ChooseThreadColor(UInt64 threadIndex)
{
    // clang-format off
    static const UInt32 colors[] = {
        39, 192, 49, 210, 27, 172, 46, 201, 99, 184,
        87, 213, 34, 230, 56, 154, 141, 37, 195, 147,
    };
    // clang-format on

    return colors[threadIndex % std::size(colors)];
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
        const auto color = ChooseThreadColor(state->ThreadIndex);

        snprintf(state->BeginColor,
            sizeof(state->BeginColor),
            "\033[38;5;%um",
            static_cast<unsigned>(color));
        snprintf(state->EndColor, sizeof(state->EndColor), "\033[0m");
    }

    return state;
}

void Tracer::DestroyThreadLocalState(void* ptr)
{
    delete static_cast<ThreadLocalState*>(ptr);
}

Tracer::ThreadLocalState& Tracer::GetThreadLocalState()
{
    void* state = pthread_getspecific(threadKey_);

    if (!state) {
        state = CreateThreadLocalState(style_);
        pthread_setspecific(threadKey_, state);
    }

    return *static_cast<ThreadLocalState*>(state);
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
        PrintImpl(
            "Tracer: detected unpaired OperationBegin/OperationEnd"
            " or infinite recursion");
    }

    if (threadState.IgnoreCounter != 0 &&
        threadState.DepthCounter >= threadState.IgnoreCounter) {
        return;
    }

    if (!FilterImpl(op)) {
        threadState.IgnoreCounter = threadState.DepthCounter;
        return;
    }

    FormatOperationBeginImpl(threadState.FormatBuffer,
        sizeof(threadState.FormatBuffer),
        op,
        threadState.DepthCounter);

    PrintImpl(threadState.FormatBuffer);
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

    va_list args;
    va_start(args, format);
    vsnprintf(threadState.MessageBuffer, sizeof(threadState.MessageBuffer), format, args);
    va_end(args);

    FormatMessageImpl(threadState.FormatBuffer,
        sizeof(threadState.FormatBuffer),
        threadState.MessageBuffer,
        threadState.DepthCounter);

    PrintImpl(threadState.FormatBuffer);
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

    FormatOperationEndImpl(threadState.FormatBuffer,
        sizeof(threadState.FormatBuffer),
        op,
        status,
        threadState.DepthCounter);

    PrintImpl(threadState.FormatBuffer);

    if (threadState.DepthCounter != 0) {
        threadState.DepthCounter--;
    } else {
        PrintImpl("Tracer: detected unpaired OperationBegin/OperationEnd");
    }
}

void Tracer::FormatOperationBeginImpl(char* buf,
    size_t bufsz,
    const Operation& op,
    UInt32 depth)
{
    if (depth > DepthSoftLimit) {
        depth = DepthSoftLimit;
    }

    auto& threadState = GetThreadLocalState();
    auto threadID = GetThreadID();

    size_t off = 0;

    Appendf(buf, bufsz, &off, "%sT%llu ", threadState.BeginColor, threadID);

    if (style_ & Style::Hierarchical) {
        Appendf(buf, bufsz, &off, "|");
        for (UInt32 i = 0; i < depth; i++) {
            Appendf(buf, bufsz, &off, "-");
        }
        Appendf(buf, bufsz, &off, " ");
    }

    Appendf(buf, bufsz, &off, "%s", threadState.EndColor);

    Appendf(buf, bufsz, &off, "%s begin", op.Name);

    if (op.PropertyAddress) {
        Appendf(buf,
            bufsz,
            &off,
            " %s",
            PropertySelectorToString(op.PropertyAddress->mSelector).c_str());
    }

    if (op.ClientPID != 0) {
        Appendf(buf, bufsz, &off, " clientPID=%d", op.ClientPID);
    }

    Appendf(buf, bufsz, &off, " objectID=%u", op.ObjectID);

    if (op.PropertyAddress) {
        Appendf(buf,
            bufsz,
            &off,
            " scope=%s",
            PropertyScopeToString(op.PropertyAddress->mScope).c_str());
    }

    if (op.InDataSize != 0 || op.InData != nullptr) {
        Appendf(buf, bufsz, &off, " inSize=%u", op.InDataSize);
    }

    if (op.QualifierDataSize != 0 || op.QualifierData != nullptr) {
        Appendf(buf, bufsz, &off, " qualSize=%u", op.QualifierDataSize);
    }
}

void Tracer::FormatMessageImpl(char* buf, size_t bufsz, const char* message, UInt32 depth)
{
    if (depth > DepthSoftLimit) {
        depth = DepthSoftLimit;
    }

    auto& threadState = GetThreadLocalState();
    auto threadID = GetThreadID();

    size_t off = 0;

    Appendf(buf, bufsz, &off, "%sT%llu ", threadState.BeginColor, threadID);

    if (style_ & Style::Hierarchical) {
        Appendf(buf, bufsz, &off, "|");
        for (UInt32 i = 0; i <= depth; i++) {
            Appendf(buf, bufsz, &off, "-");
        }
        Appendf(buf, bufsz, &off, " ");
    }

    Appendf(buf, bufsz, &off, "%s", threadState.EndColor);

    Appendf(buf, bufsz, &off, "%s", message);
}

void Tracer::FormatOperationEndImpl(char* buf,
    size_t bufsz,
    const Operation& op,
    OSStatus status,
    UInt32 depth)
{
    if (depth > DepthSoftLimit) {
        depth = DepthSoftLimit;
    }

    auto& threadState = GetThreadLocalState();
    auto threadID = GetThreadID();

    size_t off = 0;

    Appendf(buf, bufsz, &off, "%sT%llu ", threadState.BeginColor, threadID);

    if (style_ & Style::Hierarchical) {
        Appendf(buf, bufsz, &off, "|");
        for (UInt32 i = 0; i < depth; i++) {
            Appendf(buf, bufsz, &off, "-");
        }
        Appendf(buf, bufsz, &off, " ");
    }

    Appendf(buf, bufsz, &off, "%s", threadState.EndColor);

    Appendf(buf, bufsz, &off, "%s end", op.Name);

    Appendf(buf, bufsz, &off, " status=%s", StatusToString(status).c_str());

    if (status == kAudioHardwareNoError && op.OutDataSize != nullptr) {
        Appendf(buf, bufsz, &off, " outSize=%u", *op.OutDataSize);
    }
}

void Tracer::PrintImpl(const char* message)
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

bool Tracer::FilterImpl(const Operation& operation)
{
    return true;
}

} // namespace aspl
