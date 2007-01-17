/* Copyright (C) 2007 Eugeniy Meshcheryakov <eugen@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SFNT_NAMES_H
#include <cairo.h>
#include <cairo-pdf.h>
#include <cairo-ft.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "unicode_blocks.h"

#define A4_WIDTH	(8.3*72)
#define A4_HEIGHT	(11.7*72)

#define xmin_border	(72.0/1.5)
#define ymin_border	(72.0)
#define cell_width	((A4_WIDTH - 2*xmin_border) / 16)
#define cell_height	((A4_HEIGHT - 2*ymin_border) / 16)

static const char *font_file_name;
static const char *output_file_name;

static void usage(const char *);

static void parse_options(int argc, char * const argv[])
{
	for (;;) {
		int c;

		c = getopt(argc, argv, "f:o:h");

		if (c == -1)
			break;

		switch (c) {
		case 'f':
			if (font_file_name) {
				fprintf(stderr, "Font file name should be given only once!\n");
				exit(1);
			}
			font_file_name = optarg;
			break;
		case 'o':
			if (output_file_name) {
				fprintf(stderr, "Output file name should be given only once!\n");
				exit(1);
			}
			output_file_name = optarg;
			break;
		case 'h':
			usage(argv[0]);
			exit(0);
			break;
		case '?':
		default:
			usage(argv[0]);
			exit(1);
			break;
		}
	}
	if (!font_file_name || !output_file_name) {
		usage(argv[0]);
		exit(1);
	}
}

static const struct unicode_block *get_unicode_block(unsigned long charcode)
{
	const struct unicode_block *block;

	for (block = unicode_blocks; block->name; block++) {
		if ((charcode >= block->start) && (charcode <= block->end))
			return block;
	}
	return NULL;
}

static int is_in_block(unsigned long charcode, const struct unicode_block *block)
{
	return ((charcode >= block->start) && (charcode <= block->end));
}

static void draw_header(cairo_t *cr, const char *face_name, const char *range_name)
{
	cairo_text_extents_t extents;

	cairo_select_font_face (cr, "Times", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size (cr, 12.0);
	cairo_text_extents(cr, face_name, &extents);
	cairo_move_to(cr, (A4_WIDTH-extents.width)/2.0, 30.0);
	cairo_show_text(cr, face_name);

	cairo_select_font_face (cr, "Helvetica", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size (cr, 12.0);
	cairo_text_extents(cr, range_name, &extents);
	cairo_move_to(cr, (A4_WIDTH-extents.width)/2.0, 50.0);
	cairo_show_text(cr, range_name);
}

static void draw_cell(cairo_t *cr, cairo_font_face_t *face, FT_Face ft_face,
		double x, double y, FT_ULong charcode, unsigned long idx)
{
#define DOTTED_CIRCLE	0x25CC
	char buf[9];
	cairo_glyph_t glyphs[2];
	cairo_text_extents_t extents;
	int combining = (g_unichar_type(charcode) == G_UNICODE_COMBINING_MARK) ||
		(g_unichar_type(charcode) == G_UNICODE_ENCLOSING_MARK) ||
		(g_unichar_type(charcode) == G_UNICODE_NON_SPACING_MARK);
	FT_UInt circle_idx = 0;
	int nglyphs;

	if (combining)
		circle_idx = FT_Get_Char_Index(ft_face, DOTTED_CIRCLE);
	if (!circle_idx)
		combining = 0;

#if 1
	/* TODO fix ditted circle */
	combining = 0;
