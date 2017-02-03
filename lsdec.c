#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "util.h"
#include "lsdec.h"

#define LSD_MAX_SCRIPTS 50
#define LSD_INDENT_STR "  "

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

typedef struct {
	u32 start; // Start rom offset
	u32 end; // End rom offset
	int b_decoded;
} lsd_script;

typedef struct lsd_ctx {
	u32 config;
	u8* rom;
	u32 offset; // Pointer to lvs start
	char* output_buf;
	u32 output_buf_idx;
	u32 output_buf_size;
	int output_indent;
	lsd_script scripts[LSD_MAX_SCRIPTS]; // list of addresses collected from 00 and 01 commands
	int scripts_idx;
} lsd_ctx;

typedef struct {
	u32   value;
	char* name;
} lsd_label;

enum lsd_argfmt {
	U8h, U16h, U32h,
	U8d, U16d, U32d,
	S8d, S16d, S32d
};

typedef struct {
	u32 rel_offset; // offset relative to command
	enum lsd_argfmt format;
	lsd_label* label_set;
} lsd_arg;

enum lsd_indent {
	LSD_TAB_NA  = 0,
	LSD_TAB_OPEN = 1,
	LSD_TAB_CLOSE = 2
};

typedef struct {
	u8 cmd_byte;
	const char* name;
	lsd_arg* args;
	enum lsd_indent indent;
} lsd_command;

static void lsd_decode_range(lsd_ctx* ctx, u32 offset_start, u32 offset_end);

static void lsd_dec_unhandled    (lsd_ctx* ctx);

// Printing

static int lsd_printf              (lsd_ctx* ctx, const char* fmt, ...);
static void lsd_print_indent        (lsd_ctx* ctx);
static void lsd_adjust_indent       (lsd_ctx* ctx, int tabs);

// Lookups

static lsd_command* command_lookup (u8 cmdbyte);
static lsd_label*   label_lookup   (lsd_label* labelset, u32 value);

// Fetch data from ctx offset + rel_offset

static u8  lsd_u8  (lsd_ctx* ctx, u32 rel_offset);
static u16 lsd_u16 (lsd_ctx* ctx, u32 rel_offset);
static u32 lsd_u32 (lsd_ctx* ctx, u32 rel_offset);

////// Lookup tables

lsd_label lbl_segments[] = {
	{ 0x00, "SEG_00_MEM" },    // general purpose
	{ 0x01, "SEG_01_GFX" },     // master display list?
	{ 0x02, "SEG_02_HUD_GFX" }, // display lists, textures (hud stuff namely)
	{ 0x03, "SEG_03_GFX" },     // display lists
	{ 0x04, "SEG_04_MARIO" },   // mario geo layout, dlists, and textures
	{ 0x05, "SEG_05_GFX" },     // geo layouts
	{ 0x06, "SEG_06_GFX" },     // geo layouts
	{ 0x07, "SEG_07_SPECIAL" }, // collisions, special objects, macro objects, and area rendering instructions if applicable
	{ 0x08, "SEG_08_GFX" },     // display lists (exlusively?)
	{ 0x09, "SEG_09_LVL_IMG" }, // level geometry textures (1A command)
	{ 0x0A, "SEG_0A_GFX" },     // ??, demo screen level geometry textures during (1A command)
	{ 0x0B, "SEG_0B_ENV_GFX" }, // environment effect graphics
	{ 0x0C, "SEG_0C_GFX" },     // geo layouts
	{ 0x0D, "SEG_0D_GFX" },     // geo layouts
	{ 0x0E, "SEG_0E_LVL" },     // standard level scripts and some geo layouts
	{ 0x0F, "SEG_0F_GFX" },     // geo layouts
	{ 0x10, "SEG_10_LVL" },     // initial level script
	{ 0x11, "SEG_11_UNK" },     // ?????? 0x4000 bytes allocated on startup
	{ 0x12, "SEG_12_UNK" },     // ??????
	{ 0x13, "SEG_13_BHV" },     // behavior scripts
	{ 0x14, "SEG_14_LVL" },     // intro screens
	{ 0x15, "SEG_15_LVL" },     // intro screens
	{ 0x16, "SEG_16_GFX" },     // geo layouts
	{ 0x17, "SEG_17_GFX" },     // geo layouts
	{ 0x18, "SEG_18_UNK" },     // ?????? 0x800 bytes allocated on startup
	{ 0 }
};

