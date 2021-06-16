// Copyright (c) libASPL authors
// Licensed under MIT

#include <aspl/Tracer.hpp>

#include "Strings.hpp"

#include <cstdarg>
#include <cstdio>
#include <sstream>

#include <pthread.h>
#include <syslog.h>

namespace aspl {

namespace {

unsigned long GetThreadID()
{
    UInt64 tid = 0;
    pthread_threadid_np(nullptr, &tid);

    return static_cast<unsigned long>(tid);
}

void* CreateDepthCounter()
{
    return new int(0);
}

void DestroyDepthCounter(void* key)
{
    delete static_cast<int*>(key);
}

} // namespace

Tracer::Tracer(Mode mode)
    : mode_(mode)
{
    pthread_key_create(&depthCounter_, DestroyDepthCounter);
}

int& Tracer::GetDepthCounter()
{
    void* ptr = pthread_getspecific(depthCounter_);

    if (!ptr) {
        ptr = CreateDepthCounter();
        pthread_setspecific(depthCounter_, ptr);
    }

    return *static_cast<int*>(ptr);
}

void Tracer::OperationBegin(const Operation& op)
{
    if (mode_ == Mode::Noop) {
        return;
    }

    GetDepthCounter()++;

    std::ostringstream ss;

    ss << "|";
    for (int i = 0; i < GetDepthCounter(); i++) {
        ss << "-";
    }

    if (op.Name) {
        ss << " " << op.Name << " begin";
    }

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

    Print(ss.str().c_str());
}

void Tracer::Message(const char* format, ...)
{
    if (mode_ == Mode::Noop) {
        return;
    }

    va_list args;
    va_start(args, format);

    char message[1024] = {};
    vsnprintf(message, sizeof(message), format, args);

    va_end(args);

    std::ostringstream ss;

    ss << "|";
    for (int i = 0; i <= GetDepthCounter(); i++) {
        ss << "-";
    }

    ss << " " << message;

    Print(ss.str().c_str());
}

void Tracer::OperationEnd(const Operation& op, OSStatus status)
{
    if (mode_ == Mode::Noop) {
        return;
    }

    std::ostringstream ss;

    ss << "|";
    for (int i = 0; i < GetDepthCounter(); i++) {
        ss << "-";
    }

    if (op.Name) {
        ss << " " << op.Name << " end";
    }

    ss << " status=" << StatusToString(status);

    if (status == kAudioHardwareNoError && op.OutDataSize != nullptr) {
        ss << " outSize=" << *op.OutDataSize;
    }

    Print(ss.str().c_str());

    GetDepthCounter()--;
}

void Tracer::Print(const char* message)
{
    switch (mode_) {
    case Mode::Noop:
        return;

    case Mode::Stderr:
        fprintf(stderr, "[aspl] %s\n", message);
        return;

    case Mode::Syslog:
        syslog(LOG_NOTICE, "[aspl] [tid:%lu] %s", GetThreadID(), message);
        return;
    }
}

} // namespace aspl
