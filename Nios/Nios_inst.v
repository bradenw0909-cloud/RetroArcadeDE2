	Nios u0 (
		.clk_clk              (<connected-to-clk_clk>),              //           clk.clk
		.vga_CLK              (<connected-to-vga_CLK>),              //           vga.CLK
		.vga_HS               (<connected-to-vga_HS>),               //              .HS
		.vga_VS               (<connected-to-vga_VS>),               //              .VS
		.vga_BLANK            (<connected-to-vga_BLANK>),            //              .BLANK
		.vga_SYNC             (<connected-to-vga_SYNC>),             //              .SYNC
		.vga_R                (<connected-to-vga_R>),                //              .R
		.vga_G                (<connected-to-vga_G>),                //              .G
		.vga_B                (<connected-to-vga_B>),                //              .B
		.sram_DQ              (<connected-to-sram_DQ>),              //          sram.DQ
		.sram_ADDR            (<connected-to-sram_ADDR>),            //              .ADDR
		.sram_LB_N            (<connected-to-sram_LB_N>),            //              .LB_N
		.sram_UB_N            (<connected-to-sram_UB_N>),            //              .UB_N
		.sram_CE_N            (<connected-to-sram_CE_N>),            //              .CE_N
		.sram_OE_N            (<connected-to-sram_OE_N>),            //              .OE_N
		.sram_WE_N            (<connected-to-sram_WE_N>),            //              .WE_N
		.rom_data_low_export  (<connected-to-rom_data_low_export>),  //  rom_data_low.export
		.rom_data_high_export (<connected-to-rom_data_high_export>), // rom_data_high.export
		.keys_export          (<connected-to-keys_export>),          //          keys.export
		.row_ctrl_export      (<connected-to-row_ctrl_export>)       //      row_ctrl.export
	);

