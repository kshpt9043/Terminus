#include "Data/SkillTypes.h"

EStatusEffect GetStatusGroup(EStatusEffect Type)
{
	// Enum_State 시트의 GroupId 기준. 대표값은 먼저 있던 상태
	switch (Type)
	{
	case EStatusEffect::Dark:         return EStatusEffect::Acid;         // State_CostUp
	case EStatusEffect::Flooding:     return EStatusEffect::Ice;          // State_StatDown_ATK
	case EStatusEffect::Solid:        return EStatusEffect::Protection;   // State_StatUp_DEF
	case EStatusEffect::Metal:
	case EStatusEffect::Spirit:
	case EStatusEffect::Blindness:    return EStatusEffect::Indomitable;  // State_TD1
	case EStatusEffect::Contrarywind: return EStatusEffect::Fear;         // State_FDDown
	default:                          return Type;
	}
}

FString GetStatusDisplayName(EStatusEffect Type)
{
	// 시스템 이름 / 테마 이름
	auto Name = [](const TCHAR* System, const TCHAR* Theme)
	{
		return FCString::Strcmp(System, Theme) == 0 ? FString(System) : FString::Printf(TEXT("%s(%s)"), System, Theme);
	};

	switch (Type)
	{
	case EStatusEffect::Counter:        return Name(TEXT("반격"), TEXT("반격"));
	case EStatusEffect::Mark:           return Name(TEXT("급소"), TEXT("급소"));
	case EStatusEffect::Evasion:        return Name(TEXT("민첩"), TEXT("민첩"));
	case EStatusEffect::Poison:         return Name(TEXT("중독"), TEXT("중독"));
	case EStatusEffect::Brave:          return Name(TEXT("용기"), TEXT("용기"));
	case EStatusEffect::Protection:     return Name(TEXT("견고"), TEXT("가호"));
	case EStatusEffect::Ice:            return Name(TEXT("허약"), TEXT("얼음"));
	case EStatusEffect::Lava:           return Name(TEXT("허점"), TEXT("용암"));
	case EStatusEffect::Metal:          return Name(TEXT("불굴"), TEXT("강철"));
	case EStatusEffect::Acid:           return Name(TEXT("과욕"), TEXT("산성"));
	case EStatusEffect::Fear:           return Name(TEXT("공포"), TEXT("공포"));
	case EStatusEffect::Fusion:         return Name(TEXT("퓨전"), TEXT("퓨전"));
	case EStatusEffect::Vitality:       return Name(TEXT("활력"), TEXT("활력"));
	case EStatusEffect::Reflux:         return Name(TEXT("역류"), TEXT("역류"));
	case EStatusEffect::InternalInjury: return Name(TEXT("내상"), TEXT("내상"));
	case EStatusEffect::Immortality:    return Name(TEXT("불사"), TEXT("불사"));
	case EStatusEffect::Absorption:     return Name(TEXT("흡수"), TEXT("흡수"));
	case EStatusEffect::Solid:          return Name(TEXT("견고"), TEXT("견고"));
	case EStatusEffect::Indomitable:    return Name(TEXT("불굴"), TEXT("불굴"));
	case EStatusEffect::IronWall:       return Name(TEXT("철벽"), TEXT("철벽"));
	case EStatusEffect::Dark:           return Name(TEXT("과욕"), TEXT("어둠"));
	case EStatusEffect::Flooding:       return Name(TEXT("허약"), TEXT("침수"));
	case EStatusEffect::Contrarywind:   return Name(TEXT("공포"), TEXT("역풍"));
	case EStatusEffect::Spirit:         return Name(TEXT("불굴"), TEXT("정령화"));
	case EStatusEffect::Blindness:      return Name(TEXT("불굴"), TEXT("실명"));
	case EStatusEffect::SelfDamage:     return Name(TEXT("반동 피해"), TEXT("반동 피해"));
	default:                            return FString();
	}
}
