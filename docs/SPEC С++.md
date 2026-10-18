# ТЗ: компилятор Pascal → TASM (Вариант 10), реализация на C++

> Изменения относительно прошлой версии: язык реализации Python → **C++17**, добавлена библиотека **nlohmann/json**, обновлены команды запуска и примеры кода, добавлен раздел 6 (общие правила для C++). **Форматы файлов `tokens.lst`, `tree.json`, `symbols.tab` не менялись.**

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

Все три модуля пишутся на **C++17**.

- Каждый модуль — отдельная программа (`lexer/main.cpp`, `parser/main.cpp`, `codegen/main.cpp`), собирается отдельно и запускается из командной строки.
- Для чтения и записи JSON используется библиотека **nlohmann/json** (один заголовочный файл `json.hpp`, лежит в общей папке проекта, устанавливать ничего не нужно). Источник: https://github.com/nlohmann/json (раздел Releases → `json.hpp`).
- Больше никаких сторонних библиотек не используется, только стандартная библиотека C++.
- Каждая программа принимает путь к входному файлу через `argv` и пишет результат в выходной файл (имена выходных файлов фиксированы, см. ниже) в текущей директории.

Сборка:
```bash
mkdir -p build
g++ -std=c++17 -O2 -Wall -I. -o build/lexer.exe lexer/main.cpp
g++ -std=c++17 -O2 -Wall -I. -o build/parser.exe parser/main.cpp
g++ -std=c++17 -O2 -Wall -I. -o build/codegen.exe codegen/main.cpp codegen/code_generator.cpp codegen/code_generator_statements.cpp codegen/code_generator_expressions.cpp
```

Пример вызова всей цепочки из терминала:
```bash
./build/lexer.exe tests/example.pas         # → tokens.lst
./build/parser.exe tokens.lst               # → tree.json, symbols.tab
./build/codegen.exe tree.json symbols.tab   # → program.asm
```

Финальная сборка `.asm` в исполняемый файл делается уже самим TASM/TLINK (это отдельный шаг).

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

Важно: поле `value` — **всегда строка** (в том числе у `INT_LITERAL`), поле `line` — число.

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

**Регистр.** Pascal нечувствителен к регистру: ключевые слова и `true`/`false` распознаются в любом регистре, а в `value` записываются **в нижнем регистре**. Идентификаторы (`IDENT`) записываются как в исходнике.

**Ошибки.** Неизвестный символ → сообщение в `stderr` в формате `lexer: line N: unknown character 'c'` и код возврата `1`.

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

**Типы значений в JSON (важно для C++):**
- `IntLiteral.value` и `ConstDecl.value` (для integer) — **JSON-число** (`10`), не строка. Парсер конвертирует `"10"` из `tokens.lst` через `std::stoi`.
- `BoolLiteral.value` и `ConstDecl.value` (для boolean) — **JSON-bool** (`true`/`false`), не строка.
- `elseBranch` без ветки — JSON `null` (в nlohmann/json это `nullptr`).
- Пустые списки (`consts`, `vars`, `procs`, `args`, `params`) — это `[]`, а не отсутствие поля. Поля всегда присутствуют.

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
- `value` — число/bool для констант, `null` для остальных
- `paramsSignature` — только у процедур, нужно, чтобы проверять правильность вызова (сколько аргументов, каких типов)

### 2.4 Семантические проверки (делает синт. анализатор)

- Переменная/процедура используется, но не объявлена → ошибка
- Повторное объявление одного имени в одной области видимости → ошибка
- Присваивание значения переменной другого типа (`integer := boolean` и наоборот) → ошибка
- Сравнение (`condition`) разных типов → ошибка
- Присваивание значения константе → ошибка
- Вызов процедуры с неверным числом или типами аргументов → ошибка

Ошибки: сообщение в `stderr` в формате `parser: line N: <описание>` и код возврата `1` (выходные файлы при ошибке не создаются).

### 2.5 Как писать парсер (метод рекурсивного спуска)

Одна функция на каждое правило грамматики. Функция смотрит на текущий токен, понимает, что перед ней, и либо обрабатывает его сама, либо вызывает другую функцию для вложенной конструкции.

Состояние парсера удобно хранить в глобальных переменных (или в классе `Parser`): вектор токенов и индекс текущего токена.

