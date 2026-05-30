#include "FactoryHook.h"
#include <Helpers/Macro.h>
Event<FactoryClass*> FactoryHook::ProgressUpdateEvent;
Event<FactoryClass*> FactoryHook::FactoryClassCreateEvent;


DEFINE_HOOK(0x4C9B20, FactoryClass_ProgressUpdate, 5) {
	GET(FactoryClass*, pFactory, ECX);
	FactoryHook::ProgressUpdateEvent.Invoke(pFactory);
	return 0;
}

DEFINE_HOOK(0x4C9DF6, FactoryClass_CTOR, 6) {
	GET(FactoryClass*, pFactory, ESI);
	FactoryHook::FactoryClassCreateEvent.Invoke(pFactory);
	return 0;
}

