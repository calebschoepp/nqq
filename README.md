# nqq
C implementation of nqq (modified implementation of clox)

# Features to add
- [ ] Escape sequences in strings
- [ ] Struct type
- [ ] String interpolation
- [ ] Ternary operator
- [ ] Explicit multi-line strings
- [ ] Lists
- [ ] Maps
- [ ] Sets
- [ ] Error handling (error codes and abort)
- [ ] Modules
- [ ] Break statement
- [ ] Continue statement
- [ ] Lambdas
- [ ] Comprehensions
- [x] Multi-line comments (DONE)
- [ ] Postfix/prefix increment/decrement
- [ ] Exponent operator
- [ ] Short hand assignment operators
- [ ] Implicit semi-colons
- [x] Print as a function
- [ ] Modulo operator
- [ ] Bitwise operators
- [x] Assert (Make my debugging life way easier)
- [x] Build an end to end testing frameworks
- [ ] Build a benchmarking framework
- [ ] Decide on wide semantics
- [ ] Optional parameters
- [ ] Variable parameters
- [ ] Pattern matching
- [ ] Int type

# Builtins to add
- [ ] Std I/O
- [ ] File I/O
- [ ] Time
- [ ] Client networking
- [ ] Server networking
- [ ] Basic type conversions
- [ ] Basic type "methods"

# Packages to boostrap
- [ ] Requests
- [ ] HTTP server
- [ ] Regex
- [ ] Testing


# List syntax
let foo = []; // Init empty list
let bar = ["abc", 123]; // Init list with multiple types of values
print(foo[0]); // Runtime error
print(bar[1]); // Output: 123

// To start just init a list and basic indexing
// No negative or range based indexing

# Map syntax
let foo = {}; // Init empty map
let bar = {"abc": 1, 2: "def"}; // Init map with multiple types of key/val pairs
print(foo[1]); // Runtime error
print(bar[2]); // Output: def

# String syntax
let basic = 'this is a basic string';
let num = 1;
let template = "this is a template string, my number ${num + 1}";
let escape = 'basic and template strings can use \'escape characters\'';
let multiline = 'both basic and template strings can run multiple lines via \
    the use of the \\ character immediately followed by a newline in the \
    editor. Convention is to make the following lines tab in a level.'
let raw = `(this is a raw string)*\n where chars are interpreted directly`;

import re;
re.match(raw, basic);