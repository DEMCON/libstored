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

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity AsciiEscapeLayer is
	generic (
		ESCAPE_ALL : boolean := false
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

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity BufferLayer is
	generic (
		DEPTH : positive := 1
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
end BufferLayer;



library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity TerminalLayer is
	port (
		clk : in std_logic;
		rstn : in std_logic;

		encode_in : in libstored_pkg.msg_t;
		encode_out : out libstored_pkg.msg_t;

		decode_in : in libstored_pkg.msg_t;
		decode_out : out libstored_pkg.msg_t;

		terminal_in : in libstored_pkg.msg_t;
		terminal_out : out libstored_pkg.msg_t;

		idle : out std_logic
	);
end TerminalLayer;

architecture rtl of TerminalLayer is
	constant MSG_ESC : std_logic_vector(7 downto 0) := x"1b";
	constant MSG_START : std_logic_vector(7 downto 0) := x"5f"; -- _
	constant MSG_END : std_logic_vector(7 downto 0) := x"5c"; -- \
begin

	encode_g : if true generate
		type state_t is (STATE_RESET, STATE_IDLE, STATE_START_ESC, STATE_START,
			STATE_MSG, STATE_END_ESC, STATE_END);
		type r_t is record
			state : state_t;
		end record;
		signal r, r_in : r_t;
	begin
		process(r, rstn, encode_in.valid, encode_in.last, decode_in.accept)
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
				if decode_in.accept = '1' then
					v.state := STATE_START;
				end if;
			when STATE_START =>
				if decode_in.accept = '1' then
					v.state := STATE_MSG;
				end if;
			when STATE_MSG =>
				if decode_in.accept = '1' and encode_in.valid = '1' and encode_in.last = '1' then
					v.state := STATE_END_ESC;
				end if;
			when STATE_END_ESC =>
				if decode_in.accept = '1' then
					v.state := STATE_END;
				end if;
			when STATE_END =>
				if decode_in.accept = '1' then
					v.state := STATE_IDLE;
				end if;
			end case;

			if rstn /= '1' then
				v.state := STATE_RESET;
			end if;

			r_in <= v;
		end process;

		with r.state select
			encode_out.data <=
				MSG_ESC when STATE_START_ESC | STATE_END_ESC,
				MSG_START when STATE_START,
				MSG_END when STATE_END,
				encode_in.data when STATE_MSG,
				terminal_in.data when others;

		with r.state select
			encode_out.valid <=
				'1' when STATE_START_ESC | STATE_START | STATE_END_ESC | STATE_END,
				encode_in.valid when STATE_MSG,
				terminal_in.valid when others;

		with r.state select
			decode_out.accept <=
				decode_in.accept when STATE_MSG,
				'0' when others;

		with r.state select
			terminal_out.accept <=
				decode_in.accept when STATE_IDLE,
				'0' when others;

		terminal_out.last <= decode_in.last;
		decode_out.last <= decode_in.last;

		process(clk)
		begin
			if rising_edge(clk) then
				r <= r_in;
			end if;
		end process;
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
		end record;
		signal r, r_in : r_t;

		signal decode_in_valid, decode_in_accept : std_logic;
		signal decode_out_valid, decode_out_accept, decode_out_last : std_logic;
		signal terminal_out_valid, terminal_out_accept : std_logic;
		signal decode_in_data : std_logic_vector(7 downto 0);
	begin

		fifo_decode_in_inst : entity work.libstored_fifo
			generic map (
				WIDTH => 8,
				DEPTH => 2
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
				DEPTH => 2,
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
				o_valid => decode_out.valid,
				o_accept => decode_in.accept,
				almost_empty => decode_out_last
			);

		fifo_terminal_out_inst : entity work.libstored_fifo
			generic map (
				WIDTH => 8,
				DEPTH => 2
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

		terminal_out.last <= '0';

		process(r, rstn, decode_in_valid, decode_in_accept, decode_in_data)
			variable v : r_t;
		begin
			v := r;

			v.rollback_decode := '0';
			v.rollback_terminal := '0';
			v.commit_decode := '0';
			v.commit_terminal := '0';

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
				if decode_in_valid = '1' and decode_in_accept = '1' then
					if decode_in_data = MSG_START then
						v.state := STATE_MSG;
						v.rollback_terminal := '1';
					elsif decode_in_data = MSG_ESC then
						v.commit_terminal := '1';
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
					elsif decode_in_data = MSG_ESC then
						v.commit_decode := '1';
					else
						v.state := STATE_MSG;
						v.commit_decode := '1';
					end if;
				end if;
			when STATE_FLUSH =>
			end case;

			if rstn /= '1' then
				v.state := STATE_RESET;
			end if;

			r_in <= v;
		end process;

		with r.state select
			decode_out_valid <=
				decode_in_valid when STATE_MSG | STATE_END_ESC,
				'0' when others;

		with r.state select
			terminal_out_valid <=
				decode_in_valid when STATE_IDLE | STATE_START_ESC,
				'0' when others;

		with r.state select
			decode_in_accept <=
				terminal_out_accept when STATE_IDLE | STATE_START_ESC,
				decode_out_accept when STATE_MSG | STATE_END_ESC,
				'0' when others;

		with r.state select
			decode_out.last <=
				decode_out_last when STATE_FLUSH,
				'0' when others;

		process(clk)
		begin
			if rising_edge(clk) then
				r <= r_in;
			end if;
		end process;
	end generate;

end rtl;



library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity UARTLayer is
	generic (
		SYSTEM_CLK_FREQ : integer := 100e6;
		BAUD : integer := 115200
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

		idle : out std_logic
	);
end UARTLayer;

architecture rtl of UARTLayer is
	constant BIT_DURATION : integer := libstored_pkg.maximum(1, integer(real(SYSTEM_CLK_FREQ) / real(BAUD)));
	signal rx_i, cts_i : std_logic;
	signal rx_valid, rx_accept, rx_almost_full, rx_empty : std_logic;
	signal rx_data : std_logic_vector(7 downto 0);
	signal rx_idle, tx_idle : std_logic;
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

	fifo_inst : entity work.libstored_fifo
		generic map (
			WIDTH => 8,
			ALMOST_FULL_REMAINING => 2
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

	decode_out.last <= '0';
	idle <= rx_idle and tx_idle and rx_empty;

	tx_g : if true generate
		type state_t is (STATE_RESET, STATE_IDLE, STATE_START, STATE_DATA, STATE_STOP);
		type r_t is record
			state : state_t;
			cnt : natural range 0 to BIT_DURATION;
			len : natural range 0 to 7;
			data : std_logic_vector(7 downto 0);
			accept : std_logic;
			tx : std_logic;
			idle : std_logic;
		end record;
		signal r, r_in : r_t;
	begin
		process(r, rstn, encode_in.data, encode_in.valid, encode_in.last, cts_i)
			variable v : r_t;
		begin
			v := r;

			if r.cnt > 0 then
				v.cnt := r.cnt - 1;
			end if;

			v.accept := '0';

			case r.state is
			when STATE_RESET =>
				v.state := STATE_IDLE;
			when STATE_IDLE =>
				if encode_in.valid = '1' and cts_i = '0' then
					v.cnt := BIT_DURATION;
					v.data := encode_in.data;
					v.len := 7;
					v.accept := '1';
					v.state := STATE_START;
					v.idle := '0';
					v.tx := '0';
				end if;
			when STATE_START =>
				if v.cnt = 0 then
					v.state := STATE_DATA;
					v.cnt := BIT_DURATION;
					v.tx := r.data(0);
				end if;
			when STATE_DATA =>
				if v.cnt = 0 then
					v.cnt := BIT_DURATION;
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

			if encode_in.last = '1' and encode_in.valid = '1' then
				v.idle := '1';
			end if;

			if rstn /= '1' then
				v.state := STATE_RESET;
				v.tx := '1';
				v.idle := '1';
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
		type state_t is (STATE_RESET, STATE_IDLE, STATE_START, STATE_DATA, STATE_STOP);
		type r_t is record
			state : state_t;
			cnt : natural range 0 to BIT_DURATION;
			len : natural range 0 to 7;
			data : std_logic_vector(7 downto 0);
			valid : std_logic;
			rts : std_logic;
		end record;
		signal r, r_in : r_t;
	begin
		process(r, rstn, rx_i, rx_almost_full)
			variable v : r_t;
		begin
			v := r;

			if r.cnt > 0 then
				v.cnt := r.cnt - 1;
			end if;

			v.valid := '0';
			v.rts := rx_almost_full;

			case r.state is
			when STATE_RESET =>
				v.state := STATE_IDLE;
			when STATE_IDLE =>
				if rx_i = '0' then
					v.cnt := libstored_pkg.maximum(0, (BIT_DURATION) / 2 - 2);
						-- -2 is for meta stability delay
					v.state := STATE_START;
				end if;
			when STATE_START =>
				if v.cnt = 0 then
					v.state := STATE_DATA;
					v.cnt := BIT_DURATION;
					v.len := 7;
				end if;
			when STATE_DATA =>
				if v.cnt = 0 then
					v.cnt := BIT_DURATION;
					v.data := rx_i & v.data(7 downto 1);
					if r.len = 0 then
						v.state := STATE_STOP;
						v.valid := '1';
					else
						v.len := r.len - 1;
					end if;
				end if;
			when STATE_STOP =>
				if v.cnt = 0 then
--pragma translate_off
					assert rx_i = '1' report "Invalid stop bit" severity warning;
					v.data := (others => '-');
--pragma translate_on
					v.state := STATE_IDLE;
				end if;
			end case;

			if rstn /= '1' then
				v.state := STATE_RESET;
				v.rts := '1';
			end if;

			r_in <= v;
		end process;

		rx_data <= r.data;
		rx_valid <= r.valid;
		rts <= r.rts;

		tx_idle <= '1' when r.state = STATE_IDLE else '0';

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

