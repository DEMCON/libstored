VHDL
====

Store entity
------------

.. object:: Store_hdl

   The generated stores have the following entity definition (for the
   ExampleFpga store in this case):

   .. code-block:: vhdl

      entity ExampleFpga_hdl is
         generic (
            SYSTEM_CLK_FREQ : integer := 100e6;
            SYNC_OUT_INTERVAL_s : real := 0.1;
            ID : natural := 0;
            AXI_SLAVE : boolean := true;
            BUFFER_AFTER_N_VARS : positive := 4;
            VAR_ACCESS : ExampleFpga_pkg.var_access_t := ExampleFpga_pkg.VAR_ACCESS_RW;
            SIMULATION : boolean := false
         );
         port (
            clk : in std_logic;
            rstn : in std_logic;
            var_out : out ExampleFpga_pkg.var_out_t;
            var_in : in ExampleFpga_pkg.var_in_t := ExampleFpga_pkg.var_in_default;
            sync_in : in msg_t := msg_term;
            sync_out : out msg_t;
            sync_id : out unsigned(15 downto 0);
            sync_chained_id : in unsigned(15 downto 0) := (others => '0');
            sync_chained_in : in msg_t := msg_term;
            sync_chained_out : out msg_t;
            sync_out_trigger : in std_logic := '0';
            sync_out_hold : in std_logic := '0';
            sync_out_irq : out std_logic;
            sync_out_have_changes : out std_logic;
            sync_connected : out std_logic;
            sync_in_busy : out std_logic;
            s_axi_araddr  : in  std_logic_vector(31 downto 0) := (others => '-');
            s_axi_arready : out std_logic;
            s_axi_arvalid : in  std_logic := '0';
            s_axi_awaddr  : in  std_logic_vector(31 downto 0) := (others => '-');
            s_axi_awready : out std_logic;
            s_axi_awvalid : in  std_logic := '0';
            s_axi_bready  : in  std_logic := '0';
            s_axi_bresp   : out std_logic_vector(1 downto 0);
            s_axi_bvalid  : out std_logic;
            s_axi_rdata   : out std_logic_vector(31 downto 0);
            s_axi_rready  : in  std_logic := '0';
            s_axi_rresp   : out std_logic_vector(1 downto 0);
            s_axi_rvalid  : out std_logic;
            s_axi_wdata   : in  std_logic_vector(31 downto 0) := (others => '-');
            s_axi_wready  : out std_logic;
            s_axi_wvalid  : in  std_logic := '0'
         );
      end ExampleFpga_hdl;

   SYSTEM_CLK_FREQ : integer := 100e6
      Clock frequency of ``clk``.

   SYNC_OUT_INTERVAL_s : real := 0.1
      Interval to send out Synchronizer messages.

   ID : natural := 0
      The ID used for Hello messages.  If 0, the value is automatically
      determined.

   AXI_SLAVE : boolean := true
      Enable the AXI slave interface when set to ``true``.

   BUFFER_AFTER_N_VARS : positive := 4
      Inject buffers after the given number of variables.  A lower number
      increases the latency of handling sync messages, but reduces the pipeline
      length.

   VAR_ACCESS : ExampleFpga_pkg.var_access_t := ExampleFpga_pkg.VAR_ACCESS_RW
      Allowed access of the variables within the store.  The default, all
      read-write, is the most generic, but has the highest resource usage.

      To override only a few access settings, assign a function to ``VAR_ACCESS``,
      with an implementation like this:

      .. code-block:: vhdl
         :force:

         function var_access return ExampleFpga_pkg.var_access_t is
            variable v : ExampleFpga_pkg.var_access_t;
         begin
            v := ExampleFpga_pkg.VAR_ACCESS_RW;

            -- Save some LUTs by limiting how we are going to access these variables.
            v.\t_clk\ := ACCESS_WO;
            v.\default_register_write_count\ := ACCESS_WO;
            return v;
         end function;

   SIMULATION : boolean := false
      When ``true``, reduce the sync timing, such that the interval is
      better suitable for (slow) simulation.

   clk : in std_logic;
      System clock.

   rstn : in std_logic;
      Low-active reset.

   var_out : out ExampleFpga_pkg.var_out_t
      All variables within the store.

   var_in : in ExampleFpga_pkg.var_in_t := ExampleFpga_pkg.var_in_default
      Interface to write variables.

   sync_in : in msg_t := msg_term
      Synchronization input.  Connect ``sync_in`` and ``sync_out`` to the
      protocol stack.  Set to ``msg_term`` to disable synchronization.

   sync_out : out msg_t
      Synchronization output.  Connect ``sync_in`` and ``sync_out`` to the
      protocol stack.  Set to ``msg_term`` to disable synchronization.

   sync_id : out unsigned(15 downto 0)
      The used ID for the Hello message. This equals ``ID`` when ``ID`` is
      non-zero.  Otherwise, a non-zero value is determined. The value should be
      constant.

   sync_chained_id : in unsigned(15 downto 0) := (others => '0')
      The ``sync_id`` of the chained store. If ``ID`` is zero, a
      non-conflicting value is chosen for this store's ``sync_id``.

   sync_chained_in : in msg_t := msg_term
      The ``sync_out`` of a chained store.

   sync_chained_out : out msg_t
      The ``sync_in`` of a chained store.

   sync_out_trigger : in std_logic := '0'
      Trigger an immediate sequence of Synchronizer messages when set to high
      for one clock cycle. When kept high, multiple sync sequences can be sent
      back to back.

   sync_out_hold : in std_logic := '0'
      When high, prevent automatically sending out Synchronizer messages.

   sync_out_irq : out std_logic
      Interrupt flag that indicates that there is at least one Synchronizer
      message to be passed over ``sync_out``.

   sync_out_have_changes : out std_logic
      Flag that indicates that a variable has been changed in the store, and
      Synchronization is required. Either flag ``sync_out_trigger``, or wait
      till ``SYNC_OUT_INTERVAL_s`` has passed and synchronization is started.

   sync_connected : out std_logic
      Flag that indicates that we have a connection with a remote Synchronizer
      instance.

   sync_in_busy : out std_logic
      Flag that is high when Synchronizer messages are being processed.

   s_axi_*
      AXI4 LITE slave interface. This is a read-write interface for all store
      variables that are at most 32-bit in size.

Store package
-------------

TODO

