// Round Survival

#include "Enemies/RSDismembermentProfile.h"

#include "Engine/SkeletalMesh.h"

const FRSBodyPartRule* URSDismembermentProfile::FindRule(ERSBodyPart Part) const
{
	return Parts.FindByPredicate([Part](const FRSBodyPartRule& Rule) { return Rule.Part == Part; });
}

const FRSDismemberedMeshSet* URSDismembermentProfile::FindMeshSet(const USkeletalMesh* IntactMesh) const
{
	return MeshSets.FindByPredicate([IntactMesh](const FRSDismemberedMeshSet& Set) { return Set.IntactMesh == IntactMesh; });
}