lsd_label lbl_heads[] = {
	{ 0x01, "DEMO_HEAD_NONE" },
	{ 0x02, "DEMO_HEAD_STANDARD" },
	{ 0x03, "DEMO_HEAD_GAME_OVER" },
	{ 0 }
};

lsd_label lbl_terrains[] = {
	{ 0x00, "TERRAIN_STD_A" },
	{ 0x01, "TERRAIN_STD_B" },
	{ 0x02, "TERRAIN_SNOW" },
	{ 0x03, "TERRAIN_SAND" },
	{ 0x04, "TERRAIN_HAUNT" },
	{ 0x05, "TERRAIN_WATER" },
	{ 0x06, "TERRAIN_SLIDE" },
	{ 0 }
};

lsd_label lbl_operations[] = {
	{ 0, "AC_AND" },
	{ 1, "AC_NAND" },
	{ 2, "AC_EQ" },
	{ 3, "AC_NEQ" },
	{ 4, "AC_LT" },
	{ 5, "AC_LTEQ" },
	{ 6, "AC_GT" },
	{ 7, "AC_GTEQ" },
	{ 0 }
};

lsd_label lbl_songs[] = {
	{ 0x00, "SONG_00_NONE" },
	{ 0x01, "SONG_01_STAR" },
	{ 0x02, "SONG_02_DEMO" },
	{ 0x03, "SONG_03_BATTLEFIELD" },
	{ 0x04, "SONG_04_CASTLE" },
	{ 0x05, "SONG_05_WATER" },
	{ 0x06, "SONG_06_FIRE" },
	{ 0x07, "SONG_07_BOWSER" },
	{ 0x08, "SONG_08_SNOW" },
	{ 0x09, "SONG_09_RACE" },
	{ 0x0A, "SONG_0A_HAUNT" },
	{ 0x0B, "SONG_0B_LULLABY" },
	{ 0x0C, "SONG_0C_CAVE" },
	{ 0x0D, "SONG_0D_SELECT_STAR" },
	{ 0x0E, "SONG_0E_WING_CAP" },
	{ 0x0F, "SONG_0F_METAL_CAP" },
	{ 0x10, "SONG_10_WARNING" },
	{ 0x11, "SONG_11_BOWSER_COURSE" },
	{ 0x12, "SONG_12_RECORD" },
	{ 0x13, "SONG_13_MERRY_GO_ROUND" },
	{ 0x14, "SONG_14_RACE_START" },
	{ 0x15, "SONG_15_STAR_UNLOCK" },
	{ 0x16, "SONG_16_MINI_BOSS" },
	{ 0x17, "SONG_17_KEY" },
	{ 0x18, "SONG_18_STAIRS" },
	{ 0x19, "SONG_19_BOWSER_FINAL" },
	{ 0x1A, "SONG_1A_CREDITS" },
	{ 0x1B, "SONG_1B_SOLVE" },
	{ 0x1C, "SONG_1C_TOAD" },
	{ 0x1D, "SONG_1D_PEACH" },
	{ 0x1E, "SONG_1E_INTRO1" },
	{ 0x1F, "SONG_1F_END1" },
	{ 0x20, "SONG_20_END2" },
	{ 0x21, "SONG_21_SELECT_FILE" },
	{ 0x22, "SONG_22_INTRO2" },
	{ 0 }
};

lsd_label lbl_colors[] = {
	{ 0x00000000, "COLOR_BLACK" },
	{ 0xFFFFFF00, "COLOR_WHITE" },
	{ 0 }
};

static lsd_arg args_run_script[] = {
	{ 3, U8h, lbl_segments }, // segno
	{ 4, U32h }, // rom start
	{ 8, U32h }, // rom end
	{ 0 }
};

static lsd_arg args_wait[] = {
	{ 2, U16d }, // frame count
	{ 0 }
};

static lsd_arg args_imm32[] = {
	{ 4, U32h }, // misc 32 bit
	{ 0 }
};

static lsd_arg args_imm16[] =  {
	{ 2, U16h }, // misc 16 bit
	{ 0 }
};

static lsd_arg args_op[] = {
	{ 2, U8h, lbl_operations }, // operation
	{ 4, U32h }, // value
	{ 0 }
};

static lsd_arg args_op_jump[] = {
	{ 2, U8h, lbl_operations }, // operation
	{ 4, U32h }, // value
	{ 8, U32h }, // segptr jump address
	{ 0 }
};

static lsd_arg args_call[] = {
	{ 2, U16h }, // parameter
	{ 4, U32h }, // asm routine
	{ 0 }
};

