# Отчёт к практическому заданию №2

**Дисциплина:** Языки программирования
**Студент:** Богданов Александр Евгеньевич
**Группа:** Р4119
**Факультет:** ПИиКТ, Университет ИТМО
**Год:** 2025/2026

---

## 1. Цель

Реализовать построение графа потока управления (CFG) посредством анализа дерева разбора для набора входных файлов. Выполнить анализ собранной информации и сформировать набор файлов с графическим представлением для результатов анализа.

## 2. Задачи

1. Описать структуры данных для представления информации о подпрограммах, графах потока управления и деревьях операций.
2. Реализовать модуль, формирующий CFG на основе AST подпрограмм из входных файлов.
3. Реализовать тестовую программу, принимающую набор входных файлов и выводящую CFG каждой подпрограммы в отдельный DOT-файл, а также граф вызовов.
4. Протестировать на примерах, покрывающих все синтаксические конструкции: ветвления, циклы, break, repeat, вложенные вызовы.

## 3. Описание работы

### Состав модулей

| Модуль             | Файлы                              | Назначение                                                              |
| ------------------ | ---------------------------------- | ----------------------------------------------------------------------- |
| Лексер             | `lexer.h`, `lexer.cpp`             | Разбиение текста на токены (из задания 1)                               |
| Парсер             | `parser.h`, `parser.cpp`           | Построение AST (из задания 1)                                           |
| Структуры CFG      | `cfg.h`                            | Структуры данных: Operation, BasicBlock, CFG, FunctionInfo, ProgramInfo |
| Построитель CFG    | `cfg_builder.h`, `cfg_builder.cpp` | Обход AST - граф потока управления + граф вызовов                       |
| DOT-экспорт CFG    | `cfg_dot_export.h`, `.cpp`         | Сериализация CFG и графа вызовов в Graphviz DOT                         |
| Тестовая программа | `main.cpp`                         | CLI-обёртка: парсинг - построение CFG - вывод DOT                       |

**Размер реализации:** 1908 строк кода (новых модулей: cfg_builder 496, cfg_dot_export 213, main 215).

### Структуры данных

#### Дерево операций (Operation)

Каждая операция в составе базового блока представлена как совокупность вида операции и операндов:

```cpp
struct Operation {
    enum Kind {
        OP_ASSIGN,    // присваивание: lhs = rhs
        OP_BINARY,    // бинарная операция: lhs op rhs
        OP_UNARY,     // унарная операция: op operand
        OP_CALL,      // вызов функции: func(args...)
        OP_INDEX,     // индексация массива: arr[i]
        OP_SLICE,     // срез массива: arr[from..to]
        OP_PLACE,     // ссылка на переменную
        OP_LITERAL,   // константное значение
        OP_RETURN,    // неявный возврат
    };

    Kind kind;
    std::string value;                           
    SourceLocation loc;                          
    std::vector<std::unique_ptr<Operation>> operands; 
};
```

Деревья операций формируются из AST-выражений функцией `convertExpr()`. Отличие от AST: операции представляют семантику (присваивание, вызов, арифметика), а не синтаксис (скобки, ключевые слова).

#### Базовый блок (BasicBlock)

```cpp
struct BasicBlock {
    int id;                        
    std::string label;             
    std::vector<OperationPtr> operations;  

    int unconditionalNext;         
    int conditionalTrue;           
    int conditionalFalse;          
    OperationPtr condition;        
};
```

Каждый блок содержит линейную последовательность операций без ветвлений внутри. Ветвление возникает только на выходе из блока: либо безусловный переход (`unconditionalNext`), либо условный (`conditionalTrue`/`conditionalFalse` с выражением `condition`).

#### Граф потока управления (ControlFlowGraph)

```cpp
struct ControlFlowGraph {
    std::vector<std::unique_ptr<BasicBlock>> blocks;
    int entryBlockId;
};
```

#### Информация о функции и программе

```cpp
struct FunctionInfo {
    FunctionSignature signature;   
    std::string sourceFile;        
    SourceLocation loc;
    ControlFlowGraph cfg;
};

struct ProgramInfo {
    std::vector<std::unique_ptr<FunctionInfo>> functions;
    std::map<std::string, std::set<std::string>> callGraph; 
};
```

### Программный интерфейс модуля

```cpp
std::vector<SourceFileInput> inputs = { {fileName, tree.get()} };

CFGBuilder builder;
CFGBuildResult result = builder.build(inputs);
// result.program - ProgramInfo с CFG всех функций и графом вызовов
// result.errors - коллекция AnalysisError
```

### Использование тестовой программы

```bash
./build/cfg_builder test/example.v4

./build/cfg_builder -o output_dir test/example.v4 test/helpers.v4
```

Результат: по одному .dot на каждую функцию + callgraph.dot.

Ошибки выводятся тестовой программой в stderr. Формат: `файл:строка:столбец: analysis error: сообщение`.

## 4. Аспекты реализации

### Алгоритм построения CFG

