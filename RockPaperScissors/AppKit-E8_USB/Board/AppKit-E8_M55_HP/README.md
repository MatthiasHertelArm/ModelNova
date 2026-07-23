# Board: AlifSemiconductor AppKit-E8-AIML

## Board Layer for M55 High Performance Core

Device: AE822FA0E5597BS0:M55_HP

This setup is configured using **Conductor Tool**, an interactive tool provided by Alif Semiconductor for device configuration.
Refer to ["Alif Conductor Tool Manual"](https://conductor.alifsemi.com/Alif_HTML_DCT_User_Help/Content/Help%20Manual.htm) for additional information.

### System Configuration

| System Component        | Setting
|:------------------------|:----------------------------------------
| Heap                    | 96 kB (configured in M55_HP linker file)
| Stack (MSP)             |  8 kB (configured in M55_HP linker file)

### STDIO mapping

**STDIO** is routed to Virtual COM port via **UART4** peripheral

> Note:
> For STDOUT (printf output) configure SW4 to position U4 (UART4)

### CMSIS-Driver mapping

| CMSIS-Driver           | Peripheral | Board connector/component    | Connection
|:-----------------------|:-----------|:-----------------------------|:----------------------
| Driver_ETH_MAC0        | ETH        | Ethernet RJ45 connector (J4) | CMSIS_ETH
| Driver_USART4          | UART4      | PRG USB connector (J3)       | STDIN, STDOUT, STDERR
| Driver_USBD0           | USB        | MCU USB connector (J2)       | CMSIS_USB_Device
| CMSIS-Driver VIO       | GPIO       | RGB LED, Joystick            | CMSIS_VIO
| Driver_vStreamVideoIn  | MIPI CSI   | MIPI Camera (J9)             | CMSIS_VSTREAM_VIDEO_IN
| Driver_vStreamVideoOut | MIPI DSI   | GLCD Display (J15)           | CMSIS_VSTREAM_VIDEO_OUT

### CMSIS-Driver Virtual I/O mapping

| CMSIS-Driver VIO | Board component
|:-----------------|:----------------------------
|vioBUTTON0        | Joystick Select Button
|vioJOYup          | Joystick Up
|vioJOYdown        | Joystick Down
|vioJOYleft        | Joystick Left
|vioJOYright       | Joystick Right
|vioJOYselect      | Joystick Select Button
|vioLED0           | RGB LED Red
|vioLED1           | RGB LED Green
|vioLED2           | RGB LED Blue

### CMSIS-Driver vStream configuration

| Driver                 | Stream Format Description
|:-----------------------|:----------------------------------------------------
| Driver_vStreamAudioIn  | 16-bit PCM audio,      16000 samples/second
| Driver_vStreamVideoIn  | RAW8 Bayer GBRG video, resolution  640 x 480 (W x H)
| Driver_vStreamVideoOut | RGB888 video,          resolution  480 x 800 (W x H)

### Camera module: OV5675 (MIPI CSI-2)

The example uses the **OV5675** camera module on the MIPI camera connector
(J9), configured for its VGA mode (640 x 480, 4x4 binned, RAW10 over 2 CSI
lanes). The CPI captures the stream directly as RAW8 Bayer (GBRG) frames and
the application performs debayering in software; the sensor-internal AEC/AGC
drives the exposure.

The sensor driver is a local copy of the pack driver
(`OV5675_Camera_Sensor.c`), with the ISP auto-exposure hooks additionally
gated on `RTE_ISP` so that the sensor's internal auto-exposure stays enabled
when the ISP is not used.

The hardware ISP path (CSI -> ISP -> CPI, `RTE_ISP=1` +
`RTE_CPI_ISP_PORT=1`) is prepared in `vstream_video_in.c` (ISP output
buffer queueing, ISP memory-interface frame events) and `ov5675_isp_param.c`
(color fixes from alif_ensemble-cmsis-dfp PR #63), but is disabled: with
the Ensemble 2.2.0 pack the ISP stops receiving frames after the first
v-sync. Re-evaluate when a pack containing the PR #63 ISP rework is
released.

## SETOOLS

Before using layers on the board it is required to program the ATOC of the device
using the Alif SETOOLS. The required `.vscode\tasks.json` commands are part of the
Blinky examples. It is therefore recommended to start with such an example.

Refer to the section [Usage](https://www.keil.arm.com/packs/ensemble-alifsemiconductor)
in the overview page of the Alif Semiconductor Ensemble DFP/BSP for information on how
to setup these tools.

In VS Code use the menu command **Terminal - Run Tasks** and execute:

- "Alif: Install M55_HE or M55_HP debug stubs (single core configuration)"

> Note: For Windows ensure that the Terminal default is `Git Bash` or `PowerShell`.
> - Configure SW4 to position SE (Secure UART) to enable SETOOLS communication with the device.
