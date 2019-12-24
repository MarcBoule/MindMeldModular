### 1.1.5 (in development)

- Added EQ Master, a 24-track stereo multiband EQ for MixMaster
- Fixed bug in MixMaster where when using inserts, and aux sends set to pre-fader, for mono tracks it would send only L out of the aux sends (it now sends the mono signal to both L and R aux send outputs)
- Changed direct outs so that when set to pre-fader or pre-inserts, mono tracks send only a signal in the L output of the direct out poly cable for those given tracks


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
