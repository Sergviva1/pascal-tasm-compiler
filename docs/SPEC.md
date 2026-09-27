# ТЗ: компилятор Pascal → TASM (Вариант 10)


---

## 0. Общее описание

Компилятор переводит программу на упрощённом Pascal в код на ассемблере TASM (для DOS, модель `.model small`).

```
program.pas → [1. Лексер] → tokens.lst → [2. Синт. анализатор] → tree.json + symbols.tab → [3. Кодогенератор] → program.asm
```

Три модуля пишутся тремя разными людьми **независимо**, но должны строго соблюдать форматы файлов, описанные ниже — это единственное, что их связывает.

### Типы данных
Только два: `integer` (целое число) и `boolean` (`true`/`false`). Больше никаких типов нет и не нужно.

### Обязательные конструкции языка (вариант 10)
- Объявление и вызов **процедур** (без функций)
- `if ... then ... [else ...]`
- Операторы сравнения: `=`, `<>`, `<`, `>`, `<=`, `>=`
- **Передача параметра по указателю** (`var`-параметр — процедура может менять значение переменной вызывающего кода)
- **Константы** (`const`)

### Чего в языке НЕТ (не реализовывать, не усложнять)
Циклов, массивов, строк, записей, функций (возвращающих значение), `and`/`or`/`not`, вложенных процедур.

### Язык реализации компилятора

Все три модуля пишутся на **Python 3** (требований к языку реализации в задании нет — выбран Python как самый простой для этой задачи вариант).

Почему Python, а не C/C++:
- Встроенная поддержка JSON (`json.load` / `json.dump`) — не нужны сторонние библиотеки для форматов `tokens.lst`, `tree.json`, `symbols.tab`
- Словари и списки из коробки — не нужно вручную объявлять структуры и управлять памятью
- Меньше низкоуровневых багов, не относящихся к самой логике компилятора (парсинг, генерация кода)

Каждый модуль — это отдельный `.py`-скрипт, который:
- принимает путь к входному файлу через аргумент командной строки (`sys.argv`) или просто как константу в начале файла на время разработки
- пишет результат в выходной файл в своей директории

Пример вызова всей цепочки из терминала:
```bash
python lexer.py example.pas          # → tokens.lst
python parser.py tokens.lst          # → tree.json, symbols.tab
python codegen.py tree.json symbols.tab   # → program.asm
```

Финальная сборка `.asm` в исполняемый файл делается уже самим TASM/TLINK (это отдельный шаг, не Python).

---

## 1. Часть 1: Лексический анализатор

### Что это и зачем
Лексер читает исходный текст программы как обычную строку символов и разбивает её на "слова" языка — токены. Например, строка `x := 5 + y;` превращается в токены: `x`, `:=`, `5`, `+`, `y`, `;`. Он же убирает пробелы и комментарии, которые дальше никому не нужны.

### Вход
Файл `program.pas` — исходный код.

### Выход
Файл `tokens.lst` — список токенов в формате **JSON**:

```json
[
  {"type": "KEYWORD", "value": "program", "line": 1},
  {"type": "IDENT", "value": "Example", "line": 1},
  {"type": "DELIM", "value": ";", "line": 1},
  {"type": "INT_LITERAL", "value": "10", "line": 3},
  {"type": "OP", "value": ":=", "line": 8},
  {"type": "EOF", "value": "", "line": 12}
]
```

### Типы токенов (`type`)

| type | Что сюда попадает |
|---|---|
| `KEYWORD` | `program`, `var`, `const`, `procedure`, `begin`, `end`, `if`, `then`, `else`, `div`, `mod`, `integer`, `boolean` |
| `IDENT` | Имена переменных, констант, процедур |
| `INT_LITERAL` | Числа: `0`, `42`, `100` |
| `BOOL_LITERAL` | `true`, `false` |
| `OP` | `:=`, `+`, `-`, `*`, `=`, `<>`, `<`, `>`, `<=`, `>=` |
| `DELIM` | `;`, `,`, `:`, `(`, `)`, `.` |
| `EOF` | Конец файла (последний токен, всегда один) |

Комментарии (если поддерживаются — `{ ... }` или `(* ... *)`) в токены не попадают, просто выбрасываются.

---

## 2. Часть 2: Синтаксический анализатор

### Что это и зачем
Синт. анализатор берёт плоский список токенов и понимает, **как они складываются в структуру программы** — где начинается процедура, где условие, где присваивание. Результат — дерево (AST, Abstract Syntax Tree), где вложенность объектов отражает вложенность конструкций программы. Также он строит таблицу всех переменных/констант/процедур с их типами — она нужна кодогенератору, чтобы знать, сколько байт выделять и как обращаться к каждому имени.

