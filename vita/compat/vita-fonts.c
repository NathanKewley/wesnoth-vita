/*
 * PS Vita: fonts are read from memory, not from their files.
 *
 * FreeType reads a font file a little at a time, as it needs each table and each glyph's outline,
 * and through the C library every one of those reads is a seek and a read on the memory card.
 * The first time a piece of text used a new size or style, drawing it took a third of a second to
 * over a second - the hover and selection hitches in game. Each font file is read whole the first
 * time it is opened, kept, and handed to FreeType as memory (FT_New_Face is wrapped at link time;
 * cairo and pango open their fonts through it).
 */
#include <ft2build.h>
#include FT_FREETYPE_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct font_file
{
	char* path;
	FT_Byte* data;
	FT_Long size;
	struct font_file* next;
};

static struct font_file* font_files;
static pthread_mutex_t font_files_lock = PTHREAD_MUTEX_INITIALIZER;

FT_Error __real_FT_New_Face(FT_Library library, const char* path, FT_Long face_index, FT_Face* face);

/* The file's bytes, read the first time; null if it can't be read. Never freed: faces use them. */
static const struct font_file* font_file(const char* path)
{
	struct font_file* f;
	for(f = font_files; f; f = f->next) {
		if(strcmp(f->path, path) == 0) {
			return f;
		}
	}

	FILE* file = fopen(path, "rb");
	if(!file) {
		return NULL;
	}
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	FT_Byte* data = size > 0 ? malloc(size) : NULL;
	if(!data || fread(data, 1, size, file) != (size_t)size) {
		free(data);
		fclose(file);
		return NULL;
	}
	fclose(file);

	f = malloc(sizeof(*f));
	f->path = strdup(path);
	f->data = data;
	f->size = size;
	f->next = font_files;
	font_files = f;
	return f;
}

FT_Error __wrap_FT_New_Face(FT_Library library, const char* path, FT_Long face_index, FT_Face* face)
{
	pthread_mutex_lock(&font_files_lock);
	const struct font_file* f = path ? font_file(path) : NULL;
	pthread_mutex_unlock(&font_files_lock);

	if(!f) {
		return __real_FT_New_Face(library, path, face_index, face);
	}
	return FT_New_Memory_Face(library, f->data, f->size, face_index, face);
}
