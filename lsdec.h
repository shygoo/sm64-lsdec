#define LSD_CFG_VERBOSE 0x01

typedef struct lsd_ctx lsd_ctx;

lsd_ctx* lsd_create_ctx(u8* rom);
void lsd_destroy_ctx(lsd_ctx* ctx);

int  lsd_add_script(lsd_ctx* ctx, u32 start_offset, u32 end_offset);
int  lsd_count_pending_scripts(lsd_ctx* ctx);
void lsd_decode_next(lsd_ctx* ctx);
void lsd_config_set(lsd_ctx* ctx, u32 cfg_mask);
void lsd_config_unset(lsd_ctx* ctx, u32 cfg_mask);