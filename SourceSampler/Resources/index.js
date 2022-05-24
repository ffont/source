        let ss = undefined;  // Will keep source plugin state and has util methods to interact with it
        let httpPort = undefined;
        let useHttps = undefined;
        let hostname = undefined;
        
        var expStrength = 2;  // For sliders

        window.AudioContext = window.AudioContext || window.webkitAudioContext; // Audio context used for loading audio for drawing waveforms
        const audioContext = new AudioContext();

        let lastUIAction = 0;
    

        class StateSynchronizer {  // Class to synchronize with plugin's state and interact with it

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

            scheduleExtraFullStateRequestAfterBurstOfMessages () {
                // Because sometimes there can be state syncing issues if messages arrive in wrong orders or due to
                // other whings we did not contemplate, once a burst of state update messages has finished, we call request
                // once more a full state update to make sure everything is perfectly in sync.
                if (this.afterStateUpdatesFullStateRequestTimer !== undefined){
                    clearTimeout(this.afterStateUpdatesFullStateRequestTimer);
                    this.afterStateUpdatesFullStateRequestTimer = undefined;
                }
                this.afterStateUpdatesFullStateRequestTimer = setTimeout(() => {
                    this.requestFullState();
                }, this.afterStateUpdatesFullStateRequestTimerInterval);
            }

            configureWSConnection () {
                let wsUrl = "";
                if (this.useWss){
                    wsUrl = "wss://" + hostname + ":" + this.wsPort + "/source_coms/";
                } else {
                    wsUrl = "ws://" + hostname + ":" + this.wsPort + "/source_coms/";
                }
                console.log("Connecting to WS server: " + wsUrl);
                let socket = new WebSocket(wsUrl);
                
                let self = this;

                socket.onopen = function(e) {
                    console.log("[WS open] Connection established");
                    
                    // Once connection established, request full state once and start polling volatile state
                    self.requestFullState();
                    self.volatileStateRequestTimer = setInterval(function () {
                        self.requestVolatileState();
                    }, self.volatileStateRequestTimerInterval);
                };

                socket.onclose = function(event) {
                    if (event.wasClean) {
                        console.log(`[WS close] Connection closed cleanly, code=${event.code} reason=${event.reason}`);
                    } else {
                        console.log('[WS close] Connection died');
                    }
                    setTimeout(() => {
                        self.configureWSConnection (); // Start again connection if it fails, wait 2 seconds
                    }, 2000);
                    
                };

                socket.onerror = function(error) {
                    console.log(`[WS error] ${error.message}`);
                };

                socket.onmessage = function(event) {
                    const msgTypeSeparatorPosition = event.data.indexOf(':');
                    const msgType = event.data.slice(0, msgTypeSeparatorPosition);
                    const rawData = event.data.slice(msgTypeSeparatorPosition + 1, event.data.length);
                    

                    //PROBLEM HERE IS THAT FULL STATE MSG IS COMING BUT VOLATILE IS MORE? HOW TO SOLVE THIS?
                    if (msgType === "/full_state"){
                        const updateIdSeparatorPosition = rawData.indexOf(';');
                        const updateId = parseInt(rawData.slice(0, updateIdSeparatorPosition), 10);
                        const dataToParse = rawData.slice(updateIdSeparatorPosition + 1, rawData.length);
                        self.state = self.parser.parseFromString(dataToParse, "text/xml");
                        updateFullUI();

                    } else if (msgType === "/volatile_state"){
                        self.volatileState = self.parser.parseFromString(rawData, "text/xml");
                        updateVolatileUI();

                    } else if (msgType === "/state_update"){

                        if (ss.state === undefined){
                            // If no state is present, ignore stat update messages as these will fail
                            // also request full state to get in sync again
                            self.requestFullState();
                            return;                            
                        }

                        const rawDataParts = rawData.split(';');
                        const updateType = rawDataParts[0];
                        const updateId = parseInt(rawDataParts[1], 10);
                        const restOfDataSliced = rawDataParts.slice(2, rawDataParts.length);

                        if (updateType === 'propertyChanged'){
                            const treeUUID = restOfDataSliced[0];
                            const treeType = restOfDataSliced[1];
                            const propertyName = restOfDataSliced[2];
                            const propertyValue = restOfDataSliced[3];

                            const elementToUpdate = self.state.querySelector("[uuid='" + treeUUID + "']");
                            if (elementToUpdate !== null){
                                elementToUpdate.setAttribute(propertyName, propertyValue);

                                // Check if the change was to notify a query with empty results, and show in ui
                                if (propertyName === 'numResultsLastQuery'){
                                    notifyQueryResultsFromLastQuery(parseInt(propertyValue, 0));
                                }

                                // If property was found, trigger corresponding UI updates
                                if (treeType === "SOUND"){
                                    // If what changed is a property of a sound or sound sample, then update the card for the sound
                                    var sound = ss.getSoundFromSoundUUID(treeUUID);
                                    if (sound !== null){
                                        triggerRateLimitedUpdateSoundCard(sound.getAttribute('uuid'));
                                    } else {
                                        // We might receive property change messages from sounds that are being deleted and that still exist in our state
                                        // We can ignore these messages, but lets print a warning if a message refers to some unexpected property
                                        if (propertyName !== "willBeDeleted"){
                                            console.log('WARNING: property change for unexisting sound different from "willBeDeleted": ', propertyName, propertyValue);
                                        }
                                    }

                                    // If the property name is that of a slider (sound parameter), then update the slider as well
                                    if (isSoundParameter(propertyName)){
                                        // NOTE: in that case, treeUUID will always correspond to a SOUND element
                                        updateSoundParameter(treeUUID, propertyName, propertyValue, elementToUpdate);
                                    }
                                } else if (treeType === "SOUND_SAMPLE"){
                                    var sound = ss.getSoundFromSoundSampleUUID(treeUUID);
                                    if (sound !== null){
                                        triggerRateLimitedUpdateSoundCard(sound.getAttribute('uuid'));
                                    } else {
                                        // We might receive property change messages from sounds that are being deleted and that still exist in our state
                                        // We can ignore these messages, but lets print a warning if a message refers to some unexpected property
                                        if (propertyName !== "willBeDeleted"){
                                            console.log('WARNING: property change for unexisting sound different from "willBeDeleted": ', propertyName, propertyValue);
                                        }
                                    }

                                } else if ((treeType === "SOURCE_STATE") || (treeType === "PRESET")){
                                    // If one of the main state or preset state properties changed, update main preset parameters as it will be contained in one of these
                                    updateMainPresetParameters();
                                } else if (treeType === "MIDI_CC_MAPPING"){
                                    updateMIDIMappingEditor(treeUUID);
                                }
                            }
                            
                        } else if (updateType === 'addedChild'){
                            const parentTreeUUID = restOfDataSliced[0];
                            const parentTreeType = restOfDataSliced[1];
                            const indexInParentChildren = parseInt(restOfDataSliced[2], 10);
                            const childXmlString = restOfDataSliced.slice(3, restOfDataSliced.length).join(';');
                            // Add new child element to the state
                            var elementToUpdate;
                            if (parentTreeType === "PRESET"){
                                // If adding child of type preset, add it to the main SOURCE_STATE node regardless of the SOURCE_STATE uuid which some times changes
                                elementToUpdate = ss.state.childNodes[0];
                            } else {
                                elementToUpdate = self.state.querySelector("[uuid='" + parentTreeUUID + "']");
                            }
                            if (elementToUpdate !== null){
                                var nodeToAdd = self.parser.parseFromString(childXmlString, "text/xml").childNodes[0];
                                if (indexInParentChildren < 0){
                                    elementToUpdate.appendChild(nodeToAdd);
                                } else {
                                    var beforeNode = elementToUpdate.childNodes[indexInParentChildren];
                                    elementToUpdate.insertBefore(nodeToAdd, beforeNode);
                                }

                                // If new sounds or midi cc mappings are added, recreate sounds section
                                if ((nodeToAdd.tagName === "MIDI_CC_MAPPING") || (nodeToAdd.tagName === "SOUND")){
                                    updateSoundList();
                                }
                            }
                            
                        } else if (updateType === 'removedChild'){
                            const childToRemoveUUID = restOfDataSliced[0];
                            const childToRemoveType = restOfDataSliced[1];

                            // Remove the selected element from the state
                            const stateElementToRemove = self.state.querySelector("[uuid='" + childToRemoveUUID + "']");
                            if (stateElementToRemove !== null){
                                stateElementToRemove.remove();

                                // Trigger corresponding UI updates
                                const domElementToUpdate = document.getElementById(childToRemoveUUID);
                                if (domElementToUpdate !== null){
                                    domElementToUpdate.remove();    
                                }
                                
                                if (childToRemoveType === "SOUND"){
                                    updateAllSoundCardNumbers();
                                }
                            }
                        }

                        self.scheduleExtraFullStateRequestAfterBurstOfMessages();
                    }                
                };

                return socket;
            }

            sendMessageToPlugin(address, args){
                const message = address + ':' + args.join(';');
                this.wsConnection.send(message);
            }

            requestFullState(){
                this.sendMessageToPlugin('/get_state', ["full"]);
            }

            requestVolatileState(){
                this.sendMessageToPlugin('/get_state', ["volatile"]);
            }

            // Util methods to get state info

            getSounds(){
                return ss.state.getElementsByTagName('SOUND');
            }

            getSoundSamplerSounds(sound){  // sound = sound xml state
                if (sound !== null){
                    return sound.getElementsByTagName('SOUND_SAMPLE');
                } else {
                    return [];
                }
                
            }

            getFirstSoundSampleSound(sound) {  // sound = sound xml state
                return this.getSoundSamplerSounds(sound)[0];
            }

            getSoundFromSoundUUID(soundUUID){
                return ss.state.querySelector("[uuid='" + soundUUID + "']");
            }

            getSoundFromSoundSampleUUID(soundSampleUUID){
                const soundSample = ss.state.querySelector("[uuid='" + soundSampleUUID + "']");
                return ss.getSoundFromSoundUUID(soundSample.parentElement.getAttribute('uuid'));
            }

            getMidiMappingStateFromMappingUUID(mappingUUID){
                return ss.state.querySelector("[uuid='" + mappingUUID + "']");
            }

            getSoundMidiMappings(sound){  // sound = sound xml state
                return sound.getElementsByTagName('MIDI_CC_MAPPING');
            }

            findSoundIdxFromSoundUUID(soundUUID){            
                var sounds = ss.getSounds();
                for (var i=0; i<sounds.length; i++) {
                    if (sounds[i].getAttribute("uuid") == soundUUID){
                        return i + 1;
                    }
                }
                return -1
            }

            findSoundIdxFromSourceSamplerSoundUUID(samplerSoundUUID){            
                var sounds = ss.getSounds();
                for (var i=0; i<sounds.length; i++) {
                    var samplerSounds = this.getSoundSamplerSounds(sounds[i]);
                    for (var j=0; j<samplerSounds.length; j++) {
                        var samplerSound = samplerSounds[j];
                        if (samplerSound.getAttribute("uuid") == samplerSoundUUID){
                            return i + 1;
                        }
                    }
                }
                return -1
            }

            getTmpFilesLocation(){
                return this.state.getElementsByTagName('SOURCE_STATE')[0].getAttribute('tmpFilesLocation');
            }

            // Util methods to change things of the state

            setSoundParameter(sliderElement){
                var soundUUID = $("select#sound-dropdown option").filter(':selected').data('uuid');
                saveCurrentUserActionTime();
                if (soundUUID === ''){
                    // If control from "all sounds" card, label needs to be updated manually as it won't be updated when receiving the
                    // corresponding state updates
                    updateSliderLabel(sliderElement);
                } 
                this.sendMessageToPlugin('/set_sound_parameter', [soundUUID, sliderElement.name, getProcessedSoundParameterValuLinToExp(sliderElement)])
                ss.updateSoundElementOnUI(2);
            }

            setSoundParameterInt(soundUUID, sliderElement){
                saveCurrentUserActionTime();
                if (soundUUID === ''){
                    // If control from "all sounds" card, label needs to be updated manually as it won't be updated when receiving the
                    // corresponding state updates
                    updateSliderLabel(sliderElement);
                } 
                this.sendMessageToPlugin('/set_sound_parameter_int', [soundUUID, sliderElement.name, sliderElement.value])
            }

            replaceAllSoundsByQuery(){
                var query = document.getElementsByName('query')[0].value;
                var numSounds = document.getElementsByName('numSounds')[0].value || 16;
                var minSoundLength = document.getElementsByName('minSoundLength')[0].value || 0.0;
                var maxSoundLength = document.getElementsByName('maxSoundLength')[0].value || 0.5;
                var noteMappingType = document.getElementsByName('noteMappingType')[0].value;
                this.sendMessageToPlugin('/replace_sounds_from_query', [query, numSounds, minSoundLength, maxSoundLength, noteMappingType])
            }

            replaceSoundByQuery(){
                var numSoundToReplace = document.getElementsByName('soundIdxToReplace')[0].value;
                var soundUUIDToReplace = ss.getSounds()[numSoundToReplace - 1].getAttribute('uuid');
                var query = document.getElementsByName('query')[0].value;
                var numSounds = document.getElementsByName('numSounds')[0].value || 16;
                var minSoundLength = document.getElementsByName('minSoundLength')[0].value || 0.0;
                var maxSoundLength = document.getElementsByName('maxSoundLength')[0].value || 0.5;
                var noteMappingType = document.getElementsByName('noteMappingType')[0].value;
                this.sendMessageToPlugin('/replace_sound_from_query', [soundUUIDToReplace, query, numSounds, minSoundLength, maxSoundLength, noteMappingType])
            }

            addSoundsByQuery(){
                var query = document.getElementsByName('query')[0].value;
                var numSounds = document.getElementsByName('numSounds')[0].value || 16;
                var minSoundLength = document.getElementsByName('minSoundLength')[0].value || 0.0;
                var maxSoundLength = document.getElementsByName('maxSoundLength')[0].value || 0.5;
                this.sendMessageToPlugin('/add_sounds_from_query', [query, 4, minSoundLength, maxSoundLength, "contiguous"])
                updateSoundList();
            }

            reapplyLayout(){
                var noteMappingType = document.getElementsByName('noteMappingType')[0].value;
                this.sendMessageToPlugin('/reapply_layout', [noteMappingType]);
            }
                           
            clearAllSounds(){
                this.sendMessageToPlugin('/clear_all_sounds', [])
            }

            setMidiInChannel() {
                var midiInChannel = document.getElementsByName('midiInChannel')[0].value;
                this.sendMessageToPlugin('/set_midi_in_channel', [midiInChannel])
            }

            setReverbParameters() {
                var roomSize = document.getElementById('0_roomSize').value;
                var damping = document.getElementById('0_damping').value;
                var wetLevel = document.getElementById('0_wetLevel').value;
                var dryLevel = document.getElementById('0_dryLevel').value;
                var width = document.getElementById('0_width').value;
                var freezeMode = document.getElementById('0_freezeMode').value;
                updateAllSliderLabels();
                this.sendMessageToPlugin('/set_reverb_parameters', [roomSize, damping, wetLevel, dryLevel, width, freezeMode])
            }

            setNumVoices() {
                var numVoices = document.getElementsByName('numVoices')[0].value;
                this.sendMessageToPlugin('/set_polyphony', [numVoices])
            }

            playSound(){
                var soundUUID = $("select#sound-dropdown option").filter(':selected').data('uuid'); 
                this.sendMessageToPlugin('/play_sound', [soundUUID])
                var sound =  ss.getSoundFromSoundUUID(soundUUID);
                var parameterValue = sound.getAttribute('pitch');
                console.log(parameterValue);
                this.updateSoundElementOnUI(parameterValue);

            }

            stopSound(soundUUID){
                this.sendMessageToPlugin('/stop_sound', [soundUUID])
            }
                                        
            removeSound(soundUUID){
                this.sendMessageToPlugin('/remove_sound', [soundUUID])
            }

            savePreset(){
                var name = document.getElementsByName('presetName')[0].value;
                var idx = document.getElementsByName('presetIdx')[0].value;
                this.sendMessageToPlugin('/save_preset', [name, idx])
            }

            loadPreset() {
                var idx = document.getElementsByName('presetIdx')[0].value;
                this.sendMessageToPlugin('/load_preset', [idx])
            }

            nextPreset(){
                var idx = document.getElementsByName('presetIdx')[0].value;
                if (idx == undefined){
                    idx = -1;
                } else {
                    idx = parseInt(idx, 10);
                    if (isNaN(idx)){
                        idx = -1;
                    }
                }
                this.sendMessageToPlugin('/load_preset', [idx + 1])
            }

            previousPreset(){
                var idx = document.getElementsByName('presetIdx')[0].value;
                if (idx == undefined){
                    idx = 1;
                } else {
                    idx = parseInt(idx, 10);

                    if (isNaN(idx)){
                        idx = 1;
                    }
                }
                if (idx < 1){
                    idx = 1; //Will become 0 when subtracted 1
                }
                this.sendMessageToPlugin('/load_preset', [idx - 1])
            }

            updateSoundElementOnUI(sliderNewVal){
                console.log("updating pitch...");
                console.log(sliderNewVal);
                $("#pitch").attr('value',sliderNewVal);

            }

            createUpdateMapping(soundUUID, mappingID){
                var ccNumber = document.getElementById(mappingID + '_mappingCCNumber').value;
                var name = document.getElementById(mappingID + '_mappingParameterName').value;
                var minRange = parseFloat(document.getElementById(mappingID + '_mappingMinRange').value, 10);
                var maxRange = parseFloat(document.getElementById(mappingID + '_mappingMaxRange').value, 10);
                this.sendMessageToPlugin('/add_or_update_cc_mapping', [soundUUID, mappingID, ccNumber, name, minRange, maxRange]);
                if (mappingID == ""){
                    // Remove this element from DOM because this one is only while creating a new mappping
                    document.getElementById('newMappingPlaceholder_' + soundUUID).innerHTML = "";
                }
            }

            removeMidiMapping(soundUUID, mappingID){
                if (mappingID != ""){
                    this.sendMessageToPlugin('/remove_cc_mapping', [soundUUID, mappingID]);
                    // No need to remove the element itself because it will be removed when updating state
                } else {
                    document.getElementById('newMappingPlaceholder_' + soundUUID).innerHTML = "";  // Remove new mapping palceholder as the mapping was not yet created
                }
            }

        }

        document.addEventListener("DOMContentLoaded", function () {

            // Main funciont: reads some configuration properties form the URL and starte the StateSynchronizer
            // Once the StateSyncronizer is started, it will start to receive updates when changes happen in
            // the plugin's state. These updates will triger renders of appropriate parts of the UI.

            const queryString = window.location.search;
            const urlParams = new URLSearchParams(queryString);

            // If hostname is passed as a parameter, use it for WS and HTTP communications
            // Otherwise set it to the hostname of the current location
            if (urlParams.get('hostname') !== null){
                hostname = urlParams.get('hostname');
            } else {
                if (location.hostname !== ""){
                    hostname = location.hostname;
                } else {
                    hostname = 'localhost';
                }
            }
            
            // Get websockets/http connection config parameters (or use defaults)
            if (urlParams.get('wsPort') !== null){
                wsPort = urlParams.get('wsPort');
            } else {
                wsPort = "8125";
            }
            if (urlParams.get('useWss') !== null){
                useWss = urlParams.get('useWss') == '1';
            } else {
                useWss = false;
            }
            if (urlParams.get('httpPort') !== null){
                httpPort = urlParams.get('httpPort');
            } else {
                httpPort = "8124";
            }
            useHttps = useWss;

            // Start state synchronizer
            ss = new StateSynchronizer(wsPort, useWss);
        });

        // Util methods

        function saveCurrentUserActionTime(){
            lastUIAction = (new Date()).getTime();
        }

        function msSinceLastUserAction(){
            return (new Date()).getTime() - lastUIAction;
        }

        function linToExp(x){
            return (Math.pow(x,expStrength));
        }

        function expToLin(x){
            return (Math.pow(x,1/expStrength));
        }

        function sliderShouldBehaveExponentially(sliderElement){
            var name = sliderElement.name;
            if (name != null){
                if ((name.toLowerCase().indexOf("attack") > -1) ||
                    (name.toLowerCase().indexOf("decay") > -1) ||
                    (name.toLowerCase().indexOf("release") > -1) ||
                    (name == "filterCutoff")
                ){
                    return true;
                }
            }
            
            return false;
        }

        function getProcessedSoundParameterValuLinToExp(sliderElement){
            var value = parseFloat(sliderElement.value, 10);
            if (sliderShouldBehaveExponentially(sliderElement) == true){
                return linToExp(value/sliderElement.max)*sliderElement.max;
            } else {
                return value;
            }
        }

        function getProcessedSoundParameterValueExpToLin(sliderElement, valueRaw){
            if (sliderShouldBehaveExponentially(sliderElement) == true){
                return expToLin(valueRaw/sliderElement.max)*sliderElement.max;
            } else {
                return valueRaw;
            }
        }
        
        function getMidiControllableParameterNames(){
            // --> Start auto-generated code B
            parameterNames = ["startPosition", "endPosition", "loopStartPosition", "loopEndPosition", "playheadPosition", "freezePlayheadSpeed", "filterCutoff", "filterRessonance", "gain", "pan", "pitch"]
            // --> End auto-generated code B
            return parameterNames;
        }
        
        function getAllSoundParameterNames(){
            // --> Start auto-generated code C
            parameterNames = ["launchMode", "startPosition", "endPosition", "loopStartPosition", "loopEndPosition", "loopXFadeNSamples", "reverse", "noteMappingMode", "numSlices", "playheadPosition", "freezePlayheadSpeed", "filterCutoff", "filterRessonance", "filterKeyboardTracking", "filterAttack", "filterDecay", "filterSustain", "filterRelease", "filterADSR2CutoffAmt", "gain", "attack", "decay", "sustain", "release", "pan", "pitch", "pitchBendRangeUp", "pitchBendRangeDown", "mod2CutoffAmt", "mod2GainAmt", "mod2PitchAmt", "mod2PlayheadPos", "vel2CutoffAmt", "vel2GainAmt", "midiChannel"]
            // --> End auto-generated code C
            return parameterNames;
        }

        function isSoundParameter(parameterName){
            return getAllSoundParameterNames().indexOf(parameterName) > -1;
        }
        
        function getAllSoundParameterTypes(){
            // --> Start auto-generated code D
            parameterTypes = ["int", "float", "float", "float", "float", "int", "int", "int", "int", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "float", "int"]
            // --> End auto-generated code D
            return parameterTypes;
        }

        function getParameterType(parameterName){
            return getAllSoundParameterTypes()[getAllSoundParameterNames().indexOf(parameterName)];
        }

        // Methods to update different parts of the UI

        function updateAllSliderLabels(){
            var elements = document.querySelectorAll("input[type=range]")
            for (var i = 0, sliderElement; sliderElement = elements[i++];) {
                updateSliderLabel(sliderElement);
            }
        }

        function updateSliderLabel(sliderElement){
            var labelElement = document.getElementById(sliderElement.id + "Label");
            if (labelElement != null) {
                if (labelElement.id.indexOf("launchModeLabel") > -1){
                    labelElement.innerHTML = ["gate", "loop", "loopfb", "trigger", "freeze"][sliderElement.value];
                } else if (labelElement.id.indexOf("noteMappingMode") > -1){
                    labelElement.innerHTML = ["pitch", "slice", "both", "repeat"][sliderElement.value];
                } else if (labelElement.id.indexOf("soundType") > -1){
                    labelElement.innerHTML = ["single-sample", "multi-sample"][sliderElement.value];
                } else if (labelElement.id.indexOf("midiChannel") > -1){
                    labelElement.innerHTML = ["global", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"][sliderElement.value];
                } else if (labelElement.id.indexOf("numSlices") > -1){
                    var names =  ["auto (onsets)", "auto (n notes)"]
                    for (var i=2; i<=128; i++){
                        names.push(i);
                    }
                    labelElement.innerHTML = names[sliderElement.value];
                } else {
                    labelElement.innerHTML = getProcessedSoundParameterValuLinToExp(sliderElement).toFixed(2);
                }
            }
        }

        function addNewMIDIMappingEditor(soundUUID) {
                document.getElementById('newMappingPlaceholder_' + soundUUID).innerHTML = createMIDIMappingEditorFromRawParams(soundUUID, "", 0, "", 0.0, 1.0);
            }
        
        function createMIDIMappingEditor(soundUUID, mapping){ // mapping xml form state
            var mappingUUID = mapping.getAttribute('uuid');
            var ccNumber = parseInt(mapping.getAttribute('ccNumber'), 10);
            var name = mapping.getAttribute('parameterName');
            var minRange = parseFloat(mapping.getAttribute('minRange'), 10);
            var maxRange = parseFloat(mapping.getAttribute('maxRange'), 10);
            return createMIDIMappingEditorFromRawParams(soundUUID, mappingUUID, ccNumber, name, minRange, maxRange);
        }

        function createMIDIMappingEditorFromRawParams(soundUUID, mappingUUID, ccNumber, parameterName, minRange, maxRange){
            html = '<div id="' + mappingUUID + '" data-sound-id="' + soundUUID + '" class="midiMapping">';
            html += 'CC number: <input id="' + mappingUUID + '_mappingCCNumber" value="' + ccNumber + '" type="number"><br>';
            html += 'Parameter: <select class="select-css" id="' + mappingUUID + '_mappingParameterName">';
            var availableParams = getMidiControllableParameterNames();
            for (var i=0; i<availableParams.length; i++){
                if (availableParams[i] != parameterName){
                    html += '<option value="' + availableParams[i] + '">&nbsp;&nbsp;' + availableParams[i] + '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</option>';
                } else {
                    html += '<option value="' + availableParams[i] + '" selected>&nbsp;&nbsp;' + availableParams[i] + '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</option>';
                }
            }
            html += '</select><br>';
            html += '<input type="range" id="' + mappingUUID + '_mappingMinRange"  min="0" max="1.0" value="' + minRange + '" step="0.01"> min: <span id="' + mappingUUID + '_mappingMinRangeLabel"></span><br>';
            html += '<input type="range" id="' + mappingUUID + '_mappingMaxRange"  min="0" max="1.0" value="' + maxRange + '" step="0.01"> max: <span id="' + mappingUUID + '_mappingMaxRangeLabel"></span><br>';
            html += '<button onclick="ss.createUpdateMapping(\'' + soundUUID + '\',\'' + mappingUUID + '\')">save</button><button onclick="ss.removeMidiMapping(\'' + soundUUID + '\',\'' + mappingUUID + '\')">x</button>';
            html += '</div>';
            return html;
        }

        function updateMIDIMappingEditor(mappingUUID){
            var mapping = ss.getMidiMappingStateFromMappingUUID(mappingUUID);
            var ccNumber = parseInt(mapping.getAttribute('ccNumber'), 10);
            var name = mapping.getAttribute('parameterName');
            var minRange = parseFloat(mapping.getAttribute('minRange'), 10);
            var maxRange = parseFloat(mapping.getAttribute('maxRange'), 10);
            
            document.getElementById(mappingUUID + '_mappingCCNumber').value = ccNumber;
            document.getElementById(mappingUUID + '_mappingParameterName').value = name;
            document.getElementById(mappingUUID + '_mappingMinRange').value = minRange;
            document.getElementById(mappingUUID + '_mappingMinRangeLabel').innerHTML = minRange;
            document.getElementById(mappingUUID + '_mappingMaxRange').value = maxRange;
            document.getElementById(mappingUUID + '_mappingMaxRangeLabel').innerHTML = maxRange;
        }

        function updateAllSoundCardNumbers() {
            var sounds = ss.getSounds();
            for (i=0; i<sounds.length; i++) {
                var soundUUID = sounds[i].getAttribute('uuid');
                var soundCard = document.getElementById(soundUUID + '_idx');
                if (soundCard !== null){
                    soundCard.innerHTML =  document.getElementById(soundUUID).dataset.soundIdx;
                }
            }
        }

        function createSoundCard(sound){  //sound = xml element from state
            var soundUUID = sound.getAttribute('uuid');
            
            // Create sound card element
            var element = document.createElement('div');
            element.id = soundUUID;
            element.dataset.soundIdx = 0;
            element.classList = 'controls_card';

            // Create inner html of sound card element
            var html = '<div id="soundCardInfo_' + soundUUID + '"></div>';
            html += '<div><div id="waveformLoadingIndicator_' + soundUUID + '"></div><canvas id="soundCardWaveform_' + soundUUID + '"></canvas></div>';
            html += '<div id="soundCardControls_' + soundUUID +'" style="display:none;">' + generateSoundEditControls(soundUUID) + '</div>';
            html += '<div id="soundCardMappingsWrapper_' + soundUUID + '" style="display:none;"><br><button onclick="addNewMIDIMappingEditor(\'' + soundUUID +'\');">Add CC mapping</button><div id="newMappingPlaceholder_' + soundUUID + '"></div>';
            html += '<div id="soundCardMappings_' + soundUUID + '"></div>';
            html += '</div>';

            element.innerHTML = html;
            return element;
        }

        var rateLimitedTimers = {};
        var lastTimeFunctionCalled = {};
        var maxUpdateSoundRate = 100;
        function triggerRateLimitedUpdateSoundCard(soundUUID){
            // Use this function to reduce the rate of the calls to updateSoundCard
            // It can happen that when many properties change, we call updateSoundCard many times. Using this function this will be reduced.
            if ((lastTimeFunctionCalled[soundUUID] === undefined) || (Date.now() - lastTimeFunctionCalled[soundUUID] > maxUpdateSoundRate)){
                updateSoundCard(soundUUID);
                lastTimeFunctionCalled[soundUUID] = Date.now();
            }

            if (rateLimitedTimers[soundUUID] !== undefined){
                // If a timer already set, cancel it
                clearTimeout(rateLimitedTimers[soundUUID]);
            }
            rateLimitedTimers[soundUUID] = setTimeout(() => {
                updateSoundCard(soundUUID);
                lastTimeFunctionCalled[soundUUID] = Date.now();
            }, maxUpdateSoundRate);
        }
    
        function updateSoundCard(soundUUID){  //sound = xml element from state
            var sound = ss.getSoundFromSoundUUID(soundUUID);
            if (sound === null){
                // Sound can be null if triggering this method in a delayed way and sound does not longer exist
                return;
            }
            var soundUUID = sound.getAttribute('uuid');
            var samplerSounds = ss.getSoundSamplerSounds(sound);
            var samplerSound = samplerSounds[0];
            var soundCard = document.getElementById(soundUUID);
            if (soundCard === null){
                // Also if soundCard has not yet been created, don't continue
                return;
            }
            var soundInfoElement = document.getElementById('soundCardInfo_' + soundUUID);
            var downloadProgress = 0.0;  // Calculate download progress, take into account the case in which a sound has multiple source sampler sounds
            for (var i=0; i<samplerSounds.length; i++){
                var downloadCompleted = samplerSounds[i].getAttribute('downloadCompleted');
                var progress = samplerSounds[i].getAttribute('downloadProgress');
                if ((downloadCompleted === null) || (progress === null)){
                    downloadProgress += 0;
                } else {
                    var progress;
                    if (samplerSounds[i].getAttribute('downloadCompleted') === '1'){
                        progress = 100.0;
                    } else {
                        progress = parseFloat(samplerSounds[i].getAttribute('downloadProgress'));
                    }
                    downloadProgress += progress;
                }    
            }
            downloadProgress = downloadProgress/samplerSounds.length;
            
            var soundID = samplerSound.getAttribute('soundId');
            var sourceSamplerSoundUUID = samplerSound.getAttribute('uuid');
            var soundName = samplerSound.getAttribute('name');
            if (samplerSounds.length > 1){
                soundName = '(' + samplerSounds.length + ' sounds) ' + soundName;
            }
            var licenseURL = samplerSound.getAttribute('license') || "-";
            var soundIdx = 0;
            soundIdx = soundCard.dataset.soundIdx;

            if (sound.getAttribute("allSoundsLoaded") === "1"){
                // Sound already loaded, show all info (if it has not been already shown)
                html = '<div class="transportSoundButtons"><button onclick="ss.playSound(\'' + soundUUID + '\')">&#x25b6</button><button onclick="ss.stopSound(\'' + soundUUID + '\')">&#x25fc</button><button onclick="ss.removeSound(\'' + soundUUID + '\')">x</button></div>';
                html += '<h4><span id="' + soundUUID + '_idx">' + soundIdx + "</span> - " + soundID + '</h4>';
                html += '<div class="ellipsis">' + soundName + '</div>';
                var licenseName = "-";
                if ( licenseURL.indexOf('/by/') > -1) {licenseName = 'CC-BY'}
                else if ( licenseURL.indexOf('/sampling+/') > -1) {licenseName = 'S+'}
                else if ( licenseURL.indexOf('/by-nc/') > -1) {licenseName = 'CC-BY-NC'}
                else if ( licenseURL.indexOf('/zero/') > -1) {licenseName = 'CC0'}
                html += '<div class="ellipsis">' + parseFloat(samplerSound.getAttribute('duration'), 10).toFixed(2) + 's | ' + licenseName + '</div>';
                soundInfoElement.innerHTML = html;
                
                var soundControlsElement = document.getElementById('soundCardControls_' + soundUUID);
                if (soundControlsElement.style.display === "none") {
                    soundControlsElement.style.display = "block";
                }

                var soundMappingsWrapperElement = document.getElementById('soundCardMappingsWrapper_' + soundUUID);
                if (soundMappingsWrapperElement.style.display === "none") {
                    soundMappingsWrapperElement.style.display = "block";
                }

                drawSoundWaveform(sourceSamplerSoundUUID, 'soundCardWaveform_' + soundUUID, 'waveformLoadingIndicator_' + soundUUID);
            } else {
                // Sound not yet loaded, show download progress
                html = '<h4><span id="' + soundUUID + '_idx">' + soundIdx + " - " + soundID + '</h4>';
                html += '<div class="ellipsis">' + soundName + '</div>';
                if (downloadProgress === 100.0){
                    html += '<div>Loading in sampler...</div>';
                } else if (downloadProgress >= 0.0) {
                    html += '<div style="background: linear-gradient(to right, #4eb8a4 ' + downloadProgress + '%, black ' + downloadProgress + '%);">&nbsp;Downloading: ' + parseFloat(downloadProgress).toFixed(2) + '% </div>';
                }
                soundInfoElement.innerHTML = html;
            }
        }

        function updateSoundParameter(soundUUID, parameterName, parameterValue){
            const sliderElement = document.getElementById(soundUUID + '_' + parameterName);
            if (sliderElement === null){
                return;
            }
            if (parameterValue === undefined){
                var sound =  ss.getSoundFromSoundUUID(soundUUID);
                parameterValue = sound.getAttribute(parameterName);
            }
            const parameterType = getParameterType(parameterName);
            if (parameterType == "float"){
                sliderElement.value = getProcessedSoundParameterValueExpToLin(sliderElement, parseFloat(parameterValue, 10));
            } else if (parameterType == "int"){
                sliderElement.value = parseInt(parameterValue, 10);
            }
            updateSliderLabel(sliderElement);
        } 

        function updateMainPresetParameters(){
            var sourceState = ss.state.getElementsByTagName('SOURCE_STATE')[0];
            document.getElementsByName('presetIdx')[0].value = sourceState.getAttribute('currentPresetIndex');
            document.getElementsByName('midiInChannel')[0].value = sourceState.getAttribute('globalMidiInChannel');

            var presetState = sourceState.getElementsByTagName('PRESET')[0];
            document.getElementsByName('presetName')[0].value = presetState.getAttribute('name');
            document.getElementsByName('numVoices')[0].value = presetState.getAttribute('numVoices');
  
        }

        function updateSoundList(){

            var sounds = ss.getSounds();

            if ($("div.sound-list select").has('option').length > 0)
                {   
                    console.log("deleting old sounds");
                    $("div.sound-list select").empty();

                }
            for (var i = 0; i < sounds.length; i++) {
                var sound = sounds[i];
                // If sound is not scheduled for deletion, add a card for it
                var soundUUID = sound.getAttribute('uuid');
                var soundSampler = ss.getSoundSamplerSounds(ss.getSoundFromSoundUUID(soundUUID))[0];

                //Remove the previous sounds

                $("div.sound-list select").append($('<option>')
                                .text(soundSampler.getAttribute('name'))
                                .data({
                                    uuid: soundUUID,
                                    sound: soundSampler.getAttribute('name'),
                                })
                        );
            }

            // Now update all sound cards
            for (var i = 0; i < sounds.length; i++) {
                var sound = sounds[i];
                var soundUUID = sound.getAttribute('uuid');

                // Update main parameters of the sound card
                triggerRateLimitedUpdateSoundCard(soundUUID);

                // Update sound parameters (sliders) of sound card
                var soundParameters = getAllSoundParameterNames();
                for (var j=0; j<soundParameters.length; j++){
                    var parameterName = soundParameters[j];
                    updateSoundParameter(soundUUID, parameterName);                    
                }

            }
        }
        
        function updateFullUI(){
            // Set the main preset parameters to the values form the state
            updateMainPresetParameters();

            updateAllSliderLabels(); // So labels for all sounds section appear

            // Update all the section with individual sound cards
        }

        function updateVolatileUI(){
            // Update the audio meters and the voice allocation section
            // Also update other things like query status

            var VolatileState = ss.volatileState.getElementsByTagName('VOLATILE_STATE')[0];
            
            var isQuerying = VolatileState.getAttribute('isQuerying') != "0";
            var queryingIndicator = document.getElementById('queryingIndicator');
            if (isQuerying){
                queryingIndicator.classList.add('querying');
            } else {
                queryingIndicator.classList.remove('querying');
            }
            
            var voicesStateElement = document.getElementById('voicesState');
            var html = '<div class="voices">';
            if (VolatileState !== undefined){
                var voiceActivations = VolatileState.getAttribute('voiceActivations').split(',').slice(0, -1);
                var voiceSoundIdxs = VolatileState.getAttribute('voiceSoundIdxs').split(',').slice(0, -1);
                var voiceSoundPlayPosition = VolatileState.getAttribute('voiceSoundPlayPosition').split(',').slice(0, -1);
                
                for (var i=0; i<voiceSoundIdxs.length; i++){
                    if (voiceSoundIdxs[i] == "-1"){
                        // Draw empty square
                        html += '<div class="voice"><div class="voice_progress" style="width:0%;">&nbsp;</div></div>'
                    } else {
                        html += '<div class="voice"><div class="voice_progress" style="width:' + 100 * parseFloat(voiceSoundPlayPosition[i], 10) + '%;">' + ss.findSoundIdxFromSourceSamplerSoundUUID(voiceSoundIdxs[i]) + '</div></div>'
                    }
                }
            }
            html += "</div>";
            voicesStateElement.innerHTML = html;
            
            var metersStateElement = document.getElementById('metersState');
            var html = '<div class="meters">';
            if (VolatileState !== undefined){
                var audioLevels = VolatileState.getAttribute('audioLevels').split(',').slice(0, -1);
                for (var i=0; i<audioLevels.length; i++){
                    html += '<div class="channel_meter"><div class="channel_level" style="height:' + 100 * parseFloat(audioLevels[i], 10) + '%;"></div></div>'
                }
            }
            html += "</div>";
            metersStateElement.innerHTML = html;
        }

        function notifyQueryResultsFromLastQuery(numResults){
            var element = document.getElementById("numResultsIndicator");
            if (numResults > -1){
                element.innerHTML = numResults + ' results';
                setTimeout(() => {
                    element.innerHTML = '';
                }, 2000);
            } else {
                element.innerHTML = '';
            }
        }

       function drawSoundWaveform(){
            var sourceSoundUUID = $("select#sound-dropdown option").filter(':selected').data('uuid'); 
            console.log("drawinggg");
            var sound = ss.getSoundFromSoundUUID(sourceSoundUUID);
            var samplerSound = ss.getSoundSamplerSounds(sound);
            var sourceSamplerSoundUUID = samplerSound[0].getAttribute('uuid');
            // Conde inspired from https://css-tricks.com/making-an-audio-waveform-visualizer-with-vanilla-javascript/
            const drawAudio = url => {
              const request = new XMLHttpRequest();
              request.open("GET", url);
              request.responseType = "arraybuffer";
              request.onload = function() {
                let undecodedAudio = request.response;
                audioContext.decodeAudioData(undecodedAudio, (data) => draw(normalizeData(filterData(data))), (error) => console.log(error));
              };
              request.send();
              // NOTE: we used to use the syntax below but it does not seem to work in all computers...
              //fetch(url)
              //  .then(response => response.arrayBuffer())
              //  .then(arrayBuffer => audioContext.decodeAudioData(arrayBuffer))
              //  .then(audioBuffer => draw(normalizeData(filterData(audioBuffer))));
            };
            
            const filterData = audioBuffer => {
              const rawData = audioBuffer.getChannelData(0); // We only need to work with one channel of data
              const samples = 100; // Number of samples we want to have in our final data set
              const blockSize = Math.floor(rawData.length / samples); // the number of samples in each subdivision
              const filteredData = [];
              for (let i = 0; i < samples; i++) {
                let blockStart = blockSize * i; // the location of the first sample in the block
                let sum = 0;
                for (let j = 0; j < blockSize; j++) {
                  sum = sum + Math.abs(rawData[blockStart + j]) // find the sum of all the samples in the block
                }
                filteredData.push(sum / blockSize); // divide the sum by the block size to get the average
              }
              return filteredData;
            }
            
            const normalizeData = filteredData => {
              const multiplier = Math.pow(Math.max(...filteredData), -1);
              return filteredData.map(n => n * multiplier);
            }
            
            const draw = normalizedData => {
              // Set up the canvas
              const canvas = document.getElementById("waveform-canvas");
              const padding = 0;
              canvas.width = canvas.offsetWidth;
              canvas.height = (canvas.offsetHeight + padding * 2);
              const ctx = canvas.getContext("2d");
              //ctx.translate(0, canvas.offsetHeight / 2 + padding); // Set Y = 0 to be in the middle of the canvas
              
              // Draw points
              const width = canvas.offsetWidth / normalizedData.length;
              for (let i = 0; i < normalizedData.length; i++) {
                const x = width * i;
                let height = normalizedData[i] * canvas.offsetHeight - padding;
                ctx.lineWidth = 1; // how thick the line is
                ctx.strokeStyle = "#fff"; // what color our line is
                ctx.beginPath();
                ctx.moveTo(x, canvas.offsetHeight - padding);
                ctx.lineTo(x, canvas.offsetHeight - padding - height);
                ctx.stroke();
              }
            };
            
            const canvas = document.getElementById("waveform-canvas");
            if (canvas !== null){
                    if (httpPort !== undefined){
                        let url = 'http' + (useHttps ? 's':'') + '://' + hostname + ':' + httpPort + '/sounds_data/' + sourceSamplerSoundUUID + '.wav';
                        //let url = ss.getTmpFilesLocation() + '/' + sourceSamplerSoundUUID + '.wav'; // Not supported because of CORS...
                        console.log("Getting waveform for", url);
                        drawAudio(url);
                    }
                }
            }
        
        function drawAdsrEnvelope() {
            var canvas = document.getElementById("adsr-envelope");

            var context =  canvas.getContext("2d");
            var t, v, y, x;
            var i, imax;
            var duration = 10;

            context.strokeStyle = "rgba(22, 160, 133, 0.8)";
            context.lineWidth = 8;
            context.lineCap = "round";

            context.beginPath();
            for (i = 0, imax = canvas.width; i < imax; i++) {
                t = (i / imax) * duration;
                v = 10;
                y = canvas.height - (canvas.height * v);
                x = (i / imax) * canvas.width;

                context.lineTo(x, y);
            }
            context.stroke();

            imageData = context.getImageData(0, 0, canvas.width, canvas.height);
                }
