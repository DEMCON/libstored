-- libstored, distributed debuggable data stores.
-- Copyright (C) 2020-2023  Jochem Rutgers
--
-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at https://mozilla.org/MPL/2.0/.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity libstored_droptail is
	generic (
		FIFO_DEPTH : natural := 0;
		TAIL_LENGTH : positive := 1
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		data_in : in std_logic_vector(7 downto 0);
		valid_in : in std_logic;
		last_in : in std_logic;
		accept_in : out std_logic;
		drop : in std_logic := '1';

		data_out : out std_logic_vector(7 downto 0);
		valid_out : out std_logic;
		last_out : out std_logic;
		accept_out : in std_logic
	);
end libstored_droptail;

architecture rtl of libstored_droptail is
	type state_t is (STATE_NORMAL, STATE_DROP, STATE_FORWARD);
	type r_t is record
		state : state_t;
		cnt : natural range 0 to TAIL_LENGTH + 1;
		last : std_logic_vector(1 downto 0);
		last_valid : std_logic;
	end record;

	signal r, r_in : r_t;

	constant DATA_NORMAL : std_logic_vector(1 downto 0) := "00";
	constant DATA_LAST : std_logic_vector(1 downto 0) := "01";
	constant DATA_DROP : std_logic_vector(1 downto 0) := "10";

	signal valid_in_i, valid_out_i, accept_out_i, accept_in_i, last_valid : std_logic;
	signal last_out_i : std_logic_vector(1 downto 0);
begin
	data_fifo_inst : entity work.libstored_fifo
		generic map (
			WIDTH => 8,
			DEPTH => libstored_pkg.maximum(FIFO_DEPTH, TAIL_LENGTH + 1)
		)
		port map (
			clk => clk,
			rstn => rstn,
			i => data_in,
			i_valid => valid_in_i,
			i_accept => accept_in_i,
			o => data_out,
			o_valid => valid_out_i,
			o_accept => accept_out_i
		);

	last_fifo_inst : entity work.libstored_fifo
		generic map (
			WIDTH => 2,
			DEPTH => libstored_pkg.maximum(FIFO_DEPTH, TAIL_LENGTH + 1)
		)
		port map (
			clk => clk,
			rstn => rstn,
			i => r.last,
			i_valid => r.last_valid,
			i_accept => open,
			o => last_out_i,
			o_valid => last_valid,
			o_accept => accept_out_i
		);

	valid_out <=
		valid_out_i and last_valid when last_out_i /= DATA_DROP else
		'0';

	accept_out_i <= valid_out_i and last_valid and accept_out;

	last_out <=
		'1' when last_out_i = DATA_LAST else
		'0';

	with r.state select
		accept_in <=
			accept_in_i when STATE_NORMAL,
			'0' when others;

	with r.state select
		valid_in_i <=
			valid_in when STATE_NORMAL,
			'0' when others;

	process(r, rstn, accept_in_i, valid_in, last_in, drop)
		variable v : r_t;
	begin
		v := r;

		v.last := (others => '-');
		v.last_valid := '0';

		case r.state is
		when STATE_NORMAL =>
			if valid_in = '1' and accept_in_i = '1' then
				v.cnt := r.cnt + 1;
				if last_in = '1' then
					if drop = '1' then
						-- Got whole message, but drop tail.
						v.state := STATE_DROP;
					else
						-- Forward whole message.
						v.state := STATE_FORWARD;
					end if;
				elsif r.cnt = TAIL_LENGTH then
					if last_in = '0' then
						-- No end of message, next byte is not part of the tail.
						v.last := DATA_NORMAL;
						v.last_valid := '1';
						v.cnt := r.cnt;
					end if;
				end if;
			end if;
		when STATE_DROP =>
			v.last_valid := '1';
			v.cnt := r.cnt - 1;
			case v.cnt is
			when 1 =>
				v.last := DATA_LAST;
			when 0 =>
				v.last := DATA_DROP;
				v.state := STATE_NORMAL;
			when others =>
				v.last := DATA_NORMAL;
			end case;
		when STATE_FORWARD =>
			v.last_valid := '1';
			v.cnt := r.cnt - 1;
			case v.cnt is
			when 0 =>
				v.last := DATA_LAST;
				v.state := STATE_NORMAL;
			when others =>
				v.last := DATA_NORMAL;
			end case;
		end case;

		if rstn /= '1' then
			v.state := STATE_NORMAL;
			v.cnt := 0;
		end if;

		r_in <= v;
	end process;

	process(clk)
	begin
		if rising_edge(clk) then
			r <= r_in;
		end if;
	end process;

end rtl;




library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity SegmentationLayer is
	generic (
		MTU : positive
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		encode_in : in libstored_pkg.msg_t;
		encode_out : out libstored_pkg.msg_t;

		decode_in : in libstored_pkg.msg_t;
		decode_out : out libstored_pkg.msg_t;

		idle : out std_logic
	);
end SegmentationLayer;

architecture rtl of SegmentationLayer is
begin
	-- TODO
--pragma translate_off
	assert not (rising_edge(clk) and rstn /= '1')
		report "Not implemented" severity failure;
--pragma translate_on
end rtl;




