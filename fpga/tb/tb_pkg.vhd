-- SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
--
-- SPDX-License-Identifier: MPL-2.0

--pragma translate_off
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

use work.libstored_pkg;

package libstored_tb_pkg is

	-----------------------------------------------
	-- Support types
	-----------------------------------------------

	type str_t is array(natural range <>) of character;
	constant STR_NULL : character := del;

	procedure str_set(variable a: inout str_t; constant b : string);
	function "&"(constant a: str_t; constant b : string) return str_t;
	function to_string(constant s : str_t) return string;
	function to_str(constant s : string; constant len : natural := 255) return str_t;

	type buffer_t is array(natural range <>) of std_logic_vector(7 downto 0);

	function to_buffer(constant x : std_logic) return buffer_t;
	function to_buffer(constant x : std_logic_vector) return buffer_t;
	function to_buffer(constant x : integer; constant width : positive; constant littleEndian : boolean := true) return buffer_t;
	function to_buffer(constant x : buffer_t) return buffer_t;
	function string_encode(constant x : string) return buffer_t;
	function str_encode(constant x : str_t) return buffer_t;



	-----------------------------------------------
	-- Test functions
	-----------------------------------------------

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
	procedure test_expect_eq(variable test : inout test_t; constant x : in integer; constant expect : in integer; constant ref : string := "");
	procedure test_expect_eq(variable test : inout test_t; constant x : in unsigned; constant expect : in natural; constant ref : string := "");
	procedure test_expect_eq(variable test : inout test_t; constant x: in signed; constant expect : in integer; constant ref : string := "");
	procedure test_expect_eq(variable test : inout test_t; constant x, expect : in std_logic; constant ref : string := "");
	procedure test_expect_eq(variable test : inout test_t; constant x, expect : in std_logic_vector; constant ref : string := "");
	procedure test_expect_eq(variable test : inout test_t;
		signal clk : in std_logic;
		signal msg_in : in libstored_pkg.msg_t;
		signal msg_out : inout libstored_pkg.msg_t;
		constant expect : buffer_t;
		constant ref : string := "";
		constant timeout : time := 1 ms);

	function test_string(constant x : boolean) return string;
	function test_string(constant x : unsigned) return string;
	function test_string(constant x : signed) return string;
	function test_string(constant x : std_logic) return string;
	function test_string(constant x : std_logic_vector) return string;
	function test_string(constant x : str_t) return string;

	function normalize(constant x : std_logic_vector) return std_logic_vector;
	function normalize(constant x : str_t) return str_t;
	function normalize(constant x : string) return string;



	-----------------------------------------------
	-- Stream test functions
	-----------------------------------------------

	procedure msg_write(signal clk : in std_logic;
		signal msg_in : in libstored_pkg.msg_t; signal msg_out : inout libstored_pkg.msg_t;
		constant buf : buffer_t;
		constant last : boolean := true;
		constant timeout : time := 1 ms);

	procedure msg_read(signal clk : in std_logic;
		signal msg_in : in libstored_pkg.msg_t; signal msg_out : inout libstored_pkg.msg_t;
		variable buf : out buffer_t;
		variable len : out natural;
		variable last : out boolean;
		constant max_len : natural := 0;
		constant read_till_last : in boolean := true;
		constant timeout : time := 1 ms);

	-----------------------------------------------
	-- AXI test functions
	-----------------------------------------------

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
	procedure axi_read(signal clk : in std_logic; signal m2s : inout axi_m2s_t; signal s2m : in axi_s2m_t;
		constant addr : in natural; variable data : out std_logic_vector(31 downto 0);
		constant timeout : in time := 1 ms);



	-----------------------------------------------
	-- Store sync test functions
	-----------------------------------------------

	procedure sync_init(signal sync_in : inout libstored_pkg.msg_t; signal sync_out : in libstored_pkg.msg_t);

	procedure sync_accept_hello(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		constant hash : libstored_pkg.hash_t; variable id_in : out std_logic_vector(15 downto 0);
		constant littleEndian : boolean := true; constant timeout : time := 1 ms);

	procedure sync_welcome(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		constant id_in : std_logic_vector(15 downto 0);
		constant id_out : std_logic_vector(15 downto 0);
		constant buf : buffer_t;
		constant littleEndian : boolean := true;
		constant timeout : in time := 1 ms);

	procedure sync_update_start(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		constant id_in : std_logic_vector(15 downto 0);
		constant littleEndian : boolean := true;
		constant timeout : in time := 1 ms);

	procedure sync_update_var(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		constant key : std_logic_vector;
		constant buf : buffer_t;
		constant last : boolean;
		constant littleEndian : boolean := true;
		constant timeout : in time := 1 ms);

	procedure sync_update(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		constant id_in : std_logic_vector(15 downto 0);
		constant key : std_logic_vector;
		constant buf : buffer_t;
		constant littleEndian : boolean := true;
		constant timeout : in time := 1 ms);

	procedure sync_wait(signal clk : in std_logic; signal busy : in std_logic;
		constant timeout : in time := 1 ms);

	procedure sync_accept_update_start(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		constant id_out : std_logic_vector(15 downto 0);
		constant littleEndian : boolean := true;
		constant timeout : in time := 1 ms);

	procedure sync_accept_update_var(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		variable key : out std_logic_vector;
		variable buf : out buffer_t;
		variable last : out boolean;
		constant littleEndian : boolean := true;
		constant timeout : in time := 1 ms);

	procedure sync_accept_update_vars(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		constant timeout : in time := 1 ms);

	procedure sync_accept_update(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		constant id_out : std_logic_vector(15 downto 0);
		constant littleEndian : boolean := true;
		constant timeout : in time := 1 ms);



	-----------------------------------------------
	-- UART test functions
	-----------------------------------------------

	procedure uart_do_tx(constant baud : integer;
		signal tx : in std_logic;
		variable buf : out buffer_t;
		constant len : positive := 1;
		constant timeout : time := 1 ms);

	procedure uart_do_rx(constant baud : integer;
		signal rx : out std_logic;
		signal rts : in std_logic;
		constant buf : buffer_t;
		constant timeout : time := 1 ms);



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

	function to_str(constant s : string; constant len : natural := 255) return str_t is
		variable res : str_t(0 to len - 1);
	begin
		str_set(res, s);
		return res;
	end function;

	function to_buffer(constant x : std_logic) return buffer_t is
	begin
		case x is
		when '0' => return to_buffer(x"00");
		when '1' => return to_buffer(x"01");
		when others => return to_buffer("XXXXXXXX");
		end case;
	end function;

	function to_buffer(constant x : std_logic_vector) return buffer_t is
		variable buf : buffer_t(0 to x'length / 8 - 1);
		variable n : std_logic_vector(x'length - 1 downto 0);
	begin
		n := normalize(x);
		for i in buf'range loop
			buf(i) := n(n'high - i * 8 downto n'high - i * 8 - 7);
		end loop;
		return buf;
	end function;

	function to_buffer(constant x : integer; constant width : positive; constant littleEndian : boolean := true) return buffer_t is
		variable res, v : std_logic_vector(width * 8 - 1 downto 0);
	begin
		v := std_logic_vector(to_signed(x, width * 8));
		if littleEndian then
			res := libstored_pkg.swap_endian(v);
		else
			res := v;
		end if;
		return to_buffer(res);
	end function;

	function to_buffer(constant x : buffer_t) return buffer_t is
	begin
		return x;
	end function;

	function string_encode(constant x : string) return buffer_t is
		variable res : buffer_t(0 to x'length - 1);
		variable s : string(1 to x'length);
	begin
		s := normalize(x);
		for i in s'range loop
			res(i - 1) := std_logic_vector(to_unsigned(character'pos(s(i)), 8));
		end loop;
		return res;
	end function;

	function str_encode(constant x : str_t) return buffer_t is
	begin
		return string_encode(to_string(x));
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
		if test.verbose then
			report "Start test " & to_string(test.test) severity note;
		end if;
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

			report "FAILED" severity failure;
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

			report "FAILED" severity failure;
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

	procedure test_expect_eq(variable test : inout test_t; constant x : in integer; constant expect : in integer; constant ref : string := "") is
	begin
		test_expect_eq(test, to_signed(x, 32), expect, ref);
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

	procedure test_expect_eq(variable test : inout test_t;
		signal clk : in std_logic;
		signal msg_in : in libstored_pkg.msg_t;
		signal msg_out : inout libstored_pkg.msg_t;
		constant expect : buffer_t;
		constant ref : string := "";
		constant timeout : time := 1 ms)
	is
		variable deadline : time;
	begin
		deadline := now + timeout;

		msg_out.valid <= '0';
		msg_out.accept <= '1';
		for i in expect'range loop
			wait until rising_edge(clk) and msg_in.valid = '1' for deadline - now;
			assert msg_in.valid = '1' report "Timeout" severity failure;
			test_expect_eq(test, msg_in.data, expect(i), ref);
		end loop;
		msg_out.accept <= '0';
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
				when '0' => res := res & to_string("0");
				when '1' => res := res & to_string("1");
				when 'L' => res := res & to_string("L");
				when 'H' => res := res & to_string("H");
				when 'W' => res := res & to_string("W");
				when 'Z' => res := res & to_string("Z");
				when '-' => res := res & to_string("-");
				when 'U' => res := res & to_string("U");
				when 'X' => res := res & to_string("X");
				end case;
			end loop;
		else
			str_set(res, "0x");

			for i in v'length / 4 - 1 downto 0 loop
				case v(i * 4 + 3 downto i * 4) is
				when "0000" => res := res & to_string("0");
				when "0001" => res := res & to_string("1");
				when "0010" => res := res & to_string("2");
				when "0011" => res := res & to_string("3");
				when "0100" => res := res & to_string("4");
				when "0101" => res := res & to_string("5");
				when "0110" => res := res & to_string("6");
				when "0111" => res := res & to_string("7");
				when "1000" => res := res & to_string("8");
				when "1001" => res := res & to_string("9");
				when "1010" => res := res & to_string("a");
				when "1011" => res := res & to_string("b");
				when "1100" => res := res & to_string("c");
				when "1101" => res := res & to_string("d");
				when "1110" => res := res & to_string("e");
				when "1111" => res := res & to_string("f");
				when "----" => res := res & to_string("-");
				when "LLLL" => res := res & to_string("L");
				when "HHHH" => res := res & to_string("H");
				when "WWWW" => res := res & to_string("W");
				when "ZZZZ" => res := res & to_string("Z");
				when "UUUU" => res := res & to_string("U");
				when "XXXX" => res := res & to_string("X");
				when others => res := res & to_string("?");
				end case;
			end loop;
		end if;

		if x'length > v'length then
			res := res & to_string("...");
		end if;

		return to_string(res);
	end function;

	function test_string(constant x : str_t) return string is
	begin
		return to_string(x);
	end function;

	function normalize(constant x : std_logic_vector) return std_logic_vector is
	begin
		return libstored_pkg.normalize(x);
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

	procedure msg_write(signal clk : in std_logic;
		signal msg_in : in libstored_pkg.msg_t; signal msg_out : inout libstored_pkg.msg_t;
		constant buf : buffer_t;
		constant last : boolean := true;
		constant timeout : time := 1 ms)
	is
		variable deadline : time;
	begin
		deadline := now + timeout;
		msg_out.accept <= '0';
		for i in buf'range loop
			if last and i = buf'right then
				msg_out.last <= '1';
			else
				msg_out.last <= '0';
			end if;
			msg_out.data <= buf(i);
			msg_out.valid <= '1';
			wait until rising_edge(clk) and msg_in.accept = '1' for deadline - now;
			assert msg_in.accept = '1' report "Timeout" severity failure;
		end loop;
		msg_out.valid <= '0';
		msg_out.data <= (others => '-');
		msg_out.last <= '-';
	end procedure;

	procedure msg_read(signal clk : in std_logic;
		signal msg_in : in libstored_pkg.msg_t; signal msg_out : inout libstored_pkg.msg_t;
		variable buf : out buffer_t;
		variable len : out natural;
		variable last : out boolean;
		constant max_len : natural := 0;
		constant read_till_last : in boolean := true;
		constant timeout : time := 1 ms)
	is
		variable deadline : time;
		variable l : natural range 0 to buf'length - 1;
	begin
		deadline := now + timeout;
		l := 0;
		msg_out.valid <= '0';
		last := false;
		for i in buf'range loop
			msg_out.accept <= '1';
			wait until rising_edge(clk) and msg_in.valid = '1' for deadline - now;
			assert msg_in.valid = '1' report "Timeout" severity failure;
			buf(i) := msg_in.data;
			l := l + 1;
			last := msg_in.last = '1';
			if max_len > 0 and max_len = l then
				exit;
			end if;
			if msg_in.last = '1' and read_till_last then
				exit;
			end if;
		end loop;
		msg_out.accept <= '0';
		len := l;
	end procedure;

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

	procedure sync_init(signal sync_in : inout libstored_pkg.msg_t; signal sync_out : in libstored_pkg.msg_t) is
	begin
		sync_in.valid <= '0';
		sync_in.accept <= '0';
	end procedure;

	procedure sync_accept_hello(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		constant hash : libstored_pkg.hash_t; variable id_in : out std_logic_vector(15 downto 0);
		constant littleEndian : boolean := true; constant timeout : time := 1 ms)
	is
		variable deadline : time;
		variable cmd : std_logic_vector(7 downto 0);
	begin
		deadline := now + timeout;

		if littleEndian then
			cmd := x"68";
		else
			cmd := x"48";
		end if;

		sync_in.accept <= '1';
		msg_loop: while true loop
			wait until rising_edge(clk) and sync_out.valid = '1' and sync_out.data = cmd for deadline - now;
			assert sync_out.valid = '1' and sync_out.data = cmd report "Timeout" severity failure;

			hash_loop: for i in hash'range loop
				wait until rising_edge(clk) and sync_out.valid = '1' for deadline - now;
				assert sync_out.valid = '1' report "Timeout" severity failure;
				next msg_loop when sync_out.data /= hash(i);
			end loop;

			wait until rising_edge(clk) and sync_out.valid = '1' for deadline - now;
			assert sync_out.valid = '1' report "Timeout" severity failure;
			next msg_loop when sync_out.data /= x"00";

			wait until rising_edge(clk) and sync_out.valid = '1' for deadline - now;
			assert sync_out.valid = '1' report "Timeout" severity failure;

			if littleEndian then
				id_in(7 downto 0) := sync_out.data;
			else
				id_in(15 downto 8) := sync_out.data;
			end if;

			wait until rising_edge(clk) and sync_out.valid = '1' for deadline - now;
			assert sync_out.valid = '1' report "Timeout" severity failure;

			if littleEndian then
				id_in(15 downto 8) := sync_out.data;
			else
				id_in(7 downto 0) := sync_out.data;
			end if;

			assert sync_out.last = '1' report "Corrupt message" severity warning;
			exit;
		end loop;

		sync_in.accept <= '0';
	end procedure;

	procedure sync_welcome(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		constant id_in : std_logic_vector(15 downto 0);
		constant id_out : std_logic_vector(15 downto 0);
		constant buf : buffer_t;
		constant littleEndian : boolean := true;
		constant timeout : in time := 1 ms)
	is
		variable deadline : time;
	begin
		deadline := now + timeout;

		if littleEndian then
			sync_in.data <= x"77";
		else
			sync_in.data <= x"57";
		end if;

		sync_in.last <= '0';
		sync_in.valid <= '1';
		wait until rising_edge(clk) and sync_out.accept = '1' for deadline - now;
		assert sync_out.accept = '1' report "Timeout" severity failure;

		if littleEndian then
			sync_in.data <= id_in(7 downto 0);
		else
			sync_in.data <= id_in(15 downto 8);
		end if;

		wait until rising_edge(clk) and sync_out.accept = '1' for deadline - now;
		assert sync_out.accept = '1' report "Timeout" severity failure;

		if littleEndian then
			sync_in.data <= id_in(15 downto 8);
		else
			sync_in.data <= id_in(7 downto 0);
		end if;

		wait until rising_edge(clk) and sync_out.accept = '1' for deadline - now;
		assert sync_out.accept = '1' report "Timeout" severity failure;

		if littleEndian then
			sync_in.data <= id_out(7 downto 0);
		else
			sync_in.data <= id_out(15 downto 8);
		end if;
		wait until rising_edge(clk) and sync_out.accept = '1' for deadline - now;
		assert sync_out.accept = '1' report "Timeout" severity failure;

		if littleEndian then
			sync_in.data <= id_out(15 downto 8);
		else
			sync_in.data <= id_out(7 downto 0);
		end if;
		wait until rising_edge(clk) and sync_out.accept = '1' for deadline - now;
		assert sync_out.accept = '1' report "Timeout" severity failure;

		for i in buf'range loop
			if i = buf'high then
				sync_in.last <= '1';
			end if;
			sync_in.data <= buf(i);
			wait until rising_edge(clk) and sync_out.accept = '1' for deadline - now;
			assert sync_out.accept = '1' report "Timeout" severity failure;
		end loop;

		sync_in.valid <= '0';
		sync_in.data <= (others => '-');
	end procedure;

	procedure sync_update_start(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		constant id_in : std_logic_vector(15 downto 0);
		constant littleEndian : boolean := true;
		constant timeout : in time := 1 ms)
	is
		variable deadline : time;
	begin
		deadline := now + timeout;

		sync_in.last <= '0';
		sync_in.valid <= '1';

		if littleEndian then
			sync_in.data <= x"75";
		else
			sync_in.data <= x"55";
		end if;
		wait until rising_edge(clk) and sync_out.accept = '1' for deadline - now;
		assert sync_out.accept = '1' report "Timeout" severity failure;

		if littleEndian then
			sync_in.data <= id_in(7 downto 0);
		else
			sync_in.data <= id_in(15 downto 8);
		end if;

		wait until rising_edge(clk) and sync_out.accept = '1' for deadline - now;
		assert sync_out.accept = '1' report "Timeout" severity failure;

		if littleEndian then
			sync_in.data <= id_in(15 downto 8);
		else
			sync_in.data <= id_in(7 downto 0);
		end if;

		wait until rising_edge(clk) and sync_out.accept = '1' for deadline - now;
		assert sync_out.accept = '1' report "Timeout" severity failure;

		sync_in.valid <= '0';
	end procedure;

	procedure sync_update_var(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		constant key : std_logic_vector;
		constant buf : buffer_t;
		constant last : boolean;
		constant littleEndian : boolean := true;
		constant timeout : in time := 1 ms)
	is
		variable deadline : time;
		variable keyv : std_logic_vector(key'length - 1 downto 0);
		variable size : std_logic_vector(key'length - 1 downto 0);
	begin
		deadline := now + timeout;

		sync_in.valid <= '1';

		keyv := normalize(key);
		for i in 0 to keyv'length / 8 - 1 loop
			if littleEndian then
				sync_in.data <= keyv(i * 8 + 7 downto i * 8);
			else
				sync_in.data <= keyv(keyv'high - i * 8 downto keyv'high - i * 8 - 7);
			end if;
			wait until rising_edge(clk) and sync_out.accept = '1' for deadline - now;
			assert sync_out.accept = '1' report "Timeout" severity failure;
		end loop;

		size := normalize(std_logic_vector(to_unsigned(buf'length, keyv'length)));
		for i in 0 to size'length / 8 - 1 loop
			if littleEndian then
				sync_in.data <= size(i * 8 + 7 downto i * 8);
			else
				sync_in.data <= size(size'high - i * 8 downto size'high - i * 8 - 7);
			end if;
			wait until rising_edge(clk) and sync_out.accept = '1' for deadline - now;
			assert sync_out.accept = '1' report "Timeout" severity failure;
		end loop;

		for i in buf'range loop
			if i = buf'right and last then
				sync_in.last <= '1';
			end if;
			sync_in.data <= buf(i);
			wait until rising_edge(clk) and sync_out.accept = '1' for deadline - now;
			assert sync_out.accept = '1' report "Timeout" severity failure;
		end loop;

		sync_in.valid <= '0';

		if last then
			sync_in.data <= (others => '-');
			sync_in.last <= '-';
		end if;
	end procedure;

	procedure sync_update(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		constant id_in : std_logic_vector(15 downto 0);
		constant key : std_logic_vector;
		constant buf : buffer_t;
		constant littleEndian : boolean := true;
		constant timeout : in time := 1 ms)
	is
		variable deadline : time;
	begin
		deadline := now + timeout;
		sync_update_start(clk, sync_in, sync_out, id_in, littleEndian, deadline - now);
		sync_update_var(clk, sync_in, sync_out, key, buf, true, littleEndian, deadline - now);
	end procedure;

	procedure sync_wait(signal clk : in std_logic; signal busy : in std_logic;
		constant timeout : in time := 1 ms)
	is
	begin
		wait until rising_edge(clk) and busy = '0' for timeout;
		assert busy = '0' report "Timeout" severity failure;
	end procedure;

	procedure sync_accept_update_start(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		constant id_out : std_logic_vector(15 downto 0);
		constant littleEndian : boolean := true;
		constant timeout : in time := 1 ms)
	is
		variable deadline : time;
		variable cmd : std_logic_vector(7 downto 0);
	begin
		deadline := now + timeout;

		sync_in.accept <= '1';

		if littleEndian then
			cmd := x"75";
		else
			cmd := x"55";
		end if;

		msg_loop : while true loop
			wait until rising_edge(clk) and sync_out.valid = '1' for deadline - now;
			assert sync_out.valid = '1' report "Timeout" severity failure;
			if sync_out.data /= cmd then
				next msg_loop;
			end if;

			wait until rising_edge(clk) and sync_out.valid = '1' for deadline - now;
			assert sync_out.valid = '1' report "Timeout" severity failure;
			if littleEndian and sync_out.data /= id_out(7 downto 0) then
				next msg_loop;
			elsif not littleEndian and sync_out.data /= id_out(15 downto 8) then
				next msg_loop;
			end if;

			wait until rising_edge(clk) and sync_out.valid = '1' for deadline - now;
			assert sync_out.valid = '1' report "Timeout" severity failure;
			if littleEndian and sync_out.data /= id_out(15 downto 8) then
				next msg_loop;
			elsif not littleEndian and sync_out.data /= id_out(7 downto 0) then
				next msg_loop;
			end if;

			assert sync_out.last = '0' report "Corrupt message" severity failure;
			exit;
		end loop;

		sync_in.accept <= '0';
	end procedure;

	procedure sync_accept_update_var(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		variable key : out std_logic_vector;
		variable buf : out buffer_t;
		variable last : out boolean;
		constant littleEndian : boolean := true;
		constant timeout : in time := 1 ms)
	is
		variable deadline : time;
		variable len : std_logic_vector(key'length - 1 downto 0);
	begin
		deadline := now + timeout;

		sync_in.accept <= '1';
		key := (others => '-');

		for i in 0 to key'length / 8 - 1 loop
			wait until rising_edge(clk) and sync_out.valid = '1' for deadline - now;
			assert sync_out.valid = '1' report "Timeout" severity failure;
			if littleEndian then
				key(key'low + i * 8 + 7 downto key'low + i * 8) := sync_out.data;
			else
				key(key'high - i * 8 downto key'high - i * 8 - 7) := sync_out.data;
			end if;
		end loop;

		for i in 0 to len'length / 8 - 1 loop
			wait until rising_edge(clk) and sync_out.valid = '1' for deadline - now;
			assert sync_out.valid = '1' report "Timeout" severity failure;
			if littleEndian then
				len(len'low + i * 8 + 7 downto len'low + i * 8) := sync_out.data;
			else
				len(len'high - i * 8 downto len'high - i * 8 - 7) := sync_out.data;
			end if;
		end loop;

		for i in 0 to to_integer(unsigned(len)) - 1 loop
			wait until rising_edge(clk) and sync_out.valid = '1' for deadline - now;
			assert sync_out.valid = '1' report "Timeout" severity failure;
			buf(buf'low + i) := sync_out.data;
			last := sync_out.last = '1';
		end loop;

		for i in to_integer(unsigned(len)) to buf'length - 1 loop
			buf(i) := (others => '-');
		end loop;

		sync_in.accept <= '0';
	end procedure;

	procedure sync_accept_update_vars(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		constant timeout : in time := 1 ms)
	is
	begin
		sync_in.accept <= '1';
		wait until rising_edge(clk) and sync_out.valid = '1' and sync_out.last = '1' for timeout;
		assert sync_out.valid = '1' and sync_out.last = '1' report "Timeout" severity failure;
		sync_in.accept <= '0';
	end procedure;

	procedure sync_accept_update(signal clk : in std_logic;
		signal sync_in : inout libstored_pkg.msg_t;
		signal sync_out : in libstored_pkg.msg_t;
		constant id_out : std_logic_vector(15 downto 0);
		constant littleEndian : boolean := true;
		constant timeout : in time := 1 ms)
	is
		variable deadline : time;
	begin
		deadline := now + timeout;
		sync_accept_update_start(clk, sync_in, sync_out, id_out, littleEndian, deadline - now);
		sync_accept_update_vars(clk, sync_in, sync_out, deadline - now);
	end procedure;

	procedure uart_do_tx(constant baud : integer;
		signal tx : in std_logic;
		variable buf : out buffer_t;
		constant len : positive := 1;
		constant timeout : time := 1 ms)
	is
		variable deadline : time;
	begin
		deadline := now + timeout;
		for b in 1 to len loop
			if tx /= '0' then
				wait until tx = '0' for deadline - now;
				assert tx = '0' report "Timeout" severity failure;
			end if;
			wait for 0.5 sec / real(baud);

			for i in 0 to 7 loop
				wait for 1 sec / real(baud);
				buf(b - 1)(i) := tx;
			end loop;

			wait for 1 sec / real(baud);
			assert tx = '1' report "Invalid stop bit" severity failure;
		end loop;
	end procedure;

	procedure uart_do_rx(constant baud : integer;
		signal rx : out std_logic;
		signal rts : in std_logic;
		constant buf : buffer_t;
		constant timeout : time := 1 ms)
	is
		variable deadline : time;
	begin
		deadline := now + timeout;

		for b in buf'range loop
			if rts /= '0' then
				wait until rts = '0' for deadline - now;
				assert rts = '0' report "Timeout" severity failure;
			end if;

			rx <= '0';
			wait for 1 sec / real(baud);

			for i in 0 to 7 loop
				rx <= buf(b)(i);
				wait for 1 sec / real(baud);
			end loop;

			rx <= '1';
			wait for 1 sec / real(baud);
		end loop;
	end procedure;

end package body;




library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_textio.all;
use work.libstored_pkg;
use work.libstored_tb_pkg;

entity FileLayer is
	generic (
		SLEEP_s : real := 100.0e-6;
		FILENAME_IN : string := "stack_in.txt";
		FILENAME_OUT : string := "stack_out.txt";
		VERBOSE : boolean := false
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		encode_in : in libstored_pkg.msg_t;
		decode_out : out libstored_pkg.msg_t;

		idle : out std_logic;

		mtu_decode_in : in natural := 0;
		mtu_decode_out : out natural
	);
end FileLayer;

architecture behav of FileLayer is
	type file_t is file of character;

	signal fn_in : libstored_tb_pkg.str_t(0 to 255) := libstored_tb_pkg.to_str(FILENAME_IN, 256);
	signal fn_out : libstored_tb_pkg.str_t(0 to 255) := libstored_tb_pkg.to_str(FILENAME_OUT, 256);
begin
	mtu_decode_out <= mtu_decode_in;

	read_p : process
		variable status : file_open_status;
		file f : file_t;
		variable v : character;
	begin
		idle <= '0';
		decode_out.last <= '0';
		decode_out.valid <= '0';

		wait until rising_edge(clk) and rstn = '1' and encode_in.accept = '1';
		report "Open " & libstored_tb_pkg.to_string(fn_in) & " for reading..." severity note;

		file_open(status, f, libstored_tb_pkg.to_string(fn_in), READ_MODE);

		case status is
		when OPEN_OK =>
			report "Reading from " & libstored_tb_pkg.to_string(fn_in) severity note;

			while not endfile(f) loop
				-- Let TeeLayer finish first, for correct XsimLayer behavior.
				wait for 0 ns;
				read(f, v);
				if VERBOSE then
					report "Read '" & v & "' (" & integer'image(character'pos(v)) & ")" severity note;
				end if;

				if v = nul then
					wait for SLEEP_s * 1 sec;
					wait until rising_edge(clk);
				else
					decode_out.data <= std_logic_vector(to_unsigned(character'pos(v), 8));
					decode_out.valid <= '1';
					wait until rising_edge(clk) and encode_in.accept = '1';
					decode_out.valid <= '0';
					decode_out.data <= (others => '-');
				end if;
			end loop;

			report "Closed " & libstored_tb_pkg.to_string(fn_in) & "; EOF" severity warning;
		when STATUS_ERROR =>
			report "Cannot open " & libstored_tb_pkg.to_string(fn_in) & "; STATUS_ERROR" severity failure;
			wait;
		when NAME_ERROR =>
			report "Cannot open " & libstored_tb_pkg.to_string(fn_in) & "; NAME_ERROR" severity failure;
			wait;
		when MODE_ERROR =>
			report "Cannot open " & libstored_tb_pkg.to_string(fn_in) & "; MODE_ERROR" severity failure;
			wait;
		when others =>
			report "Cannot open " & libstored_tb_pkg.to_string(fn_in) severity failure;
			wait;
		end case;

		file_close(f);

		-- Signal end of file.
		wait until rising_edge(clk);
		decode_out.data <= x"00";
		decode_out.last <= '1';
		decode_out.valid <= '1';
		wait until rising_edge(clk) and encode_in.accept = '1';
		decode_out.valid <= '0';

		idle <= '1';
		wait;
	end process;

	write_p : process
		variable status : file_open_status;
		file f : file_t;
		variable v : character;
	begin
		decode_out.accept <= '0';

		wait until rising_edge(clk) and rstn = '1' and encode_in.valid = '1';
		report "Open " & libstored_tb_pkg.to_string(fn_out) & " for writing..." severity note;

		file_open(status, f, libstored_tb_pkg.to_string(fn_out), WRITE_MODE);

		case status is
		when OPEN_OK =>
			report "Writing to " & libstored_tb_pkg.to_string(fn_out) severity note;
		when STATUS_ERROR =>
			report "Cannot open " & libstored_tb_pkg.to_string(fn_out) & "; STATUS_ERROR" severity failure;
			wait;
		when NAME_ERROR =>
			report "Cannot open " & libstored_tb_pkg.to_string(fn_out) & "; NAME_ERROR" severity failure;
			wait;
		when MODE_ERROR =>
			report "Cannot open " & libstored_tb_pkg.to_string(fn_out) & "; MODE_ERROR" severity failure;
			wait;
		when others =>
			report "Cannot open " & libstored_tb_pkg.to_string(fn_out) severity failure;
			wait;
		end case;

		while true loop
			decode_out.accept <= '1';
			wait until rising_edge(clk) and encode_in.valid = '1';
			v := character'val(to_integer(unsigned(encode_in.data)));
			decode_out.accept <= '0';

			if VERBOSE then
				report "Write '" & v & "' (" & integer'image(character'pos(v)) & ")" severity note;
			end if;
			write(f, v);
		end loop;

		file_close(f);
		wait;
	end process;

end behav;







library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_textio.all;
use work.libstored_pkg;
use work.libstored_tb_pkg;

entity TeeLayer is
	generic (
		FILENAME_ENCODE : string := "";
		FILENAME_DECODE : string := ""
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		encode_in : in libstored_pkg.msg_t;
		encode_out : out libstored_pkg.msg_t;

		decode_in : in libstored_pkg.msg_t;
		decode_out : out libstored_pkg.msg_t;

		idle : out std_logic;

		mtu_decode_in : in natural := 0;
		mtu_decode_out : out natural
	);
end TeeLayer;

architecture behav of TeeLayer is
	type file_t is file of character;

	signal fn_enc : libstored_tb_pkg.str_t(0 to 255) := libstored_tb_pkg.to_str(FILENAME_ENCODE, 256);
	signal fn_dec : libstored_tb_pkg.str_t(0 to 255) := libstored_tb_pkg.to_str(FILENAME_DECODE, 256);
begin

	mtu_decode_out <= mtu_decode_in;

	idle <= '1';

	encode_out <= encode_in;
	decode_out <= decode_in;

	enc_p : process
		variable status : file_open_status;
		file f : file_t;
		variable v : character;
	begin
		wait until rising_edge(clk) and rstn = '1' and encode_in.valid = '1' and decode_in.accept = '1';
		if fn_enc(0) = libstored_tb_pkg.STR_NULL then
			wait;
		end if;

		report "Open " & libstored_tb_pkg.to_string(fn_enc) & " for encode..." severity note;

		file_open(status, f, libstored_tb_pkg.to_string(fn_enc), WRITE_MODE);

		case status is
		when OPEN_OK =>
			report "Write encode to " & libstored_tb_pkg.to_string(fn_enc) severity note;
		when STATUS_ERROR =>
			report "Cannot open " & libstored_tb_pkg.to_string(fn_enc) & "; STATUS_ERROR" severity failure;
			wait;
		when NAME_ERROR =>
			report "Cannot open " & libstored_tb_pkg.to_string(fn_enc) & "; NAME_ERROR" severity failure;
			wait;
		when MODE_ERROR =>
			report "Cannot open " & libstored_tb_pkg.to_string(fn_enc) & "; MODE_ERROR" severity failure;
			wait;
		when others =>
			report "Cannot open " & libstored_tb_pkg.to_string(fn_enc) severity failure;
			wait;
		end case;

		while true loop
			v := character'val(to_integer(unsigned(encode_in.data)));
			write(f, v);
			wait until rising_edge(clk) and encode_in.valid = '1' and decode_in.accept = '1';
		end loop;

		file_close(f);
		wait;
	end process;

	dec_p : process
		variable status : file_open_status;
		file f : file_t;
		variable v : character;
	begin
		wait until rising_edge(clk) and rstn = '1' and decode_in.valid = '1' and encode_in.accept = '1';
		if fn_dec(0) = libstored_tb_pkg.STR_NULL then
			wait;
		end if;

		report "Open " & libstored_tb_pkg.to_string(fn_dec) & " for decode..." severity note;

		file_open(status, f, libstored_tb_pkg.to_string(fn_dec), WRITE_MODE);

		case status is
		when OPEN_OK =>
			report "Write decode to " & libstored_tb_pkg.to_string(fn_dec) severity note;
		when STATUS_ERROR =>
			report "Cannot open " & libstored_tb_pkg.to_string(fn_dec) & "; STATUS_ERROR" severity failure;
			wait;
		when NAME_ERROR =>
			report "Cannot open " & libstored_tb_pkg.to_string(fn_dec) & "; NAME_ERROR" severity failure;
			wait;
		when MODE_ERROR =>
			report "Cannot open " & libstored_tb_pkg.to_string(fn_dec) & "; MODE_ERROR" severity failure;
			wait;
		when others =>
			report "Cannot open " & libstored_tb_pkg.to_string(fn_dec) severity failure;
			wait;
		end case;

		while true loop
			v := character'val(to_integer(unsigned(decode_in.data)));
			write(f, v);
			wait until rising_edge(clk) and decode_in.valid = '1' and encode_in.accept = '1';
		end loop;

		file_close(f);
		wait;
	end process;

end behav;





library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_textio.all;
use work.libstored_pkg;
use work.libstored_tb_pkg;

entity XsimLayer is
	generic (
		PIPE_PREFIX : string;
		VERBOSE : boolean := false
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		encode_in : in libstored_pkg.msg_t;
		decode_out : out libstored_pkg.msg_t;

		idle : out std_logic;

		mtu_decode_in : in natural := 0;
		mtu_decode_out : out natural
	);
end XsimLayer;

architecture behav of XsimLayer is
	signal idle_pipe, idle_tee : std_logic;
	signal file_encode_in, file_decode_out : libstored_pkg.msg_t;
begin
	mtu_decode_out <= mtu_decode_in;

	pipe_inst : entity work.FileLayer
		generic map (
			SLEEP_s => 0.0,
			FILENAME_IN => PIPE_PREFIX & "_to_xsim",
			FILENAME_OUT => PIPE_PREFIX & "_from_xsim",
			VERBOSE => VERBOSE
		)
		port map (
			clk => clk,
			rstn => rstn,
			encode_in => file_encode_in,
			decode_out => file_decode_out,
			idle => idle_pipe
		);

	tee_inst : entity work.TeeLayer
		generic map (
			FILENAME_DECODE => PIPE_PREFIX & "_req_xsim"
		)
		port map (
			clk => clk,
			rstn => rstn,
			encode_in => encode_in,
			encode_out => file_encode_in,
			decode_in => file_decode_out,
			decode_out => decode_out,
			idle => idle_tee
		);

	idle <= idle_pipe and idle_tee;

end behav;






library ieee;
use ieee.std_logic_1164.all;
use ieee.math_real.all;
use work.libstored_pkg;
use work.libstored_tb_pkg;

entity RandomDelayLayer is
	generic (
		SYSTEM_CLK_FREQ : integer := 100e6;
		MIN_DELAY_s : real := 0.0;
		MAX_DELAY_s : real := 1.0e-6;
		SEED : positive := 42
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		encode_in : in libstored_pkg.msg_t;
		encode_out : out libstored_pkg.msg_t;

		decode_in : in libstored_pkg.msg_t;
		decode_out : out libstored_pkg.msg_t;

		mtu_decode_in : in natural := 0;
		mtu_decode_out : out natural
	);
end RandomDelayLayer;

architecture behav of RandomDelayLayer is
	constant MIN_DELAY : natural := integer(MIN_DELAY_s * real(SYSTEM_CLK_FREQ));
	constant MAX_DELAY : natural := libstored_pkg.maximum(MIN_DELAY + 1, integer(MAX_DELAY_s * real(SYSTEM_CLK_FREQ)));

	shared variable seed1, seed2 : integer := SEED;

	impure function rand_duration return natural is
		variable r : real;
	begin
		uniform(seed1, seed2, r);
		return natural(round(r * real(MAX_DELAY - MIN_DELAY + 1) + real(MIN_DELAY) - 0.5));
	end function;

	procedure accept_delay(
		signal valid_in : in std_logic; signal accept_in : std_logic;
		signal valid_out : out std_logic; signal accept_out : out std_logic) is
		variable delay : natural;
	begin
		accept_out <= '0';
		valid_out <= '0';
		wait until rising_edge(clk) and rstn = '1';

		while true loop
			accept_out <= '0';
			valid_out <= '0';

			delay := rand_duration;

			while delay > 0 and not valid_in = '1' loop
				delay := delay - 1;
--				report "wait " & integer'image(delay) severity note;
				wait until rising_edge(clk);
			end loop;

			if delay = 0 then
				-- delay is 0, so pass through
				accept_out <= accept_in;
				valid_out <= valid_in;
				while not (rising_edge(clk) and valid_in = '1') loop
					wait until rising_edge(clk) or accept_in'event or valid_in'event;
					accept_out <= accept_in;
					valid_out <= valid_in;
--					report "pass through " severity note;
				end loop;
			else
				-- is valid, but wait a bit longer
				for n in delay - 1 downto 0 loop
--					report "delay " & integer'image(n) severity note;
					wait until rising_edge(clk);
				end loop;

				wait for 0 ns;
				valid_out <= '1';
				accept_out <= accept_in;
			end if;

			while not (rising_edge(clk) and accept_in = '1') loop
				accept_out <= accept_in;
				wait until rising_edge(clk) or accept_in'event;
			end loop;
		end loop;
	end procedure;

	signal encode_out_accept_i, decode_out_accept_i : std_logic;
	signal encode_out_valid_i, decode_out_valid_i : std_logic;
begin
	mtu_decode_out <= mtu_decode_in;

	process
	begin
		accept_delay(encode_in.valid, decode_in.accept, encode_out_valid_i, decode_out_accept_i);
	end process;

	encode_out.valid <= encode_out_valid_i;
	encode_out.data <= encode_in.data when encode_out_valid_i = '1' else (others => '-');
	encode_out.last <= encode_in.last when encode_out_valid_i = '1' else '-';
	decode_out.accept <= decode_out_accept_i;

	process
	begin
		accept_delay(decode_in.valid, encode_in.accept, decode_out_valid_i, encode_out_accept_i);
	end process;

	decode_out.valid <= decode_out_valid_i;
	decode_out.data <= decode_in.data when decode_out_valid_i = '1' else (others => '-');
	decode_out.last <= decode_in.last when decode_out_valid_i = '1' else '-';
	encode_out.accept <= encode_out_accept_i;

end behav;

--pragma translate_on

