// Copyright Epic Games, Inc. All Rights Reserved.

#include "PresetColorsIni.h"

#include "FGFactoryColoringTypes.h"
#include "FGGameState.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Patching/NativeHookManager.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogPresetColorsIni, Log, All);

#define LOCTEXT_NAMESPACE "FPresetColorsIniModule"

namespace
{
	// In the game's Saved folder (%LOCALAPPDATA%\FactoryGame\Saved).
	const TCHAR* const ConfigFileName = TEXT("PresetColorsIni.ini");

	// Written with the presets the player saved in the current save.
	const TCHAR* const ExportFileName = TEXT("PresetColorsIni.Exported.ini");

	// The only section that is read. Other sections are ignored.
	const TCHAR* const ColorsSection = TEXT("Colors");

	const TCHAR* const ExampleFile =
		TEXT("; Preset Colors INI - extra entries for the color picker presets.\n")
		TEXT("; Read once when a game starts. Restart the game (or reload your save) after editing.\n")
		TEXT(";\n")
		TEXT("; Format, one color per line, under [Colors]:\n")
		TEXT(";   Name=#RRGGBB        hex, as shown in the color picker\n")
		TEXT(";   Name=R,G,B          each 0-255\n")
		TEXT(";   Name=hsv(H,S,V)     hue 0-360, saturation 0-1, value 0 or more; above 1 is brighter than white\n")
		TEXT(";   Name=linear(R,G,B)  the game's own linear float values (1 = white), no conversion; a 4th value is alpha\n")
		TEXT("; Lines starting with ; or # are comments. Remove the leading ; below to enable an example.\n")
		TEXT("\n")
		TEXT("[Colors]\n")
		TEXT(";Sunset Orange=#FF7A1A\n")
		TEXT(";Deep Teal=0,128,128\n")
		TEXT(";Super White=hsv(0,0,10)\n");

