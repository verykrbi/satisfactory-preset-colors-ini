// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class AFGGameState;

class FPresetColorsIniModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Adds the colors from PresetColorsIni.ini to the game state's static color presets. Safe to call more than once. */
	void AddColorsTo(AFGGameState* GameState);

	/** Writes the color presets saved in the current save to PresetColorsIni.Exported.ini. */
	void ExportPlayerPresets(AFGGameState* GameState) const;

	/** Writes a commented example file if none exists yet. */
	void WriteExampleFile(const FString& Path) const;
};
