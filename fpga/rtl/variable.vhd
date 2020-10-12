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

entity libstored_variable is
	generic (
		SYSTEM_CLK_FREQ : integer := 100e6;
		LEN_LENGTH : natural := 16;
		DATA_INIT : std_logic_vector;
		KEY : std_logic_vector;
		BUFFER_PADDING_BEFORE : natural := 0;
		LITTLE_ENDIAN : boolean := true;
		BLOB : boolean := false;
		BUFFER_CHAIN : boolean := true;
		SIMULATION : boolean := false
--pragma translate_off
			or true
--pragma translate_on
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		data_out : out std_logic_vector(DATA_INIT'length - 1 downto 0);
		data_out_changed : out std_logic;
		data_in : in std_logic_vector(DATA_INIT'length - 1 downto 0) := (others => '-');
		data_in_we : in std_logic := '0';

		sync_in_commit : in std_logic := '1';
		sync_out_snapshot : in std_logic := '1';

		-- Input of sync messages.
		-- When sync_in_buffer is true, this variable takes
		-- the first DATA_INIT'length bits and forwards the rest.
		-- Otherwise, it expects a Update message header (key/size),
		-- and either consumes or forwards the message.
		sync_in_data : in std_logic_vector(7 downto 0) := (others => '-');
		sync_in_last : in std_logic := '-';
		sync_in_valid : in std_logic := '0';
		sync_in_buffer : in std_logic := '0';

		-- Unhandled sync messages, for daisy chaining.
		sync_in_data_next : out std_logic_vector(7 downto 0) := (others => '-');
		sync_in_last_next : out std_logic := '-';
		sync_in_valid_next : out std_logic := '0';
		sync_in_buffer_next : out std_logic := '0';

		-- Sync messages generated (or forwarded) by this variable.
		sync_out_len : out std_logic_vector(LEN_LENGTH - 1 downto 0);
		sync_out_data : out std_logic_vector(7 downto 0);
		sync_out_last : out std_logic;
		sync_out_valid : out std_logic;
		sync_out_accept : in std_logic := '0';
		sync_out_have_changes : out std_logic;

		-- Sync messages generated (of forwarded) by another variable, for daisy chaining.
		sync_out_len_prev : in std_logic_vector(LEN_LENGTH - 1 downto 0) := (others => '-');
		sync_out_data_prev : in std_logic_vector(7 downto 0) := (others => '-');
		sync_out_last_prev : in std_logic := '-';
		sync_out_valid_prev : in std_logic := '0';
		sync_out_accept_prev : out std_logic;
		sync_out_have_changes_prev : in std_logic := '0'
	);
end libstored_variable;

architecture rtl of libstored_variable is
	constant DATA_BITS : natural := DATA_INIT'length;
	constant DATA_BYTES : natural := DATA_BITS / 8;
	constant KEY_BYTES : natural := KEY'length / 8;

	signal data_in_i, data, data_update, data_snapshot : std_logic_vector(DATA_INIT'length - 1 downto 0);
	signal data_in_we_i, data_out_changed_i, data_snapshot_changed : std_logic;

	signal sync_in_data_next_i : std_logic_vector(7 downto 0);
	signal sync_in_last_next_i : std_logic;
	signal sync_in_valid_next_i : std_logic;
	signal sync_in_buffer_next_i : std_logic;

	signal sync_out_have_changes_i : std_logic;
begin

	process(clk)
	begin
		if rising_edge(clk) then
			data_out_changed_i <= '0';
			data_snapshot_changed <= '0';

			if sync_out_snapshot = '1' then
				data_snapshot <= data;
				if data_snapshot /= data then
					data_snapshot_changed <= '1';
				end if;
			end if;

			if data_in_we_i = '1' then
				data <= data_in_i;

				if data /= data_in_i then
					data_out_changed_i <= '1';
				end if;
			end if;

			if rstn /= '1' then
				data <= DATA_INIT;
				data_snapshot <= DATA_INIT;
				data_out_changed_i <= '0';
			end if;
		end if;
	end process;

	data_out <= data;

	data_in_i <=
		data_in when data_in_we = '1' else
		data_update
--pragma translate_off
			when sync_in_commit = '1' else (others => '-')
--pragma translate_on
			;

	data_in_we_i <= data_in_we or sync_in_commit;

	data_out_changed <= data_out_changed_i;

	sync_out_have_changes_i <= data_snapshot_changed or sync_out_have_changes_prev;

	sync_in_g : if true generate
		type state_t is (STATE_RESET, STATE_IDLE,
			STATE_PADDING, STATE_BUFFER, STATE_BUFFER_PASSTHROUGH,
			STATE_UPDATE, STATE_UPDATE_LEN, STATE_UPDATE_DATA, STATE_UPDATE_SKIP,
			STATE_UPDATE_LEN_SKIP, STATE_UPDATE_DATA_SKIP);
		type r_t is record
			state : state_t;
			cnt : natural range 0 to maximum(2**(KEY_BYTES * 8) - 1, maximum(DATA_BYTES, BUFFER_PADDING_BEFORE));
			data : std_logic_vector(DATA_BITS - 1 downto 0);
			len : std_logic_vector(KEY_BYTES * 8 - 1 downto 0);
			save : std_logic;
		end record;

		signal r, r_in : r_t;
	begin

		process(r, rstn, sync_in_valid, sync_in_last, sync_in_data, sync_in_buffer)
			variable v : r_t;
		begin
			v := r;

			if sync_in_valid = '1' and r.cnt > 0 then
				v.cnt := r.cnt - 1;
			end if;

			v.save := '0';

			case r.state is
			when STATE_RESET =>
				v.state := STATE_IDLE;
			when STATE_IDLE =>
				v.data := (others => '-');
				if sync_in_valid = '1' then
					if sync_in_buffer = '1' then
						if BUFFER_PADDING_BEFORE > 0 then
							v.state := STATE_PADDING;
							v.cnt := BUFFER_PADDING_BEFORE;
						else
							v.state := STATE_BUFFER;
							v.cnt := KEY_BYTES;
						end if;
					else
						v.state := STATE_UPDATE;
					end if;
				end if;
			when STATE_PADDING =>
				if v.cnt = 0 then
					v.state := STATE_BUFFER;
					v.cnt := r.data'length / 8;
				end if;
			when STATE_BUFFER =>
				if sync_in_valid = '1' then
					if LITTLE_ENDIAN and not BLOB then
						v.data := sync_in_data & r.data(r.data'high downto 8);
					else
						v.data := r.data(r.data'high - 8 downto 0) & sync_in_data;
					end if;

					if v.cnt = 0 then
						v.state := STATE_BUFFER_PASSTHROUGH;
						v.len := std_logic_vector(to_unsigned(DATA_BYTES, v.len'length));
						v.save := '1';
					end if;
				end if;
			when STATE_BUFFER_PASSTHROUGH =>
				null;
			when STATE_UPDATE =>
				if sync_in_valid = '1' then
					if sync_in_data /= KEY(r.cnt * 8 - 1 downto r.cnt * 8 - 8) then
						if v.cnt = 0 then
							v.state := STATE_UPDATE_LEN_SKIP;
							v.cnt := KEY_BYTES;
						else
							v.state := STATE_UPDATE_SKIP;
						end if;
					elsif v.cnt = 0 then
						v.state := STATE_UPDATE_LEN;
						v.cnt := KEY_BYTES;
					end if;
				end if;
			when STATE_UPDATE_LEN =>
				if sync_in_valid = '1' then
					if LITTLE_ENDIAN then
						v.len := sync_in_data & r.len(r.len'high downto 8);
					else
						v.len := r.len(r.len'high - 8 downto 0) & sync_in_data;
					end if;

					if v.cnt = 0 then
						v.cnt := to_integer(unsigned(v.len));
						if BLOB and v.cnt > DATA_BYTES then
							v.state := STATE_UPDATE_DATA_SKIP;
						elsif not BLOB and v.cnt /= DATA_BYTES then
							v.state := STATE_UPDATE_DATA_SKIP;
						else
							v.state := STATE_UPDATE_DATA;
							v.data := (others => '-');
						end if;
					end if;
				end if;
			when STATE_UPDATE_DATA =>
				if sync_in_valid = '1' then
					if LITTLE_ENDIAN and not BLOB then
						v.data := sync_in_data & r.data(r.data'high downto 8);
					else
						v.data := r.data(r.data'high - 8 downto 0) & sync_in_data;
					end if;

					if v.cnt = 0 then
						v.state := STATE_UPDATE;
						v.save := '1';
					end if;
				end if;
			when STATE_UPDATE_SKIP =>
				if v.cnt = 0 then
					v.state := STATE_UPDATE_LEN_SKIP;
					v.cnt := KEY_BYTES;
				end if;
			when STATE_UPDATE_LEN_SKIP =>
				if sync_in_valid = '1' then
					if LITTLE_ENDIAN then
						v.len := sync_in_data & r.len(r.len'high downto 8);
					else
						v.len := r.len(r.len'high - 8 downto 0) & sync_in_data;
					end if;

					if v.cnt = 0 then
						v.cnt := to_integer(unsigned(v.len));
						v.state := STATE_UPDATE_DATA_SKIP;
					end if;
				end if;
			when STATE_UPDATE_DATA_SKIP =>
				if v.cnt = 0 then
					v.state := STATE_UPDATE;
				end if;
			end case;

			if sync_in_valid = '1' and sync_in_last = '1' then
				v.state := STATE_IDLE;
			end if;

			if rstn /= '1' then
				v.state := STATE_RESET;
			end if;

			r_in <= v;
		end process;

		process(clk)
			variable l : natural range 0 to 2**(KEY_BYTES * 8) - 1;
		begin
			l := 0;

			if rising_edge(clk) then
				r <= r_in;

				if r_in.save = '1' then
					if not BLOB then
						-- always full buffer
						data_update <= r_in.data;
					else
						-- right aligned in r_in.data
						l := to_integer(unsigned(r.len));
						data_update <= (others => '0');
						data_update(data_update'high downto data_update'high - l * 8 + 1) <=
							r_in.data(l * 8 downto 0);
					end if;
				end if;
			end if;
		end process;

		with r_in.state select
			sync_in_valid_next_i <=
				'0' when STATE_PADDING | STATE_BUFFER,
				sync_in_valid when others;

		sync_in_data_next_i <= sync_in_data;
		sync_in_last_next_i <= sync_in_last;
		sync_in_buffer_next_i <= sync_in_buffer;

		sync_in_buffer_g : if BUFFER_CHAIN generate
		begin
			process(clk)
			begin
				if rising_edge(clk) then
					sync_in_data_next <= sync_in_data_next_i;
					sync_in_valid_next <= sync_in_valid_next_i;
					sync_in_last_next <= sync_in_last_next_i;
					sync_in_buffer_next <= sync_in_buffer_next_i;
				end if;
			end process;
		end generate;

		sync_in_no_buffer_g : if not BUFFER_CHAIN generate
		begin
			sync_in_data_next <= sync_in_data_next_i;
			sync_in_valid_next <= sync_in_valid_next_i;
			sync_in_last_next <= sync_in_last_next_i;
			sync_in_buffer_next <= sync_in_buffer_next_i;
		end generate;

--pragma translate_off
		assert not (rising_edge(clk) and r.state /= STATE_IDLE and sync_in_commit = '1')
			report "Sync commit while syncing" severity warning;
--pragma translate_on
	end generate;

	sync_out_g : if true generate
	begin
		-- TODO

		sync_out_buffer_g : if BUFFER_CHAIN generate
		begin
			process(clk)
			begin
				if rising_edge(clk) then
					sync_out_have_changes <= sync_out_have_changes_i;
				end if;
			end process;
		end generate;

		sync_out_no_buffer_g : if BUFFER_CHAIN generate
		begin
			sync_out_have_changes <= sync_out_have_changes_i;
		end generate;
	end generate;

--pragma translate_off
	assert DATA_BITS mod 8 = 0 report "Data width not multiple of bytes" severity failure;
	assert KEY'length mod 8 = 0 report "Key width not multiple of bytes" severity failure;
	assert LEN_LENGTH mod 8 = 0 report "Len width not multiple of bytes" severity failure;
--pragma translate_on
end rtl;

