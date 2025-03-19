# ES-synth-starter
Sandro,Sophie and Arif
## Theoretical Minimum Initiation Interval (II) and Execution Time Measurement

### **1. Assumptions**
- **Clock Speed**: 72 MHz (typical for STM32F103)
 **Tasks & Interrupts**: The execution time depends on:
  - `scanKeysTask()`
  - `displayUpdateTask()`
  - `setISR()` (Interrupt Service Routine)
  - `decodeTask()`
  - `CAN_RX_ISR()`
- **Key bottleneck**: `setISR()` runs at the highest priority and executes for every audio sample.
### **2. Theoretical Minimum Initiation Interval**
The minamum II is dictated by the task with the fastest periodic time
| Task Name           | Periodicity (ms) |
|---------------------|----------------|
| `scanKeysTask`      | 20             |
| `displayUpdateTask` | 100            |
| `setISR` (ISR)      | 0.04545 |
| `decodeTask` (CAN)  | Event-driven |

Since `setISR()` is the **fastest executing routine**, the **theoretical minimum II** is **0.04545 μs**
## **3. Execution Time Measurement**
To measure execution time, we used the `micros()` function.
| Task Name           | Time (ms) |
|---------------------|----------------|
| `scanKeysTask`      |         |
| `displayUpdateTask` |       |
| `setISR` (ISR)      |  |
| `decodeTask` (CAN)  |  |

## Tasks and thier implementation
!!!!!!!!!!mention for addvnace tasks what they r a\and wee explanation!!!!!!

## Critical instant Analysis of the Monotonic Scheduler


## Dependancies, and Shared Structures and  Methods

## CPU Utilization