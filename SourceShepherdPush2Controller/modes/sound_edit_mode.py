import definitions
import push2_python

from definitions import ShepherdControllerMode
from utils import show_text, draw_knob


class SoundEditMode(ShepherdControllerMode):

    tabs_button_names = [
        push2_python.constants.BUTTON_UPPER_ROW_1,
        push2_python.constants.BUTTON_UPPER_ROW_2,
        push2_python.constants.BUTTON_UPPER_ROW_3,
        push2_python.constants.BUTTON_UPPER_ROW_4,
        push2_python.constants.BUTTON_UPPER_ROW_5,
        push2_python.constants.BUTTON_UPPER_ROW_6,
        push2_python.constants.BUTTON_UPPER_ROW_7,
        push2_python.constants.BUTTON_UPPER_ROW_8
    ]
    page_left_button = push2_python.constants.BUTTON_PAGE_LEFT
    page_right_button = push2_python.constants.BUTTON_PAGE_RIGHT

    buttons_used = tabs_button_names + [page_left_button, page_right_button]
    current_tab = 0

    tabs = [
        {},
        {},
        {}
    ]

    def get_current_track_color_helper(self):
        return self.app.track_selection_mode.get_current_track_color()

    def get_should_show_next_prev_page_buttons(self):
        show_prev = False
        if self.current_tab > 0:
            show_prev = True
        show_next = False
        if self.current_tab + 1 < len(self.tabs):
            show_next = True
        return show_prev, show_next

    def activate(self):
        self.update_buttons()

    def update_buttons(self):
        for count, name in enumerate(self.tabs_button_names):
            self.set_button_color_if_expression(name, count < len(self.tabs), false_color=definitions.BLACK)
        show_prev, show_next = self.get_should_show_next_prev_page_buttons()
        self.set_button_color_if_expression(self.page_left_button, show_prev)
        self.set_button_color_if_expression(self.page_right_button, show_next)

    def update_display(self, ctx, w, h):
        if not self.app.is_mode_active(self.app.settings_mode) and \
           not self.app.is_mode_active(self.app.clip_triggering_mode) and \
           not self.app.is_mode_active(self.app.clip_edit_mode):
            # If settings mode is active, don't draw the upper parts of the screen because settings page will
            # "cover them"

            current_track_color = self.get_current_track_color_helper()

            # Draw tab names
            tab_names = ["Tab {}".format(count + 1) for count, _ in enumerate(self.tabs)]
            if tab_names:
                height = 20
                for i, tab_name in enumerate(tab_names):
                    show_text(ctx, i, 0, tab_name, background_color=definitions.RED)
                    is_selected = self.current_tab == i
                    if is_selected:
                        background_color = current_track_color
                        font_color = definitions.BLACK
                    else:
                        background_color = definitions.BLACK
                        font_color = current_track_color
                    show_text(ctx, i, 0, tab_name, height=height,
                              font_color=font_color, background_color=background_color)

            # TODO: Draw actual controls
            sound_name = self.app.source_interface.get_sound_property(self.app.track_selection_mode.selected_track, 'name_sound', default='Unknown sound')
            show_text(ctx, 0, 25, sound_name, height=30,
                      font_color=current_track_color, background_color=definitions.BLACK)

    def on_button_pressed(self, button_name, shift=False, select=False, long_press=False, double_press=False):
        if button_name in self.tabs_button_names:
            idx = self.tabs_button_names.index(button_name)
            if idx < len(self.tabs):
                self.current_tab = idx
            return True

        elif button_name in [self.page_left_button, self.page_right_button]:
            show_prev, show_next = self.get_should_show_next_prev_page_buttons()
            if button_name == self.page_left_button and show_prev:
                self.current_tab -= 1
            elif button_name == self.page_right_button and show_next:
                self.current_tab += 1
            return True
