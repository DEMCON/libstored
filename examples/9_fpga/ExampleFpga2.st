// SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
//
// SPDX-License-Identifier: CC0-1.0

// I assume that the FPGA has only one Synchronizer interface,
// which is likely to be a UART, or otherwise byte-oriented stream.
// If there are multiple stores in the FPGA, like one store per module in the FPGA,
// as that matches nicely to the same amount of AXI slaves, they are daisy chained,
// such that this single UART/Synchronizer interface can access both.
int8 other store's int8

