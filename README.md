# README for VE482 FA2022 Project 1 - Mum's shell

## Description
Mum's shell, or `mumsh` is a minimalistic implementation of a command-line interface to a POSIX operating system kernel.  

`mumsh` implements all functionalities of the good-old *Thompson shell* `sh`, plus some extensions:

- Support for quotes
- Support for background jobs (on modern systems; MINIX would not benefit from this feature)
  

Note, Mum's shell is by no means a POSIX-compliant shell. Never use it as your system shell!
## Build
`mumsh` is written in C, on Debian Bookworm, and tested thoroughly on it.  
Building `mumsh` on another system requires a *recent C compiler*, *C standard library headers*, and *GNU Make*.  
If you are on some variant of BSD or an ancient Linux, build may not succeed. To build, run

```
make
```
under the source directory. Run `make install` to install `mumsh` to your private `bin` folder. Run `make clean` to remove generated object files and executable.  
## Running
Type `./mumsh` under the source directory to begin using `mumsh`.  
`mumsh` currently has the following functionalities:
 - A basic RPEL
 - GNU Bash-style I/O redirection syntax
 - Arbitrary deep pipes
 - Built-in commands: `pwd` and `cd`
 - Arbirtrary number of quotes
 - Ability to run job in background, and command `job` to check their status
## Limitations
Since `mumsh` is programmed as a course project, it is incomplete and not suitable for daily use.  
Not implemented functions of a standard shell include:
 - Support for escape characters
 - Set and unset environment variables from the command-line
 - Important built-in commands like `test`
 - Support for functions. Together with a lack of `test` make it impossible to script for `mumsh`

Other common functionalites found in a shell but missing in `mumsh` include:
 - Use arrow keys to insert or remove characters
 - Tab completion
 - History trace
## Known Problems
Currently there are none known problems 😃