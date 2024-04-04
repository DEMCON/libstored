-- SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
--
-- SPDX-License-Identifier: MPL-2.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.math_real.all;

use work.TestStore_pkg;

use work.libstored_pkg.all;
use work.libstored_tb_pkg.all;

entity test_fpga is
end entity;

architecture behav of test_fpga is
	constant SYSTEM_CLK_FREQ : integer := 100e6;

	signal clk, rstn : std_logic;
	signal done : boolean;

	signal var_in, var_in2 : TestStore_pkg.var_in_t;
	signal var_out, var_out2 : TestStore_pkg.var_out_t;

	signal axi_m2s : axi_m2s_t;
	signal axi_s2m : axi_s2m_t;
	signal sync_in, sync_out, sync_chained_in, sync_chained_out : msg_t;
	signal sync_in_busy, sync_in_busy2 : std_logic;
	signal sync_out_hold : std_logic := '0';
	signal sync_out_hold2 : std_logic := '1';
	signal sync_chained_id : unsigned(15 downto 0);

	function var_access return TestStore_pkg.var_access_t is
		variable v : TestStore_pkg.var_access_t;
	begin
		v := TestStore_pkg.VAR_ACCESS_RW;
		v.\default uint16\ := ACCESS_RO;
		v.\default uint32\ := ACCESS_WO;
		v.\default uint64\ := ACCESS_NA;
		return v;
	end function;

	constant BAUD : natural := 11000000;
	signal uart_encode_in, uart_decode_out : msg_t := msg_term;
	signal uart_rx, uart_tx, uart_cts, uart_rts : std_logic := '1';
	signal uart_bit_clk : natural;

	signal term_encode_in, term_encode_out, term_decode_in, term_decode_out : msg_t := msg_term;
	signal term_terminal_out, term_terminal_in : msg_t := msg_term;
	signal esc_encode_in, esc_encode_out, esc_decode_in, esc_decode_out : msg_t := msg_term;
	signal file_encode_in, file_decode_out : msg_t := msg_term;
	signal segment_encode_in, segment_encode_out, segment_decode_in, segment_decode_out : msg_t := msg_term;
	signal crc8_encode_in, crc8_encode_out, crc8_decode_in, crc8_decode_out : msg_t := msg_term;
	signal crc16_encode_in, crc16_encode_out, crc16_decode_in, crc16_decode_out : msg_t := msg_term;
	signal arq_encode_in, arq_encode_out, arq_decode_in, arq_decode_out : msg_t := msg_term;
	signal arq_reconnect : std_logic := '0';

	signal full_stack : boolean := false;

	shared variable seed1 : positive := 1;
	shared variable seed2 : positive := 2;
