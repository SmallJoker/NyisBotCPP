# NyisBot C++

C++ port of the buggy C# implementation. This is an IRC bot
which may support more chat backends in the future.

License: MIT

## Features

 * Rudimentary IRC client implementation
 * Text-based user interface for testing
 * Hotplug dynamic libraries (modules)
 * Various helper classes:
     * Settings file read/write API
     * Generic data container
     * cURL-based communication API
     * Simple CLI argument parser
     * Utility functions

## Setup

Requirements: (using Debian package names)

 * Build setup: `cmake`
 * Any C++17 compiler
 * Networking: `libcurl4-*-dev` (one of them)
 * Threading: `libpthread-stubs0-dev` (maybe)
 * XML parsing: `libpugixml-dev`
 * JSON: [PicoJSON](https://github.com/kazuho/picojson/blob/master/picojson.h)
 * Lua (optional, for Lua module)

Steps:

1. Clone the repository
2. Setup cmake: `cmake -S . -B build`
3. Build: `cd build && make -j`
4. Add config dir: `ln -s ../config .`
5. Configure
6. Run: `./loader`

