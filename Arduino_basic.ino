#include <EEPROM.h>

#define SERIAL_GREETING F("Arduino basic v1.0")
#define SERIAL_PROMPT F(">")
#define MESSAGE_FREE_MEMORY F("Free memory: ")

#define BUZZER_PIN 3

#define CODE_RUN 1
#define MESSAGE_READY F("Processing")
#define CODE_READY 2
#define MESSAGE_READY F("Ready")
#define CODE_UNKNOWN_COMMAND 3
#define MESSAGE_UNKNOWN_COMMAND F("Unknown command")
#define CODE_INVALID_EXPRESSION 4
#define MESSAGE_INVALID_EXPRESSION F("Invalid expression")
#define CODE_DIVIDE_BY_ZERO 5
#define MESSAGE_DIVIDE_BY_ZERO F("Divide by zero")
#define CODE_OUT_OF_MEMORY 6
#define MESSAGE_OUT_OF_MEMORY F("Out of memory")
#define CODE_INVALID_ARGUMENT 7
#define MESSAGE_INVALID_ARGUMENT F("Invalid argument")
#define CODE_BREAK 8
#define MESSAGE_BREAK F("Break")

#define OPERATOR_LEN 10
#define COMMAND_LEN 80
#define PROGRAM_LEN 500
#define STACK_LEN 10
#define INPUT_LEN 10

#define COMMAND_END 0x81
#define COMMAND_REM 0x82
#define COMMAND_LET 0x83
#define COMMAND_PRINT 0x84
#define COMMAND_INPUT 0x85
#define COMMAND_GOTO 0x86
#define COMMAND_CLS 0x87
#define COMMAND_SAVE 0x88
#define COMMAND_LOAD 0x89
#define COMMAND_BEEP 0x8a
#define COMMAND_LIST 0x8b
#define COMMAND_RND 0x8c
#define COMMAND_IF 0x8d
#define COMMAND_THEN 0x8e
#define COMMAND_RUN 0x8f
#define COMMAND_CLEAR 0x90
#define COMMAND_DUMP 0x91
#define COMMAND_DELAY 0x92
#define COMMAND_RANDOM 0x93
#define COMMAND_READ 0x94
#define COMMAND_AREAD 0x95
#define COMMAND_WRITE 0x96
#define COMMAND_AWRITE 0x97
#define COMMAND_CURSOR 0x98
#define COMMAND_MEMORY 0x99
#define COMMAND_SREAD 0x9a

const unsigned char PROGMEM pgm_command_codes[]="\
\x81 end\n\
\x82 rem\n\
\x83 let\n\
\x84 print\n\
\x85 input\n\
\x86 goto\n\
\x87 cls\n\
\x88 save\n\
\x89 load\n\
\x8a beep\n\
\x8b list\n\
\x8c rnd\n\
\x8d if\n\
\x8e then\n\
\x8f run\n\
\x90 clear\n\
\x91 dump\n\
\x92 delay\n\
\x93 random\n\
\x94 read\n\
\x95 aread\n\
\x96 write\n\
\x97 awrite\n\
\x98 cursor\n\
\x99 memory\n\
\x9a sread\n\
";

unsigned char program[PROGRAM_LEN];

// Значения переменных (a-z)
int variables[26];

// Переменные выполнения
unsigned char runtime_current_str;
unsigned char runtime_error_flag;
unsigned char serial_code;
//int runtime_stack[STACK_LEN];

