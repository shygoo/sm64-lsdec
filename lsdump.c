/*

SM64 Level Script Decoder
shygoo 2017
License: MIT

*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "lsdec.h"

void* loadfile(const char* filename, size_t* pSize)
{
	FILE* pFile;
	pFile = fopen(filename, "rb");
	
	if(pFile == NULL)
	{
		*pSize = 0;
		return NULL;
	}
	
	fseek(pFile, 0, SEEK_END);
	*pSize = ftell(pFile);
	rewind(pFile);
	void* buffer = malloc(*pSize);
	fread(buffer, *pSize, 1, pFile);
	fclose(pFile);
	return buffer;
}

int main(int argc, const char* argv[])
{
	const char* rom_path = argv[1];
	const char* dir_path = argv[2];
	
	if(argc < 3)
	{
		printf("SM64 Level Script Dumper\nUsage: lsdump <rom> <output directory>");
		return EXIT_FAILURE;
	}

	size_t rom_size;
	uint8_t* rom = loadfile(rom_path, &rom_size);
	
	if(rom == NULL)
	{
		printf("Error: Failed to load %s", rom_path);
		return EXIT_FAILURE;
	}
	
	lsd_ctx* lsd = lsd_create_ctx(rom);
	lsd_add_script(lsd, 0x108A10, 0x108A38);
	
	//lsd_config_set(lsd, LSD_CFG_VERBOSE);
	
	int levelno = 0;
	
	while(lsd_count_pending_scripts(lsd) > 0)
	{
		lsd_decode_next(lsd);
		uint32_t last_addr = lsd_get_offset_start(lsd);
		
		char file_path[MAX_PATH];
		sprintf(file_path, "%s/level_%02X_%08X.txt", dir_path, levelno, last_addr);
		
		lsd_flush_output(lsd, file_path);
		
		printf("Decoded %08X -> %s\n", last_addr, file_path);
		
		levelno++;
	}
	
	lsd_destroy_ctx(lsd);
	
	return EXIT_SUCCESS;
}