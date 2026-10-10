# LeakSpy Memory Profiler

LeakSpy is a Linux-based C memory profiling tool that detects memory leaks by tracking dynamic memory allocations.

## Features

- Intercepts `malloc()`, `calloc()`, `realloc()`, and `free()`.
- Reports leaked memory blocks and total leaked bytes.
- Captures stack frames to help investigate leaks.
- Provides an interactive command-line menu.
- Supports report analysis and previous report viewing.
- Warns when memory tracking is incomplete.
- Includes seven regression tests for allocation edge cases.

## Requirements

- Linux
- GCC
- GNU Make
- Standard C development libraries

## Build

Run these commands from the project directory:

    make

To clean and rebuild:

    make clean
    make

## Run the Application

Start the interactive menu:

    ./leakspy

Follow the on-screen menu prompts to run a program, analyze a log, view reports, or access settings and help.

## Run Regression Tests

Run the complete test suite from the terminal, not from inside the interactive menu:

    bash scripts/test_regression.sh

The suite checks memory leak counts and byte totals for the included test programs.

## Reports and Logs

- `reports/` contains generated reports.
- `reports/latest.log` is the documented latest report location.
- `logs/` is available for log files.

Review reported leaked blocks, leaked bytes, allocation details, and any incomplete-tracking warning.

## Project Structure

- `src/` — core profiler implementation
- `cli/` — command-line symbolization utility
- `include/` — header files
- `tests/` — regression test programs
- `scripts/` — test scripts
- `docs/` — additional documentation
- `reports/` — generated reports
- `logs/` — log files
- `Makefile` — build and cleanup commands

## Limitations

LeakSpy is a debugging aid, not a complete memory-safety analyzer. Tracking capacity limits, unsupported allocation behavior, or unavailable symbols can affect report completeness. Read the report warnings when interpreting results.

## Development Verification

After changing the source code, rebuild and test:

    make clean
    make
    bash scripts/test_regression.sh