Модуль `CFGBuilder` обходит AST каждой функции рекурсивно через `processStatement()`. Каждый вызов `processStatement()` принимает ID текущего блока и возвращает ID блока, в котором продолжается выполнение после данной конструкции (или -1, если управление не продолжается - break).

#### If-then-else

Схема:

```
entry (текущий блок) - условие
true  -> if.then -> if.merge
false -> if.else -> if.merge
```

Создаются блоки `if.then`, опционально `if.else`, и `if.merge`. Обе ветви сходятся в `if.merge`.

#### While/Until цикл

Схема:

```
current -> loop.cond
loop.cond:
    true  -> loop.body -> loop.cond
    false -> loop.exit
```

Для `until` переходы true/false меняются местами (выход при истинном условии). Тело цикла может содержать вложенные конструкции; `loopExitBlockId` передаётся рекурсивно для поддержки `break`.

#### Repeat (do-while/do-until)

Схема:

```
current -> repeat.body -> repeat.cond
repeat.cond:
    true  -> repeat.body
    false -> repeat.exit
```

Отличие от while: тело выполняется до проверки условия. Для `until` переходы снова меняются.

#### Break

```cpp
case ASTNode::STMT_BREAK:
    currentBlock->unconditionalNext = loopExitBlockId;
    return -1;
```

Break создаёт безусловную дугу из текущего блока в `loop.exit` ближайшего цикла. Возвращает -1, сигнализируя что код после break недостижим.

#### Блок (begin/end, { })

Обрабатывается как линейная последовательность вложенных инструкций - новые блоки создаются только при наличии ветвлений или циклов внутри.

### Формирование дерева операций

Функция `convertExpr()` преобразует AST-выражения в `Operation`:

| AST узел     | Operation           | Особенность                                                  |
| ------------ | ------------------- | ------------------------------------------------------------ |
| EXPR_BINARY  | OP_BINARY           | Оператор в `value`, два операнда                             |
| EXPR_UNARY   | OP_UNARY            | Оператор в `value`, один операнд                             |
| EXPR_CALL    | OP_CALL             | Первый операнд - вызываемое выражение, остальные - аргументы |
| EXPR_SLICE   | OP_INDEX / OP_SLICE | INDEX если индекс, SLICE если диапазон                       |
| EXPR_PLACE   | OP_PLACE            | Имя переменной                                               |
| EXPR_LITERAL | OP_LITERAL          | Значение литерала                                            |
| STMT_ASSIGN  | OP_ASSIGN           | Два операнда: lhs и rhs                                      |

### Граф вызовов

После построения CFG для каждой функции выполняется обход `collectCalls()` по деревьям операций всех блоков и собираются имена вызываемых функций. Результат - `map<string, set<string>>`: для каждого вызывающего - множество вызываемых. Функции, не объявленные в проанализированных файлах, помечаются в DOT как внешние (style=dashed).

## 5. Результаты тестирования

### Пример 1: If-then (без else) - функция `abs`

Вход:

```
def abs(x of int) of int
    if x < 0 then
        x = -x;
    result = x;
end
```

CFG:

```
BB0 entry: условие (x < 0)
    true  -> BB2 if.then
    false -> BB3 if.merge

BB2 if.then:
    x = -x
    -> BB3

BB3 if.merge:
    result = x
    -> BB1 exit
```

4 блока.

### Пример 2: If-then-else - функция `max`

Вход:

```
def max(a of int, b of int) of int
    if a > b then
        result = a;
    else
        result = b;
end
```

CFG:

```
BB0 entry: условие (a > b)
    true  -> BB2 if.then
    false -> BB4 if.else

BB2:
    result = a
    -> BB3

BB4:
    result = b
    -> BB3

BB3 if.merge:
    -> BB1 exit
```

5 блоков.

### Пример 3: While-цикл - функция `sum`

Вход:

```
def sum(arr of int array[1], n of int) of int
    s = 0;
    i = 0;
    while i < n
        s = s + arr[i];
        i = i + 1;
    end
    s;
end
```

CFG:

```
BB0 entry:
    s = 0
    i = 0
    -> BB2 loop.cond

BB2 loop.cond: условие (i < n)
    true  -> BB3 loop.body
    false -> BB4 loop.exit

BB3 loop.body:
    s = s + arr[i]
    i = i + 1
    -> BB2

BB4 loop.exit:
    s
    -> BB1 exit
```

5 блоков. Есть обратная дуга BB3 -> BB2.

### Пример 4: Break внутри цикла - функция `search`

Вход:

```
def search(arr of int array[1], n of int, target of int) of int
    i = 0;
    while i < n
        if arr[i] == target then
            break;
        i = i + 1;
    end
    i;
end
```

CFG:

```
BB0 entry:
    i = 0
    -> BB2 loop.cond

BB2 loop.cond: условие (i < n)
    true  -> BB3 loop.body
    false -> BB4 loop.exit

BB3 loop.body: условие (arr[i] == target)
    true  -> BB5 break
    false -> BB6

BB5:
    -> BB4 loop.exit

BB6:
    i = i + 1
    -> BB2

BB4 loop.exit:
    i
    -> BB1 exit
```