static lsd_arg args_load_raw[] = {
	{ 4, U32h }, // ram ptr dest
	{ 8, U32h }, // rom ptr start
	{ 12, U32h }, // rom ptr end
	{ 0 }
};

static lsd_arg args_load_seg[] = {
	{ 3, U8h, lbl_segments }, // segment number
	{ 4, U32h }, // rom ptr start
	{ 8, U32h }, // rom ptr end
	{ 0 }
};

static lsd_arg args_demo_head[] = {
	{ 2, U8d, lbl_heads },
	{ 0 }
};

static lsd_arg args_def[] = { // area and gfx id
	{ 2, U8h }, //id
	{ 4, U32h }, // segptr
	{ 0 }
};

static lsd_arg args_set_obj[] = {
	{ 2, U8h }, // act flags
	{ 3, U8h }, // gfx id
	{ 4, S16d }, // x
	{ 6, S16d }, // y
	{ 8, S16d }, // z
	{ 10, S16d }, // roll
	{ 12, S16d }, // pitch
	{ 14, S16d }, // yaw
	{ 16, U16h }, // bparam a
	{ 18, U16h }, // bparam b
	{ 20, U32h }, // segptr behavior
	{ 0 }
};

static lsd_arg args_warp[] = {
	{ 2, U8h }, // from
	{ 3, U8h }, // course to
	{ 4, U8h }, // area to
	{ 5, U8h }, // id to
	{ 0 }
};

static lsd_arg args_terrain[] = {
	{ 3, U8h, lbl_terrains },
	{ 0 }
};

static lsd_arg args_transition[] = {
	{ 2, U8h }, // on/off
	{ 3, U8h }, // frame count
	{ 4, U32h, lbl_colors }, // color
	{ 0 }
};

static lsd_arg args_music_a[] = { // UNFINISHED
	{ 5, U8h, lbl_songs },
	{ 0 }
};

static lsd_arg args_music_b[] = {
	{ 2, U8h, lbl_songs },
	{ 0 }
};

static lsd_command cmd_list[] = {
//   cmd, name, args, indentation
	{ 0x00, "run_script_a",      args_run_script },
	{ 0x01, "run_script_b",      args_run_script },
	{ 0x02, "end_script" },
	{ 0x03, "wait",              args_wait },
	{ 0x04, "wait_end_sig",      args_wait },
	{ 0x05, "jump",              args_imm32 },
	{ 0x06, "jal",               args_imm32 },
	{ 0x07, "return" },
	{ 0x08, "push",              args_imm16 },
	{ 0x09, "pop" },
	{ 0x0A, "link" },
	{ 0x0B, "pop_if",            args_op },
	{ 0x0C, "jump_if",           args_op_jump },
	{ 0x0D, "jal_if",            args_op_jump },
	{ 0x0E, "skip_if_not" },
	{ 0x0F, "skip" },
	{ 0x10, "nop" },
	{ 0x11, "call",              args_call },
	{ 0x12, "call_active",       args_call },
	{ 0x13, "set_acc",           args_imm16 },
	// 14
	// 15
	{ 0x16, "load_virtual_raw",  args_load_raw },
	{ 0x17, "load_seg_raw",      args_load_seg },
	{ 0x18, "load_seg_mio0",     args_load_seg },
	{ 0x19, "load_demo_head",    args_demo_head },
	{ 0x1A, "load_seg_mio0_ter", args_load_seg },
	{ 0x1B, "start_load_seq",    NULL, LSD_TAB_OPEN },
	{ 0x1D, "end_load_seq",      NULL, LSD_TAB_CLOSE },
	// 1e builds collision?
	{ 0x1F, "start_area",        args_def, LSD_TAB_OPEN },
	{ 0x20, "end_area",          NULL, LSD_TAB_CLOSE },
	{ 0x21, "def_disp_list",     args_def },
	{ 0x22, "def_geo_layout",    args_def },
	// 23
	{ 0x24, "set_object",        args_set_obj },
	{ 0x25, "load_mario_object" }, // need decoder
	{ 0x26, "connect_warp",      args_warp },
	{ 0x27, "connect_painting",  args_warp },
	{ 0x28, "set_col_warp" }, // ned decode
	// 29
	// 2a
	{ 0x2B, "set_player_pos" }, // need decode
	// 2b
	// 2c
	// 2d
	{ 0x2E, "set_specials",      args_imm32 },
	{ 0x2F, "set_geo_rendering", args_imm32 },
	{ 0x30, "show_dialog" },
	{ 0x31, "set_terrain",       args_terrain },
	{ 0x32, "nop" },
	{ 0x33, "fade_color",        args_transition },
	{ 0x34, "fade_black" },
	// 35
	{ 0x36, "set_music_a",       args_music_a },
	{ 0x37, "set_music_b",       args_music_b },
	// 38
	{ 0x39, "set_macros",        args_imm32 },
	// 3a
	// 3b
	// 3c
	{ 0 } // end
};

