```
_______  ______ _     _                                                   
|_____| |_____/ |____/                                                    
|     | |    \_ |    \_                                                   
                                                                          
_______ _______ _______ _______ __   _ ______  _______ ______             
|_____| |______ |       |______ | \  | |     \ |______ |     \            
|     | ______| |_____  |______ |  \_| |_____/ |______ |_____/            
                                                                          
_______  _____  ______        ______ _______  _____  _______ _____  ______
|  |  | |     | |     \      |_____/ |______ |_____] |_____|   |   |_____/
|  |  | |_____| |_____/      |    \_ |______ |       |     | __|__ |    \_
                                                                          
 ______ _     _ _____ ______  _______                                     
|  ____ |     |   |   |     \ |______                                     
|_____| |_____| __|__ |_____/ |______  
```
## ASA Mod Doctor (tool)

A small Windows tool that shows the **real** state of every mod in your ARK: Survival Ascended mod library, and fixes the cases the game can't.

**Download:** [latest release](https://github.com/bushome/arkascendedpcsinglemodrepairguide/releases/tag/Tool). Close ARK before making any changes.

**Technical documentation:** [validation findings prepared for Wildcard review](docs/ASA-Mod-Doctor-Technical-Findings.md) and [C++ startup-validation proposal](docs/Startup-Validation-Proposal.md). The report distinguishes reproduced client behavior from hypotheses and describes the newer companion test candidate.

What it does:
- **Finds broken mods the game shows as "installed":** interrupted downloads, mods stuck as Invalid, missing or damaged files, wrong versions.
- **Fixes Invalid without re-downloading:** if a mod's files check out, it resets the mod to Normal.
- **Removes any mod cleanly**, including cosmetics pulled in by dynamic downloading, which the game's mod browser doesn't show. One or many at a time. The folder goes to the Recycle Bin and a backup of your library is saved first.
- **Shows what's hiding on your disk:** every dynamic download, which ones are Premium, and how much space they take.
- **Checks file integrity** against the installed record's module fingerprints; see the technical report for tested behavior and limitations.
- **Keeps a history** of what the game changed in your library between launches.

Works with Steam and Game Pass / Microsoft Store installs (PC only).

## Cosmetics tip for 5.8

- **Dynamic downloading downloads every cosmetic you see near other players' bases**, including Premium ones you don't own.
- **Suppressing a cosmetic** (Settings → General → Cosmetics) only hides it. It still downloads.
- **To delete suppressed cosmetics**, go back to the **main menu** and press **"Uninstall suppressed cosmetics"**. The button doesn't appear during a session. It removes **every** suppressed cosmetic, including ones you own and installed yourself.
- **To stop cosmetics downloading at all**, turn off dynamic downloading.

## Fixing mods by hand

The original step-by-step guide is in the wiki:
https://github.com/bushome/arkascendedpcsinglemodrepairguide/wiki/Ark-Ascended-Surgical-Guide-to-fixing-singular-mods-on-PC
