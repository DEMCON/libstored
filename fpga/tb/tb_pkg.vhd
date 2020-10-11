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

package libstored_tb_pkg is

	type test_t is record
		checks : natural;
		errors : natural;
	end record;

	procedure test_init(variable test : inout test_t);
	procedure test_finish(variable test : inout test_t);

	procedure test_error(variable test : inout test_t; constant msg : in string);
	procedure test_expect_eq(variable test : inout test_t; constant x, expect : in unsigned);
	procedure test_expect_eq(variable test : inout test_t; constant x, expect : in std_logic_vector);

end libstored_tb_pkg;

package body libstored_tb_pkg is

	procedure test_init(variable test : inout test_t) is
	begin
		test.checks := 0;
		test.errors := 0;
		report "Test start" severity note;
	end procedure;

	procedure test_finish(variable test : inout test_t) is
	begin
		report "Test result: " & integer'image(test.checks) & " checks, "
			& integer'image(test.errors) & " errors" severity note;

		if test.errors = 0 then
			report "Test PASSED" severity note;
		else
			report "Test FAILED" severity error;
		end if;
	end procedure;

	procedure test_error(variable test : inout test_t; constant msg : in string) is
	begin
		test.errors := test.errors + 1;
		report msg severity error;
	end procedure;

	procedure test_expect_eq(variable test : inout test_t; constant x, expect : in unsigned) is
	begin
		test_expect_eq(test, std_logic_vector(x), std_logic_vector(expect));
	end procedure;

	procedure test_expect_eq(variable test : inout test_t; constant x, expect : in std_logic_vector) is
	begin
		test.checks := test.checks + 1;

		if x'length /= expect'length then
			test_error(test, "Lengths do not match: got " & integer'image(x'length) &
				", expected " & integer'image(expect'length));
		elsif x /= expect then
			test_error(test, "Values are not equal");
		end if;
	end procedure;

end package body;

