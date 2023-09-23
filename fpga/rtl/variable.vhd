-- SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
--
-- SPDX-License-Identifier: MPL-2.0

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
		VAR_ACCESS : access_t := ACCESS_RW;
		SIMULATION : boolean := false
--pragma translate_off
			or true
--pragma translate_on
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		data_out : out std_logic_vector(DATA_INIT'length - 1 downto 0);
		data_out_updated : out std_logic;
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
		sync_out_data : out std_logic_vector(7 downto 0);
		sync_out_last : out std_logic;
		sync_out_valid : out std_logic;
		sync_out_accept : in std_logic := '0';
		sync_out_have_changes : out std_logic;

		-- Sync messages generated (of forwarded) by another variable, for daisy chaining.
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
	constant VAR_READ : boolean := VAR_ACCESS = ACCESS_RO or VAR_ACCESS = ACCESS_RW;
	constant VAR_WRITE : boolean := VAR_ACCESS = ACCESS_WO or VAR_ACCESS = ACCESS_RW;

	signal data_in_i, data, data_update, data_snapshot : std_logic_vector(DATA_INIT'length - 1 downto 0);
	signal data_in_we_i, data_update_valid, data_out_updated_i, data_out_changed : std_logic;

	signal sync_out_have_changes_i, sync_in_commit_r : std_logic;
begin

	process(clk)
	begin
		if rising_edge(clk) then
			data_out_updated_i <= '0';
			sync_in_commit_r <= sync_in_commit;

			if VAR_WRITE and data_snapshot /= data then
				data_out_changed <= '1';
			else
				data_out_changed <= '0';
			end if;

			if VAR_WRITE and (sync_out_snapshot = '1' or sync_in_commit_r = '1') then
				data_snapshot <= data;
			end if;

			if data_in_we_i = '1' then
				data <= data_in_i;
				data_out_updated_i <= '1';
			end if;
--pragma translate_off
			if is_x(data_in_we_i) then
				data <= (others => 'X');
				data_out_updated_i <= 'X';
			end if;
--pragma translate_on

			if rstn /= '1' then
				data <= DATA_INIT;
				if VAR_WRITE then
					data_snapshot <= DATA_INIT;
				end if;
			end if;
		end if;
	end process;

	data_out <=
		(others => '0') when not VAR_READ and not VAR_WRITE else
		data;

	data_in_i <=
		data_in when VAR_WRITE and data_in_we = '1' else
		data_update when VAR_READ and sync_in_commit = '1' and data_update_valid = '1' else
		(others => '-');

	data_in_we_i <=
		data_in_we or (sync_in_commit and data_update_valid) when VAR_READ and VAR_WRITE else
		data_in_we when not VAR_READ and VAR_WRITE else
		sync_in_commit and data_update_valid when VAR_READ and not VAR_WRITE else
		'0';

	data_out_updated <=
		data_out_updated_i when VAR_READ or VAR_WRITE else
		'0';

	sync_out_have_changes_i <=
		data_out_changed or sync_out_have_changes_prev when VAR_WRITE else
		sync_out_have_changes_prev;

	sync_in_g : if true generate
		type state_t is (STATE_RESET, STATE_IDLE,
			STATE_PADDING, STATE_BUFFER, STATE_PASSTHROUGH,
			STATE_UPDATE_KEY, STATE_UPDATE_LEN, STATE_UPDATE_DATA);
		type r_t is record
			state : state_t;
			cnt : natural range 0 to maximum(KEY_BYTES, maximum(DATA_BYTES, maximum(1, BUFFER_PADDING_BEFORE) - 1));
			offset : natural range 0 to DATA_BYTES - 1;
			data : std_logic_vector(DATA_BITS - 1 downto 0);
			keylen : std_logic_vector(KEY_BYTES * 8 - 1 downto 0);
			save : std_logic;
		end record;

		signal r, r_in : r_t;

		signal sync_in_data_next_i : std_logic_vector(7 downto 0);
		signal sync_in_last_next_i : std_logic;
		signal sync_in_valid_next_i : std_logic;
		signal sync_in_buffer_next_i : std_logic;
	begin

		process(r, rstn, sync_in_valid, sync_in_last, sync_in_data, sync_in_buffer)
			variable v : r_t;
		begin
			v := r;

			v.save := '0';

			if VAR_READ and sync_in_valid = '1' then
				if BLOB then
					v.data(v.data'high - r.offset * 8 downto v.data'high - r.offset * 8 - 7) := sync_in_data;
					v.offset := (r.offset + 1) mod DATA_BYTES;
				elsif LITTLE_ENDIAN then
					v.data := sync_in_data & r.data(r.data'high downto 8);
				else
					v.data := r.data(r.data'high - 8 downto 0) & sync_in_data;
				end if;
			end if;

			if sync_in_valid = '1' then
				if LITTLE_ENDIAN then
					v.keylen := sync_in_data & r.keylen(r.keylen'high downto 8);
				else
					v.keylen := r.keylen(r.keylen'high - 8 downto 0) & sync_in_data;
				end if;

				if r.cnt > 0 then
					v.cnt := r.cnt - 1;
				end if;
			end if;

			case r.state is
			when STATE_RESET =>
				v.state := STATE_IDLE;
			when STATE_IDLE =>
				if sync_in_valid = '1' then
					if sync_in_buffer = '1' then
						if BUFFER_PADDING_BEFORE > 1 then
							v.state := STATE_PADDING;
							v.cnt := BUFFER_PADDING_BEFORE - 1;
						elsif BUFFER_PADDING_BEFORE = 1 then
							v.state := STATE_BUFFER;
							v.offset := 0;
							v.cnt := DATA_BYTES;
						elsif DATA_BYTES > 1 then
							v.state := STATE_BUFFER;
							v.cnt := DATA_BYTES - 1;
						else
							v.state := STATE_PASSTHROUGH;
							v.save := '1';
						end if;
					else
						if KEY_BYTES > 1 then
							if not VAR_READ then
								v.state := STATE_PASSTHROUGH;
							else
								v.state := STATE_UPDATE_KEY;
								v.cnt := KEY_BYTES - 1;
							end if;
						else
							if VAR_READ and v.keylen = normalize(KEY) then
								v.state := STATE_UPDATE_LEN;
							else
								v.state := STATE_PASSTHROUGH;
							end if;
							v.cnt := KEY_BYTES;
						end if;
					end if;
				end if;
			when STATE_PADDING =>
				if v.cnt = 0 then
					v.state := STATE_BUFFER;
					v.offset := 0;
					v.cnt := DATA_BYTES;
				end if;
			when STATE_BUFFER =>
				if v.cnt = 0 then
					v.state := STATE_PASSTHROUGH;
					v.save := '1';
				end if;
			when STATE_PASSTHROUGH =>
				null;
			when STATE_UPDATE_KEY =>
				if v.cnt = 0 then
					v.cnt := KEY_BYTES;
					if v.keylen = normalize(KEY) then
						v.state := STATE_UPDATE_LEN;
					else
						v.state := STATE_PASSTHROUGH;
					end if;
				end if;
			when STATE_UPDATE_LEN =>
				if v.cnt = 0 then
					v.state := STATE_UPDATE_DATA;
					v.cnt := to_integer(unsigned(v.keylen));
				end if;
			when STATE_UPDATE_DATA =>
				if v.cnt = 0 then
					v.state := STATE_IDLE;
					v.save := '1';
				end if;
			end case;

			if sync_in_valid = '1' and sync_in_last = '1' then
				v.state := STATE_IDLE;
				v.offset := 0;
			end if;

			if rstn /= '1' then
				v.state := STATE_RESET;
				v.offset := 0;
				v.cnt := 0;
			end if;

			r_in <= v;
		end process;

		process(clk)
		begin
			if rising_edge(clk) then
				r <= r_in;

				if VAR_READ then
					if r_in.save = '1' then
						data_update <= r_in.data;
						data_update_valid <= '1';
					elsif data_in_we_i = '1' then
--pragma translate_off
						data_update <= (others => '-');
--pragma translate_on
						data_update_valid <= '0';
					end if;

					if rstn /= '1' then
						data_update_valid <= '0';
					end if;
				end if;
			end if;
		end process;

		with r.state select
			sync_in_valid_next_i <=
				sync_in_valid when STATE_PASSTHROUGH,
				sync_in_valid and not sync_in_buffer when others;

		sync_in_data_next_i <= sync_in_data;
		sync_in_last_next_i <= sync_in_last;
		sync_in_buffer_next_i <= sync_in_buffer;

		sync_in_buffer_g : if BUFFER_CHAIN generate
		begin
			process(clk)
			begin
				if rising_edge(clk) then
					sync_in_valid_next <= sync_in_valid_next_i;
					if sync_in_valid_next_i = '1' then
						-- Save some bit flips.
						sync_in_data_next <= sync_in_data_next_i;
						sync_in_last_next <= sync_in_last_next_i;
						sync_in_buffer_next <= sync_in_buffer_next_i;
--pragma translate_off
					else
						sync_in_data_next <= (others => '-');
						sync_in_last_next <= '-';
						sync_in_buffer_next <= '-';
--pragma translate_on
					end if;
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
		type state_t is (STATE_RESET, STATE_IDLE, STATE_WAIT, STATE_KEY,
			STATE_LEN, STATE_DATA, STATE_PASSTHROUGH);

		type r_t is record
			state : state_t;
			cnt : natural range 0 to maximum(KEY_BYTES, DATA_BYTES);
			changed : std_logic;
		end record;

		signal r, r_in : r_t;

		signal sync_out_data_i : std_logic_vector(7 downto 0);
		signal sync_out_valid_i : std_logic;
		signal sync_out_accept_i : std_logic;
		signal sync_out_last_i : std_logic;
		signal sync_out_accept_prev_i : std_logic;
	begin

		process(r, rstn, sync_out_valid_prev, sync_out_accept_i, sync_in_commit_r,
			data_out_changed, sync_out_data_prev, sync_out_last_prev, sync_out_snapshot)
		is
			variable v : r_t;
		begin
			v := r;

			if VAR_WRITE and sync_out_accept_i = '1' and r.cnt > 0 then
				v.cnt := r.cnt - 1;
			end if;

			if VAR_WRITE and (sync_out_snapshot = '1' or sync_in_commit_r = '1') then
				v.changed := data_out_changed;
			end if;

			case r.state is
			when STATE_RESET =>
				v.state := STATE_IDLE;
			when STATE_IDLE =>
				if sync_out_valid_prev = '1' and sync_out_accept_i = '1' then
					if not VAR_WRITE or v.changed = '0' then
						if sync_out_last_prev = '0' then
							v.state := STATE_PASSTHROUGH;
						end if;
					elsif LITTLE_ENDIAN and sync_out_data_prev = x"75" then -- u
						if sync_out_last_prev = '1' then
							v.state := STATE_KEY;
							v.cnt := KEY_BYTES;
						else
							v.state := STATE_WAIT;
						end if;
					elsif not LITTLE_ENDIAN and sync_out_data_prev = x"55" then -- U
						if sync_out_last_prev = '1' then
							v.state := STATE_KEY;
							v.cnt := KEY_BYTES;
						else
							v.state := STATE_WAIT;
						end if;
					elsif sync_out_last_prev = '0' then
						v.state := STATE_PASSTHROUGH;
					end if;
				end if;
			when STATE_WAIT =>
				if sync_out_valid_prev = '1' and sync_out_last_prev = '1' and sync_out_accept_i = '1' then
					v.state := STATE_KEY;
					v.cnt := KEY_BYTES;
				end if;
			when STATE_KEY =>
				if v.cnt = 0 then
					v.state := STATE_LEN;
					v.cnt := KEY_BYTES;
				end if;
			when STATE_LEN =>
				if v.cnt = 0 then
					v.state := STATE_DATA;
					v.cnt := DATA_BYTES;
				end if;
			when STATE_DATA =>
				if v.cnt = 0 then
					v.state := STATE_IDLE;
				end if;
			when STATE_PASSTHROUGH =>
				if sync_out_valid_prev = '1' and sync_out_last_prev = '1' and sync_out_accept_i = '1' then
					v.state := STATE_IDLE;
				end if;
			end case;

			if rstn /= '1' then
				v.state := STATE_RESET;
				v.changed := '0';
				v.cnt := 0;
			end if;

			if not VAR_WRITE then
				v.state := STATE_PASSTHROUGH;
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
			sync_out_accept_prev_i <=
				sync_out_accept_i when STATE_IDLE | STATE_WAIT | STATE_PASSTHROUGH,
				'0' when others;

		sync_out_accept_prev <= sync_out_accept_prev_i;

		process(r, sync_out_data_prev, data_snapshot)
			variable len : std_logic_vector(KEY'length - 1 downto 0);
		begin
			sync_out_data_i <= (others => '-');
			len := (others => '-');

			case r.state is
			when STATE_IDLE | STATE_WAIT | STATE_PASSTHROUGH =>
				sync_out_data_i <= sync_out_data_prev;
			when STATE_KEY =>
				if LITTLE_ENDIAN then
					sync_out_data_i <= KEY(KEY'high - r.cnt * 8 + 8 downto KEY'high - r.cnt * 8 + 1);
				else
					sync_out_data_i <= KEY(r.cnt * 8 - 1 downto r.cnt * 8 - 8);
				end if;
			when STATE_LEN =>
				len := std_logic_vector(to_unsigned(DATA_BYTES, KEY'length));
				if LITTLE_ENDIAN then
					sync_out_data_i <= len(KEY'high - r.cnt * 8 + 8 downto KEY'high - r.cnt * 8 + 1);
				else
					sync_out_data_i <= len(r.cnt * 8 - 1 downto r.cnt * 8 - 8);
				end if;
			when STATE_DATA =>
				if LITTLE_ENDIAN and not BLOB then
					sync_out_data_i <= data_snapshot(data_snapshot'high - r.cnt * 8 + 8 downto data_snapshot'high - r.cnt * 8 + 1);
				else
					sync_out_data_i <= data_snapshot(r.cnt * 8 - 1 downto r.cnt * 8 - 8);
				end if;
			when others => null;
			end case;

			if not VAR_WRITE then
				sync_out_data_i <= sync_out_data_prev;
			end if;
		end process;

		with r.state select
			sync_out_valid_i <=
				sync_out_valid_prev when STATE_IDLE | STATE_WAIT | STATE_PASSTHROUGH,
				'1' when STATE_KEY | STATE_LEN | STATE_DATA,
				'0' when others;

		sync_out_last_i <=
			sync_out_last_prev when not VAR_WRITE else
			'1' when r.state = STATE_DATA and r.cnt = 1 else
			sync_out_last_prev and not r.changed when r.state = STATE_IDLE else
			sync_out_last_prev when r.state = STATE_PASSTHROUGH else
			'0';

		sync_out_buffer_g : if BUFFER_CHAIN generate
			signal sync_out_i, sync_out_r : std_logic_vector(8 downto 0);
		begin
			buffer_inst : entity work.libstored_stream_buffer
				generic map (
					WIDTH => sync_out_i'length
				)
				port map (
					clk => clk,
					rstn => rstn,
					i => sync_out_i,
					i_valid => sync_out_valid_i,
					i_accept => sync_out_accept_i,
					o => sync_out_r,
					o_valid => sync_out_valid,
					o_accept => sync_out_accept
				);

			sync_out_i <= sync_out_last_i & sync_out_data_i;
			sync_out_last <= sync_out_r(8);
			sync_out_data <= sync_out_r(7 downto 0);

			process(clk)
			begin
				if rising_edge(clk) then
					sync_out_have_changes <= sync_out_have_changes_i;
				end if;
			end process;
		end generate;

		sync_out_no_buffer_g : if not BUFFER_CHAIN generate
		begin
			sync_out_have_changes <= sync_out_have_changes_i;
			sync_out_valid <= sync_out_valid_i;
			sync_out_data <= sync_out_data_i;
			sync_out_last <= sync_out_last_i;
			sync_out_accept_i <= sync_out_accept;
		end generate;
	end generate;

--pragma translate_off
	assert DATA_BITS mod 8 = 0 report "Data width not multiple of bytes" severity failure;
	assert KEY'length mod 8 = 0 report "Key width not multiple of bytes" severity failure;
	assert LEN_LENGTH mod 8 = 0 report "Len width not multiple of bytes" severity failure;

	process
	begin
		while true loop
			wait until not VAR_WRITE and rstn = '1' and rising_edge(clk) and data_in_we = '1';
			report "Writing non-writable variable" severity warning;
			wait until rising_edge(clk) and data_in_we = '0';
		end loop;
		wait;
	end process;
--pragma translate_on
end rtl;

