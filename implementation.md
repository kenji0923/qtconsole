# Realtime console app using Qt6

## Overview

This is a C++ / Qt6 console app to show measurement results with negligible latency for realtime optimization.
Refresh rate could be upto 60Hz and latency should be less than 10ms.
Three visualization modes are prepared.
Each visualization can be enabled / disabled for optimized performance.
Visualization panes can be selected by tab in a single window, or undocked / docked to side or below / above of an existing visualization.

## Visualization

### 1. Numeric and ratio bar
Large numeric value and color bar representing the ratio of the reading to the maximum value. The max value can be specified in this console.

### 2. Time series
Time series chart. Min and max can be specified or automatically scaled. Can reset the history by button.

### 3. Statistics
Statistics to show current value, average, standard deviation, min and max, acquired number, and acquisition rate. Can control by start and stop button.

## Communicatoin
Accepts TCP through WebSocket with keep-alive feature, or UDP packet at the port number which can be specified. Would be expanded for other methods like serial port.

## Configurations
- Save and load functions. Auto-save the configurations, including window layouts, to OS-specific locations. The organization name is "kshu" and app name is "qtconsole".
- Line input to set the title of measurement. The widget title should be updated dynamically according to this as "qtconsole - {title_of_measurement}".
- An offset for the received values and scale factor for the received values. Then values will be recorded as "scale_factor * raw_value + offset".
- Averaging window length used in Numeric / Ratio and Time series.
- Time duration (width of x-axis) for time series
- Export function for time series, as numeric data (timestamp,raw_value,processed_value) or image files (png or pdf).

## Coding rules
- Follow Google C++ Style Guide
