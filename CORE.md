# Virex CORE Language Specification

---

## 1. Language Identity

**Name:** Virex

**Tagline:** Explicit control. Predictable speed. Minimal magic.

Virex is a compiled, statically typed, manual-memory systems language designed for clarity and performance, inspired by C/Zig/Rust but with an original, readable syntax.

---

## 2. Core Syntax

### 2.1 Variables and Constants

* `var` defines a mutable variable
* `const` defines an immutable constant
* Must always be initialized
* Syntax: `var <TYPE> <NAME> = <VALUE>;` or `const <TYPE> <NAME> = <VALUE>;`

**Examples:**

```virex
var i64 count = 3;
const i32 limit = 2;
```

### 2.2 Primitive Types

| Type | Size            |
| ---- | --------------- |
| i8   | 8-bit signed    |
| i16  | 16-bit signed   |
| i32  | 32-bit signed   |
| i64  | 64-bit signed   |
| u8   | 8-bit unsigned  |
| u16  | 16-bit unsigned |
| u32  | 32-bit unsigned |
| u64  | 64-bit unsigned |
| f32  | 32-bit float    |
| f64  | 64-bit float    |
| bool | 1 byte          |
| void | no value        |

### 2.3 Functions

* Keyword: `func`
* Parameters: `<TYPE> <NAME>`
* Return type: `-> <TYPE>`
* `return` is required

**Examples:**

```virex
func add(i32 a, i32 b) -> i32 {
    return a + b;
}

func main() -> i32 {
    return 0;
}
```

### 2.4 Control Flow

**If Statement:**

```virex
if (a > b) {
    return a;
} else {
    return b;
}
```

**While Loop:**

```virex
while (i < 10) {
    i = i + 1;
}
```

**For Loop:**

```virex
for (i32 i = 0; i < 10; i = i + 1) {
    print(i);
}
```

**Loop Control:**

* `break`: Exits the current loop immediately.
* `continue`: Skips to the next iteration.

**Example:**

```virex
while (true) {
    if (i > 10) break;
    i = i + 1;
    if (i % 2 == 0) continue;
    print(i);
}
```

### 2.5 Structs

* Keyword: `struct`
* Standard C-like declaration

**Example:**

```virex
struct Vec2 {
    f32 x;
    f32 y;
};

var Vec2 v;
v.x = 1.0;
v.y = 2.0;
```

### 2.6 Pointers & Memory

**Pointer Syntax:**

```virex
var i32* p;    // nullable pointer
var i32*! q;   // non-null pointer
```

* `*T` → nullable pointer
* `*!T` → guaranteed non-null

**Address-of:** `&`
**Dereference:** `*`

**Example:**

```virex
p = &x;
*p = 42;
```

**Unsafe Blocks:**

```virex
unsafe {
    *p = 99;
}
```

**Memory Allocation:**

```virex
var i32* buffer = alloc(i32, 4);
free(buffer);
```

### 2.7 String Literals

String literals are enclosed in double quotes and support the following escape sequences:

| Escape Sequence | Description |
| --------------- | ----------- |
| `\n`            | Newline     |
| `\t`            | Tab         |
| `\r`            | Carriage return |
| `\\`            | Backslash   |
| `\"`            | Double quote |

**Example:**

```virex
var cstring message = "Hello, World!\n";
var cstring path = "C:\\Users\\Documents\\file.txt";
```

### 2.8 Arrays

* Fixed size included in type
* Stack allocated by default

**Example:**

```virex
var i32[4] values = {1, 2, 3, 4};
values[0] = 10;
```

### 2.8 Enums

* Keyword: `enum`
* Simple tagged list

**Example:**

```virex
enum Status {
    OK,
    ERROR,
    UNKNOWN
};

var Status s = OK;
if (s == ERROR) {
    // Handle error
}
```

### 2.9 Generics

* Function-level generics
* Compile-time monomorphization
* Syntax: `<T>`

**Example:**

```virex
func max<T>(T a, T b) -> T {
    if (a > b) {
        return a;
    }
    return b;
}
```

### 2.10 Example Program

```virex
struct Point {
    i32 x;
    i32 y;
};

func move(Point* p, i32 dx, i32 dy) -> void {
    unsafe {
        p->x = p->x + dx;
        p->y = p->y + dy;
    }
}

func main() -> i32 {
    var Point pt;
    pt.x = 0;
    pt.y = 0;

    move(&pt, 3, 4);
    return 0;
}
```

