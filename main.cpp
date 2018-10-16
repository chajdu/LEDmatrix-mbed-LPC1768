#include "mbed.h"
#include "dot_matrix_font_5x7.h"

#define SCORE_MAX 99
#define DEBOUNCE_TIMEOUT_MS 75
#define SCORE_DEC_TIMEOUT_MS 1000
#define RESET_TIMEOUT_MS 3000


char player_names[2][12] = { "Andr\xa0s", "Gy\x94rgy" };
char player_names_t[2][12];
char LED_R_map[32][64];
char LED_G_map[32][64];
char LED_B_map[32][64];

DigitalOut LED_CLK(p19);
DigitalOut LED_OEn(p20);
DigitalOut LED_STB(p22);
DigitalOut LED_R0(p13);
DigitalOut LED_R1(p15);
DigitalOut LED_G0(p28);
DigitalOut LED_G1(p26);
DigitalOut LED_B0(p14);
DigitalOut LED_B1(p16);
DigitalOut LED_A(p17);
DigitalOut LED_B(p24);
DigitalOut LED_C(p18);
DigitalOut LED_D(p23);

DigitalIn BTN1(p5);
DigitalIn BTN2(p6);
Timer BTN_timer[2];
int BTN_press_time_ms[2] = {0, 0}, BTN_depress_time_ms[2] = {0, 0};
char BTN_prev[2] = {0, 0}, BTN_curr[2] = {0, 0}, BTN_new_press[2] = {0, 0};

unsigned char score[2] = {0, 0};    // If the score underflows, it will automatically reset everything


// Transcode string: all Hungarian diacritic characters are transcoded from
//                   CP-852 to their equivalents in dot_matrix_font_5x7.h
//                   (Character numbers 128-145)
void transcode_str(char * str_in, char * str_out) {
    //                            Á    É    Í    Ó    Ö    Ő    Ú    Ü    Ű
    char diacritic_chars_HU[] = {181, 144, 214, 224, 153, 138, 233, 154, 235,   // Upper case
                                 160, 130, 161, 162, 148, 139, 163, 129, 251};  // Lower case
    
    // Go through the input string character by character
    for (int i = 0; i < strlen(str_in); i++) {
        // If a 0 remains, it means the character in question hasn't been transcoded
        str_out[i] = 0;
        
        // Compare to each diacritic character, transcode if it matches
        for (int j = 0; j < strlen(diacritic_chars_HU); j++)
            if (str_in[i] == diacritic_chars_HU[j]) {
                str_out[i] = 128 + j;
                break;
            }
               
        // Take care of non-transcoded chars
        if (str_out[i] == 0)
            // The characters that are not part of the font are converted to spaces
            if (str_in[i] < 32 || str_in[i] > 145)
                str_out[i] = 32;
            // Everything else is left as it was
            else
                str_out[i] = str_in[i];
    }
}


// Load string into LED display buffer
void LED_load_str(char * str_in, char map[32][64], char text_row) {
    // Start at column 2
    int map_Col = 2;

    // Load string    
    for (int Char = 0; Char < strlen(str_in); Char++)
        for (int Col = 0; Col < 6; Col++) {
            for (int Row = 0; Row < 7; Row++)
                if (Col < 5)
                    map[Row + text_row * 8][map_Col] = (Font5x7[str_in[Char] - 32][Col] >> Row) & 0x01;
                else    // Insert space between letters
                    map[Row + text_row * 8][map_Col] = 0;

            // Clear LED in the 8th row
            map[text_row * 8 + 7][map_Col] = 0;
                    
            map_Col++;
        }
    
    // Clear remaining dots
    for (int Char = strlen(str_in); Char < 10; Char++)
        for (int Col = 0; Col < 6; Col++) {
            for (int Row = 0; Row <= 7; Row++)
                map[Row + text_row * 8][map_Col] = 0;
                    
            map_Col++;
        }
}


// Load string into LED display buffer
// Overloaded version: the string is loaded at an arbitrary position specified
//                     as [row, col]
void LED_load_str(char * str_in, char map[32][64], char LED_row, char LED_col) {
    // Load string    
    for (int Char = 0; Char < strlen(str_in); Char++)
        for (int Col = 0; Col < 6; Col++) {
            for (int Row = 0; Row < 7; Row++)
                if (Col < 5)
                    map[Row + LED_row][LED_col] = (Font5x7[str_in[Char] - 32][Col] >> Row) & 0x01;
                else    // Insert space between letters
                    map[Row + LED_row][LED_col] = 0;

            // Clear LED in the 8th row
            map[LED_row + 7][LED_col] = 0;
                    
            LED_col++;
        }
}


// Clear LED display buffer
// The limits for clearing can also be specified as parameters with default values
void LED_clear_buf(char map[32][64], char row_min = 0, char row_max = 31, char col_min = 0, char col_max = 63) {
    // Clear dots
    for (int Row = row_min; Row <= row_max; Row++)
        for (int Col = col_min; Col <= col_max; Col++)
            map[Row][Col] = 0;                    
}


