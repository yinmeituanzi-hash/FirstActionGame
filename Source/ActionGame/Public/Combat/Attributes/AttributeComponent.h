#pragma once

#include "CoreMinimal.h"
#include "Combat/Attributes/AttributeTypes.h"
#include "Components/ActorComponent.h"
#include "AttributeComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FAttributeChangedSignature,
	EAttributeType,
	Attribute,
	float,
	OldValue,
	float,
	NewValue);

/**
 * 角色战斗属性组件。
 *
 * 这是项目自己的轻量 AttrMap：角色、技能、AI、UI 都通过统一入口读写属性。
 * AActionCharacterBase 不再保存 HP / AttackPower 等字段，默认值和运行时值都由这里管理。
 */
UCLASS(ClassGroup = (Action), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAttributeComponent();

	UPROPERTY(BlueprintAssignable, Category = "Action|Attributes")
	FAttributeChangedSignature OnAttributeChanged;

	/** BeginPlay 时调用：把编辑器默认属性复制成运行时权威属性。 */
	UFUNCTION(BlueprintCallable, Category = "Action|Attributes")
	void InitializeAttributesFromDefaults();

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	float GetAttribute(EAttributeType Attribute) const;

	UFUNCTION(BlueprintCallable, Category = "Action|Attributes")
	void SetAttribute(EAttributeType Attribute, float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Action|Attributes")
	float ModifyAttribute(EAttributeType Attribute, float Delta);

	UFUNCTION(BlueprintPure, Category = "Action|Attributes")
	bool HasAttribute(EAttributeType Attribute) const;

	UFUNCTION(BlueprintCallable, Category = "Action|Attributes")
	void ClampCoreAttributes();

private:
	/** 编辑器配置用默认属性。不同角色的基础 HP / 攻击力优先在这里配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action|Attributes", meta = (AllowPrivateAccess = "true"))
	TMap<EAttributeType, float> DefaultAttributeValues;

	/** 运行时权威属性表。伤害、技能、AI、UI 都应读取这里。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Action|Attributes", meta = (AllowPrivateAccess = "true"))
	TMap<EAttributeType, float> AttributeValues;

	void EnsureDefaultAttributes();
	float GetClampedValue(EAttributeType Attribute, float Value) const;
	void SetAttributeInternal(EAttributeType Attribute, float NewValue, bool bBroadcast);
};
