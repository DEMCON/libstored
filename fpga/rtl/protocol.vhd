-- SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
--
-- SPDX-License-Identifier: MPL-2.0

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
		accept_out : in std_logic;
		dropped_out : out std_logic;

		empty : out std_logic
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
	constant DATA_DROP_LAST : std_logic_vector(1 downto 0) := "11";

	signal valid_in_i, valid_out_i, valid_out_o, accept_out_i, accept_in_i, last_valid : std_logic;
	signal last_out_i : std_logic_vector(1 downto 0);
	signal data_out_i : std_logic_vector(7 downto 0);
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
			o => data_out_i,
			o_valid => valid_out_i,
			o_accept => accept_out_i,
			empty => empty
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

	valid_out_o <=
		valid_out_i and last_valid when last_out_i = DATA_NORMAL or last_out_i = DATA_LAST else
		'0';

	valid_out <= valid_out_o;

	data_out <=
--pragma translate_off
		(others => '-') when valid_out_o /= '1' else
--pragma translate_on
		data_out_i;

	dropped_out <=
		'1' when last_out_i = DATA_DROP_LAST else
		'0';

	accept_out_i <=
		valid_out_i and last_valid and accept_out when last_out_i = DATA_NORMAL or last_out_i = DATA_LAST else
		valid_out_i and last_valid;

	last_out <=
--pragma translate_off
		'-' when valid_out_o /= '1' else
--pragma translate_on
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
			if r.cnt > TAIL_LENGTH + 1 then
				v.last := DATA_NORMAL;
			elsif r.cnt = TAIL_LENGTH + 1 then
				v.last := DATA_LAST;
			elsif r.cnt > 1 then
				v.last := DATA_DROP;
			else
				v.last := DATA_DROP_LAST;
				v.state := STATE_NORMAL;
			end if;
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
		MTU : positive;
		ENCODE_OUT_FIFO_DEPTH : natural := 0;
		DECODE_IN_FIFO_DEPTH : natural := 0
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
end SegmentationLayer;

architecture rtl of SegmentationLayer is
	constant ENCODE_MTU : natural := libstored_pkg.maximum(MTU, 2);
	constant MSG_C : std_logic_vector(7 downto 0) := x"43";
	constant MSG_E : std_logic_vector(7 downto 0) := x"45";

	signal encode_idle, decode_idle : std_logic;