#endif

	if (combining) {
		glyphs[0] = (cairo_glyph_t){circle_idx, 0, 0};
		glyphs[1] = (cairo_glyph_t){idx, 0, 0};
		nglyphs = 2;
	}
	else {
		glyphs[0] = (cairo_glyph_t){idx, 0, 0};
		nglyphs = 1;
	}


	cairo_set_font_face(cr, face);
	cairo_set_font_size(cr, 20.0);

	cairo_glyph_extents(cr, glyphs, 1, &extents);

	glyphs[0].x += x + (cell_width - extents.width)/2.0 - extents.x_bearing;
	glyphs[0].y += y + cell_height / 2.0;
	if (nglyphs == 2) {
		glyphs[1].x = glyphs[0].x + extents.x_advance;
		glyphs[1].y = glyphs[0].y;
	}

	cairo_show_glyphs(cr, glyphs, nglyphs);

	/* draw glyph unicode value */
	snprintf(buf, sizeof(buf), "%04lX", charcode);
	cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 8.0);
	cairo_text_extents(cr, buf, &extents);
	cairo_move_to(cr, x + (cell_width - extents.width)/2.0, y + cell_height - 4.0);
	cairo_show_text(cr, buf);
}

static void draw_grid(cairo_t *cr, unsigned int x_cells,
		unsigned long block_start)
{
	unsigned int i;
	double x_min = (A4_WIDTH - x_cells * cell_width) / 2;
	double x_max = (A4_WIDTH + x_cells * cell_width) / 2;
	char buf[9];
	cairo_text_extents_t extents;

#define TABLE_H (A4_HEIGHT - ymin_border * 2)
	cairo_rectangle(cr, x_min, ymin_border, x_max - x_min, TABLE_H);
	cairo_move_to(cr, x_min, ymin_border);
	cairo_line_to(cr, x_min, ymin_border - 15.0);
	cairo_move_to(cr, x_max, ymin_border);
	cairo_line_to(cr, x_max, ymin_border - 15.0);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	cairo_set_line_width(cr, 0.5);
	/* draw horizontal lines */
	for (i = 1; i < 16; i++) {
		cairo_move_to(cr, x_min, 72.0 + i * TABLE_H/16);
		cairo_line_to(cr, x_max, 72.0 + i * TABLE_H/16);
	}

	/* draw vertical lines */
	for (i = 1; i < x_cells; i++) {
		cairo_move_to(cr, x_min + i * cell_width, ymin_border);
		cairo_line_to(cr, x_min + i * cell_width, A4_HEIGHT - ymin_border);
	}
	cairo_stroke(cr);

	/* draw glyph numbers */
	buf[1] = '\0';
#define hexdigs	"0123456789ABCDEF"
	cairo_select_font_face(cr, "Helvetica", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 12.0);

	for (i = 0; i < 16; i++) {
		buf[0] = hexdigs[i];
		cairo_text_extents(cr, buf, &extents);
		cairo_move_to(cr, x_min -  extents.x_advance /*+ extents.x_bearing*/ - 5.0,
				72.0 + (i+0.5) * TABLE_H/16 + extents.height/2);
		cairo_show_text(cr, buf);
		cairo_move_to(cr, x_min + x_cells * cell_width + 5.0,
				72.0 + (i+0.5) * TABLE_H/16 + extents.height/2);
		cairo_show_text(cr, buf);
	}

	for (i = 0; i < x_cells; i++) {
		snprintf(buf, sizeof(buf), "%03lX", block_start / 16 + i);
		cairo_text_extents(cr, buf, &extents);
		cairo_move_to(cr, x_min + i*cell_width + (cell_width - extents.width)/2,
				ymin_border - 5.0);
		cairo_show_text(cr, buf);
	}

}

static void draw_empty_cell(cairo_t *cr, double x, double y, unsigned long charcode)
{
	cairo_save(cr);
	if (g_unichar_isdefined(charcode)) {
		if (g_unichar_iscntrl(charcode))
			cairo_set_source_rgb(cr, 0.0, 0.0, 0.5);
		else
			cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
	}
	cairo_rectangle(cr, x, y, cell_width, cell_height);
	cairo_fill(cr);
	cairo_restore(cr);
}

