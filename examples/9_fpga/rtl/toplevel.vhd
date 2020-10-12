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

use work.ExampleFpga_pkg;

use work.libstored_tb_pkg.all;

entity example_9_fpga is
end entity;

architecture behav of example_9_fpga is
	constant SYSTEM_CLK_FREQ : integer := 100e6;

	signal clk, rstn : std_logic;
	signal done : boolean;

	signal \default_register__out\ : ExampleFpga_pkg.\default_register__type\;
	signal \default_register__out_changed\ : std_logic;
	signal \default_register__in\ : ExampleFpga_pkg.\default_register__type\;
	signal \default_register__in_we\ : std_logic;

	signal \initialized_register__out\ : ExampleFpga_pkg.\initialized_register__type\;
	signal \initialized_register__out_changed\ : std_logic;
	signal \initialized_register__in\ : ExampleFpga_pkg.\initialized_register__type\;
	signal \initialized_register__in_we\ : std_logic;

	signal \read_only_register__out\ : ExampleFpga_pkg.\read_only_register__type\;
	signal \read_only_register__out_changed\ : std_logic;
	signal \read_only_register__in\ : ExampleFpga_pkg.\read_only_register__type\;
	signal \read_only_register__in_we\ : std_logic;
begin

	store_inst : entity work.ExampleFpga_hdl
		generic map (
			SYSTEM_CLK_FREQ => SYSTEM_CLK_FREQ,
			SIMULATION => true
		)
		port map (
			clk => clk,
			rstn => rstn,

			\default_register__out\ => \default_register__out\,
			\default_register__out_changed\ => \default_register__out_changed\,
			\default_register__in\ => \default_register__in\,
			\default_register__in_we\ => \default_register__in_we\,

			\initialized_register__out\ => \initialized_register__out\,
			\initialized_register__out_changed\ => \initialized_register__out_changed\,
			\initialized_register__in\ => \initialized_register__in\,
			\initialized_register__in_we\ => \initialized_register__in_we\,

			\read_only_register__out\ => \read_only_register__out\,
			\read_only_register__out_changed\ => \read_only_register__out_changed\,
			\read_only_register__in\ => \read_only_register__in\,
			\read_only_register__in_we\ => \read_only_register__in_we\
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

	process
		variable test : test_t;
	begin
		\default_register__in_we\ <= '0';
		\initialized_register__in_we\ <= '0';
		\read_only_register__in_we\ <= '1';
		\read_only_register__in\ <= x"12345678";

		wait until rising_edge(clk) and rstn = '1';
		wait until rising_edge(clk);
		wait until rising_edge(clk);

		test_init(test);
		test_verbose(test);

		test_start(test, "ReadOnly");
		test_expect_eq(test, \read_only_register__out\, x"12345678");

		test_finish(test);
		wait for 1 us;
		done <= true;
		wait;
	end process;

end behav;
