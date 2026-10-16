#include "Widgets/Common/TerminusUIBuilder.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Fonts/CompositeFont.h"
#include "Misc/Paths.h"
#include "Widgets/TerminusUIColors.h"

namespace TerminusUI
{
	// =====================================================================
	// 글꼴
	// =====================================================================

	namespace
	{
		// Content/UI/Fonts 의 TTF 로 만든 글꼴 하나 (Regular / Bold). 패키징 땐 DefaultGame.ini 에서 UFS 로 같이 실림
		TSharedPtr<const FCompositeFont> GetThemeCompositeFont()
		{
			static TSharedPtr<const FCompositeFont> Font;
			if (!Font.IsValid())
			{
				const FString Dir = FPaths::ProjectContentDir() / TEXT("UI/Fonts");
				const FString Regular = Dir / TEXT("NotoSansKR-Regular.ttf");
				const FString Bold = Dir / TEXT("NotoSansKR-Bold.ttf");

				if (FPaths::FileExists(Regular))
				{
					TSharedRef<FStandaloneCompositeFont> NewFont = MakeShared<FStandaloneCompositeFont>();
					NewFont->DefaultTypeface.AppendFont(TEXT("Regular"), Regular, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
					NewFont->DefaultTypeface.AppendFont(TEXT("Bold"), FPaths::FileExists(Bold) ? Bold : Regular, EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
					Font = NewFont;
				}
			}
			return Font;
		}

		bool IsBoldTypeface(const FName& Typeface)
		{
			const FString Name = Typeface.ToString();
			return Name.Contains(TEXT("Bold")) || Name.Contains(TEXT("Black")) || Name.Contains(TEXT("Heavy"));
		}

		// 테마 글꼴로 바꾼 정보. 크기 / 외곽선 / 자간 / 재질은 그대로
		FSlateFontInfo ToThemeFont(const FSlateFontInfo& Source)
		{
			const TSharedPtr<const FCompositeFont> Composite = GetThemeCompositeFont();
			if (!Composite.IsValid()) return Source;

			FSlateFontInfo Result(Composite, Source.Size, IsBoldTypeface(Source.TypefaceFontName) ? FName(TEXT("Bold")) : FName(TEXT("Regular")), Source.OutlineSettings);
			Result.LetterSpacing = Source.LetterSpacing;
			Result.FontMaterial = Source.FontMaterial;
			Result.SkewAmount = Source.SkewAmount;
			return Result;
		}

		bool IsThemeFont(const FSlateFontInfo& Font)
		{
			return Font.CompositeFont.IsValid() && Font.CompositeFont == GetThemeCompositeFont();
		}

		UTexture2D* LoadUITexture(const TCHAR* Name)
		{
			const FString Path = FString::Printf(TEXT("/Game/UI/Textures/Common/%s.%s"), Name, Name);
			return LoadObject<UTexture2D>(nullptr, *Path);
		}

		FSlateBrush MakeHexBrush(const TCHAR* TextureName)
		{
			FSlateBrush Brush;
			if (UTexture2D* Texture = LoadUITexture(TextureName))
			{
				Brush.SetResourceObject(Texture);
			}
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.ImageSize = FVector2D(280.f, 56.f);
			Brush.Margin = FMargin(18.f / 280.f, 0.f);   // 양 끝 18px 는 늘이지 않음
			return Brush;
		}

		bool SameBrush(const FSlateBrush& A, const FSlateBrush& B)
		{
			return A.DrawAs == B.DrawAs && A.GetResourceObject() == B.GetResourceObject() && A.TintColor == B.TintColor
				&& A.OutlineSettings.Color == B.OutlineSettings.Color && A.OutlineSettings.Width == B.OutlineSettings.Width;
		}

		// 디자이너에서 손대지 않은 UButton 기본 모양인가
		bool IsEngineDefaultButtonStyle(const FButtonStyle& Style)
		{
			const FButtonStyle& Default = GetDefault<UButton>()->GetStyle();
			return SameBrush(Style.Normal, Default.Normal) && SameBrush(Style.Hovered, Default.Hovered) && SameBrush(Style.Pressed, Default.Pressed);
		}
	}

	FSlateFontInfo GetFont(int32 Size, bool bBold)
	{
		const TSharedPtr<const FCompositeFont> Composite = GetThemeCompositeFont();
		if (!Composite.IsValid())
		{
			FSlateFontInfo Fallback = FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
			return Fallback;
		}
		return FSlateFontInfo(Composite, Size, bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular")));
	}

	void ApplyTheme(UWidgetTree* Tree)
	{
		if (!Tree || !GetThemeCompositeFont().IsValid()) return;

		Tree->ForEachWidget([](UWidget* Widget)
		{
			if (UTextBlock* Text = Cast<UTextBlock>(Widget))
			{
				if (!IsThemeFont(Text->GetFont()))
				{
					Text->SetFont(ToThemeFont(Text->GetFont()));

					// 디자이너 기본 흰 글자 -> 본문색 (따뜻한 흰색)
					const FSlateColor& Color = Text->GetColorAndOpacity();
					if (Color.IsColorSpecified() && Color.GetSpecifiedColor().Equals(FLinearColor::White, 0.001f))
					{
						Text->SetColorAndOpacity(FSlateColor(Color::TextBody));
					}
				}
			}
			else if (UButton* Button = Cast<UButton>(Widget))
			{
				if (IsEngineDefaultButtonStyle(Button->GetStyle()))
				{
					Button->SetStyle(MakeFlatButtonStyle(FLinearColor::White));
				}
			}
		});
	}

	// =====================================================================
	// 글자
	// =====================================================================

	UTextBlock* MakeText(UWidgetTree* Tree, const FName& Name, const FText& Initial, int32 Size, const FLinearColor& Color, ETextJustify::Type Justify)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		if (!Initial.IsEmpty())
		{
			Text->SetText(Initial);
		}
		Text->SetFont(GetFont(Size, false));
		// 흰색으로 부르던 곳은 본문색으로 (팔레트에 순백 없음)
		Text->SetColorAndOpacity(FSlateColor(Color.Equals(FLinearColor::White, 0.001f) ? Color::TextBody : Color));
		Text->SetJustification(Justify);
		return Text;
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const FName& Name, const FString& Initial, int32 Size, const FLinearColor& Color, ETextJustify::Type Justify)
	{
		return MakeText(Tree, Name, FText::FromString(Initial), Size, Color, Justify);
	}

	UTextBlock* MakeWrapText(UWidgetTree* Tree, const FName& Name, const FString& Initial, int32 Size, const FLinearColor& Color, ETextJustify::Type Justify)
	{
		UTextBlock* Text = MakeText(Tree, Name, Initial, Size, Color, Justify);
		Text->SetAutoWrapText(true);
		return Text;
	}

	UTextBlock* MakeTitle(UWidgetTree* Tree, const FName& Name, const FString& Initial, int32 Size)
	{
		UTextBlock* Text = MakeText(Tree, Name, Initial, Size, Color::GoldBase);
		SetBold(Text);
		SetLetterSpacing(Text, 80);   // 0.08em
		return Text;
	}

	void SetBold(UTextBlock* Text, bool bBold)
	{
		if (!Text) return;
		FSlateFontInfo Font = Text->GetFont();
		Font.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
		Text->SetFont(Font);
	}

	void SetLetterSpacing(UTextBlock* Text, int32 Spacing)
	{
		if (!Text) return;
		FSlateFontInfo Font = Text->GetFont();
		Font.LetterSpacing = Spacing;
		Text->SetFont(Font);
	}

	void AddShadow(UTextBlock* Text, float Opacity)
	{
		if (!Text) return;
		Text->SetShadowOffset(FVector2D(1.f, 1.f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, Opacity));
	}

	// =====================================================================
	// 버튼
	// =====================================================================

	FButtonStyle MakeFlatButtonStyle(const FLinearColor& Fill)
	{
		const bool bThemeFill = Fill.Equals(FLinearColor::White, 0.001f);
		const FLinearColor Base = bThemeFill ? Color::Card : Fill;
		const FLinearColor Hover = bThemeFill ? Color::Disabled : Base * 1.15f;

		FButtonStyle Style;
		Style.Normal = MakeBoxBrush(Base, Color::Line);
		Style.Hovered = MakeBoxBrush(Hover, Color::GoldBase);
		Style.Pressed = MakeBoxBrush(Color::Section, Color::GoldPressed);
		Style.Disabled = MakeBoxBrush(Color::Disabled, Color::Line);
		Style.NormalPadding = FMargin(24.f, 10.f);
		Style.PressedPadding = FMargin(24.f, 11.f, 24.f, 9.f);
		Style.NormalForeground = FSlateColor(Color::TextBody);
		Style.HoveredForeground = FSlateColor(Color::GoldHover);
		Style.PressedForeground = FSlateColor(Color::TextSub);
		Style.DisabledForeground = FSlateColor(Color::TextFaint);
		return Style;
	}

	FButtonStyle MakeHexButtonStyle(EHexButton Kind)
	{
		const TCHAR* Prefix = Kind == EHexButton::Primary ? TEXT("Primary") : Kind == EHexButton::Secondary ? TEXT("Secondary") : TEXT("Gold");

		FButtonStyle Style;
		Style.Normal = MakeHexBrush(*FString::Printf(TEXT("T_UI_Hex_%s_Normal"), Prefix));
		Style.Hovered = MakeHexBrush(*FString::Printf(TEXT("T_UI_Hex_%s_Hover"), Prefix));
		Style.Pressed = MakeHexBrush(*FString::Printf(TEXT("T_UI_Hex_%s_Pressed"), Prefix));
		Style.Disabled = MakeHexBrush(TEXT("T_UI_Hex_Disabled"));
		Style.NormalPadding = FMargin(28.f, 12.f);
		Style.PressedPadding = FMargin(28.f, 13.f, 28.f, 11.f);
		Style.NormalForeground = FSlateColor(Color::TextBody);
		Style.HoveredForeground = FSlateColor(Color::TextBody);
		Style.PressedForeground = FSlateColor(Color::TextSub);
		Style.DisabledForeground = FSlateColor(Color::TextFaint);
		return Style;
	}

	void StyleHexButton(UButton* Button, EHexButton Kind)
	{
		if (!Button) return;
		Button->SetStyle(MakeHexButtonStyle(Kind));
		Button->SetBackgroundColor(FLinearColor::White);
	}

	void StyleFlatButton(UButton* Button, const FLinearColor& Fill)
	{
		if (!Button) return;
		Button->SetStyle(MakeFlatButtonStyle(Fill));
		Button->SetBackgroundColor(FLinearColor::White);
	}

	UButton* MakeButton(UWidgetTree* Tree, const FName& Name, const FString& Label, int32 Size, const FLinearColor& TextColor, const FLinearColor& Background)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetStyle(MakeFlatButtonStyle(Background));

		// 예전엔 흰 바탕에 검은 글자가 기본이었음 -> 어두운 판 위라 본문색으로
		const bool bOldDefaultText = TextColor.Equals(FLinearColor::Black, 0.001f) || TextColor.Equals(FLinearColor::White, 0.001f);
		UTextBlock* Text = MakeText(Tree, NAME_None, Label, Size, bOldDefaultText ? Color::TextBody : TextColor);
		SetBold(Text, Size >= FontSize::Subtitle);
		Button->SetContent(Text);
		return Button;
	}

