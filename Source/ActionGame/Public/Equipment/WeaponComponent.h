#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponComponent.generated.h"

class ACharacter;
class UMeshComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * EWeaponHand
 * 标识武器挂在哪只手。
 */
UENUM(BlueprintType)
enum class EWeaponHand : uint8
{
	Right UMETA(DisplayName = "Right"),
	Left  UMETA(DisplayName = "Left")
};

/**
 * UWeaponComponent
 *
 * 角色武器挂载/拆卸管理。挂在 ACharacter 子类上。
 *
 * 第一版职责（保持简单）：
 *   1. 持有左右手武器 MeshComponent
 *   2. 提供 EquipDualSword / EquipSingle / Unequip API
 *   3. Attach 到 Skeleton 上指定的 Socket（默认 WeaponSocket_R / WeaponSocket_L）
 *   4. 支持 StaticMesh 和 SkeletalMesh 两种武器资源
 *
 * 不做的事（避免过度设计）：
 *   - 拔刀/收刀切换、武器栏、武器属性数据 → 等真正需要时再加
 *
 * 在编辑器里使用：
 *   - 打开 Skeleton，给 hand_r / hand_l 加 socket WeaponSocket_R / WeaponSocket_L
 *   - 在 BP_ActionPlayerCharacter 上配 DefaultRightHandStaticMesh / DefaultLeftHandStaticMesh
 *   - 角色 BeginPlay 时会自动装备默认武器
 */
UCLASS(ClassGroup = (Equipment), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponComponent();

	// ---------- 默认 Socket 名（按 UE 标准命名）----------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Socket")
	FName RightHandSocketName = TEXT("WeaponSocket_R");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Socket")
	FName LeftHandSocketName = TEXT("WeaponSocket_L");

	// ---------- 默认武器（可在 BP 详情面板里指定）----------

	/** 默认装备的右手 StaticMesh 武器。BeginPlay 自动装备。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Default")
	TObjectPtr<UStaticMesh> DefaultRightHandStaticMesh;

	/** 默认装备的左手 StaticMesh 武器。BeginPlay 自动装备。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Default")
	TObjectPtr<UStaticMesh> DefaultLeftHandStaticMesh;

	/** 如果武器是 SkeletalMesh（带骨骼/物理）则用这两个字段。比 StaticMesh 优先级高。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Default")
	TObjectPtr<USkeletalMesh> DefaultRightHandSkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Default")
	TObjectPtr<USkeletalMesh> DefaultLeftHandSkeletalMesh;

	// ---------- API ----------

	/** 装备一把 StaticMesh 武器到指定手。会替换原有武器。 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void EquipStaticMesh(EWeaponHand Hand, UStaticMesh* WeaponMesh);

	/** 装备一把 SkeletalMesh 武器到指定手。会替换原有武器。 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void EquipSkeletalMesh(EWeaponHand Hand, USkeletalMesh* WeaponMesh);

	/** 卸下指定手的武器（销毁该手的 MeshComponent）。 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Unequip(EWeaponHand Hand);

	/** 同时卸下两只手。 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void UnequipAll();

	/** 查询某只手当前是否装备了武器。 */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool HasWeapon(EWeaponHand Hand) const;

	/** 拿到某只手当前的 MeshComponent（可能是 StaticMesh 也可能是 SkeletalMesh）。 */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	UMeshComponent* GetWeaponMeshComponent(EWeaponHand Hand) const;

protected:
	virtual void BeginPlay() override;

private:
	ACharacter* GetCharacterOwner() const;
	FName GetSocketNameForHand(EWeaponHand Hand) const;

	/** 通用 Attach：销毁旧 mesh 组件、动态新建一个并 Attach 到 Skeleton socket。 */
	void AttachStaticMeshToSocket(EWeaponHand Hand, UStaticMesh* WeaponMesh);
	void AttachSkeletalMeshToSocket(EWeaponHand Hand, USkeletalMesh* WeaponMesh);

	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> RightHandWeapon;

	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> LeftHandWeapon;
};
