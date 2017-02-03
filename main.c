#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#include "lsdec.h"

void* loadfile(char* filename, u64* ret_size)
{
	FILE* pFile;
	pFile = fopen(filename, "rb");
	fseek(pFile, 0, SEEK_END);
	u64 fsize = ftell(pFile);
	rewind(pFile);
	void* buffer = malloc(fsize);
	fread(buffer, (size_t)fsize, 1, pFile);
	fclose(pFile);
	*ret_size = fsize;
	return buffer;
}

int main(int argc, char* argv[])
{
	char* rompath = argv[1];
	
	//index = parseInt(argv[2]);

	u64 romsize;
	
	u8* rom = loadfile(rompath, &romsize);
	
	lsd_ctx* ctx = lsd_create_ctx(rom);
	lsd_add_script(ctx, 0x108A10, 0x108A38);
	
	lsd_config_set(ctx, LSD_CFG_VERBOSE);
	
	while(lsd_count_pending_scripts(ctx))
	{
		lsd_decode_next(ctx);
	}
	
	//printf(ctx->output_buf);
	
}