	/** Parses "hsv(H,S,V)": hue 0-360, saturation 0-1, value 0 and up (above 1 is brighter than white). Gives a linear color. */
	bool ParseHsv(const FString& Value, FLinearColor& OutColor)
	{
		const int32 Open = Value.Find(TEXT("("));
		if (Open == INDEX_NONE || !Value.EndsWith(TEXT(")")))
		{
			return false;
		}
		TArray<FString> Parts;
		Value.Mid(Open + 1, Value.Len() - Open - 2).ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() != 3)
		{
			return false;
		}
		float Numbers[3];
		for (int32 i = 0; i < 3; ++i)
		{
			const FString Part = Parts[i].TrimStartAndEnd();
			if (!Part.IsNumeric())
			{
				return false;
			}
			Numbers[i] = FCString::Atof(*Part);
		}
		const float Hue = FMath::Fmod(FMath::Fmod(Numbers[0], 360.f) + 360.f, 360.f);
		OutColor = FLinearColor(Hue, FMath::Clamp(Numbers[1], 0.f, 1.f), FMath::Max(Numbers[2], 0.f)).HSVToLinearRGB();
		OutColor.A = 1.f;
		return true;
	}

	/**
	 * True if two colors are the same. Ordinary colors compare as 8-bit sRGB, which is what a hex entry in the file can hold;
	 * colors brighter than white compare as floats.
	 */
	bool SameColor(const FLinearColor& A, const FLinearColor& B)
	{
		const auto InRange = [](const FLinearColor& C) { return C.R <= 1.f && C.G <= 1.f && C.B <= 1.f; };
		if (InRange(A) && InRange(B))
		{
			return A.ToFColor(true) == B.ToFColor(true);
		}
		return A.Equals(B, 1e-3f);
	}

	/** Parses "linear(R,G,B)" or "linear(R,G,B,A)": the game's own linear float values, used as is. Alpha defaults to 1. */
	bool ParseLinear(const FString& Value, FLinearColor& OutColor)
	{
		const int32 Open = Value.Find(TEXT("("));
		if (Open == INDEX_NONE || !Value.EndsWith(TEXT(")")))
		{
			return false;
		}
		TArray<FString> Parts;
		Value.Mid(Open + 1, Value.Len() - Open - 2).ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() != 3 && Parts.Num() != 4)
		{
			return false;
		}
		float Numbers[4] = { 0.f, 0.f, 0.f, 1.f };
		for (int32 i = 0; i < Parts.Num(); ++i)
		{
			const FString Part = Parts[i].TrimStartAndEnd();
			if (!Part.IsNumeric())
			{
				return false;
			}
			Numbers[i] = FCString::Atof(*Part);
		}
		OutColor = FLinearColor(Numbers[0], Numbers[1], Numbers[2], Numbers[3]);
		return true;
	}

	/** Parses "hsv(H,S,V)", "#RRGGBB" / "RRGGBB" or "R,G,B" (0-255, sRGB) into a linear color. */
	bool ParseColor(const FString& Text, FLinearColor& OutColor)
	{
		FString Value = Text.TrimStartAndEnd();

		if (Value.StartsWith(TEXT("hsv"), ESearchCase::IgnoreCase))
		{
			return ParseHsv(Value, OutColor);
		}
		if (Value.StartsWith(TEXT("linear"), ESearchCase::IgnoreCase))
		{
			return ParseLinear(Value, OutColor);
		}

		if (Value.Contains(TEXT(",")))
		{
			TArray<FString> Parts;
			Value.ParseIntoArray(Parts, TEXT(","));
			if (Parts.Num() != 3)
			{
				return false;
			}
			int32 Channels[3];
			for (int32 i = 0; i < 3; ++i)
			{
				const FString Part = Parts[i].TrimStartAndEnd();
				if (!Part.IsNumeric() || Part.Contains(TEXT(".")))
				{
					return false;
				}
				Channels[i] = FMath::Clamp(FCString::Atoi(*Part), 0, 255);
			}
			OutColor = FLinearColor::FromSRGBColor(FColor(Channels[0], Channels[1], Channels[2]));
			return true;
		}

		Value.RemoveFromStart(TEXT("#"));
		if (Value.Len() != 6)
		{
			return false;
		}
		for (const TCHAR Ch : Value)
		{
			if (!FChar::IsHexDigit(Ch))
			{
				return false;
			}
		}
		OutColor = FLinearColor::FromSRGBColor(FColor::FromHex(Value));
		return true;
	}

	/** Reads the [Colors] section. Returns the valid entries, in file order. */
	TArray<TPair<FString, FLinearColor>> ReadColors(const FString& Path)
	{
		TArray<TPair<FString, FLinearColor>> Result;

		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *Path))
		{
			return Result;
		}

		TArray<FString> Lines;
		Contents.ParseIntoArrayLines(Lines);

		bool bInColors = false;
		int32 LineNumber = 0;
		for (const FString& RawLine : Lines)
		{
			++LineNumber;
			const FString Line = RawLine.TrimStartAndEnd();
			if (Line.IsEmpty() || Line.StartsWith(TEXT(";")) || Line.StartsWith(TEXT("#")))
			{
				continue;
			}

			if (Line.StartsWith(TEXT("[")) && Line.EndsWith(TEXT("]")))
			{
				bInColors = Line.Mid(1, Line.Len() - 2).TrimStartAndEnd().Equals(ColorsSection, ESearchCase::IgnoreCase);
				continue;
			}
			if (!bInColors)
			{
				continue;
			}

			FString Name, Value;
			if (!Line.Split(TEXT("="), &Name, &Value))
			{
				UE_LOG(LogPresetColorsIni, Warning, TEXT("%s(%d): expected Name=Color, ignoring '%s'."), *Path, LineNumber, *Line);
				continue;
			}
			Name.TrimStartAndEndInline();

			FLinearColor Color;
			if (Name.IsEmpty() || !ParseColor(Value, Color))
			{
				UE_LOG(LogPresetColorsIni, Warning, TEXT("%s(%d): invalid color '%s' for '%s', ignoring."), *Path, LineNumber, *Value, *Name);
				continue;
			}
			Result.Emplace(Name, Color);
		}
		return Result;
	}
}

void FPresetColorsIniModule::StartupModule()
{
#if !WITH_EDITOR
	// The game's color presets are filled in from the game state's defaults; patch each game state as it starts.
	AFGGameState* SampleGameState = GetMutableDefault<AFGGameState>();
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGGameState::BeginPlay, SampleGameState,
		[this](AFGGameState* GameState)
		{
			AddColorsTo(GameState);
		});
#endif
}

void FPresetColorsIniModule::ShutdownModule()
{
}

