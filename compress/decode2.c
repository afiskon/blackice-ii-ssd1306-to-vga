/* vim: set ai et ts=4 sw=4: */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

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

    int out = open(argv[2], O_WRONLY | O_CREAT, 0660);
    if(out == -1) {
        printf("Failed to open %s: errno = %d\n", argv[2], errno);
        close(in);
        exit(1);
    }

    uint8_t zero_buff[128] = { 0 };
    uint8_t regular_buff[64];
    uint8_t curr_code;

    for(;;) {
        ssize_t rd = read(in, &curr_code, 1);
        if(rd != 1) // end of file, probably
            break;

        if((curr_code & (1 << 7)) == 0) { // zeros
            uint8_t zeros_cnt = (curr_code & 0x7F) + 1;
            write(out, zero_buff, zeros_cnt);
        } else if((curr_code & 0xC0) == 0x80) { // even more zeros
            uint8_t extra_code;
            ssize_t rd = read(in, &extra_code, 1);
            if(rd != 1) {
                printf("ERROR: end of file while reading extra code\n");
                close(in);
                close(out);
                exit(1);
            }

            uint16_t zeros_cnt = (((uint16_t)(curr_code & 0x3F)) << 8) | (uint16_t)extra_code;
            zeros_cnt += 129;

            while(zeros_cnt > 0) {
                uint8_t nwrite = zeros_cnt > sizeof(zero_buff) ? sizeof(zero_buff) : zeros_cnt;
                write(out, zero_buff, nwrite);
                zeros_cnt -= nwrite;
            }
        } else { // (curr_code & 0xC0) == 0xC0, regular bytes
            uint8_t regular_cnt = (curr_code & 0x3F) + 1;
            rd = read(in, regular_buff, regular_cnt);
            if(rd != regular_cnt) {
                printf("ERROR: rd (%ld) != regular_cnt (%d)\n", rd, regular_cnt);
                close(in);
                close(out);
                exit(1);
            }
            write(out, regular_buff, regular_cnt);
        }
    }

    close(in);
    close(out);
}
