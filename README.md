# ES-synth-starter
Sandro,Sophie and Arif
## Theoretical Minimum Initiation Interval (II) and Execution Time Measurement

### **1. Assumptions**
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
| **Data Structure**  | **Usage** | **Potential Conflict** | **Synchronization Method** | **Deadlock Risks & Solutions** |
|---------------------|----------|------------------------|---------------------------|--------------------------------|
| **GPIO Key States** | Stores current keyboard key states ehich is read in `scanKeysTask()` | Multiple tasks may want to read and/or write simultaneously | Use **Mutex** to protect the access | **Deadlock Risk**: If a high-priority task holds onto the mutex too long → **Solution**: Keep critical sections short |
| **Clock Configuration (`RCC_OscInitTypeDef`, `RCC_ClkInitTypeDef`)** | Configures the system's clocks at startup | If it is reconfigured during runtime the system will become unstable | Avoid modifying the clocks dynamically | **Deadlock Risk**: If a task waits for the clock to reconfigur while another task holds a lock → **Solution**: Ensure clocks are configured at boot and remain static |
| **USB Clock Settings (`PeriphClkInit`)** | Configures the  peripheral USB clock | If modified dynamically, the USB may lose synchronization | Use **semaphores** for synchronization | **Deadlock Risk**: USB task may block others who are waiting for clock access → **Solution**: Assign a higher priority to the USB clock task |
| **Tick Timing (`portTICK_PERIOD_MS`)** | Controls task execution frequency | Tasks may not recieve data/run if execution timing is not managed properly | Ensure **timeouts** on blocking calls | **Deadlock Risk**: A task may block others indefinitely while waiting for a resource → **Solution**: Use **timeout-based semaphores** (`xSemaphoreTake()` with timeout) |


## CPU Utilization