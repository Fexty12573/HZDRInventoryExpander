# HzdrInventoryExpander

This is a mod for Horizon Zero Dawn **Remastered** that allows expanding the number of slots in the player's inventory, as well as by how much each bag upgrade increases the capacity.

## Installation
1. Download the latest release from the [Releases](https://github.com/Fexty12573/HZDRInventoryExpander/releases) page or from [Nexus Mods](https://www.nexusmods.com/horizonzerodawnremastered/mods/20/).
2. Extract the contents of the downloaded archive to the game's directory (where `HorizonZeroDawnRemastered.exe` is located).

## How to edit
Open the `InventoryUpgrades.json` and make your desired changes there.
The numbers in the "Base" category are the base capacity of each bag before you get any upgrades.

After that you can modify how many additional slots each subsequent bag upgrade gives by editing the "Increase" property in each upgrade. **Do NOT change the "GUID" property!**

For example, say I want to make the final weapon bag upgrade increase the bags capacity by 75 instead of 30, I would take the appropriate upgrade:
```json
{
    "Name": "Weapon Satchel Upgrade 4",
    "Increase": 30,
    "GUID": "{2D512E8B-8CB3-774F-8710-E69126CDDE2E}"
}
```
And change the "Increase" property:
```json
"Increase": 75,
```