// =================================================
// Прочитать строку с клавиатуры
// =================================================
unsigned char read_str(unsigned char *buff,unsigned char max_len) {
  int input_byte;
  int index;
  memset(buff,0,max_len);
  index=0;
  do {
    while(Serial.available()>0) {
      input_byte=Serial.read();
      switch(input_byte) {
        // Ctrl+C
        case 3:
          return 0;
          break;
        // Backspace
        case 8: // Ctrl+H
        case 127: // Backspace
          // Стираем первый байт
          if(index>0) {
            Serial.print((char)input_byte);
            index--;
          }
          buff[index]=0;
          // Поддержка двухбайтовых символов
          // Если второй байт - начало юникодового символа - стираем его тоже
          if(index>0 && buff[index-1]>=0xc0 && buff[index-1]<=0xdf) {
            index--;
            buff[index]=0;
          }
          break;
        // Enter
        case 10:
        case 13:
          Serial.println("");
          break;
        // Прочие символы
        default:
          // Если символ отображаемый или это второй байт юникода
          if(index+1<max_len && (isPrintable(input_byte) || (input_byte>=0x80 && input_byte<=0xbf)))
            {
            buff[index]=input_byte;
            Serial.print((char)input_byte);
            index++;
          // Двухбайтовые символы - первый байт юникода
          } else if(index+2<max_len && (input_byte>=0xc0 && input_byte<=0xdf)) {
            buff[index]=input_byte;
            Serial.print((char)input_byte);
            index++;
          } 
          break;
      }
    }
  } while(input_byte!=13 && input_byte!=10);
  return 1;
}

// =================================================
// Сообщение об ошибке или готовности
// =================================================
void message_error_code() {
  if(runtime_current_str>0) {
    Serial.print(runtime_current_str);
    Serial.print(' ');
  }
  switch(runtime_error_flag) {
    case CODE_READY: Serial.println(MESSAGE_READY); break;
    case CODE_UNKNOWN_COMMAND: Serial.println(MESSAGE_UNKNOWN_COMMAND); break;
    case CODE_INVALID_EXPRESSION: Serial.println(MESSAGE_INVALID_EXPRESSION); break;
    case CODE_DIVIDE_BY_ZERO: Serial.println(MESSAGE_DIVIDE_BY_ZERO); break;
    case CODE_OUT_OF_MEMORY: Serial.println(MESSAGE_OUT_OF_MEMORY); break;
    case CODE_INVALID_ARGUMENT: Serial.println(MESSAGE_INVALID_ARGUMENT); break;
    case CODE_BREAK: Serial.println(MESSAGE_BREAK); break;
  }
}

// =================================================
// Получить код команды
// =================================================
unsigned char get_cmd_code(unsigned char *single_command) {
  int i;
  unsigned char command_code;
  int command_index;
  // Код команды
  // В начале и после каждого символа перевода строки равен 0
  // Далее пока есть совпадение равен коду команды
  // Когда совпадения нет равен -1
  command_code=0;
  for(i=0;i!=strlen_P(pgm_command_codes);i++) {
    /*Serial.print(i);
    Serial.print(" ");
    Serial.print((int)pgm_read_byte_near(pgm_command_codes+i));
    Serial.print(" ");
    Serial.print((int)single_command[command_index]);
    
    Serial.print(" ");
    Serial.print((int)command_code);
    
    Serial.println("");
    */
    if(command_code==0) {
      command_code=(unsigned char)pgm_read_byte_near(pgm_command_codes+i);
      command_index=0;
    } else if(pgm_read_byte_near(pgm_command_codes+i)==' ') {
      continue;
    } else if(pgm_read_byte_near(pgm_command_codes+i)=='\n') {
      if(single_command[command_index]=='\0' && command_code!=255) {
        return command_code;
      }
      command_code=0;
    } else {
      if(single_command[command_index]!=(unsigned char)pgm_read_byte_near(pgm_command_codes+i)) {
        command_code=255;
      }
      command_index++;
    }
  }
  return command_code;
}

// =================================================
// Получить смещение аргумента в строке
// =================================================
int sub_argument(unsigned char *argument,unsigned char count) {
  enum {ARG_ORDINARY,ARG_QUOTE} state;
  int offset;
  offset=0;
  state=ARG_ORDINARY;
  while(count!=0 && argument[offset]!='\0') {
    if(argument[offset]=='"') {
      if(state==ARG_ORDINARY) state=ARG_QUOTE;
      else if(state==ARG_QUOTE) state=ARG_ORDINARY;
    } else if(state==ARG_ORDINARY && argument[offset]==',') {
      count--;
    }
    offset++;
  }
  return offset;
}

