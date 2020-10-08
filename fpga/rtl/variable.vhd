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
		KEY_LENGTH : natural := 8;
		DATA_LENGTH : natural := 32;
		BUFFER_OFFSET : natural := 0;
		LITTLE_ENDIAN := boolean := true;
		BUFFER_CHAIN : boolean := true;
		SIMULATION : boolean := false
--pragma translate_off
			or true
--pragma translate_on
	);
	port (
		clk : in std_logic;
		rstn : in std_logic;

		data__out : out std_logic_vector(DATA_LENGTH - 1 downto 0);
		data__out_changed : out std_logic;
		data__in : in std_logic_vector(DATA_LENGTH - 1 downto 0) := (others => '-');
		data__in_we : in std_logic := '0';

		sync_in_commit : in std_logic := '1';
		sync_out_snapshot : in std_logic := '1';

		-- Input of sync messages.
		-- When sync_in_buffer is true, this variable takes
		-- the first DATA_LENGTH bits and forwards the rest.
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

		-- Sync messages generated (of forwarded) by another variable, for daisy chaining.
		sync_out_len_prev : in std_logic_vector(LEN_LENGTH - 1 downto 0) := (others => '-');
		sync_out_data_prev : in std_logic_vector(7 downto 0) := (others => '-');
		sync_out_last_prev : in std_logic := '-';
		sync_out_valid_prev : in std_logic := '0';
		sync_out_accept_prev : out std_logic
	);
end libstored_variable;

architecture rtl of libstored_variable is
begin
end rtl;

