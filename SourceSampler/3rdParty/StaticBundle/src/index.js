const $ = require('jquery');
Window.prototype.$ = $;
const WaveSurfer = require('wavesurfer');
Window.prototype.WaveSurfer = WaveSurfer;
const RegionsPlugin = require('wavesurfer.js/dist/plugin/wavesurfer.regions.min.js');
Window.prototype.RegionsPlugin = RegionsPlugin;
const CursorPlugin = require('wavesurfer.js/dist/plugin/wavesurfer.cursor.min.js');
Window.prototype.CursorPlugin = CursorPlugin;
const ADSREnvelope = require('adsr-envelope');
Window.prototype.ADSREnvelope  = ADSREnvelope;
require('jquery-knob');
const noUiSlider = require('nouislider');
Window.prototype.noUiSlider = noUiSlider;
const wNumb = require('wnumb');
Window.prototype.wNumb = wNumb;
import styles from './style.css'
import StateSynchronizer from './core';
export {StateSynchronizer};