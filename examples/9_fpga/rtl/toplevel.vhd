-- libstored, a Store for Embedded Debugger.
-- Copyright (C) 2020  Jochem Rutgers
--
-- This program is free software: you can redistribute it and/or modify
-- it under the terms of the GNU Lesser General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU Lesser General Public License for more details.
--
-- You should have received a copy of the GNU Lesser General Public License
-- along with this program.  If not, see <https://www.gnu.org/licenses/>.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity example_9_fpga is
end entity;

architecture rtl of example_9_fpga is
	constant SYSTEM_CLK_FREQ : integer := 100e6;

	signal clk, rstn : std_logic;
	signal done : boolean;
begin

	store_inst : entity work.ExampleFpga_hdl
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

		if done then
			wait;
		end if;
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

end rtl;
