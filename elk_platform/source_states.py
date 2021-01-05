import time


class StateManager(object):

    state_stack = []

    def __init__(self, initial_state):
        self.state_stack = [initial_state]
        print('[SM] Initialized with state\n{0}'.format(self.current_state))

    def move_to(self, new_state, replace_current=False):
        if replace_current:
            self.state_stack.pop()
        self.state_stack.append(new_state)
        print('[SM] New state is\n{0}'.format(self.current_state))

    def go_back(self):
        if len(self.state_stack) > 1:
            self.state_stack.pop()
            print('[SM] New state\n{0}'.format(self.current_state))

    @property
    def current_state(self):
        return self.state_stack[-1]
    

class State(object):

    name = ""
    activation_time = None

    def __init__(self, *args, **kwargs):
        self.activation_time = time.time()

    def __str__(self):
        serialized = 'STATE: {0}'.format(self.name)
        for key, value in self.get_properties().items():
            serialized += '\n- {0}: {1}'.format(key, value)
        return serialized

    def get_properties(self):
        return {}

    @property
    def seconds_since_activation(self):
        return time.time() - self.activation_time

    def get_sound_idx_from_buttons(self, button_idx, shift=False):
        sound_idx = -1
        if button_idx > 0:
            sound_idx = button_idx - 1   # from 0-7
            if shift:  # if "shift" button is pressed, sound index is form 8-15
                sound_idx += 8
        # TODO: should check with number of loaded sounds
        return sound_idx

    def on_encoder_rotated(self, direction, shift=False):
        print('- Encoder rotated ({0}) in state {1} (shift={2})'.format(direction, self.name, shift))

    def on_encoder_pressed(self, shift=False):
        print('- Encoder pressed in state {0} (shift={1})'.format(self.name, shift))

    def on_encoder_released(self, shift=False):
        print('- Encoder released in state {0} (shift={1})'.format(self.name, shift))

    def on_button_pressed(self, button_idx, shift=False):
        print('- Button pressed ({0}) in state {1} (shift={2})'.format(button_idx, self.name, shift))

    def on_button_released(self, button_idx, shift=False):
        print('- Button released ({0}) in state {1} (shift={2})'.format(button_idx, self.name, shift))

    def on_fader_moved(self, fader_idx, value, shift=False):
        print('- Fader moved ({0}, {1}) in state {2} (shift={3})'.format(fader_idx, value, self.name, shift))


class PaginatedState(State):

    current_page = 0
    num_pages = 1

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.current_page = kwargs.get('current_page', 0)

    def get_properties(self):
        properties = super().get_properties().copy()
        properties.update({
           'current_page': self.current_page
        })
        return properties

    def next_page(self):
        self.current_page += 1
        if self.current_page >= self.num_pages:
            self.current_page = 0

    def previous_page(self):
        self.current_page -= 1
        if self.current_page < 0:
            self.current_page = self.num_pages - 1

    def on_encoder_rotated(self, direction, shift=False):
        if direction > 0:
            self.next_page()
        else:
            self.previous_page()
        print(self)


class HomeState(PaginatedState):

    name = "HomeState"
    num_pages = 4

    def on_button_released(self, button_idx, shift=False):
        # Select a sound
        sound_idx = self.get_sound_idx_from_buttons(button_idx, shift=shift)
        if sound_idx > -1:
            state_manager.move_to(SoundSelectedState(sound_idx))

    def on_encoder_released(self, shift=False):
        if shift:
            # Do nothing (we don't have more levels to go back)
            # TODO: maybe add some handy action here?
            pass
        else:
            # Open home contextual menu
            state_manager.move_to(HomeContextualMenuState())

    def on_fader_moved(self, fader_idx, value, shift=False):
        # TODO: set parameter via OSC
        pass


class SoundSelectedState(PaginatedState):

    name = "SoundSelectedState"
    sound_idx = -1
    num_pages = 4

    def __init__(self, sound_idx, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.sound_idx = sound_idx

    def get_properties(self):
        properties = super().get_properties().copy()
        properties.update({
            'sound_idx': self.sound_idx
        })
        return properties
    
    def on_button_released(self, button_idx, shift=False):
        # Select another sound
        sound_idx = self.get_sound_idx_from_buttons(button_idx, shift=shift)
        if sound_idx > -1:
            state_manager.move_to(SoundSelectedState(sound_idx, current_page=self.current_page), replace_current=True)

    def on_encoder_released(self, shift=False):
        if shift:
            # Go back to home state
            state_manager.go_back()
        else:
            # Open sound contextual menu
            state_manager.move_to(SoundSelectedContextualMenuState(sound_idx=self.sound_idx))

    def on_fader_moved(self, fader_idx, value, shift=False):
        # TODO: set parameter via OSC
        pass



class HomeContextualMenuState(State):

    name = "HomeContextualMenuState"

    def on_encoder_released(self, shift=False):
        if shift:
            # Go back to home state
            state_manager.go_back()


class SoundSelectedContextualMenuState(State):

    name = "SoundSelectedContextualMenuState"
    sound_idx = -1

    def __init__(self, sound_idx, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.sound_idx = sound_idx

    def get_properties(self):
        properties = super().get_properties().copy()
        properties.update({
            'sound_idx': self.sound_idx
        })
        return properties

    def on_encoder_released(self, shift=False):
        if shift:
            # Go back to sound selected state
            state_manager.go_back()


class SoundParameterMIDILearnMenuState(State):

    name = "SoundParameterMIDILearnMenuState"


state_manager = StateManager(HomeState())
