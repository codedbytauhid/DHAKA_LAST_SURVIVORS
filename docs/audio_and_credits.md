# Audio, effects and credits (Milestone 7)

## Where the sounds come from
**Every sound in the game is synthesised by our own code** (`src/audio/SoundBank.cpp`) the moment the game starts:
noise, sine and saw waves, filters, envelopes and a little reverb. No audio file was copied from anywhere, so there
is nothing to download and **no third-party licence or attribution is needed**. Music (menu, mission completed),
sirens, wind, voices, footsteps, gunshots and interface sounds are all original, generated material.

## Using your own recordings (optional)
If you have royalty-free recordings you prefer, put WAV files into `assets/audio/`. A file replaces the built-in sound
with the same name. Any WAV works (8/16/24/32-bit, mono or stereo, any sample rate; it is converted to 44.1 kHz mono).

* One recording: `assets/audio/pistol_shot.wav`
* Several variations (chosen at random): `zombie_groan_1.wav`, `zombie_groan_2.wav`, ... up to `_8`

Names: step_walk_indoor, step_walk_outdoor, step_run_indoor, step_run_outdoor, jump, land, pistol_shot, pistol_reload,
pistol_empty, bullet_impact, player_hurt, player_death, zombie_groan, zombie_alert, zombie_step, zombie_attack,
zombie_hit, zombie_death, amb_indoor, amb_wind, amb_siren (loops), electric_zap, gate_open, booth_beep, booth_denied,
emergency_broadcast, ui_select, ui_confirm, objective_complete, keycard, ammo_pickup, gate_unlocked, mission_failed,
menu_music (loop), mission_completed.

To get all built-in sounds as WAV files to listen to or edit, create the folder `assets/audio` and run
`build\main.exe --export-audio` once. The game prints which files it is using when it starts.
If you add recordings that need a licence (for example from freesound.org or pixabay.com), write their name,
author, link and licence here:

| File | Author | Source link | Licence |
|------|--------|-------------|---------|
| (none: all sounds are generated) | | | |

## Settings
Saved automatically to `settings.cfg` next to the game (master, music, sfx, ambience, sensitivity).
Delete the file to restore the defaults.
