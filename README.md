# NyisBot C++

C++ port of the buggy C# implementation.

License: MIT

## Features

 * Rudimentary IRC client implementation
 * Hotplug dynamic libraries (modules)
 * Various helper classes:
     * Settings file read/write API
     * Generic data container
     * cURL-based communication API
     * Simple CLI argument parser
     * Utility functions

## Setup

1. Clone the repository
2. Setup cmake: `cmake -S . -B build`
3. Build: `cd build && make -j`
4. Add config dir: `ln -s ../config .`
5. Configure
6. Run: `./loader`

## TODO

 * Chat command helper class
 * Modules:
     * Lua
     * Timebomb
     * Liar game
     * Superior UNO
     * ???
     * More stuff
 * Testing

