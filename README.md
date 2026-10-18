# pascal-tasm-compiler

Учебный компилятор упрощённого Pascal → TASM для DOS (`.model small`), вариант 10. Реализация на **C++17**.

Поддерживаемые по ТЗ конструкции: `integer`, `boolean`, константы, процедуры, параметры по значению и по ссылке (`var`), `if/then/else`, сравнения и арифметика. Циклы, массивы, строки, функции и вложенные процедуры не входят в язык.

**Статус:** парсер и кодогенератор реализованы; их совместная работа проверяется автоматически на эталонном потоке токенов. Лексер пока является собираемой заглушкой. Проверка с помощью TASM/TLINK ещё не выполнена.

## Спецификация и структура

Полное ТЗ: [docs/SPEC С++.md](docs/SPEC%20%D0%A1%2B%2B.md).

```text
program.pas → [lexer] → tokens.lst → [parser] → tree.json + symbols.tab → [codegen] → program.asm
```

```text
lexer/main.cpp       лексический анализатор (§1 ТЗ)
parser/              синтаксический/семантический анализатор и его тесты (§2)
codegen/             исходники генератора TASM (§3), точка входа — main.cpp
json.hpp            nlohmann/json 3.11.3, общий заголовок
third_party/        сведения о зависимости и лицензия
tests/              исходник примера, эталонные JSON-файлы, проверка фикстур
CMakeLists.txt      необязательная сборка через CMake и CTest
```

Модули собираются независимо и взаимодействуют **только через файлы**. Все промежуточные файлы, включая `.lst` и `.tab`, содержат JSON. При переходе на C++ их формат не менялся, поэтому тестовые данные не требуют конвертации.

## Требования

- Компилятор C++17: актуальный GCC, Clang или MSVC.
- Только стандартная библиотека и **nlohmann/json**. Заголовок уже включён в репозиторий, скачивание при сборке не требуется. См. [third_party/README.md](third_party/README.md).
- Опционально: CMake ≥ 3.16 и подходящий генератор (Ninja, Makefiles или Visual Studio).
- TASM/TLINK и DOS-окружение нужны только для последующей сборки и запуска сгенерированного ASM.

Python не требуется.

## Сборка без CMake

Из корня репозитория, в Bash / Git Bash:

```bash
mkdir -p build
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -I. -o build/lexer.exe lexer/main.cpp
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -I. -o build/parser.exe parser/main.cpp
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -I. -o build/codegen.exe codegen/main.cpp codegen/code_generator.cpp codegen/code_generator_statements.cpp codegen/code_generator_expressions.cpp
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -I. -o build/fixture_tests.exe tests/fixture_tests.cpp

./build/fixture_tests.exe tests
```

Расширение `.exe` здесь задано явно для одинаковых команд в Windows и Linux. Каждый модуль можно собрать отдельно.

В Windows с MinGW/MSYS2 используй терминал выбранного toolchain. Если программа собрана, но не запускается (например, код `127` или ошибка DLL), проверь, что каталог `bin` именно этого компилятора стоит в `PATH` раньше каталогов других MinGW. Смешивание DLL из Git Bash и MSYS2 может мешать запуску.

## Сборка через CMake

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

CMake использует установленный компилятор. При необходимости выбери генератор явно, например `-G Ninja`; в Windows с MSVC выполняй команды из Developer PowerShell/Command Prompt.

При одноконфигурационном генераторе программы находятся в `build/`, при Visual Studio — обычно в `build/Release/`. В Linux CMake не добавляет `.exe`. Для сборки без тестов передай `-DBUILD_TESTING=OFF`.

## Интерфейс модулей

Ниже — команды для сборки без CMake. Полная цепочка от исходного Pascal заработает после реализации лексера; парсер и кодогенератор уже можно запускать вместе на `tests/tokens.lst`.

```bash
./build/lexer.exe tests/example.pas          # → tokens.lst
./build/parser.exe tokens.lst               # → tree.json, symbols.tab
./build/codegen.exe tree.json symbols.tab   # → program.asm
```

| Программа | Аргументы | Результат |
|---|---|---|
| `lexer` | `<source.pas>` | `tokens.lst` |
| `parser` | `<tokens.lst>` | `tree.json`, `symbols.tab` |
| `codegen` | `<tree.json> <symbols.tab>` | `program.asm` |

Выходные файлы записываются в **текущую рабочую директорию**, а не рядом с входным файлом. Успех реализованного модуля — код `0`, ошибка — `1` с сообщением в `stderr`. Генерируемые результаты в корне и каталог `build/` игнорируются Git.

## Тестирование

См. [tests/README.md](tests/README.md): состав эталонного примера, изолированная проверка модулей и ограничения автоматического теста.

CTest проверяет эталонные JSON, объявления и семантику парсера, а также реальную связку `tokens.lst → parser → codegen → program.asm`. Отдельный `parser/run_tests.sh` дополнительно проверяет CLI парсера, повреждённые входы и сохранность выходных файлов при ошибках. Лексер и работоспособность ASM в TASM этими тестами пока не проверяются.