### Вход
Файл `tokens.lst` (см. формат выше).

### Выход
1. `tree.json` — дерево разбора
2. `symbols.tab` — таблица символов

### 2.1 Грамматика

```
program     → 'program' ID ';' block '.'

block       → const_part? var_part? proc_part* 'begin' stmt_list 'end'

const_part  → 'const' (ID '=' const_value ';')+
const_value → INT_LITERAL | BOOL_LITERAL

var_part    → 'var' (id_list ':' type ';')+
id_list     → ID (',' ID)*
type        → 'integer' | 'boolean'

proc_part   → 'procedure' ID '(' param_list? ')' ';' block ';'

param_list  → param (';' param)*
param       → 'var'? id_list ':' type

stmt_list   → statement (';' statement)*

statement   → assign_stmt | if_stmt | proc_call_stmt | compound_stmt

compound_stmt → 'begin' stmt_list 'end'

assign_stmt → ID ':=' expr

if_stmt     → 'if' condition 'then' statement ('else' statement)?

proc_call_stmt → ID ('(' arg_list? ')')?
arg_list       → expr (',' expr)*

condition   → expr rel_op expr
rel_op      → '=' | '<>' | '<' | '>' | '<=' | '>='

expr        → term (('+' | '-') term)*
term        → factor (('*' | 'div' | 'mod') factor)*
factor      → ID | INT_LITERAL | BOOL_LITERAL | '(' expr ')' | '-' factor
```

### 2.2 Формат дерева (`tree.json`)

Каждый узел — JSON-объект с полем `"node"` (тип узла) и своими полями. Полный список узлов:

```
{"node": "Program", "name": string, "block": Block}

{"node": "Block",
 "consts": [ConstDecl], "vars": [VarDecl], "procs": [ProcDecl],
 "body": [Statement]}

{"node": "ConstDecl", "name": string, "type": "integer"|"boolean", "value": число или true/false}

{"node": "VarDecl", "names": [string], "type": "integer"|"boolean"}

{"node": "ProcDecl", "name": string, "params": [Param], "block": Block}

{"node": "Param", "names": [string], "type": "integer"|"boolean", "isVar": true|false}

// --- Statements ---
{"node": "Assign", "target": string, "expr": <expr>}
{"node": "If", "condition": <Compare>, "thenBranch": <Statement>, "elseBranch": <Statement>|null}
{"node": "ProcCall", "name": string, "args": [<expr>]}
{"node": "Compound", "statements": [<Statement>]}

// --- Expressions ---
{"node": "Compare", "op": "="|"<>"|"<"|">"|"<="|">=", "left": <expr>, "right": <expr>}
{"node": "BinOp", "op": "+"|"-"|"*"|"div"|"mod", "left": <expr>, "right": <expr>}
{"node": "UnaryMinus", "operand": <expr>}
{"node": "VarRef", "name": string}
{"node": "ConstRef", "name": string}
{"node": "IntLiteral", "value": число}
{"node": "BoolLiteral", "value": true|false}
```

**Пример:** `if x < 10 then y := true` превращается в:

```json
{
  "node": "If",
  "condition": {
    "node": "Compare", "op": "<",
    "left": {"node": "VarRef", "name": "x"},
    "right": {"node": "IntLiteral", "value": 10}
  },
  "thenBranch": {
    "node": "Assign", "target": "y",
    "expr": {"node": "BoolLiteral", "value": true}
  },
  "elseBranch": null
}
```

### 2.3 Формат таблицы символов (`symbols.tab`)

Тоже JSON — список записей:

```json
[
  {
    "name": "x",
    "kind": "param",
    "type": "integer",
    "isVar": false,
    "scope": "Compare",
    "value": null
  },
  {
    "name": "result",
    "kind": "param",
    "type": "boolean",
    "isVar": true,
    "scope": "Compare",
    "value": null
  },
  {
    "name": "Limit",
    "kind": "const",
    "type": "integer",
    "isVar": false,
    "scope": "global",
    "value": 10
  },
  {
    "name": "Compare",
    "kind": "proc",
    "type": null,
    "isVar": false,
    "scope": "global",
    "value": null,
    "paramsSignature": [
      {"type": "integer", "isVar": false},
      {"type": "boolean", "isVar": true}
    ]
  }
]
```

