# Moore Automaton Simulator

This project is a **dynamic C library** implementing a simulator for Moore finite state machines (FSM). The library supports creating automata with customizable transition and output functions, connecting and disconnecting automata, and simulating synchronous steps across multiple machines.

## Notes

This project was implemented as part of an academic assignment in the course **Computer Architecture and Operating Systems**

## Features

- Memory safety with error handling and support for valgrind verification.
- Modular and efficient bit-level representation of state and signals.
- Custom state size and input/output signal count.
- Support for connecting outputs of one automaton to inputs of another.
- Manual input and state setting.
- Output query and synchronous simulation step across automata.

## File structure

```text
project-root/
├── Makefile              # Build configuration file
├── ma.c                  # Implementation of the Moore automaton
├── ma.h                  # Header file for the automaton interface (externally provided)
├── memory_tests.c        # Memory tracking for test purposes (externally provided)
├── memory_tests.h        # Header for memory tracking (externally provided)
└── ma_example.c          # Example usage of the library (externally provided)
```

## How to Build and Run

This project requires **Linux** and the **GCC toolchain**.

### Prerequisites

Make sure you have the following installed:

- `gcc`
- `make`

### Run the Example
To build and run the example program using the library:

```bash
make run_example
```
This compiles ma_example.c and runs it with a test argument.

### Clean the Build
To remove all generated files:

```bash
make clean
```