// =================================================
// Вычислить выражение
// =================================================
int calc_expression(unsigned char *expr) {
  int i,result,operand;
  unsigned char operation;
  result=0;
  operand=0;
  operation=0;
  for(i=0;expr[i]!=',' && expr[i]!='\0';i++) {
    if(isdigit(expr[i])) {
      operand=operand*10+(expr[i]-'0');
    } else if(expr[i]>='a' && expr[i]<='z') {
      operand=variables[expr[i]-'a'];
    } else {
      if(operation=='+') result+=operand;
      else if(operation=='-') result-=operand;
      else if(operation=='*') result*=operand;
      else if(operation=='%') result%=operand;
      else if(operation=='/') {
        if(operand!=0) {
          result/=operand;
        } else {
          runtime_error_flag=CODE_DIVIDE_BY_ZERO;
          return;
        }
      } else if(operation=='&') result&=operand;
      else if(operation=='|') result|=operand;
      else if(operation=='^') result^=operand;
      else if(operation=='=') result=(result==operand);
      else if(operation=='>') result=(result>operand);
      else if(operation=='<') result=(result<operand);
      else result=operand;
      operand=0;
      operation=0;
      if(expr[i]=='+' ||
         expr[i]=='-' ||
         expr[i]=='*' ||
         expr[i]=='%' ||
         expr[i]=='/' ||
         expr[i]=='&' ||
         expr[i]=='|' ||
         expr[i]=='^' ||
         expr[i]=='=' ||
         expr[i]=='>' ||
         expr[i]=='<') {
        operation=expr[i];
      } else {
        runtime_error_flag=CODE_INVALID_EXPRESSION;
        return;
      }
    }
  }
  if(operation=='+') result+=operand;
  else if(operation=='-') result-=operand;
  else if(operation=='*') result*=operand;
  else if(operation=='%') result%=operand;
  else if(operation=='/') {
    if(operand!=0) {
      result/=operand;
    } else {
      runtime_error_flag=CODE_DIVIDE_BY_ZERO;
      return;
    }
  } else if(operation=='&') result&=operand;
  else if(operation=='|') result|=operand;
  else if(operation=='^') result^=operand;
  else if(operation=='=') result=(result==operand);
  else if(operation=='>') result=(result>operand);
  else if(operation=='<') result=(result<operand);
  else result=operand;
  return result;
}

// =================================================
// Сдвиг блока в памяти
// =================================================
void block_move(unsigned char *mem,int len,int from,int to) {
  int i;
  /*Serial.print("block_move(");
  Serial.print(mem);
  Serial.print(",");
  Serial.print(len);
  Serial.print(",");
  Serial.print(from);
  Serial.print(",");
  Serial.print(to);
  Serial.println(");\n");
  delay(2000);
  */
  //return;
  // Если from>to, то сдвигать надо назад
  // Начинаем с первого байта и двигаемся вперёд
  if(from>to) {
    for(i=to;i!=len;i++) {
      if((i+from-to)>=len) {
        mem[i]=0;
      } else {
        mem[i]=mem[i+from-to];
      }
    }
  // Если from<to, то сдвигаем вперёд
  // Начинаем с последнего байта и двигаемся назад
  } else if(from<to) {
    for(i=len-1;i>=to;i--) {
      if((i+from-to)<0) {
        mem[i]=0;
      } else {
        mem[i]=mem[i+from-to];
      }
    }
  }
}

// =================================================
// Сколько ещё места для программы?
// =================================================
int get_free_space() {
  int i;
  for(i=0;i!=PROGRAM_LEN;i++) {
    if(program[PROGRAM_LEN-1-i]!='\0') break;
  }
  return i;
}

