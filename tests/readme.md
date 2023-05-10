# sleepdart tests

sleepdart provides a basic facility for performing automated regression tests.

## Adding new tests

Adding a new test is as simple as creating a new directory with a configuration file, as well as any required dependencies.

Each test consists of a `sleepdart-test.ini` file, which may look similar to this:

```ini
# A file to be loaded at the start of the test.
# Preferably a savestate to ensure consistent starting state.
file=snapshot.szx

# Type of test stop condition, can be e.g. "frame" or "breakpoint".
stop-condition=frame

# Stop value meaning depends on condition type:
# * breakpoint - pc register value
# * frame - frame number
stop-value=42

# Determines the test scope, i.e. what emulator state is being validated.
#
# Scopes like "registers" or "allflags" put the state through a hash function
# on each iteration, meaning the test will fail on even the slightest deviation.
# This can be a good or bad thing depending on circumstances. Use carefully.
#
# The "print" scope hooks into the ROM character print routine, then compares
# the text output with the expected one. This is a very useful option for running
# various test programs which print out the results, such as Patrik Rak's Z80 tests.
scope=docflags allflags registers cycles print

# Specifies a macro file to be used.
# Used to automate keyboard inputs, useful for running applications
# which cannot reach the desired state when running unattended.
macro=macro.txt
```

When running the test, reference results for each test scope will be saved (if they don't exist already). This means the first test run should be performed on an emulator version with known good behavior.

### Keyboard macros

A keyboard macro file consist of lines specifying a frame, command type and value. Elements are delimited by spaces. Commands can be `key` or `goto`.

Key values are `address_line * 5 + bit`. For example, ENTER key is address line 6, bit 0, which resolves to key value of 30. See [48K ZX Spectrum Reference](https://worldofspectrum.org/faq/reference/48kreference.htm#Hardware) for details.

Goto command simply sets the current frame number and is a simple way to make a macro run indefinitely.

Below is an example of a macro that presses the ENTER key every 100 frames.

```
100 key 30
101 goto 1
```

