#include "HCM1HUD.h"

#include "HCM1PlayerController.h"
#include "M5VS2/HCM5VS2FlightComponent.h"
#include "M4/HCM4CombatComponent.h"
#include "M4R2/HCM4R2Navigation.h"
#include "M4R2/SHCM4R2Minimap.h"
#include "Engine/Canvas.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Fonts/SlateFontInfo.h"
#include "Fonts/CompositeFont.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
class SHCM1Overlay : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHCM1Overlay) {} SLATE_ARGUMENT(TWeakObjectPtr<AHCM1PlayerController>, Controller) SLATE_END_ARGS()

    void Construct(const FArguments& Arguments)
    {
        Controller = Arguments._Controller;
        const FString FontPath = FPaths::ProjectContentDir() / TEXT("HarborCity/UI/Fonts/DroidSansFallback.ttf");
        if (!FPaths::FileExists(FontPath)) UE_LOG(LogTemp, Error, TEXT("HCM1 runtime Chinese font missing: %s"), *FontPath);
        TSharedPtr<const FCompositeFont> RuntimeFont = MakeShared<FCompositeFont>(
            FName(TEXT("M1Chinese")), FontPath, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
        const FSlateFontInfo Body(RuntimeFont, 18);
        const FSlateFontInfo Heading(RuntimeFont, 28);
        const FSlateFontInfo Hint(RuntimeFont, 22);
        ChildSlot
        [
            SNew(SDPIScaler).DPIScale(this, &SHCM1Overlay::GetUIScale)
            [
            SNew(SOverlay)
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(26)
            [
                SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .Padding(20).Visibility(this, &SHCM1Overlay::ControlsVisibility).BorderBackgroundColor(FLinearColor(0.025f, 0.05f, 0.07f, 0.88f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [SNew(STextBlock).Text(this, &SHCM1Overlay::ExperienceTitle).Font(Heading).ColorAndOpacity(FLinearColor(0.5f, 0.9f, 0.85f))]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
                    [SNew(STextBlock).Text(this, &SHCM1Overlay::StateText).Font(Hint)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
                    [SNew(STextBlock).Text(this, &SHCM1Overlay::ControlsText).Font(Body).ColorAndOpacity(FLinearColor(0.82f, 0.88f, 0.92f))]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 0)
                    [SNew(STextBlock).Text(FText::FromString(TEXT("P / Esc  暂停\nF5  保存    F9  读取\n保存条件：步行站稳、车辆停稳"))).Font(Body).ColorAndOpacity(FLinearColor(0.67f, 0.75f, 0.8f))]
                ]
            ]
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(26)
            [
                SNew(SBox).WidthOverride(280).Visibility(this, &SHCM1Overlay::QuestVisibility)
                [SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [SNew(SHCM4R2Minimap).Navigation(Controller.IsValid() ? Controller->GetNavigationComponent() : nullptr)
                        .Font(Body).Visibility(this, &SHCM1Overlay::MapVisibility)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8)
                    [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                        .Padding(12).BorderBackgroundColor(FLinearColor(.025f,.055f,.065f,.9f))
                        [SNew(STextBlock).Text(this, &SHCM1Overlay::NavigationText).Font(Body).WrapTextAt(256)
                            .ColorAndOpacity(FLinearColor(.8f,.94f,.9f))]]
                    + SVerticalBox::Slot().AutoHeight()
                    [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                        .Padding(12).BorderBackgroundColor(FLinearColor(.025f,.055f,.065f,.9f))
                        [SNew(STextBlock).Text(this, &SHCM1Overlay::QuestText).Font(Body).WrapTextAt(256)
                            .ColorAndOpacity(FLinearColor(.8f,.94f,.9f))]]]
            ]
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(26)
            [
                SNew(SBox).WidthOverride(330).Visibility(this, &SHCM1Overlay::CombatVisibility)
                [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .Padding(16).BorderBackgroundColor(FLinearColor(.025f,.045f,.065f,.9f))
                    [SNew(STextBlock).Text(this, &SHCM1Overlay::CombatText).Font(Body).WrapTextAt(300)
                        .ColorAndOpacity(FLinearColor(.95f,.88f,.7f))]]
            ]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(20, 20, 20, 50)
            [
                SNew(SBox).WidthOverride(900).Visibility(this, &SHCM1Overlay::DialogueVisibility)
                [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .Padding(FMargin(4,2,2,2)).BorderBackgroundColor(FLinearColor(.18f,.81f,.76f,.94f))
                [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .Padding(26).BorderBackgroundColor(FLinearColor(.014f,.025f,.05f,.98f))
                    [SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
                        [SNew(STextBlock).Text(FText::FromString(TEXT("NIGHT RELAY    /    LINK ESTABLISHED"))).Font(Body).ColorAndOpacity(FLinearColor(.4f,.58f,.66f))]
                        + SVerticalBox::Slot().AutoHeight()
                        [SNew(STextBlock).Text(this, &SHCM1Overlay::DialogueName).Font(Heading).ColorAndOpacity(FLinearColor(.5f,.9f,.85f))]
                        + SVerticalBox::Slot().AutoHeight().Padding(0,16)
                        [SNew(STextBlock).Text(this, &SHCM1Overlay::DialogueLine).Font(Hint).WrapTextAt(835)]
                        + SVerticalBox::Slot().AutoHeight()
                        [SNew(STextBlock).Text(this, &SHCM1Overlay::DialogueHint).Font(Body).ColorAndOpacity(FLinearColor(.75f,.83f,.88f))]
                    ]]]
            ]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(20, 20, 20, 76)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [SNew(STextBlock).Text(this, &SHCM1Overlay::InteractionText).Font(Hint).ColorAndOpacity(FLinearColor(0.55f, 1.0f, 0.8f)).ShadowOffset(FVector2D(1, 2))]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 14, 0, 0)
                [SNew(STextBlock).Text(this, &SHCM1Overlay::MessageText).Font(Body).ColorAndOpacity(FLinearColor(1.0f, 0.86f, 0.6f)).ShadowOffset(FVector2D(1, 2))]
            ]
            + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)
            [
                SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .Visibility(this, &SHCM1Overlay::PauseVisibility)
                .BorderBackgroundColor(FLinearColor(0.005f, 0.015f, 0.025f, 0.8f)).HAlign(HAlign_Center).VAlign(VAlign_Center)
                [
                    SNew(SBox).WidthOverride(380)
                    [
                        SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                        .Padding(30).BorderBackgroundColor(FLinearColor(0.03f, 0.07f, 0.09f, 1.0f))
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 24)
                            [SNew(STextBlock).Text(FText::FromString(TEXT("已暂停"))).Font(Heading)]
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 5)
                            [SNew(SButton).HAlign(HAlign_Center).ContentPadding(10).OnClicked(this, &SHCM1Overlay::FootViewClicked)
                                [SNew(STextBlock).Text(this, &SHCM1Overlay::FootViewText).Font(Body)]]
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 5)
                            [SNew(SButton).HAlign(HAlign_Center).ContentPadding(10).OnClicked(this, &SHCM1Overlay::CarViewClicked)
                                [SNew(STextBlock).Text(this, &SHCM1Overlay::CarViewText).Font(Body)]]
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 5)
                            [SNew(SButton).HAlign(HAlign_Center).ContentPadding(10).OnClicked(this, &SHCM1Overlay::TrackingClicked)
                                [SNew(STextBlock).Text(this, &SHCM1Overlay::TrackingText).Font(Body)]]
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 5)
                            [SNew(SButton).HAlign(HAlign_Center).ContentPadding(12).OnClicked(this, &SHCM1Overlay::ContinueClicked)
                                [SNew(STextBlock).Text(FText::FromString(TEXT("继续游戏"))).Font(Hint)]]
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 5)
                            [SNew(SButton).HAlign(HAlign_Center).ContentPadding(12).OnClicked(this, &SHCM1Overlay::RestartClicked)
                                [SNew(STextBlock).Text(FText::FromString(TEXT("重新开始"))).Font(Hint)]]
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 5)
                            [SNew(SButton).HAlign(HAlign_Center).ContentPadding(12).OnClicked(this, &SHCM1Overlay::QuitClicked)
                                [SNew(STextBlock).Text(FText::FromString(TEXT("退出游戏"))).Font(Hint)]]
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 18, 0, 0)
                            [SNew(STextBlock).Text(FText::FromString(TEXT("P / Esc  继续"))).Font(Body)]
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 14, 0, 0)
                            [SNew(STextBlock).Visibility(this, &SHCM1Overlay::CharacterCreditVisibility)
                                .Text(FText::FromString(TEXT("Selestia  ©ジンゴ / STUDIO JINGO"))).Font(Body)
                                .ColorAndOpacity(FLinearColor(.65f,.72f,.76f))]
                        ]
                    ]
                ]
            ]
            ]
        ];
    }
