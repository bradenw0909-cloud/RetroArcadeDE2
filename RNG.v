module rng (
    input  wire       clk,        // Connect to 50 MHz clock in Qsys
    input  wire       rst_n,      // Connect to reset_n in Qsys
    input  wire       next_btn,   // Connect to a 1-bit Output PIO from Nios II
    output reg  [1:0] current_seg // Connect to a 2-bit Input PIO to Nios II
);

    // =========================================================
    // PART 1: The Random Number Generator (LFSR)
    // This spins constantly in the background to make random bits.
    // =========================================================
    reg [7:0] rand_num;
    wire feedback = rand_num[7] ^ rand_num[5] ^ rand_num[4] ^ rand_num[3];

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            rand_num <= 8'b1010_1101; // Starting seed (cannot be all 0s)
        else
            rand_num <= {rand_num[6:0], feedback};
    end


    // =========================================================
    // PART 2: Detect when Nios II asks for a new track piece
    // =========================================================
    reg last_btn;
    wire ask_for_new_piece = (next_btn == 1'b1) && (last_btn == 1'b0);

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            last_btn <= 1'b0;
        else
            last_btn <= next_btn;
    end


    // =========================================================
    // PART 3: YOUR LOGIC (The Whitelist / Outlaw Rules)
    // Edit the 'case' block below to change which pieces are allowed!
    // =========================================================
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            current_seg <= 2'd0; // Always start the race on a Straight (0)
        end 
        else if (ask_for_new_piece) begin
            
            case (current_seg)
                // -------------------------------------------------
                // RULE 1: Currently on a STRAIGHT (0)
                // Whitelisted: Everything (0, 1, 2, or 3)
                // -------------------------------------------------
                2'd0: begin
                    current_seg <= rand_num[1:0]; // Picks 0, 1, 2, or 3 randomly
                end

                // -------------------------------------------------
                // RULE 2: Currently in a LEFT TURN (1)
                // Outlawed: Right Turn (2) and Obstacle (3)
                // Whitelisted: Straight (0) or another Left Turn (1)
                // -------------------------------------------------
                2'd1: begin
                    if (rand_num[0] == 1'b0)
                        current_seg <= 2'd0; // Go back to Straight
                    else
                        current_seg <= 2'd1; // Keep turning Left
                end

                // -------------------------------------------------
                // RULE 3: Currently in a RIGHT TURN (2)
                // Outlawed: Left Turn (1) and Obstacle (3)
                // Whitelisted: Straight (0) or another Right Turn (2)
                // -------------------------------------------------
                2'd2: begin
                    if (rand_num[0] == 1'b0)
                        current_seg <= 2'd0; // Go back to Straight
                    else
                        current_seg <= 2'd2; // Keep turning Right
                end

                // -------------------------------------------------
                // RULE 4: Currently on an OBSTACLE (3)
                // Outlawed: Everything except Straight (0)
                // -------------------------------------------------
                2'd3: begin
                    current_seg <= 2'd0; // Always force a Straight next
                end
            endcase

        end
    end

endmodule