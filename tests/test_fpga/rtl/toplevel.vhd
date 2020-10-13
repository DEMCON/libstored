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

	-- default int8
	signal \default_int8__out\ : TestStore_pkg.\default_int8__type\;
	signal \default_int8__out_changed\ : std_logic;
	signal \default_int8__in\ : TestStore_pkg.\default_int8__type\;
	signal \default_int8__in_we\ : std_logic;

	-- default int16
	signal \default_int16__out\ : TestStore_pkg.\default_int16__type\;
	signal \default_int16__out_changed\ : std_logic;
	signal \default_int16__in\ : TestStore_pkg.\default_int16__type\;
	signal \default_int16__in_we\ : std_logic;

	-- default int32
	signal \default_int32__out\ : TestStore_pkg.\default_int32__type\;
	signal \default_int32__out_changed\ : std_logic;
	signal \default_int32__in\ : TestStore_pkg.\default_int32__type\;
	signal \default_int32__in_we\ : std_logic;

	-- default int64
	signal \default_int64__out\ : TestStore_pkg.\default_int64__type\;
	signal \default_int64__out_changed\ : std_logic;
	signal \default_int64__in\ : TestStore_pkg.\default_int64__type\;
	signal \default_int64__in_we\ : std_logic;

	-- init decimal
	signal \init_decimal__out\ : TestStore_pkg.\init_decimal__type\;
	signal \init_decimal__out_changed\ : std_logic;
	signal \init_decimal__in\ : TestStore_pkg.\init_decimal__type\;
	signal \init_decimal__in_we\ : std_logic;

	-- array bool[0]
	signal \array_bool_0__out\ : TestStore_pkg.\array_bool_0__type\;
	signal \array_bool_0__out_changed\ : std_logic;
	signal \array_bool_0__in\ : TestStore_pkg.\array_bool_0__type\;
	signal \array_bool_0__in_we\ : std_logic;

	-- array string[0]
	signal \array_string_0__out\ : TestStore_pkg.\array_string_0__type\;
	signal \array_string_0__out_changed\ : std_logic;
	signal \array_string_0__in\ : TestStore_pkg.\array_string_0__type\;
	signal \array_string_0__in_we\ : std_logic;

	-- scope/inner int
	signal \scope__inner_int__out\ : TestStore_pkg.\scope__inner_int__type\;
	signal \scope__inner_int__out_changed\ : std_logic;
	signal \scope__inner_int__in\ : TestStore_pkg.\scope__inner_int__type\;
	signal \scope__inner_int__in_we\ : std_logic;

	signal axi_m2s : axi_m2s_t;
	signal axi_s2m : axi_s2m_t;
	signal sync_in, sync_out : msg_t;
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

			-- default int8
			\default_int8__out\ => \default_int8__out\,
			\default_int8__out_changed\ => \default_int8__out_changed\,
			\default_int8__in\ => \default_int8__in\,
			\default_int8__in_we\ => \default_int8__in_we\,

			-- default int16
			\default_int16__out\ => \default_int16__out\,
			\default_int16__out_changed\ => \default_int16__out_changed\,
			\default_int16__in\ => \default_int16__in\,
			\default_int16__in_we\ => \default_int16__in_we\,

			-- default int32
			\default_int32__out\ => \default_int32__out\,
			\default_int32__out_changed\ => \default_int32__out_changed\,
			\default_int32__in\ => \default_int32__in\,
			\default_int32__in_we\ => \default_int32__in_we\,

			-- default int64
			\default_int64__out\ => \default_int64__out\,
			\default_int64__out_changed\ => \default_int64__out_changed\,
			\default_int64__in\ => \default_int64__in\,
			\default_int64__in_we\ => \default_int64__in_we\,

			-- init decimal
			\init_decimal__out\ => \init_decimal__out\,
			\init_decimal__out_changed\ => \init_decimal__out_changed\,
			\init_decimal__in\ => \init_decimal__in\,
			\init_decimal__in_we\ => \init_decimal__in_we\,

			-- array bool[0]
			\array_bool_0__out\ => \array_bool_0__out\,
			\array_bool_0__out_changed\ => \array_bool_0__out_changed\,
			\array_bool_0__in\ => \array_bool_0__in\,
			\array_bool_0__in_we\ => \array_bool_0__in_we\,

			-- array string[0]
			\array_string_0__out\ => \array_string_0__out\,
			\array_string_0__out_changed\ => \array_string_0__out_changed\,
			\array_string_0__in\ => \array_string_0__in\,
			\array_string_0__in_we\ => \array_string_0__in_we\,

			-- scope/inner int
			\scope__inner_int__out\ => \scope__inner_int__out\,
			\scope__inner_int__out_changed\ => \scope__inner_int__out_changed\,
			\scope__inner_int__in\ => \scope__inner_int__in\,
			\scope__inner_int__in_we\ => \scope__inner_int__in_we\,

			sync_in => sync_in,
			sync_out => sync_out,

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
	begin
		\default_int8__in_we\ <= '0';
		\default_int16__in_we\ <= '0';
		\default_int32__in_we\ <= '0';
		\default_int64__in_we\ <= '0';
		\init_decimal__in_we\ <= '0';
		\array_bool_0__in_we\ <= '0';
		\array_string_0__in_we\ <= '0';
		\scope__inner_int__in_we\ <= '0';

		axi_init(axi_m2s, axi_s2m);
		sync_init(sync_in, sync_out);

		wait until rising_edge(clk) and rstn = '1';
		wait until rising_edge(clk);
		wait until rising_edge(clk);

		test_init(test);
		test_verbose(test);

		test_start(test, "Initial");
		test_expect_eq(test, \default_int8__out\, 0);
		test_expect_eq(test, \default_int16__out\, 0);
		test_expect_eq(test, \default_int32__out\, 0);
		test_expect_eq(test, \init_decimal__out\, 42);
		test_expect_eq(test, \array_bool_0__out\, '1');
		test_expect_eq(test, \array_string_0__out\, x"00000000");

		test_start(test, "Set");
		\default_int8__in\ <= x"12";
		\default_int8__in_we\ <= '1';
		wait until rising_edge(clk);
		\default_int8__in_we\ <= '0';
		wait until rising_edge(clk) and \default_int8__out_changed\ = '1';
		test_expect_eq(test, \default_int8__out\, 18);

		test_start(test, "AXI");
		axi_read(clk, axi_m2s, axi_s2m, TestStore_pkg.\DEFAULT_INT8__ADDR\, data);
		test_expect_eq(test, data, x"00000012");

		axi_write(clk, axi_m2s, axi_s2m, TestStore_pkg.\DEFAULT_INT16__ADDR\, x"abcd1122");
		test_expect_eq(test, \default_int16__out\, 16#1122#);

		test_start(test, "Hello");
		sync_accept_hello(clk, sync_in, sync_out, TestStore_pkg.HASH, id_in, TestStore_pkg.LITTLE_ENDIAN);
		sync_welcome(clk, sync_in, sync_out, id_in, x"aabb",
			(0 to TestStore_pkg.BUFFER_LENGTH - 1 => x"EF"),
--			(x"12", x"34", x"56", x"78", x"9a", x"bc", x"de", x"f0", x"11"),
			TestStore_pkg.LITTLE_ENDIAN);

		test_finish(test);
		wait for 1 us;
		done <= true;
		wait;
	end process;

end behav;
