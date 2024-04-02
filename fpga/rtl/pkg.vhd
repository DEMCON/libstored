-- SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
--
-- SPDX-License-Identifier: MPL-2.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.math_real;

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

	constant msg_highz : msg_t := (
		data => (others => 'Z'),
		last => 'Z',
		valid => 'Z',
		accept => 'Z'
	);

	function maximum(constant a, b : integer) return integer;
	function minimum(constant a, b : integer) return integer;
	function bits(constant x : natural) return natural;
	function ceil(constant x : real) return integer;
	function normalize(constant x : std_logic_vector) return std_logic_vector;
	function swap_endian(constant x : std_logic_vector) return std_logic_vector;

	type access_t is (ACCESS_RW, ACCESS_RO, ACCESS_WO, ACCESS_NA);
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

	function bits(constant x : natural) return natural is
	begin
		if x = 0 then
			return 1;
		else
			return integer(math_real.ceil(math_real.log2(real(x + 1))));
		end if;
	end function;

	function ceil(constant x : real) return integer is
	begin
		return integer(math_real.ceil(x));
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

	function swap_endian(constant x : std_logic_vector) return std_logic_vector is
		variable n, v : std_logic_vector(x'length - 1 downto 0);
	begin
		n := normalize(x);
		for i in 0 to n'length / 8 - 1 loop
			v(i * 8 + 7 downto i * 8) := n(n'high - i * 8 downto n'high - i * 8 - 7);
		end loop;
		return v;
	end function;

end package body;
