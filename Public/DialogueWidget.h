#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"
#include "Components/Button.h"
#include "DialogueWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogueFinished);

class UTextBlock;

UCLASS()
class RHYTHMFIGHTER_API UDialogueWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    TArray<FText> Lines;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FName LevelToOpen;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* Text_Line;

    UPROPERTY(meta = (BindWidgetOptional))
    UButton* Button_CatchAll;

    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void InitDialogue(const TArray<FText>& InLines);

    UPROPERTY(BlueprintAssignable)
    FOnDialogueFinished OnFinished;

    UFUNCTION(BlueprintCallable)
    void NextLine();                

protected:
    virtual void NativeConstruct() override;

    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
            const FPointerEvent& InMouseEvent) override;

    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry,
            const FKeyEvent& InKeyEvent) override;

private:
    UFUNCTION() void HandleClicked();
    int32 Index = 0;
    void RefreshLine();
};