begin

	store_inst : entity work.TestStore_hdl
		generic map (
			SYSTEM_CLK_FREQ => SYSTEM_CLK_FREQ,
			AXI_SLAVE => true,
			VAR_ACCESS => var_access
		)
		port map (
			clk => clk,
			rstn => rstn,

			var_out => var_out,
			var_in => var_in,

			sync_in => sync_in,
			sync_out => sync_out,
			sync_in_busy => sync_in_busy,
			sync_id => sync_chained_id,
			sync_chained_in => sync_chained_in,
			sync_chained_out => sync_chained_out,
			sync_out_hold => sync_out_hold,

			s_axi_araddr => axi_m2s.araddr,
			s_axi_arready => axi_s2m.arready,
			s_axi_arvalid => axi_m2s.arvalid,
			s_axi_awaddr => axi_m2s.awaddr,
			s_axi_awready => axi_s2m.awready,
			s_axi_awvalid => axi_m2s.awvalid,
			s_axi_bready => axi_m2s.bready,
			s_axi_bresp => axi_s2m.bresp,
			s_axi_bvalid => axi_s2m.bvalid,
			s_axi_rdata => axi_s2m.rdata,
			s_axi_rready => axi_m2s.rready,
			s_axi_rresp => axi_s2m.rresp,
			s_axi_rvalid => axi_s2m.rvalid,
			s_axi_wdata => axi_m2s.wdata,
			s_axi_wready => axi_s2m.wready,
			s_axi_wvalid => axi_m2s.wvalid
		);

	store_inst2 : entity work.TestStore_hdl
		generic map (
			SYSTEM_CLK_FREQ => SYSTEM_CLK_FREQ
		)
		port map (
			clk => clk,
			rstn => rstn,

			var_out => var_out2,
			var_in => var_in2,

			sync_in => sync_chained_out,
			sync_out => sync_chained_in,
			sync_chained_id => sync_chained_id,
			sync_out_hold => sync_out_hold2
		);

	UARTLayer_inst : entity work.UARTLayer
		generic map (
			SYSTEM_CLK_FREQ => SYSTEM_CLK_FREQ,
			BAUD => 0, --BAUD,
			AUTO_BAUD_MINIMUM => 960000,
			XON_XOFF => true
		)
		port map (
			clk => clk,
			rstn => rstn,
			encode_in => uart_encode_in,
			decode_out => uart_decode_out,
			rx => uart_rx,
			tx => uart_tx,
			cts => uart_cts,
			rts => uart_rts,
			bit_clk => uart_bit_clk
		);

	TerminalLayer_inst : entity work.TerminalLayer
		generic map (
			ENCODE_OUT_FIFO_DEPTH => 8,
			DECODE_IN_FIFO_DEPTH => 8
		)
		port map (
			clk => clk,
			rstn => rstn,
			encode_in => term_encode_in,
			encode_out => term_encode_out,
			decode_in => term_decode_in,
			decode_out => term_decode_out,
			terminal_in => term_terminal_in,
			terminal_out => term_terminal_out
		);

	ASCIIEscapeLayer_inst : entity work.ASCIIEscapeLayer
		generic map (
			ENCODE_OUT_FIFO_DEPTH => 12,
			DECODE_OUT_FIFO_DEPTH => 12
		)
		port map (
			clk => clk,
			rstn => rstn,
			encode_in => esc_encode_in,
			encode_out => esc_encode_out,
			decode_in => esc_decode_in,
			decode_out => esc_decode_out
		);

	FileLayer_inst : entity work.FileLayer
		generic map (
			FILENAME_IN => "test_stack.txt",
			FILENAME_OUT => "test_stack.txt"
		)
		port map (
			clk => clk,
			rstn => rstn,
			encode_in => file_encode_in,
			decode_out => file_decode_out
		);

	SegmentationLayer_inst : entity work.SegmentationLayer
		generic map (
			MTU => 5,
			ENCODE_OUT_FIFO_DEPTH => 12,
			DECODE_IN_FIFO_DEPTH => 12
		)
		port map (
			clk => clk,
			rstn => rstn,
			encode_in => segment_encode_in,
			encode_out => segment_encode_out,
			decode_in => segment_decode_in,
			decode_out => segment_decode_out
		);

	Crc8Layer_inst : entity work.Crc8Layer
		generic map (
			MTU => 8,
			ENCODE_OUT_FIFO_DEPTH => 12,
			DECODE_IN_FIFO_DEPTH => 12
		)
		port map (
			clk => clk,
			rstn => rstn,
			encode_in => crc8_encode_in,
			encode_out => crc8_encode_out,
			decode_in => crc8_decode_in,
			decode_out => crc8_decode_out
		);

	Crc16Layer_inst : entity work.Crc16Layer
		generic map (
			MTU => 8,
			ENCODE_OUT_FIFO_DEPTH => 12,
			DECODE_IN_FIFO_DEPTH => 12
		)
		port map (
			clk => clk,
			rstn => rstn,
			encode_in => crc16_encode_in,
			encode_out => crc16_encode_out,
			decode_in => crc16_decode_in,
			decode_out => crc16_decode_out
		);

	ArqLayer_inst : entity work.ArqLayer
		generic map (
			MTU => 8,
			ENCODE_OUT_FIFO_DEPTH => 12,
			DECODE_IN_FIFO_DEPTH => 12,
			SYSTEM_CLK_FREQ => SYSTEM_CLK_FREQ,
			ACK_TIMEOUT_s => 1.0e-3
		)
		port map (
			clk => clk,
			rstn => rstn,
			encode_in => arq_encode_in,
			encode_out => arq_encode_out,
			decode_in => arq_decode_in,
			decode_out => arq_decode_out,
			reconnect => arq_reconnect
		);

	process(rstn, full_stack, segment_encode_out, arq_encode_out, crc8_encode_out, esc_encode_out,
		term_encode_out, term_decode_out, esc_decode_out, crc8_decode_out, arq_decode_out)

		variable msg : msg_t := msg_term;
		variable rand : real;
	begin
		if full_stack'event or rstn'event then
			if full_stack then
				arq_encode_in <= msg_term;
				crc8_encode_in <= msg_term;
				esc_encode_in <= msg_term;
				term_encode_in <= msg_term;
				term_decode_in <= msg_term;
				esc_decode_in <= msg_term;
				crc8_decode_in <= msg_term;
				arq_decode_in <= msg_term;
				segment_decode_in <= msg_term;
			else
				arq_encode_in <= msg_highz;
				crc8_encode_in <= msg_highz;
				esc_encode_in <= msg_highz;
				term_encode_in <= msg_highz;
				term_decode_in <= msg_highz;
				esc_decode_in <= msg_highz;
				crc8_decode_in <= msg_highz;
				arq_decode_in <= msg_highz;
				segment_decode_in <= msg_highz;
			end if;
		end if;

		if full_stack then
			arq_encode_in <= segment_encode_out;
			crc8_encode_in <= arq_encode_out;
			esc_encode_in <= crc8_encode_out;
			term_encode_in <= esc_encode_out;

			term_decode_in <= term_encode_out;
			esc_decode_in <= term_decode_out;

			msg := esc_decode_out;
			if msg.valid = '1' and msg.last = '1' then
				uniform(seed1, seed2, rand);
				-- Drop messages by corrupting the CRC.
				if rand < 0.33 then
					msg.data(0) := not msg.data(0);
				end if;
			end if;

			crc8_decode_in <= msg;
			arq_decode_in <= crc8_decode_out;
			segment_decode_in <= arq_decode_out;
		end if;
	end process;

	process
	begin
		clk <= '0';
		wait for 0.5 / real(SYSTEM_CLK_FREQ) * 1 sec;
		clk <= '1';
		wait for 0.5 / real(SYSTEM_CLK_FREQ) * 1 sec;

		if done then
			wait;
		end if;
	end process;

	process
	begin
		rstn <= '0';
		for i in 0 to 15 loop
			wait until rising_edge(clk);
		end loop;
		rstn <= '1';
		wait;
	end process;

	process
		variable test : test_t;
		variable id_in, id_out, id_in2, id_out2 : std_logic_vector(15 downto 0);

		procedure do_test_initial is
		begin
			test_start(test, "Initial");
			test_expect_eq(test, var_out.\default int8\.value, 0);
			test_expect_eq(test, var_out.\default int16\.value, 0);
			test_expect_eq(test, var_out.\default int32\.value, 0);
			test_expect_eq(test, var_out.\init decimal\.value, 42);
			test_expect_eq(test, var_out.\array bool[0]\.value, '1');
			test_expect_eq(test, var_out.\array string[0]\.value, x"00000000");
		end procedure;

		procedure do_test_set is
		begin
			test_start(test, "Set");
			var_in.\default int8\.value <= x"12";
			var_in.\default int8\.we <= '1';
			wait until rising_edge(clk);
			var_in.\default int8\.we <= '0';
			wait until rising_edge(clk) and var_out.\default int8\.updated = '1' for 1 ms;
			test_expect_eq(test, var_out.\default int8\.value, 18);
		end procedure;

		procedure do_test_axi is
			variable data : std_logic_vector(31 downto 0);
		begin
			test_start(test, "AXI");
			axi_read(clk, axi_m2s, axi_s2m, TestStore_pkg.\default int8/ADDR\, data);
			test_expect_eq(test, data, x"00000012");

			axi_write(clk, axi_m2s, axi_s2m, TestStore_pkg.\default int16/ADDR\, x"abcd1122");
			test_expect_eq(test, var_out.\default int16\.value, 16#1122#);
		end procedure;

		procedure do_test_hello is
			variable buf : buffer_t(0 to TestStore_pkg.BUFFER_LENGTH - 1);
		begin
			test_start(test, "Hello");
			sync_accept_hello(clk, sync_in, sync_out, TestStore_pkg.HASH, id_in, TestStore_pkg.LITTLE_ENDIAN);
			for i in buf'range loop
				buf(i) := std_logic_vector(to_unsigned(i, 8));
			end loop;

			sync_welcome(clk, sync_in, sync_out, id_in, id_out, buf, TestStore_pkg.LITTLE_ENDIAN);
		end procedure;

		procedure do_test_update_single is
		begin
			test_start(test, "UpdateSingle");
			sync_update(clk, sync_in, sync_out, id_in, TestStore_pkg.\default int8/KEY\,
				to_buffer(x"24"), TestStore_pkg.LITTLE_ENDIAN);
			sync_wait(clk, sync_in_busy);
			test_expect_eq(test, var_out.\default int8\.value, 16#24#);
		end procedure;

		procedure do_test_update_multi is
		begin
			test_start(test, "UpdateMulti");
			sync_update_start(clk, sync_in, sync_out, id_in, TestStore_pkg.LITTLE_ENDIAN);
			sync_update_var(clk, sync_in, sync_out, TestStore_pkg.\default int8/KEY\,
				to_buffer(x"25"), false, TestStore_pkg.LITTLE_ENDIAN);
			sync_update_var(clk, sync_in, sync_out, TestStore_pkg.\default int32/KEY\,
				to_buffer(101, 4), false, TestStore_pkg.LITTLE_ENDIAN);
			sync_update_var(clk, sync_in, sync_out, TestStore_pkg.\some other scope/some other inner bool/KEY\,
				to_buffer('0'), true, TestStore_pkg.LITTLE_ENDIAN);
			sync_wait(clk, sync_in_busy);
			test_expect_eq(test, var_out.\default int8\.value, 16#25#);
			test_expect_eq(test, var_out.\default int32\.value, 101);
			test_expect_eq(test, var_out.\some other scope/some other inner bool\.value, '0');
		end procedure;

		procedure do_test_update_burst is
		begin
			test_start(test, "UpdateBurst");
			sync_update(clk, sync_in, sync_out, id_in, TestStore_pkg.\default int8/KEY\,
				to_buffer(x"26"), TestStore_pkg.LITTLE_ENDIAN);
			sync_update(clk, sync_in, sync_out, id_in, TestStore_pkg.\default int8/KEY\,
				to_buffer(x"27"), TestStore_pkg.LITTLE_ENDIAN);
			sync_update(clk, sync_in, sync_out, id_in, TestStore_pkg.\default int8/KEY\,
				to_buffer(x"28"), TestStore_pkg.LITTLE_ENDIAN);
			sync_wait(clk, sync_in_busy);
			test_expect_eq(test, var_out.\default int8\.value, 16#28#);
		end procedure;

		procedure do_test_update_out is
			variable buf : buffer_t(0 to TestStore_pkg.BUFFER_LENGTH - 1);
			variable key : std_logic_vector(TestStore_pkg.KEY_LENGTH - 1 downto 0);
			variable last : boolean;
		begin
			test_start(test, "UpdateOut");
			var_in.\default int8\.value <= x"45";
			var_in.\default int8\.we <= '1';
			wait until rising_edge(clk);
			var_in.\default int8\.we <= '0';

			sync_accept_update_start(clk, sync_in, sync_out, id_out, TestStore_pkg.LITTLE_ENDIAN);
			sync_accept_update_var(clk, sync_in, sync_out, key, buf, last, TestStore_pkg.LITTLE_ENDIAN);
			test_expect_eq(test, key, TestStore_pkg.\default int8/KEY\);
			test_expect_eq(test, buf(0), x"45");
			test_expect_true(test, last);
		end procedure;

		procedure do_test_access_ro is
		begin
			test_start(test, "AccessRO");
			sync_update(clk, sync_in, sync_out, id_in, TestStore_pkg.\default uint16/KEY\,
				to_buffer(16#2345#, 2, TestStore_pkg.LITTLE_ENDIAN), TestStore_pkg.LITTLE_ENDIAN);
			sync_wait(clk, sync_in_busy);
			test_expect_eq(test, var_out.\default uint16\.value, 16#2345#);

			var_in.\default uint16\.value <= x"1234";
			var_in.\default uint16\.we <= '1';
			wait until rising_edge(clk);
			var_in.\default uint16\.we <= '0';
			wait until rising_edge(clk) and var_out.\default int32\.updated = '1' for 1 us;
			wait until rising_edge(clk);
			test_expect_eq(test, var_out.\default uint16\.value, 16#2345#);
		end procedure;

		procedure do_test_access_wo is
		begin
			test_start(test, "AccessWO");
			var_in.\default uint32\.value <= x"11223344";
			var_in.\default uint32\.we <= '1';
			wait until rising_edge(clk);
			var_in.\default uint32\.we <= '0';
			wait until rising_edge(clk) and var_out.\default uint32\.updated = '1' for 1 ms;
			test_expect_eq(test, var_out.\default uint32\.value, 16#11223344#);

			sync_update(clk, sync_in, sync_out, id_in, TestStore_pkg.\default uint32/KEY\,
				to_buffer(16#22334455#, 4, TestStore_pkg.LITTLE_ENDIAN), TestStore_pkg.LITTLE_ENDIAN);
			sync_wait(clk, sync_in_busy);
			test_expect_eq(test, var_out.\default uint32\.value, 16#11223344#);
		end procedure;

		procedure do_test_access_na is
		begin
			test_start(test, "AccessNA");
			var_in.\default uint64\.value <= x"1122334455667788";
			var_in.\default uint64\.we <= '1';
			wait until rising_edge(clk);
			var_in.\default uint64\.we <= '0';
			wait until rising_edge(clk) and var_out.\default uint64\.updated = '1' for 1 us;
			wait until rising_edge(clk);
			test_expect_eq(test, var_out.\default uint64\.value, 0);

			sync_update(clk, sync_in, sync_out, id_in, TestStore_pkg.\default uint64/KEY\,
				to_buffer(x"2233445566778899"), TestStore_pkg.LITTLE_ENDIAN);
			sync_wait(clk, sync_in_busy);
			test_expect_eq(test, var_out.\default uint64\.value, 0);
		end procedure;

		procedure do_test_chained_hello is
			variable buf : buffer_t(0 to TestStore_pkg.BUFFER_LENGTH - 1);
		begin
			test_start(test, "ChainedHello");
			sync_out_hold2 <= '0';
			sync_accept_hello(clk, sync_in, sync_out, TestStore_pkg.HASH, id_in2, TestStore_pkg.LITTLE_ENDIAN);
			for i in buf'range loop
				buf(i) := std_logic_vector(to_unsigned(i + 16, 8));
			end loop;

			sync_welcome(clk, sync_in, sync_out, id_in2, id_out2, buf, TestStore_pkg.LITTLE_ENDIAN);
		end procedure;

		procedure do_test_chained_update is
		begin
			test_start(test, "ChainedUpdate");
			var_in.\default int8\.value <= x"80";
			var_in.\default int8\.we <= '1';
			wait until rising_edge(clk);
			var_in.\default int8\.we <= '0';
			wait until rising_edge(clk) and var_out.\default int8\.updated = '1' for 1 ms;
			test_expect_eq(test, var_out.\default int8\.value, 16#80#);

			sync_update(clk, sync_in, sync_out, id_in2, TestStore_pkg.\default int8/KEY\,
				to_buffer(x"81"), TestStore_pkg.LITTLE_ENDIAN);
			wait until rising_edge(clk) and var_out2.\default int8\.updated = '1' for 1 ms;
			test_expect_eq(test, var_out.\default int8\.value, 16#80#);
			test_expect_eq(test, var_out2.\default int8\.value, 16#81#);
		end procedure;

		procedure do_test_chained_update_out is
			variable buf : buffer_t(0 to TestStore_pkg.BUFFER_LENGTH - 1);
			variable key : std_logic_vector(TestStore_pkg.KEY_LENGTH - 1 downto 0);
			variable last : boolean;
		begin
			test_start(test, "ChainedUpdateOut");
			var_in2.\default int8\.value <= x"82";
			var_in2.\default int8\.we <= '1';
			wait until rising_edge(clk);
			var_in2.\default int8\.we <= '0';

			sync_accept_update_start(clk, sync_in, sync_out, id_out2, TestStore_pkg.LITTLE_ENDIAN);
			sync_accept_update_var(clk, sync_in, sync_out, key, buf, last, TestStore_pkg.LITTLE_ENDIAN);
			test_expect_eq(test, key, TestStore_pkg.\default int8/KEY\);
			test_expect_eq(test, buf(0), x"82");
			test_expect_true(test, last);
			test_expect_eq(test, var_out.\default int8\.value, 16#80#);
			test_expect_eq(test, var_out2.\default int8\.value, 16#82#);
		end procedure;

		procedure reset_auto_baud is
		begin
			wait until rising_edge(clk);
			uart_rx <= '0';
			wait for 1000 ms / SYSTEM_CLK_FREQ * uart_bit_clk * 11;
			wait until rising_edge(clk);
			uart_rx <= '1';
			-- Wait for UART SYNC_DURATION.
			wait for 0.021 ms;
			wait until rising_edge(clk);
		end procedure;

		procedure do_test_uart_auto_baud is
		begin
			test_start(test, "UartAutoBaud");
			-- SYNC_DURATION is two bytes at minimum baud rate, which is set
			-- at 9600 * 100.
			wait for 0.021 ms;

			uart_do_rx(BAUD / 3, uart_rx, uart_rts, to_buffer(x"11")); -- XON
			test_expect_eq(test, uart_bit_clk, integer(real(SYSTEM_CLK_FREQ) / real(BAUD / 3)));
			reset_auto_baud;

			uart_do_rx(BAUD / 2, uart_rx, uart_rts, to_buffer(x"13")); -- XOFF
			test_expect_eq(test, uart_bit_clk, integer(real(SYSTEM_CLK_FREQ) / real(BAUD / 2)));
			reset_auto_baud;

			uart_do_rx(BAUD, uart_rx, uart_rts, to_buffer(x"1b")); -- ESC
			test_expect_eq(test, uart_bit_clk, integer(real(SYSTEM_CLK_FREQ) / real(BAUD)));
		end procedure;

		procedure do_test_uart_tx is
			variable buf : buffer_t(0 to 0);
		begin
			test_start(test, "UartTx");
			uart_cts <= '0';
			uart_encode_in.data <= x"12";
			uart_encode_in.valid <= '1';
			uart_encode_in.last <= '1';
			uart_encode_in.accept <= '0';
			wait until rising_edge(clk) and uart_decode_out.accept = '1' for 1 ms;
			uart_encode_in.valid <= '0';
			uart_do_tx(BAUD, uart_tx, buf);
			wait until rising_edge(clk);
			test_expect_eq(test, buf(0), x"12");
		end procedure;

		procedure do_test_uart_tx_fc is
			variable buf : buffer_t(0 to 0);
		begin
			test_start(test, "UartTxFlowControl");
			uart_cts <= '1';
			uart_encode_in.data <= x"34";
			uart_encode_in.valid <= '1';
			uart_encode_in.last <= '1';
			uart_encode_in.accept <= '0';
			wait until rising_edge(clk) and uart_decode_out.accept = '1' for 10 us;
			test_expect_eq(test, uart_decode_out.accept, '0');
			test_expect_eq(test, uart_tx, '1');
			uart_cts <= '0';
			wait until rising_edge(clk) and uart_decode_out.accept = '1' for 1 ms;
			uart_encode_in.valid <= '0';
			uart_do_tx(BAUD, uart_tx, buf);
			uart_cts <= '1';
			wait until rising_edge(clk);
			test_expect_eq(test, buf(0), x"34");

			-- Suspend because of software flow control.
			uart_do_rx(BAUD, uart_rx, uart_rts, to_buffer(x"13")); -- XOFF
			-- Hardware flow control allows tx to start.
			uart_cts <= '0';
			uart_encode_in.data <= x"56";
			uart_encode_in.valid <= '1';
			uart_encode_in.last <= '1';
			uart_encode_in.accept <= '0';
			wait until rising_edge(clk) and uart_decode_out.accept = '1' for 10 us;
			-- Nothing should have happened.
			test_expect_eq(test, uart_decode_out.accept, '0');
			test_expect_eq(test, uart_tx, '1');

			uart_do_rx(BAUD, uart_rx, uart_rts, to_buffer(x"11")); -- XON
			-- msg should have been accepted already.
			uart_encode_in.valid <= '0';
			uart_do_tx(BAUD, uart_tx, buf);
			uart_cts <= '1';
			wait until rising_edge(clk);
			test_expect_eq(test, buf(0), x"56");

			-- Duplicate XON, should be responded to with XON.
			uart_do_rx(BAUD, uart_rx, uart_rts, to_buffer(x"11")); -- XON
			uart_cts <= '0';
			uart_do_tx(BAUD, uart_tx, buf);
			uart_cts <= '1';
			wait until rising_edge(clk);
			test_expect_eq(test, buf(0), x"11");
		end procedure;

		procedure do_test_uart_rx is
		begin
			test_start(test, "UartRx");
			uart_encode_in.accept <= '0';
			uart_do_rx(BAUD, uart_rx, uart_rts, to_buffer(x"34"));
			wait until rising_edge(clk);
			uart_encode_in.accept <= '1';
			wait until rising_edge(clk) and uart_decode_out.valid = '1' for 1 ms;
			test_expect_eq(test, uart_decode_out.valid, '1');
			test_expect_eq(test, uart_decode_out.data, x"34");
			uart_encode_in.accept <= '0';
		end procedure;

		procedure do_test_uart_rx_fc is
			variable buf : buffer_t(0 to 0);
		begin
			test_start(test, "UartRxFlowControl");
			uart_cts <= '1';
			-- FIFO has a size of 16 and will be almost full at 9, not counting XOFF and XON.
			uart_do_rx(BAUD, uart_rx, uart_rts,
				(x"01", x"02", x"03", x"04", x"13", x"05", x"06", x"11", x"07", x"08", x"09"));
			wait until rising_edge(clk) and uart_rts = '1' for 1 us;
			test_expect_eq(test, uart_rts, '1');
			uart_cts <= '0';
			uart_do_tx(BAUD, uart_tx, buf);
			uart_cts <= '1';
			wait until rising_edge(clk);
			test_expect_eq(test, buf(0), x"13"); -- XOFF

			uart_encode_in.accept <= '1';
			for i in 1 to 9 loop
				wait until rising_edge(clk);
				test_expect_eq(test, uart_decode_out.valid, '1');
				test_expect_eq(test, unsigned(uart_decode_out.data), i);
			end loop;
			uart_encode_in.accept <= '0';

			uart_cts <= '0';
			uart_do_tx(BAUD, uart_tx, buf);
			uart_cts <= '1';
			wait until rising_edge(clk);
			test_expect_eq(test, buf(0), x"11"); -- XON

			uart_do_rx(BAUD, uart_rx, uart_rts, to_buffer(x"0a"));
			wait until rising_edge(clk) and uart_decode_out.valid = '1' for 1 ms;
			uart_encode_in.accept <= '1';
			test_expect_eq(test, unsigned(uart_decode_out.data), 10);
			wait until rising_edge(clk);
			uart_encode_in.accept <= '0';
			wait until rising_edge(clk);
		end procedure;

		procedure do_test_term_inject is
			variable buf : buffer_t(0 to 15);
			variable len : natural;
			variable last : boolean;
		begin
			test_start(test, "TerminalInject");
			term_decode_in.accept <= '0';
			msg_write(clk, term_decode_out, term_encode_in, to_buffer(x"aa"));
			test_expect_eq(test, clk, term_encode_out, term_decode_in, (x"1b", x"5f", x"aa", x"1b", x"5c"));
			msg_write(clk, term_decode_out, term_encode_in, to_buffer(x"bb"), false);
			test_expect_eq(test, clk, term_encode_out, term_decode_in, (x"1b", x"5f", x"bb"));
			msg_write(clk, term_decode_out, term_encode_in, to_buffer(x"cc"));
			test_expect_eq(test, clk, term_encode_out, term_decode_in, (x"cc", x"1b", x"5c"));

			msg_write(clk, term_terminal_out, term_terminal_in, to_buffer(x"11"));
			test_expect_eq(test, clk, term_encode_out, term_decode_in, to_buffer(x"11"));

			msg_write(clk, term_decode_out, term_encode_in, to_buffer(x"dd"), false);
			test_expect_eq(test, term_terminal_out.accept, '0');
			msg_write(clk, term_decode_out, term_encode_in, to_buffer(x"ee"));
			msg_write(clk, term_terminal_out, term_terminal_in, to_buffer(x"22"));
			test_expect_eq(test, clk, term_encode_out, term_decode_in, (x"1b", x"5f", x"dd", x"ee", x"1b", x"5c", x"22"));
		end procedure;

		procedure do_test_term_extract is
		begin
			test_start(test, "TerminalExtract");
			term_encode_in.accept <= '0';
			term_terminal_in.accept <= '0';
			msg_write(clk, term_encode_out, term_decode_in, (x"11", x"22"));
			test_expect_eq(test, clk, term_terminal_out, term_terminal_in, (x"11", x"22"));

			msg_write(clk, term_encode_out, term_decode_in, (x"1b", x"5f", x"aa", x"1b", x"5c"));
			test_expect_eq(test, clk, term_decode_out, term_encode_in, to_buffer(x"aa"));
			test_expect_eq(test, term_decode_out.last, '1');

			msg_write(clk, term_encode_out, term_decode_in, (x"1b", x"1b", x"5f", x"bb", x"1b", x"1b", x"5c", x"33"));
			test_expect_eq(test, clk, term_terminal_out, term_terminal_in, to_buffer(x"1b"));
			test_expect_eq(test, clk, term_decode_out, term_encode_in, (x"bb", x"1b"));
			test_expect_eq(test, term_decode_out.last, '1');
			test_expect_eq(test, clk, term_terminal_out, term_terminal_in, to_buffer(x"33"));

			msg_write(clk, term_encode_out, term_decode_in, (x"1b", x"44", x"1b", x"5f", x"cc", x"1b", x"dd", x"1b", x"5c", x"55"));
			test_expect_eq(test, clk, term_terminal_out, term_terminal_in, (x"1b", x"44"));
			test_expect_eq(test, clk, term_decode_out, term_encode_in, (x"cc", x"1b", x"dd"));
			test_expect_eq(test, term_decode_out.last, '1');
			test_expect_eq(test, clk, term_terminal_out, term_terminal_in, to_buffer(x"55"));
		end procedure;

		procedure do_test_esc_encode is
		begin
			test_start(test, "ASCIIEscapeEncode");
			esc_encode_in.accept <= '0';
			esc_decode_in.accept <= '0';
			msg_write(clk, esc_decode_out, esc_encode_in,
				(x"32", x"14", x"11",        x"7f",        x"44", x"00"));
			test_expect_eq(test, clk, esc_encode_out, esc_decode_in,
				(x"32", x"14", x"7f", x"51", x"7f", x"7f", x"44", x"7f", x"40"));
			test_expect_eq(test, esc_encode_out.last, '1');
		end procedure;

		procedure do_test_esc_decode is
		begin
			test_start(test, "ASCIIEscapeDecode");
			esc_encode_in.accept <= '0';
			esc_decode_in.accept <= '0';
			msg_write(clk, esc_encode_out, esc_decode_in,
				(x"32", x"0d", x"14", x"7f", x"51", x"7f", x"7f", x"44", x"7f", x"40"));
			test_expect_eq(test, clk, esc_decode_out, esc_encode_in,
				(x"32",        x"14", x"11",        x"7f",        x"44", x"00"));
			test_expect_eq(test, esc_decode_out.last, '1');

			msg_write(clk, esc_encode_out, esc_decode_in,
				(x"32", x"33", x"34", x"35"));
			test_expect_eq(test, clk, esc_decode_out, esc_encode_in,
				(x"32", x"33", x"34", x"35"));
			test_expect_eq(test, esc_decode_out.last, '1');

			msg_write(clk, esc_encode_out, esc_decode_in,
				(x"32", x"33", x"7f"));
			test_expect_eq(test, clk, esc_decode_out, esc_encode_in,
				(x"32", x"33", x"7f"));
			test_expect_eq(test, esc_decode_out.last, '1');

			msg_write(clk, esc_encode_out, esc_decode_in,
				(x"32", x"33", x"0d"));
			test_expect_eq(test, clk, esc_decode_out, esc_encode_in,
				(x"32", x"33"));
			test_expect_eq(test, esc_decode_out.last, '1');

			msg_write(clk, esc_encode_out, esc_decode_in,
				to_buffer(x"0d"));
			-- Nothing expected.

			msg_write(clk, esc_encode_out, esc_decode_in,
				(x"7f", x"ff"));
			test_expect_eq(test, clk, esc_decode_out, esc_encode_in,
				to_buffer(x"1f"));
			test_expect_eq(test, esc_decode_out.last, '1');
		end procedure;

		procedure do_test_file_write is
		begin
			test_start(test, "FileWrite");
			msg_write(clk, file_decode_out, file_encode_in, string_encode("Hello World!"));
		end procedure;

		procedure do_test_file_read is
		begin
			test_start(test, "FileRead");
			test_expect_eq(test, clk, file_decode_out, file_encode_in,
				string_encode("Hello World!") & to_buffer(x"00"));
			test_expect_eq(test, file_decode_out.last, '1');
		end procedure;

		procedure do_test_segment_encode is
		begin
			test_start(test, "SegmentEncode");
			segment_encode_in.accept <= '0';
			segment_decode_in.accept <= '0';
			msg_write(clk, segment_decode_out, segment_encode_in,
				(x"01", x"02"));
			test_expect_eq(test, clk, segment_encode_out, segment_decode_in,
				(x"01", x"02", x"45"));
			test_expect_eq(test, segment_encode_out.last, '1');

			msg_write(clk, segment_decode_out, segment_encode_in,
				(x"01", x"02", x"03", x"04"));
			test_expect_eq(test, clk, segment_encode_out, segment_decode_in,
				(x"01", x"02", x"03", x"04", x"45"));
			test_expect_eq(test, segment_encode_out.last, '1');

			msg_write(clk, segment_decode_out, segment_encode_in,
				(x"01", x"02", x"03", x"04", x"05"));
			test_expect_eq(test, clk, segment_encode_out, segment_decode_in,
				(x"01", x"02", x"03", x"04", x"43"));
			test_expect_eq(test, segment_encode_out.last, '1');
			test_expect_eq(test, clk, segment_encode_out, segment_decode_in,
				(x"05", x"45"));
			test_expect_eq(test, segment_encode_out.last, '1');

			msg_write(clk, segment_decode_out, segment_encode_in,
				(x"01", x"02", x"03", x"04",        x"05", x"06", x"07", x"08",        x"09"));
			test_expect_eq(test, clk, segment_encode_out, segment_decode_in,
				(x"01", x"02", x"03", x"04", x"43", x"05", x"06", x"07", x"08", x"43", x"09", x"45"));
		end procedure;

		procedure do_test_segment_decode is
		begin
			test_start(test, "SegmentDecode");
			segment_encode_in.accept <= '0';
			segment_decode_in.accept <= '0';
			msg_write(clk, segment_encode_out, segment_decode_in,
				(x"01", x"02", x"45"));
			test_expect_eq(test, clk, segment_decode_out, segment_encode_in,
				(x"01", x"02"));
			test_expect_eq(test, segment_decode_out.last, '1');

			msg_write(clk, segment_encode_out, segment_decode_in,
				(x"01", x"02", x"03", x"04", x"45"));
			test_expect_eq(test, clk, segment_decode_out, segment_encode_in,
				(x"01", x"02", x"03", x"04"));
			test_expect_eq(test, segment_decode_out.last, '1');

			msg_write(clk, segment_encode_out, segment_decode_in,
				(x"01", x"02", x"03", x"04", x"43"));
			msg_write(clk, segment_encode_out, segment_decode_in,
				(x"05", x"45"));
			test_expect_eq(test, segment_decode_out.last, '0');
			test_expect_eq(test, clk, segment_decode_out, segment_encode_in,
				(x"01", x"02", x"03", x"04", x"05"));
			test_expect_eq(test, segment_decode_out.last, '1');

			msg_write(clk, segment_encode_out, segment_decode_in,
				(x"01", x"02", x"03", x"04", x"43"));
			msg_write(clk, segment_encode_out, segment_decode_in,
				(x"05", x"06", x"07", x"08", x"43"));
			msg_write(clk, segment_encode_out, segment_decode_in,
				(x"09", x"45"));
			test_expect_eq(test, clk, segment_decode_out, segment_encode_in,
				(x"01", x"02", x"03", x"04"));
			test_expect_eq(test, segment_decode_out.last, '0');
			test_expect_eq(test, clk, segment_decode_out, segment_encode_in,
				(x"05", x"06", x"07", x"08"));
			test_expect_eq(test, segment_decode_out.last, '0');
			test_expect_eq(test, clk, segment_decode_out, segment_encode_in,
				to_buffer(x"09"));
			test_expect_eq(test, segment_decode_out.last, '1');
		end procedure;

		procedure do_test_crc8_encode is
		begin
			test_start(test, "Crc8Encode");
			crc8_encode_in.accept <= '0';
			crc8_decode_in.accept <= '0';
			msg_write(clk, crc8_decode_out, crc8_encode_in,
				to_buffer(x"31"));
			test_expect_eq(test, clk, crc8_encode_out, crc8_decode_in,
				(x"31", x"5e"));
			test_expect_eq(test, crc8_encode_out.last, '1');

			msg_write(clk, crc8_decode_out, crc8_encode_in,
				(x"31", x"32"));
			test_expect_eq(test, clk, crc8_encode_out, crc8_decode_in,
				(x"31", x"32", x"54"));
			test_expect_eq(test, crc8_encode_out.last, '1');

			msg_write(clk, crc8_decode_out, crc8_encode_in,
				(x"31", x"32", x"33"));
			test_expect_eq(test, clk, crc8_encode_out, crc8_decode_in,
				(x"31", x"32", x"33", x"fc"));
			test_expect_eq(test, crc8_encode_out.last, '1');

			msg_write(clk, crc8_decode_out, crc8_encode_in,
				(x"31", x"32"));
			msg_write(clk, crc8_decode_out, crc8_encode_in,
				(x"31", x"32", x"33"));
			test_expect_eq(test, clk, crc8_encode_out, crc8_decode_in,
				(x"31", x"32", x"54", x"31", x"32", x"33", x"fc"));
		end procedure;

		procedure do_test_crc8_decode is
		begin
			test_start(test, "Crc8Decode");
			crc8_encode_in.accept <= '0';
			crc8_decode_in.accept <= '0';
			msg_write(clk, crc8_encode_out, crc8_decode_in,
				(x"31", x"5e"));
			test_expect_eq(test, clk, crc8_decode_out, crc8_encode_in,
				to_buffer(x"31"));
			test_expect_eq(test, crc8_decode_out.last, '1');

			msg_write(clk, crc8_encode_out, crc8_decode_in,
				(x"31", x"32", x"54"));
			test_expect_eq(test, clk, crc8_decode_out, crc8_encode_in,
				(x"31", x"32"));
			test_expect_eq(test, crc8_decode_out.last, '1');

			msg_write(clk, crc8_encode_out, crc8_decode_in,
				(x"31", x"32", x"33", x"fc"));
			test_expect_eq(test, clk, crc8_decode_out, crc8_encode_in,
				(x"31", x"32", x"33"));
			test_expect_eq(test, crc8_decode_out.last, '1');

			msg_write(clk, crc8_encode_out, crc8_decode_in,
				(x"31", x"5f")); -- bad
			msg_write(clk, crc8_encode_out, crc8_decode_in,
				(x"32", x"5e")); -- bad
			msg_write(clk, crc8_encode_out, crc8_decode_in,
				(x"31", x"32", x"54"));
			test_expect_eq(test, clk, crc8_decode_out, crc8_encode_in,
				(x"31", x"32"));

			msg_write(clk, crc8_encode_out, crc8_decode_in,
				(x"31", x"5e"));
			msg_write(clk, crc8_encode_out, crc8_decode_in,
				(x"31", x"33", x"54")); -- bad
			test_expect_eq(test, clk, crc8_decode_out, crc8_encode_in,
				to_buffer(x"31"));

			-- Drop because of overflow.
			msg_write(clk, crc8_encode_out, crc8_decode_in,
				(x"00", x"01", x"02", x"03", x"04", x"05", x"06", x"07", x"08", x"09", x"0a", x"0b", x"0c", x"0d", x"0e", x"0f",
				 x"10", x"11", x"12", x"13", x"14", x"15", x"16", x"17", x"18", x"19", x"1a", x"1b", x"1c", x"1d", x"1e", x"1f"));
			msg_write(clk, crc8_encode_out, crc8_decode_in,
				(x"31", x"5e"));
			test_expect_eq(test, clk, crc8_decode_out, crc8_encode_in,
				to_buffer(x"31"));
		end procedure;

		procedure do_test_crc16_encode is
		begin
			test_start(test, "Crc16Encode");
			crc16_encode_in.accept <= '0';
			crc16_decode_in.accept <= '0';
			msg_write(clk, crc16_decode_out, crc16_encode_in,
				to_buffer(x"31"));
			test_expect_eq(test, clk, crc16_encode_out, crc16_decode_in,
				(x"31", x"49", x"d6"));
			test_expect_eq(test, crc16_encode_out.last, '1');

			msg_write(clk, crc16_decode_out, crc16_encode_in,
				(x"31", x"32"));
			test_expect_eq(test, clk, crc16_encode_out, crc16_decode_in,
				(x"31", x"32", x"77", x"a2"));
			test_expect_eq(test, crc16_encode_out.last, '1');

			msg_write(clk, crc16_decode_out, crc16_encode_in,
				(x"31", x"32", x"33"));
			test_expect_eq(test, clk, crc16_encode_out, crc16_decode_in,
				(x"31", x"32", x"33", x"1c", x"84"));
			test_expect_eq(test, crc16_encode_out.last, '1');

			msg_write(clk, crc16_decode_out, crc16_encode_in,
				to_buffer(x"31"));
			msg_write(clk, crc16_decode_out, crc16_encode_in,
				(x"31", x"32"));
			test_expect_eq(test, clk, crc16_encode_out, crc16_decode_in,
				(x"31", x"49", x"d6", x"31", x"32", x"77", x"a2"));
		end procedure;

		procedure do_test_crc16_decode is
		begin
			test_start(test, "Crc16Decode");
			crc16_encode_in.accept <= '0';
			crc16_decode_in.accept <= '0';

			msg_write(clk, crc16_encode_out, crc16_decode_in,
				(x"31", x"49", x"d6"));
			test_expect_eq(test, clk, crc16_decode_out, crc16_encode_in,
				to_buffer(x"31"));
			test_expect_eq(test, crc16_decode_out.last, '1');

			msg_write(clk, crc16_encode_out, crc16_decode_in,
				(x"31", x"32", x"77", x"a2"));
			test_expect_eq(test, clk, crc16_decode_out, crc16_encode_in,
				(x"31", x"32"));
			test_expect_eq(test, crc16_decode_out.last, '1');

			msg_write(clk, crc16_encode_out, crc16_decode_in,
				(x"31", x"32", x"33", x"34", x"1c", x"84")); -- bad
			msg_write(clk, crc16_encode_out, crc16_decode_in,
				(x"00", x"31", x"32", x"33", x"1c", x"84")); -- bad
			msg_write(clk, crc16_encode_out, crc16_decode_in,
				(x"ff", x"ff")); -- ok, but empty
			msg_write(clk, crc16_encode_out, crc16_decode_in,
				(x"fe", x"ff")); -- bad and empty

			msg_write(clk, crc16_encode_out, crc16_decode_in,
				(x"31", x"32", x"33", x"1c", x"84"));
			test_expect_eq(test, clk, crc16_decode_out, crc16_encode_in,
				(x"31", x"32", x"33"));
			test_expect_eq(test, crc16_decode_out.last, '1');
		end procedure;

		procedure do_test_arq_encode is
		begin
			test_start(test, "ArqEncode");
			arq_encode_in.accept <= '0';
			arq_decode_in.accept <= '0';

			arq_reconnect <= '1';
			wait until rising_edge(clk);
			arq_reconnect <= '0';

			-- Expect a few resets.
			test_expect_eq(test, clk, arq_encode_out, arq_decode_in,
				(x"40", x"40", x"40"));
			test_expect_eq(test, arq_encode_out.last, '1');

			-- Accept the reset.
			msg_write(clk, arq_encode_out, arq_decode_in,
				to_buffer(x"c0"));

			-- Send actual data.
			msg_write(clk, arq_decode_out, arq_encode_in,
				(x"31", x"32", x"33"));
			test_expect_eq(test, clk, arq_encode_out, arq_decode_in,
				(x"01", x"31", x"32", x"33"));
			msg_write(clk, arq_encode_out, arq_decode_in,
				to_buffer(x"c1"));

			-- Send more data.
			msg_write(clk, arq_decode_out, arq_encode_in,
				(x"34", x"35"));
			msg_write(clk, arq_decode_out, arq_encode_in,
				to_buffer(x"36"));
			test_expect_eq(test, clk, arq_encode_out, arq_decode_in,
				(x"02", x"34", x"35"));

			-- No ack; expect retransmit.
			test_expect_eq(test, clk, arq_encode_out, arq_decode_in,
				(x"02", x"34", x"35"));

			-- Wrong ack; expect retransmit.
			msg_write(clk, arq_encode_out, arq_decode_in,
				to_buffer(x"c1"));
			test_expect_eq(test, clk, arq_encode_out, arq_decode_in,
				(x"02", x"34", x"35"));

			-- Correct ack.
			msg_write(clk, arq_encode_out, arq_decode_in,
				to_buffer(x"c2"));
			test_expect_eq(test, clk, arq_encode_out, arq_decode_in,
				(x"03", x"36"));
			msg_write(clk, arq_encode_out, arq_decode_in,
				to_buffer(x"c3"));
		end procedure;

		procedure do_test_arq_decode is
		begin
			test_start(test, "ArqDecode");
			arq_encode_in.accept <= '0';
			arq_decode_in.accept <= '0';

			arq_reconnect <= '1';
			wait until rising_edge(clk);
			arq_reconnect <= '0';

			-- Handle encode reset
			test_expect_eq(test, clk, arq_encode_out, arq_decode_in,
				to_buffer(x"40"));
			msg_write(clk, arq_encode_out, arq_decode_in,
				to_buffer(x"c0"));

			-- Do decode reset.
			msg_write(clk, arq_encode_out, arq_decode_in,
				to_buffer(x"40"));
			test_expect_eq(test, clk, arq_encode_out, arq_decode_in,
				to_buffer(x"c0"));

			-- Decode
			msg_write(clk, arq_encode_out, arq_decode_in,
				(x"01", x"31", x"32", x"33"));
			test_expect_eq(test, clk, arq_decode_out, arq_encode_in,
				(x"31", x"32", x"33"));
			test_expect_eq(test, clk, arq_encode_out, arq_decode_in,
				to_buffer(x"c1"));

			-- Retransmit same message.
			msg_write(clk, arq_encode_out, arq_decode_in,
				(x"01", x"31", x"32", x"33"));
			test_expect_eq(test, clk, arq_encode_out, arq_decode_in,
				to_buffer(x"c1"));
			test_expect_eq(test, arq_decode_out.valid, '0');

			-- Decode next.
			msg_write(clk, arq_encode_out, arq_decode_in,
				(x"02", x"34", x"35"));
			test_expect_eq(test, clk, arq_decode_out, arq_encode_in,
				(x"34", x"35"));
			test_expect_eq(test, clk, arq_encode_out, arq_decode_in,
				to_buffer(x"c2"));

			-- Simultaneous encode/decode.
			msg_write(clk, arq_decode_out, arq_encode_in,
				(x"41", x"42", x"43"));
			msg_write(clk, arq_encode_out, arq_decode_in,
				(x"03", x"36"));
			test_expect_eq(test, clk, arq_decode_out, arq_encode_in,
				to_buffer(x"36"));
			test_expect_eq(test, clk, arq_encode_out, arq_decode_in,
				(x"01", x"41", x"42", x"43"));
			msg_write(clk, arq_encode_out, arq_decode_in,
				to_buffer(x"c1"));
			test_expect_eq(test, clk, arq_encode_out, arq_decode_in,
				to_buffer(x"c3"));
		end procedure;

		procedure do_full_stack is
		begin
			test_start(test, "Full stack");
			arq_encode_in <= msg_highz;
			crc8_encode_in <= msg_highz;
			esc_encode_in <= msg_highz;
			term_encode_in <= msg_highz;
			term_decode_in <= msg_highz;
			esc_decode_in <= msg_highz;
			crc8_decode_in <= msg_highz;
			arq_decode_in <= msg_highz;
			segment_decode_in <= msg_highz;

			full_stack <= true;

			segment_encode_in.accept <= '0';

			arq_reconnect <= '1';
			wait until rising_edge(clk);
			arq_reconnect <= '0';

			msg_write(clk, segment_decode_out, segment_encode_in,
				(x"31", x"32"));
			msg_write(clk, segment_decode_out, segment_encode_in,
				(x"33", x"34", x"35", x"36", x"37", x"38"));
			msg_write(clk, segment_decode_out, segment_encode_in,
				(x"39", x"3a", x"3b"));
			msg_write(clk, segment_decode_out, segment_encode_in,
				to_buffer(x"3c"));
			msg_write(clk, segment_decode_out, segment_encode_in,
				(x"3d", x"3e", x"3f", x"40", x"41", x"42", x"43"));
			msg_write(clk, segment_decode_out, segment_encode_in,
				(x"44", x"45", x"46", x"47", x"48", x"49", x"4a", x"4b", x"4c", x"4d"));
			msg_write(clk, segment_decode_out, segment_encode_in,
				(x"4e", x"4f"));

			test_expect_eq(test, clk, segment_decode_out, segment_encode_in,
				(x"31", x"32"));
			test_expect_eq(test, clk, segment_decode_out, segment_encode_in,
				(x"33", x"34", x"35", x"36", x"37", x"38"));
			test_expect_eq(test, clk, segment_decode_out, segment_encode_in,
				(x"39", x"3a", x"3b"));
			test_expect_eq(test, clk, segment_decode_out, segment_encode_in,
				to_buffer(x"3c"));
			test_expect_eq(test, clk, segment_decode_out, segment_encode_in,
				(x"3d", x"3e", x"3f", x"40", x"41", x"42", x"43"));
			test_expect_eq(test, clk, segment_decode_out, segment_encode_in,
				(x"44", x"45", x"46", x"47", x"48", x"49", x"4a", x"4b", x"4c", x"4d"));
			test_expect_eq(test, clk, segment_decode_out, segment_encode_in,
				(x"4e", x"4f"));

			full_stack <= false;

			arq_encode_in <= msg_term;
			crc8_encode_in <= msg_term;
			esc_encode_in <= msg_term;
			term_encode_in <= msg_term;
			term_decode_in <= msg_term;
			esc_decode_in <= msg_term;
			crc8_decode_in <= msg_term;
			arq_decode_in <= msg_term;
			segment_decode_in <= msg_term;
		end procedure;

	begin
		id_out := x"aabb";
		id_out2 := x"ccdd";

		var_in <= TestStore_pkg.var_in_default;
		var_in2 <= TestStore_pkg.var_in_default;

		axi_init(axi_m2s, axi_s2m);
		sync_init(sync_in, sync_out);

		file_encode_in.accept <= '0';

		wait until rising_edge(clk) and rstn = '1';
		wait until rising_edge(clk);
		wait until rising_edge(clk);

		test_init(test);

		do_test_initial;
		do_test_set;

		do_test_axi;

		do_test_hello;
		do_test_update_single;
		do_test_update_multi;
		do_test_update_burst;
		do_test_update_out;

		do_test_access_ro;
		do_test_access_wo;
		do_test_access_na;

		do_test_chained_hello;
		do_test_chained_update;
		do_test_chained_update_out;

		do_test_uart_auto_baud;
		do_test_uart_tx;
		do_test_uart_tx_fc;
		do_test_uart_rx;
		do_test_uart_rx_fc;

		do_test_term_inject;
		do_test_term_extract;

		do_test_esc_encode;
		do_test_esc_decode;

		do_test_file_write;
		do_test_file_read;

		do_test_segment_encode;
		do_test_segment_decode;

		do_test_crc8_encode;
		do_test_crc8_decode;

		do_test_crc16_encode;
		do_test_crc16_decode;

		do_test_arq_encode;
		do_test_arq_decode;

		do_full_stack;

		test_verbose(test);
		-- ...

		test_finish(test);

		wait for 1 us;
		done <= true;
		wait;
	end process;

end behav;