// =================================================
// Дописать строку в программу
// =================================================
void command_append(int str_number,unsigned char command_code,unsigned char *argument) {
  int i;
  enum {PROGRAM_LINE_NUMBER,PROGRAM_OPERATION,PROGRAM_ARGUMENT} state;
/*
  Serial.print("command_append(");
  Serial.print(str_number);
  Serial.print(",");
  Serial.print((int)command_code);
  Serial.print(",");
  Serial.print(argument);
  Serial.println(");");
  delay(2000);
  */
  state=PROGRAM_LINE_NUMBER;
  for(i=0;i!=PROGRAM_LEN;i++) {
    if(state==PROGRAM_LINE_NUMBER) {
      // Если в программе есть такая строка - удаляем её
      if(str_number==program[i]) {
        block_move(program,PROGRAM_LEN,i+strlen(program+i)+sizeof(char),i);
      }
      
      if(get_free_space()<=(sizeof(char)+sizeof(char)+strlen(argument)+sizeof(char))) {
        runtime_error_flag=CODE_OUT_OF_MEMORY;
        Serial.print(MESSAGE_FREE_MEMORY);
        Serial.println(get_free_space());
        return;
      }
      
      // Вставляем новую в нужное место
      if(command_code!=0 && (str_number<program[i] || program[i]==0)) {
        //                                 N строки     операция     аргумент         конец строки
        block_move(program,PROGRAM_LEN,i,i+sizeof(char)+sizeof(char)+strlen(argument)+sizeof(char));
        program[i]=str_number;
        program[i+1]=command_code;
        /*Serial.println("Insert ");
        Serial.println(i+2);
        Serial.println(command_code);*/
        strcpy(program+i+2,argument);
        Serial.print(MESSAGE_FREE_MEMORY);
        Serial.println(get_free_space());
        break;
      }
      state=PROGRAM_OPERATION;
    } else if(state==PROGRAM_OPERATION && program[i]=='\0') {
      state=PROGRAM_LINE_NUMBER;
    }
  }
/*  for(i=0;i!=40;i++) {
    Serial.println((int) program[i]);
  }*/
}

// =================================================
// Сохранить в EEPROM
// =================================================
void command_save(unsigned char *argument) {
  int i;
  if(strlen(argument)==0) {
    for(i=0;i!=PROGRAM_LEN;i++) {
      EEPROM.update(i,program[i]);
    }
  } else {
    runtime_error_flag=CODE_INVALID_ARGUMENT;
  }
}

// =================================================
// Загрузить из EEPROM
// =================================================
void command_load(unsigned char *argument) {
  int i;
  if(strlen(argument)==0) {
    for(i=0;i!=PROGRAM_LEN;i++) {
      program[i]=EEPROM.read(i);
    }
  } else {
    runtime_error_flag=CODE_INVALID_ARGUMENT;
  }
}

// =================================================
// Стереть программу во временной памяти
// =================================================
void command_clear(unsigned char *argument) {
  int i;
  if(strlen(argument)==0) {
    for(i=0;i!=PROGRAM_LEN;i++) {
      program[i]=0;
    }
  } else {
    runtime_error_flag=CODE_INVALID_ARGUMENT;
  }
}

// =================================================
// Дамп
// =================================================
void command_dump(unsigned char *argument) {
  int i;
  if(strlen(argument)==0) {
    for(i=0;i!=PROGRAM_LEN;i++) {
      Serial.print((int)program[i]);
      if(((i+1)%20)==0) Serial.println("");
      else Serial.print(" ");
    }
  } else {
    runtime_error_flag=CODE_INVALID_ARGUMENT;
  }
}

// =================================================
// Очистка экрана
// =================================================
void command_cls(unsigned char *argument) {
  int i;
  if(strlen(argument)==0) {
    Serial.print((char)12);
  } else {
    runtime_error_flag=CODE_INVALID_ARGUMENT;
  }
}

// =================================================
// Выполнить команду cursor x,y
// =================================================
void command_cursor(unsigned char *argument) {
  int x;
  int y;
  // Первый аргумент - абсцисса
  // Второй - ордината
  if(argument[sub_argument(argument,0)]=='\0' || argument[sub_argument(argument,1)]=='\0') {
    runtime_error_flag=CODE_INVALID_ARGUMENT;
  } else {
    x=calc_expression(argument+sub_argument(argument,0));
    y=calc_expression(argument+sub_argument(argument,1));
    if(x>=0 && y>=0) {
      Serial.print((char)27); //ESC
      Serial.print('[');
      Serial.print(y); // y
      Serial.print(';'); // ;
      Serial.print(x); // x
      Serial.print('H'); // H
    } else {
      runtime_error_flag=CODE_INVALID_ARGUMENT;
    }
  }
}


