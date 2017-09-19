# Arduino_Basic
Basic programming language in arduino EEPROM

# Requirements
* Arduino

# Sample
    10 rem volume of cube
    20 print "a,b,c:"
    30 input a,b,c
    40 let v=a*b*c
    50 print "V=",v
    60 end

# Operators
There are 26 signed integer (16 bit) variables, from a to z
* end - end of program, stop execution
* rem [some text] - comment
* let {variable}={expression} - set variable
* print "some text" - print text to serial
* input - get variable from terminal
* goto - jump to string
* cls - clear terminal
* save - save program to EEPROM
* load - load program from EEPROM
* list - show program text
* random {variable} - write random value to variable
* if {expression},{string_number}[,else_string_number]
* run - run program
* clear - clear program
* dump - show program memory
* delay {value} - delay
* read {pin},{variable} - digitalRead to variable
* aread {pin},{variable} - analogRead to variable
* sread {variable} - Serial.read tovariable
* write {pin},{variable} - digitalWrite from variable
* awrite {pin},{variable} - analogWrite from variable
* cursor {x},{y} - place cursor to coordinates x,y on terminal
* memory - show free memory

# Expressions
Expression is something like {variable_or_const1}{operation}{variable_or_const2}
Possible operations:
* '+' addiction
* '-' substraction
* '*' multiplication
* '/' division
* '%' modulo
* '&' and
* '|' or
* '^' not
* '=' equal
* '<' less
* '>' more

# Errors
* Ready - no errors
* Unknown command
* Invalid expression
* Divide by zero
* Out of memory
* Invalid argument
* Break (Ctrl+C in terminal)
