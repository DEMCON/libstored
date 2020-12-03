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

use work.libstored_pkg.all;
use work.ExampleFpga_pkg;
use work.ExampleFpga2_pkg;

entity example_9_fpga is
	generic (
		SYSTEM_CLK_FREQ : integer := 100e6;
		SIMULATION : boolean := false
--pragma translate_off
			or true
--pragma translate_on
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		rx : in std_logic := '1';
		tx : out std_logic;
		cts : in std_logic := '0';
		rts : out std_logic
	);
end entity;

architecture rtl of example_9_fpga is
	function var_access return ExampleFpga_pkg.var_access_t is
		variable v : ExampleFpga_pkg.var_access_t;
	begin
		v := ExampleFpga_pkg.VAR_ACCESS_RW;

		-- Save some LUTs by limiting how we are going to access these variables.
		v.\t_clk\ := ACCESS_WO;
		v.\default_register_write_count\ := ACCESS_WO;
		return v;
	end function;

	signal var_out : ExampleFpga_pkg.var_out_t;
	signal var_in : ExampleFpga_pkg.var_in_t;
	signal var2_out : ExampleFpga2_pkg.var_out_t;
	signal var2_in : ExampleFpga2_pkg.var_in_t;
	signal sync_in, sync_out, sync_chained_in, sync_chained_out : msg_t;
	signal sync_chained_id : unsigned(15 downto 0);

	signal term_encode_in, term_decode_out : msg_t;
	signal uart_encode_in, uart_decode_out : msg_t;

	signal clk_cnt, write_cnt : unsigned(31 downto 0);
begin

	-----------------------------------------
	-- Store instances
	-----------------------------------------

	store_inst : entity work.ExampleFpga_hdl
		generic map (
			SYSTEM_CLK_FREQ => SYSTEM_CLK_FREQ,
			VAR_ACCESS => var_access,
			SIMULATION => SIMULATION
		)
		port map (
			clk => clk,
			rstn => rstn,
			var_out => var_out,
			var_in => var_in,
			sync_in => sync_in,
			sync_out => sync_out,
			sync_id => sync_chained_id --,
--			sync_chained_in => sync_chained_out,
--			sync_chained_out => sync_chained_in
		);

	store2_inst : entity work.ExampleFpga2_hdl
		generic map (
			SYSTEM_CLK_FREQ => SYSTEM_CLK_FREQ,
			SIMULATION => SIMULATION
		)
		port map (
			clk => clk,
			rstn => rstn,
			var_out => var2_out,
			var_in => var2_in,
--			sync_in => sync_chained_in,
--			sync_out => sync_chained_out,
			sync_chained_id => sync_chained_id
		);


	-----------------------------------------
	-- Protocol stack for Synchronizer
	-----------------------------------------

	ASCIIEscapeLayer_inst : entity work.ASCIIEscapeLayer
		port map (
			clk => clk,
			rstn => rstn,
			encode_in => sync_out,
			encode_out => term_encode_in,
			decode_in => term_decode_out,
			decode_out => sync_in
		);

	TerminalLayer_inst : entity work.TerminalLayer
		port map (
			clk => clk,
			rstn => rstn,
			encode_in => term_encode_in,
			encode_out => uart_encode_in,
			decode_in => uart_decode_out,
			decode_out => term_decode_out
		);

	uart_g : if not SIMULATION generate
	begin
		UARTLayer_inst : entity work.UARTLayer
			generic map (
				SYSTEM_CLK_FREQ => SYSTEM_CLK_FREQ
			)
			port map (
				clk => clk,
				rstn => rstn,
				encode_in => uart_encode_in,
				decode_out => uart_decode_out,
				rx => rx,
				tx => tx,
				cts => cts,
				rts => rts
			);
	end generate;

--pragma translate_off
	file_g : if SIMULATION generate
	begin
		FileLayer_inst : entity work.FileLayer
			generic map (
--				FILENAME_IN => "../../../../../stack_in.txt",
--				FILENAME_OUT => "../../../../../stack_out.txt"
				FILENAME_IN => "\\.\pipe\9_fpga_to_xsim",
				FILENAME_OUT => "\\.\pipe\9_fpga_from_xsim"
			)
			port map (
				clk => clk,
				rstn => rstn,
				encode_in => uart_encode_in,
				decode_out => uart_decode_out
			);
	end generate;
--pragma translate_on


	-----------------------------------------
	-- Implementation of application
	-----------------------------------------

	process(clk)
	begin
		if rising_edge(clk) then
			var_in <= ExampleFpga_pkg.var_in_default;



			-- implementation of ExampleFpga/t (clk)
			var_in.\t_clk\.value <= resize(clk_cnt, var_in.\t_clk\.value'length);
			var_in.\t_clk\.we <= '1';
			clk_cnt <= clk_cnt + 1;

			if rstn /= '1' then
				var_in.\t_clk\.we <= '0';
				clk_cnt <= (others => '0');
			end if;



			-- implementaton of ExampleFpga/read-only register
			var_in.\read_only_register\.value <= x"abcd";
			var_in.\read_only_register\.we <= '1';



			-- implementation of ExampleFpga/default register write count
			if var_out.\default_register\.updated = '1' then
				write_cnt <= write_cnt + 1;
			end if;

			var_in.\default_register_write_count\.value <=
				resize(write_cnt, var_in.\default_register_write_count\.value'length);
			var_in.\default_register_write_count\.we <= '1';

			if rstn /= '1' then
				write_cnt <= (others => '0');
				var_in.\default_register_write_count\.we <= '0';
			end if;
		end if;
	end process;

end rtl;