7 блоков. Break ведёт напрямую в loop.exit.

### Пример 5: Repeat-while (do-while) - функция `factorial`

Вход:

```
def factorial(n of int) of int
    result = 1;
    i = 2;
    result = result * i while i <= n;
end
```

CFG:

```
BB0 entry:
    result = 1
    i = 2
    -> BB2 repeat.body

BB2 repeat.body:
    result = result * i
    -> BB3 repeat.cond

BB3 repeat.cond: условие (i <= n)
    true  -> BB2
    false -> BB4 repeat.exit

BB4 repeat.exit:
    -> BB1 exit
```

5 блоков.

### Пример 6: Repeat-until - функция `waitUntilReady`

Вход:

```
def waitUntilReady(sensor of int) of int
    x = read(sensor);
    x = read(sensor) until x > 0;
    x;
end
```

CFG:

```
BB0 entry:
    x = read(sensor)
    -> BB2 repeat.body

BB2 repeat.body:
    x = read(sensor)
    -> BB3 repeat.cond

BB3 repeat.cond: условие (x > 0)
    true  -> BB4 repeat.exit
    false -> BB2

BB4 repeat.exit:
    -> BB1 exit
```

5 блоков.

### Пример 7: Несколько файлов + граф вызовов

Команда:

```bash
./build/cfg_builder -o test test/example.v4 test/helpers.v4
```

Результат: 10 DOT-файлов для CFG и `callgraph.dot`.

Граф вызовов:

```
main -> {init, sum, max, abs, search, factorial, print}
clamp -> {max, min}
init, print - внешние
```

### Пример 8: Составная программа - функция `main`

CFG:

```
BB0 entry:
    data = init(10)
    i = 0
    -> BB2 loop.cond

BB2 loop.cond: условие (i < 10)
    true  -> BB3 loop.body
    false -> BB4 loop.exit

BB3 loop.body:
    data[i] = i * 2
    i = i + 1
    -> BB2

BB4 loop.exit:
    total = sum(data, 10)
    m = max(abs(-5), abs(3))
    idx = search(data, 10, 6)
    f = factorial(5)
    x = total + m
    y = idx + f
    print("total=", total)
    print("max=", m)
    print("idx=", idx)
    print("fact=", f)
    -> BB1 exit
```

Блок begin/end не создаёт новых узлов CFG, если внутри нет ветвлений.

### Сводка

| Конструкция                  | Покрыта | Пример |
| ---------------------------- | ------- | ------ |
| if-then (без else)           | да      | 1      |
| if-then-else                 | да      | 2      |
| while ... end                | да      | 3      |
| break внутри цикла           | да      | 4      |
| repeat ... while             | да      | 5      |
| repeat ... until             | да      | 6      |
| Несколько входных файлов     | да      | 7      |
| Граф вызовов                 | да      | 7      |
| Блоки begin/end              | да      | 8      |
| Вызовы функций (вложенные)   | да      | 8      |
| Деревья операций в узлах CFG | да      | все    |

## 6. Результаты

1. Описаны структуры данных: `Operation`, `BasicBlock`, `ControlFlowGraph`, `FunctionInfo`, `ProgramInfo` с графом вызовов.
2. Реализован модуль `CFGBuilder` (496 строк), строящий CFG обходом AST с корректной обработкой всех конструкций варианта 4: ветвления, три вида циклов, break, блоки.
3. Реализован DOT-экспорт (213 строк) для визуализации CFG: entry - зелёный, exit - жёлтый, условные дуги - зелёная/красная.
4. Тестовая программа принимает несколько входных файлов, выводит DOT-файл на каждую функцию и граф вызовов.
5. Протестировано на 10 функциях из 2 файлов, покрывающих все синтаксические конструкции.

## 7. Выводы

Цель задания достигнута: для всех подпрограмм из набора входных файлов построены корректные графы потока управления и граф вызовов.

Ключевое архитектурное решение - рекурсивная функция `processStatement()`, возвращающая ID блока продолжения. Это позволило единообразно обрабатывать вложенные конструкции: `if` внутри `while`, `break` внутри `if` внутри `while` и т.д. Параметр `loopExitBlockId`, передаваемый рекурсивно, обеспечивает корректную обработку `break` - дуга ведёт в exit-блок ближайшего охватывающего цикла.

Преобразование AST-выражений в деревья операций (`convertExpr`) отделяет синтаксическое представление от семантического: скобки исчезают, остаются только операции с операндами. Это упрощает последующую кодогенерацию (задание 3).

Блоки `begin/end` и `{ }` не порождают новых узлов CFG, если внутри нет ветвлений - их содержимое вливается в текущий базовый блок, что соответствует определению базового блока как максимальной последовательности инструкций без ветвлений.

Результат работы модуля - структура `ProgramInfo` - используется без изменений в задании 3 для кодогенерации.