private:
    TWeakObjectPtr<AHCM1PlayerController> Controller;
    EVisibility ControlsVisibility() const
    { return Controller.IsValid() && Controller->IsControlsPanelOpen() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }
    EVisibility CharacterCreditVisibility() const
    { return Controller.IsValid() && Controller->GetM5Story() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }
    float GetUIScale() const
    {
        int32 Width = 1920, Height = 1080;
        if (Controller.IsValid()) Controller->GetViewportSize(Width, Height);
        return FMath::Clamp(FMath::Min(Width / 1920.f, Height / 1080.f), .65f, 1.4f);
    }
    EVisibility MapVisibility() const
    { return Controller.IsValid() && Controller->GetNavigationComponent() && Controller->GetNavigationComponent()->IsMinimapEnabled() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }
    FText NavigationText() const
    {
        if (!Controller.IsValid() || !Controller->GetNavigationComponent()) return FText::GetEmpty();
        FString Text=Controller->GetNavigationComponent()->GetNavigationText();
        if (Controller->IsFlying()) Text += FString::Printf(TEXT("\n飞行高度 %.1f m"),Controller->GetFlightComponent()->GetHeightAboveSea()/100.f);
        return FText::FromString(Text);
    }
    FText TrackingText() const
    { return Controller.IsValid() && Controller->GetNavigationComponent() ? FText::FromString(Controller->GetNavigationComponent()->GetTrackingButtonText()) : FText::GetEmpty(); }
    FReply TrackingClicked()
    { if (Controller.IsValid() && Controller->IsPauseMenuOpen() && Controller->GetNavigationComponent()) Controller->GetNavigationComponent()->CycleTrackedQuest(); return FReply::Handled(); }
    FText ExperienceTitle() const
    { return Controller.IsValid() ? FText::FromString(Controller->GetExperienceTitle()) : FText::GetEmpty(); }
    FText StateText() const
    {
        const AHCM1PlayerController* PC = Controller.Get();
        if (!PC) return FText::GetEmpty();
        if (PC->IsFlying()) return FText::FromString(FString::Printf(TEXT("%s  高度 %.1f m"),
            PC->GetFlightComponent()->IsLanding() ? TEXT("降落中") : TEXT("天使飞行"),PC->GetFlightComponent()->GetHeightAboveSea()/100.f));
        switch (PC->GetPlayerMode())
        {
        case EHCPlayerMode::Driving: return FText::FromString(FString::Printf(TEXT("驾驶   %.0f km/h"), PC->GetSpeedKmh()));
        case EHCPlayerMode::Entering: return FText::FromString(TEXT("正在上车"));
        case EHCPlayerMode::Exiting: return FText::FromString(TEXT("正在下车"));
        default: return FText::FromString(TEXT("步行探索"));
        }
    }
    FText ControlsText() const
    {
        const AHCM1PlayerController* PC = Controller.Get();
        const bool bDriving = PC && (PC->GetPlayerMode() == EHCPlayerMode::Driving || PC->GetPlayerMode() == EHCPlayerMode::Exiting);
        if (PC && PC->IsFlying()) return FText::FromString(TEXT("WASD  水平移动    鼠标  环视\nSpace  上升    左 Ctrl  下降\nShift  沿镜头加速    F  降落\nV  第一／第三人称"));
        const bool bFlightAvailable=PC && PC->GetFlightComponent() && PC->GetFlightComponent()->IsFlightAvailable();
        if (bFlightAvailable && !bDriving) return FText::FromString(TEXT("WASD  移动    Shift  跑步\nSpace  跳跃    F  起飞\nE  互动    V  视角    Q  武器"));
        if (PC && PC->GetM5Story())
            return FText::FromString(bDriving ? TEXT("W/S  油门/刹车    A/D  转向\n鼠标  环视    V  视角\nE  停稳下车    R  复位")
                : TEXT("WASD  移动    Shift  跑步\nE  互动    V  视角    Q  武器"));
        FString Text = bDriving
            ? TEXT("鼠标  环视（无需按右键）\nW  加速    S  刹车／倒车\nA / D  转向    Space  手刹\nR  安全复位    E  停车后下车")
            : (PC && PC->GetM3Experience()
                ? TEXT("WASD  移动    鼠标  镜头\nShift  跑步    Space  跳跃\nE  上车／照明／交谈")
                : TEXT("WASD  移动    鼠标  镜头\nShift  跑步    Space  跳跃\nE  上车／照明开关"));
        Text += TEXT("\nV  第一／第三人称");
        if (PC && !PC->GetSceneControlHint().IsEmpty()) Text += TEXT("\n") + PC->GetSceneControlHint();
        return FText::FromString(Text);
    }
    FText InteractionText() const
    { return Controller.IsValid() ? FText::FromString(Controller->GetInteractionPrompt()) : FText::GetEmpty(); }
    FText MessageText() const
    { return Controller.IsValid() && !Controller->IsDialogueOpen() ? FText::FromString(Controller->GetStatusMessage()) : FText::GetEmpty(); }
    EVisibility CombatVisibility() const
    {
        const AHCM1PlayerController* PC = Controller.Get();
        return PC && PC->GetM3Experience() && PC->GetPlayerMode() == EHCPlayerMode::OnFoot
            && !PC->IsFlying() && !PC->IsDialogueOpen() && !PC->IsPauseMenuOpen() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
    }
    FText CombatText() const
    {
        const AHCM1PlayerController* PC = Controller.Get();
        return PC && PC->GetCombatComponent() ? FText::FromString(PC->GetCombatComponent()->GetHUDText()) : FText::GetEmpty();
    }
    EVisibility PauseVisibility() const
    { return Controller.IsValid() && Controller->IsPauseMenuOpen() ? EVisibility::Visible : EVisibility::Collapsed; }
    EVisibility QuestVisibility() const
    { return Controller.IsValid() && !Controller->GetQuestHUDText().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }
    EVisibility DialogueVisibility() const
    { return Controller.IsValid() && Controller->IsDialogueOpen() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }
    FText QuestText() const
    { return Controller.IsValid() ? FText::FromString(Controller->GetQuestHUDText()) : FText::GetEmpty(); }
    FText DialogueName() const
    { return Controller.IsValid() ? FText::FromString(Controller->GetDialogueName()) : FText::GetEmpty(); }
    FText DialogueLine() const
    { return Controller.IsValid() ? FText::FromString(Controller->GetDialogueLine()) : FText::GetEmpty(); }
    FText DialogueHint() const
    { return Controller.IsValid() ? FText::FromString(Controller->GetDialogueAdvanceHint()) : FText::GetEmpty(); }
    FReply ContinueClicked()
    { if (Controller.IsValid() && Controller->IsPauseMenuOpen()) Controller->TogglePauseMenu(); return FReply::Handled(); }
    FText FootViewText() const
    { return FText::FromString(Controller.IsValid() && Controller->GetOnFootPerspective() == EHCM4R2Perspective::FirstPerson ? TEXT("步行视角：第一人称") : TEXT("步行视角：第三人称")); }
    FText CarViewText() const
    { return FText::FromString(Controller.IsValid() && Controller->GetDrivingPerspective() == EHCM4R2Perspective::FirstPerson ? TEXT("驾驶视角：第一人称") : TEXT("驾驶视角：第三人称")); }
    FReply FootViewClicked()
    { if (Controller.IsValid()) Controller->ToggleOnFootPerspectivePreference(); return FReply::Handled(); }
    FReply CarViewClicked()
    { if (Controller.IsValid()) Controller->ToggleDrivingPerspectivePreference(); return FReply::Handled(); }
    FReply RestartClicked()
    { if (Controller.IsValid()) Controller->RestartPrototype(); return FReply::Handled(); }
    FReply QuitClicked()
    { if (Controller.IsValid()) Controller->QuitPrototype(); return FReply::Handled(); }
};
}

