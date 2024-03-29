# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

launch_simulation -noclean_dir
run all
if { [get_value /test_fpga/done] == FALSE } {
	# Did not finish the simulation successfully.
	error "FAILED"
}
close_sim
