SYNTAX GRAMMAR
--------------

```
program        → declaration* EOF ;

declaration    → funDecl | varDecl | statement ;

funDecl        → "fun" function ;
varDecl        → "let" IDENTIFIER ( "=" expression )? ";" ;

statement      → breakStmt
               | continueStmt
               | exprStmt
               | forStmt
               | ifStmt
               | returnStmt
               | whileStmt
               | block ;

breakStmt      → "break" ";" ;
continueStmt   → "continue" ";" ;
exprStmt       → expression ";" ;
forStmt        → "for" "(" ( varDecl | exprStmt | ";" )
                           expression? ";"
                           expression? ")" statement ;
ifStmt         → "if" "(" expression ")" statement ( "else" statement )? ;
returnStmt     → "return" expression? ";" ;
whileStmt      → "while" "(" expression ")" statement ;
block          → "{" declaration* "}" ;

expression     → assignment ;

assignment     → IDENTIFIER assigner assignment
               | IDENTIFIER ( "[" logic_or "]" )+ "=" assignment
               | logic_or ;

logic_or       → logic_and ( "or" logic_and )* ;
logic_and      → equality ( "and" equality )* ;
equality       → comparison ( ( "!=" | "==" ) comparison )* ;
comparison     → addition ( ( ">" | ">=" | "<" | "<=" ) addition )* ;
addition       → multiplication ( ( "-" | "+" ) multiplication )* ;
multiplication → unary ( ( "/" | "*" | "%" ) unary )* ;
unary          → ( "!" | "-" ) unary | power ;
power          → call ( "**" call )* ;
call           → subscript ( "(" arguments? ")" )* ;
subscript      → primary ( "[" logic_or "]" )* ;
primary        → literal | IDENTIFIER | "(" expression ")" ;

function       → IDENTIFIER "(" parameters? ")" block ;
parameters     → IDENTIFIER ( "," IDENTIFIER )* ;
arguments      → expression ( "," expression )* ;
assigner       → "=" | "+=" | "-=" | "*=" | "/=" | "%=" | "**=" ;
literal        → "true" | "false" | "nil"
               | STRING | NUMBER | "[" listDisplay? "]" ;
listDisplay    → logic_or ( "," logic_or )* ( "," )? ;
```

LEXICAL GRAMMAR
---------------

```
STRING         → BASIC | TEMPLATE | RAW ;
BASIC          → "'" <any char except un-escaped "'">* "'" ;
TEMPLATE       → '"' <any char except un-escaped '"'>* '"' ;
RAW            → '`' <any char except '`'>* '`' ;
NUMBER         → DIGIT+ ( "." DIGIT+ )? ;
IDENTIFIER     → ALPHA ( ALPHA | DIGIT )* ;
ALPHA          → 'a' ... 'z' | 'A' ... 'Z' | '_' ;
DIGIT          → '0' ... '9' ;
```

4. Consolidate list object error interface 6. Seperate index checking 8. Pull out OP_INDEX_SUBSCR into a seperate function