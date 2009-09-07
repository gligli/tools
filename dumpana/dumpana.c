#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>

volatile void * ioremap(unsigned long long physaddr, unsigned long size)
{
    static int axs_mem_fd = -1;
    unsigned long long page_addr, ofs_addr, reg, pgmask;
    void* reg_mem = NULL;

    /*
     * looks like mmap wants aligned addresses?
     */
    pgmask = getpagesize()-1;
    page_addr = physaddr & ~pgmask;
    ofs_addr  = physaddr & pgmask;

    /*
     * Don't forget O_SYNC, esp. if address is in RAM region.
     * Note: if you do know you'll access in Read Only mode,
     *    pass O_RDONLY to open, and PROT_READ only to mmap
     */
    if (axs_mem_fd == -1) {
        axs_mem_fd = open("/dev/mem", O_RDWR|O_SYNC);
        if (axs_mem_fd < 0) {
                perror("AXS: can't open /dev/mem");
                return NULL;
        }
    }

    /* memory map */
    reg_mem = (void *) mmap64(
        0,
        size + (unsigned long) ofs_addr,
        PROT_READ|PROT_WRITE,
        MAP_SHARED,
        axs_mem_fd,
        page_addr
    );
    if (reg_mem == MAP_FAILED) {
        perror("AXS: mmap error");
        close(axs_mem_fd);
        return NULL;
    }

    reg = (unsigned long) reg_mem + (unsigned long) ofs_addr;
    return (volatile void *)reg;
}

int fd;

int _xenon_smc_ana_read(uint8_t addr, uint32_t *val)
{
  uint8_t buf[16];
  memset(buf, 0, 16);

  buf[0] = 0x11;
  buf[1] = 0x10;
  buf[2] = 5;
  buf[3] = 0x80 | 0x70;
  buf[5] = 0xF0;
  buf[6] = addr;

  write(fd, buf, 16);
  if (read(fd, buf, 16) != 16)
  {
    perror("read");
    exit(1);
  }
  //xenon_smc_send_message(buf);
  //xenon_smc_receive_response(buf);
  if (buf[1] != 0)
  {
    printf("xenon_smc_read_smbus failed, addr=%02x, err=%d\n", addr, buf[1]);
    return -1;
  }
  *val = buf[4] | (buf[5] << 8) | (buf[6] << 16) | (buf[7] << 24);
  return 0;
}

int main(void)
{
  volatile uint32_t *gpu = ioremap(0xec800000, 0x10000);
  printf("%dx%d\n", gpu[0x6134/4], gpu[0x6138/4]);

#if 0
  volatile uint32_t *smc = ioremap(0xea001000, 0x10000);
  smc[0x84/4] = 0;
  smc[0x84/4] = 0x04000000;

  smc[0x94/4] = 0x04000000;
  smc[0x94/4] = 0;
  
  
  printf("val: %08x %08x\n", smc[0x84/4], smc[0x94/4]);
  exit(1);
#endif

#if 1
  fd = open("/dev/smc", O_RDWR);
  if (fd <0)
  {
    perror("smc");
    return 1;
  }
  
  unsigned char dummy[16];
  fcntl(fd, F_SETFL, O_NONBLOCK); 
  while (read(fd, dummy, 16) == 16);
  fcntl(fd, F_SETFL, 0); 
  
  int i;
  for (i = 0; i < 0x100; ++i)
  {
    uint32_t v;
    _xenon_smc_ana_read(i, &v);
    printf("0x%08x, ",v);
    if ((i&0x7)==0x7)
      printf(" // %02x\n", i &~0x7);
  }
#endif

  
#if 1
  for (i = 0x0; i < 0x10000; i += 4)
  {
    uint32_t v =gpu[i/4];
    if (!v)
      continue;
    printf("%04x -> %08x\n",
      i, v);
  }
#endif
}
