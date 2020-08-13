# Adding lists to the interpreted language Clox

Lists are an important data type for any kind of serious programming. The following is a detailed walkthrough of how to add lists to the interpreted programming language Clox. We'll go over the entire design of a list data type including things like there bytecode representation and syntax grammar. There will also be lots of code snippets[^1] goinv over the implementation details.

Clox is the second of two languages you write in the fantastic book [Crafting Interpreters](https://craftinginterpreters.com/) by [Bob Nystrom](https://journal.stuffwithstuff.com/). If you haven't read the book yet, I highly recommend it. This post assumes a fairly high level of familiarty with Clox and Crafting Interpreters, but it should still be a valuable lesson on programming language design if you haven't read the book.

My hope is that this post will help give you the confidence to go and design new features for Clox. When I finished Crafting Interpreters I certainly didn't feel qualified to keep working on the language myself. I missed the comfort of Robert's witty writing, delightful drawings, and copious code-snippets. I'm here to show that if I can add something like lists to Clox then so can you.

# Overview

When the Clox interpreter runs it starts by lexing source code into a stream of tokens. This stream of tokens can be parsed into an AST which is then compiled into bytecode[^2]. Finally, the bytecode is executed by a VM.

When I'm thinking about adding a new feature to Clox, I tend to work roughly in the opposite direction that the interpreter does. My process looks something like this:

1. Think about what the desired outcome of the feature will be and what the corresponding source code would look like. Or in other words, outline the semantics of the feature.

2. Design and implement the feature's runtime. The Clox interpreter is nothing but a big C program. Therefore, the feature's semantics need to be cleanly represented in some C code.

3. Determine the necessary opcodes for the feature. In the VM tie the decoding of these opcodes to the runtime I've just built.

4. Formalize the grammar of the new feature's syntax. With that in hand write the scanning and parsing code to compile bytecode for the feature.

I generally find working in the reverse direction easier. When I start with something like lexing I have hard time understanding how a couple of new lexemes will help me get where I want to go. Flipping the script lets me pretend everything already works and then slowly fill it in. The one disadvantage is that starting at the end often makes testing harder until you have finished.

With this in mind, let's dive in!

# Semantics and Source Code

Semantics is just a fancy way of saying what we want something to do. We should start with the basics. How do we want the user of Clox to define a list literal.

```js
// In an assignment
let foo = ["a", "b", "c"]

// An empty list
let bar = [];

// Directly in an expression
print [1, 2, 3]; // Expect: [1, 2, 3]
```

Commonly you'll see the items of large lists being defined on multiple lines. In many languages it is idiomatic to include a trailing comma. Let's support that.

```js
let foo = [
    1,
    2,
    ...
    25,
];
```

If all we could do with lists is define them at compile time then they would be pretty useless. We want to be able to access items in the list. We'd also like to be able to store new values into the list.

```js
let foo = [1, 2];

print foo[0]; // Expect: 1

foo[0] = 2;

print foo[0]; // Expect: 2
```

Our list will be pass by reference. This means that functions will receive references to list arguments instead of copies. As a result a function can modify a list without returning it.

```js
fun modifyList(list) {
    list[0] = 0;
}

let foo = [1, 2];
modifyList(foo);
print foo; // Expect: [0, 2]
```

Up to this point we've defined the semantics of something more like an array. That is, it has a fixed length set at compile time, and it contains only a single type of value. To make this list a bit more "listy", let's start by saying it can store a mix of any value types.

```js
let foo = [1, "b", false];
```

As a convenience, we also want to be able to use expressions when building, indexing, or storing to lists.

```js
let foo = [1, 1 + 1];
print foo[add(0, 1)]; // Expect: 2

foo[foo[0]] = 7;
print foo[3 - 2]; // Expect: 7
```

Now let's make the list a bit more dynamic. We should be able to append items to the end of the list. Also, there should be a way to delete items at a specific index. In the name of simplicity I opted for this functionality to exist as native functions instead more common alternatives. More on that [later](#challenges).

```js
let foo = [1, 2, 3];

print append(foo, 4); // Expect: nil
// append returns nil because it modifies foo in place. Now foo = [1, 2, 3, 4]

print delete(foo, 2); // Expect: nil
// delete returns nil because it modifies foo in place. Now foo = [1, 2, 4]
```

Great work, users can now grow and shrink there lists as they wish. This concludes the list semantics that we will be implementing in this post. Again for simplicities sake, a number of common list features have been left out. These include, slicing lists `print foo[1:8];`, reverse indexing `print foo[-1];`, and more. For more details on some of these items you can checkout the [challenges](#challenges) section

# Building a Runtime

Having determined the behavior of the lists, we now need to figure out how the interpreter will actually store and operate on them. The runtime is where the rubber hits the road. If the list component of the runtime is poorly written and slow, then you can be sure that lists will be slow in the language too. Considering this, how should we implement lists in Clox? Two data structures immediatley come to mind: dynamic arrays and linked lists.

Let's examine linked lists first. This data structure consists of a number of nodes connected in one direction via pointers — each object has a pointer to the next object in the list. You may recall that a linked list was used in the implementation of Clox to link all the objects together for garbage collection. Leaving the formal proofs for another time a linked list gives us O(n) item access; O(1) item insertion; and O(1) item deletion.

We should note that the stated insertion/deletion complexities assume that you are already holding a pointer to the location you are inserting/deleting into. This is not the case in our runtime as we would only be storing a pointer to the root of the list. Considering this we have O(n) across all operations — not great at all. The final nail in the coffin for linked lists is that the nodes aren't guarenteed to exist in contigous sections of memory. Scattered memory means terrible cache locality and as a result slow speeds.

Maybe dynamic arrays will have more favourable characteristics. Spoiler, it does. Dynamic arrays have been used all across the implementation of Clox. The implementation is quite simple. It is nothing more than a simple array with some additional metadata allowing the program to track when the array should grow. Growing the array is as simple as allocating a larger block of memory and copying all of the data over. Moving on to the algorithmic complexity we see O(1) item access; O(1).....TODODODO. Additionally, because an array is contiguous memory we see much better cache locality. All things considered a dynamic array is a better fit for our list implementation.

To make the dynamic array cooperate with the rest of Clox we will define a new `ObjList` struct that extends the base `obj` struct in `object.h`. Remember that in the Clox implementation an object is just a subtype of Clox value. Other examples of objects include strings, functions, etc.

```c++
typedef struct {
    Obj obj;
    int count;
    int capacity;
    Value* items;
} ObjList;
```

Now let's add the associated functionality of lists to `object.c`. We need functions for retrieving, storing, appending, and deleting items in the list. Additionally, we need a function to build a new list for us and a helper function to verify indices might be nice too. It is a lot of code but keep in mind the functions are mostly simple wrappers around a C array.

```c++
ObjList* newList() {
    ObjList* list = ALLOCATE_OBJ(ObjList, OBJ_LIST);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

void appendToList(ObjList* list, Value value) {
    // Add an item to the end of a list.
    // Length of list will grow by 1 from users perspective.
    // Capacity of internal representation may or may not increase.
    // Expects list and value are already trackable by GC i.e. on stack.
    if (list->capacity < list->count + 1) {
        int oldCapacity = list->capacity;
        list->capacity = GROW_CAPACITY(oldCapacity);
        list->items = GROW_ARRAY(list->items, Value, oldCapacity, list->capacity);
    }
    list->items[list->count] = value;
    list->count++;
    return;
}

void storeToList(ObjList* list, int index, Value value) {
    // Change the value stored at a particular index in a list.
    // Index is assumed to be valid.
    list->items[index] = value;
}

Value indexFromList(ObjList* list, int index) {
    // Index is assumed to be valid.
    return list->items[index];
}

void deleteFromList(ObjList* list, int index) {
    // Index is assumed to be valid
    for (int i = index; i < list->count - 1; i++) {
        list->items[i] = list->items[i+1];
    }
    list->items[list->count - 1] = NIL_VAL;
    list->count--;
}

bool isValidListIndex(ObjList* list, int index) {
    if (index < 0 || index > list->count - 1) {
        return false;
    }
    return true;
}
```

Still with me? Fantastic, because we just implemented the entire runtime for lists. Not too shabby. Note that if you are following along with your own implementation there are still a few things you'll need to add. Notably, type checking macros like `IS_LIST`, function declarations in `object.h` and code to print our new list object.

Notes
---
- Make algo complexity actually match use cases of append, delete, index
- Should I seperate the different functions?
- Code highlight complexity
---

# Operation Opcode

Clox is a bytecode interpreter. That means it compiles source code into a stream of opcodes which it then executes. In order for lists to work we will need to add some new opcodes to Clox. Clox's VM is responsible for decoding the opcode and leveraging the runtime to produce the desired effect. 

The key to designing the opcodes for a new feature is to K.I.S.S. — Keep It Simple Stupid. That is to say, if the current set of opcodes can already support the new semantics you want, then do not add more opcodes. If you aren't that lucky, then try to find the minimal number of opcodes you can add to support the semantics. Minimizing the possible number of opcodes simplifies implementation and helps you keep the common case fast. This comes at the cost of extra overhead decoding the opcodes because less specific opcodes means you need more of them to achieve the same result. From my perspective this is a good tradeoff.

Many parts of this implementation including the grammar and semantics are heavily influenced by the desing of Python. The opcodes are no exception. All of the new semantics we are adding to Clox can be achieved by only three new opcodes: `OP_BUILD_LIST`, `OP_INDEX_SUBSCR`, and `OP_STORE_SUBSCR`.

`OP_BUILD_LIST` does the obvious thing. Notably, the opcode takes an operand `itemCount` that is the number of values on the stack that it should build into the list. Because of the way the source code is compiled it peeks at the items in reverse order and then pops all the items from the stack. When it is finished it pushes the newly built list onto the stack. Here is the switch-case for it in `vm.c`:

```c++
case OP_BUILD_LIST: {
    // before: [item1, item2, ..., itemN] after: [list]
    ObjList* list = newList();
    uint8_t itemCount = READ_BYTE();

    push(OBJ_VAL(list)); // So list isn't sweeped by GC in appendToList
    for (int i = itemCount; i > 0; i--) {
        appendToList(list, peek(i));
    }
    pop();

    while (itemCount-- > 0) {
        pop();
    }
    push(OBJ_VAL(list));
    break;
}
```

`OP_INDEX_SUBSCR` stands for *index subscript* — meaning access an item at a particular index. It's corresponding code in the VM is simple and only looks complex due to a plethora of error handling. Simply put, it pops a list and index off of the stack, runs `indexFromList` and pushes the resulting value back on the stack.

```c++
case OP_INDEX_SUBSCR: {
    // before: [list, index] after: [index(list, index)]
    Value index = pop();
    Value list = pop();
    Value result;
    if (IS_LIST(list)) {
        ObjList* list = AS_LIST(list);
        if (!IS_NUMBER(index)) {
            runtimeError("List index is not a number.");
            return INTERPRET_RUNTIME_ERROR;
        } else if (!isValidListIndex(list, AS_NUMBER(index))) {
            runtimeError("List index out of range.");
            return INTERPRET_RUNTIME_ERROR;
        }
        result = indexFromList(list, AS_NUMBER(index));
    } else {
        runtimeError("Invalid type to index into.");
        return INTERPRET_RUNTIME_ERROR;
    }
    push(result);
    break;
}
```

`OP_STORE_SUBSCR` of course means *store subscript*. Instead of pulling a value from the list, this time, we are storing a value in the list. Again, this is a simple thing made verbose by error handling. A list, index, and item are popped from the stack. The item is then stored in the list at the particular index.

```c++
case OP_STORE_SUBSCR: {
    // before: [list, index, item] after: [item]
    Value item = pop();
    Value index = pop();
    Value list = pop();
    if (!IS_LIST(list)) {
        runtimeError("Cannot store value in a non-list.");
        return INTERPRET_RUNTIME_ERROR;
    } else if (!IS_NUMBER(index)) {
        runtimeError("List index is not a number.");
        return INTERPRET_RUNTIME_ERROR;
    } else if (!isValidListIndex(AS_LIST(list), AS_NUMBER(index))) {
        runtimeError("Invalid list index.");
        return INTERPRET_RUNTIME_ERROR;
    }
    storeToList(AS_LIST(list), AS_NUMBER(index), item);
    push(item);
    break;
}
```

And that concludes designing the opcodes for lists in Clox. For a full implementation be sure to update `debug.c` with switch-cases for the new opcodes.

# The Power of Parsing

Up until this point our interpreter still can't handle lists end to end. Hypothetically, if we hand compiled some bytecode the interpreter could execute it. But, hand compiling is no fun, so let's automate it.

This is going to require getting a little bit more formal about our syntax's [grammar](https://craftinginterpreters.com/appendix-i.html). I've shown an excerpt of the grammar below that includes the modifications we will be making.

```plain
...

assignment     → ( call "." )? IDENTIFIER ( "[" logic_or "]" )* "=" assignment
               | logic_or ;

...

call           → subscript ( "(" arguments? ")" | "." IDENTIFIER )* ;
subscript      → primary ( "[" logic_or "]" )* ;
primary        → "true" | "false" | "nil" | "this"
               | NUMBER | STRING | IDENTIFIER | "(" expression ")"
               | "super" "." IDENTIFIER | "[" list_display? "]" ;

list_display    → logic_or ( "," logic_or )* ( "," )? ;

...
```

The biggest change is that we have added a list literal to the `primary` rule. It uses the utility rule `list_display` which accepts multiple comma seperated items. The items are any valid expression except for an assignment. `list_display` also supports an optional trailing comma like we wanted.

To support indexing from lists, we've added a new rule `subscript`. It has a higher precedence then call so that you could as an example index into a list for a function and call it (`myFunctions[0]()`). For storing to lists we updated the `assignment` rule. For both, a valid index is any expression excpet an assignment.

With a formal grammar in hand, we can tie this new feature into our parser. First we add two new rules to the Pratt parsing table. When we encounter a `[` in a prefix scenario we parse a list literal. An infix `[` kicks off parsing of a subscript (index or store).

```c++
{ list, subscript, PREC_SUBSCRIPT }, // TOKEN_LEFT_BRACKET
{ NULL, NULL,      PREC_NONE },      // TOKEN_RIGHT_BRACKET
```

Below is the code to parse and compile a list literal. By the time `list` is entered the left bracket has already been consumed. If it doesn't immediatley find a right bracket it starts comma seperated items one at a time. Each cycle it verifies it isn't just at a trailing comma. Finally it emits an `OP_BUILD_LIST` and the `itemCount` operand.

```c++
static void list(bool canAssign) {
    int itemCount = 0;
    if (!check(TOKEN_RIGHT_BRACKET)) {
        do {
            if (check(TOKEN_RIGHT_BRACKET)) {
                // Trailing comma case
                break;
            }

            parsePrecedence(PREC_OR);

            if (itemCount == UINT8_COUNT) {
                error("Cannot have more than 256 items in a list literal.");
            }
            itemCount++;
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after list literal.");

    emitByte(OP_BUILD_LIST);
    emitByte(itemCount);

    return;
}
```

The code is a bit simpler for a subscript. First it parse the index via `parsePrecedence(PREC_OR)`. Then if it sees an equal sign it knows it needs to emit an `OP_STORE_SUBSCR`. Otherwise a `OP_INDEX_SUBSCR` is emitted.

```c++
static void subscript(bool canAssign) {
    parsePrecedence(PREC_OR);
    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after index.");

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitByte(OP_STORE_SUBSCR);
    } else {
        emitByte(OP_INDEX_SUBSCR);
    }
    return;
}
```

Naturally, the parser expects that the source code has already been tokenized by the scanner. To make lists work we need to handle the cases for two new tokens. Thanks to the good design of Clox adding this is trivial.

```c++
case '[': return makeToken(TOKEN_LEFT_BRACKET);
case ']': return makeToken(TOKEN_RIGHT_BRACKET);
```


Notes
-----
- assignment grammar is wrong I think
-----

# The Rest of It

Alright, let's come up for air because that was a lot of code. We should be proud of ourselves though we just worked through an end to end implementation of a new data type. Lists will certainly make programming in Clox much nicer.

The astute reader may have realized that we are still missing a few key parts of the implementation to really be call it done. In fact we still need to do two things: implement `append` and `delete` functions, and update the garbage collector.

## Append and Delete

`append` and `delete` will be available to the user through the native functions interface. Both functions are simple wrappers around the runtime we have already built earlier in the post. Notably, the Clox native function interface does not handle errors so this has been left as an exercise to the reader.

```c++
static Value appendNative(int argCount, Value* args) {
    // Append a value to the end of a list increasing the list's length by 1
    if (argCount != 2 || !IS_LIST(*args)) {
        // Handle error
    }
    ObjList* list = AS_LIST(*args);
    Value item = *(args + 1);
    appendToList(list, item);
    return NIL_VAL;
}

static Value deleteNative(int argCount, Value* args) {
    // Delete an item from a list at the given index.
    // Every item past the deleted item has it's index decreased by 1.
    if (argCount != 2 || !IS_LIST(*args) || !IS_NUMBER(*(args + 1))) {
        // Handle error
    }

    ObjList* list = AS_LIST(*args);
    int index = AS_NUMBER(*(args + 1));

    if (!isValidListIndex(list, index)) {
        // Handle error
    }

    deleteFromList(list, index);
    return NIL_VAL;
}
```

## Garbage Collection

Of all the parts of this implementation, ensuring that garbage collection was working correctly was what I found to be the most difficult. On it's face it is relatively simple. To wire up lists to the collector we just needed to handle two switch-cases in `memory.c`. In `blackenObject` all we need to do is mark every item in the list.

```c++
case OBJ_LIST: {
    ObjList* list = (ObjList*)object;
    for (int i = 0; i < list->count; i++) {
        markValue(list->items[i]);
    }
    break;
}
```
In `freeObject` we want to release the memory of the list as it is no longer needed. This requires both freeing the dynamic array and the object itself.

```c++
case OBJ_LIST: {
    ObjList* list = (ObjList*)object;
    FREE_ARRAY(Value*, list->items, list->count);
    FREE(ObjList, object);
    break;
}
```

The devil is in the details with garbage collection. After multiple days of testing and careful inspection of the code I found two bugs in my implementation. The fixes have already been included in the code of this post but I'll outline the bugs anyways.

1. In the `OBJ_LIST` switch-case of `freeObject` I was only freeing the dynamic array. By forgetting to free the object itself I had introduced a memory leak.

2. You may have noticed the peculiar `pop` and `push` statements for `OP_BUILD_LIST`. Those are there because `appendList` allocates memory which may kick off a garbage collection. Since `list` isn't rooted anywhere it would get sweeped away. Pushing it onto the stack prevents that.

```c++
push(OBJ_VAL(list)); // So list isn't sweeped by GC in appendToList
for (int i = itemCount; i > 0; i--) {
    appendToList(list, peek(i));
}
pop();
```

# Challenges

Congratulations! We've just finished a complete implementation of a performant list data type. This is no small feat. Programming in Clox will certainly be a lot easier now that we can store lists of data. If you are looking for more things to explore beyond what we've gone over in this post then you are in luck. Following in the footsteps of Crafting Interpreters, I've included some challenges below. Thanks for reading!

1. More fully featured languages often present a wider variety of ways to access items in a list. This includes negative indexing and slicing. Negative indexing is quite simple; an index of `-1` accesses the last item, `-2` the second last, and so forth. Slicing allows a user to easily extract and operate on portions of a list. In Python something like `myList[2:8:2]` would take every second item of the list starting at index `2` and going to index `8`. Add negative indexing and a native function with the signature `Value slice(start, stop, step)` to Clox.

2. Making `append` and `delete` native functions was the easiest to implement but is uncommon in other languages. Two other options exist. These could be keywords that form a statement e.g. `del foo[0]`. This statement would then be compiled down into some new opcodes. Alternatively, append and delete could be made into methods on a list object. This would require some more rewiring of Clox but would perhaps be more idiomatic. Pick the approach you prefer and try implementing it.

3. Most languages have a unified theory on iterable types. This includes how you work with lists/arrays, iterators, strings, generators and more. Currently, our implementation is pretty lacking in this area. Research on how other languages implement iterators and try adding them to Clox. Additionally, try adding the ability to index individual characters of a Clox string.

4. The current implementation of `delete` only removes an item and reshuffles the content in the array as necessary. Hypothetically, if a very large list was deleted from enough then it would have too much memory allocated to it for the number of items it is storing. Try adding some logic to `delete` to deallocate memory when the number of list items becomes small enough.

[^1]: Lots of code will be shown but it will not be comprehensive. Not every line of code required for a complete implementation will be provided.

[^2]: In the Clox implementation parsing and compilation have actually been squeezed into a single step. Despite the fact that they are happening at the same time, they are still providing different functions. Parsing is all about turning a flat stream of tokens into a hiearchal structure that represents the intent of the computation. Compilation is about turning that structure into something that is easier and quicker to execute. By squishing these two steps into one we are essentially skipping building the AST. This has the possible benefits of being conceptually simpler and faster but inhibits static analysis of the code.
