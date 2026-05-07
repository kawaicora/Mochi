#pragma once

#include <ArrayClasses.h>
#include <CCFileClass.h>
#include <CCINIClass.h>
#include <Helpers/Macro.h>

class INILoader
{
public:
	static int LastReadIndex;
	static DynamicVectorClass<CCINIClass*> LoadedINIs;
	static DynamicVectorClass<char*> LoadedINIFiles;

	static CCFileClass* CCFileClass_CTOR() {
		alignas(CCFileClass) char buffer[sizeof(CCFileClass)];
		auto ccFile = reinterpret_cast<CCFileClass*>(buffer);
		memset(ccFile, 0, sizeof(CCFileClass));
		return ccFile;
	}
};


int INILoader::LastReadIndex = -1;
DynamicVectorClass<CCINIClass*> INILoader::LoadedINIs;
DynamicVectorClass<char*> INILoader::LoadedINIFiles;

DEFINE_HOOK(0x474200, CCINIClass_ReadCCFile1, 6)
{
	GET(CCINIClass*, pINI, ECX);
	GET(CCFileClass*, pFile, EAX);

	const char* filename = pFile->GetFileName();
	Debug::Log("CCINIClass_ReadCCFile1 %s \n", filename);
	INILoader::LoadedINIs.AddItem(pINI);
	INILoader::LoadedINIFiles.AddItem(_strdup(filename));
	return 0;
}

DEFINE_HOOK(0x474314, CCINIClass_ReadCCFile2, 6)
{
	char buffer[0x80];
	CCINIClass* xINI =INILoader::LoadedINIs[INILoader::LoadedINIs.Count - 1];

	if (!xINI) {
		return 0;
	}

	const char* section = "#include";

	int len = xINI->GetKeyCount(section);
	for (int i =INILoader::LastReadIndex; i < len; i =INILoader::LastReadIndex) {
		const char* key = xINI->GetKeyName(section, i);
		++INILoader::LastReadIndex;
		buffer[0] = '\0';
		if (xINI->ReadString(section, key, "", buffer)) {
			bool canLoad = true;
			for (int j = 0; j <INILoader::LoadedINIFiles.Count; ++j) {
				if (!strcmp(INILoader::LoadedINIFiles[j], buffer)) {
					canLoad = false;
					break;
				}
			}

			if (canLoad) {
				CCFileClass* xFile = GameCreate<CCFileClass>(buffer);
				if (xFile->Exists()) {
					xINI->ReadCCFile(xFile);
				}
				GameDelete(xFile);
			}
		}
	}

	INILoader::LoadedINIs.RemoveItem(INILoader::LoadedINIs.Count - 1);
	if (!INILoader::LoadedINIs.Count) {
		for (int j =INILoader::LoadedINIFiles.Count - 1; j >= 0; --j) {
			if (char* ptr =INILoader::LoadedINIFiles[j]) {
				free(ptr);
			}
			INILoader::LoadedINIFiles.RemoveItem(j);
		}
		INILoader::LastReadIndex = -1;
	}
	return 0;
}