---

## 3. Memory Model & Pointer Safety

### 3.1 Object Lifetimes

* All variables and constants have **deterministic lifetimes** based on scope.
* Stack variables are automatically destroyed at the end of their scope.
* Heap allocations persist until explicitly freed via `free`.
* Pointers must not outlive the objects they point to; the compiler enforces scope-based validity for non-null pointers.

### 3.2 Pointer Categories

| Category | Syntax | Description                                                                   |
| -------- | ------ | ----------------------------------------------------------------------------- |
| Nullable | `*T`   | May hold `null`; must be checked or used in `unsafe` block before dereference |
| Non-null | `*!T`  | Guaranteed to never be null; compiler ensures initialization and validity     |

### 3.3 Nullability Rules

* Dereferencing a `*T` pointer outside `unsafe` is a compile-time error.
* Non-null pointers (`*!T`) cannot be assigned `null`.
* Nullable pointers can be assigned `null` at any point.

### 3.4 Dereference Rules

* Dereference operator: `*`
* Nullable pointers must be dereferenced inside an `unsafe` block:

```virex
var i32* p = &x;
unsafe {
    *p = 42;
}
```

* Non-null pointers may be dereferenced safely without `unsafe`:

```virex
var i32*! q = &y;
*q = 10;
```

### 3.5 Unsafe Semantics

* The `unsafe` keyword is required for operations that could violate memory safety:

  * Dereferencing nullable pointers
  * Manual memory manipulation
  * Pointer arithmetic (if supported)
* Code outside `unsafe` blocks is **memory-safe by construction**.

### 3.6 Aliasing and Mutation Rules

* Multiple non-null pointers to the same memory location are allowed.
* Compiler does **not enforce borrow rules**, but misuse outside `unsafe` blocks is illegal.
* Mutable aliasing within `unsafe` blocks is allowed; it is the programmer's responsibility to avoid race conditions or memory corruption.

### 3.7 Undefined Behavior vs Compile-Time Errors

* **Undefined Behavior (UB)** occurs only inside `unsafe` blocks if:

  * A pointer is dereferenced after the memory has been freed.
  * A non-null pointer is created incorrectly within `unsafe`.
* **Compile-time errors** occur when:

  * Nullable pointer is dereferenced outside `unsafe`.
  * Non-null pointer is assigned `null`.
  * Pointer is used after its lifetime ends (detected via static analysis).

### 3.8 Guaranteed Optimizations

* Since lifetimes are deterministic, compiler can safely:

  * Remove unused stack variables
  * Inline non-escaping functions
  * Allocate non-escaping heap objects on the stack
  * Perform pointer alias analysis within `unsafe` blocks, with user guarantees

---

## 4. Error Handling & Result Types

### 4.1 Philosophy

* Virex does **not use exceptions**.
* All recoverable operations return a **Result type**, encoding success or failure.
* Unrecoverable errors use `fail` to terminate the program.

### 4.2 Result Type

* Keyword: `result` (generic)
* Syntax: `result<T, E>`

  * `T` → Success type
  * `E` → Error type

**Example:**

```virex
func divide(i32 a, i32 b) -> result<i32, i32> {
    if (b == 0) {
        return result::err(1);
    }
    return result::ok(a / b);
}
```

### 4.3 Handling Results

* Pattern matching via `match` keyword:

```virex
var result<i32, i32> r = divide(10, 2);
match r {
    ok(value) => print(value);
    err(code) => print("Error: ", code);
}
```

* Must handle both success and error; ignoring a result is a compile-time warning.

### 4.4 Fail

* Keyword: `fail`
* Terminates execution immediately

```virex
if (x < 0) {
    fail("Negative value not allowed");
}
```

---

## 5. Module & Package System

### 5.1 Philosophy

* Modules provide **namespaces and code organization**.
* Packages are collections of modules.
* Default visibility is **private**; explicit `public` keyword exposes entities.
* Namespaces hierarchy: local functions and types > imported modules > std library

### 5.2 Module Declaration

* Keyword: `module`
* File-based module: the filename corresponds to the module name
* Syntax:

```virex
module "math.vx";
```

### 5.3 Importing Modules

* Keyword: `import`
* Imports use **file paths**:

  1. Compiler searches for the file in the local path relative to the current file
  2. If not found, compiler searches in the standard library paths
  3. If still not found, a compile-time error is thrown
* Example:

