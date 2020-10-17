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

use work.TestStore_pkg;

use work.libstored_pkg.all;
use work.libstored_tb_pkg.all;

entity test_fpga is
end entity;

architecture behav of test_fpga is
	constant SYSTEM_CLK_FREQ : integer := 100e6;

	signal clk, rstn : std_logic;
	signal done : boolean;

	signal var_in : TestStore_pkg.var_in_t;
	signal var_out : TestStore_pkg.var_out_t;

	signal axi_m2s : axi_m2s_t;
	signal axi_s2m : axi_s2m_t;
	signal sync_in, sync_out : msg_t;
	signal sync_in_busy : std_logic;
begin

	store_inst : entity work.TestStore_hdl
		generic map (
			SYSTEM_CLK_FREQ => SYSTEM_CLK_FREQ,
			AXI_SLAVE => true,
			SIMULATION => true
		)
		port map (
			clk => clk,
			rstn => rstn,

			var_out => var_out,
			var_in => var_in,

			sync_in => sync_in,
			sync_out => sync_out,
			sync_in_busy => sync_in_busy,

			s_axi_araddr => axi_m2s.araddr,
			s_axi_arready => axi_s2m.arready,
			s_axi_arvalid => axi_m2s.arvalid,
			s_axi_awaddr => axi_m2s.awaddr,
			s_axi_awready => axi_s2m.awready,
			s_axi_awvalid => axi_m2s.awvalid,
			s_axi_bready => axi_m2s.bready,
			s_axi_bresp => axi_s2m.bresp,
			s_axi_bvalid => axi_s2m.bvalid,
			s_axi_rdata => axi_s2m.rdata,
			s_axi_rready => axi_m2s.rready,
			s_axi_rresp => axi_s2m.rresp,
			s_axi_rvalid => axi_s2m.rvalid,
			s_axi_wdata => axi_m2s.wdata,
			s_axi_wready => axi_s2m.wready,
			s_axi_wvalid => axi_m2s.wvalid
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
		variable data : std_logic_vector(31 downto 0);
		variable id_in, id_out : std_logic_vector(15 downto 0);
		variable buf : buffer_t(0 to TestStore_pkg.BUFFER_LENGTH - 1);
		variable key : std_logic_vector(TestStore_pkg.KEY_LENGTH - 1 downto 0);
		variable last : boolean;
	begin
		id_out := x"aabb";

		var_in <= TestStore_pkg.var_in_default;
		axi_init(axi_m2s, axi_s2m);
		sync_init(sync_in, sync_out);

		wait until rising_edge(clk) and rstn = '1';
		wait until rising_edge(clk);
		wait until rising_edge(clk);

		test_init(test);
		test_verbose(test);



		test_start(test, "Initial");
		test_expect_eq(test, var_out.\default_int8\.value, 0);
		test_expect_eq(test, var_out.\default_int16\.value, 0);
		test_expect_eq(test, var_out.\default_int32\.value, 0);
		test_expect_eq(test, var_out.\init_decimal\.value, 42);
		test_expect_eq(test, var_out.\array_bool_0\.value, '1');
		test_expect_eq(test, var_out.\array_string_0\.value, x"00000000");



		test_start(test, "Set");
		var_in.\default_int8\.value <= x"12";
		var_in.\default_int8\.we <= '1';
		wait until rising_edge(clk);
		var_in.\default_int8\.we <= '0';
		wait until rising_edge(clk) and var_out.\default_int8\.updated = '1' for 1 ms;
		test_expect_eq(test, var_out.\default_int8\.value, 18);



		test_start(test, "AXI");
		axi_read(clk, axi_m2s, axi_s2m, TestStore_pkg.\DEFAULT_INT8__ADDR\, data);
		test_expect_eq(test, data, x"00000012");

		axi_write(clk, axi_m2s, axi_s2m, TestStore_pkg.\DEFAULT_INT16__ADDR\, x"abcd1122");
		test_expect_eq(test, var_out.\default_int16\.value, 16#1122#);

		test_start(test, "Hello");
		sync_accept_hello(clk, sync_in, sync_out, TestStore_pkg.HASH, id_in, TestStore_pkg.LITTLE_ENDIAN);
		for i in buf'range loop
			buf(i) := std_logic_vector(to_unsigned(i, 8));
		end loop;

		sync_welcome(clk, sync_in, sync_out, id_in, id_out, buf, TestStore_pkg.LITTLE_ENDIAN);



		test_start(test, "UpdateSingle");
		sync_update(clk, sync_in, sync_out, id_in, TestStore_pkg.\DEFAULT_INT8__KEY\,
			to_buffer(x"24"), TestStore_pkg.LITTLE_ENDIAN);
		sync_wait(clk, sync_in_busy);
		test_expect_eq(test, var_out.\default_int8\.value, 16#24#);



		test_start(test, "UpdateMulti");
		sync_update_start(clk, sync_in, sync_out, id_in, TestStore_pkg.LITTLE_ENDIAN);
		sync_update_var(clk, sync_in, sync_out, TestStore_pkg.\DEFAULT_INT8__KEY\,
			to_buffer(x"25"), false, TestStore_pkg.LITTLE_ENDIAN);
		sync_update_var(clk, sync_in, sync_out, TestStore_pkg.\DEFAULT_INT32__KEY\,
			to_buffer(101, 4), false, TestStore_pkg.LITTLE_ENDIAN);
		sync_update_var(clk, sync_in, sync_out, TestStore_pkg.\SOME_OTHER_SCOPE__SOME_OTHER_INNER_BOOL__KEY\,
			to_buffer('0'), true, TestStore_pkg.LITTLE_ENDIAN);
		sync_wait(clk, sync_in_busy);
		test_expect_eq(test, var_out.\default_int8\.value, 16#25#);
		test_expect_eq(test, var_out.\default_int32\.value, 101);
		test_expect_eq(test, var_out.\some_other_scope__some_other_inner_bool\.value, '0');



		test_start(test, "UpdateBurst");
		sync_update(clk, sync_in, sync_out, id_in, TestStore_pkg.\DEFAULT_INT8__KEY\,
			to_buffer(x"26"), TestStore_pkg.LITTLE_ENDIAN);
		sync_update(clk, sync_in, sync_out, id_in, TestStore_pkg.\DEFAULT_INT8__KEY\,
			to_buffer(x"27"), TestStore_pkg.LITTLE_ENDIAN);
		sync_update(clk, sync_in, sync_out, id_in, TestStore_pkg.\DEFAULT_INT8__KEY\,
			to_buffer(x"28"), TestStore_pkg.LITTLE_ENDIAN);
		sync_wait(clk, sync_in_busy);
		test_expect_eq(test, var_out.\default_int8\.value, 16#28#);



		test_start(test, "UpdateOut");
		var_in.\default_int8\.value <= x"45";
		var_in.\default_int8\.we <= '1';
		wait until rising_edge(clk);
		var_in.\default_int8\.we <= '0';

		sync_accept_update_start(clk, sync_in, sync_out, id_out, TestStore_pkg.LITTLE_ENDIAN);
		sync_accept_update_var(clk, sync_in, sync_out, key, buf, last, TestStore_pkg.LITTLE_ENDIAN);
		test_expect_eq(test, key, TestStore_pkg.\DEFAULT_INT8__KEY\);
		test_expect_eq(test, buf(0), x"45");
		test_expect_true(test, last);



		test_finish(test);

		wait for 1 us;
		done <= true;
		wait;
	end process;

end behav;