begin

	mtu_decode_out <= 0;

	assert mtu_decode_in = 0 or mtu_decode_in > 1
		report "mtu_decode_in is too small; must be > 1" severity failure;
	assert mtu_decode_in = 0 or mtu_decode_in >= MTU
		report "Specified MTU does not fit downstream" severity failure;

	encode_g : if true generate
		type state_t is (STATE_RESET, STATE_FORWARD, STATE_C, STATE_E);
		type r_t is record
			state : state_t;
			cnt : natural range 0 to ENCODE_MTU - 1;
		end record;

		signal r, r_in : r_t;

		signal encode_out_data_i, encode_out_data_o : std_logic_vector(8 downto 0);
		signal encode_out_valid, encode_out_accept : std_logic;
	begin
		process(clk)
		begin
			if rising_edge(clk) then
				r <= r_in;
			end if;
		end process;

		process(r, rstn, encode_in, encode_out_accept)
			variable v : r_t;
		begin
			v := r;

			case r.state is
			when STATE_RESET =>
				v.state := STATE_FORWARD;
			when STATE_FORWARD =>
				if encode_in.valid = '1' and encode_out_accept = '1' then
					v.cnt := r.cnt + 1;

					if encode_in.last = '1' then
						v.state := STATE_E;
					elsif v.cnt = ENCODE_MTU - 1 then
						v.state := STATE_C;
					end if;
				end if;
			when STATE_C | STATE_E =>
				if encode_out_accept = '1' then
					v.cnt := 0;
					v.state := STATE_FORWARD;
				end if;
			end case;

			if rstn /= '1' then
				v.state := STATE_RESET;
				v.cnt := 0;
			end if;

			r_in <= v;
		end process;

		fifo_inst : entity work.libstored_fifo
			generic map (
				WIDTH => 9,
				DEPTH => ENCODE_OUT_FIFO_DEPTH
			)
			port map (
				clk => clk,
				rstn => rstn,
				i => encode_out_data_i,
				i_valid => encode_out_valid,
				i_accept => encode_out_accept,
				o => encode_out_data_o,
				o_valid => encode_out.valid,
				o_accept => decode_in.accept,
				empty => encode_idle
			);

		with r.state select
			encode_out_data_i <=
				'1' & MSG_C when STATE_C,
				'1' & MSG_E when STATE_E,
				'0' & encode_in.data when others;

		encode_out.data <= encode_out_data_o(7 downto 0);
		encode_out.last <= encode_out_data_o(8);
		with r.state select
			decode_out.accept <=
				'0' when STATE_RESET,
				'0' when STATE_C | STATE_E,
				encode_out_accept when others;

		with r.state select
			encode_out_valid <=
				'0' when STATE_RESET,
				'1' when STATE_C | STATE_E,
				encode_in.valid when others;
	end generate;

	decode_g : if true generate
		signal decode_out_data : std_logic_vector(7 downto 0);
		signal decode_out_valid, decode_out_accept, decode_out_last, skip : std_logic;
	begin
		fifo_inst : entity work.libstored_droptail
			generic map (
				FIFO_DEPTH => DECODE_IN_FIFO_DEPTH,
				TAIL_LENGTH => 1 -- Only 'E' is to be removed.
			)
			port map (
				clk => clk,
				rstn => rstn,
				data_in => decode_out_data,
				last_in => decode_out_last,
				valid_in => decode_out_valid,
				accept_in => decode_out_accept,
				data_out => decode_out.data,
				valid_out => decode_out.valid,
				last_out => decode_out.last,
				accept_out => encode_in.accept,
				empty => decode_idle
			);

		decode_out_data <= decode_in.data;
		skip <= decode_in.last when decode_out_data = MSG_C else '0';
		decode_out_last <= decode_in.last;
		decode_out_valid <= decode_in.valid and not skip;
		encode_out.accept <= decode_out_accept or skip;
	end generate;

	idle <= encode_idle and decode_idle;
end rtl;




library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity ArqLayer is
	generic (
		MTU : positive;
		ENCODE_OUT_FIFO_DEPTH : natural := 0;
		DECODE_IN_FIFO_DEPTH : natural := 0;
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

		reconnect : in std_logic := '0';
		retransmit : in std_logic := '0';

		retransmitted : out std_logic;
		connected : out std_logic;
		idle : out std_logic;

		mtu_decode_in : in natural := 0;
		mtu_decode_out : out natural
	);
end ArqLayer;

