#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AIBudgetSubsystem.generated.h"

class UAIBudgetComponent;

/**
 * 全局 AI Tick 配额分配器。
 *
 * Significance 先回答“单只怪需要多高精度”；Budget 再处理“高重要度怪太多时，
 * 哪些怪优先保留 Tick”。第一版采用 Enabled / Disabled，接口保持可扩展。
 */
UCLASS()
class ACTIONGAME_API UAIBudgetSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UAIBudgetSubsystem* Get(const UObject* WorldContextObject);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	void RegisterComponent(UAIBudgetComponent* Component);
	void UnregisterComponent(UAIBudgetComponent* Component);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI|Budget", meta = (ClampMin = "1"))
	int32 MaxActiveAITickCount = 60;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI|Budget", meta = (ClampMin = "0.05"))
	float ReevaluateInterval = 0.5f;

private:
	TArray<TWeakObjectPtr<UAIBudgetComponent>> RegisteredComponents;
	float TimeSinceLastUpdate = 0.0f;

	void UpdateBudget();
	void DrawDebugInfo(int32 ActiveCount, int32 TotalCount, int32 ProtectedCount) const;
};
