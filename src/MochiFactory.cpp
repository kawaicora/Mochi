#include "MochiFactory.h"
bool MochiFactory::CompleteProdution(FactoryClass* pFactory) {
	if (!pFactory) {
		return false;
	}
	if (!pFactory->Owner) {
		return false;
	}

	if (pFactory->Owner->Available_Money() >= pFactory->Balance) {
		pFactory->Owner->TakeMoney(pFactory->Balance);
		pFactory->Production.Value = 54;
		pFactory->Balance = 0;
		pFactory->Production.HasChanged = 1;
		pFactory->IsDifferent = 1;
		pFactory->IsSuspended = 1;
		pFactory->Production.Step = Unsorted::CurrentFrame;
		CDTimerClass timer{};
		pFactory->Production.Timer = timer;
	}
	return true;
}