```virex
import "math.vx";
var i32 result = math.add(2, 3);
```

* Optional aliasing:

```virex
import "math.vx" as m;
var i32 result = m.add(2, 3);
```

### 5.4 Exporting

* Keyword: `public`
* Only entities marked `public` are accessible from other modules

```virex
public func add(i32 a, i32 b) -> i32 {
    return a + b;
}
```

### 5.5 Package Organization

* Packages are folders containing modules
* Each folder may contain a `package.vx` descriptor (optional for core compiler)
* Modules can import other modules from the same package or external packages

---

## 6. ABI & C Interop

### 6.1 Philosophy

* Virex provides direct interoperability with C code and libraries.
* Ensures that Virex types, memory layout, and calling conventions match C standards.

### 6.2 Foreign Function Interface (FFI)

* Keyword: `extern`
* Used to declare external C functions
* Syntax:

```virex
extern func printf(cstring format, ...) -> i32;
```

### 6.3 C-Compatible ABI Types

| Virex ABI Type | C Type             | Purpose                      |
| -------------- | ------------------ | ---------------------------- |
| c_char         | char               | For ABI compatibility with C |
| c_short        | short              | For ABI compatibility with C |
| c_ushort       | unsigned short     | For ABI compatibility with C |
| c_int          | int                | For ABI compatibility with C |
| c_uint         | unsigned int       | For ABI compatibility with C |
| c_long         | long               | For ABI compatibility with C |
| c_ulong        | unsigned long      | For ABI compatibility with C |
| c_longlong     | long long          | For ABI compatibility with C |
| c_ulonglong    | unsigned long long | For ABI compatibility with C |
| c_longdouble   | long double        | For ABI compatibility with C |

### 6.4 Struct Layout

* `struct` is laid out in memory **identically to C structs** by default
* Supports optional `packed` attribute to remove padding:

```virex
struct packed MyStruct {
    u8 a;
    u32 b;
};
```

### 6.5 Calling Conventions

* Default calling convention is **C ABI** (`cdecl`) on supported platforms
* Allows direct function pointer interop

### 6.6 Linking External Libraries

* Compiler accepts flags for linking external libraries:

```bash
virex build main.vx -l m
```

* Ensures symbols are resolved at link time

---

## 7. Compiler Architecture

### 7.1 Compilation Model

* Virex is an **ahead-of-time (AOT) compiled** language.
* Compilation produces native machine code for the target platform.
* No runtime or virtual machine is required.

### 7.2 Compilation Stages

1. **Lexing** – Source files are tokenized.
2. **Parsing** – Tokens are converted into an Abstract Syntax Tree (AST).
3. **Semantic Analysis**

   * Type checking
   * Name resolution
   * Generic instantiation
   * Lifetime and pointer validation
4. **Intermediate Representation (IR)**

   * Lower-level, platform-agnostic IR
   * Explicit control flow and memory operations
5. **Optimization**

   * Dead code elimination
   * Constant folding
   * Inlining
   * Escape analysis
6. **Code Generation**

   * Target-specific machine code emission (initially x86_64 Linux)
7. **Linking**

   * Static or dynamic linking
   * External symbol resolution

### 7.3 Type System Enforcement

* All types are known at compile time.
* Implicit type conversions are disallowed, except for safe numeric widening.
* ABI types (`c_int`, `c_long`, etc.) are treated as opaque-sized primitives tied to the target ABI.

### 7.4 Generics Implementation

* Generics are implemented via **monomorphization**.
* Each concrete type instantiation generates specialized code.
* No runtime overhead for generics.

### 7.5 Safety Enforcement

* Memory safety is enforced outside `unsafe` blocks.
* Pointer lifetimes and nullability are validated statically when possible.
* Unsafe blocks disable certain compiler checks but do not disable optimizations.

### 7.6 Diagnostics

* Compiler errors are precise and contextual.
* Warnings are emitted for:

  * Unused variables
  * Ignored `result` values
  * Unreachable code

### 7.7 Tooling Interface

* Compiler executable: `virex`
* Example build:

```bash
virex build main.vx
```

* Optional flags:

  * `-O0`, `-O1`, `-O2`, `-O3`
  * `--emit-ir`
  * `--emit-asm`

---

## 8. Concurrency Model (Extensions)

### 8.1 Philosophy

* Concurrency in Virex is **explicit and opt-in**.
* The core language remains single-threaded by default.
* Concurrency primitives are minimal and predictable, avoiding hidden runtime behavior.

