#include "Char/ActionCharacterMovementComponent.h"

FVector UActionCharacterMovementComponent::ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const
{
	FVector Result = Super::ConstrainAnimRootMotionVelocity(RootMotionVelocity, CurrentVelocity);

	if (!bEnableRootMotionZExtraction)
	{
		Result.Z = CurrentVelocity.Z;
	}

	Result.Z *= RootMotionZScale;
	return Result;
}