void FPresetColorsIniModule::AddColorsTo(AFGGameState* GameState)
{
	if (!GameState)
	{
		return;
	}

	ExportPlayerPresets(GameState);

	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / ConfigFileName);
	if (!FPaths::FileExists(Path))
	{
		WriteExampleFile(Path);
		return;
	}

	const TArray<TPair<FString, FLinearColor>> Colors = ReadColors(Path);
	if (Colors.IsEmpty())
	{
		return;
	}

	// mStaticGlobalColorPresets is private, so find it by name.
	const FArrayProperty* Property = CastField<FArrayProperty>(AFGGameState::StaticClass()->FindPropertyByName(TEXT("mStaticGlobalColorPresets")));
	if (!Property)
	{
		UE_LOG(LogPresetColorsIni, Error, TEXT("AFGGameState::mStaticGlobalColorPresets not found; the game may have changed."));
		return;
	}
	TArray<FGlobalColorPreset>& Presets = *Property->ContainerPtrToValuePtr<TArray<FGlobalColorPreset>>(GameState);

	// The presets saved in the save (private too). Only the host may change them; clients get the host's list.
	const FArrayProperty* PlayerProperty = CastField<FArrayProperty>(AFGGameState::StaticClass()->FindPropertyByName(TEXT("mPlayerGlobalColorPresets")));
	TArray<FGlobalColorPreset>* PlayerPresets = PlayerProperty ? PlayerProperty->ContainerPtrToValuePtr<TArray<FGlobalColorPreset>>(GameState) : nullptr;
	const bool bCanReorder = PlayerPresets && GameState->HasAuthority();

	int32 Added = 0;
	int32 Moved = 0;
	for (const TPair<FString, FLinearColor>& Entry : Colors)
	{
		// A color that is already in the save keeps its saved entry. It moves up to follow the file's order instead of being added again.
		if (PlayerPresets)
		{
			int32 Found = INDEX_NONE;
			for (int32 i = Moved; i < PlayerPresets->Num(); ++i)
			{
				if (SameColor((*PlayerPresets)[i].Color, Entry.Value))
				{
					Found = i;
					break;
				}
			}
			if (Found != INDEX_NONE)
			{
				if (bCanReorder && Found != Moved)
				{
					// Move it up without disturbing the order of the others.
					const FGlobalColorPreset Entry_ = (*PlayerPresets)[Found];
					PlayerPresets->RemoveAt(Found);
					PlayerPresets->Insert(Entry_, Moved);
				}
				++Moved;
				continue;
			}
		}

		// BeginPlay can run more than once for a game state object; don't add the same entry twice.
		const bool bExists = Presets.ContainsByPredicate([&Entry](const FGlobalColorPreset& Preset)
		{
			return Preset.PresetName.ToString() == Entry.Key && SameColor(Preset.Color, Entry.Value);
		});
		if (bExists)
		{
			continue;
		}

		// Put ours at the top of the list, in file order.
		FGlobalColorPreset Preset;
		Preset.PresetName = FText::FromString(Entry.Key);
		Preset.Color = Entry.Value;
		Presets.Insert(Preset, Added);
		++Added;
	}
	UE_LOG(LogPresetColorsIni, Log, TEXT("Added %d custom color preset(s) from %s; %d color(s) already in the save %s."),
		Added, *Path, Moved, bCanReorder ? TEXT("moved to follow the file order") : TEXT("left as they are (not the host)"));
}

void FPresetColorsIniModule::ExportPlayerPresets(AFGGameState* GameState) const
{
	// mPlayerGlobalColorPresets is private too. These are the presets the player saved in the color picker, stored in the save.
	const FArrayProperty* Property = CastField<FArrayProperty>(AFGGameState::StaticClass()->FindPropertyByName(TEXT("mPlayerGlobalColorPresets")));
	if (!Property)
	{
		UE_LOG(LogPresetColorsIni, Error, TEXT("AFGGameState::mPlayerGlobalColorPresets not found; the game may have changed."));
		return;
	}
	const TArray<FGlobalColorPreset>& Presets = *Property->ContainerPtrToValuePtr<TArray<FGlobalColorPreset>>(GameState);
	if (Presets.IsEmpty())
	{
		return;
	}

	FString Out = TEXT("; Color presets saved in your current save, in PresetColorsIni.ini format.\n")
		TEXT("; Copy the lines you want into the [Colors] section of PresetColorsIni.ini.\n")
		TEXT("; Rewritten every time a world starts.\n\n[Colors]\n");
	for (const FGlobalColorPreset& Preset : Presets)
	{
		// '=' would break the Name=Color format; line breaks would split the entry.
		FString Name = Preset.PresetName.ToString().Replace(TEXT("="), TEXT("-")).Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" "));
		// The values exactly as the game stores them: linear floats, no conversion.
		const FLinearColor& C = Preset.Color;
		Out += FString::Printf(TEXT("%s=linear(%.6f,%.6f,%.6f,%.6f)\n"), *Name, C.R, C.G, C.B, C.A);
	}

	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / ExportFileName);
	if (FFileHelper::SaveStringToFile(Out, *Path))
	{
		UE_LOG(LogPresetColorsIni, Log, TEXT("Exported %d saved color preset(s) to %s."), Presets.Num(), *Path);
	}
	else
	{
		UE_LOG(LogPresetColorsIni, Warning, TEXT("Could not write %s."), *Path);
	}
}

void FPresetColorsIniModule::WriteExampleFile(const FString& Path) const
{
	if (FFileHelper::SaveStringToFile(FString(ExampleFile), *Path))
	{
		UE_LOG(LogPresetColorsIni, Log, TEXT("Wrote an example color file to %s."), *Path);
	}
	else
	{
		UE_LOG(LogPresetColorsIni, Warning, TEXT("Could not write an example color file to %s."), *Path);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPresetColorsIniModule, PresetColorsIni)
