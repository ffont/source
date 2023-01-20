export default class StateSynchronizer {  // Class to synchronize with plugin's state and interact with it

    constructor(wsPort, useWss) {
        this.state = undefined;
        this.volatileState = undefined;
        this.volatileStateRequestTimer = undefined;
        this.volatileStateRequestTimerInterval = 100;
        this.parser = new DOMParser();

        this.wsPort = wsPort;
        this.useWss = useWss;
        this.wsConnection = this.configureWSConnection();

        this.afterStateUpdatesFullStateRequestTimer = undefined;
        this.afterStateUpdatesFullStateRequestTimerInterval = 1000;
    }

    scheduleExtraFullStateRequestAfterBurstOfMessages() {
        // Because sometimes there can be state syncing issues if messages arrive in wrong orders or due to
        // other whings we did not contemplate, once a burst of state update messages has finished, we call request
        // once more a full state update to make sure everything is perfectly in sync.
        if (this.afterStateUpdatesFullStateRequestTimer !== undefined) {
            clearTimeout(this.afterStateUpdatesFullStateRequestTimer);
            this.afterStateUpdatesFullStateRequestTimer = undefined;
        }
        this.afterStateUpdatesFullStateRequestTimer = setTimeout(() => {
            this.requestFullState();
        }, this.afterStateUpdatesFullStateRequestTimerInterval);
    }

    configureWSConnection() {
        let wsUrl = "";
        if (this.useWss) {
            wsUrl = "wss://" + hostname + ":" + this.wsPort + "/source_coms/";
        } else {
            wsUrl = "ws://" + hostname + ":" + this.wsPort + "/source_coms/";
        }
        console.log("Connecting to WS server: " + wsUrl);
        let socket = new WebSocket(wsUrl);

        let self = this;

        socket.onopen = function (e) {
            console.log("[WS open] Connection established");

            // Once connection established, request full state once and start polling volatile state
            self.requestFullState();
            self.volatileStateRequestTimer = setInterval(function () {
                self.requestVolatileState();
            }, self.volatileStateRequestTimerInterval);

            //Clear all sounds from previous session
            ss.clearAllSounds();
        };

        socket.onclose = function (event) {
            if (event.wasClean) {
                console.log(`[WS close] Connection closed cleanly, code=${event.code} reason=${event.reason}`);
            } else {
                console.log('[WS close] Connection died');
            }
            setTimeout(() => {
                self.configureWSConnection(); // Start again connection if it fails, wait 2 seconds
            }, 2000);

        };

        socket.onerror = function (error) {
            console.log(`[WS error] ${error.message}`);
        };

        socket.onmessage = function (event) {
            const msgTypeSeparatorPosition = event.data.indexOf(':');
            const msgType = event.data.slice(0, msgTypeSeparatorPosition);
            const rawData = event.data.slice(msgTypeSeparatorPosition + 1, event.data.length);


            //PROBLEM HERE IS THAT FULL STATE MSG IS COMING BUT VOLATILE IS MORE? HOW TO SOLVE THIS?
            if (msgType === "/full_state") {
                const updateIdSeparatorPosition = rawData.indexOf(';');
                const updateId = parseInt(rawData.slice(0, updateIdSeparatorPosition), 10);
                const dataToParse = rawData.slice(updateIdSeparatorPosition + 1, rawData.length);
                self.state = self.parser.parseFromString(dataToParse, "text/xml");

            } else if (msgType === "/volatile_state") {
                self.volatileState = self.parser.parseFromString(rawData, "text/xml");
                updateVolatileUI();

            } else if (msgType === "/state_update") {

                if (ss.state === undefined) {
                    // If no state is present, ignore stat update messages as these will fail
                    // also request full state to get in sync again
                    self.requestFullState();
                    return;
                }

                const rawDataParts = rawData.split(';');
                const updateType = rawDataParts[0];
                const updateId = parseInt(rawDataParts[1], 10);
                const restOfDataSliced = rawDataParts.slice(2, rawDataParts.length);

                if (updateType === 'propertyChanged') {
                    const treeUUID = restOfDataSliced[0];
                    const treeType = restOfDataSliced[1];
                    const propertyName = restOfDataSliced[2];
                    const propertyValue = restOfDataSliced[3];

                    const elementToUpdate = self.state.querySelector("[uuid='" + treeUUID + "']");
                    if (elementToUpdate !== null) {
                        elementToUpdate.setAttribute(propertyName, propertyValue);


                        // If property was found, trigger corresponding UI updates
                        if (treeType === "SOUND") {
                            // If what changed is a property of a sound or sound sample, then update the sound list
                            var sound = ss.getSoundFromSoundUUID(treeUUID);
                            //If sound exists and not in the deleted mode,
                            if ((sound !== null) && (propertyName !== "willBeDeleted" && propertyName === "downloadCompleted")) {
                                console.log(propertyName);
                                updateSoundList(sound.getAttribute('uuid'));
                            } else {

                            }

                            // If the property name is that of a slider (sound parameter), then update the slider as well
                            if (isSoundParameter(propertyName)) {
                                // NOTE: in that case, treeUUID will always correspond to a SOUND element
                                updateSoundParameter(treeUUID, propertyName, propertyValue, elementToUpdate);
                            }
                        } else if (treeType === "SOUND_SAMPLE") {
                            var sound = ss.getSoundFromSoundSampleUUID(treeUUID);

                            //If the property belongs to querying, add the sounds to the sound list
                            if ((sound !== null) && (propertyName !== "willBeDeleted" && propertyName === "downloadCompleted")) {
                                console.log('sound_sample', propertyName);
                                updateSoundList(sound.getAttribute('uuid'));
                            } else {
                                // We might receive property change messages from sounds that are being deleted and that still exist in our state
                                // We can ignore these messages, but lets print a warning if a message refers to some unexpected property
                                if (propertyName !== "willBeDeleted") {
                                    console.log('WARNING: property change for unexisting sound different from "willBeDeleted": ', propertyName, propertyValue);
                                }
                            }

                        } else if ((treeType === "SOURCE_STATE") || (treeType === "PRESET")) {
                            // If one of the main state or preset state properties changed, update main preset parameters as it will be contained in one of these
                        } else if (treeType === "MIDI_CC_MAPPING") {
                        }
                    }

                } else if (updateType === 'addedChild') {
                    const parentTreeUUID = restOfDataSliced[0];
                    const parentTreeType = restOfDataSliced[1];
                    const indexInParentChildren = parseInt(restOfDataSliced[2], 10);
                    const childXmlString = restOfDataSliced.slice(3, restOfDataSliced.length).join(';');
                    // Add new child element to the state
                    var elementToUpdate;
                    if (parentTreeType === "PRESET") {
                        // If adding child of type preset, add it to the main SOURCE_STATE node regardless of the SOURCE_STATE uuid which some times changes
                        elementToUpdate = ss.state.childNodes[0];
                    } else {
                        elementToUpdate = self.state.querySelector("[uuid='" + parentTreeUUID + "']");
                    }
                    if (elementToUpdate !== null) {
                        var nodeToAdd = self.parser.parseFromString(childXmlString, "text/xml").childNodes[0];
                        if (indexInParentChildren < 0) {
                            elementToUpdate.appendChild(nodeToAdd);
                        } else {
                            var beforeNode = elementToUpdate.childNodes[indexInParentChildren];
                            elementToUpdate.insertBefore(nodeToAdd, beforeNode);
                        }

                        // If new sounds or midi cc mappings are added, recreate sounds section
                        if ((nodeToAdd.tagName === "MIDI_CC_MAPPING") || (nodeToAdd.tagName === "SOUND")) {
                            updateSoundList();
                        }
                    }

                } else if (updateType === 'removedChild') {
                    const childToRemoveUUID = restOfDataSliced[0];
                    const childToRemoveType = restOfDataSliced[1];

                    // Remove the selected element from the state
                    const stateElementToRemove = self.state.querySelector("[uuid='" + childToRemoveUUID + "']");
                    if (stateElementToRemove !== null) {
                        stateElementToRemove.remove();

                        // Trigger corresponding UI updates
                        const domElementToUpdate = document.getElementById(childToRemoveUUID);
                        if (domElementToUpdate !== null) {
                            domElementToUpdate.remove();
                        }


                    }
                }

                self.scheduleExtraFullStateRequestAfterBurstOfMessages();
            }
        };

        return socket;

    }

    sendMessageToPlugin(address, args) {
        const message = address + ':' + args.join(';');
        this.wsConnection.send(message);
    }

    requestFullState() {
        this.sendMessageToPlugin('/get_state', ["full"]);
    }

    requestVolatileState() {
        this.sendMessageToPlugin('/get_state', ["volatile"]);
    }

    // Util methods to get state info

    getSounds() {
        return ss.state.getElementsByTagName('SOUND');
    }

    getSoundSamplerSounds(sound) {  // sound = sound xml state
        if (sound !== null) {
            return sound.getElementsByTagName('SOUND_SAMPLE');
        } else {
            return [];
        }

    }

    getSoundDuration(selectedSoundUUID) {
        var sound = ss.getSoundFromSoundUUID(selectedSoundUUID);
        var samplerSounds = ss.getSoundSamplerSounds(sound);
        return samplerSounds[0].getAttribute('duration')
    }

    getFirstSoundSampleSound(sound) {  // sound = sound xml state
        return this.getSoundSamplerSounds(sound)[0];
    }

    getSoundFromSoundUUID(soundUUID) {
        return ss.state.querySelector("[uuid='" + soundUUID + "']");
    }

    getSoundFromSoundSampleUUID(soundSampleUUID) {
        const soundSample = ss.state.querySelector("[uuid='" + soundSampleUUID + "']");
        return ss.getSoundFromSoundUUID(soundSample.parentElement.getAttribute('uuid'));
    }

    getMidiMappingStateFromMappingUUID(mappingUUID) {
        return ss.state.querySelector("[uuid='" + mappingUUID + "']");
    }

    getSoundMidiMappings(sound) {  // sound = sound xml state
        return sound.getElementsByTagName('MIDI_CC_MAPPING');
    }

    findSoundIdxFromSoundUUID(soundUUID) {
        var sounds = ss.getSounds();
        for (var i = 0; i < sounds.length; i++) {
            if (sounds[i].getAttribute("uuid") == soundUUID) {
                return i + 1;
            }
        }
        return -1
    }

    findSoundIdxFromSourceSamplerSoundUUID(samplerSoundUUID) {
        var sounds = ss.getSounds();
        for (var i = 0; i < sounds.length; i++) {
            var samplerSounds = this.getSoundSamplerSounds(sounds[i]);
            for (var j = 0; j < samplerSounds.length; j++) {
                var samplerSound = samplerSounds[j];
                if (samplerSound.getAttribute("uuid") == samplerSoundUUID) {
                    return i + 1;
                }
            }
        }
        return -1
    }

    getTmpFilesLocation() {
        return this.state.getElementsByTagName('SOURCE_STATE')[0].getAttribute('tmpFilesLocation');
    }

    // Util methods to change things of the state

    setSoundParameter(parameterName, parameterVal, parameterMin, parameterMax, resetMode = undefined) {
        if (resetMode !== undefined) {
            this.sendMessageToPlugin('/set_sound_parameter', [resetMode, parameterName, getProcessedSoundParameterValuLinToExp(parameterName, parameterVal, parameterMin, parameterMax)])

        } else {
            this.sendMessageToPlugin('/set_sound_parameter', [selectedSoundUUID, parameterName, getProcessedSoundParameterValuLinToExp(parameterName, parameterVal, parameterMin, parameterMax)])
        }
    }

    setSoundParameterInt(elementName, val) {
        saveCurrentUserActionTime();
        this.sendMessageToPlugin('/set_sound_parameter_int', [selectedSoundUUID, elementName, val]);
    }

    getSoundParameterValue(soundUUID, soundParameterName) {
        let sound = this.getSoundFromSoundUUID(soundUUID);
        let soundParameterVal = sound.getAttribute(soundParameterName);
        return soundParameterVal;
    }

    addSoundsByQuery() {
        var VolatileState = ss.volatileState.getElementsByTagName('VOLATILE_STATE')[0];
        var isQuerying = VolatileState.getAttribute('isQuerying');
        //Block querying if a query is still going on
        if (isQuerying == 0) {
            var query = document.getElementsByName('query')[0].value;
            var numSounds = $('.single-slider')[0].noUiSlider.get()[0];
            var minSoundLength = $('.double-slider')[0].noUiSlider.get()[0];
            var maxSoundLength = $('.double-slider')[0].noUiSlider.get()[1];
            var noteLayoutType = $('#noteLayout p').text();
            this.sendMessageToPlugin('/add_sounds_from_query', [query, numSounds, minSoundLength, maxSoundLength, noteLayoutType])
            $('#noteLayout p').text('contiguous');
        }

    }

    reapplyLayout(noteMappingType) {
        this.sendMessageToPlugin('/reapply_layout', [noteMappingType]);
    }

    clearAllSounds() {
        this.sendMessageToPlugin('/clear_all_sounds', [])
    }

    setMidiInChannel(midiInChannel) {
        this.sendMessageToPlugin('/set_midi_in_channel', [midiInChannel])
    }

    setReverbParameters() {
        var reverbParameterList = Object.keys(reverbParams).map(function (key) {
            return reverbParams[key];
        })
        this.sendMessageToPlugin('/set_reverb_parameters', reverbParameterList)
    }

    setNumVoices() {
        var numVoices = document.getElementsByName('numVoices')[0].value;
        this.sendMessageToPlugin('/set_polyphony', [numVoices])
    }

    playSound() {
        //Add sound to the list to stop if a new sound is selected
        playStopSoundList.push(selectedSoundUUID);


        if (playStopSoundList.length > 1) {
            //Stop the previous sound and remove it
            ss.stopSound(playStopSoundList[0]);
            playStopSoundList.shift();
        }
        var sound = ss.getSoundFromSoundUUID(selectedSoundUUID);
        var samplerSounds = ss.getSoundSamplerSounds(sound);

        this.sendMessageToPlugin('/play_sound', [selectedSoundUUID])

    }

    stopSound(soundUUID) {

        //Kind of a hack for stopping sound from the button below sound list
        if (soundUUID === 'stop') {
            var soundUUID = selectedSoundUUID;
        }
        this.sendMessageToPlugin('/stop_sound', [soundUUID])
    }

    removeSound(soundUUID) {
        this.sendMessageToPlugin('/remove_sound', [soundUUID])
    }

    savePreset() {
        var name = document.getElementsByName('presetName')[0].value;
        var idx = document.getElementsByName('presetIdx')[0].value;
        this.sendMessageToPlugin('/save_preset', [name, idx])
    }

    loadPreset() {
        var idx = document.getElementsByName('presetIdx')[0].value;
        this.sendMessageToPlugin('/load_preset', [idx])
    }

    nextPreset() {
        var idx = document.getElementsByName('presetIdx')[0].value;
        if (idx == undefined) {
            idx = -1;
        } else {
            idx = parseInt(idx, 10);
            if (isNaN(idx)) {
                idx = -1;
            }
        }
        this.sendMessageToPlugin('/load_preset', [idx + 1])
    }

    sendMidiNoteOn(rootNote) {
        this.sendMessageToPlugin('/note_on', [rootNote, 120, 0]);
    }

    sendMidiNoteOff(rootNote) {
        this.sendMessageToPlugin('/note_off', [rootNote, 120, 0]);
    }


    previousPreset() {
        var idx = document.getElementsByName('presetIdx')[0].value;
        if (idx == undefined) {
            idx = 1;
        } else {
            idx = parseInt(idx, 10);

            if (isNaN(idx)) {
                idx = 1;
            }
        }
        if (idx < 1) {
            idx = 1; //Will become 0 when subtracted 1
        }
        this.sendMessageToPlugin('/load_preset', [idx - 1])
    }

    rootNote() {
        var sound = ss.getSoundFromSoundUUID(selectedSoundUUID);
        var samplerSounds = ss.getSoundSamplerSounds(sound);
        var samplerSound = samplerSounds[0];
        this.sendMessageToPlugin('/set_sampler_sound_root_note', [selectedSoundUUID, samplerSound.getAttribute('uuid'), 60]);

    }


    createUpdateMapping(soundUUID, mappingID) {
        var ccNumber = document.getElementById(mappingID + '_mappingCCNumber').value;
        var name = document.getElementById(mappingID + '_mappingParameterName').value;
        var minRange = parseFloat(document.getElementById(mappingID + '_mappingMinRange').value, 10);
        var maxRange = parseFloat(document.getElementById(mappingID + '_mappingMaxRange').value, 10);
        this.sendMessageToPlugin('/add_or_update_cc_mapping', [soundUUID, mappingID, ccNumber, name, minRange, maxRange]);
        if (mappingID == "") {
            // Remove this element from DOM because this one is only while creating a new mappping
            document.getElementById('newMappingPlaceholder_' + soundUUID).innerHTML = "";
        }
    }

    removeMidiMapping(soundUUID, mappingID) {
        if (mappingID != "") {
            this.sendMessageToPlugin('/remove_cc_mapping', [soundUUID, mappingID]);
            // No need to remove the element itself because it will be removed when updating state
        } else {
            document.getElementById('newMappingPlaceholder_' + soundUUID).innerHTML = "";  // Remove new mapping palceholder as the mapping was not yet created
        }
    }

}
