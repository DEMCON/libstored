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

--pragma translate_off
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

use work.libstored_pkg;

package libstored_tb_pkg is

	type str_t is array(natural range <>) of character;
	constant STR_NULL : character := del;

	procedure str_set(variable a: inout str_t; constant b : string);
	function "&"(constant a: str_t; constant b : string) return str_t;
	function to_string(constant s : str_t) return string;

	type test_t is record
		suite : str_t(0 to 63);
		test : str_t(0 to 63);
		test_start : time;
		running : boolean;
		verbose : boolean;
		checks : natural;
		errors : natural;
		test_errors : natural;
	end record;

	procedure test_init(variable test : inout test_t; constant name : string := "Test");
	procedure test_start(variable test : inout test_t; constant name : string);
	procedure test_end(variable test : inout test_t);
	procedure test_finish(variable test : inout test_t);

	procedure test_verbose(variable test : inout test_t; constant verbose : boolean := true);

	procedure test_check(variable test : inout test_t; constant msg : in string; constant ref : string := "");
	procedure test_error(variable test : inout test_t; constant msg : in string; constant ref : string := "");

	procedure test_expect_true(variable test : inout test_t; constant x : in boolean; constant ref : string := "");
	procedure test_expect_false(variable test : inout test_t; constant x : in boolean; constant ref : string := "");
	procedure test_expect_eq(variable test : inout test_t; constant x : in unsigned; constant expect : in natural; constant ref : string := "");
	procedure test_expect_eq(variable test : inout test_t; constant x: in signed; constant expect : in natural; constant ref : string := "");
	procedure test_expect_eq(variable test : inout test_t; constant x, expect : in std_logic; constant ref : string := "");
	procedure test_expect_eq(variable test : inout test_t; constant x, expect : in std_logic_vector; constant ref : string := "");

	function test_string(constant x : boolean) return string;
	function test_string(constant x : unsigned) return string;
	function test_string(constant x : signed) return string;
	function test_string(constant x : std_logic) return string;
	function test_string(constant x : std_logic_vector) return string;
	function test_string(constant x : str_t) return string;

	function normalize(constant x : std_logic_vector) return std_logic_vector;
	function normalize(constant x : str_t) return str_t;
	function normalize(constant x : string) return string;

	-- AXI4 LITE master to slave signals
	type axi_m2s_t is record
		araddr : std_logic_vector(31 downto 0);
		arvalid : std_logic;
		awaddr : std_logic_vector(31 downto 0);
		awvalid : std_logic;
		bready : std_logic;
		rready : std_logic;
		wdata : std_logic_vector(31 downto 0);
		wvalid : std_logic;
	end record;

	-- AXI4 LITE slave to master signals
	type axi_s2m_t is record
		arready : std_logic;
		awready : std_logic;
		bresp : std_logic_vector(1 downto 0);
		bvalid : std_logic;
		rdata : std_logic_vector(31 downto 0);
		rresp : std_logic_vector(1 downto 0);
		rvalid : std_logic;
		wready : std_logic;
	end record;

	procedure axi_init(signal m2s : inout axi_m2s_t; signal s2m : in axi_s2m_t);
	procedure axi_write(signal clk : in std_logic; signal m2s : inout axi_m2s_t; signal s2m : in axi_s2m_t;
		constant addr : in natural; constant data : in std_logic_vector(31 downto 0);
		constant timeout : in time := 1 ms);
	procedure axi_read(signal clk : std_logic; signal m2s : inout axi_m2s_t; signal s2m : in axi_s2m_t;
		constant addr : in natural; variable data : out std_logic_vector(31 downto 0);
		constant timeout : in time := 1 ms);

end libstored_tb_pkg;

