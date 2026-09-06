cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(
    triax
    VERSION 1.0.0
    LANGUAGES C CXX
)

# PROJECT_IS_TOP_LEVEL was only added in CMake 3.21. Keep compatibility with
# the declared CMake 3.16 minimum.
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(TRIAX_IS_TOP_LEVEL ON)
else()
    set(TRIAX_IS_TOP_LEVEL OFF)
endif()

add_library(triax INTERFACE)
add_library(Triax::Triax ALIAS triax)

target_include_directories(
    triax
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(
    triax
    INTERFACE
        c_std_11
        cxx_std_11
)

if(TRIAX_IS_TOP_LEVEL)
    option(TRIAX_BUILD_EXAMPLES "Build Triax examples" ON)
else()
    option(TRIAX_BUILD_EXAMPLES "Build Triax examples" OFF)
endif()

if(TRIAX_BUILD_EXAMPLES)
    add_executable(triax-example-c examples/basic.c)
    target_link_libraries(triax-example-c PRIVATE Triax::Triax)
endif()

include(CTest)

if(BUILD_TESTING)

    # Strict warning flags are applied only to Triax's own test/fixture
    # executables. They are deliberately not part of Triax::Triax's public
    # interface, so consumers do not inherit the project's warning policy.
    add_library(triax-warnings INTERFACE)

    # Anything linking triax-warnings also links Triax::Triax, which already
    # puts include/ on its (non-SYSTEM) interface include path. Re-declaring
    # it here as SYSTEM, specifically for these strict-warnings consumers,
    # makes the compiler treat triax.h's own internals as a system header for
    # warning purposes — so strict-warning CI measures the test code actually
    # being written against Triax, not triax.h's own implementation details
    # (e.g. its internal float-promotion patterns tripping -Wdouble-promotion).
    # Consumers that don't link triax-warnings are unaffected.
    target_include_directories(
        triax-warnings
        SYSTEM INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    set(TRIAX_STRICT_WARNINGS_COMMON
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wcast-align
        -Wcast-qual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
        -Wswitch-enum
        -Wundef
    )

    set(TRIAX_STRICT_WARNINGS_C
        ${TRIAX_STRICT_WARNINGS_COMMON}
        -Wstrict-prototypes
        -Wmissing-prototypes
    )

    set(TRIAX_STRICT_WARNINGS_CXX
        ${TRIAX_STRICT_WARNINGS_COMMON}
        -Wmissing-declarations
        -Wnon-virtual-dtor
        -Woverloaded-virtual
    )

    # GCC-only diagnostics. These either use GCC-specific spellings/levels or
    # are intentionally kept out of the common GCC/Clang set.
    set(TRIAX_STRICT_WARNINGS_GCC
        -Wcast-align=strict
        -Warray-bounds=2
        -Wformat-overflow=2
        -Wformat-truncation=2
        -Wstringop-overflow=4
    )

    # MSVC warning set based on the commonly used cppbestpractices list.
    # /permissive- is C++-specific and is therefore applied only to C++.
    set(TRIAX_STRICT_WARNINGS_MSVC
        /w14242
        /w14254
        /w14263
        /w14265
        /w14287
        /we4289
        /w14296
        /w14311
        /w14545
        /w14546
        /w14547
        /w14549
        /w14555
        /w14619
        /w14640
        /w14826
        /w14905
        /w14906
        /w14928
    )

    if(MSVC)
        # CMake's MSVC variable is also true for compilers simulating the
        # Visual C++ command-line environment, such as clang-cl.
        target_compile_options(
            triax-warnings
            INTERFACE
                /W4
                ${TRIAX_STRICT_WARNINGS_MSVC}
                $<$<COMPILE_LANGUAGE:CXX>:/permissive->
        )
    else()
        target_compile_options(
            triax-warnings
            INTERFACE
                $<$<COMPILE_LANGUAGE:C>:${TRIAX_STRICT_WARNINGS_C}>
                $<$<COMPILE_LANGUAGE:CXX>:${TRIAX_STRICT_WARNINGS_CXX}>
        )

        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
            target_compile_options(
                triax-warnings
                INTERFACE
                    $<$<COMPILE_LANGUAGE:C>:${TRIAX_STRICT_WARNINGS_GCC}>
            )
        endif()

        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(
                triax-warnings
                INTERFACE
                    $<$<COMPILE_LANGUAGE:CXX>:${TRIAX_STRICT_WARNINGS_GCC}>
            )
        endif()
    endif()

    option(
        TRIAX_WARNINGS_AS_ERRORS
        "Treat warnings in Triax's own test targets as errors"
        OFF
    )

    if(TRIAX_WARNINGS_AS_ERRORS)
        if(MSVC)
            target_compile_options(triax-warnings INTERFACE /WX)
        else()
            target_compile_options(triax-warnings INTERFACE -Werror)
        endif()
    endif()

    # Optional runtime checking for development/CI. Kept separate from normal
    # warning builds because Triax deliberately exercises crashes, exits,
    # signals, fork/CreateProcess, and other behavior that may require
    # sanitizer-specific test filtering or environment settings.
    option(
        TRIAX_ENABLE_SANITIZERS
        "Enable AddressSanitizer and UndefinedBehaviorSanitizer for Triax tests"
        OFF
    )

    if(TRIAX_ENABLE_SANITIZERS)
        if(MSVC)
            message(
                WARNING
                "TRIAX_ENABLE_SANITIZERS is not configured for the MSVC-style "
                "compiler frontend; ignoring it"
            )
        elseif(
            CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$"
            AND CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$"
        )
            add_library(triax-sanitizers INTERFACE)

            target_compile_options(
                triax-sanitizers
                INTERFACE
                    -fsanitize=address,undefined
                    -fno-omit-frame-pointer
            )

            target_link_options(
                triax-sanitizers
                INTERFACE
                    -fsanitize=address,undefined
            )
        else()
            message(
                WARNING
                "TRIAX_ENABLE_SANITIZERS is only configured for GCC/Clang-style "
                "compiler frontends; ignoring it"
            )
        endif()
    endif()

    # Every test here — including ones deliberately designed to fail, crash,
    # or time out — is either plain (core, fixtures: genuinely all-passing)
    # or wrapped in triaxi_validate/triaxi_validate_opts (see
    # tests/triax_selfverify.h), which spawns a filtered child copy of itself
    # and asserts the child produced the designed outcome. That means a
    # deliberate "_fail" test still reports an overall PASS here, so
    # everything can live in one ctest-gated binary instead of needing a
    # separate, unverified demo executable.

    add_executable(
        triax-selftest-c
        tests/c/main.c
        tests/c/core.c
        tests/c/fixtures.c
        tests/c/exits.c
        tests/c/assertions.c
        tests/c/strings.c
        tests/c/memory.c
        tests/c/print.c
        tests/c/inputs.c
        tests/c/arrays.c
        tests/c/errors.c
    )

    target_link_libraries(
        triax-selftest-c
        PRIVATE
            Triax::Triax
            triax-warnings
    )

    target_include_directories(
        triax-selftest-c
        PRIVATE
            tests
    )

    add_executable(
        triax-selftest-cpp
        tests/cpp/main.cpp
        tests/cpp/core.cpp
        tests/cpp/fmt.cpp
        tests/cpp/inputs.cpp
    )

    target_link_libraries(
        triax-selftest-cpp
        PRIVATE
            Triax::Triax
            triax-warnings
    )

    target_include_directories(
        triax-selftest-cpp
        PRIVATE
            tests
    )

    if(TARGET triax-sanitizers)
        target_link_libraries(triax-selftest-c PRIVATE triax-sanitizers)
        target_link_libraries(triax-selftest-cpp PRIVATE triax-sanitizers)
    endif()

    add_test(
        NAME triax-c
        COMMAND triax-selftest-c --no-color
    )

    add_test(
        NAME triax-c-parallel
        COMMAND triax-selftest-c --no-color --jobs=2
    )

    add_test(
        NAME triax-cpp
        COMMAND triax-selftest-cpp --no-color
    )

    add_test(
        NAME triax-cpp-parallel
        COMMAND triax-selftest-cpp --no-color --jobs=2
    )

    # A small, stable-roster binary used only by validate_cli.sh to check
    # CLI-level behavior (--help, --list, --tags, --fail-fast, argument-error
    # handling). Kept separate from triax-selftest-{c,cpp}: their rosters are
    # expected to keep growing, which would make exact fail-fast-ordering and
    # --list-output assertions fragile and unrelated to whatever else changed
    # there. Not registered as its own ctest test — only ever invoked through
    # validate_cli.sh below.

    add_executable(triax-cli-fixture tests/cli_fixture.c)

    target_link_libraries(
        triax-cli-fixture
        PRIVATE
            Triax::Triax
            triax-warnings
    )

    if(TARGET triax-sanitizers)
        target_link_libraries(triax-cli-fixture PRIVATE triax-sanitizers)
    endif()

    # Validates that --json/--tap/--junit output is well-formed by feeding it
    # through a real parser for each format, run against a filtered "child
    # mode" invocation (see validate_formats.sh) so there's a genuine mix of
    # pass/fail/crash/timeout content worth checking escaping against, rather
    # than the outer run's mostly-"passed" triaxi_validate meta-results.
    #
    # Skipped entirely if the required tools aren't found, rather than
    # failing the whole test suite over missing development tooling.

    find_program(TRIAX_BASH_EXECUTABLE bash)
    find_program(TRIAX_XMLLINT_EXECUTABLE xmllint)
    find_program(TRIAX_PYTHON3_EXECUTABLE python3)

    if(
        TRIAX_BASH_EXECUTABLE
        AND TRIAX_XMLLINT_EXECUTABLE
        AND TRIAX_PYTHON3_EXECUTABLE
    )
        add_test(
            NAME triax-c-formats
            COMMAND
                ${TRIAX_BASH_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_formats.sh
                $<TARGET_FILE:triax-selftest-c>
                exits
        )

        add_test(
            NAME triax-cpp-formats
            COMMAND
                ${TRIAX_BASH_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_formats.sh
                $<TARGET_FILE:triax-selftest-cpp>
                cpp_fmt
        )
    else()
        message(
            STATUS
            "triax: skipping output-format validation tests (bash, xmllint, "
            "and python3 are all required; one or more not found)"
        )
    endif()

    # Validates --help/--list/--tags/--fail-fast/argument-error handling
    # against triax-cli-fixture. Needs bash + python3 (for JSON inspection)
    # but not xmllint, unlike the format-validation tests above.

    if(TRIAX_BASH_EXECUTABLE AND TRIAX_PYTHON3_EXECUTABLE)
        add_test(
            NAME triax-cli
            COMMAND
                ${TRIAX_BASH_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_cli.sh
                $<TARGET_FILE:triax-cli-fixture>
        )
    else()
        message(
            STATUS
            "triax: skipping CLI validation tests (bash and python3 are "
            "both required; one or more not found)"
        )
    endif()

    # Pre-release combination coverage: isolation x jobs, timeouts x jobs,
    # parameters x jobs, capture x jobs, crashes x jobs, fixtures x
    # crashes/errors, debug x assertion types, and all reporters x every
    # outcome. Each axis alone is already covered elsewhere; these are only
    # for interactions between axes, driven externally (needs --jobs=/
    # --debug CLI overrides triaxi_validate can't apply from in-process), so
    # — like triax-cli-fixture — these are plain, unwrapped triax_test
    # registrations, not registered as their own ctest test directly.

    add_executable(triax-combo-fixture tests/combo_fixture.c)
    target_link_libraries(triax-combo-fixture PRIVATE Triax::Triax triax-warnings)

    add_executable(triax-combo-fixture-cpp tests/combo_fixture.cpp)
    target_link_libraries(triax-combo-fixture-cpp PRIVATE Triax::Triax triax-warnings)

    # Multi-TU x C/C++: a single suite's tests spread across two translation
    # units, checking the runner aggregates cross-TU registrations correctly
    # — a scenario TRIAX_MULTI_TU exists for but nothing else here actually
    # exercises (elsewhere it's only used to share triax_selfverify.h's own
    # static across files, never to split one suite's tests across TUs).

    add_executable(triax-multitu-c tests/multitu_c_a.c tests/multitu_c_b.c)
    target_link_libraries(triax-multitu-c PRIVATE Triax::Triax triax-warnings)

    add_executable(triax-multitu-cpp tests/multitu_cpp_a.cpp tests/multitu_cpp_b.cpp)
    target_link_libraries(triax-multitu-cpp PRIVATE Triax::Triax triax-warnings)

    if(TARGET triax-sanitizers)
        target_link_libraries(triax-combo-fixture PRIVATE triax-sanitizers)
        target_link_libraries(triax-combo-fixture-cpp PRIVATE triax-sanitizers)
        target_link_libraries(triax-multitu-c PRIVATE triax-sanitizers)
        target_link_libraries(triax-multitu-cpp PRIVATE triax-sanitizers)
    endif()

    if(TRIAX_BASH_EXECUTABLE AND TRIAX_PYTHON3_EXECUTABLE)
        add_test(
            NAME triax-combinations
            COMMAND
                ${TRIAX_BASH_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_combinations.sh
                $<TARGET_FILE:triax-combo-fixture>
                $<TARGET_FILE:triax-combo-fixture-cpp>
                $<TARGET_FILE:triax-multitu-c>
                $<TARGET_FILE:triax-multitu-cpp>
        )
    else()
        message(
            STATUS
            "triax: skipping combination validation tests (bash and python3 "
            "are both required; one or more not found)"
        )
    endif()

endif()