////////// API

lsd_ctx* lsd_create_ctx(u8* rom)
{
	lsd_ctx* ctx = (lsd_ctx*) malloc(sizeof(lsd_ctx));
	memset(ctx, 0x00, sizeof(lsd_ctx));
	
	ctx->rom = rom;
	ctx->output_buf_size = 4096;
	ctx->output_buf = malloc(4096);
	
	return ctx;
}

void lsd_destroy_ctx(lsd_ctx* ctx)
{
	free(ctx->output_buf);
	free(ctx);
}

void lsd_config_set(lsd_ctx* ctx, u32 cfg_mask)
{
	ctx->config |= cfg_mask;
}

void lsd_config_unset(lsd_ctx* ctx, u32 cfg_mask)
{
	ctx->config &= ~cfg_mask;
}

int lsd_add_script(lsd_ctx* ctx, u32 start_offset, u32 end_offset)
{
	for(int i = 0; i < ctx->scripts_idx; i++)
	{
		if(ctx->scripts[i].start == start_offset)
		{
			return -1;
		}
	}
	
	lsd_script script;
	
	script.start = start_offset;
	script.end = end_offset;
	script.b_decoded = 0;
	
	if(ctx->scripts_idx < LSD_MAX_SCRIPTS)
	{
		ctx->scripts[ctx->scripts_idx++] = script;
		return 1;
	}
	
	return 0;
}

// Return count of pending scripts
int lsd_count_pending_scripts(lsd_ctx* ctx)
{
	int count = 0;
	
	for(int i = 0; i < ctx->scripts_idx; i++)
	{
		if(ctx->scripts[i].b_decoded == 0)
		{
			count++;
		}
	}
	
	return count;
}

// Decode first undecoded script in the queue
void lsd_decode_next(lsd_ctx* ctx)
{
	for(int i = 0; i < ctx->scripts_idx; i++)
	{
		if(ctx->scripts[i].b_decoded == 0)
		{
			lsd_script script = ctx->scripts[i];
			
			if(ctx->config & LSD_CFG_VERBOSE)
			{
				printf("Decoding range %08X : %08X\n\n", script.start, script.end);
			}
			
			lsd_decode_range(ctx, script.start, script.end);
			ctx->scripts[i].b_decoded = 1;
			
			break;
		}
	}
}

/////////////////////

static void lsd_decode_args(lsd_ctx* ctx, lsd_arg* args)
{
	for(int i = 0;; i++)
	{
		lsd_arg arg = args[i];
		
		u32 value;
		
		const char* fmt_str = NULL;
		
		switch(arg.format)
		{
			case S8d:
				value = (char) lsd_u8(ctx, arg.rel_offset);
				fmt_str = "%d";
				break;
				
			case U8d:
				value = lsd_u8(ctx, arg.rel_offset);
				fmt_str = "%u";
				break;
				
			case U8h:
				value = lsd_u8(ctx, arg.rel_offset);
				fmt_str = "0x%02X";
				break;
				
			case S16d:
				value = (short) lsd_u16(ctx, arg.rel_offset);
				fmt_str = "%d";
				break;
				
			case U16d:
				value = lsd_u16(ctx, arg.rel_offset);
				fmt_str = "%d";
				break;
				
			case U16h:
				value = lsd_u16(ctx, arg.rel_offset);
				fmt_str = "0x%04X";
				break;
				
			case S32d:
				value = (int) lsd_u32(ctx, arg.rel_offset);
				fmt_str = "%d";
				break;
				
			case U32d:
				value = lsd_u32(ctx, arg.rel_offset);
				fmt_str = "%d";
				break;
				
			case U32h:
				value = lsd_u32(ctx, arg.rel_offset);
				fmt_str = "0x%08X";
				break;
		}
		
		lsd_label* label = NULL;
		
		if(arg.label_set != NULL)
		{
			label = label_lookup(arg.label_set, value);
		}
		
		if(label != NULL)
		{
			lsd_printf(ctx, label->name);
		}
		else
		{
			lsd_printf(ctx, fmt_str, value);
		}
		
		if(args[i + 1].rel_offset == 0)
		{
			break;
		}
		
		lsd_printf(ctx, ", ");
	}
}

