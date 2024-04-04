-- SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
--
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

use work.libstored_pkg.all;
use work.ExampleFpga_pkg;
use work.ExampleFpga2_pkg;

entity example_9_fpga is
	generic (
		SYSTEM_CLK_FREQ : integer := 100e6;
		SIMULATION : boolean := false
--pragma translate_off
			or true
--pragma translate_on
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		rx : in std_logic := '1';
		tx : out std_logic;
		cts : in std_logic := '0';
		rts : out std_logic
	);
end entity;

architecture rtl of example_9_fpga is
	function var_access return ExampleFpga_pkg.var_access_t is
		variable v : ExampleFpga_pkg.var_access_t;
	begin
		v := ExampleFpga_pkg.VAR_ACCESS_RW;

		-- Save some LUTs by limiting how we are going to access these variables.
		v.\t (clk)\ := ACCESS_WO;
		v.\default register write count\ := ACCESS_WO;
		v.\read-only register\ := ACCESS_RO;
		return v;
	end function;

	signal var_out : ExampleFpga_pkg.var_out_t;
	signal var_in : ExampleFpga_pkg.var_in_t;
	signal var2_out : ExampleFpga2_pkg.var_out_t;
	signal var2_in : ExampleFpga2_pkg.var_in_t;
	signal sync_in, sync_out, sync_chained_in, sync_chained_out : msg_t;
	signal sync_chained_id : unsigned(15 downto 0);

	signal segment_encode_in, segment_decode_out : msg_t;
	signal arq_encode_in, arq_decode_out : msg_t;
	signal crc16_encode_in, crc16_decode_out : msg_t;
	signal ascii_encode_in, ascii_decode_out : msg_t;
	signal term_encode_in, term_decode_out : msg_t;
	signal uart_encode_in, uart_decode_out : msg_t;

	signal segment_mtu, arq_mtu, crc16_mtu, ascii_mtu, term_mtu, uart_mtu  : natural := 0;

	signal clk_cnt, write_cnt : unsigned(31 downto 0);
