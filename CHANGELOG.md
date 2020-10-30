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