static unsigned long draw_unicode_block(cairo_t *cr, cairo_font_face_t *face,
		FT_Face ft_face, const char *fontname, unsigned long charcode,
		const struct unicode_block *block)
{
	FT_UInt idx;
	unsigned long prev_charcode;
	unsigned long prev_cell;

	idx = FT_Get_Char_Index(ft_face, charcode);

	do {
		unsigned long masked_charcode = (charcode & ~0xffL) + (block->start & 0xf0);
		unsigned long tbl_start = (masked_charcode > block->start) ?
			masked_charcode : block->start;
		unsigned long tbl_end = ((masked_charcode + 0x100L) > block->end) ?
			(block->end | 0xf) + 1 : masked_charcode + 0x100L;
		unsigned int rows = (tbl_end - tbl_start) / 16;
		double x_min = (A4_WIDTH - rows * cell_width) / 2;
		unsigned long i;

		draw_header(cr, fontname, block->name);
		prev_cell = tbl_start - 1;

		do {
			for (i = prev_cell + 1; i < charcode; i++) {
				draw_empty_cell(cr, x_min + cell_width*((i - tbl_start) / 16),
							ymin_border + cell_height*((i - tbl_start) % 16), i);
			}
			draw_cell(cr, face, ft_face, x_min + cell_width*((charcode - tbl_start) / 16),
					ymin_border + cell_height*((charcode - tbl_start) % 16),
					charcode, idx);
			prev_charcode = charcode;
			prev_cell = charcode;
			charcode = FT_Get_Next_Char(ft_face, charcode, &idx);
		} while (idx && (charcode < tbl_end) && is_in_block(charcode, block));
		
		for (i = prev_cell + 1; i < tbl_end; i++) {
			draw_empty_cell(cr, x_min + cell_width*((i - tbl_start) / 16),
					ymin_border + cell_height*((i - tbl_start) % 16),
					i);
		}

		draw_grid(cr, rows, tbl_start);
		cairo_show_page(cr);
	} while (idx && is_in_block(charcode, block));

	return prev_charcode;
}

static void draw_glyphs(cairo_t *cr, cairo_font_face_t *face, FT_Face ft_face,
		const char *fontname)
{
	FT_ULong charcode;
	FT_UInt idx;
	const struct unicode_block *block;

	charcode = FT_Get_First_Char(ft_face, &idx);

	while (idx) {
		block = get_unicode_block(charcode);
		if (block) {
			charcode = draw_unicode_block(cr, face, ft_face, fontname, charcode, block);
		}
		charcode = FT_Get_Next_Char(ft_face, charcode, &idx);
	}
}

static void usage(const char *cmd)
{
	fprintf(stderr, "Usage: %s -f FONT-FILE -o OUTPUT-FILE\n"
			"       %s -h\n" , cmd, cmd);
}

int main(int argc, char **argv)
{
	cairo_surface_t *surface;
	cairo_t *cr;
	FILE *file;
	FT_Error error;
	FT_Library library;
	FT_Face face;
	FT_SfntName face_name;
	char *fontname; /* full name of the font */
	cairo_font_face_t *cr_face;

	parse_options(argc, argv);

	file = fopen(output_file_name, "w");
	if (!file) {
		perror("fopen");
		exit(2);
	}

	error = FT_Init_FreeType(&library);
	if (error) {
		fprintf(stderr, "Freetype error\n");
		exit(3);
	}

	error = FT_New_Face(library, font_file_name, 0, &face);
	if (error) {
		fprintf(stderr, "Failed to create new face\n");
		exit(4);
	}

	error = FT_Get_Sfnt_Name(face, 4 /* full font name */, &face_name);
	if (error) {
		fprintf(stderr, "Failed to get face name\n");
		exit(5);
	}

	fontname = malloc(face_name.string_len + 1);
	if (!fontname) {
		perror("malloc");
		exit(6);
	}
	memcpy(fontname, face_name.string, face_name.string_len);
	fontname[face_name.string_len] = '\0';
	
	cr_face = cairo_ft_font_face_create_for_ft_face(face, 0);

	surface = cairo_pdf_surface_create(output_file_name, A4_WIDTH, A4_HEIGHT); /* A4 paper */

	cr = cairo_create(surface);
	cairo_surface_destroy(surface);

	draw_glyphs(cr, cr_face, face, fontname);
	cairo_destroy(cr);
	fclose(file);
	return 0;
}