package body libstored_tb_pkg is

	procedure str_set(variable a: inout str_t; constant b : string) is
		variable v : str_t(0 to a'length - 1);
		variable bn : string(1 to b'length);
	begin
		bn := normalize(b);
		v := (v'range => STR_NULL);

		for i in 0 to libstored_pkg.minimum(a'length, b'length) - 1 loop
			v(i) := bn(i + 1);
		end loop;

		a := v;
	end procedure;

	function "&"(constant a: str_t; constant b : string) return str_t is
		variable index : natural;
		variable v : str_t(0 to a'length - 1);
		variable bn : string(1 to b'length);
	begin
		v := normalize(a);
		bn := normalize(b);

		index := v'length;
		for i in v'reverse_range loop
			if v(i) = STR_NULL then
				index := i;
			end if;
		end loop;
		if index < v'length then
			for i in 0 to libstored_pkg.minimum(v'high - index, bn'length - 1) loop
				v(index + i) := bn(i + 1);
			end loop;
		end if;
		return v;
	end function;

	function to_string(constant s : str_t) return string is
		variable n : natural := 0;
		variable v : str_t(0 to s'length - 1);
		variable res : string(1 to s'length);
	begin
		v := normalize(s);
		for i in v'range loop
			if v(i) = STR_NULL then
				return res(1 to i);
			end if;

			res(i + 1) := v(i);
		end loop;
		return res;
	end function;

	procedure test_init(variable test : inout test_t; constant name : string := "Test") is
	begin
		test.checks := 0;
		test.errors := 0;
		str_set(test.suite, name);
		str_set(test.test, "?");
		test.running := false;
		test.verbose := false;
		std.textio.write(std.textio.output, "[==========] Initialize test suite." & lf);
		std.textio.write(std.textio.output, "[----------] Global test environment set-up." & lf);
		std.textio.write(std.textio.output, "[----------] Running tests from " & to_string(test.suite) & lf);
	end procedure;

	procedure test_start(variable test : inout test_t; constant name : string) is
	begin
		test_end(test);
		str_set(test.test, name);
		test.running := true;
		test.test_errors := 0;
		test.test_start := now;
		std.textio.write(std.textio.output, "[ RUN      ] " & to_string(test.suite) & "." & to_string(test.test) & lf);
	end procedure;

	procedure test_end(variable test : inout test_t) is
	begin
		if not test.running then
			return;
		end if;

		if test.test_errors = 0 then
			std.textio.write(std.textio.output, "[       OK ] " & to_string(test.suite) & "." & to_string(test.test) &
				" (" & time'image(now - test.test_start) & ")" & lf);
		else
			std.textio.write(std.textio.output, "[  FAILED  ] " & to_string(test.suite) & "." & to_string(test.test) &
				" (" & time'image(now - test.test_start) & ")" & lf);
		end if;

		str_set(test.test, "?");
		test.running := false;
	end procedure;

	procedure test_finish(variable test : inout test_t) is
	begin
		test_end(test);
		std.textio.write(std.textio.output, "[----------] Global test environment tear-down." & lf);
		std.textio.write(std.textio.output, "[==========] Finished test suite. " & lf);

		if test.errors = 0 then
			std.textio.write(std.textio.output, "[  PASSED  ] " & integer'image(test.checks) & " checks" & lf);
		else
			std.textio.write(std.textio.output, "[  FAILED  ] " & integer'image(test.checks) & " checks, " &
				integer'image(test.errors) & " errors" & lf);
		end if;
	end procedure;

	procedure test_verbose(variable test : inout test_t; constant verbose : boolean := true) is
	begin
		test.verbose := verbose;
	end procedure;

	procedure test_log(variable test : inout test_t; constant msg : in string; constant ref : string := "") is
		variable prefix : str_t(0 to ref'length + 3 - 1);
	begin
		str_set(prefix, "");

		if test.verbose then
			if ref /= "" then
				str_set(prefix, "(" & ref & ") ");
			end if;

			std.textio.write(std.textio.output, test_string(prefix) & msg & lf);
		end if;
	end procedure;

	procedure test_check(variable test : inout test_t; constant msg : in string; constant ref : string := "") is
	begin
		if not test.running then
			test_start(test, "?");
		end if;

		test.checks := test.checks + 1;
		test_log(test, msg, ref);
	end procedure;

	procedure test_error(variable test : inout test_t; constant msg : in string; constant ref : string := "") is
		variable prefix : str_t(0 to ref'length + 3 - 1);
	begin
		str_set(prefix, "");
		if ref /= "" then
			str_set(prefix, "(" & ref & ") ");
		end if;

		test.errors := test.errors + 1;
		test.test_errors := test.test_errors + 1;
		report to_string(prefix) & msg severity error;
	end procedure;

	procedure test_expect_true(variable test : inout test_t; constant x : in boolean; constant ref : string := "") is
	begin
		test_check(test, "Check " & test_string(x) & " = " & test_string(true), ref);

		if not x then
			test_error(test, "Got " & test_string(x) & ", expected " & test_string(true), ref);
		end if;
	end procedure;

	procedure test_expect_false(variable test : inout test_t; constant x : in boolean; constant ref : string := "") is
	begin
		test_check(test, "Check " & test_string(x) & " = " & test_string(false), ref);

		if x then
			test_error(test, "Got " & test_string(x) & ", expected " & test_string(false), ref);
		end if;
	end procedure;

	procedure test_expect_eq(variable test : inout test_t; constant x : in unsigned; constant expect : in natural; constant ref : string := "") is
	begin
		test_expect_eq(test, std_logic_vector(x), std_logic_vector(to_unsigned(expect, x'length)), ref);
	end procedure;

	procedure test_expect_eq(variable test : inout test_t; constant x : in signed; constant expect : in integer; constant ref : string := "") is
	begin
		test_expect_eq(test, std_logic_vector(x), std_logic_vector(to_signed(expect, x'length)), ref);
	end procedure;

	procedure test_expect_eq(variable test : inout test_t; constant x, expect : in std_logic; constant ref : string := "") is
	begin
		test_check(test, "Check " & test_string(x) & " = " & test_string(expect), ref);

		if expect /= '-' and x /= expect then
			test_error(test, "Got " & test_string(x) & ", expected " & test_string(expect), ref);
		end if;
	end procedure;

	procedure test_expect_eq(variable test : inout test_t; constant x, expect : in std_logic_vector; constant ref : string := "") is
		variable xn : std_logic_vector(x'length - 1 downto 0);
		variable en : std_logic_vector(expect'length - 1 downto 0);
	begin
		test_check(test, "Check " & test_string(x) & " = " & test_string(expect), ref);

		xn := normalize(x);
		en := normalize(expect);

		if x'length /= expect'length then
			test_error(test, "Lengths do not match: got " & integer'image(x'length) &
				", expected " & integer'image(expect'length));
		else
			for i in xn'range loop
				if en(i) /= '-' and xn(i) /= en(i) then
					test_error(test, "Got " & test_string(x) & ", expected " & test_string(expect), ref);
					return;
				end if;
			end loop;
		end if;
	end procedure;

	function test_string(constant x : boolean) return string is
	begin
		return boolean'image(x);
	end function;

	function test_string(constant x : unsigned) return string is
	begin
		return test_string(std_logic_vector(x));
	end function;

	function test_string(constant x : signed) return string is
	begin
		return test_string(std_logic_vector(x));
	end function;

	function test_string(constant x : std_logic) return string is
	begin
		return std_logic'image(x);
	end function;

	function test_string(constant x : std_logic_vector) return string is
		variable bits : boolean := false;
		variable res : str_t(1 to 256);
		variable v : std_logic_vector(libstored_pkg.minimum(res'length - 4, x'length) - 1 downto 0);
	begin
		str_set(res, "");

		if x'ascending then
			for i in 0 to libstored_pkg.minimum(v'length, x'length) - 1 loop
				v(v'high - i) := x(x'low + i);
			end loop;
		else
			for i in libstored_pkg.minimum(v'length, x'length) - 1 downto 0 loop
				v(i) := x(x'length - v'length + i);
			end loop;
		end if;

		if v'length mod 4 /= 0 then
			bits := true;
		else
			for i in v'length / 4 - 1 downto 0 loop
				case v(i * 4 + 3 downto i * 4) is
				when "0000" => null;
				when "0001" => null;
				when "0010" => null;
				when "0011" => null;
				when "0100" => null;
				when "0101" => null;
				when "0110" => null;
				when "0111" => null;
				when "1000" => null;
				when "1001" => null;
				when "1010" => null;
				when "1011" => null;
				when "1100" => null;
				when "1101" => null;
				when "1110" => null;
				when "1111" => null;
				when "----" => null;
				when "LLLL" => null;
				when "HHHH" => null;
				when "WWWW" => null;
				when "ZZZZ" => null;
				when "UUUU" => null;
				when "XXXX" => null;
				when others => bits := true;
				end case;
			end loop;
		end if;

		if bits then
			str_set(res, "");
			for i in v'range loop
				case v(i) is
				when '0' => res := res & "0";
				when '1' => res := res & "1";
				when 'L' => res := res & "L";
				when 'H' => res := res & "H";
				when 'W' => res := res & "W";
				when 'Z' => res := res & "Z";
				when '-' => res := res & "-";
				when 'U' => res := res & "U";
				when 'X' => res := res & "X";
				end case;
			end loop;
		else
			str_set(res, "0x");

			for i in v'length / 4 - 1 downto 0 loop
				case v(i * 4 + 3 downto i * 4) is
				when "0000" => res := res & "0";
				when "0001" => res := res & "1";
				when "0010" => res := res & "2";
				when "0011" => res := res & "3";
				when "0100" => res := res & "4";
				when "0101" => res := res & "5";
				when "0110" => res := res & "6";
				when "0111" => res := res & "7";
				when "1000" => res := res & "8";
				when "1001" => res := res & "9";
				when "1010" => res := res & "a";
				when "1011" => res := res & "b";
				when "1100" => res := res & "c";
				when "1101" => res := res & "d";
				when "1110" => res := res & "e";
				when "1111" => res := res & "f";
				when "----" => res := res & "-";
				when "LLLL" => res := res & "L";
				when "HHHH" => res := res & "H";
				when "WWWW" => res := res & "W";
				when "ZZZZ" => res := res & "Z";
				when "UUUU" => res := res & "U";
				when "XXXX" => res := res & "X";
				when others => res := res & "?";
				end case;
			end loop;
		end if;

		if x'length > v'length then
			res := res & "...";
		end if;

		return to_string(res);
	end function;

	function test_string(constant x : str_t) return string is
	begin
		return to_string(x);
	end function;

	function normalize(constant x : std_logic_vector) return std_logic_vector is
		variable v : std_logic_vector(x'length - 1 downto 0);
	begin
		if x'ascending then
			for i in v'range loop
				v(i) := x(x'high - i);
			end loop;
		else
			for i in v'range loop
				v(i) := x(x'low + i);
			end loop;
		end if;

		return v;
	end function;

	function normalize(constant x : str_t) return str_t is
		variable v : str_t(0 to x'length - 1);
	begin
		if not x'ascending then
			for i in v'range loop
				v(i) := x(x'high - i);
			end loop;
		else
			for i in v'range loop
				v(i) := x(x'low + i);
			end loop;
		end if;

		return v;
	end function;

	function normalize(constant x : string) return string is
		variable v : string(1 to x'length);
	begin
		if not x'ascending then
			for i in v'range loop
				v(i) := x(x'high - i - 1);
			end loop;
		else
			for i in v'range loop
				v(i) := x(x'low + i - 1);
			end loop;
		end if;

		return v;
	end function;

	procedure axi_init(signal m2s : inout axi_m2s_t; signal s2m : in axi_s2m_t) is
	begin
		m2s.arvalid <= '0';
		m2s.awvalid <= '0';
		m2s.bready <= '0';
		m2s.wvalid <= '0';
		m2s.rready <= '0';
	end procedure;

	procedure axi_write(signal clk : in std_logic; signal m2s : inout axi_m2s_t; signal s2m : in axi_s2m_t;
		constant addr : in natural; constant data : in std_logic_vector(31 downto 0);
		constant timeout : in time := 1 ms)
	is
		variable deadline : time;
	begin
		deadline := now + timeout;

		m2s.awaddr <= std_logic_vector(to_unsigned(addr, 32));
		m2s.awvalid <= '1';
		wait until rising_edge(clk) and s2m.awready = '1' for deadline - now;
		assert s2m.awready = '1' report "Timeout" severity failure;
		m2s.awvalid <= '0';
		m2s.awaddr <= (others => '-');

		m2s.wdata <= data;
		m2s.wvalid <= '1';
		wait until rising_edge(clk) and s2m.wready = '1' for deadline - now;
		assert s2m.wready = '1' report "Timeout" severity failure;
		m2s.wvalid <= '0';
		m2s.wdata <= (others => '-');

		m2s.bready <= '1';
		wait until rising_edge(clk) and s2m.bvalid = '1' for deadline - now;
		assert s2m.bvalid = '1' report "Timeout" severity failure;
		assert s2m.bresp = "00" report "AXI error response received" severity error;
		m2s.bready <= '0';
	end procedure;

	procedure axi_read(signal clk : in std_logic; signal m2s : inout axi_m2s_t; signal s2m : in axi_s2m_t;
		constant addr : in natural; variable data : out std_logic_vector(31 downto 0);
		constant timeout : in time := 1 ms)
	is
		variable deadline : time;
	begin
		deadline := now + timeout;

		m2s.araddr <= std_logic_vector(to_unsigned(addr, 32));
		m2s.arvalid <= '1';
		wait until rising_edge(clk) and s2m.arready = '1' for deadline - now;
		assert s2m.arready = '1' report "Timeout" severity failure;
		m2s.arvalid <= '0';
		m2s.araddr <= (others => '-');

		m2s.rready <= '1';
		wait until rising_edge(clk) and s2m.rvalid = '1' for deadline - now;
		assert s2m.rvalid = '1' report "Timeout" severity failure;
		assert s2m.rresp = "00" report "AXI error response received" severity error;
		data := s2m.rdata;
		m2s.rready <= '0';
	end procedure;

end package body;
--pragma translate_on

