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

package libstored_pkg is
	constant SIMULATION_SPEEDUP : real := 1.0e4;

	type hash_t is array(0 to 39) of std_logic_vector(7 downto 0);

	type msg_t is record
		data : std_logic_vector(7 downto 0);
		last : std_logic;
		valid : std_logic;
		accept : std_logic;
	end record;

	constant msg_term : msg_t := (
		data => (others => '-'),
		last => '-',
		valid => '0',
		accept => '1'
	);

	function maximum(constant a, b : integer) return integer;
	function minimum(constant a, b : integer) return integer;
	function ceil(constant x : real) return integer;
	function normalize(constant x : std_logic_vector) return std_logic_vector;
end libstored_pkg;

package body libstored_pkg is
	function maximum(constant a, b : integer) return integer is
	begin
		if a > b then
			return a;
		else
			return b;
		end if;
	end function;

	function minimum(constant a, b : integer) return integer is
	begin
		if a < b then
			return a;
		else
			return b;
		end if;
	end function;

	function ceil(constant x : real) return integer is
	begin
		return integer(x + 0.5);
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

end package body;
