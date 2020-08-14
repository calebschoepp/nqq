# Features to add
- [x] Break statement
- [x] Continue statement
- [x] Short hand assignment operators
- [ ] Decide on wide semantics
- [ ] Lists
- [ ] Maps
- [ ] Sets
- [ ] Enums
- [ ] Build a benchmarking framework
- [ ] Iterators
- [ ] Optional parameters
- [ ] Variable parameters
- [ ] Error handling (error codes and abort)
- [ ] Modules
- [ ] Lambdas i.e. anonymous functions
- [ ] Comprehensions
- [ ] Implicit semi-colons
- [ ] Pattern matching
- [ ] Concurrency primitives in syntax
- [x] Multi-line comments
- [x] Exponent operator
- [x] Explicit multi-line strings
- [x] Print as a function
- [x] Modulo operator
- [x] Assert (Make my debugging life way easier)
- [x] Build an end to end testing frameworks
- [x] Escape sequences in strings
- [ ] String interpolation (Not possible without huge refactor of frontend)
- [ ] Struct type (Can I just use maps? Do I need native syntax for this behavior)
- [ ] Int type? (Only bother if project gets serious)
- [ ] Ternary operator? (Do I even want? A ? X : Y === A and X or Y)
- [ ] Bitwise operators? (Only bother if project gets serious)
- [ ] Postfix/prefix increment/decrement (Prefer short hand assignment)

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

# Map syntax
let foo = {}; // Init empty map
let bar = {"abc": 1, 2: "def"}; // Init map with multiple types of key/val pairs
print(foo[1]); // Runtime error
print(bar[2]); // Output: def
