# Mini vending machine diagrams

Five diagrams are provided as PNG images and editable SVG vectors. Open index.html to view them together. Software function names are proposed organization, not existing firmware.

## Confirmed behavior
- Two MSPM0 controllers communicate using SPI. Master: UART1 barcode scanner, periodically scanned keypad, UI TFT, speaker 1. Slave: SD card, second TFT, speaker 2, effects, coil decoder and motors, winch and solenoid.
- Scan a readable barcode and check used-ID records before enabling selection. Previously used IDs are denied.
- Timed coil cycles operate one vending coil at a time. Ordinary input actions are blocked during dispensing while keypad scanning continues to accept the master code and Emergency Stop menu action.
- Launch OFF: record the ID after the coil stops. Launch ON: record it after the commanded launch. Completion is inferred from timing, not physically confirmed.
- LEDs and atomizers run after the coil stops in both launch modes. Their window spans the configured launch-equivalent duration plus a short tail.
- With launch enabled, a fixed candy-settling delay precedes solenoid release. After release, the winch winds to latch the block and then fully unwinds to provide slack.
- Both speakers normally play background music continuously. Key click sounds are optional. Emergency Stop is an option in Master Mode and stops music, effects and actuators; keypad scanning and LCD updates continue. Reset is also a Master Mode option. No physical reset button or separate emergency-stop state is included.
- A master code opens Master Mode, including Reset and Emergency Stop options. ID removal stays active for successive scans until Done. Other options include motor operation without scanning, motor override, and launcher enable/disable.

## Proposed SPI command vocabulary
CHECK_ID(barcode), COMMIT_USED_ID(barcode), REMOVE_ID(barcode), VEND(selection), SET_LAUNCH(enabled), SERVICE_MOTOR(parameters TBD), STOP_ALL, RESET_CONTROL, GET_STATUS.
Replies include eligible/already-used, storage result/error, accepted/busy, timing-based cycle status, and stopped. These are logical messages, not a finalized byte protocol. During SPI transfers the master must clock responses; the drawings do not imply that the slave initiates the SPI clock.

## Explicit open items
- Exact MSPM0 part numbers and pin assignments; separate/shared SPI instances and chip selects.
- Speaker output hardware, audio data format, audio source and transport to each MCU. SD card storage is preferred, not finalized. No unconfirmed SD-to-master audio stream is drawn.
- SD card electrical interface, second TFT content (graphic and/or stored-ID count), and motor/effect driver interfaces.
- Startup launcher positioning, recovery after interrupted movement, reset command behavior across both controllers, and whether reset changes any persistent records.
- SD missing/read/write failure policy and possible internal storage fallback. No fallback is assumed.
- Exact service-motor semantics, passcodes, timing constants, and transaction timeout behavior.

## Interpretation
Rectangles represent hardware. Ovals represent software modules or interfaces. Each graph includes a separate shape and arrow legend. Data flow arrows show data and control information. Call graph arrows show calls or hardware access and do not imply execution order. Periodic keypad interrupt entry is separate from main(). The Emergency Stop action in Master Mode has priority over the ordinary sequence, including post-launch recocking. No graph claims that stopping outputs physically releases a latched or tensioned launcher.
