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
- Actually say I implement the things?
- Maybe just make that middle part a numbered list?
---

# Semantics and Source Code

We should start with the basics. A list literal can be defined like the following:

```
// Directly in an expression
print [1, 2, 3]; // Expect: [1, 2, 3]

// In an assignment
let foo = ["a", "b", "c"]
```

It is common to see the items of large lists defined on multiple lines. In many languages it is idiomatic to add a trailing comma in this case. Our list will support this.

```
let foo = [
    1,
    2,
    ...
    25,
];
```

If all we could with lists is define them at compile time then they would be pretty useless. Naturally, we should be able to access items in the list via an integer index. We'd also like to be able to store new values in the list at a specified index.

```
let foo = [1, 2];

print foo[0]; // Expect: 1

foo[0] = 2;

print foo[0]; // Expect: 2
```

An important question is how the list will be passed as an argument to a function. We can either pass by value or by reference. We'll choose the latter. This means that a function can modify a list without needing to return a new list to overwrite the original.

```
fun modifyList(list) {
    list[0] = 0;
}

let foo = [1, 2];
modifyList(foo);
print foo; // Expect: [0, 2]
```

Up to this point we've defined the semantics of something more like an array. That is, it has a fixed length which is set at compile time, and it contains only asingle type of value. To make this list a bit more "listy", let's start by saying it can store multiple value types.

```
let foo = [1, "b", false, add(1, 2)];
```

Now let's make the list a bit more dynamic. We'd like to be able to grow the list by adding items to end and by deleting items at specific indexes i.e. append and delete. To keep this post as simple as possible I opted for this functionality to exist as native functions instead of as bytecode operations. More on that later.

```
let foo = [1, 2, 3];

print append(foo, 4); // Expect: nil
// append returns nil because it modifies foo in place. Now foo = [1, 2, 3, 4]

print delete(foo, 2); // Expect: nil
// delete returns nil because it modifies foo in place. Now foo = [1, 2, 4]
```

Great work, users can now grow and shrink there lists as they wish. This concludes the semantics that we will be implementing in this post. A number of valuable list features have been left out for simplicities sake. These include, slicing lists `print foo[1:8];`, reverse indexing `print foo[-1];`, list type conversion (convert a string to a list etc.). For more details on some of these items you can checkout the [challenges](#challenges) section


[^1]: In the Clox implementation parsing and compilation have actually been squeezed into a single step. Despite the fact that they are happening at the same time, they are still providing different functions. Parsing is all about turning a flat stream of tokens into a hiearchal structure that represents the intent of the computation. Compilation is about turning that structure into something that is easier and quicker to execute. By squishing these two steps into one we are essentially skipping building the AST. This has the possible benefits of being conceptually simpler and faster but inhibits static analysis of the code.
