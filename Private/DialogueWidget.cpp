#include "DialogueWidget.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Input/Reply.h" 
#include "Framework/Application/SlateApplication.h"

void UDialogueWidget::NativeConstruct()
{
    Super::NativeConstruct();
    bIsFocusable = true;   // 키 입력 받기
    Index = 0;
    RefreshLine();

    if (Button_CatchAll)
    {
        Button_CatchAll->OnClicked.Clear();
        Button_CatchAll->OnClicked.AddDynamic(this, &UDialogueWidget::HandleClicked);
        Button_CatchAll->SetIsEnabled(true);
        Button_CatchAll->SetVisibility(ESlateVisibility::Visible);
    }

    SetKeyboardFocus();
}

void UDialogueWidget::InitDialogue(const TArray<FText>& InLines)
{
    Lines = InLines;
    Index = 0;
    RefreshLine();
}

FReply UDialogueWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        NextLine();
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UDialogueWidget::HandleClicked()
{
    NextLine();
}

FReply UDialogueWidget::NativeOnKeyDown(const FGeometry& Geo, const FKeyEvent& KeyEvent)
{
    const FKey Key = KeyEvent.GetKey();
    if (Key == EKeys::SpaceBar || Key == EKeys::Enter)
    {
        NextLine();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geo, KeyEvent);
}

void UDialogueWidget::NextLine()
{
    if (Lines.Num() == 0)
    {
        OnFinished.Broadcast();
        RemoveFromParent();
        return;
    }

    ++Index;

    if (Index < Lines.Num())
    {
        // 다음 줄 보여주기
        RefreshLine();
    }
    else
    {
        // 끝
        OnFinished.Broadcast();
        RemoveFromParent();
    }
}

void UDialogueWidget::RefreshLine()
{
    if (!Text_Line) return;

    const FText Line = Lines.IsValidIndex(Index)
        ? Lines[Index]
        : FText::GetEmpty();

    Text_Line->SetText(Line);
}