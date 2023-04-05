-- libstored, distributed debuggable data stores.
-- Copyright (C) 2020-2023  Jochem Rutgers
--
-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at https://mozilla.org/MPL/2.0/.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

use work.ExampleFpga_pkg;
use work.ExampleFpga2_pkg;

use work.libstored_tb_pkg.all;

entity example_9_fpga_tb is
end entity;

architecture behav of example_9_fpga_tb is
	constant SYSTEM_CLK_FREQ : integer := 100e6;

	signal clk, rstn : std_logic;
begin

	dut : entity work.example_9_fpga
		generic map (
			SYSTEM_CLK_FREQ => SYSTEM_CLK_FREQ,
			SIMULATION => true
		)
		port map (
			clk => clk,
			rstn => rstn
		);

	process
	begin
		clk <= '0';
		wait for 0.5 / real(SYSTEM_CLK_FREQ) * 1 sec;
		clk <= '1';
		wait for 0.5 / real(SYSTEM_CLK_FREQ) * 1 sec;
	end process;

	process
	begin
		rstn <= '0';
		for i in 0 to 15 loop
			wait until rising_edge(clk);
		end loop;
		rstn <= '1';
		wait;
	end process;

end behav;