// =================================================
// Вывести команду по коду
// =================================================
void command_decode_show(unsigned char command_code) {
  int i;
  enum {CODE_BEGIN,CODE_MISMATCH,CODE_SPACE,CODE_MATCH} code_flag;
  
  code_flag=CODE_BEGIN;
  for(i=0;i!=strlen_P(pgm_command_codes);i++) {
    if(code_flag==CODE_BEGIN) {
      // Если код совпадает - переходим в состояние CODE_SPACE
      // Иначе в CODE_MISMATCH
      if(pgm_read_byte_near(pgm_command_codes+i)==command_code)
        code_flag=CODE_SPACE;
      else
        code_flag=CODE_MISMATCH;
    } else if(code_flag==CODE_SPACE) {
      // Пропускаем пробел
      code_flag=CODE_MATCH;
    } else if(code_flag==CODE_MATCH) {
      if(pgm_read_byte_near(pgm_command_codes+i)!='\n') {
        Serial.print((char)pgm_read_byte_near(pgm_command_codes+i));
      } else {
        return;
      }
    } else if(pgm_read_byte_near(pgm_command_codes+i)=='\n') {
      code_flag=CODE_BEGIN;
    }
  }
  Serial.print("<CMD");
  Serial.print((unsigned int)command_code);
  Serial.print(">");
}

// =================================================
// Выполнить команду list [строка]
// =================================================
void command_list(unsigned char *argument) {
  int i;
  enum {PROGRAM_END,PROGRAM_STRING_NUMBER,PROGRAM_OPERATION,PROGRAM_ARGUMENT,PROGRAM_SKIP} state;
  int from=0;
  if(strlen(argument)>0) {
    from=calc_expression(argument);
  }
  
  // Программа начинается с номера первой её строки
  state=PROGRAM_STRING_NUMBER;
  for(i=0;i!=PROGRAM_LEN;i++) {
    
    // Если должен быть номер строки - выводим его
    if(state==PROGRAM_STRING_NUMBER) {
      if(program[i]!=0) {
        if(program[i]>=from) {
          Serial.print((int)program[i]);
          Serial.print(' ');
          state=PROGRAM_OPERATION;
        } else {
          state=PROGRAM_SKIP;
        }
      } else {
        // Конец программы, выход
        return;
      }
      
    // Если должна быть операция - выводим её
    } else if(state==PROGRAM_OPERATION) {
      command_decode_show(program[i]);
      Serial.print(' ');
      state=PROGRAM_ARGUMENT;
    } else if(state==PROGRAM_ARGUMENT) {
      
      // Выводим остальную часть строки программы
      if(program[i]!='\0') {
        Serial.print((char)program[i]);
      } else {
        Serial.println("");
        state=PROGRAM_STRING_NUMBER;
      }
    } if(state==PROGRAM_SKIP) {
      if(program[i]=='\0') {
        state=PROGRAM_STRING_NUMBER;
      }
    }
  }
}

// =================================================
// Выполнить команду print [(выражение|"текст")[,(выражение|"текст")[,...]]
// =================================================
void command_print(unsigned char *argument) {
  int i;
  enum {EXPRESSION_OTHER,EXPRESSION_BEGIN} state;
  state=EXPRESSION_BEGIN;
  for(i=0;i!=strlen(argument);i++) {
    if(argument[i]==',') {
      state=EXPRESSION_BEGIN;
    }
    else if(state==EXPRESSION_BEGIN) {
      if(argument[i]=='"') {
        i++;
        while(argument[i]!='"' && argument[i]!='\0') {
          Serial.print((char)argument[i]);
          i++;
        }
      } else {
        Serial.print(calc_expression(argument+i));
      }
      state=EXPRESSION_OTHER;
    }
  }
  Serial.println("");
}

// =================================================
// Выполнить команду input [(переменная|"текст")[,(переменная|"текст")[,...]]
// =================================================
void command_input(unsigned char *argument) {
  int i;
  unsigned char buff[INPUT_LEN];
  enum {EXPRESSION_OTHER,EXPRESSION_BEGIN} state;
  state=EXPRESSION_BEGIN;
  for(i=0;i!=strlen(argument);i++) {
    if(argument[i]==',') {
      state=EXPRESSION_BEGIN;
    }
    else if(state==EXPRESSION_BEGIN) {
      if(argument[i]=='"') {
        i++;
        while(argument[i]!='"' && argument[i]!='\0') {
          Serial.print((char)argument[i]);
          i++;
        }
      } else {
        //Serial.print(calc_expression(argument+i));
        if(argument[i]>='a' && argument[i]<='z') {
          read_str(buff,INPUT_LEN);
          sscanf(buff,"%d",variables+(*(argument+i)-'a'));
        } else {
          runtime_error_flag=CODE_INVALID_EXPRESSION;
          return;
        }
      }
      state=EXPRESSION_OTHER;
    }
  }
}

