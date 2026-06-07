#include "Combat/Skills/ActionSkillCreature.h"

#include "Char/ActionCharacterBase.h"

// 技能生成物的轻量基类。来源角色用于阵营判断、伤害归属、仇恨和 VFX 上下文。
AActionSkillCreature::AActionSkillCreature()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AActionSkillCreature::InitSkillCreature(AActionCharacterBase* InSourceCharacter, FName InSourceSkillId)
{
	SourceCharacter = InSourceCharacter;
	SourceSkillId = InSourceSkillId;
}