library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity ArqLayer is
	generic (
		SYSTEM_CLK_FREQ : integer := 100e6;
		ACK_TIMEOUT_s : real := 0.1;
		SIMULATION : boolean := false
--pragma translate_off
			or true
--pragma translate_on
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		encode_in : in libstored_pkg.msg_t;
		encode_out : out libstored_pkg.msg_t;

		decode_in : in libstored_pkg.msg_t;
		decode_out : out libstored_pkg.msg_t;

		idle : out std_logic
	);
end ArqLayer;

architecture rtl of ArqLayer is
begin
	-- TODO
--pragma translate_off
	assert not (rising_edge(clk) and rstn /= '1')
		report "Not implemented" severity failure;
--pragma translate_on
end rtl;




library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity Crc8Layer is
	port (
		clk : in std_logic;
		rstn : in std_logic;

		encode_in : in libstored_pkg.msg_t;
		encode_out : out libstored_pkg.msg_t;

		decode_in : in libstored_pkg.msg_t;
		decode_out : out libstored_pkg.msg_t;

		idle : out std_logic
	);
end Crc8Layer;

architecture rtl of Crc8Layer is
begin
	-- TODO
--pragma translate_off
	assert not (rising_edge(clk) and rstn /= '1')
		report "Not implemented" severity failure;
--pragma translate_on
end rtl;




library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity Crc16Layer is
	port (
		clk : in std_logic;
		rstn : in std_logic;

		encode_in : in libstored_pkg.msg_t;
		encode_out : out libstored_pkg.msg_t;

		decode_in : in libstored_pkg.msg_t;
		decode_out : out libstored_pkg.msg_t;

		idle : out std_logic
	);
end CRC16Layer;

architecture rtl of CRC16Layer is
begin
	-- TODO
--pragma translate_off
	assert not (rising_edge(clk) and rstn /= '1')
		report "Not implemented" severity failure;
--pragma translate_on
end rtl;





library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity AsciiEscapeLayer is
	generic (
		ESCAPE_ALL : boolean := false;
		ENCODE_OUT_FIFO_DEPTH : natural := 0;
		DECODE_OUT_FIFO_DEPTH : natural := 0
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		encode_in : in libstored_pkg.msg_t;
		encode_out : out libstored_pkg.msg_t;

		decode_in : in libstored_pkg.msg_t;
		decode_out : out libstored_pkg.msg_t;

		idle : out std_logic
	);
end AsciiEscapeLayer;

