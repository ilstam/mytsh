# mytsh

This repository contains my implementation of a simple shell written in C from scratch. The shell's name, _mytsh_, stands for "my toy shell". I used **flex** and **bison** to generate a scanner and a parser for the command line accordingly. Additionally, I used libreadline for line editing features support.

## Features

The shell implements all of the following:

* I/O redirection and pipes (supporting an arbitrary length for the pipe chain)
* executing commands in the background
* support for aliases
* a few shell built-ins such as kill, pwd, etc.
* auto-completion, command history and line editing features
