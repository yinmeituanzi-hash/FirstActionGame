#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ActionCharacterMovementComponent.generated.h"

UCLASS()
class ACTIONGAME_API UActionCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|RootMotion")
	bool bEnableRootMotionZExtraction = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|RootMotion", meta = (ClampMin = "0.0"))
	float RootMotionZScale = 1.0f;

	virtual FVector ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const override;
};
