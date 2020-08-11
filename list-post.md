# Adding a list type to an interpreted language

# Intro 1

If you've found your way to this post then it's likely you've at least heard of Robert Nystrom's fantastic book [Crafting Interpreters](https://craftinginterpreters.com/). For those of you who have finished reading it, congratulations! For those of you who haven't, you seriously need to drop everything and go read it! Robert does a great job of taking his readers from zero to hero and by the end of the book you'll have a deeper sense for how programming languages work and you'll have even built one yourself.

Finishing the book can be a little intimidating though, especially when you want to keep working on the programming language you were building, Clox. That was my experience. Sure, after reading the book I could whip up a parser and was accquainted with garbage collection — but I certainly didn't feel prepared to keep adding features to Clox. I missed the comfort of Robert's witty writing, delightful drawings, and copious code-snippets.

I'm here to help if even only in a small way. You see, I did eventually muster the courage to add a feature to my version of Clox. I added a list data type, and once I'd finished adding the feature I realized it wasn't actually so hard. Turns out Crafting Interpreters taught us a lot more then we thought it did. And if I can add something like lists to Clox then so can you.

Notes
---
Notes, maybe too heavy of a focus on this being only applicable to readers of Crafting Interpreters... Maybe mention it but focus more on the general concept of adding a list data type to a bytecode interpreted language.

### Possible intro 1
- This post will show full process of how to add list data type to an interpreted language
- Will be using Clox from crafting interpreters as a base.
- I was inspired to write this because I was lost after reading his book and couldn't find any resources

### Possible intro 2
- This post will show full process of how to add list data type to an interpreted language
- The language I'm adding it to is Clox (footnote nqq); talk about Clox and Crafting interpreters at a high level
- I'm really writing this for the following persona
- Joma, who just finished Crafting interpreters and is a bit confused on where to go next with building the language. Missing the comfort of Robert and the book
- But you might also find it useful if you are X, Y, Z
- You should go read the book

---

# Overview

I'm going to walk through the entire process of adding a list data type to Clox. All the way from the design to the implementation. Code will be presented for most parts of the implementation, but it will not be comprehensive (every line of code required is included) like the code in Crafting Interpreters is.

When the Clox interpreter runs it starts by lexing source code into a stream of tokens. This stream of tokens can be parsed into an AST which is then compiled into bytecode[^1]. Finally, the bytecode is executed by a VM producing the desired results of the source code.

When I'm thinking about adding a new feature to Clox I tend to work roughly in the opposite direction of the interpreter.

First, I like to think about what the desired outcome of the feature will be and what the corresponding source code would look like. Another way to phrase this is that I'm outlining the semantics of the new feature.

Second, I consider what the runtime implementation will look like. Clox is just an interpreter written in C. Therefore, the semantics will eventually need to be cleanly reprsented by some C code.

Third, is the bytecode it will compile into. The new feature may or may not need some new opcodes to represent the intent of the source code. Sneak peek, adding lists requires new opcodes.

Fourth, I formalize the grammar for the new features syntax. Once that is locked in I go ahead and write the parser.

Finally, I end with what is usually the easiet parse. I add the lexing for any new tokens the feature adds.

It isn't require to work in this direction but I find it much easier. At every step of the process I always have a clear goal for what I need to do. Starting at the end goal helps me avoid situations where I don't have the necessary underlying constructs to achieve a desired result e.g. I don't have the write opcodes to represent what I want to do with lists.

Let's dive in!

Notes
---
- Actually say I implement the things instead of just design backwards?
- Maybe just make that middle part a numbered list?
- Combine lexing and parsing
---

# Semantics and Source Code

We should start with the basics. A list literal can be defined like the following:

```js
// Directly in an expression
print [1, 2, 3]; // Expect: [1, 2, 3]

// In an assignment
let foo = ["a", "b", "c"]
```

It is common to see the items of large lists defined on multiple lines. In many languages it is idiomatic to add a trailing comma in this case. Our list will support this.

```js
let foo = [
    1,
    2,
    ...
    25,
];
```

If all we could with lists is define them at compile time then they would be pretty useless. Naturally, we should be able to access items in the list via an integer index. We'd also like to be able to store new values in the list at a specified index.

```js
let foo = [1, 2];

print foo[0]; // Expect: 1

foo[0] = 2;

print foo[0]; // Expect: 2
```

An important question is how the list will be passed as an argument to a function. We can either pass by value or by reference. We'll choose the latter. This means that a function can modify a list without needing to return a new list to overwrite the original.

```js
fun modifyList(list) {
    list[0] = 0;
}

let foo = [1, 2];
modifyList(foo);
print foo; // Expect: [0, 2]
```

Up to this point we've defined the semantics of something more like an array. That is, it has a fixed length which is set at compile time, and it contains only asingle type of value. To make this list a bit more "listy", let's start by saying it can store multiple value types.

```js
let foo = [1, "b", false, add(1, 2)];
```

Now let's make the list a bit more dynamic. We'd like to be able to grow the list by adding items to end and by deleting items at specific indexes i.e. append and delete. To keep this post as simple as possible I opted for this functionality to exist as native functions instead of as bytecode operations. More on that later.

```js
let foo = [1, 2, 3];

print append(foo, 4); // Expect: nil
// append returns nil because it modifies foo in place. Now foo = [1, 2, 3, 4]

print delete(foo, 2); // Expect: nil
// delete returns nil because it modifies foo in place. Now foo = [1, 2, 4]
```

Great work, users can now grow and shrink there lists as they wish. This concludes the semantics that we will be implementing in this post. A number of valuable list features have been left out for simplicities sake. These include, slicing lists `print foo[1:8];`, reverse indexing `print foo[-1];`, list type conversion (convert a string to a list etc.). For more details on some of these items you can checkout the [challenges](#challenges) section

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

# 

Up until this point are interpreter still can't handle lists end to end. Hypothetically, if we hand compiled some bytecode the interpreter could execute it. But, hand compiling is no fun, so let's automate it.

This is going to require getting a little bit more formal about our syntax and lexical grammars. 



[^1]: In the Clox implementation parsing and compilation have actually been squeezed into a single step. Despite the fact that they are happening at the same time, they are still providing different functions. Parsing is all about turning a flat stream of tokens into a hiearchal structure that represents the intent of the computation. Compilation is about turning that structure into something that is easier and quicker to execute. By squishing these two steps into one we are essentially skipping building the AST. This has the possible benefits of being conceptually simpler and faster but inhibits static analysis of the code.
