const $ = require('jquery');
Window.prototype.$ = $;
const WaveSurfer = require('wavesurfer');
Window.prototype.WaveSurfer = WaveSurfer;
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