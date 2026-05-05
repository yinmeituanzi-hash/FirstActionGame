#pragma once

#include "NativeGameplayTags.h"

namespace ActionGameplayTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_Attacking);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_Dodging);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_HitReact);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Action_Dead);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Block_Attack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Block_Dodge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Block_Move);

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Window_Attack_CanDodgeCancel);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Window_Attack_CanCombo);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Window_Attack_CanTurn);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Window_Dodge_CanRecover);
}
