#include "diva.h"

namespace diva {
FUNCTION_PTR (void *, operatorNew, 0x1409777D0, u64);
FUNCTION_PTR (void *, operatorDelete, 0x1409B1E90, void *);
FUNCTION_PTR (void, FreeString, 0x14014BCD0, string *);
FUNCTION_PTR (void, DefaultSprArgs, 0x1405B78D0, SprArgs *args);
FUNCTION_PTR (SprArgs *, DrawSpr, 0x1405B49C0, SprArgs *args);
FUNCTION_PTR (bool, ResolveFilePath, 0x1402A5320, string *from);
FUNCTION_PTR (int, GetLanguage, 0x1402C8D20);
FUNCTION_PTR (Texture *, TextureLoadTex2D, 0x1405F0720, u32 id, i32 format, u32 width, u32 height, i32 mip_levels, void **data, i32, bool generate_mips);
FUNCTION_PTR (i32, PlaySoundEffect, 0x1405AA540, const char *name, float volume);
FUNCTION_PTR (void, FileHandlerLoad, 0x1402A4710, void **p_file_handler, const char *file, bool loadNow);
FUNCTION_PTR (bool, FileHandlerCheckNotReady, 0x151C03830, void *p_file_handler);
FUNCTION_PTR (void *, FileHandlerCheckGetData, 0x151C0EF70, void *p_file_handler);
FUNCTION_PTR (void *, FileHandlerCheckGetDataSize, 0x151C7ADA, void *p_file_handler);
FUNCTION_PTR (void, FarcRead, 0x1402A0750, farc *farc);
FUNCTION_PTR (bool, SoundDbFarcRead, 0x159FABCC0, sound_db_farc *farc);
FUNCTION_PTR (void *, GetPvLoadData, 0x14040B2A0);
FUNCTION_PTR (void *, GetPvGameData, 0x140266720);
FUNCTION_PTR (bool, SoundEffectCanPlay, 0x1405AAFB0, i32 queue_index, const char *name);
FUNCTION_PTR (bool, SoundEffectRelease, 0x1405AAD00, i32 queue_index, const char *name, bool force_release);
FUNCTION_PTR (bool, IsSurvival, 0x14023B6A0);

vector<string> *romDirs                    = (vector<string> *)0x1414AB8A0;
map<string, sound_db_property> *sound_work = (map<string, sound_db_property> *)0x14CBFC520;
vector<ModuleData> *modules                = (vector<ModuleData> *)0x1416EBEE8;

SprArgs::SprArgs () {
	memset (this, 0, sizeof (SprArgs));
	DefaultSprArgs (this);
}

} // namespace diva
