## Design

```
"SELECT name FROM users WHERE age > 30"
        │
        ▼
Lexer            character-level cursor: Advance()/Peek()/IsDigit()/...
        │
        ▼
Tokenizer         groups characters into Tokens, recognizes keywords
        │  [SELECT][name][FROM][users][WHERE][age][>][30][EOF]
        ▼
Parser            recursive descent, tokens -> AST
        │
        ▼
Statement (std::variant<CreateTableStmt, DropTableStmt, InsertStmt,
                         UpdateStmt, DeleteStmt, SelectStmt>)
```

Grammar (informal EBNF, exactly what `Parser` implements):

```
statement      := create_stmt | drop_stmt | insert_stmt | update_stmt
                 | delete_stmt | select_stmt ;

create_stmt    := CREATE TABLE ident '(' column_def (',' column_def)* ')' ;
column_def     := ident type [NOT NULL] ;
type           := INT | BIGINT | DOUBLE | BOOLEAN | VARCHAR ['(' int ')'] ;

drop_stmt      := DROP TABLE ident ;

insert_stmt    := INSERT INTO ident ['(' ident (',' ident)* ')']
                   VALUES '(' literal (',' literal)* ')'
                          (',' '(' literal (',' literal)* ')')* ;

update_stmt    := UPDATE ident SET ident '=' literal (',' ident '=' literal)*
                   [where_clause] ;

delete_stmt    := DELETE FROM ident [where_clause] ;

select_stmt    := SELECT (STAR | select_item (',' select_item)*)
                   FROM ident [where_clause]
                   [ORDER BY ident [ASC|DESC]] [LIMIT int] ;
select_item    := (agg_func '(' (STAR | ident) ')' | ident) [AS ident] ;
agg_func       := COUNT | SUM | AVG | MIN | MAX ;

where_clause   := WHERE or_expr ;
or_expr        := and_expr (OR and_expr)* ;
and_expr       := comparison (AND comparison)* ;
comparison     := primary [('=' | '!=' | '<' | '<=' | '>' | '>=') primary] ;
primary        := '(' or_expr ')' | ident | literal ;
literal        := int | float | string | TRUE | FALSE | NULL ;
```


## Complexity

- Tokenizing: O(n) in input length.
- Parsing: O(n) in token count — each token is consumed exactly once by
  the recursive descent (no re-scanning).
- Memory: O(d) recursion depth for expressions (d = AND/OR/paren nesting
  depth), O(n) for the resulting AST.

## Flow: parsing a WHERE clause with mixed AND/OR

```
"WHERE age > 18 AND age < 65 OR name = 'admin'"

ParseWhereClause() consumes WHERE, calls ParseOrExpr()
  ParseOrExpr()
    left = ParseAndExpr()
      left = ParseComparison()   -> (age > 18)
      Match(AND)? yes -> right = ParseComparison() -> (age < 65)
      returns BinaryExpr{ (age>18), AND, (age<65) }
    Match(OR)? yes -> right = ParseAndExpr() -> ParseComparison() -> (name='admin')
    returns BinaryExpr{ [(age>18) AND (age<65)], OR, (name='admin') }
```

