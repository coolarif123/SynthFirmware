# ES-synth-starter

Sandro,Sophie and Arif

## Theoretical Minimum Initiation Interval (II) and Execution Time Measurement

### **1. Assumptions**

 **Tasks & Interrupts**:

 The execution time depends on:

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

### **3. Execution Time Measurement**

To measure execution time, we used the `micros()` function.

| Task Name           | Time (ms) |
|---------------------|----------------|
| `scanKeysTask`      |         |
| `displayUpdateTask` |       |
| `setISR` (ISR)      |  |
| `decodeTask` (CAN)  |  |

## Tasks and thier implementation

### **Core Functions**

#### 1. Display Interface

To display information, such as the current pressed key, we used an OLED display in a slave/master configuration.

#### 2. Note Detection

To know which note is being pressed we scan the inputs/key matrix to see what is being pressed. It cycles through different rows and reads the state of columns, forming a bitset that represents active notes.

#### 3. Knob Detection

This works the same as the note detection however, another step is required as to decode the knob we had to keep track of its previous state and the current state its in to decide on what change has taken place.

#### 4. Sound Generation

When a key press is detected, the corresponding frequency is calculated based on the current octave and waveform settings and a step size is made to generate the correct sound output. If a key is released, the system updates the step size to zero; stopping the note .

#### 5. Communication

In order to send relliable communication between one singular sender and multiple diffrent recivers, we used CAN communication. When sending messages, the message were limited to 8 bits. It used an interrupt-driven approach to handle incoming CAN messages and a queueing system to manage the outgoing ones.

### **Advance Functions**

## Critical instant Analysis of the Monotonic Scheduler

## Dependancies, and Shared Structures and  Methods

| **Data Structure**  | **Usage** | **Potential Conflict** | **Synchronization Method** | **Deadlock Risks & Solutions** |
|---------------------|----------|------------------------|---------------------------|--------------------------------|
| **GPIO Key States** | Stores current keyboard key states ehich is read in `scanKeysTask()` | Multiple tasks may want to read and/or write simultaneously | Use **Mutex** to protect the access | **Deadlock Risk**: If a high-priority task holds onto the mutex too long → **Solution**: Keep critical sections short |
| **Clock Configuration (`RCC_OscInitTypeDef`, `RCC_ClkInitTypeDef`)** | Configures the system's clocks at startup | If it is reconfigured during runtime the system will become unstable | Avoid modifying the clocks dynamically | **Deadlock Risk**: If a task waits for the clock to reconfigur while another task holds a lock → **Solution**: Ensure clocks are configured at boot and remain static |
| **USB Clock Settings (`PeriphClkInit`)** | Configures the  peripheral USB clock | If modified dynamically, the USB may lose synchronization | Use **semaphores** for synchronization | **Deadlock Risk**: USB task may block others who are waiting for clock access → **Solution**: Assign a higher priority to the USB clock task |
| **Tick Timing (`portTICK_PERIOD_MS`)** | Controls task execution frequency | Tasks may not recieve data/run if execution timing is not managed properly | Ensure **timeouts** on blocking calls | **Deadlock Risk**: A task may block others indefinitely while waiting for a resource → **Solution**: Use **timeout-based semaphores** (`xSemaphoreTake()` with timeout) |

### Stratagies to prevent Deadlocking

- Use timeouts for blocking calls: sSemaphoreTake() has a timeout
- Always lock in the same order to stop circualar weights
- Useage of Prioroties for mutex
- USB clock is always static when configured

## CPU Utilization
