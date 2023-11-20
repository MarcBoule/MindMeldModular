### 2.4.0 (in development)

- Add one decimal of precision to dim gain sliders' readouts in Mixers and MasterChan (in menus)
- Fix code warnings using Cppcheck


### 2.2.3 (2023-04-11)

- Fix issue with Schmitt trigger


### 2.2.2 (2023-03-26)

- PatchMaster: Fix issue with smooth parameter API change


### 2.2.1 (2023-01-12)

- PatchMaster: Fixed tile color not copied when duplicating tiles
- PatchMaster: Added new button type (momentary with toggled light)
- PatchMaster: Added option to hide mapping indicators


### 2.2.0 (2022-12-03)

- ShapeMaster: Changed horizontal node tooltip and value entry from seconds to volts when in CV trig mode
- ShapeMaster: Removed channel color and name from channel paste
- Added new modules: PatchMaster, RouteMasters (4 variants) and MasterChannel
- Optimized knob indicator dots in panels SVGs (AuxSpanders, MixMasters, ShapeMaster, EqMaster, BassMasters)


### 2.1.1 (2022-10-04)

- ShapeMaster: Added vertical-only option in randomize menu
- MixMaster: Fix track label tabbing cursor bug
- ShapeMaster: Added channel option to force a 0V CV when stopped
- ShapeMaster: Added a very small grace period at the end of the shape for retriggering in T/G mode when retriggering is off


### 2.1.0 (2022-03-26)

- Fix menu bug with global aux returns in MixMaster


### 2.0.9 (2022-02-12)

- Make menus stay open and refresh checkmarks when ctrl/cmd-clicked
- Draw ShapeMaster and EqMaster displays in module browser
- Add channel options in ShapeMaster to output a trigger in the VCA output when the playhead crosses nodes
- Add warning overlay message in AuxSpanders when mismatched to a MixMaster of wrong size (8 vs 16 tracks)
- Add new routing module called M/S Melder to facilitate mid/side EQ'ing with VCV Mid/Side and EqMaster
- Add automatic panel detection in Meld (using its polyphonic output) and in Unmeld (using its polyphonic input)
- Fix crash in editable labels in MixMasters, AuxSpanders; same in uMeld for the ShapeMaster-PRO plugin
- Automatically move EQ settings in EqMaster when tracks are moved in the mixer to which it is linked
- Add track move to EqMaster (labels not moved, only EQ settings; only available when EQ is not linked to a mixer)
- Add per-track option for CV mode in Mixers and AuxSpanders
- Make all settings that have a per-track option write the current state to all individual per-track settings when per-track is selected


### 2.0.8 (2021-11-27)

- First v2 library candidate
- Many changes that are not listed here and that were not part of an official release, such as:
- MixMaster: Added gain adjust to groups
- ShapeMaster: Added cloaked mode
- ShapeMaster: Added keyboard hover entry for grid-x
- ShapeMaster: Added channel-reset-on-sustain option in channel menu
- ShapeMaster: Made channel copy/paste use clipboard for inter-module copy-pasting of channel


### 1.1.14 (2021-04-30)

- Made shape copy/paste use clipboard for inter-module copy-pasting of shape
- Never show triangle in unsynced length knob label
- Fix last char entry bug in channel name (menu)
- Improved the mouse behavior in the main display to fix a bug on Arch Linux
- Improved history (undo) management
- Added a channel option to make node tooltips show V/Oct frequency or Note instead of volts


### 1.1.13 (2021-04-20)

- Added the ShapeMaster module (free version)


### 1.1.12 (2020-11-15)

- Fixed DC-blocker init bug in MixMasters


### 1.1.11 (2020-10-30)

- Added options in MixMaster and AuxSpander to copy/paste the modules' states, to help when swapping 8-track versions for 16-track versions or vice-versa
- Added placeholder "Stereo width: N/A" in track menus when mono


### 1.1.10 (2020-09-17)

- Added option to MixMasters' master label menus to solo chain inputs (useful for RackWindows' Console MM)
- Tweaked scope buttons' behaviors in EQMaster


### 1.1.9 (2020-07-19)

- Added poly spreading of track 1's Pan and Vol cv input to other tracks in MixMasters
- Added stereo width control in groups in MixMasters
- Changed VU meters so that they work even when the lights are out (see Modular Fungi's "Light Out" module)
- Fixed bug with non-symmetrical log fades in MixMasters and AuxSpanders


### 1.1.8 (2020-07-01)

- Fixed initialization bug in AuxSpander filters
- Added option to MixMaster track menus to invert input signal
- Added filters on groups in MixMasters
- Added option for initializing individual group settings in MixMasters
- Added option for initializing individual aux channel settings in AuxSpanders
- Made track initialization in MixMaster also initialize related aux sends and aux mute in AuxSpanders
- Added many panel options to the Meld module to facilitate using the poly CV inputs in MixMasters and AuxSpanders
- Added option to MM main menu direct outs to not send tracks that are assigned to a group


### 1.1.7 (2020-06-07)

- Added new modules called BassMaster and BassMasterJr: dual band spatialisers for high spread and bass mono
- Added option in MixMasters to select Vol CV input response: Linear-VCA (new) or Fader-control (default)
- Allowed Stereo width in MixMasters to go to 200%
- Re-coded all filters in MixMasters


### 1.1.6 (2020-05-04)

- Implemented darkened VUs when muted/faded in MixMasters and AuxSpanders
- Added option for initializing individual track settings in MixMasters and EqMaster
- Made filters mappable (click filter lights when mapping)
- Added option in EQMaster menu to hide EQ curves when bypassed, for when EQ is to be used as a spectrum analyzer only
- Added option in MixMaster to make master fader scale aux sends (for chaining mixers with shared aux effects)


### 1.1.5 (2020-01-29)

- Added EqMaster/EqSpander, a 24-track stereo multiband EQ with expander, for MixMaster
- Added stereo width slider in track-label menus of MixMaster and in aux-label menus of AuxSpander (R cable must be connected)
- Added new poly mixing option in track-label menus of MixMaster (support for track-localized poly stereo)
- Fixed bug in MixMaster where when using inserts, and aux sends set to pre-fader, for mono tracks it would send only L out of the aux sends (it now sends the mono signal to both L and R aux send outputs)
- Changed direct outs in MixMaster so that when set to pre-fader or pre-inserts, mono tracks send only a signal in the L output of the direct out poly cable for those given tracks


### 1.1.4 (2019-11-20)

- Fixed a critical bug in the Meld module


### 1.1.3 (2019-11-17)

- Added two new modules: MixMasterJr and AuxSpanderJr (8-track versions)
- Added two new modules: Meld and Unmeld (useful for managing direct outs and inserts); each has three panels available in the right-click menu: 1-8, 9-16, Grp-Aux
- Added menu entries for pan cv input attenuation in tracks, groups and aux
- Implemented fade in aux returns in AuxSpander
- Added setting for buttons' CV inputs that are of type Gate high/low, with the original Toggle trigger as the default setting
- Added tabbing to move across track labels when editing; same for groups and aux labels
- Added a ±20V clamping on all track, insert and aux inputs to catch invalid samples produced by other modules (-inf, inf, nan) 
- Changed "Mute track sends when group is muted" (in "Aux sends" menu, when AuxSpander is attached) to "Groups control track send levels" such that group fader, pan, mute, fade etc. control the track sends also


### 1.1.2 (2019-10-27)

- Initial release with MixMaster and AuxSpander
