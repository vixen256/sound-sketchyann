#include "diva.h"

namespace diva {
SIG_SCAN (sigOperatorNew, "40 53 48 83 EC 20 48 8B ?? EB 0F", 0x1409777D0, 0x1409774C0, 0x140978A10, 0x14097DEC0);
SIG_SCAN (sigOperatorDelete, "48 85 C9 74 37 53 48 83 EC 20 4C 8B", 0x1409B1E90, 0x1409B1B80, 0x1409B30D0, 0x1409B8580);
SIG_SCAN_XREF (sigFreeString, "E8 ?? ?? ?? ?? 8B 53 1C", 0x14014EE9D, 0x14014EF5D, 0x14014EF1D);
SIG_SCAN_XREF (sigResolveFilePath, "E8 ?? ?? ?? ?? 84 C0 75 29 4C", 0x14024A894, 0x14024A884, 0x14024A9F4, 0x14024A234);
SIG_SCAN (sigPlaySoundEffect, "48 8B D1 0F 28", 0x1405AA540, 0x1405AA500, 0x1405ABF20, 0x1405B1250);
SIG_SCAN_XREF (sigGetPvLoadData, "E8 ?? ?? ?? ?? FF 40 14", 0x1404044A4, 0x140404464, 0x140404564, 0x140409194);
SIG_SCAN_XREF (sigGetPvGameData, "E8 ?? ?? ?? ?? 8B 88 34", 0x14023B283, 0x14023B273, 0x14023B3E3, 0x14023AC63);
SIG_SCAN_XREF (sigSoundEffectCanPlay, "E8 ?? ?? ?? ?? 44 8B 4D F8 84", 0x14026BDDB, 0x14026BDCB, 0x14026BECB, 0x14026B9FB);
SIG_SCAN (sigSoundEffectRelease, "40 53 48 83 EC 20 41 0F B6 ?? 48", 0x1405AAD00, 0x1405AACC0, 0x1405AC6E0, 0x1405B1A10);
SIG_SCAN_XREF (sigIsSurvival, "E8 ?? ?? ?? ?? 48 BF 67", 0x140206D4A, 0x140206D3A, 0x140206E5A, 0x1402066DA);
SIG_SCAN_XREF (sigSoundDbPropertyInsertNode, "E8 ?? ?? ?? ?? 48 01 ?? 20", 0x14018385A, 0x14018393A, 0x14018378A);
SIG_SCAN_XREF (sigGetAetMarker, "E8 ?? ?? ?? ?? 8B C? 0F 2F", 0x1401B5C36, 0x1401B5D26, 0x1401B5B76);

FUNCTION_PTR (void *, operatorNew, sigOperatorNew (), u64);
FUNCTION_PTR (void *, operatorDelete, sigOperatorDelete (), void *);
FUNCTION_PTR (void, FreeString, sigFreeString (), string *);
FUNCTION_PTR (bool, ResolveFilePath, sigResolveFilePath (), string *from);
FUNCTION_PTR (i32, PlaySoundEffect, sigPlaySoundEffect (), const char *name, float volume);
FUNCTION_PTR (void *, GetPvLoadData, sigGetPvLoadData ());
FUNCTION_PTR (void *, GetPvGameData, sigGetPvGameData ());
FUNCTION_PTR (bool, SoundEffectCanPlay, sigSoundEffectCanPlay (), i32 queue_index, const char *name);
FUNCTION_PTR (bool, SoundEffectRelease, sigSoundEffectRelease (), i32 queue_index, const char *name, bool force_release);
FUNCTION_PTR (bool, IsSurvival, sigIsSurvival ());
FUNCTION_PTR (void, SoundDbPropertyInsertNode, sigSoundDbPropertyInsertNode (), map<string, sound_db_property> *root, pair<mapElement<string, sound_db_property> *, u32> *location,
              mapElement<string, sound_db_property> *element);
FUNCTION_PTR (f32, GetAetMarker, sigGetAetMarker (), i32 sceneId, const char *layerName, const char *marker);

SIG_SCAN_XREF (sigRomDirs, "48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 90 EB 22", 0x1402A306D, 0x1402A305D, 0x1402A319D, 0x1402A2CCD);
SIG_SCAN_XREF (sigSoundWork, "4C 8D 05 ?? ?? ?? ?? 42 80", 0x1405AA70A, 0x1405AA6CA, 0x1405AC0EA, 0x1405B141A);
SIG_SCAN_XREF (sigModules, "48 8D 0D ?? ?? ?? ?? 48 83 C4 20 5B E9 ?? ?? ?? ?? CC 48 83 EC 28 48 8B", 0x140A57A0E, 0x140A576FE, 0x140A58C4E, 0x140A5E50E);

vector<string> *romDirs                    = (vector<string> *)sigRomDirs ();
map<string, sound_db_property> *sound_work = (map<string, sound_db_property> *)sigSoundWork ();
vector<ModuleData> *modules                = (vector<ModuleData> *)sigModules ();

} // namespace diva
