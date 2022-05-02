from .manager import state_manager, source_plugin_interface
from .states_home import HomeState

state_manager.move_to(HomeState())