architecture rtl of AsciiEscapeLayer is
begin
	encode_g : if true generate
		type state_t is (STATE_DATA, STATE_ESC);
		type r_t is record
			state : state_t;
			esc_data : std_logic_vector(7 downto 0);
			postpone_last : std_logic;
		end record;

		signal r, r_in : r_t;

		signal encode_out_i, encode_out_o : std_logic_vector(8 downto 0);
		signal encode_out_data : std_logic_vector(7 downto 0);
		signal encode_out_valid, encode_out_accept, encode_out_last : std_logic;
	begin
		encode_out_fifo_inst : entity work.libstored_fifo
			generic map (
				WIDTH => 9,
				DEPTH => ENCODE_OUT_FIFO_DEPTH
			)
			port map (
				clk => clk,
				rstn => rstn,
				i => encode_out_i,
				i_valid => encode_out_valid,
				i_accept => encode_out_accept,
				o => encode_out_o,
				o_valid => encode_out.valid,
				o_accept => decode_in.accept
			);

		encode_out_i <= encode_out_last & encode_out_data;
		encode_out.data <= encode_out_o(7 downto 0);
		encode_out.last <= encode_out_o(8);

		process(r, rstn, encode_in.data, encode_in.valid, encode_in.last, encode_out_accept)
			variable v : r_t;
		begin
			v := r;

			case r.state is
			when STATE_DATA =>
				if encode_in.valid = '1' then
					if ESCAPE_ALL then
						if unsigned(encode_in.data) < 32 then
							v.state := STATE_ESC;
							v.esc_data := encode_in.data or x"40";
						elsif encode_in.data = x"7f" then
							v.state := STATE_ESC;
							v.esc_data := x"7f";
						end if;
					else
						case encode_in.data is
						when  x"00" -- \0
							| x"11" -- XON
							| x"13" -- XOFF
							| x"1b" -- ESC
							| x"0d" => -- \r
							v.state := STATE_ESC;
							v.esc_data := encode_in.data or x"40";
						when x"7f" =>
							v.state := STATE_ESC;
							v.esc_data := x"7f";
						when others => null;
						end case;
					end if;
				end if;

				if v.state = STATE_ESC then
					v.postpone_last := encode_in.last;
				else
					v.postpone_last := '0';
				end if;
			when STATE_ESC =>
				v.state := STATE_DATA;
			end case;

			r_in <= v;
		end process;

		process(clk)
		begin
			if rising_edge(clk) then
				if rstn /= '1' then
					r.state <= STATE_DATA;
					r.postpone_last <= '0';
				elsif encode_out_accept = '1' then
					r <= r_in;
				end if;
			end if;
		end process;

		encode_out_data <=
			encode_in.data when r.state = STATE_DATA and r_in.state = STATE_DATA else
			x"7f" when r.state = STATE_DATA else -- switching to STATE_ESC
			r.esc_data; -- in STATE_ESC

		with r.state select
			encode_out_last <=
				encode_in.last and not r_in.postpone_last when STATE_DATA,
				r.postpone_last when STATE_ESC;

		with r.state select
			encode_out_valid <=
				encode_in.valid when STATE_DATA,
				'1' when STATE_ESC;

		with r.state select
			decode_out.accept <=
				encode_out_accept when STATE_DATA,
				'0' when others;

	end generate;

	decode_g : if true generate
		type state_t is (STATE_DATA, STATE_ESC);
		type r_t is record
			state : state_t;
		end record;

		signal r, r_in : r_t;

		signal decode_out_i, decode_out_o : std_logic_vector(8 downto 0);
		signal decode_out_data : std_logic_vector(7 downto 0);
		signal decode_out_valid, decode_out_accept, decode_out_last, decode_out_drop : std_logic;
		signal decode_in_is_esc : boolean;
	begin
		decode_out_fifo_inst : entity work.libstored_droptail
			generic map (
				FIFO_DEPTH => DECODE_OUT_FIFO_DEPTH,
				TAIL_LENGTH => 1 -- Only x"0d" may be removed.
			)
			port map (
				clk => clk,
				rstn => rstn,
				data_in => decode_out_data,
				last_in => decode_out_last,
				valid_in => decode_out_valid,
				accept_in => decode_out_accept,
				drop => decode_out_drop,
				data_out => decode_out.data,
				valid_out => decode_out.valid,
				last_out => decode_out.last,
				accept_out => encode_in.accept
			);

		process(r, rstn, decode_in.valid, decode_in.data, decode_in.last, encode_in.accept, decode_out_accept, decode_in_is_esc)
			variable v : r_t;
		begin
			v := r;

			case r.state is
			when STATE_DATA =>
				if decode_in.valid = '1' and decode_out_accept = '1' then
					if decode_in_is_esc and decode_in.last = '0' then
						v.state := STATE_ESC;
					end if;
				end if;
			when STATE_ESC =>
				if decode_in.valid = '1' and decode_out_accept = '1' then
					v.state := STATE_DATA;
				end if;
			end case;

			if rstn /= '1' then
				v.state := STATE_DATA;
			end if;

			r_in <= v;
		end process;

		process(clk)
		begin
			if rising_edge(clk) then
				r <= r_in;
			end if;
		end process;

		decode_in_is_esc <= decode_in.data = x"7f";

		decode_out_data <=
			decode_in.data when decode_in_is_esc else
			decode_in.data and x"1f" when r.state = STATE_ESC else
			decode_in.data;

		decode_out_valid <=
			decode_in.valid and decode_in.last when decode_in.data = x"0d" else -- the last is valid, but will be dropped
			'0' when r.state = STATE_DATA and decode_in_is_esc and decode_in.last = '0' else
			decode_in.valid;

		decode_out_last <= decode_in.last;

		encode_out.accept <=
			decode_out_accept;

		decode_out_drop <=
			decode_in.last when decode_in.data = x"0d" else
			'0';

	end generate;

end rtl;




library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity BufferLayer is
	generic (
		ENCODE_DEPTH : natural := 1;
		DECODE_DEPTH : natural := 1
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		encode_in : in libstored_pkg.msg_t := libstored_pkg.msg_term;
		encode_out : out libstored_pkg.msg_t;

		decode_in : in libstored_pkg.msg_t := libstored_pkg.msg_term;
		decode_out : out libstored_pkg.msg_t;

		idle : out std_logic
	);
end BufferLayer;

architecture rtl of BufferLayer is
	signal encode_empty, decode_empty : std_logic;
begin

	encode_buf_g : if ENCODE_DEPTH > 0 generate
		signal encode_i, encode_o : std_logic_vector(8 downto 0);
	begin
		encode_fifo_inst : entity work.libstored_fifo
			generic map (
				WIDTH => 9,
				DEPTH => ENCODE_DEPTH
			)
			port map (
				clk => clk,
				rstn => rstn,
				i => encode_i,
				i_valid => encode_in.valid,
				i_accept => decode_out.accept,
				o => encode_o,
				o_valid => encode_out.valid,
				o_accept => decode_in.accept,
				empty => encode_empty
			);

		encode_i <= encode_in.last & encode_in.data;
		encode_out.data <= encode_o(7 downto 0);
		encode_out.last <= encode_o(8);
	end generate;

	encode_nobuf_g : if ENCODE_DEPTH = 0 generate
	begin
		encode_out.data <= encode_in.data;
		encode_out.last <= encode_in.last;
		encode_out.valid <= encode_in.valid;
		decode_out.accept <= decode_in.accept;

		process(clk)
		begin
			if rising_edge(clk) then
				encode_empty <= not encode_in.valid;
			end if;
		end process;
	end generate;

	decode_buf_g : if DECODE_DEPTH > 0 generate
		signal decode_i, decode_o : std_logic_vector(8 downto 0);
	begin
		decode_fifo_inst : entity work.libstored_fifo
			generic map (
				WIDTH => 9,
				DEPTH => DECODE_DEPTH
			)
			port map (
				clk => clk,
				rstn => rstn,
				i => decode_i,
				i_valid => decode_in.valid,
				i_accept => encode_out.accept,
				o => decode_o,
				o_valid => decode_out.valid,
				o_accept => encode_in.accept,
				empty => decode_empty
			);

		decode_i <= decode_in.last & decode_in.data;
		decode_out.data <= decode_o(7 downto 0);
		decode_out.last <= decode_o(8);
	end generate;

	decode_nobuf_g : if DECODE_DEPTH = 0 generate
	begin
		decode_out.data <= decode_in.data;
		decode_out.last <= decode_in.last;
		decode_out.valid <= decode_in.valid;
		encode_out.accept <= encode_in.accept;

		process(clk)
		begin
			if rising_edge(clk) then
				decode_empty <= not decode_in.valid;
			end if;
		end process;
	end generate;

	idle <= encode_empty and decode_empty;
