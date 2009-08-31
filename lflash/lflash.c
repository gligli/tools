	/* placed in public domain, written by Felix Domke <tmbinc@elitedvb.net> */
	/* USE ON YOUR OWN RISK. */
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <byteswap.h>
#include <string.h>

extern void *mmap64 (void *__addr, size_t __len, int __prot, int __flags, int __fd, __off64_t __offset) __THROW;

volatile void * ioremap(unsigned long long physaddr, unsigned size, int sync)
{
	int axs_mem_fd = -1;
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
	 *	pass O_RDONLY to open, and PROT_READ only to mmap
	 */
	if (axs_mem_fd == -1) {
		axs_mem_fd = open("/dev/mem", O_RDWR|(sync ? O_SYNC : 0));
		if (axs_mem_fd < 0) {
				perror("AXS: can't open /dev/mem");
				return NULL;
		}
	}

	/* memory map */
	reg_mem = mmap64(
		(caddr_t)reg_mem,
		size+ofs_addr,
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

	reg = (unsigned long )reg_mem + ofs_addr;
	return (volatile void *)reg;
}

int iounmap(volatile void *start, size_t length)
{
	unsigned long ofs_addr;
	ofs_addr = (unsigned long)start & (getpagesize()-1);

	/* do some cleanup when you're done with it */
	return munmap((unsigned char*)start-ofs_addr, length+ofs_addr);
}

#define STATUS  1
#define COMMAND 2
#define ADDRESS 3
#define DATA    4
#define LOGICAL 5
#define PHYSICAL 6

volatile unsigned int *flash;

void sfcx_writereg(int reg, int value)
{
	flash[reg] = bswap_32(value);
}

unsigned int sfcx_readreg(int reg)
{
	return bswap_32(flash[reg]);
}

void readsector(unsigned char *data, int sector, int raw)
{
	int status;
	sfcx_writereg(STATUS, sfcx_readreg(STATUS));
	sfcx_writereg(ADDRESS, sector);	
	sfcx_writereg(COMMAND, raw ? 3 : 2);

	while ((status = sfcx_readreg(STATUS))&1);
 
	if (status != 0x200)
	{
		if (status & 0x40)
			printf(" * Bad block found at %08x\n", sector);
		else if (status & 0x1c)
			printf(" * (corrected) ECC error %08x: %08x\n", sector, status);
		else if (!raw)
			printf(" * illegal logical block %08x\n", sector);
		else
			printf(" * Unknown error at %08x: %08x. Please worry.\n", sector, status);
	}

	sfcx_writereg(ADDRESS, 0);

	int i;
	for (i = 0; i < 0x210; i+=4)
	{
		sfcx_writereg(COMMAND, 0);
		*(int*)(data + i) = bswap_32(sfcx_readreg(DATA));
	}
}

void flash_erase(int address)
{
	sfcx_writereg(0, sfcx_readreg(0) | 8);
	sfcx_writereg(STATUS, 0xFF);
	sfcx_writereg(ADDRESS, address);
	while (sfcx_readreg(STATUS) & 1);
	sfcx_writereg(COMMAND, 0xAA);
	sfcx_writereg(COMMAND, 0x55);
	while (sfcx_readreg(STATUS) & 1);
	sfcx_writereg(COMMAND, 0x5);
	while (sfcx_readreg(STATUS) & 1);
	int status = sfcx_readreg(STATUS);
	if (status != 0x200)
		printf("[%08x]", status);
	sfcx_writereg(STATUS, 0xFF);
	sfcx_writereg(0, sfcx_readreg(0) & ~8);
}

void write_page(int address, unsigned char *data)
{
	sfcx_writereg(STATUS, 0xFF);
	sfcx_writereg(0, sfcx_readreg(0) | 8);

	sfcx_writereg(ADDRESS, 0);

	int i;

	for (i = 0; i < 0x210; i+=4)
	{
		sfcx_writereg(DATA, bswap_32(*(int*)(data + i)));
		sfcx_writereg(COMMAND, 1);
	}

	sfcx_writereg(ADDRESS, address);
	sfcx_writereg(COMMAND, 0x55);
	while (sfcx_readreg(STATUS) & 1);
	sfcx_writereg(COMMAND, 0xAA);
	while (sfcx_readreg(STATUS) & 1);
	sfcx_writereg(COMMAND, 0x4);
	while (sfcx_readreg(STATUS) & 1);
	int status = sfcx_readreg(STATUS);
	if (status != 0x200)
		printf("[%08x]", status);
	sfcx_writereg(0, sfcx_readreg(0) & ~8);
}



extern volatile void * ioremap(unsigned long long physaddr, unsigned size, int sync);
extern int iounmap(volatile void *start, size_t length);

int dump_flash_to_file(const char *filename)
{
	printf(" * Dumping to %s...\n", filename);
	
	FILE *f = fopen(filename, "wb");
	
	int i;
	for (i = 0; i < 16*1024*1024; i += 0x200)
	{
		unsigned char sector[0x210];
		readsector(sector, i, 1);
		if (!(i&0x3fff))
		{
			printf("%08x\r", i);
			fflush(stdout);
		}
		if (fwrite(sector, 1, 0x210, f) != 0x210)
			return -1;
	}
	printf("done!   \n");
	fclose(f);
	return 0;
}

int verify_flash_with_file(const char *filename, int raw)
{
	FILE *f = fopen(filename, "rb");
	if (!f)
		return -1;

	if (raw == -1) /* auto */
	{
		fseek(f, 0, SEEK_END);
	
		if (ftell(f) == 16*1024*1024 / 0x200 * 0x210)
		{
			raw = 1;
			printf(" * detected RAW nand file, verifying in raw mode.\n");
		} else
		{
			raw = 0;
			printf(" * detected short nand file, verifying in cooked mode.\n");
		}
		fseek(f, 0, SEEK_SET);
	}
	
	printf(" * Verifying flash with %s...\n", filename);
	
	int i;
	for (i = 0; i < 16*1024*1024; i += 0x200)
	{
		unsigned char sector[0x210], sector_flash[0x210];
		if (!(i&0x3fff))
		{
			printf("%08x\r", i);
			fflush(stdout);
		}
		if (fread(sector, 1, 0x210, f) != 0x210)
			return i;
		readsector(sector_flash, i, raw);
		if (sector_flash[0x205] != 0xFF) /* bad sector */
		{
			printf(" * ignoring bad sector at %08x\n", i);
			continue;
		}
		if (memcmp(sector, sector_flash, 0x210))
		{
			printf(" * VERIFY error at %08x\n", i);
			return -2;
		}
	}
	printf("done!   \n");
	fclose(f);
	return i;
}

int flash_from_file(const char *filename, int raw)
{
	printf(" * Flashing from %s...\n", filename);

	FILE *f = fopen(filename, "rb");
	if (!f)
		return -1;

	if (raw == -1) /* auto */
	{
		fseek(f, 0, SEEK_END);
	
		if (ftell(f) == 16*1024*1024 / 0x200 * 0x210)
		{
			raw = 1;
			printf(" * detected RAW nand file, flashing in raw mode.\n");
		} else
		{
			raw = 0;
			printf(" * detected short nand file, flashing in cooked mode.\n");
		}
		fseek(f, 0, SEEK_SET);
	}
	
	int i;
	for (i = 0; i < 16*1024*1024; i += 0x4000)
	{
		unsigned char sector[0x210*32], sector_flash[0x210*32];
		memset(sector, 0xFF, sizeof(sector));
		if (!fread(sector, 1, 0x210*32, f))
			return i;

		printf("%08x\r", i);
		fflush(stdout);
		
		readsector(sector_flash, i, 0);
		
		int phys_pos;
		
		if (!raw)
		{
			phys_pos = sfcx_readreg(PHYSICAL);
		
			if (!(phys_pos & 0x04000000)) /* shouldn't happen, unless the existing image is broken. just assume the sector is okay. */
			{
				printf(" * Uh, oh, don't know. Reading at %08x failed.\n", i);
				phys_pos = i;
			}
			phys_pos &= 0x3fffe00;
		
			if (phys_pos != i)
				printf(" * relocating sector %08x to %08x...\n", i, phys_pos);
		} else
			phys_pos = i;

		flash_erase(phys_pos);
		int j;
		for (j = 0; j < 32; ++j)
			write_page(phys_pos + j * 0x200, sector + j * 0x210);
	}
	return 0;
}

int main(int argc, char **argv)
{
	flash = ioremap(0xea00c000, 0x1000, 1);
	
	printf(" * flash config: %08x\n", sfcx_readreg(0));
	
	sfcx_writereg(0, sfcx_readreg(0) &~ (4|8|0x3c0));
	
	if (sfcx_readreg(0) != 0x01198010)
	{
		printf(" * unknown flash config %08x\n", sfcx_readreg(0));
		return 1;
	}
	
	if (argc != 2 && argc != 3)
	{
		printf("usage: %s <current> [<new>]\n", *argv);
		return 2;
	}

	const char *orig = argv[1];
	int res = verify_flash_with_file(orig, 1);
	if (res == -1)
	{
		dump_flash_to_file(orig);
		res = verify_flash_with_file(orig, 1);
	}

	if (res != 16*1024*1024)
	{
		if (res == -2)
			printf(" * verify failed!\n");
		else if (res > 0)
			printf(" * verified correctly, but only %d bytes.\n", res);
		else
			printf(" * original image invalid\n");
		printf(" * I won't flash if you don't have a full, working backup, sorry.\n");
		return 1;
	}
	printf(" * verify ok.\n");
	
	if (argc > 2)
	{
		const char *image = argv[2];
		
		flash_from_file(image, -1);
		res = verify_flash_with_file(image, -1);
		if (res > 0)
			printf(" * verified %d bytes ok\n", res);
		else
			printf(" * verify failed! (%d)\n", res);
	}
	return 0;
}