void AHCM1HUD::DrawHUD()
{
    Super::DrawHUD();
    const AHCM1PlayerController* PC = Cast<AHCM1PlayerController>(GetOwningPlayerController());
    const UHCM4CombatComponent* Combat = PC ? PC->GetCombatComponent() : nullptr;
    if (!Canvas || !Combat || !Combat->CanUseCombat() || Combat->GetWeaponMode() != EHCM4WeaponMode::Pistol) return;
    const float X = Canvas->SizeX * .5f, Y = Canvas->SizeY * .5f;
    const float Gap = FMath::Lerp(15.f, 4.f, Combat->GetAimBlend());
    const FLinearColor Color = Combat->IsReloading() ? FLinearColor(.65f,.65f,.65f) : FLinearColor(.9f,1.f,.95f);
    DrawLine(X-Gap-6,Y,X-Gap,Y,Color,1.5f); DrawLine(X+Gap,Y,X+Gap+6,Y,Color,1.5f);
    DrawLine(X,Y-Gap-6,X,Y-Gap,Color,1.5f); DrawLine(X,Y+Gap,X,Y+Gap+6,Color,1.5f);
}

void AHCM1HUD::BeginPlay()
{
    Super::BeginPlay();
    AHCM1PlayerController* PC = Cast<AHCM1PlayerController>(GetOwningPlayerController());
    UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    if (PC && PC->IsLocalController() && Viewport)
    {
        OverlayWidget = SNew(SHCM1Overlay).Controller(PC);
        Viewport->AddViewportWidgetContent(OverlayWidget.ToSharedRef(), 20);
    }
}

void AHCM1HUD::EndPlay(const EEndPlayReason::Type Reason)
{
    if (OverlayWidget.IsValid() && GetWorld() && GetWorld()->GetGameViewport())
        GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(OverlayWidget.ToSharedRef());
    OverlayWidget.Reset();
    Super::EndPlay(Reason);
}
