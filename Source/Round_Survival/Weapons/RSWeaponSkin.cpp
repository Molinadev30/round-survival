// Round Survival

#include "Weapons/RSWeaponSkin.h"

#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

void URSWeaponSkin::ApplyToComponent(const URSWeaponSkin* Skin, UMeshComponent* Component)
{
	if (!Component)
	{
		return;
	}

	// Back to the mesh's own materials first, so what is read below as "the original" is the
	// mesh's material and not the instance a previous skin left there.
	Component->EmptyOverrideMaterials();

	if (!Skin || !Skin->Material)
	{
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < Component->GetNumMaterials(); ++SlotIndex)
	{
		UMaterialInterface* Original = Component->GetMaterial(SlotIndex);
		if (!Skin->CanReskin(Original))
		{
			continue;
		}

		// One instance per slot, because each slot keeps its own textures.
		UMaterialInstanceDynamic* Skinned = UMaterialInstanceDynamic::Create(Skin->Material, Component);

		for (const FName& Parameter : Skin->CopiedTextureParameters)
		{
			UTexture* Texture = nullptr;
			if (Original->GetTextureParameterValue(FHashedMaterialParameterInfo(Parameter), Texture) && Texture)
			{
				Skinned->SetTextureParameterValue(Parameter, Texture);
			}
		}

		Component->SetMaterial(SlotIndex, Skinned);
	}
}

bool URSWeaponSkin::CanReskin(const UMaterialInterface* Original) const
{
	if (!Original || CopiedTextureParameters.IsEmpty())
	{
		return false;
	}

	for (const FName& Parameter : CopiedTextureParameters)
	{
		UTexture* Texture = nullptr;
		if (!Original->GetTextureParameterValue(FHashedMaterialParameterInfo(Parameter), Texture) || !Texture)
		{
			return false;
		}
	}

	return true;
}