// =================================================
// Выполнить команду let переменная=выражение
// =================================================
void command_let(unsigned char *argument) {
  // По смещению 1 находится переменная
  // По смещению 2 знак равно
  // По смещению 3 выражение
  if(strlen(argument)==0 || argument[0]<'a' || argument[0]>'z' || argument[1]!='=') {
    runtime_error_flag=CODE_INVALID_ARGUMENT;
  } else {
    variables[argument[0]-'a']=calc_expression(argument+sizeof(char)*2);
  }
}

// =================================================
// Выполнить команду memory
// =================================================
void command_memory(unsigned char *argument) {
  // По смещению 1 находится переменная
  if(argument[sub_argument(argument,0)]!='\0') {
    runtime_error_flag=CODE_INVALID_ARGUMENT;
  } else {
    Serial.print(MESSAGE_FREE_MEMORY);
    Serial.println(get_free_space());
  }
}


// =================================================
// Выполнить команду random переменная
// =================================================
void command_random(unsigned char *argument) {
  int val;
  int var;
  // По смещению 1 находится переменная
  if(argument[sub_argument(argument,0)]=='\0') {
    runtime_error_flag=CODE_INVALID_ARGUMENT;
  } else {
    var=argument[sub_argument(argument,0)]-'a';
    if(argument[sub_argument(argument,1)]=='\0') {
      val=1;
    } else {
      val=calc_expression(argument+sub_argument(argument,1));
    }
    if(var>=0 && var<=25) {
      variables[var]=random(val);
    } else {
      runtime_error_flag=CODE_INVALID_ARGUMENT;
    }
  }
}

// =================================================
// Выполнить команду read переменная,выражение
// =================================================
void command_read(unsigned char *argument) {
  int pin;
  int var;
  // Первый аргумент - номер пина
  // Второй - переменная
  if(argument[sub_argument(argument,0)]=='\0' || argument[sub_argument(argument,1)]=='\0') {
    runtime_error_flag=CODE_INVALID_ARGUMENT;
  } else {
    pin=calc_expression(argument+sub_argument(argument,0));
    var=argument[sub_argument(argument,1)]-'a';
    if(pin>=2 && pin<=19 && var>=0 && var<=25) {
      pinMode(pin,INPUT_PULLUP);
      variables[var]=digitalRead(pin);
    } else {
      runtime_error_flag=CODE_INVALID_ARGUMENT;
    }
  }
}

// =================================================
// Выполнить команду aread переменная,выражение
// =================================================
void command_aread(unsigned char *argument) {
  int pin;
  int var;
  // Первый аргумент - номер пина
  // Второй - переменная
  if(argument[sub_argument(argument,0)]=='\0' || argument[sub_argument(argument,1)]=='\0') {
    runtime_error_flag=CODE_INVALID_ARGUMENT;
  } else {
    pin=calc_expression(argument+sub_argument(argument,0));
    var=argument[sub_argument(argument,1)]-'a';
    if(pin>=0 && pin<=7 && var>=0 && var<=25) {
      variables[var]=analogRead(pin);
    } else {
      runtime_error_flag=CODE_INVALID_ARGUMENT;
    }
  }
}

// =================================================
// Выполнить команду sread переменная,выражение
// =================================================
void command_sread(unsigned char *argument) {
  int pin;
  int var;
  // Первый аргумент - переменная
  if(argument[sub_argument(argument,0)]=='\0') {
    runtime_error_flag=CODE_INVALID_ARGUMENT;
  } else {
    var=argument[sub_argument(argument,0)]-'a';
    if(var>=0 && var<=25) {
      variables[var]=serial_code;
    } else {
      runtime_error_flag=CODE_INVALID_ARGUMENT;
    }
  }
}

