	component Nios is
		port (
			clk_clk              : in    std_logic                     := 'X';             -- clk
			vga_CLK              : out   std_logic;                                        -- CLK
			vga_HS               : out   std_logic;                                        -- HS
			vga_VS               : out   std_logic;                                        -- VS
			vga_BLANK            : out   std_logic;                                        -- BLANK
			vga_SYNC             : out   std_logic;                                        -- SYNC
			vga_R                : out   std_logic_vector(7 downto 0);                     -- R
			vga_G                : out   std_logic_vector(7 downto 0);                     -- G
			vga_B                : out   std_logic_vector(7 downto 0);                     -- B
			sram_DQ              : inout std_logic_vector(15 downto 0) := (others => 'X'); -- DQ
			sram_ADDR            : out   std_logic_vector(19 downto 0);                    -- ADDR
			sram_LB_N            : out   std_logic;                                        -- LB_N
			sram_UB_N            : out   std_logic;                                        -- UB_N
			sram_CE_N            : out   std_logic;                                        -- CE_N
			sram_OE_N            : out   std_logic;                                        -- OE_N
			sram_WE_N            : out   std_logic;                                        -- WE_N
			rom_data_low_export  : in    std_logic_vector(31 downto 0) := (others => 'X'); -- export
			rom_data_high_export : in    std_logic_vector(31 downto 0) := (others => 'X'); -- export
			keys_export          : in    std_logic_vector(2 downto 0)  := (others => 'X'); -- export
			row_ctrl_export      : out   std_logic_vector(4 downto 0)                      -- export
		);
	end component Nios;

	u0 : component Nios
		port map (
			clk_clk              => CONNECTED_TO_clk_clk,              --           clk.clk
			vga_CLK              => CONNECTED_TO_vga_CLK,              --           vga.CLK
			vga_HS               => CONNECTED_TO_vga_HS,               --              .HS
			vga_VS               => CONNECTED_TO_vga_VS,               --              .VS
			vga_BLANK            => CONNECTED_TO_vga_BLANK,            --              .BLANK
			vga_SYNC             => CONNECTED_TO_vga_SYNC,             --              .SYNC
			vga_R                => CONNECTED_TO_vga_R,                --              .R
			vga_G                => CONNECTED_TO_vga_G,                --              .G
			vga_B                => CONNECTED_TO_vga_B,                --              .B
			sram_DQ              => CONNECTED_TO_sram_DQ,              --          sram.DQ
			sram_ADDR            => CONNECTED_TO_sram_ADDR,            --              .ADDR
			sram_LB_N            => CONNECTED_TO_sram_LB_N,            --              .LB_N
			sram_UB_N            => CONNECTED_TO_sram_UB_N,            --              .UB_N
			sram_CE_N            => CONNECTED_TO_sram_CE_N,            --              .CE_N
			sram_OE_N            => CONNECTED_TO_sram_OE_N,            --              .OE_N
			sram_WE_N            => CONNECTED_TO_sram_WE_N,            --              .WE_N
			rom_data_low_export  => CONNECTED_TO_rom_data_low_export,  --  rom_data_low.export
			rom_data_high_export => CONNECTED_TO_rom_data_high_export, -- rom_data_high.export
			keys_export          => CONNECTED_TO_keys_export,          --          keys.export
			row_ctrl_export      => CONNECTED_TO_row_ctrl_export       --      row_ctrl.export
		);

