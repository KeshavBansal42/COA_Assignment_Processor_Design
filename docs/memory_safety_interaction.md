# Memory Safety Module: CPU Interaction Report

This document outlines exactly how, when, and why Keshav's Simulator Core talks to your Memory Safety module, and how your code controls the fate of the CPU.

## 1. The Core Workflow (How they communicate)
Your memory safety module acts as a **gatekeeper**. Neither the sequential `Pipeline` nor the overlapped `CyclePipeline` is allowed to touch memory or the stack without asking your module for permission first.

The workflow always looks like this:
1. The executable stage calculates a memory address or stack pointer.
2. The pipeline pauses and calls a function in your `MemorySafetyImpl` class.
3. Your code evaluates the address and returns an `ExceptionCode`.
4. If your code returns `ExceptionCode::NONE`, the `Pipeline` proceeds with the operation.
5. If your code returns any other error (e.g., `STACK_OVERFLOW`), the `Pipeline` immediately **aborts the operation**, sets the CPU's internal exception state to your error, and permanently halts the processor.

---

## 2. Specific CPU Triggers & Your Responses

### A. Stack Operations (`PUSH`, `POP`, `CALL`, `RET`)
Whenever the CPU attempts to use the stack, it transfers the current Stack Pointer (SP) and the direction of the operation to your module.

* **The Call:** `safety_.checkStack(cpu_.sp(), isPush)`
* **Your Code's Logic & Response:**
  * **Trigger 1 (Pushing):** If `isPush == true` (used by `PUSH` and `CALL`), your code checks if the new SP will drop below the current stack floor (`heapTop_`). That floor is `0x0C00` in the default fixed mode, or the heap high-water mark when the dynamic stack is enabled.
    * *Response:* If it does, you throw `ExceptionCode::STACK_OVERFLOW`.
  * **Trigger 2 (Popping):** If `isPush == false` (used by `POP` and `RET`), your code checks if the current SP is already at or above `0x1000`.
    * *Response:* If it is, you throw `ExceptionCode::STACK_UNDERFLOW`.
  * **Safe:** If neither bound is violated, you return `ExceptionCode::NONE`.

### A.1 Dynamic stack and heap boundary
`MemorySafety` also owns the heap high-water mark. `cpu.setDynamicStack(true)` (or the
CLI's `dynamic on`) sets `heapTop_` to `max(heapAllocTop_, kDataBase)`, so an empty heap
leaves the whole data region available and the stack may pass `0xC00`. `dynamic off`
restores the fixed `0xC00` floor. `allocHeap` grows the heap upward and fails if the
request would reach the current SP, and `availableFreeWords(sp)` reports the gap between
`heapTop_` and SP. `Cpu::memorySafety()` exposes this state to the CLI and the
visualizer, which draw the floor and heap usage in the stack panel.

### B. Standard Memory Access (`LW`, `SW`)
Whenever the CPU attempts to Load Word (read) or Store Word (write) to general memory, it calculates the target address and asks your module for permission.

* **The Call:** `safety_.checkMem(targetAddr, isWrite)`
* **Your Code's Logic & Response:**
  * **Trigger 1 (Out of Bounds):** Your code checks if the address is greater than `0x0FFF` (the physical limit of the RAM).
    * *Response:* If it is, you throw `ExceptionCode::INVALID_MEM_ACCESS`.
  * **Trigger 2 (Code Corruption):** Your code checks if `isWrite == true` AND the address is `<= 0x07FF` (the protected Code region).
    * *Response:* If the CPU is trying to overwrite the program instructions, you throw `ExceptionCode::INVALID_MEM_ACCESS`.
  * **Safe:** If the address is safe, you return `ExceptionCode::NONE`.

### C. Program Counter Fetch (`checkPC`)
*Note on architecture:* `checkPC()` in the safety module raises `PC_OUT_OF_RANGE` if the processor tries to execute code past `0x07FF`. The sequential `Pipeline::fetch()` and the overlapped `CyclePipeline` fetch stage perform the same bound check natively before reading memory, so the CPU is fully protected in every pipeline mode; `checkPC()` remains available for direct use and tests.

---

## 3. CLI (Command Line Interface) Interactions
Your module doesn't just block bad code; it also controls how the user interacts with the simulator in the terminal.

### A. Exception Translations
Whenever the CPU halts due to an exception (whether caught by your module, the ALU dividing by zero, or an illegal instruction), the CLI needs to tell the user what happened.
* **The Call:** `exceptionToString(cpu_.exception())`
* **Your Response:** The CLI passes the raw `ExceptionCode` enum to your function. Your code uses a `switch` statement to return a beautifully formatted, human-readable `std::string` (e.g., `"Stack overflow (SP crossed the stack floor)"`). The CLI prints this directly to the user.

### B. The Stack Visualizer
When the user types `stack` in the simulator terminal to debug their program, the CLI hands complete control over to your module.
* **The Call:** `renderStackAscii(cpu_.rawMemory(), cpu_.sp(), maxDepth, cpu_.memorySafety().heapTop())`
* **Your Response:** Your code takes the raw memory array, the current SP and the active stack floor, calculates the boundaries, and prints a fully formatted ASCII GUI to the terminal. It highlights the current SP, warns the user in red text when SP reaches the floor, and maps out standard stack frames. In dynamic mode the CLI passes the heap high-water mark instead of `0xC00`, so the view follows the stack into the data region.
