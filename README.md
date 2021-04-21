Modules for [VCV Rack](https://vcvrack.com), available in the [plugin library](https://vcvrack.com/plugins.html).

Mind Meld is a designer / developer collaboration for VCV Rack between Marc '_Spock_' Boulé (coding and development) and Steve '_Make it so_' Baker (concept and design). 

Version 1.1.13

[//]: # (!!!!!UPDATE VERSION NUMBER IN PLUGIN.JSON ALSO!!!!!   140% Zoom for pngs, all size of MixMaster.png, use transparency)

<a id="manuals"></a>
# Manuals

Here are the manuals for [MixMaster/AuxSpander](https://github.com/MarcBoule/MindMeldModular/blob/master/doc/MindMeld-MixMaster-Manual-V1_1_4.pdf) and 
[ShapeMaster](https://github.com/MarcBoule/MindMeldModular/blob/master/doc/MindMeld-ShapeMaster-Manual-V1_0.pdf)

<a id="modules"></a>
# Modules

The following modules are part of the MindMeld module pack:

* [MixMaster](#mixmaster): 16-track stereo mixer with 4 group busses.

* [AuxSpander](#auxspander): 4-aux FX bus expander for MixMaster.

* [MixMasterJr](#mixmasterjr): 8-track stereo mixer with 2 group busses.

* [AuxSpanderJr](#auxspanderjr): 4-aux FX bus expander for MixMasterJr.

* [EqMaster](#eqmaster): 24-track multiband EQ for MixMaster and MixMasterJr (user manual available soon).

* [EqSpander](#eqspander): CV expander for EqMaster.

* [Meld / Unmeld](#meldunmeld): Utility modules for MixMaster and MixMasterJr.

* [BassMaster](#bassmaster): Dual band spatialiser for high spread and bass mono.

* [ShapeMaster](#shapemaster): Multi-stage envelope generator (MSEG) / complex LFO.


<a id="mixmaster"></a>
## MixMaster

![IM](res/img/MixMaster.png)

The MixMaster beams the following features into your Rack for your demanding mixing needs:

* 16 mono/stereo tracks
* 4 aux busses (requires [AuxSpander](#auxspander)) and 4 group busses
* Editable track labels (scribble strips)
* Gain adjustment (trim) on every track (±20 dB)
* Hi Pass Filter (HPF) on every track and bus (18 dB/oct.)
* Low Pass Filter (LPF) on every track and bus (12 dB/oct.)
* Fade automation with lin/log/exp curves
* Track re-ordering and copy/paste settings
* User selectable pan law
* Stereo balance and true stereo panning
* Accurate RMS and Peak VUs with peak hold
* User selectable VU colour and display colour
* Long fader runs and fader linking
* Inserts on every track and bus
* Direct outs for every track and bus
* Chain input
* Flexible signal routing options
* Dim and fold to mono on the Master
* CV visualisation
* CV control over just about everything...

Many labels contain separate menus that are different from the module's main menu; these can be accessed by right-clicking the labels.


<a id="auxspander"></a>
## AuxSpander

![IM](res/img/Auxspander.png)

4-aux FX bus expander for MixMaster, must be placed immediately to the right of the MixMaster module.


<a id="mixmasterjr"></a>
## MixMasterJr

![IM](res/img/MixMasterJr.png)

A smaller 8-track version of MixMaster, with 2 group busses. Functionality and options are identical to those found in MixMaster.


<a id="auxspanderjr"></a>
## AuxSpanderJr

![IM](res/img/AuxspanderJr.png)

4-aux FX bus expander for MixMasterJr, must be placed immediately to the right of the MixMasterJr module. Functionality and options are identical to those found in AuxSpander.


<a id="eqmaster"></a>
## EqMaster

![IM](res/img/EqMaster.png)

24-track multiband EQ for MixMaster and MixMasterJr. Can be placed anywhere in the patch, uses polyphonic cables for connecting to the inserts on the MixMaster mixers. Track names and colors can be linked to any MixMaster in the patch (right-click menu in EqMaster). Allows separate EQ control of each of the 24 stereo tracks. A [CV expander](#eqspander) is also available for added control over the module's parameters.


<a id="eqspander"></a>
## EqSpander

![IM](res/img/EqSpander.png)

Expander module to add CV control over most parameters in the EqMaster. All jacks on the module are polyphonic CVs, and it can be placed on either side of the EqMaster. The state jacks at the top control the active/bypass status of the 24 tracks, with the global bypass located in channel 9 of the G/A jack; the other 24 jacks (one for each track) control the 4 parameters (state, freq, gain, Q) for each of the 4 bands in each track.


<a id="meldunmeld"></a>
## Meld / Unmeld

![IM](res/img/MeldUnmeld.png)

Two utility modules to help manage direct out and insert connections. Three panel options are available in each module: 1-8, 9-16, Grp/Aux. The 8 LEDs in the right side of the Meld module can be clicked and are used to bypass an insert (red), or not (green). The Meld module also features a set of panels for merging the CV poly-inputs found on the MixMaster and AuxSpander modules.


<a id="bassmaster"></a>
## BassMaster

![IM](res/img/BassMaster.png)

Dual band stereo width controllers for bass mono and high spread. Both modules have the same core functionality; however the Jr version omits the mix and master gain controls, the VU meter and the width CV inputs.


<a id="shapemaster"></a>
## ShapeMaster

![IM](res/img/ShapeMaster.png)

Multi-stage envelope generator (MSEG) / complex LFO:

* 8 Polyphonic channels
* Built in VCA and CV out on every channel
* Draw complex shapes with up to 270 nodes
* Hundreds of built in Shapes and Presets
* Powerful random shape generation
* Freeze, Sustain and Loop envelopes
* Flexible Trigger modes
* Play forward, reverse or pingpong
* Repeat count from 1 to 99 or Infinite (LFO)
* All controls independently set per channel
* Integrated dual band crossover
* Unsynced lengths from 0.56ms to 30 minutes
* Built-in scope shows pre and post envelope
* Gain adjustment (Trim) on every track (± 20dB)
* Editable Channel Labels
* Trigger generation from sidechain audio

The ShapeMaster Pro (not shown) is a commercial plugin which adds the following features 

* Sync and quantise channels to clock
* Synced lengths from 1/128 to 128 bars
* Includes triplet and dotted lengths
* CV Visualisation
* SM-CV expander module for CV control
* SM-Triggers expander module
* uMeld - 8 channel poly merge module

The ShapeMaster Pro plugin can be purchased in the [VCV Rack plugin library](https://library.vcvrack.com/MindMeld-ShapeMasterPro).


<a id="videos"></a>
# Videos

### Promo
* Eurikon (Latif Karoumi), [MindMeld Modular - Mixmaster for VCV Rack](https://www.youtube.com/watch?v=8g_BwxgEuSw)
* Eurikon (Latif Karoumi), [MindMeld Modular - Mixmaster for VCV Rack : Go On (Eurikon Remix) - ShAi Dawn](https://www.youtube.com/watch?v=U_Wx2Jxx6Yg)
* Eurikon (Latif Karoumi), [ShapeMaster by MindMeld for VCV Rack](https://www.youtube.com/watch?v=O-diK-PWzEs)

### Tutorials
* Omri Cohen, [MixMaster from MindMeld](https://www.youtube.com/watch?v=YcTPaG6N6nI)
* Omri Cohen, [The Many Things You Can Do With ShapeMaster Pro from MindMeld](https://www.youtube.com/watch?v=GL6e4Mqqp2Y)
* Artem Leonov, [Techno Patch from Scratch with new MindMelder MixMaster mixer in VCV Rack](https://www.youtube.com/watch?v=WsjscQvwBVk)
* Artem Leonov, [Mixing mini patches in VCV RACK with new MINDMELD EQMASTER module](https://www.youtube.com/watch?v=wW1UREZQQXU)

### In the wild
* Wouter Spekkink, [Street lights](https://www.youtube.com/watch?v=QpDp3RGGcBg)
* Alasdair Moons, [Hardbeatz - VCV Rack Voyage](https://www.youtube.com/watch?v=N7RGjp2ydIk)
* Richard Squires, [Squelch](https://www.youtube.com/watch?v=Som0uU9kzxw)
* Nick Dutton, [I Feel Love By Donna Summer (Cover)](https://www.youtube.com/watch?v=skfb8ZFm0yA)
