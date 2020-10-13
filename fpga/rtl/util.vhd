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

entity libstored_delay1 is
	generic (
		DELAY : natural := 1;
		INIT : std_logic := '-'
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;
		i : in std_logic;
		o : out std_logic
	);
end libstored_delay1;

architecture rtl of libstored_delay1 is
	signal r : std_logic_vector(0 to DELAY - 1);
begin
	process(clk)
	begin
		if rising_edge(clk) then
			r <= i & r(0 to r'high - 1);

			if rstn /= '1' then
				r <= (others => INIT);
			end if;
		end if;
	end process;

	o <= r(r'high);
end rtl;




library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity libstored_stream_buffer is
	generic (
		WIDTH : natural
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		i : in std_logic_vector(WIDTH - 1 downto 0);
		i_valid : in std_logic;
		i_accept : out std_logic;

		o : out std_logic_vector(WIDTH - 1 downto 0);
		o_valid : out std_logic;
		o_accept : in std_logic
	);
end libstored_stream_buffer;

architecture rtl of libstored_stream_buffer is
	type state_t is (STATE_RESET, STATE_EMPTY, STATE_HAVE_1, STATE_HAVE_2
--pragma translate_off
		, STATE_ERROR
--pragma translate_on
	);

	type r_t is record
		state : state_t;
		d1 : std_logic_vector(WIDTH - 1 downto 0);
		d2 : std_logic_vector(WIDTH - 1 downto 0);
	end record;

	signal r, r_in : r_t;
begin
	process(r, rstn, i_valid, o_accept, i)
		variable v : r_t;
	begin
		v := r;

		case r.state is
		when STATE_RESET =>
			v.state := STATE_EMPTY;
		when STATE_EMPTY =>
			if i_valid = '1' then
				v.d1 := i;
				v.state := STATE_HAVE_1;
			end if;
		when STATE_HAVE_1 =>
			if i_valid = '1' and o_accept = '0' then
				v.d2 := r.d1;
				v.d1 := i;
				v.state := STATE_HAVE_2;
			elsif i_valid = '1' and o_accept = '1' then
				v.d1 := i;
			elsif i_valid = '0' and o_accept = '1' then
				v.state := STATE_EMPTY;
--pragma translate_off
				v.d1 := (others => '-');
--pragma translate_on
			end if;
		when STATE_HAVE_2 =>
			if o_accept = '1' then
				v.state := STATE_HAVE_1;
--pragma translate_off
				v.d2 := (others => '-');
--pragma translate_on
			end if;
--pragma translate_off
		when STATE_ERROR => null;
--pragma translate_on
		end case;

--pragma translate_off
		if is_x(i_valid) or is_x(o_accept) then
			v.state := STATE_ERROR;
		end if;
--pragma translate_on

		if rstn /= '1' then
			v.state := STATE_RESET;
		end if;

		r_in <= v;
	end process;

	process(clk)
	begin
		if rising_edge(clk) then
			r <= r_in;
		end if;
	end process;

	with r.state select
		o <=
--pragma translate_off
			(others => 'X') when STATE_ERROR,
--pragma translate_on
			r.d1 when STATE_HAVE_1,
			r.d2 when STATE_HAVE_2,
			(others => '-') when others;

	with r.state select
		o_valid <=
--pragma translate_off
			'X' when STATE_ERROR,
--pragma translate_on
			'1' when STATE_HAVE_1 | STATE_HAVE_2,
			'0' when others;

	with r.state select
		i_accept <=
--pragma translate_off
			'X' when STATE_ERROR,
--pragma translate_on
			'1' when STATE_EMPTY | STATE_HAVE_1,
			'0' when others;

--pragma translate_off
	assert not(rising_edge(clk) and is_x(i_valid)) report "Invalid i_valid" severity error;
	assert not(rising_edge(clk) and is_x(o_accept)) report "Invalid o_accept" severity error;
--pragma translate_on

end rtl;
