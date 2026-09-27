# pascal-tasm-compiler

Pascal to TASM compiler

Процедуры, if/then/else, операторы сравнения, передача параметров по указателю (var), константы.

## Архитектура
 
```
program.pas → [1. Лексер] → tokens.lst → [2. Синт. анализатор] → tree.json + symbols.tab → [3. Кодогенератор] → program.asm
```
 
Полная спецификация форматов и грамматики — в [`docs/SPEC.md`](docs/SPEC.md).
 
## Как запустить
 
```bash
python lexer/main.py tests/example.pas          # → tokens.lst
python parser/main.py tokens.lst                # → tree.json, symbols.tab
python codegen/main.py tree.json symbols.tab    # → program.asm
```
 
Требуется Python 3.