begin

	-----------------------------------------
	-- Store instances
	-----------------------------------------

	store_inst : entity work.ExampleFpga_hdl
		generic map (
			SYSTEM_CLK_FREQ => SYSTEM_CLK_FREQ,
			VAR_ACCESS => var_access,
			SIMULATION => SIMULATION
		)
		port map (
			clk => clk,
			rstn => rstn,
			var_out => var_out,
			var_in => var_in,
			sync_in => sync_in,
			sync_out => sync_out,
			sync_id => sync_chained_id,
			sync_chained_in => sync_chained_out,
			sync_chained_out => sync_chained_in
		);

	store2_inst : entity work.ExampleFpga2_hdl
		generic map (
			SYSTEM_CLK_FREQ => SYSTEM_CLK_FREQ,
			SIMULATION => SIMULATION
		)
		port map (
			clk => clk,
			rstn => rstn,
			var_out => var2_out,
			var_in => var2_in,
			sync_in => sync_chained_in,
			sync_out => sync_chained_out,
			sync_chained_id => sync_chained_id
		);


	-----------------------------------------
	-- Protocol stack for Synchronizer
	-----------------------------------------

	segment_encode_in <= sync_out;
	sync_in <= segment_decode_out;

	SegmentationLayer_inst : entity work.SegmentationLayer
		generic map (
			MTU => 24
		)
		port map (
			clk => clk,
			rstn => rstn,
			encode_in => segment_encode_in,
			encode_out => arq_encode_in,
			decode_in => arq_decode_out,
			decode_out => segment_decode_out,

			-- The following two lines are not required, but they
			-- statically check the configured MTU.
			mtu_decode_in => arq_mtu,
			mtu_decode_out => segment_mtu
		);

	ArqLayer_inst : entity work.ArqLayer
		generic map (
			MTU => 24,
			SYSTEM_CLK_FREQ => SYSTEM_CLK_FREQ
		)
		port map (
			clk => clk,
			rstn => rstn,
			encode_in => arq_encode_in,
			encode_out => crc16_encode_in,
			decode_in => crc16_decode_out,
			decode_out => arq_decode_out,
			mtu_decode_in => crc16_mtu,
			mtu_decode_out => arq_mtu
		);

	Crc16Layer_inst : entity work.Crc16Layer
		generic map (
			MTU => 25
		)
		port map (
			clk => clk,
			rstn => rstn,
			encode_in => crc16_encode_in,
			encode_out => ascii_encode_in,
			decode_in => ascii_decode_out,
			decode_out => crc16_decode_out,
			mtu_decode_in => term_mtu,
			mtu_decode_out => crc16_mtu
		);

	ASCIIEscapeLayer_inst : entity work.ASCIIEscapeLayer
		port map (
			clk => clk,
			rstn => rstn,
			encode_in => ascii_encode_in,
			encode_out => term_encode_in,
			decode_in => term_decode_out,
			decode_out => ascii_decode_out,
			mtu_decode_in => term_mtu,
			mtu_decode_out => ascii_mtu
		);

	TerminalLayer_inst : entity work.TerminalLayer
		port map (
			clk => clk,
			rstn => rstn,
			encode_in => term_encode_in,
			encode_out => uart_encode_in,
			decode_in => uart_decode_out,
			decode_out => term_decode_out,
			mtu_decode_in => uart_mtu,
			mtu_decode_out => term_mtu
		);

	uart_g : if not SIMULATION generate
	begin
		UARTLayer_inst : entity work.UARTLayer
			generic map (
				SYSTEM_CLK_FREQ => SYSTEM_CLK_FREQ,
				XON_XOFF => true
			)
			port map (
				clk => clk,
				rstn => rstn,
				encode_in => uart_encode_in,
				decode_out => uart_decode_out,
				rx => rx,
				tx => tx,
				cts => cts,
				rts => rts,
				mtu_decode_out => uart_mtu
			);
	end generate;

--pragma translate_off
	xsim_g : if SIMULATION generate
		function pipe_name return string is
		begin
			if ExampleFpga_pkg.WIN32 then
				return "\\.\pipe\9_fpga";
			else
				return "/tmp/9_fpga";
			end if;
		end function;
	begin
		XsimLayer_inst : entity work.XsimLayer
			generic map (
				PIPE_PREFIX => pipe_name
			)
			port map (
				clk => clk,
				rstn => rstn,
				encode_in => uart_encode_in,
				decode_out => uart_decode_out,
				mtu_decode_out => uart_mtu
			);
	end generate;
--pragma translate_on


	-----------------------------------------
	-- Implementation of application
	-----------------------------------------

	process(clk)
	begin
		if rising_edge(clk) then
			var_in <= ExampleFpga_pkg.var_in_default;
			var2_in <= ExampleFpga2_pkg.var_in_default;



			-- implementation of ExampleFpga/t (clk)
			var_in.\t (clk)\.value <= resize(clk_cnt, var_in.\t (clk)\.value'length);
			var_in.\t (clk)\.we <= '1';
			clk_cnt <= clk_cnt + 1;

			if rstn /= '1' then
				var_in.\t (clk)\.we <= '0';
				clk_cnt <= (others => '0');
			end if;



			-- writes to ExampleFpga/read-only register should be ignored
			var_in.\read-only register\.value <= x"abcd";
			var_in.\read-only register\.we <= '1';



			-- implementation of ExampleFpga/default register write count
			if var_out.\default register\.updated = '1' then
				write_cnt <= write_cnt + 1;
			end if;

			var_in.\default register write count\.value <=
				resize(write_cnt, var_in.\default register write count\.value'length);
			var_in.\default register write count\.we <= '1';

			if rstn /= '1' then
				write_cnt <= (others => '0');
				var_in.\default register write count\.we <= '0';
			end if;
		end if;
	end process;

end rtl;
