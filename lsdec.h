/*

SM64 Level Script Decoder
shygoo 2017
License: MIT

*/

#include <stdint.h>
#include <stdarg.h>

#define LSD_CFG_VERBOSE       0x01 // Prints scripts to stdout
#define LSD_CFG_TABULATE_ARGS 0x02 // Tabulates command parameters
#define LSD_CFG_INDENT_BLOCKS 0x04 // 

typedef struct lsd_ctx lsd_ctx;

lsd_ctx* lsd_create_ctx(uint8_t* rom);
void lsd_destroy_ctx(lsd_ctx* ctx);

void lsd_config_set(lsd_ctx* ctx, uint32_t cfg_mask);
void lsd_config_unset(lsd_ctx* ctx, uint32_t cfg_mask);

// Add script range to the queue
int  lsd_add_script(lsd_ctx* ctx, uint32_t offset_start, uint32_t offset_end);

// Count undecoded scripts in the queue
int  lsd_count_pending_scripts(lsd_ctx* ctx);

// Decode next undecoded script
void lsd_decode_next(lsd_ctx* ctx);

// Get pointer to output buffer
int lsd_printf(lsd_ctx* ctx, const char* fmt, ...);
const char* lsd_get_output(lsd_ctx* ctx);

// Flush ouput buffer to file
void lsd_flush_output(lsd_ctx* ctx, char* path);

// Offsets of last decoded script
uint32_t lsd_get_offset_start(lsd_ctx* ctx);
uint32_t lsd_get_offset_end(lsd_ctx* ctx);