end rtl;




library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity TerminalLayer is
	generic (
		ENCODE_OUT_FIFO_DEPTH : natural := 0;
		DECODE_IN_FIFO_DEPTH : natural := 0;
		DECODE_OUT_FIFO_DEPTH : natural := 1;
		TERMINAL_OUT_FIFO_DEPTH : natural := 1
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		encode_in : in libstored_pkg.msg_t;
		encode_out : out libstored_pkg.msg_t;

		decode_in : in libstored_pkg.msg_t;
		decode_out : out libstored_pkg.msg_t;

		terminal_in : in libstored_pkg.msg_t := libstored_pkg.msg_term;
		terminal_out : out libstored_pkg.msg_t;

		idle : out std_logic
	);
end TerminalLayer;

architecture rtl of TerminalLayer is
	constant MSG_ESC : std_logic_vector(7 downto 0) := x"1b";
	constant MSG_START : std_logic_vector(7 downto 0) := x"5f"; -- '_'
	constant MSG_END : std_logic_vector(7 downto 0) := x"5c";   -- '\'

	signal encode_idle, decode_idle : std_logic;
begin

	encode_g : if true generate
		type state_t is (STATE_RESET, STATE_IDLE, STATE_START_ESC, STATE_START,
			STATE_MSG, STATE_END_ESC, STATE_END);
		type r_t is record
			state : state_t;
		end record;
		signal r, r_in : r_t;

		signal encode_out_data : std_logic_vector(7 downto 0);
		signal encode_out_valid, encode_out_accept : std_logic;
	begin

		process(r, rstn, encode_in.valid, encode_in.last, encode_out_accept)
			variable v : r_t;
		begin
			v := r;

			case r.state is
			when STATE_RESET =>
				v.state := STATE_IDLE;
			when STATE_IDLE =>
				if encode_in.valid = '1' then
					v.state := STATE_START_ESC;
				end if;
			when STATE_START_ESC =>
				if encode_out_accept = '1' then
					v.state := STATE_START;
				end if;
			when STATE_START =>
				if encode_out_accept = '1' then
					v.state := STATE_MSG;
				end if;
			when STATE_MSG =>
				if encode_out_accept = '1' and encode_in.valid = '1' and encode_in.last = '1' then
					v.state := STATE_END_ESC;
				end if;
			when STATE_END_ESC =>
				if encode_out_accept = '1' then
					v.state := STATE_END;
				end if;
			when STATE_END =>
				if encode_out_accept = '1' then
					v.state := STATE_IDLE;
				end if;
			end case;

			if rstn /= '1' then
				v.state := STATE_RESET;
			end if;

			r_in <= v;
		end process;

		with r.state select
			encode_out_data <=
				MSG_ESC when STATE_START_ESC | STATE_END_ESC,
				MSG_START when STATE_START,
				MSG_END when STATE_END,
				encode_in.data when STATE_MSG,
				terminal_in.data when others;

		with r.state select
			encode_out_valid <=
				'1' when STATE_START_ESC | STATE_START | STATE_END_ESC | STATE_END,
				encode_in.valid when STATE_MSG,
				terminal_in.valid when others;

		with r.state select
			decode_out.accept <=
				encode_out_accept when STATE_MSG,
				'0' when others;

		with r.state select
			terminal_out.accept <=
				encode_out_accept when STATE_IDLE,
				'0' when others;

		fifo_encode_out_inst : entity work.libstored_fifo
			generic map (
				WIDTH => 8,
				DEPTH => ENCODE_OUT_FIFO_DEPTH
			)
			port map (
				clk => clk,
				rstn => rstn,
				i => encode_out_data,
				i_valid => encode_out_valid,
				i_accept => encode_out_accept,
				o => encode_out.data,
				o_valid => encode_out.valid,
				o_accept => decode_in.accept
			);

		encode_out.last <= '0';

		process(clk)
		begin
			if rising_edge(clk) then
				r <= r_in;
			end if;
		end process;

		encode_idle <= '1' when r.state = STATE_IDLE else '0';
	end generate;

	decode_g : if true generate
		type state_t is (STATE_RESET, STATE_IDLE, STATE_START_ESC,
			STATE_MSG, STATE_END_ESC, STATE_FLUSH);
		type r_t is record
			state : state_t;
			commit_decode : std_logic;
			rollback_decode : std_logic;
			commit_terminal : std_logic;
			rollback_terminal : std_logic;
			drop : std_logic;
		end record;
		signal r, r_in : r_t;

		signal decode_in_valid, decode_in_accept : std_logic;
		signal decode_out_valid, decode_out_accept, decode_out_last : std_logic;
		signal terminal_out_valid, terminal_out_accept : std_logic;
		signal decode_in_data : std_logic_vector(7 downto 0);
		signal decode_out_empty, decode_out_valid_o, decode_out_accept_o : std_logic;
	begin

		fifo_decode_in_inst : entity work.libstored_fifo
			generic map (
				WIDTH => 8,
				DEPTH => DECODE_IN_FIFO_DEPTH
			)
			port map (
				clk => clk,
				rstn => rstn,
				i => decode_in.data,
				i_valid => decode_in.valid,
				i_accept => encode_out.accept,
				o => decode_in_data,
				o_valid => decode_in_valid,
				o_accept => decode_in_accept
			);

		fifo_decode_out_inst : entity work.libstored_fifo
			generic map (
				WIDTH => 8,
				DEPTH => libstored_pkg.maximum(2, DECODE_OUT_FIFO_DEPTH),
				ALMOST_EMPTY_REMAINING => 1
			)
			port map (
				clk => clk,
				rstn => rstn,
				i => decode_in_data,
				i_valid => decode_out_valid,
				i_accept => decode_out_accept,
				i_commit => r_in.commit_decode,
				i_rollback => r_in.rollback_decode,
				o => decode_out.data,
				o_valid => decode_out_valid_o,
				o_accept => decode_out_accept_o,
				almost_empty => decode_out_last,
				empty => decode_out_empty
			);

		fifo_terminal_out_inst : entity work.libstored_fifo
			generic map (
				WIDTH => 8,
				DEPTH => TERMINAL_OUT_FIFO_DEPTH
			)
			port map (
				clk => clk,
				rstn => rstn,
				i => decode_in_data,
				i_valid => terminal_out_valid,
				i_accept => terminal_out_accept,
				i_commit => r_in.commit_terminal,
				i_rollback => r_in.rollback_terminal,
				o => terminal_out.data,
				o_valid => terminal_out.valid,
				o_accept => terminal_in.accept
			);

		process(r, rstn, decode_in_valid, decode_in_accept, decode_in_data, decode_out_empty)
			variable v : r_t;
		begin
			v := r;

			v.rollback_decode := '0';
			v.rollback_terminal := '0';
			v.commit_decode := '0';
			v.commit_terminal := '0';
			v.drop := '0';

			case r.state is
			when STATE_RESET =>
				v.state := STATE_IDLE;
			when STATE_IDLE =>
				if decode_in_valid = '1' and decode_in_accept = '1' and decode_in_data = MSG_ESC then
					v.state := STATE_START_ESC;
				else
					v.commit_terminal := '1';
				end if;
			when STATE_START_ESC =>
				if decode_in_valid = '1' then
					if decode_in_data = MSG_START then
						v.state := STATE_MSG;
						v.rollback_terminal := '1';
						v.drop := '1';
					else
						v.state := STATE_IDLE;
						v.commit_terminal := '1';
					end if;
				end if;
			when STATE_MSG =>
				if decode_in_valid = '1' and decode_in_accept = '1' then
					if decode_in_data = MSG_ESC then
						v.state := STATE_END_ESC;
					else
						v.commit_decode := '1';
					end if;
				end if;
			when STATE_END_ESC =>
				if decode_in_valid = '1' then
					if decode_in_data = MSG_END then
						v.state := STATE_FLUSH;
						v.rollback_decode := '1';
						v.drop := '1';
					else
						v.state := STATE_MSG;
						v.commit_decode := '1';
					end if;
				end if;
			when STATE_FLUSH =>
				if decode_out_empty = '1' then
					v.state := STATE_IDLE;
				end if;
			end case;

			if rstn /= '1' then
				v.state := STATE_RESET;
			end if;

			r_in <= v;
		end process;

		with r.state select
			decode_out_valid <=
				decode_in_valid when STATE_MSG,
				'0' when others;

		with r.state select
			terminal_out_valid <=
				decode_in_valid when STATE_IDLE,
				'0' when others;

		with r.state select
			decode_in_accept <=
				terminal_out_accept when STATE_IDLE,
				decode_out_accept when STATE_MSG,
				r_in.drop when STATE_START_ESC | STATE_END_ESC,
				'0' when others;

		with r.state select
			decode_out.last <=
				decode_out_last when STATE_FLUSH,
				'0' when others;

		with r.state select
			decode_out.valid <=
				decode_out_valid_o when STATE_FLUSH,
				decode_out_valid_o and not decode_out_last when others;

		with r.state select
			decode_out_accept_o <=
				encode_in.accept when STATE_FLUSH,
				encode_in.accept and not decode_out_last when others;

		terminal_out.last <= '0';

		process(clk)
		begin
			if rising_edge(clk) then
				r <= r_in;
			end if;
		end process;

		decode_idle <= '1' when r.state = STATE_IDLE else '0';
	end generate;

	idle <= encode_idle and decode_idle;