int main() {
    // Initialize buttons
    BTN1.mode(PullDown);
    BTN2.mode(PullDown);

    // Initialize timers to be sure
    BTN_timer[0].stop();
    BTN_timer[0].reset();
    BTN_timer[1].stop();
    BTN_timer[1].reset();
    
    // Initialize all signals
    LED_CLK = 0;
    LED_OEn = 1;
    LED_STB = 0;
    
    LED_R0 = 0;
    LED_R1 = 0;
    LED_G0 = 0;
    LED_G1 = 0;
    LED_B0 = 0;
    LED_B1 = 0;
    
    // Clear all LED buffers to be sure
    LED_clear_buf(LED_R_map);
    LED_clear_buf(LED_G_map);
    LED_clear_buf(LED_B_map);
    
    
    // Prepare player names for display
    for (int i = 0; i < 2; i++)
        transcode_str(player_names[i], player_names_t[i]);

    LED_load_str(player_names_t[0], LED_R_map, 6, 5);
    LED_load_str(player_names_t[1], LED_G_map, 19, 5);
      
    // Handle key presses, count score, update display buffers, send buffers to display
    while(1) {
// ------------------------------
//  ACQUIRE BUTTON PRESSES
// ------------------------------
        // Poll buttons
        BTN_curr[0] = BTN1;
        BTN_curr[1] = BTN2;
        
        // Detect rising edge on BTN1 (press)
        if (!BTN_prev[0] && BTN_curr[0]) {
            // Set new press flag for decrement timeout measurement
            BTN_new_press[0] = 1;
            
            // Read out time measurement
            BTN_depress_time_ms[0] = BTN_timer[0].read_ms();

            // Restart time measurement
            BTN_timer[0].reset();
            BTN_timer[0].start();
        }
        // Detect falling edge on BTN1 (depress)
        else if (BTN_prev[0] && !BTN_curr[0]) {
            // Clear new press flag for decrement timeout measurement
            BTN_new_press[0] = 0;
            
            // Read out time measurement
            BTN_press_time_ms[0] = BTN_timer[0].read_ms();

            // Restart time measurement
            BTN_timer[0].reset();
            BTN_timer[0].start();
        }              

        // Detect rising edge on BTN2 (press)
        if (!BTN_prev[1] && BTN_curr[1]) {
            // Set new press flag for decrement timeout measurement
            BTN_new_press[1] = 1;
            
            // Read out time measurement
            BTN_depress_time_ms[1] = BTN_timer[1].read_ms();

            // Restart time measurement
            BTN_timer[1].reset();
            BTN_timer[1].start();
        }
        // Detect falling edge on BTN2 (depress)
        else if (BTN_prev[1] && !BTN_curr[1]) {
            // Clear new press flag for decrement timeout measurement
            BTN_new_press[1] = 0;
            
            // Read out time measurement
            BTN_press_time_ms[1] = BTN_timer[1].read_ms();

            // Restart time measurement
            BTN_timer[1].reset();
            BTN_timer[1].start();
        }
        
        // Save previous button values
        BTN_prev[0] = BTN_curr[0];
        BTN_prev[1] = BTN_curr[1];


// ------------------------------
//  HANDLE BUTTON PRESSES
// ------------------------------
        // If either button has been held down for more than the timeout, reset everything
        if ( (BTN_curr[0] && BTN_timer[0].read_ms() > RESET_TIMEOUT_MS) || (BTN_curr[1] && BTN_timer[1].read_ms() > RESET_TIMEOUT_MS) ) {
            // Stop and reset timers
            BTN_timer[0].stop();
            BTN_timer[0].reset();
            BTN_timer[1].stop();
            BTN_timer[1].reset();
            
            // Reset scores
            score[0] = 0;
            score[1] = 0;
        }
        // If one of the buttons has been held down for more than the timeout, decrement the corresponding score
        else if ( (BTN_curr[0] && BTN_timer[0].read_ms() > SCORE_DEC_TIMEOUT_MS) || (BTN_curr[1] && BTN_timer[1].read_ms() > SCORE_DEC_TIMEOUT_MS) ) {
            if (BTN_timer[0].read_ms() > SCORE_DEC_TIMEOUT_MS && BTN_new_press[0]) {
                // Clear new press flag so this event won't be handled again
                BTN_new_press[0] = 0;
                
                // Decrement score
                score[0]--;
            }

            if (BTN_timer[1].read_ms() > SCORE_DEC_TIMEOUT_MS && BTN_new_press[1]) {
                // Clear new press flag so this event won't be handled again
                BTN_new_press[1] = 0;
                
                // Decrement score
                score[1]--;
            }
        }
        // Increment scores as applicable
        // - Debounce buttons
        // - Don't increment if the decrement timeout has been exceeded
        else {
            if (DEBOUNCE_TIMEOUT_MS < BTN_press_time_ms[0] && BTN_press_time_ms[0] < SCORE_DEC_TIMEOUT_MS)
                score[0]++;
            else if (DEBOUNCE_TIMEOUT_MS < BTN_press_time_ms[1] && BTN_press_time_ms[1] < SCORE_DEC_TIMEOUT_MS)
                score[1]++;
        }

        // Reset timer values to avoid detecting an event twice
        BTN_press_time_ms[0] = 0;
        BTN_press_time_ms[1] = 0;
        
        // If either score exceeds the max, reset everything
        if ( (score[0] > SCORE_MAX) || (score[1] > SCORE_MAX) ) {
            // Stop and reset timers
            BTN_timer[0].stop();
            BTN_timer[0].reset();
            BTN_timer[1].stop();
            BTN_timer[1].reset();
            
            // Reset scores
            score[0] = 0;
            score[1] = 0;
        }
        
        // Load scores into display buffer
        char tmp_buf[3];
        
        sprintf(tmp_buf, "%2d", score[0]);
        LED_load_str(tmp_buf, LED_R_map, 6, 48);
        LED_load_str(tmp_buf, LED_G_map, 6, 48);
        LED_load_str(tmp_buf, LED_B_map, 6, 48);
        sprintf(tmp_buf, "%2d", score[1]);
        LED_load_str(tmp_buf, LED_R_map, 19, 48);
        LED_load_str(tmp_buf, LED_G_map, 19, 48);
        LED_load_str(tmp_buf, LED_B_map, 19, 48);


// ------------------------------
//  UPDATE DISPLAY
// ------------------------------        
        for (int Row = 0; Row < 16; Row++) {
            // -----------------
            //  RED PIXELS
            // -----------------
            // Send out 64 bits
            for (int Col = 0; Col < 64; Col++) {
                LED_CLK = 0;

                LED_R0 = LED_R_map[Row][Col];
                LED_R1 = LED_R_map[Row + 16][Col];
                LED_G0 = 0;
                LED_G1 = 0;
                LED_B0 = 0;
                LED_B1 = 0;

                LED_CLK = 1;
            }
            
            // Disable LED driver output
            LED_OEn = 1;
            
            // Strobe transmission to LED driver output
            LED_STB = 1;
            LED_STB = 0;
            
            // Select channel to operate on
            LED_A = (Row & 0x01);
            LED_B = (Row & 0x02) >> 1;
            LED_C = (Row & 0x04) >> 2;
            LED_D = (Row & 0x08) >> 3;
            
            // Enable LED driver output
            LED_OEn = 0;

            // -----------------
            //  GREEN PIXELS
            // -----------------
            // Send out 64 bits
            for (int Col = 0; Col < 64; Col++) {
                LED_CLK = 0;

                LED_R0 = 0;
                LED_R1 = 0;
                LED_G0 = LED_G_map[Row][Col];
                LED_G1 = LED_G_map[Row + 16][Col];
                LED_B0 = 0;
                LED_B1 = 0;

                LED_CLK = 1;
            }
            
            // Disable LED driver output
            LED_OEn = 1;
            
            // Strobe transmission to LED driver output
            LED_STB = 1;
            LED_STB = 0;
            
            // Select channel to operate on
            LED_A = (Row & 0x01);
            LED_B = (Row & 0x02) >> 1;
            LED_C = (Row & 0x04) >> 2;
            LED_D = (Row & 0x08) >> 3;
            
            // Enable LED driver output
            LED_OEn = 0;

            // -----------------
            //  BLUE PIXELS
            // -----------------
            // Send out 64 bits
            for (int Col = 0; Col < 64; Col++) {
                LED_CLK = 0;

                LED_R0 = 0;
                LED_R1 = 0;
                LED_G0 = 0;
                LED_G1 = 0;
                LED_B0 = LED_B_map[Row][Col];
                LED_B1 = LED_B_map[Row + 16][Col];

                LED_CLK = 1;
            }
            
            // Disable LED driver output
            LED_OEn = 1;
            
            // Strobe transmission to LED driver output
            LED_STB = 1;
            LED_STB = 0;
            
            // Select channel to operate on
            LED_A = (Row & 0x01);
            LED_B = (Row & 0x02) >> 1;
            LED_C = (Row & 0x04) >> 2;
            LED_D = (Row & 0x08) >> 3;
            
            // Enable LED driver output
            LED_OEn = 0;
        }
        
        // -----------------------
        //  BRIGHTNESS CORRECTION
        // -----------------------
        // Drive the blue pixels of the last row once more. These won't actually
        // get transferred to the output, but by disabling the LED drivers once
        // this is done, we can ensure that the 16th and 32nd blue rows get
        // driven for roughly the same amount of time as every other row, thus
        // these rows won't be a little bit brighter than the others
        for (int Col = 0; Col < 64; Col++) {
            LED_CLK = 0;

            LED_R0 = 0;
            LED_R1 = 0;
            LED_G0 = 0;
            LED_G1 = 0;
            LED_B0 = LED_B_map[15][Col];
            LED_B1 = LED_B_map[31][Col];

            LED_CLK = 1;
        }
        
        // Disable LED driver output
        LED_OEn = 1;
    }
}
