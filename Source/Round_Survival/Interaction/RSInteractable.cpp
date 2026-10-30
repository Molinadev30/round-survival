// Round Survival

#include "Interaction/RSInteractable.h"

#define LOCTEXT_NAMESPACE "RoundSurvival"

const FString IRSInteractable::PromptKeyToken = TEXT("{Key}");

FText IRSInteractable::MakeActionPrompt(const FText& Action, float HoldSeconds)
{
	// The token goes in as an argument, so Format inserts it as it is rather than reading it as a
	// placeholder of its own.
	const FText KeyToken = FText::FromString(PromptKeyToken);

	return HoldSeconds > 0.0f
		? FText::Format(LOCTEXT("InteractHold", "Hold {0} {1}"), KeyToken, Action)
		: FText::Format(LOCTEXT("InteractPress", "Press {0} {1}"), KeyToken, Action);
}

#undef LOCTEXT_NAMESPACE