end rtl;




library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity UARTLayer is
	generic (
		SYSTEM_CLK_FREQ : integer := 100e6;

		-- Baud rate. When set to 0, do auto-baud. For this, the first
		-- received character is used to determine the baud rate. For this,
		-- the time between the first and the third rising edge is used as
		-- the byte length. XON, XOFF, ESC are all valid for this detection.
		-- To restart auto-baud, send a break (or any other character
		-- with invalid stop bit).
		BAUD : natural := 0;
		AUTO_BAUD_MINIMUM : positive := 9600;

		DECODE_OUT_FIFO_DEPTH : natural := 0;

		-- Only enable XON/XOFF when there are no bit flips possible.
		-- Otherwise, one could falsely see XOFF and suspend transmission.
		-- The UARTLayer sends an XON when receiving the XOFF-XON-XON sequence,
		-- which may solve the deadlock, but this mechanism may not be fool proof.
		-- Moreover, always use the AsciiEscapeLayer to escape XON/XOFF in the data.
		XON_XOFF : boolean := false;

		-- Minimum number of bytes that can be received after sending XOFF.
		XOFF_SPARE : natural := 0
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		encode_in : in libstored_pkg.msg_t;
		decode_out : out libstored_pkg.msg_t;

		rx : in std_logic;
		tx : out std_logic;
		cts : in std_logic := '0';
		rts : out std_logic;

		idle : out std_logic;
		bit_clk : out natural
	);