	UButton* MakeHexButton(UWidgetTree* Tree, const FName& Name, const FString& Label, EHexButton Kind, int32 Size)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		StyleHexButton(Button, Kind);

		UTextBlock* Text = MakeText(Tree, NAME_None, Label, Size, Color::TextBody);
		SetBold(Text);
		SetLetterSpacing(Text, 120);   // 0.12em
		Button->SetContent(Text);
		return Button;
	}

	// =====================================================================
	// 판
	// =====================================================================

	UVerticalBoxSlot* AddRow(UVerticalBox* Column, UWidget* Widget, float Top, EHorizontalAlignment Align)
	{
		UVerticalBoxSlot* Slot = Column ? Column->AddChildToVerticalBox(Widget) : nullptr;
		if (Slot)
		{
			Slot->SetHorizontalAlignment(Align);
			Slot->SetPadding(FMargin(0.f, Top, 0.f, 0.f));
		}
		return Slot;
	}

	UBorder* MakeDimRoot(UWidgetTree* Tree, const FName& Name, const FLinearColor& Color)
	{
		UBorder* Dim = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Dim->SetBrushColor(Color);
		Dim->SetHorizontalAlignment(HAlign_Center);
		Dim->SetVerticalAlignment(VAlign_Center);
		Tree->RootWidget = Dim;
		return Dim;
	}

