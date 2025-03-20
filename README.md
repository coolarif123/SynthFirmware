# Embedded Systems Synthesiser Coursework Report

Sandro, Sophie and, Arif

The goal of this coursework was to create software to control a music synthesiser which can interface with other synthesisers and to add further functionality to the synths. Below we will outline how this was achieved.
## **1. Basic Functionality**
All of the fundamental features specified in the github lab guide were implemented in to the system. These include:
* Ability to shsynthesis notes with a sawtooth waveform
* Multiple board CAN bus interfacing
* Keyboard setting controls (Volume, Octave)
* Display that shows control settings

### Board Control Scheme

The basic control scheme for the synthesiser.

| Name | Property | Control Nob Location | 
| --- | --- | --- | --- |
| Volume Knob | Volume | ○ ○ ○ ● | 
| Octave Knob | Octave| ○ ○ ● ○ | 
| Shape Knob | Waveform | ○ ● ○ ○ |
| Tremolo Depth/Amplitude Knob | Tremolo | ● ○ ○ ○  |

## **2. Advanced Features**
To further expand upon the base functionality of the synth, the following advanced features were built into our synth:
* LFO - Tremolo oscillation effect
* Output waveform selection
* Polyphony
* CAN Bus Automatic Detection
* Distortion effect

### LFO
#### Description
The LFO uses a rotary dial to allow the user to increase/decrease a tremolo effect to the synth output

#### Implementation
The LFO is programmed to the unused 4th dial as it was felt that it would be best to take advantage of the extra input. The volume knob implementation was used as a basis to create a generic knob class which made implementation of more knobs simpler. Tremolo is a form of AM, therefore the LFO modulates the amplitude of the output (V_out) and the depth of modulation and the rate of modulation are both encoded to the rotary dial. The value of the dial is checked similarly to the other dials, being scanned in scanKeysTask() and written to a global system variable which can then be read during the ISR. The LFO has the form of a sine wave which means that the modulation effect has a natural, smooth transition from louder to quieter volumes as it was felt that this gave the best sounding output.

### Waveform selection
#### Description
The user is given the ability to change the shape of the output waveform. Different waveforms have different sonic profiles so allowing the user to switch between them gives a greater range of sounds that they can produce. 

#### Implementation
The waveform selector is programmed to the unused 3th dial as it was felt that it would be best to take advantage of the extra input. The user is given a choice between 4 different waveforms:
* Sawtooth (default)
* Sine wave
* Square wave
* Triangle wave
Similarly to the LFO, the generic knob class simplified the implementation of this feature. The encoding of the knob scrolling and passing to global system variables was handled identically to the above case. In order to control the shape of the output waveform, however, the waveform value which is stored in the knob global is used to change the summation into the phase accumulator giving each individual output. Whenever values were updated in the scanKeysTask() memory access and stores were done using atomic memory access operations to ensure thread saftey.

### Polyphony
#### Description
Polyphony describes the boards ability to play multiple notes at once. It is configured in such a way that it is always on and it applies across all boards in the appropriate octave.

#### Implementation
Our SetISR() was changed so that there were enough phase accumulators dynamically created to allow for an accumulator per note. This allows us to play as many notes as we would like at one time. Each key writes a single bit to the inputs bitset which tracks the state of each key in the matrix and outputs the appropriate notes. During the main interupt loop, the accumulator contents are summed together and a voltage sum is produced which represents the combination of all key presses which is then passed to the DAC and then the speaker.

### CAN Auto Detection For More Than 3 Boards
#### Description
CAN Auto Detection is the ability for the boards to automatically detect when they are connected to each other, communicate with each other sending and receiving keypress, waveform, and octave messages and, to scale their octaves according to their position relative the main board. This has been done by assigning the left most keyboard as the receiver and all of the other keyboards as the transmitters.

#### Implementation
CAN Auto Detection works based on a polling approach where boards constantly poll listening for messages over their CAN bus. Upon the main board completing any action such as pressing a key or rotating a dial its action is added to its own message receive queue, and whenever another board is used the information from that action is then immediately broadcasted to the network where the leader can listen to the message and take appropriate action. 

Each board also stores information about its position relative to the main board which can be used to set the correct octave output which cuts down on the need for excessive additional processing to achieve this goal which helps to speed up the process.

The boards establish leadership status through their handshaking procedure which is initiated whenever they detect a new connection on their busses. This allows them to establish the order of all of the boards, and a receiver to adjust settings on.

## Timing Analysis
 **Tasks & Interrupts**:
 The execution time depends on:

- `scanKeysTask()`
- `displayUpdateTask()`
- `setISR()` 
- `decodeTask()`
- `CAN_RX_ISR()`
- `CAN_TX_ISR()`
- `CAN_TX_TASK()`
- **Key bottleneck**: `setISR()` runs at the highest priority and executes for every audio sample.

### **3. Critial Instant Analysis**

| Task                          | Max Execution Time /μs    | Initiation Interval/Deadline /ms | CPU Util  |
| ----------------------------- | ------------------------------| -------------------------------------| -----------|
| `scanKeysTask    `                   | 116                      | 20                                  | 0.0058 | 
|`displayUpdateTask       `            | 17417                       | 100                                   | 0.17417 |
| `decodeTask  `                      | 214                        | 25.2                                   | 0.0088492 | 
| `CAN_TX_TASK  `                      |  1.21                     | 60                                    | 0.000020167| 

Total CPU Utilisation -> 0.1889 <= $n(2^{ \frac{1}{n}} - 1)$ where n=4 (no of tasks)

In order to test CAN_TX_TASK interupts had to be enabled as the task relies on their function

## Tasks and their implementation

## ** 4.Critical instant Analysis of the Monotonic Scheduler **

### Dependancies, and Shared Structures and  Methods

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


