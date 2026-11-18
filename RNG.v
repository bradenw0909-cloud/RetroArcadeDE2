module rng (
    input  wire       clk,        // 50 MHz system clock
    input  wire       rst_n,      // Active-low system reset
    input  wire       next_btn,   // ROW_CTRL_BASE[4] pulse from Nios II
    output reg  [2:0] current_seg // 3-bit segment index to ROM address[6:4]
);

    // PART 1: 8-bit Fibonacci LFSR (x^8 + x^6 + x^5 + x^4 + 1)
    reg [7:0] rand_num;
    wire feedback = rand_num[7] ^ rand_num[5] ^ rand_num[4] ^ rand_num[3];

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            rand_num <= 8'b1010_1101;
        else
            rand_num <= {rand_num[6:0], feedback};
    end

    // PART 2: Rising-Edge Pulse Detector
    reg last_btn;
    wire ask_for_new_piece = (next_btn == 1'b1) && (last_btn == 1'b0);

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            last_btn <= 1'b0;
        else
            last_btn <= next_btn;
    end

    // PART 3: 8-State Whitelist / Outlaw Markov State Machine
    // 3'd0: Straight Empty (Breather / Reset)
    // 3'd1: Left Sweeping Curve (Out & Back)
    // 3'd2: Right Sweeping Curve (Out & Back)
    // 3'd3: Dual-Lane Staggered Traffic
    // 3'd4: Center Island / Split-Lane Obstacle
    // 3'd5: Tight S-Bend (Left-to-Right)
    // 3'd6: Tight S-Bend (Right-to-Left)
    // 3'd7: High-Speed Slalom (3 Staggered 2x2 Obstacles)
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            current_seg <= 3'd0;
        end 
        else if (ask_for_new_piece) begin
            case (current_seg)
                3'd0: begin
                    // From Straight: Allow any segment (0..7)
                    current_seg <= rand_num[2:0];
                end

                3'd1: begin
                    // From Left Curve: Allow Straight (0), Left Curve (1), or Staggered Traffic (3)
                    // Outlaws immediate opposite S-bends (5,6) or dense Slalom (7)
                    case (rand_num[1:0])
                        2'b00: current_seg <= 3'd0;
                        2'b01: current_seg <= 3'd1;
                        2'b10: current_seg <= 3'd3;
                        2'b11: current_seg <= 3'd0;
                    endcase
                end

                3'd2: begin
                    // From Right Curve: Allow Straight (0), Right Curve (2), or Center Island (4)
                    case (rand_num[1:0])
                        2'b00: current_seg <= 3'd0;
                        2'b01: current_seg <= 3'd2;
                        2'b10: current_seg <= 3'd4;
                        2'b11: current_seg <= 3'd0;
                    endcase
                end

                3'd3, 3'd4: begin
                    // From Traffic/Split-Lane: Allow Straight (0) or Gentle Curve (1 or 2)
                    case (rand_num[1:0])
                        2'b00: current_seg <= 3'd0;
                        2'b01: current_seg <= 3'd1;
                        2'b10: current_seg <= 3'd2;
                        2'b11: current_seg <= 3'd0;
                    endcase
                end

                3'd5, 3'd6: begin
                    // From Tight S-Bends: Force Straight (0) or single gentle curve (1/2) to recover
                    current_seg <= rand_num[0] ? 3'd0 : (current_seg == 3'd5 ? 3'd2 : 3'd1);
                end

                3'd7: begin
                    // From High-Speed Slalom: Mandatory Straight (0) cooldown
                    current_seg <= 3'd0;
                end

                default: current_seg <= 3'd0;
            endcase
        end
    end

endmodule