	FSlateBrush MakeBoxBrush(const FLinearColor& Fill, const FLinearColor& Outline, float Radius, float OutlineWidth)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(Radius, Radius, Radius, Radius), FSlateColor(Outline), OutlineWidth);
		return Brush;
	}

	UBorder* MakePanel(UWidgetTree* Tree, const FName& Name, const FLinearColor& Color, const FMargin& Padding)
	{
		UBorder* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Panel->SetBrush(MakeBoxBrush(FLinearColor(Color.R, Color.G, Color.B, 1.f), Color::Line));
		Panel->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, Color.A));   // 반투명 판은 알파만 따로
		Panel->SetPadding(Padding);
		return Panel;
	}

	UImage* MakeGoldDivider(UWidgetTree* Tree, bool bTitle)
	{
		UImage* Line = Tree->ConstructWidget<UImage>();
		FSlateBrush Brush;
		if (UTexture2D* Texture = LoadUITexture(TEXT("T_UI_GoldDivider")))
		{
			Brush.SetResourceObject(Texture);
		}
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = FVector2D(256.f, bTitle ? 2.f : 1.f);
		Line->SetBrush(Brush);
		Line->SetColorAndOpacity(bTitle ? Color::GoldBase : Color::GoldPressed);
		return Line;
	}

	// =====================================================================
	// 기타
	// =====================================================================

	void SetTextIfChanged(UTextBlock* Text, const FText& Value)
	{
		if (Text && !Text->GetText().EqualTo(Value))
		{
			Text->SetText(Value);
		}
	}

	void SetTextIfChanged(UTextBlock* Text, const FString& Value)
	{
		if (Text && Text->GetText().ToString() != Value)
		{
			Text->SetText(FText::FromString(Value));
		}
	}
}
