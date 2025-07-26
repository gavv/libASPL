# AI agent instructions

## Assistant chat

- If file(s) were changed since last user message, assume the changes ware made by user intentionally and retain them.

## About project

libASPL is an open-source C++17 library helping to create macOS CoreAudio Audio Server Plug-Ins (a.k.a User-Space CoreAudio Drivers) with custom virtual devices.

## Platforms & targets

- C++17
- no Objective-C
- macOS-only
- clang-only
- x86_64 and aarch64
- C++ exceptions disabled
- RTTI disabled

## Code style

- Language:

    - Don't use C-style casts
    - Use std::nullopt
    - Use types like SInt32 instead of int32_t

- Naming:

    - PascalCase for classes, functions, methods, constants, globals
    - camelCases for fields, variables
    - trailing underscore for private fields
    - no special rules for for private vs. public methods

- Comments:

    - Doxygen comments in Qt style for classes and public members
    - no doxygen comments for private members and in .cpp files
    - avoid comments in code that just repeat what code does

- File header:

    ```
    // Copyright (c) libASPL authors
    // Licensed under MIT

    #include "aspl/Foo.hpp"
    #include "aspl/Bar.hpp"

    #include "Foo.hpp"
    #include "Bar.hpp"

    #include <foo_3rdparty.h>
    #include <bar_3rdparty.h>

    #include <CoreAudio/Foo.h>
    #include <CoreAudio/Bar.h>

    #include <foo_stdlib>
    #include <bar_stdlib>
    ```

## Tests

- Tests for class Foo are placed into test/TestFoo.cpp.

- Test file structure:

    ```
    <copyright>

    <includes>

    namespace {
    <common constantants>
    } // anonymous namespace

    struct FooTest : testing::Test
    {
        ...
    };

    TEST_F(FooTest, SomeUseCase) // note: global namespace
    {
        <test constants>
        ...
    }

    ...
    ```

- In tests, prefer brief, lower-case comments.

- To check if `std::optional`, `std::unique_ptr`, or `std::shared_ptr` is non-empty/non-null, prefer `ASSERT_TRUE(foo)` over `ASSERT_TRUE(foo.has_value())` / `ASSERT_NE(foo, nullptr)`.

- When a test consists of a few logical sub-tests, try to isolate sub-tests into blocks with local variables.

    For example, instead of:

    ```
    // bundle exe
    auto bundleExe = bundle.getValue("CFBundleExecutable");
    ASSERT_TRUE(bundleExe);
    EXPECT_EQ(*bundleExe, "FooExe");

    // bundle id
    auto bundleID = bundle.getValue("CFBundleIdentifier");
    ASSERT_TRUE(bundleID);
    EXPECT_EQ(*bundleID, "foo.id");
    ```

    Prefer:

    ```
    { // bundle exe
        auto bundleExe = bundle.getValue("CFBundleExecutable");
        ASSERT_TRUE(bundleExe);
        EXPECT_EQ(*bundleExe, "FooExe");
    }

    { // bundle id
        auto bundleID = bundle.getValue("CFBundleIdentifier");
        ASSERT_TRUE(bundleID);
        EXPECT_EQ(*bundleID, "foo.id");
    }
    ```
