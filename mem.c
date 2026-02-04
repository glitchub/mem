// This software is released as-is into the public domain, as described at
// https://unlicense.org. Do whatever you like with it.

// See https://github.com/glitchub/mem for more information.

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// die with a message
#define die(...) fprintf(stderr, __VA_ARGS__), exit(1)

static void usage(void)
{
    die(R"(Usage:

    mem [width] operation [... [width] operation]

Perform read and write operations on physical memory.

"operation" is in one of the following forms:

   address        - output the value at the address
   address=value  - write the value to the address
   address&=value - binary AND the value to the address
   address^=value - binary XOR the value to the address
   address|=value - binary OR the value to the address

Addresses and values can be up to 64-bit, given in decimal, hex, or octal.

"width" can be one of the following, and sets the data size of all subsequent operations
(until the next width character):

   b - 8 bit
   h - 16 bit
   w - 32 bit (this is the default)
   d - 64 bit

Values that exceed the current width are silently truncated.

Output values are printed to stdout in hex, zero-justified to the current width, one per line.

Exit status is zero on success or non-zero on any error.

E.G. set the LSB of a 32-bit register, write a value to another, print a byte from a third, clear
the first register's LSB again:

    $ sudo mem "0x12345678|=1" 0x1234567C=44 b 0x12345674 w 0x12345678^=1
    0xA7

Remember to quote | and & shell meta-characters!

)"
    );
}

#define MAXOPS 256
enum { READ, WRITE, AND, OR, XOR };

int main(int argc, char* argv[])
{
    // parse
    struct
    {
        int operator; // one of the enums above
        int width;
        uint64_t address;
        uint64_t data;
    } OP[MAXOPS] = {0};

    int width = 32, ops = 0;

    while (optind < argc)
    {
        char *arg = argv[optind++], *p;
        switch (*arg)
        {
            case 'b': width = 8; continue;
            case 'h': width = 16; continue;
            case 'w': width = 32; continue;
            case 'd': width = 64; continue;
        }

        if (ops == MAXOPS) die ("Too many operations\n");

        OP[ops].width = width;
        OP[ops].address = strtoull(arg, &p, 0);
        if (p == arg) goto choke;
        switch (*p++)
        {
            case 0:
                OP[ops].operator = READ;
                goto next;

            case '&':
                if (*p++ != '=') goto choke;
                OP[ops].operator = AND;
                break;

            case '^':
                if (*p++ != '=') goto choke;
                OP[ops].operator = XOR;
                break;

            case '|':
                if (*p++ != '=') goto choke;
                OP[ops].operator = OR;
                break;

            case '=':
                OP[ops].operator = WRITE;
                break;

            default:
            choke: die("'%s' is invalid\n", arg);
        }

        // get value
        if (!*p) goto choke;
        OP[ops].data = strtoull(p, &p, 0);
        if (*p) goto choke;

        next: ops++; // next
    }

    if (!ops) usage();

    // perform
    int fd = open("/dev/mem", O_RDWR|O_SYNC);
    if (fd < 0)  die("Can't open /dev/mem: %s\n", strerror(errno));

    for (int op = 0; op < ops; op++)
    {
        #define PAGESIZE 4096 // XXX get this from sysconf?
        int offset = OP[op].address % PAGESIZE;
        int size = ((offset + (OP[op].width/8)) > PAGESIZE) ? 2*PAGESIZE : PAGESIZE;
        void *map = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, (off_t)(OP[op].address - offset));
        if (map == MAP_FAILED) die("Can't map address 0x%" PRIX64" :%s\n", OP[op].address, strerror(errno));
        void *address = map + offset;

        switch(OP[op].operator)
        {
            case READ:
                switch(OP[op].width)
                {
                    case  8: printf("0x%.2"   PRIX8 "\n", *(volatile uint8_t *)  address); break;
                    case 16: printf("0x%.4"  PRIX16 "\n", *(volatile uint16_t *) address); break;
                    case 32: printf("0x%.8"  PRIX32 "\n", *(volatile uint32_t *) address); break;
                    case 64: printf("0x%.16" PRIX64 "\n", *(volatile uint64_t *) address); break;
                }
                break;

            case WRITE:
                switch(OP[op].width)
                {
                    case  8: *(uint8_t *)  address = OP[op].data; break;
                    case 16: *(uint16_t *) address = OP[op].data; break;
                    case 32: *(uint32_t *) address = OP[op].data; break;
                    case 64: *(uint64_t *) address = OP[op].data; break;
                }
                break;

            case AND:
                switch(OP[op].width)
                {
                    case  8: *(uint8_t *)  address &= OP[op].data; break;
                    case 16: *(uint16_t *) address &= OP[op].data; break;
                    case 32: *(uint32_t *) address &= OP[op].data; break;
                    case 64: *(uint64_t *) address &= OP[op].data; break;
                }
                break;

            case OR:
                switch(OP[op].width)
                {
                    case  8: *(uint8_t *)  address |= OP[op].data; break;
                    case 16: *(uint16_t *) address |= OP[op].data; break;
                    case 32: *(uint32_t *) address |= OP[op].data; break;
                    case 64: *(uint64_t *) address |= OP[op].data; break;
                }
                break;

            case XOR:
                switch(OP[op].width)
                {
                    case  8: *(uint8_t *)  address ^= OP[op].data; break;
                    case 16: *(uint16_t *) address ^= OP[op].data; break;
                    case 32: *(uint32_t *) address ^= OP[op].data; break;
                    case 64: *(uint64_t *) address ^= OP[op].data; break;
                }
                break;
        }

        munmap(map, size);
    }
    return 0;
}