// =================================================
// Выполнить команду write переменная,выражение
// =================================================
void command_write(unsigned char *argument) {
  int pin;
  int val;
  // Первый аргумент - номер пина
  // Второй - выражение
  if(argument[sub_argument(argument,0)]=='\0' || argument[sub_argument(argument,1)]=='\0') {
    runtime_error_flag=CODE_INVALID_ARGUMENT;
  } else {
    pin=calc_expression(argument+sub_argument(argument,0));
    val=calc_expression(argument+sub_argument(argument,1));
    if(pin>=2 && pin<=19) {
      pinMode(pin,OUTPUT);
      /*Serial.print("digitalWrite(");
      Serial.print(pin),
      Serial.print(",");
      Serial.print(variables[argument[0]-'a']),
      Serial.println(");");*/
      digitalWrite(pin,val);
    } else {
      runtime_error_flag=CODE_INVALID_ARGUMENT;
    }
  }
}

// =================================================
// Выполнить команду awrite переменная,выражение
// =================================================
void command_awrite(unsigned char *argument) {
  int pin;
  int val;
  // Первый аргумент - номер пина
  // Второй - выражение
  if(argument[sub_argument(argument,0)]=='\0' || argument[sub_argument(argument,1)]=='\0') {
    runtime_error_flag=CODE_INVALID_ARGUMENT;
  } else {
    pin=calc_expression(argument+sub_argument(argument,0));
    val=calc_expression(argument+sub_argument(argument,1));
    if(pin==3 || pin==5 || pin==6 || pin==9 || pin==10 || pin==11) {
      pinMode(pin,OUTPUT);
      analogWrite(pin,val);
    } else {
      runtime_error_flag=CODE_INVALID_ARGUMENT;
    }
  }
}

// =================================================
// Выполнить команду if выражение,строка[,строка]
// =================================================
void command_if(unsigned char *argument) {
  int val;
  // Первый аргумент - номер пина
  // Второй - выражение
  if(argument[sub_argument(argument,0)]=='\0' || argument[sub_argument(argument,1)]=='\0') {
    runtime_error_flag=CODE_INVALID_ARGUMENT;
  } else {
    val=calc_expression(argument+sub_argument(argument,0));
    if(val) {
      runtime_current_str=calc_expression(argument+sub_argument(argument,1));
    } else {
      if(argument[sub_argument(argument,2)]!='\0') {
        runtime_current_str=calc_expression(argument+sub_argument(argument,2));
      } else {
        runtime_current_str++;
      }
    }
  }
}

// =================================================
// Выполнить команду goto выражение
// =================================================
void command_goto(unsigned char *argument) {

  if(strlen(argument)==0) {
    runtime_error_flag=CODE_INVALID_ARGUMENT;
  } else {
    runtime_current_str=calc_expression(argument);
  }
}

// =================================================
// Выполнить команду delay выражение
// =================================================
void command_delay(unsigned char *argument) {
  if(strlen(argument)==0) {
    runtime_error_flag=CODE_INVALID_ARGUMENT;
  } else {
    delay(calc_expression(argument));
  }
}

// =================================================
// Выполнить программный шаг
// =================================================
int command_execute(unsigned char *encoded_command) {
  switch(encoded_command[0]) {
    case COMMAND_REM: break;
    case COMMAND_PRINT: command_print(encoded_command+1); break;
    case COMMAND_INPUT: command_input(encoded_command+1); break;
    case COMMAND_LET: command_let(encoded_command+1); break;
    case COMMAND_LIST: command_list(encoded_command+1); break;
    case COMMAND_SAVE: command_save(encoded_command+1); break;
    case COMMAND_LOAD: command_load(encoded_command+1); break;
    case COMMAND_CLEAR: command_clear(encoded_command+1); break;
    case COMMAND_DUMP: command_dump(encoded_command+1); break;
    case COMMAND_GOTO: command_goto(encoded_command+1); return 1; break;
    case COMMAND_DELAY: command_delay(encoded_command+1); break;
    case COMMAND_CLS: command_cls(encoded_command+1); break;
    case COMMAND_RANDOM: command_random(encoded_command+1); break;
    case COMMAND_READ: command_read(encoded_command+1); break;
    case COMMAND_AREAD: command_aread(encoded_command+1); break;
    case COMMAND_SREAD: command_sread(encoded_command+1); break;
    case COMMAND_WRITE: command_write(encoded_command+1); break;
    case COMMAND_AWRITE: command_awrite(encoded_command+1); break;
    case COMMAND_IF: command_if(encoded_command+1); return 1; break;
    case COMMAND_CURSOR: command_cursor(encoded_command+1); break;
    case COMMAND_MEMORY: command_memory(encoded_command+1); break;
    case 0:
    case COMMAND_END:
      return 0;
      break;
    default:
      runtime_error_flag=CODE_UNKNOWN_COMMAND;
      break;
  }
  // Возвращаем следующую строку
  if(runtime_current_str>0) {
    runtime_current_str++;
  }
  return 1;
}

