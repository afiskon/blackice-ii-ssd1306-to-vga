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

    uint8_t regular_buff[128];
    uint8_t curr_code;
    uint64_t offset = 0;

    for(;;) {
        ssize_t rd = read(in, &curr_code, 1);
        if(rd != 1) // end of file, probably
            break;

        printf("off = %lu, rd = %ld, curr_code = 0x%02X\n", offset, rd, curr_code);
        offset += rd;

        if(curr_code & (1 << 7)) { // regular bytes
            uint8_t regular_cnt = (curr_code & 0x7F) + 1;
            rd = read(in, regular_buff, regular_cnt);
            offset += rd;
            if(rd != regular_cnt) {
                printf("ERROR: rd (%ld) != regular_cnt (%d)\n", rd, regular_cnt);
                close(in);
                close(out);
                exit(1);
            }

            printf("  writing %d regular bytes\n", regular_cnt);

            write(out, regular_buff, regular_cnt);
        } else { // zeros
            uint8_t zeros_cnt = (curr_code & 0x7F) + 1;
            memset(regular_buff, 0, zeros_cnt);
            printf("  writing %d zeros\n", zeros_cnt);
            write(out, regular_buff, zeros_cnt);
        }
    }

    close(in);
    close(out);
}