end UARTLayer;

architecture rtl of UARTLayer is
	constant XON : std_logic_vector(7 downto 0) := x"11";
	constant XOFF : std_logic_vector(7 downto 0) := x"13";

	impure function calc_decode_out_fifo_almost_full return natural is
	begin
		if XON_XOFF then
			-- When this threshold is hit, we should finish the current TX,
			-- send XOFF, which should be received, but the other party might
			-- be currently sending a byte when it parses the XOFF.
			-- So, we need at least 3, but allow for a bit more.
			return 6 + XOFF_SPARE;
		else
			-- In case of RTS/CTS, hardware flow control is supposed to
			-- react immediately. So, it only finishes the current byte
			-- that is in transmission. Just add room for one more.
			return 2;
		end if;
	end function;

	impure function calc_decode_out_fifo_depth return natural is
	begin
		if XON_XOFF then
			-- XON/XOFF is slow and possibly expensive. So, a large
			-- buffer would be nice.
			return libstored_pkg.maximum(DECODE_OUT_FIFO_DEPTH, calc_decode_out_fifo_almost_full + 6);
		else
			return libstored_pkg.maximum(DECODE_OUT_FIFO_DEPTH, 4);
		end if;
	end function;

	impure function max_bit_duration return natural is
	begin
		if BAUD = 0 then
			return libstored_pkg.maximum(1, integer(real(SYSTEM_CLK_FREQ) / real(AUTO_BAUD_MINIMUM)));
		else
			return libstored_pkg.maximum(1, integer(real(SYSTEM_CLK_FREQ) / real(BAUD)));
		end if;
	end function;

	impure function max_byte_duration return natural is
	begin
		return max_bit_duration * 10;
	end function;

	signal auto_baud_valid : std_logic;
	signal auto_bit_duration : natural range 0 to max_bit_duration;

	impure function bit_duration return natural is
	begin
		if BAUD = 0 then
			if auto_baud_valid = '1' then
				return auto_bit_duration;
			else
				return 0;
			end if;
		else
			return libstored_pkg.maximum(1, integer(real(SYSTEM_CLK_FREQ) / real(BAUD)));
		end if;
	end function;

	impure function half_bit_duration return natural is
	begin
		return bit_duration / 2;
	end function;

	impure function quarter_bit_duration return natural is
	begin
		return half_bit_duration / 2;
	end function;

	signal rx_i, cts_i : std_logic;
	signal rx_valid, rx_accept, rx_almost_full, rx_empty : std_logic;
	signal rx_data : std_logic_vector(7 downto 0);
	signal rx_idle, tx_idle : std_logic;
	signal rx_pause, tx_pause, tx_xon : std_logic;