### 8.2 Threads

* Thread support is provided via the standard library (not built-in syntax).
* Core language guarantees that:

  * Each thread has its own stack
  * Heap memory is shared

**Conceptual API (std::thread):**

```virex
var thread t = thread.spawn(func() -> void {
    // work
});

t.join();
```

### 8.3 Memory Sharing Rules

* Virex does **not** enforce data-race freedom at compile time.
* Shared mutable memory must be explicitly synchronized by the programmer.
* The compiler assumes **no synchronization unless explicitly specified**.

### 8.4 Atomic Types

* Atomic operations are provided via library types:

  * `atomic_i32`, `atomic_u64`, etc.
* Atomic types guarantee lock-free operations when supported by the hardware.

**Example:**

```virex
var atomic_i32 counter = 0;
counter.fetch_add(1);
```

### 8.5 Mutexes and Locks

* Mutual exclusion primitives are provided by the standard library:

  * `mutex<T>`
  * `rwlock<T>`

**Example:**

```virex
var mutex<i32> m = mutex.init(0);
m.lock();
*m.value = *m.value + 1;
m.unlock();
```

### 8.6 Unsafe and Concurrency

* All concurrency primitives internally rely on `unsafe` operations.
* Using concurrency APIs is considered safe at the language level.
* Writing custom lock-free or low-level concurrent code requires `unsafe` blocks.

### 8.7 Memory Model

* Virex follows the **C/C++11-style memory model**:

  * Sequential consistency by default
  * Explicit acquire/release semantics via atomic operations

---

## 9. Standard Library Core Functions

### 9.1 Philosophy

* The standard library is **minimal by design**.
* It provides only what is required to:

  * Write real programs
  * Interface with the OS
  * Support memory, errors, and concurrency
* Anything non-essential lives in external libraries.

The standard library is referred to as `std`.

---

### 9.2 Core Modules

#### 9.2.1 `std::mem`

Low-level memory operations.

Provided functions:

```virex
func alloc<T>(usize count) -> T*;
func free<T>(T* ptr) -> void;
func copy<T>(T* dst, T* src, usize count) -> void;
func set<T>(T* dst, T value, usize count) -> void;
```

Notes:

* All functions may require `unsafe` depending on usage.
* `alloc` returns a nullable pointer.

---

#### 9.2.2 `std::result`

Utilities for working with `result<T, E>`.

Provided helpers:

```virex
func unwrap<T, E>(result<T, E> r) -> T;    // fails on err
func expect<T, E>(result<T, E> r, cstring msg) -> T;
```

---

#### 9.2.3 `std::io`

Basic input/output facilities.

Provided functions:

```virex
func print<T>(T value) -> void;
func println<T>(T value) -> void;
```

Notes:

* Implemented internally via platform-specific backends.
* No formatting macros or complex formatting in core.

---

#### 9.2.4 `std::thread`

Threading primitives.

Provided types and functions:

```virex
struct thread;

func spawn(func() -> void) -> thread;
func join(thread* t) -> void;
```

---

#### 9.2.5 `std::sync`

Synchronization primitives.

Provided types:

```virex
struct mutex<T>;
struct rwlock<T>;

func mutex.init<T>(T value) -> mutex<T>;
func rwlock.init<T>(T value) -> rwlock<T>;
```

---

#### 9.2.6 `std::atomic`

Atomic operations.

Provided atomic types:

* `atomic_i32`
* `atomic_u32`
* `atomic_i64`
* `atomic_u64`
* `atomic_bool`

Each atomic type provides:

```virex
func load() -> T;
func store(T value) -> void;
func fetch_add(T value) -> T;
func fetch_sub(T value) -> T;
```

---

### 9.3 `std::os`

Minimal OS interaction layer.

Provided functionality:

* Process exit
* Environment variables
* File descriptors (opaque)

Example:

```virex
func exit(i32 code) -> void;
```

---

### 9.4 What Is Explicitly NOT Included

* No collections (Vec, Map, etc.)
* No strings beyond `cstring`
* No filesystem abstraction
* No async runtime
* No reflection
* No garbage collection

These are intended to live in **external libraries**.

---

### 9.5 Stability Guarantees

* Core language + `std` are versioned together.
* Breaking changes require a major version bump.
* ABI types (`c_*`) are guaranteed stable per platform.

---

*This completes the initial Virex CORE specification.*
