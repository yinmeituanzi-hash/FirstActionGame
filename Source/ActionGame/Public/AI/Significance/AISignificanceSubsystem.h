#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AISignificanceSubsystem.generated.h"

class UAISignificanceComponent;

/**
 * 全局 AI 重要度管理器。
 *
 * 统一采样玩家位置，再让每个 AI 的组件
 * 按距离、可见性、战斗状态计算等级。Subsystem 不直接知道 BT / Mesh / Movement
 * 如何降级，具体响应交给 UAISignificanceComponent。
 */
UCLASS()
class ACTIONGAME_API UAISignificanceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UAISignificanceSubsystem* Get(const UObject* WorldContextObject);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	void RegisterComponent(UAISignificanceComponent* Component);
	void UnregisterComponent(UAISignificanceComponent* Component);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action|AI|Significance", meta = (ClampMin = "0.02"))
	float UpdateInterval = 0.2f;

private:
	TArray<TWeakObjectPtr<UAISignificanceComponent>> RegisteredComponents;
	float TimeSinceLastUpdate = 0.0f;

	void UpdateSignificance();
};
