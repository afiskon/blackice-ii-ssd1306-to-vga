/* vim: set ai et ts=4 sw=4: */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>

/*
Encoding:
0xxxxxxx          - X+1   0x00 bytes
10yyyyyy yyyyyyyy - Y+129 0x00 bytes
11zzzzzz          - Z+1   regular bytes following this one
*/

#define MAX_ZEROS (((1 << (6+8)) - 1) + 129)

int main(int argc, char** argv) {
    if(argc < 3) {
        printf("Usage: %s <infile> <outfile>\n", argv[0]);
        exit(1);
    }

    int in = open(argv[1], O_RDONLY);
    if(in == -1) {
        printf("Failed to open %s: errno = %d\n", argv[1], errno);
        exit(1);
    }

    int out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0660);
    if(out == -1) {
        printf("Failed to open %s: errno = %d\n", argv[2], errno);
        close(in);
        exit(1);
    }

    uint8_t regular_buff[64];
    uint8_t regular_cnt = 0;
    uint16_t zeros_cnt = 0;
    uint8_t curr_byte;

    for(;;) {
        ssize_t rd = read(in, &curr_byte, 1);
        if(rd != 1) // end of file, probably
            break;

        bool encode_zero = (curr_byte == 0x00);
        if(encode_zero && (zeros_cnt == 0)) { // don't encode single 0x00 byte
            uint8_t next_byte;
            rd = read(in, &next_byte, 1);
            encode_zero = (rd == 1) && (next_byte == 0x00);
            if(rd == 1) { // end of file was not reached yet
                lseek(in, (off_t)(-1), SEEK_CUR);
            }
        }

        if(encode_zero) {
            if((zeros_cnt > 0) && (zeros_cnt < MAX_ZEROS)) { // encoding zeros
                zeros_cnt++;
            } else if(zeros_cnt == MAX_ZEROS) { // encoding zeros, but no more bits left
                zeros_cnt -= 129;
                uint8_t temp = ((uint8_t)(zeros_cnt >> 8)) | 0x80; // 10yyyyyy part
                write(out, &temp, 1);
                temp = zeros_cnt & 0xFF; // yyyyyyyy part
                write(out, &temp, 1); 
                zeros_cnt = 1;
            } else { // we were encoding regular bytes, this is the first zero
                if(regular_cnt) {
                    regular_cnt--;
                    regular_cnt |= 0xC0; // 11000000
                    write(out, &regular_cnt, 1);
                    regular_cnt &= 0x3F; // 00111111
                    write(out, regular_buff, regular_cnt + 1);
                    regular_cnt = 0;
                }
                zeros_cnt = 1;
            }
        } else {
            // were we encoding zeros?
            if(zeros_cnt > 0) {
                if(zeros_cnt <= 128) {
                    zeros_cnt--;
                    write(out, &zeros_cnt, 1); // 0xxxxxxx code
                    zeros_cnt = 0;
                } else {
                    zeros_cnt -= 129;
                    uint8_t temp = ((uint8_t)(zeros_cnt >> 8)) | 0x80; // 10yyyyyy part
                    write(out, &temp, 1);
                    temp = zeros_cnt & 0xFF; // yyyyyyyy part
                    write(out, &temp, 1); 
                    zeros_cnt = 0;
                }
            }

            // check if buffer is full
            if(regular_cnt == 64) {
                regular_cnt--;
                regular_cnt |= 0xC0; // 11000000
                write(out, &regular_cnt, 1);
                write(out, regular_buff, 64);
                regular_cnt = 0;
            }

            regular_buff[regular_cnt] = curr_byte;
            regular_cnt++;
        }
    }

    if(regular_cnt) {
        regular_cnt--;
        regular_cnt |= 0xC0; // 11000000
        write(out, &regular_cnt, 1);
        regular_cnt &= 0x3F; // 00111111
        write(out, regular_buff, regular_cnt + 1);
    } else if(zeros_cnt) {
        if(zeros_cnt <= 128) {
            zeros_cnt--;
            write(out, &zeros_cnt, 1); // 0xxxxxxx code
        } else {
            zeros_cnt -= 129;
            uint8_t temp = ((uint8_t)(zeros_cnt >> 8)) | 0x80; // 10yyyyyy part
            write(out, &temp, 1);
            temp = zeros_cnt & 0xFF; // yyyyyyyy part
            write(out, &temp, 1); 
        }
    }

    close(in);
    close(out);
}