architecture rtl of ArqLayer is
	function ack_timeout_calc return natural is
		variable s : real;
	begin
		s := ACK_TIMEOUT_s;

		if SIMULATION then
			s := s / 1000.0;
		end if;

		return libstored_pkg.maximum(1, integer(s * real(SYSTEM_CLK_FREQ)));
	end function;

	constant ACK_TIMEOUT_CLK : natural := ack_timeout_calc;

	constant FLAG_NOP : natural := 6;
	constant FLAG_ACK : natural := 7;
	constant SEQ_BITS : positive := 6;
	constant SEQ_MAX : natural := 2 ** SEQ_BITS - 1;
	constant SEQ_MASK : std_logic_vector(7 downto 0) := std_logic_vector(to_unsigned(SEQ_MAX, 8));

	function arq_msg(constant ack : boolean; constant nop : boolean; constant seq : natural) return std_logic_vector is
		variable msg : std_logic_vector(7 downto 0);
	begin
		msg := (others => '0');
		if ack then
			msg(FLAG_ACK) := '1';
		end if;
		if nop then
			msg(FLAG_NOP) := '1';
		end if;

		msg := msg or (std_logic_vector(to_unsigned(seq, msg'length)) and SEQ_MASK);

		return msg;
	end function;

	function next_seq(constant seq : natural) return natural is
	begin
		if seq = SEQ_MAX then
			return 1;
		else
			return seq + 1;
		end if;
	end function;

	type state_encode_t is (SE_RESET, SE_RECONNECT, SE_RECONNECTING, SE_IDLE,
		SE_HEADER, SE_DATA, SE_WAITING);
	type state_decode_t is (SD_RESET, SD_IDLE, SD_DROP, SD_DECODE);

	type r_t is record
		se : state_encode_t;
		se_prev : state_encode_t;
		sd : state_decode_t;
		t : natural range 0 to ACK_TIMEOUT_CLK;
		timeout : std_logic;
		retransmit : std_logic;
		seq_e : natural range 0 to SEQ_MAX;
		seq_d : natural range 0 to SEQ_MAX;
		seq_d_prev : natural range 0 to SEQ_MAX;
		do_ack : std_logic;
		do_reset : std_logic;
		resetting : boolean;
	end record;

	signal r_in, r : r_t;

	signal ef_data_in, ef_data_out : std_logic_vector(8 downto 0);
	signal ef_data : std_logic_vector(7 downto 0);
	signal ef_last, ef_commit, ef_rollback, ef_valid, ef_accept, ef_empty : std_logic;

	signal df_data_in, df_data_out : std_logic_vector(8 downto 0);
	signal df_valid, df_accept, df_empty : std_logic;

	signal decode_seq : natural range 0 to SEQ_MAX;
begin

	mtu_decode_out <=
		MTU when mtu_decode_in = 0 else
		libstored_pkg.minimum(MTU, mtu_decode_in - 1);

	assert mtu_decode_in = 0 or mtu_decode_in > 1
		report "mtu_decode_in is too small; must be > 1" severity failure;
	assert mtu_decode_in = 0 or mtu_decode_in - 1 >= MTU
		report "Specified MTU does not fit downstream" severity failure;

	ef_data_in <= encode_in.last & encode_in.data;
	ef_data <= ef_data_out(7 downto 0);
	ef_last <= ef_data_out(8);

	encode_fifo_inst : entity work.libstored_fifo
		generic map (
			WIDTH => 9,
			DEPTH => libstored_pkg.maximum(MTU, ENCODE_OUT_FIFO_DEPTH)
		)
		port map (
			clk => clk,
			rstn => rstn,
			i => ef_data_in,
			i_valid => encode_in.valid,
			i_accept => decode_out.accept,
			o => ef_data_out,
			o_valid => ef_valid,
			o_accept => ef_accept,
			o_commit => ef_commit,
			o_rollback => ef_rollback,
			empty => ef_empty
		);

	df_data_in <= decode_in.last & decode_in.data;
	decode_out.data <= df_data_out(7 downto 0);
	decode_out.last <= df_data_out(8);

	decode_seq <=
		0 when decode_in.valid /= '1' else
--pragma translate_off
		0 when is_x(decode_in.data(SEQ_BITS - 1 downto 0)) else
--pragma translate_on
		to_integer(unsigned(decode_in.data(SEQ_BITS - 1 downto 0)));

	decode_fifo_inst : entity work.libstored_fifo
		generic map (
			WIDTH => 9,
			DEPTH => DECODE_IN_FIFO_DEPTH
		)
		port map (
			clk => clk,
			rstn => rstn,
			i => df_data_in,
			i_valid => df_valid,
			i_accept => df_accept,
			o => df_data_out,
			o_valid => decode_out.valid,
			o_accept => encode_in.accept,
			empty => df_empty
		);

	process(r, rstn, decode_in, ef_valid, ef_last, decode_seq, retransmit, reconnect, df_accept)
		variable v : r_t;
	begin
		v := r;

		v.retransmit := '0';

		if r.t > 0 then
			v.t := r.t - 1;
		end if;

		if r.t = 1 or retransmit = '1' then
			v.timeout := '1';
		end if;

		ef_commit <= '0';
		ef_rollback <= '0';

		case r.se is
		when SE_RESET =>
			v.se := SE_RECONNECT;
			v.do_ack := '0';
			v.do_reset := '1';
			v.resetting := false;
		when SE_RECONNECT =>
			-- Send out reset message.
			if decode_in.accept = '1' then
				v.se := SE_RECONNECTING;
				v.timeout := '0';
				v.t := ACK_TIMEOUT_CLK;
				v.seq_e := 0;
				v.do_reset := '0';
			end if;
		when SE_RECONNECTING =>
			if r.sd = SD_IDLE and decode_in.data(FLAG_ACK) = '1' and decode_seq = 0 and decode_in.valid = '1' then
				-- Ack on our reset msg. We are connected now.
				v.se := SE_IDLE;
				v.seq_e := 1;
				v.resetting := true;
			elsif r.timeout = '1' and r.do_ack = '0' then
				v.se := SE_RECONNECT;
				v.retransmit := '1';
			end if;
		when SE_IDLE =>
			if r.do_ack = '1' then
				-- Wait for accept.
				null;
			elsif r.do_reset = '1' then
				v.se := SE_RECONNECT;
			elsif ef_valid = '1' then
				v.se := SE_HEADER;
			end if;
		when SE_HEADER =>
			-- Prepend data with ARQ header.
			if decode_in.accept = '1' then
				v.se := SE_DATA;
			end if;
		when SE_DATA =>
			-- Forward data.
			if decode_in.accept = '1' and ef_last = '1' then
				v.se := SE_WAITING;
				v.t := ACK_TIMEOUT_CLK;
				v.timeout := '0';
			end if;
		when SE_WAITING =>
			-- Wait for ack.
			if r.sd = SD_IDLE and decode_in.data(FLAG_ACK) = '1' and decode_seq = r.seq_e and decode_in.valid = '1' then
				-- Got ack.
				v.se := SE_IDLE;
				v.seq_e := next_seq(r.seq_e);
				ef_commit <= '1';
			elsif r.timeout = '1' or r.do_reset = '1' then
				-- Timeout, retransmit.
				ef_rollback <= '1';
				v.se := SE_IDLE;
				v.retransmit := '1';
			end if;
		when others =>
			v.se := SE_RESET;
		end case;

		if decode_in.valid = '1' and decode_in.last = '1' then
			v.resetting := false;
		end if;

		case r.se is
		when SE_RECONNECTING | SE_IDLE | SE_WAITING =>
			if r.do_ack = '1' and decode_in.accept = '1' then
				v.do_ack := '0';
				v.seq_d_prev := r.seq_d;
				v.seq_d := next_seq(r.seq_d);
			end if;
		when others =>
			null;
		end case;

		case r.sd is
		when SD_RESET =>
			v.sd := SD_IDLE;
			v.seq_d := 0;
			v.seq_d_prev := 0;
		when SD_IDLE =>
			-- Expect next ARQ header byte.

			if decode_in.valid = '0' then
				-- No data.
				null;
			elsif decode_in.data(FLAG_ACK) = '1' then
				-- Ignore this message. r.se should pick it up.
				-- If r.se is not in de proper state now, drop it anyway.
				null;
				-- An ack may be followed by another command.
			elsif decode_seq = 0 then
				-- Got a reset msg. Queue an ack response.
				v.do_ack := '1';
				if not r.resetting then
					v.do_reset := '1';
				end if;
				v.seq_d := 0;
				v.seq_d_prev := 0;

				if decode_in.last = '0' and decode_in.data(FLAG_NOP) = '0' then
					v.sd := SD_DROP;
				end if;
			elsif decode_seq = r.seq_d_prev then
				-- Retransmit. Ack again.
				v.seq_d := r.seq_d_prev;
				v.do_ack := '1';

				if decode_in.last = '0' then
					v.sd := SD_DROP;
				end if;
			elsif decode_seq /= r.seq_d then
				-- Wrong seq. Ignore.
				if decode_in.last = '0' then
					v.sd := SD_DROP;
				end if;
			else
				-- Send an ack.
				v.do_ack := '1';

				if decode_in.last = '1' then
					-- That's it.
					null;
				elsif decode_in.data(FLAG_NOP) = '1' then
					-- No content, resume parsing.
					null;
				else
					-- Pass through for decoding.
					v.sd := SD_DECODE;
				end if;
			end if;
		when SD_DROP | SD_DECODE =>
			if decode_in.valid = '1' and decode_in.last = '1' and df_accept = '1' then
				v.sd := SD_IDLE;
			end if;
		when others =>
			v.sd := SD_RESET;
		end case;

		if reconnect = '1' then
			v.se := SE_RECONNECT;
		end if;

		if rstn /= '1' then
			v.se := SE_RESET;
			v.sd := SD_RESET;
		end if;

		r_in <= v;
	end process;

	encode_out.data <=
		arq_msg(true, true, r.seq_d) when r.do_ack = '1' and (r.se = SE_RECONNECTING or r.se = SE_IDLE or r.se = SE_WAITING) else
		arq_msg(false, true, 0) when r.se = SE_RECONNECT else
		arq_msg(false, false, r.seq_e) when r.se = SE_HEADER else
		ef_data when r.se = SE_DATA else
		(others => '-');

	with r.se select
		encode_out.last <=
			r.do_ack and not r.do_reset when SE_RECONNECTING | SE_IDLE | SE_WAITING,
			'1' when SE_RECONNECT,
			'0' when SE_HEADER,
			ef_last when SE_DATA,
			'-' when others;

	with r.se select
		encode_out.valid <=
			r.do_ack when SE_RECONNECTING | SE_IDLE | SE_WAITING,
			'1' when SE_RECONNECT | SE_HEADER,
			ef_valid when SE_DATA,
			'0' when others;

	with r.se select
		ef_accept <=
			decode_in.accept when SE_DATA,
			'0' when others;

	retransmitted <= r.retransmit;

	with r.se select
		connected <=
			'1' when SE_IDLE | SE_HEADER | SE_DATA | SE_WAITING,
			'0' when others;

	with r.sd select
		encode_out.accept <=
			'1' when SD_IDLE | SD_DROP,
			df_accept when SD_DECODE,
			'0' when others;

	with r.sd select
		df_valid <=
			decode_in.valid when SD_DECODE,
			'0' when others;

	idle <= ef_empty and df_empty and not r.do_ack;

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

entity Crc is
	generic (
		WIDTH : positive := 8;
		POLYNOMIAL : std_logic_vector;
		INIT : std_logic
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		i : in std_logic_vector(WIDTH - 1 downto 0);
		valid : in std_logic;
		last : in std_logic;

		crc : out std_logic_vector(POLYNOMIAL'length - 1 downto 0);
		crc_valid : out std_logic
	);
end Crc;

architecture rtl of Crc is
	type r_t is record
		crc : std_logic_vector(POLYNOMIAL'length - 1 downto 0);
		valid : std_logic;
		reset : std_logic;
	end record;

	constant POLY : std_logic_vector := libstored_pkg.normalize(POLYNOMIAL);
	signal r_in, r : r_t;
begin
	process(r, rstn, i, valid, last)
		variable v : r_t;
	begin
		v := r;

		if r.reset = '1' then
			v.crc := (others => INIT);
			v.valid := '0';
			v.reset := '0';
		end if;

		v.valid := valid;

		if valid = '1' then
			for b in WIDTH - 1 downto 0 loop
				v.crc := v.crc(v.crc'high - 1 downto 0) & '0' xor
					(POLY and (POLY'range => i(b) xor v.crc(v.crc'high)));
			end loop;

			if last = '1' then
				v.reset := '1';
			end if;
		end if;

		if rstn /= '1' then
			v.crc := (others => '-');
			v.valid := '0';
			v.reset := '1';
		end if;

		r_in <= v;
	end process;

	process(clk)
	begin
		if rising_edge(clk) then
			r <= r_in;
		end if;
	end process;

	crc <= r_in.crc;
	crc_valid <= r_in.valid;
end rtl;



library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity CrcLayer is
	generic (
		POLYNOMIAL : std_logic_vector;
		INIT : std_logic := '1';
		MTU : positive;
		ENCODE_OUT_FIFO_DEPTH : natural := 0;
		DECODE_IN_FIFO_DEPTH : natural := 0
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
end CrcLayer;

architecture rtl of CrcLayer is
	constant CRC_BYTES : natural := (POLYNOMIAL'length + 7) / 8;

	signal encode_idle, decode_idle : std_logic;
begin

	mtu_decode_out <=
		MTU when mtu_decode_in = 0 else
		libstored_pkg.minimum(MTU, mtu_decode_in - CRC_BYTES);

	assert mtu_decode_in = 0 or mtu_decode_in > CRC_BYTES
		report "mtu_decode_in is too small; must be > polynomial length" severity failure;
	assert mtu_decode_in = 0 or mtu_decode_in - CRC_BYTES >= MTU
		report "Specified MTU does not fit downstream" severity failure;

	encode_g : if true generate
		type state_t is (STATE_RESET, STATE_FORWARD, STATE_CRC);

		type r_t is record
			state : state_t;
			cnt : natural range 0 to CRC_BYTES - 1;
			crc : std_logic_vector(CRC_BYTES * 8 - 1 downto 0);
		end record;

		signal r, r_in : r_t;

		signal encode_out_i, encode_out_o : std_logic_vector(8 downto 0);
		signal encode_out_data : std_logic_vector(7 downto 0);
		signal encode_out_valid, encode_out_accept, encode_out_last : std_logic;

		signal crc_i_valid : std_logic;
		signal crc : std_logic_vector(POLYNOMIAL'length - 1 downto 0);
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
				o_accept => decode_in.accept,
				empty => encode_idle
			);

		encode_out_i <= encode_out_last & encode_out_data;
		encode_out.data <= encode_out_o(7 downto 0);
		encode_out.last <= encode_out_o(8);

		process(r, rstn, encode_in, encode_out_accept, crc)
			variable v : r_t;
		begin
			v := r;

			case r.state is
			when STATE_RESET =>
				v.state := STATE_FORWARD;
			when STATE_FORWARD =>
				if encode_out_accept = '1' and encode_in.valid = '1' and encode_in.last = '1' then
					v.state := STATE_CRC;
					v.cnt := CRC_BYTES - 1;
					v.crc := (others => '0');
					v.crc(crc'length - 1 downto 0) := crc;
				end if;
			when STATE_CRC =>
				if encode_out_accept = '1' then
					v.crc := r.crc(r.crc'high - 8 downto 0) & (7 downto 0 => '-');

					if r.cnt = 0 then
						v.state := STATE_FORWARD;
					else
						v.cnt := r.cnt - 1;
					end if;
				end if;
			end case;

			if rstn /= '1' then
				v.state := STATE_FORWARD;
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
			encode_out_valid <=
				'0' when STATE_RESET,
				'1' when STATE_CRC,
				encode_in.valid when others;

		with r.state select
			decode_out.accept <=
				'0' when STATE_RESET,
				'0' when STATE_CRC,
				encode_out_accept when others;

		encode_out_last <=
			'1' when r.state = STATE_CRC and r.cnt = 0 else
			'0';

		with r.state select
			encode_out_data <=
				r.crc(r.crc'high downto r.crc'high - 7) when STATE_CRC,
				encode_in.data when others;

		with r.state select
			crc_i_valid <=
				'0' when STATE_CRC,
				encode_in.valid and encode_out_accept when others;

		crc_inst : entity work.Crc
			generic map (
				POLYNOMIAL => POLYNOMIAL,
				INIT => INIT
			)
			port map (
				clk => clk,
				rstn => rstn,

				i => encode_in.data,
				valid => crc_i_valid,
				last => encode_in.last,
				crc => crc
			);
	end generate;

	decode_g : if true generate
		type state_t is (STATE_RESET, STATE_PACKET, STATE_CRC, STATE_CHECK, STATE_COMMIT, STATE_DROP,
			STATE_FLUSH, STATE_FLUSH_END);
		type r_t is record
			state : state_t;
			crc_in : std_logic_vector(CRC_BYTES * 8 - 1 downto 0);
			crc : std_logic_vector(POLYNOMIAL'length - 1 downto 0);
		end record;

		signal r, r_in : r_t;

		signal crc : std_logic_vector(POLYNOMIAL'length - 1 downto 0);
		signal crc_valid : std_logic;

		signal decode_in_data : std_logic_vector(7 downto 0);
		signal decode_in_accept, decode_in_valid, decode_in_last, decode_in_dropped : std_logic;

		signal decode_in_data2, decode_out_data : std_logic_vector(8 downto 0);
		signal decode_in_valid2, decode_in_accept2, decode_in_drop, decode_in_commit : std_logic;
		signal decode_full, decode_tail_empty : std_logic;

		constant MORE_BUFFER : natural :=
			libstored_pkg.maximum(
				integer(libstored_pkg.maximum(MTU, DECODE_IN_FIFO_DEPTH)) - MTU - CRC_BYTES,
				0);
		constant MORE_BUFFER_TAIL : natural := libstored_pkg.minimum(3, MORE_BUFFER);
		constant MORE_BUFFER_FIFO : natural := MORE_BUFFER - MORE_BUFFER_TAIL;
	begin
		drop_fifo_inst : entity work.libstored_droptail
			generic map (
				FIFO_DEPTH => CRC_BYTES + MORE_BUFFER_TAIL,
				TAIL_LENGTH => CRC_BYTES
			)
			port map (
				clk => clk,
				rstn => rstn,
				data_in => decode_in.data,
				last_in => decode_in.last,
				valid_in => decode_in_valid,
				accept_in => decode_in_accept,
				data_out => decode_in_data,
				valid_out => decode_in_valid2,
				last_out => decode_in_last,
				accept_out => decode_in_accept2,
				dropped_out => decode_in_dropped,
				empty => decode_tail_empty
			);

		decode_out_fifo_inst : entity work.libstored_fifo
			generic map (
				WIDTH => 9,
				DEPTH => MTU + MORE_BUFFER_FIFO
			)
			port map (
				clk => clk,
				rstn => rstn,
				i => decode_in_data2,
				i_valid => decode_in_valid2,
				i_accept => decode_in_accept2,
				i_rollback => decode_in_drop,
				i_commit => decode_in_commit,
				o => decode_out_data,
				o_valid => decode_out.valid,
				o_accept => encode_in.accept,
				empty => decode_idle,
				full => decode_full
			);

		with r.state select
			decode_in_valid <=
				decode_in.valid when STATE_PACKET | STATE_FLUSH,
				'0' when others;

		decode_in_data2 <= decode_in_last & decode_in_data;
		decode_out.data <= decode_out_data(7 downto 0);
		decode_out.last <= decode_out_data(8);

		with r.state select
			encode_out.accept <=
				decode_in_accept when STATE_PACKET | STATE_FLUSH,
				'0' when others;

		with r.state select
			decode_in_drop <=
				'1' when STATE_DROP | STATE_FLUSH | STATE_FLUSH_END,
				'0' when others;

		with r.state select
			decode_in_commit <=
				'1' when STATE_COMMIT,
				'0' when others;

		crc_inst : entity work.Crc
			generic map (
				POLYNOMIAL => POLYNOMIAL,
				INIT => INIT
			)
			port map (
				clk => clk,
				rstn => rstn,

				i => decode_in_data,
				valid => crc_valid,
				last => decode_in_last,
				crc => crc
			);

		crc_valid <= decode_in_valid2 and decode_in_accept2;

		process(r, rstn, decode_in, decode_in_valid, decode_in_accept, decode_in_valid2, decode_in_accept2, decode_in_last,
			crc, decode_full, decode_idle, decode_tail_empty, decode_in_dropped)
			variable v : r_t;
		begin
			v := r;

			case r.state is
			when STATE_RESET =>
				v.state := STATE_PACKET;
			when STATE_PACKET =>
				-- Feed data packet through drop_fifo_inst.
				if decode_in_valid = '1' and decode_in_accept = '1' then
					-- The CRC is at the end of the packet.
					v.crc_in := v.crc_in(v.crc_in'high - 8 downto 0) & decode_in.data;

					if decode_in.last = '1' then
						v.state := STATE_CRC;
					end if;
				elsif decode_full = '1' then
					v.state := STATE_FLUSH;
				end if;
			when STATE_CRC =>
				-- Feed the packet, without CRC, through crc_inst.
				if decode_in_dropped = '1' then
					-- Empty packet.
					v.crc := (others => '-');
					v.crc_in := (others => '-');
					v.state := STATE_PACKET;
				elsif decode_in_valid2 = '1' and decode_in_accept2 = '1' then
					v.crc := crc;

					if decode_in_last = '1' then
						v.state := STATE_CHECK;
					end if;
				elsif decode_full = '1' then
					v.state := STATE_FLUSH;
				end if;
			when STATE_CHECK =>
				if r.crc_in(r.crc'range) = r.crc then
					-- CRC is valid. Release packet.
					v.state := STATE_COMMIT;
				else
					v.state := STATE_DROP;
				end if;
			when STATE_COMMIT | STATE_DROP =>
				v.crc := (others => '-');
				v.crc_in := (others => '-');
				v.state := STATE_PACKET;
			when STATE_FLUSH =>
				-- Pass the packet through drop_fifo_inst, and drop afterwards.
				if decode_in_valid = '1' and decode_in_accept = '1' and decode_in.last = '1' then
					v.state := STATE_FLUSH_END;
				end if;
			when STATE_FLUSH_END =>
				-- Wait for end of packet to drop.
				if decode_in_valid2 = '1' and decode_in_accept2 = '1' and decode_in_last = '1' then
					v.crc := (others => '-');
					v.crc_in := (others => '-');
					v.state := STATE_PACKET;
				end if;
			end case;

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
	end generate;

	idle <= encode_idle and decode_idle;
end rtl;



library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity Crc8Layer is
	generic (
		MTU : positive;
		ENCODE_OUT_FIFO_DEPTH : natural := 0;
		DECODE_IN_FIFO_DEPTH : natural := 0
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
end Crc8Layer;

architecture rtl of Crc8Layer is
	constant POLYNOMIAL : std_logic_vector(7 downto 0) := x"a6";
begin
	inst : entity work.CrcLayer
		generic map (
			POLYNOMIAL => POLYNOMIAL,
			MTU => MTU,
			ENCODE_OUT_FIFO_DEPTH => ENCODE_OUT_FIFO_DEPTH,
			DECODE_IN_FIFO_DEPTH => DECODE_IN_FIFO_DEPTH
		)
		port map (
			clk => clk,
			rstn => rstn,

			encode_in => encode_in,
			encode_out => encode_out,

			decode_in => decode_in,
			decode_out => decode_out,

			idle => idle,

			mtu_decode_in => mtu_decode_in,
			mtu_decode_out => mtu_decode_out
		);
end rtl;



library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.libstored_pkg;

entity Crc16Layer is
	generic (
		MTU : positive;
		ENCODE_OUT_FIFO_DEPTH : natural := 0;
		DECODE_IN_FIFO_DEPTH : natural := 0
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
end CRC16Layer;

architecture rtl of CRC16Layer is
	constant POLYNOMIAL : std_logic_vector(15 downto 0) := x"baad";
begin
	inst : entity work.CrcLayer
		generic map (
			POLYNOMIAL => POLYNOMIAL,
			MTU => MTU,
			ENCODE_OUT_FIFO_DEPTH => ENCODE_OUT_FIFO_DEPTH,
			DECODE_IN_FIFO_DEPTH => DECODE_IN_FIFO_DEPTH
		)
		port map (
			clk => clk,
			rstn => rstn,

			encode_in => encode_in,
			encode_out => encode_out,

			decode_in => decode_in,
			decode_out => decode_out,

			idle => idle,

			mtu_decode_in => mtu_decode_in,
			mtu_decode_out => mtu_decode_out
		);
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

		idle : out std_logic;

		mtu_decode_in : in natural := 0;
		mtu_decode_out : out natural
	);
end AsciiEscapeLayer;

architecture rtl of AsciiEscapeLayer is
begin
	mtu_decode_out <=
		0 when mtu_decode_in = 0 else
		mtu_decode_in / 2;

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

		idle : out std_logic;

		mtu_decode_in : in natural := 0;
		mtu_decode_out : out natural
	);
end BufferLayer;

architecture rtl of BufferLayer is
	signal encode_empty, decode_empty : std_logic;
begin

	mtu_decode_out <= mtu_decode_in;

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

		idle : out std_logic;

		mtu_decode_in : in natural := 0;
		mtu_decode_out : out natural
	);
end TerminalLayer;

architecture rtl of TerminalLayer is
	constant MSG_ESC : std_logic_vector(7 downto 0) := x"1b";
	constant MSG_START : std_logic_vector(7 downto 0) := x"5f"; -- '_'
	constant MSG_END : std_logic_vector(7 downto 0) := x"5c";   -- '\'

	signal encode_idle, decode_idle : std_logic;
begin

	mtu_decode_out <=
		0 when mtu_decode_in = 0 else
		mtu_decode_in - 4;

	assert mtu_decode_in = 0 or mtu_decode_in > 4
		report "mtu_decode_in is too small; must be > 4" severity failure;

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
		bit_clk : out natural;

		mtu_decode_in : in natural := 0;
		mtu_decode_out : out natural
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

	mtu_decode_out <= mtu_decode_in;

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

