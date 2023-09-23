# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

create_clock -period 10.000 -name clk -waveform {0.000 5.000} [get_ports clk]