begin

	rx_meta_inst : entity work.libstored_metastabilize
		generic map (
			DELAY => 2
		)
		port map (
			clk => clk,
			i => rx,
			o => rx_i
		);

	cts_meta_inst : entity work.libstored_metastabilize
		port map (
			clk => clk,
			i => cts,
			o => cts_i
		);

	decode_out_fifo_inst : entity work.libstored_fifo
		generic map (
			WIDTH => 8,
			DEPTH => calc_decode_out_fifo_depth,
			ALMOST_FULL_REMAINING => calc_decode_out_fifo_almost_full
		)
		port map (
			clk => clk,
			rstn => rstn,
			i => rx_data,
			i_valid => rx_valid,
			i_accept => rx_accept,
			o => decode_out.data,
			o_valid => decode_out.valid,
			o_accept => encode_in.accept,
			almost_full => rx_almost_full,
			empty => rx_empty
		);

	rx_pause <= rx_almost_full;
	decode_out.last <= '0';
	idle <= rx_idle and tx_idle and rx_empty;

	tx_g : if true generate
		type state_t is (STATE_RESET, STATE_IDLE, STATE_START, STATE_DATA, STATE_STOP);
		type r_t is record
			state : state_t;
			cnt : natural range 0 to max_bit_duration;
			len : natural range 0 to 7;
			data : std_logic_vector(7 downto 0);
			accept : std_logic;
			tx : std_logic;
			idle : std_logic;
			rx_paused : std_logic;
			tx_paused : std_logic;
			tx_xon : std_logic;
		end record;
		signal r, r_in : r_t;
	begin
		process(r, rstn, encode_in.data, encode_in.valid, encode_in.last, cts_i, rx_pause, tx_pause, tx_xon, auto_baud_valid, auto_bit_duration)
			variable v : r_t;
		begin
			v := r;

			if r.cnt > 0 then
				v.cnt := r.cnt - 1;
			end if;

			v.accept := '0';
			v.tx_xon := r.tx_xon or tx_xon;

			case r.state is
			when STATE_RESET =>
				v.state := STATE_IDLE;
			when STATE_IDLE =>
				if auto_baud_valid = '1' and cts_i = '0' and tx_pause = '0' then
					if XON_XOFF and r.rx_paused = '0' and rx_pause = '1' then
						-- Need to send XOFF now
						v.rx_paused := '1';
						v.data := XOFF;
						v.state := STATE_START;
					elsif XON_XOFF and (r.rx_paused = '1' or v.tx_xon = '1') and rx_pause = '0' then
						-- Let's send XON first
						v.rx_paused := '0';
						v.tx_xon := '0';
						v.data := XON;
						v.state := STATE_START;
					elsif encode_in.valid = '1' then
						v.data := encode_in.data;
						v.accept := '1';
						v.state := STATE_START;
					end if;

					if v.state = STATE_START then
						v.cnt := bit_duration;
						v.len := 7;
						v.idle := '0';
						v.tx := '0';
					end if;
				end if;
			when STATE_START =>
				if v.cnt = 0 then
					v.state := STATE_DATA;
					v.cnt := bit_duration;
					v.tx := r.data(0);
				end if;
			when STATE_DATA =>
				if v.cnt = 0 then
					v.cnt := bit_duration;
					v.data := '-' & v.data(7 downto 1);
					if r.len = 0 then
						v.state := STATE_STOP;
						v.tx := '1';
					else
						v.len := r.len - 1;
						v.tx := r.data(1);
					end if;
				end if;
			when STATE_STOP =>
				if v.cnt = 0 then
					v.state := STATE_IDLE;
				end if;
			end case;

			if encode_in.last = '1' and encode_in.valid = '1' and v.accept = '1' then
				v.idle := '1';
			end if;

			if rstn /= '1' then
				v.state := STATE_RESET;
				v.tx := '1';
				v.idle := '1';
				v.rx_paused := '0';
				v.tx_paused := '0';
				v.tx_xon := '0';
			end if;

			r_in <= v;
		end process;

		tx <= r.tx;
		decode_out.accept <= r.accept;

		rx_idle <= r.idle when r.state = STATE_IDLE else '0';

		process(clk)
		begin
			if rising_edge(clk) then
				r <= r_in;
			end if;
		end process;
	end generate;

	rx_g : if true generate
		constant SYNC_DURATION : natural := max_byte_duration * 2;
		constant CNT_BITS : natural := libstored_pkg.bits(SYNC_DURATION);
		constant CNT_MAX : natural := 2**CNT_BITS - 1;
		type state_t is (
			STATE_RESET, STATE_SYNC,
			STATE_AUTO_BAUD, STATE_EDGE_1, STATE_HIGH_1, STATE_EDGE_2, STATE_HIGH_2, STATE_EDGE_3, STATE_AUTO_BAUD_CHECK,
			STATE_IDLE, STATE_EDGE, STATE_START, STATE_DATA, STATE_STOP);
		type r_t is record
			state : state_t;
			cnt : natural range 0 to CNT_MAX;
			len : natural range 0 to 7;
			data : std_logic_vector(7 downto 0);
			valid : std_logic;
			rts : std_logic;
			tx_paused : std_logic;
			tx_xon : std_logic;
			tx_xon_suppress : std_logic;
			auto_baud : natural range 0 to CNT_MAX;
			auto_baud_valid : std_logic;
		end record;
		signal r, r_in : r_t;
	begin
		process(r, rstn, rx_i, rx_almost_full, auto_baud_valid, auto_bit_duration, rx_pause)
			variable v : r_t;
		begin
			v := r;

			if r.cnt > 0 then
				v.cnt := r.cnt - 1;
			end if;

			v.valid := '0';
			v.rts := rx_almost_full;
			v.tx_xon := '0';

			case r.state is
			when STATE_RESET =>
				v.state := STATE_SYNC;
				v.cnt := SYNC_DURATION;
				v.tx_paused := '0';
				v.tx_xon_suppress := '0';
			when STATE_SYNC =>
				if rx_i /= '1' then
					-- Must be high for a while.
					v.state := STATE_RESET;
				elsif v.cnt = 0 then
					if BAUD = 0 and r.auto_baud_valid = '0' then
						v.state := STATE_AUTO_BAUD;
					else
						v.state := STATE_IDLE;
					end if;
				end if;
			when STATE_AUTO_BAUD =>
				if rx_i = '0' then
					-- falling edge
					v.cnt := max_bit_duration;
					v.state := STATE_EDGE_1;
				end if;
			when STATE_EDGE_1 =>
				if v.cnt = 0 then
					-- Took too long. Abort.
					v.state := STATE_SYNC;
				elsif rx_i = '1' then
					-- First rising edge, start measuring.
					v.cnt := CNT_MAX - 4; -- -4 for rounding to auto_bit_duration later on
					v.state := STATE_HIGH_1;
				end if;
			when STATE_HIGH_1 =>
				if v.cnt = 0 then
					v.state := STATE_SYNC;
				elsif rx_i = '0' then
					v.state := STATE_EDGE_2;
				end if;
			when STATE_EDGE_2 =>
				if v.cnt = 0 then
					v.state := STATE_SYNC;
				elsif rx_i = '1' then
					v.state := STATE_HIGH_2;
				end if;
			when STATE_HIGH_2 =>
				if v.cnt = 0 then
					v.state := STATE_SYNC;
				elsif rx_i = '0' then
					v.state := STATE_EDGE_3;
				end if;
			when STATE_EDGE_3 =>
				if v.cnt = 0 then
					v.state := STATE_SYNC;
				elsif rx_i = '1' then
					-- Got it.
					v.auto_baud := to_integer(not to_unsigned(v.cnt, CNT_BITS));
					v.state := STATE_AUTO_BAUD_CHECK;
				end if;
			when STATE_AUTO_BAUD_CHECK =>
				if r.auto_baud > max_byte_duration then
					v.state := STATE_SYNC;
				else
					v.auto_baud_valid := '1';
					v.state := STATE_IDLE;
				end if;
			when STATE_IDLE =>
				if rx_i = '0' then
					-- rx must be low for a quarter of a bit, otherwise
					-- it is discarded as a glitch.
					v.cnt := quarter_bit_duration;
					v.state := STATE_EDGE;
				end if;
			when STATE_EDGE =>
				if v.cnt = 0 then
					if BAUD = 0 then
						v.cnt := quarter_bit_duration;
					else
						-- On a perfect edge, we sync at sampling the bit exactly
						-- halve way. If there is some noise at the edge,
						-- this should be solved after a quarter of a bit, otherwise
						-- the sample time gets close to the edge of the next bit.
						v.cnt := libstored_pkg.maximum(0, half_bit_duration - quarter_bit_duration);
					end if;

					v.state := STATE_START;
				elsif rx_i = '1' then
					-- Was probably just a glitch.
					v.state := STATE_IDLE;
				end if;
			when STATE_START =>
				if v.cnt = 0 then
					v.state := STATE_DATA;
					v.cnt := bit_duration;
					v.len := 7;
				end if;
			when STATE_DATA =>
				if v.cnt = 0 then
					v.cnt := bit_duration;
					v.data := rx_i & v.data(7 downto 1);
					if r.len = 0 then
						v.state := STATE_STOP;
					else
						v.len := r.len - 1;
					end if;
				end if;
			when STATE_STOP =>
				if v.cnt = 0 then
					if rx_i = '1' then
						v.state := STATE_IDLE;

						if XON_XOFF and r.data = XON then
							if r.tx_paused = '0' and rx_pause = '0' and r.tx_xon_suppress = '0' then
								-- Not paused, just send an XON to make sure that
								-- we did not send an XOFF by accident (e.g., due to bit flips).
								v.tx_xon := '1';
								v.tx_xon_suppress := '1';
							end if;

							v.tx_paused := '0';
						elsif XON_XOFF and r.data = XOFF then
							v.tx_paused := '1';
							v.tx_xon_suppress := '0';
						else
							v.valid := '1';
						end if;
					else