static void lsd_dec_unhandled(lsd_ctx* ctx)
{
	u8 len_byte = lsd_u8(ctx, 1);
	
	lsd_printf(ctx, ".db ");
	
	for(int i = 0; i < len_byte; i++)
	{
		lsd_printf(ctx, "0x%02X", lsd_u8(ctx, i));
		
		if(i < len_byte - 1)
		{
			lsd_printf(ctx, ", ");
		}
	}
	
	lsd_printf(ctx, " ; Unhandled command");
}

static void lsd_collect_jump(lsd_ctx* ctx)
{
	u32 start_offset = lsd_u32(ctx, 4);
	u32 end_offset = lsd_u32(ctx, 8);
	lsd_add_script(ctx, start_offset, end_offset);
}

static void lsd_decode_range(lsd_ctx* ctx, u32 offset_start, u32 offset_end)
{
	ctx->output_indent = 0;

	ctx->offset = offset_start;
	
	while(ctx->offset < offset_end)
	{
		u8 cmd_byte = lsd_u8(ctx, 0);
		u8 len_byte = lsd_u8(ctx, 1);
		
		if(len_byte == 0)
		{
			// printf("\n// Error: hit zero length byte");
			break;
		}
		
		lsd_command* command = command_lookup(cmd_byte);
		
		if(command->indent == LSD_TAB_CLOSE)
		{
			lsd_adjust_indent(ctx, -1); //this crashes?
		}
		
		if(command != NULL)
		{
			lsd_print_indent(ctx);
			lsd_printf(ctx, "%-20s", command->name);
			
			if(command->args != NULL)
			{
				lsd_decode_args(ctx, command->args);
			}
			
			lsd_printf(ctx, "\n");
		}
		else
		{
			lsd_dec_unhandled(ctx);
		}
		
		if(command->indent == LSD_TAB_OPEN)
		{
			lsd_adjust_indent(ctx, 1); //this crashes?
		}
		
		if(cmd_byte == 0x00 || cmd_byte == 0x01)
		{
			lsd_collect_jump(ctx);
		}
		
		ctx->offset += len_byte;
	}
}

////////// Lookup implementations

static lsd_command* command_lookup(u8 cmd_byte)
{
	for(int i = 0; cmd_list[i].name != NULL; i++)
	{
		if(cmd_list[i].cmd_byte == cmd_byte)
		{
			return &cmd_list[i];
		}
	}
	return NULL;
}

static lsd_label* label_lookup(lsd_label* labelset, u32 value)
{
	for(int i = 0; labelset[i].name; i++)
	{
		if(labelset[i].value == value)
		{
			return &labelset[i];
		}
	}
	return NULL;
}

////////// Fetch implementations

static u8 lsd_u8(lsd_ctx* ctx, u32 rel_offset)
{
	return ctx->rom[ctx->offset + rel_offset];
}

static u16 lsd_u16(lsd_ctx* ctx, u32 rel_offset)
{
	u16 ret = U16BE(&ctx->rom[ctx->offset + rel_offset]);
	return ret;
}

static u32 lsd_u32(lsd_ctx* ctx, u32 rel_offset)
{
	u32 ret = U32BE(&ctx->rom[ctx->offset + rel_offset]);
	return ret;
}

////////// Printing implementation

static int lsd_printf(lsd_ctx* ctx, const char* fmt, ...)
{
	va_list valist;
	va_start(valist, fmt);

	u32 len = vsnprintf(NULL, 0, fmt, valist);
	
	if(ctx->config & LSD_CFG_VERBOSE)
	{
		vprintf(fmt, valist);
	}
	
	// Realloc until the output buffer is big enough
	while(ctx->output_buf_idx + len > ctx->output_buf_size)
	{
		//printf("growing output buffer\n");
		ctx->output_buf_size *= 2;
		ctx->output_buf = realloc(ctx->output_buf, ctx->output_buf_size);
	}
	
	int out_len = vsprintf(ctx->output_buf + ctx->output_buf_idx, fmt, valist);
	
	ctx->output_buf_idx += out_len;
	return out_len;
}

static void lsd_print_indent(lsd_ctx* ctx)
{
	for(int i = 0; i < ctx->output_indent; i++)
	{
		lsd_printf(ctx, LSD_INDENT_STR);
	}
}

static void lsd_adjust_indent(lsd_ctx* ctx, int tabs)
{
	ctx->output_indent += tabs;
	
	if(ctx->output_indent < 0)
	{
		ctx->output_indent = 0;
	}
}