```cpp
#include "json.hpp"
using json = nlohmann::json;

std::vector<json> tokens;   // загружен из tokens.lst
size_t pos = 0;

const json& peek()    { return tokens[pos]; }
const json& advance() { return tokens[pos++]; }

bool isTok(const std::string& value) {
    return peek()["value"] == value && peek()["type"] != "IDENT";
}

void expect(const std::string& value) {
    if (!isTok(value))
        throw std::runtime_error("line " + std::to_string(peek()["line"].get<int>())
                                 + ": expected '" + value + "'");
    advance();
}

json parseIfStmt() {
    expect("if");
    json condition = parseCondition();
    expect("then");
    json thenBranch = parseStatement();
    json elseBranch = nullptr;
    if (isTok("else")) {
        advance();
        elseBranch = parseStatement();
    }
    return {
        {"node", "If"},
        {"condition", condition},
        {"thenBranch", thenBranch},
        {"elseBranch", elseBranch}
    };
}
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
- `const` → директива `equ`: `Limit equ 10` (boolean-константы: `true` → `1`, `false` → `0`)
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

### 3.3 Подсказки для C++

- Вывод ассемблера — через `std::ostringstream` или `std::ofstream`, по одной функции `genStatement(const json&)`, `genExpr(const json&)` на группу узлов; диспетчеризация по `node["node"].get<std::string>()`.
- Счётчик меток — обычная глобальная переменная `int labelCounter = 0;`.
- Таблицу символов удобно загрузить в `std::map<std::pair<std::string,std::string>, json>` (ключ: имя + scope) для быстрого поиска «это var-параметр?».

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

---

## 6. Общие правила для C++ (новый раздел)

### 6.1 Структура папки проекта
```
project/
├── json.hpp          # nlohmann/json, один файл, общий для всех
├── CMakeLists.txt    # необязательная сборка через CMake
├── lexer/main.cpp
├── parser/main.cpp
├── codegen/
│   ├── main.cpp                       # аргументы командной строки и файлы
│   ├── code_generator.hpp             # интерфейс генератора
│   ├── code_generator.cpp             # программа, символы и процедуры
│   ├── code_generator_statements.cpp  # операторы и вызовы
│   └── code_generator_expressions.cpp # выражения и доступ к значениям
├── tests/           # example.pas, tokens.lst, tree.json, symbols.tab, fixture_tests.cpp
├── third_party/     # сведения о зависимости и лицензия
└── docs/SPEC С++.md
```
`json.hpp` подключается как `#include "json.hpp"` (в кавычках, не в угловых скобках). При ручной сборке из корня проекта нужен флаг `-I.`, так как исходники находятся в подпапках. В репозитории включена версия nlohmann/json 3.11.3 с MIT-лицензией; сеть при сборке не нужна. Команды выше предназначены для Bash / Git Bash; суффикс `.exe` задан явно и допустим также в Linux. Дополнительные команды CMake и проверки фикстур — в `README.md` и `tests/README.md`.

### 6.2 Стандарт и флаги
C++17, компилятор `g++` (или `clang++`), флаги `-std=c++17 -Wall`. Код не должен зависеть от платформы (Windows/Linux).

### 6.3 Аргументы командной строки
| Программа | Аргументы | Выходные файлы |
|---|---|---|
| `lexer` | `<файл.pas>` | `tokens.lst` |
| `parser` | `<tokens.lst>` | `tree.json`, `symbols.tab` |
| `codegen` | `<tree.json> <symbols.tab>` | `program.asm` |

Если аргументов не хватает — вывести usage в `stderr` и вернуть `1`.

### 6.4 Обработка ошибок
- Внутри модуля ошибки — через `throw std::runtime_error("line N: ...")`.
- В `main` один `try/catch`, который печатает сообщение в `stderr` с префиксом модуля (`lexer:`, `parser:`, `codegen:`) и возвращает `1`.
- При успехе `main` возвращает `0`.

### 6.5 Запись JSON
Записывать с отступом: `file << j.dump(2);` — так файлы удобно читать глазами при отладке.

### 6.6 Типичные ошибки при работе с nlohmann/json
- Доступ к полю как строке: `node["name"].get<std::string>()`, как к числу: `node["value"].get<int>()`, как к bool: `node["value"].get<bool>()`.
- Проверка типа узла: `node["node"] == "If"`.
- Если обратиться через `[]` к несуществующему полю константного `json`, будет падение. Для необязательных полей использовать `node.contains("paramsSignature")` или `node.value("field", default)`.
- Присваивание `nullptr` создаёт JSON `null`; проверка: `node["elseBranch"].is_null()`.