--pragma translate_off
						report "Invalid stop bit" severity warning;
						v.data := (others => '-');
--pragma translate_on
						v.state := STATE_RESET;
						v.auto_baud_valid := '0';
					end if;
				end if;
			end case;

			if rstn /= '1' then
				v.state := STATE_RESET;
				v.rts := '1';
				v.tx_xon_suppress := '0';
				v.auto_baud_valid := '0';
			end if;

			r_in <= v;
		end process;

		rx_data <= r.data;
		rx_valid <= r.valid;
		rts <= r.rts;

		tx_idle <= '1' when r.state = STATE_IDLE else '0';
		tx_pause <= r.tx_paused;
		tx_xon <= r.tx_xon;

		auto_baud_valid <= '1' when BAUD > 0 else r.auto_baud_valid;
		auto_bit_duration <= bit_duration when BAUD > 0 else r.auto_baud / 8;

		process(auto_baud_valid, auto_bit_duration)
		begin
			bit_clk <= bit_duration;
		end process;

--pragma translate_off
		assert not (rising_edge(clk) and rx_valid = '1' and rx_accept = '0')
			report "Fifo overflow; flow control disfunctioning" severity warning;
--pragma translate_on

		process(clk)
		begin
			if rising_edge(clk) then
				r <= r_in;
			end if;
		end process;
	end generate;

end rtl;