Поля:
- `kind` — `"var"` (обычная переменная), `"param"` (параметр процедуры), `"const"`, `"proc"`
- `isVar` — **важнее всего**: `true`, если это var-параметр (передача по указателю). У всего, кроме параметров, всегда `false`
- `scope` — `"global"` или имя процедуры, если переменная/параметр локальны для неё
- `paramsSignature` — только у процедур, нужно, чтобы проверять правильность вызова (сколько аргументов, каких типов)

### 2.4 Семантические проверки (делает синт. анализатор)

- Переменная/процедура используется, но не объявлена → ошибка
- Повторное объявление одного имени в одной области видимости → ошибка
- Присваивание значения переменной другого типа (`integer := boolean` и наоборот) → ошибка
- Сравнение (`condition`) разных типов → ошибка
- Присваивание значения константе → ошибка
- Вызов процедуры с неверным числом или типами аргументов → ошибка

### 2.5 Как писать парсер (метод рекурсивного спуска)

Одна функция на каждое правило грамматики. Функция смотрит на текущий токен, понимает, что перед ней, и либо обрабатывает его сама, либо вызывает другую функцию для вложенной конструкции.

```python
def parse_if_stmt(tokens):
    expect(tokens, 'if')
    condition = parse_condition(tokens)
    expect(tokens, 'then')
    then_branch = parse_statement(tokens)
    else_branch = None
    if peek(tokens) == 'else':
        advance(tokens)
        else_branch = parse_statement(tokens)
    return {"node": "If", "condition": condition,
            "thenBranch": then_branch, "elseBranch": else_branch}
```

Таблица символов заполняется по ходу разбора `const_part`, `var_part`, `param_list` — сразу с проверкой на дублирование имени в текущей области видимости.

---

## 3. Часть 3: Кодогенератор

### Что это и зачем
Кодогенератор обходит дерево (`tree.json`) и для каждого узла печатает соответствующие строки на ассемблере, подглядывая в таблицу символов, чтобы знать типы и то, var-параметр это или нет.

### Вход
`tree.json` + `symbols.tab`.

### Выход
`program.asm`.

### 3.1 Шаблон файла

```asm
.model small
.stack 100h

.data
    ; сюда — глобальные переменные и константы

.code
main proc
    mov ax, @data
    mov ds, ax

    ; сюда — тело программы (body из корневого Block)

    mov ax, 4c00h
    int 21h
main endp

; сюда — все процедуры (proc ... endp)

end main
```

### 3.2 Правила трансляции

- `integer`, `boolean` → `dw` (2 байта; `false` = 0, `true` = 1)
- `const` → директива `equ`: `Limit equ 10`
- Обычный параметр → в стек кладётся **значение** (`push [var]`), внутри процедуры читается как `[bp+N]`
- **Var-параметр** (`isVar: true`) → в стек кладётся **адрес** (`lea ax, var` / `push ax`); внутри процедуры: `mov bx, [bp+N]` (прочитать адрес), затем `[bx]` — косвенное чтение/запись значения
- `if`/сравнение → `cmp` + условный переход на **противоположное** условие (чтобы перепрыгнуть через then-ветку):

| Pascal | Прямой переход | Обратный (jump over then) |
|---|---|---|
| `=`  | `je`  | `jne` |
| `<>` | `jne` | `je`  |
| `<`  | `jl`  | `jge` |
| `>`  | `jg`  | `jle` |
| `<=` | `jle` | `jg`  |
| `>=` | `jge` | `jl`  |

- Каждый `if` — уникальные метки (`else_1`, `endif_1`, `else_2`, ...), счётчик сквозной по всей программе
- Процедуры: пролог `push bp` / `mov bp, sp`, эпилог `pop bp` / `ret N` (N — суммарный размер параметров в байтах, для очистки стека)

---

## 4. Сквозной пример (проверка стыковки всех трёх частей)

### Исходник (`example.pas`)

```pascal
program Example;
const
  Limit = 10;
var
  a, b: integer;
  flag: boolean;

procedure Compare(x: integer; var result: boolean);
begin
  if x < Limit then
    result := true
  else
    result := false
end;

begin
  a := 5;
  b := 0;
  Compare(a, flag)
end.
```

Каждый участник может прогнать этот файл через свой модуль на любом этапе разработки и свериться с форматами выше — если промежуточный результат совпадает по структуре, стыковка работает.

---

## 5. Порядок работы

1. Каждый пишет свой модуль **независимо**, ориентируясь только на разделы "Вход"/"Выход" своей части
2. Тестировать на примере из раздела 4
3. Финальная интеграция: прогнать `example.pas` через все три модуля подряд и убедиться, что на выходе получается рабочий `.asm`
