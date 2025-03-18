# ES-synth-starter

  
|Arif|  |
|Sandro|--|
| Sophie  | sj922 |

##Theoretical Minimum Initiation Interval (II) and Execution Time Measurement

## **1. Assumptions**
- **Clock Speed**: 72 MHz (typical for STM32F103)
 **Tasks & Interrupts**: The execution time depends on:
  - `scanKeysTask()`
  - `displayUpdateTask()`
  - `setISR()` (Interrupt Service Routine)
  - `decodeTask()`
  - `CAN_RX_ISR()`
- **Key bottleneck**: `setISR()` runs at the highest priority and executes for every audio sample.
## **2. Theoretical Minimum Initiation Interval**
The minamum II is dictated by the task with the fastest periodic time
| Task Name           | Periodicity (ms) |
|---------------------|----------------|
| `scanKeysTask`      | 20             |
| `displayUpdateTask` | 100            |
| `setISR` (ISR)      | 0.04545 |
| `decodeTask` (CAN)  | Event-driven |
- Since `setISR()` is the **fastest executing routine**, the **theoretical minimum II** is **0.04545 μs**