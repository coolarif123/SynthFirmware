# Embedded Systems Coursework 2 - Synthesizer Firmware 

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

#### 1. Sine Wave

We also decided to offer the user the option to change the waveform from the  described sawtooth to a sin wave. The sin w ave is also implemented by a look up table.

#### 2. Polyphoney

We implemented polyphoney using a modular design. We used a voice management sysrem that keeps track of which controller's speaker is active and assigns it a note to output. We used a priority-based allocation where the oldest or quietest voice is stolen when all voices are in use.

## Critical instant Analysis of the Monotonic Scheduler

## Dependancies, and Shared Structures and  Methods

| **Data Structure / Mechanism**                 | **Usage** | **Potential Conflict** | **Synchronization Method** | **Deadlock Risks & Solutions** |
|------------------------------------------------|-----------|------------------------|----------------------------|--------------------------------|
| **GPIO Key States**                            | Stores current keyboard key states read in `scanKeysTask()` | Multiple tasks can read and/or write concurrently | **Mutex** protects access | **Deadlock Risk**: High-priority tasks holding the mutex too long. **Solution**: Keep critical sections short. |
| **Clock Configuration (`RCC_OscInitTypeDef`, `RCC_ClkInitTypeDef`)** | Configures system clocks at startup | Reconfiguring clocks at runtime may destabilize the system | Configure at boot; avoid dynamic changes | **Deadlock Risk**: Tasks waiting on clock reconfiguration locks. **Solution**: Set clocks once and keep them static. |
| **USB Clock Settings (`PeriphClkInit`)**       | Configures the USB peripheral clock | Dynamic modifications can lead to loss of synchronization | **Semaphores** for coordination | **Deadlock Risk**: Blocking by USB tasks. **Solution**: Assign higher priority to USB clock management tasks. |
| **Tick Timing (`portTICK_PERIOD_MS`)**         | Controls task execution frequency | Incorrect timing may prevent tasks from receiving data or executing properly | Use **timeouts** on blocking calls | **Deadlock Risk**: Indefinite blocking waiting for resources. **Solution**: Use timeout-based semaphores (e.g., `xSemaphoreTake()` with timeout). |
| **Atomic Operations on Shared Variables** (e.g., `volume`, `octave`, `waveform`, `currentStepSize`) | Ensures that shared variables are read/written consistently between tasks and ISRs | Race conditions if accessed concurrently without protection | **Atomic load/store** functions (`__atomic_load_n`, `__atomic_store_n`) guarantee uninterrupted operations | **Deadlock Risk**: Minimal, but avoid using in long loops or critical ISR sections that could delay other tasks. |
| **Message Queues (`msgInQ`, `msgOutQ`)**         | Facilitate inter-task and ISR communication (e.g., CAN messages, key scan events) | Simultaneous access may lead to message corruption or loss | **FreeRTOS Queue API** which is thread-safe; combined with **semaphores** for additional control (e.g., CAN TX Semaphore) | **Deadlock Risk**: Blocking on queue operations might occur. **Solution**: Implement timeout-based queue operations to prevent indefinite waiting. |

### Stratagies to prevent Deadlocking

- Use timeouts for blocking calls: sSemaphoreTake() has a timeout
- Always lock in the same order to stop circualar weights
- Useage of Prioroties for mutex
- USB clock is always static when configured
- atomic stores to ensure the store operation happens in a single step and guaranteeing that  read or write operations do not occur during the store cycles.

## CPU Utilization

| Task Name           | CPU used (%) |
|---------------------|----------------|
| `scanKeysTask`      |         |
| `displayUpdateTask` |       |
| `setISR` (ISR)      |  |
| `decodeTask` (CAN)  |  |
