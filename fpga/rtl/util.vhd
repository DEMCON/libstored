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

	attribute retiming_forward : integer;
	attribute retiming_forward of r : signal is 1;
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

entity libstored_metastabilize is
	generic (
		DELAY : natural := 2
	);
	port (
		clk : in std_logic;
		rstn : in std_logic := '1';
		i : in std_logic;
		o : out std_logic
	);
end libstored_metastabilize;

architecture rtl of libstored_metastabilize is
	signal r : std_logic_vector(0 to DELAY - 1);

	attribute retiming_forward : integer;
	attribute retiming_forward of r : signal is 0;
	attribute retiming_backward : integer;
	attribute retiming_backward of r : signal is 0;
begin
	process(clk)
	begin
		if rising_edge(clk) then
			r <= i & r(0 to r'high - 1);
		end if;
	end process;

	o <= r(r'high);
end rtl;



library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity libstored_stream_buffer is
	generic (
		WIDTH : positive
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
	type state_t is (STATE_EMPTY, STATE_HAVE_1, STATE_HAVE_2
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
		when STATE_EMPTY =>
			v.d1 := i;
			if i_valid = '1' then
				v.state := STATE_HAVE_1;
--pragma translate_off
			else
				v.d1 := (others => '-');
--pragma translate_on
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
			v.state := STATE_EMPTY;
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
			(others => '-') when STATE_EMPTY,
--pragma translate_on
			r.d2 when STATE_HAVE_2,
			r.d1 when others;

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
	assert not(rising_edge(clk) and rstn = '1' and is_x(i_valid)) report "Invalid i_valid" severity error;
	assert not(rising_edge(clk) and rstn = '1' and is_x(o_accept)) report "Invalid o_accept" severity error;
--pragma translate_on

end rtl;




library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity libstored_fifo is
	generic (
		WIDTH : positive;
		DEPTH : positive := 1;
		ALMOST_FULL_REMAINING : natural := 0;
		ALMOST_EMPTY_REMAINING : natural := 0
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		i : in std_logic_vector(WIDTH - 1 downto 0);
		i_valid : in std_logic;
		i_accept : out std_logic;
		i_commit : in std_logic := '1';
		i_rollback : in std_logic := '0';

		o : out std_logic_vector(WIDTH - 1 downto 0);
		o_valid : out std_logic;
		o_accept : in std_logic;
		o_commit : in std_logic := '1';
		o_rollback : in std_logic := '0';

		full : out std_logic;
		empty : out std_logic;
		almost_full : out std_logic;
		almost_empty : out std_logic
	);
end libstored_fifo;

architecture rtl of libstored_fifo is
	constant ACTUAL_DEPTH : positive := 2**(libstored_pkg.bits(libstored_pkg.maximum(ALMOST_FULL_REMAINING + 1, DEPTH)));
	type fifo_t is array(0 to ACTUAL_DEPTH - 1) of std_logic_vector(WIDTH - 1 downto 0);
	subtype ptr_t is natural range 0 to ACTUAL_DEPTH - 1;

	shared variable fifo : fifo_t;

	signal rp, rp_next, rp_tmp, rp_tmp_next, wp, wp_tmp, wp_tmp_next : ptr_t;
	signal full_i, empty_i : std_logic;
begin

	rp_tmp_next <= (rp_tmp + 1) mod ACTUAL_DEPTH;
	wp_tmp_next <= (wp_tmp + 1) mod ACTUAL_DEPTH;

	full_i <=
		'1' when wp_tmp_next = rp else
		'0';

	full <= full_i;
	i_accept <= not full_i;

	empty_i <=
		'1' when rp_tmp = wp else
		'0';

	empty <= empty_i;
	o <= fifo(rp_tmp);
	o_valid <= not empty_i;

	almost_full <=
		full_i when ALMOST_FULL_REMAINING = 0 else
		'1' when (rp + ACTUAL_DEPTH - wp_tmp_next) mod ACTUAL_DEPTH <= ALMOST_FULL_REMAINING else
		'0';

	almost_empty <=
		empty_i when ALMOST_EMPTY_REMAINING = 0 else
		'1' when (wp + ACTUAL_DEPTH - rp_tmp) mod ACTUAL_DEPTH <= ALMOST_EMPTY_REMAINING else
		'0';

	process(clk)
	begin
		if rising_edge(clk) then
			if full_i = '0' and i_valid = '1' and i_rollback = '0' then
				fifo(wp_tmp) := i;
				wp_tmp <= wp_tmp_next;
				if i_commit = '1' then
					wp <= wp_tmp_next;
				end if;
			elsif i_rollback = '1' then
				wp_tmp <= wp;
			elsif i_commit = '1' then
				wp <= wp_tmp;
			end if;

--pragma translate_off
			if i_rollback = '1' then
				if wp < wp_tmp then
					for i in wp to wp_tmp - 1 loop
						fifo(i) := (others => '-');
					end loop;
				else
					for i in wp to ACTUAL_DEPTH - 1 loop
						fifo(i) := (others => '-');
					end loop;
					for i in 1 to wp_tmp loop
						fifo(i - 1) := (others => '-');
					end loop;
				end if;
			end if;
--pragma translate_on

--pragma translate_off
			if is_x(i_valid) or is_x(o_accept) or is_x(i_commit) or is_x(i_rollback) then
				fifo := (others => (others => 'X'));
				assert rstn /= '1' report "Invalid FIFO control signals" severity warning;
			end if;
--pragma translate_on

			if rstn /= '1' then
--pragma translate_off
				fifo := (others => (others => '-'));
--pragma translate_on
				wp <= 0;
				wp_tmp <= 0;
			end if;
		end if;
	end process;

	process(clk)
	begin
		if rising_edge(clk) then
			if empty_i = '0' and o_accept = '1' and o_rollback = '0' then
				rp_tmp <= rp_tmp_next;
				if i_commit = '1' then
					rp <= rp_tmp_next;
				end if;
			elsif o_rollback = '1' then
				rp_tmp <= rp;
			elsif o_commit = '1' then
				rp <= rp_tmp;
			end if;

--pragma translate_off
			if o_rollback = '0' and o_commit = '1' then
				if rp < rp_tmp then
					for i in rp to rp_tmp - 1 loop
						fifo(i) := (others => '-');
					end loop;
				else
					for i in rp to ACTUAL_DEPTH - 1 loop
						fifo(i) := (others => '-');
					end loop;
					for i in 1 to rp_tmp loop
						fifo(i - 1) := (others => '-');
					end loop;
				end if;
			end if;
--pragma translate_on

			if rstn /= '1' then
				rp <= 0;
				rp_tmp <= 0;
			end if;
		end if;
	end process;

end rtl;

