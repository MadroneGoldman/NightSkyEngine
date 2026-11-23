#include "NightSkyBattleRules.h"

UNightSkyBattleRules::UNightSkyBattleRules()
{
	SectionName = TEXT("Battle Rules");
}

int32 UNightSkyBattleRules::ScaleSimpleInputDamage(const int32 Damage)
{
	const int32 Percent = FMath::Clamp(GetDefault<UNightSkyBattleRules>()->SimpleInputDamageReductionPercent, 0, 100);
	return Damage * (100 - Percent) / 100;
}
