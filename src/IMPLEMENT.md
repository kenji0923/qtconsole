# Realtime console app using Qt6

## Overview

This is a C++ / Qt6 console app to show measurement results with negligible latency for realtime optimization.
Refresh rate could be upto 60Hz and latency should be less than 10ms.
Three visualization modes are prepared.
Each visualization can be enabled / disabled for optimized performance.
Visualization panes can be selected by tab in a single window, or undocked / docked to side or below / above of an existing visualization.

## Visualization

### 1. Numeric and ratio bar
Large numeric value and color bar representing the ratio of the reading to the maximum / minimum value. The max value can be specified in this console.

### 2. Time series
Time series chart. Min and max can be specified or automatically scaled. Can reset the history by button. Can pause by button.

### 3. Statistics
Statistics to show current value, average, standard deviation, min and max, acquired number, and acquisition rate. Can control by start and stop button.

## Communicatoin
Accepts TCP through WebSocket with keep-alive feature, or UDP packet at the port number which can be specified. Would be expanded for other methods like serial port.

## Configurations
- Auto-save the configurations, including window layouts, to OS-specific locations. The organization name is "kshu" and app name is the window title defined by the following line. From the "File" menu, configurations can be saved and loaded to/from a specified path.
- Line input to set the title of measurement. The widget title should be updated dynamically according to this such as "qtconsole_{receiver_protocol(UDP|WS)}{port_number}_{title_of_measurement}". For Windows, use ini files to be put in AppData.
- A transform equation applied to each received value, where `x` is the raw value (e.g. "sqrt(x)*2 + sin(x)"). Supports arithmetic and functions (exp, sin, cos, tan, sqrt, pow, log, abs, ...) via exprtk. An invalid equation falls back to the identity (x). Legacy offset/scale settings migrate to "scale*x + offset".
- A printf-style display format for values (e.g. "%.3f", "%8.2e", "%.1f V"); must contain exactly one float conversion (e/E/f/F/g/G).
- Averaging window length used in Numeric / Ratio and Time series.
- Time duration (width of x-axis) for time series
- Export function for time series, as numeric data (timestamp,raw_value,processed_value) or image files (png or pdf).
- Can hide the content of Input panel
- Can toggle always-on-top by a button in view menu
- Can specify the listening protocol, port, and measurement title by options at starting the app

## Coding rules
- Follow Google C++ Style Guide