// =================================================
// Выполнить программу
// =================================================
void command_run() {
  int offset;
  enum {COMMAND_LINE_NUMBER,COMMAND_OTHER} state;
  // Строка с которой будет выполняться код
  runtime_current_str=1;
  do {
    // Ищем в программе следующую строку для выполнения
    state=COMMAND_LINE_NUMBER;
    for(offset=0;offset!=PROGRAM_LEN;offset++) {
      if(state==COMMAND_LINE_NUMBER) {
        if(program[offset]>=runtime_current_str) {
          runtime_current_str=program[offset];
          break;
        }
        state=COMMAND_OTHER;
      } else if(program[offset]=='\0') {
        state=COMMAND_LINE_NUMBER;
      }
    }

    // Нажато Ctrl+C
    if(Serial.available()>0) {
      serial_code=Serial.read();
      if(serial_code==3) {
        runtime_error_flag=CODE_BREAK;
      }
    }

    // Не найдено строки с большим или равным номером
    if(offset==PROGRAM_LEN) break;
    // Произошла ошибка
    if(runtime_error_flag!=CODE_READY) break;
    
    /*Serial.print("Current str: ");
    Serial.println(runtime_current_str);
    Serial.print("Current offset: ");
    Serial.println(offset);*/
    // Выполянем команды
  } while(command_execute(program+offset+1)!=0);
}

// =================================================
// Преобразовать строку в программный вид
// =================================================
int command_encode(unsigned char *command) {
  int str_number;
  unsigned char cmd_code;
  unsigned char cmd_buff[OPERATOR_LEN];
  int command_index;
  // Если указан номер строки - дополняем программу строкой
  if(isdigit(command[0])) {
    command_index=0;
    if(sscanf(command,"%d %s",&str_number,cmd_buff)==2) {
      cmd_code=get_cmd_code(cmd_buff);
      // Если команда указана: а кода нет
      if(cmd_code==0) {
        runtime_error_flag=CODE_UNKNOWN_COMMAND;
        return;
      }
      while(*(command+command_index)!='\0' && *(command+command_index)!=' ') command_index++;
      command_index++;
      while(*(command+command_index)!='\0' && *(command+command_index)!=' ') command_index++;
    } else {
      while(*(command+command_index)!='\0') command_index++;
      cmd_code=0;
    }
    command_index++;
    command_append(str_number,cmd_code,command+command_index);
  } else {
    if(sscanf(command,"%s",&cmd_buff)==1) {
      str_number=0;
      cmd_code=get_cmd_code(cmd_buff);
        
      if(cmd_code==COMMAND_RUN) {
        command_run();
      } else if(cmd_code!=0) {
        block_move(command,COMMAND_LEN,strlen(cmd_buff)+1,1);
        command[0]=cmd_code;
        command_execute(command);
      } else {
        runtime_error_flag=CODE_UNKNOWN_COMMAND;
      }
    }
  }
  return 0;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println(SERIAL_GREETING);

  command_load("");
  runtime_error_flag=CODE_READY;
  command_run();
}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned char command[COMMAND_LEN];
  Serial.print(SERIAL_PROMPT);
  read_str(command,COMMAND_LEN);
  runtime_error_flag=CODE_READY;
  runtime_current_str=0;
  command_encode(command);
  message_error_code();
}

