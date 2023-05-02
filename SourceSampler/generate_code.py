import csv
import os
from argparse import ArgumentParser
from collections import defaultdict


SOURCE_CODE_FOLDER = 'Source/'
START_DELIMITER_TEMPLATE = '// --> Start auto-generated code {0}'
END_DELIMITER_TEMPLATE = '// --> End auto-generated code {0}'
CONTROLS_DATA_FILENAME = 'data_for_code_gen.csv'


def generate_code(controls_data_filename):
    code_dict = {}

    # Read controls data from CSV file
    controls_list = list()
    for count, row in enumerate(csv.DictReader(open(controls_data_filename, 'r'), delimiter=';')):
        control_data = {
            'name': row.get('name', ''),
            'type': row.get('type', None),
            'min': row.get('min', None),
            'max':  row.get('max', ''),
            'default': row.get('default', ''),
            'midiCCenabled': row.get('midiCCenabled', '') == '1',
        }
        controls_list.append(control_data)

    # Generate SourceSamplerSound paramter attributes assignation for setParameterByNameFloat and setParameterByNameFloatNorm
    current_code = ''
    for count, control_data in enumerate([control_data for control_data in controls_list if control_data['type'] in ['float']]):
        iftag = 'else if' if count > 0 else 'if'
        control_data.update({'iftag': iftag})
        if control_data['type'] == 'float':
            minf = float(control_data['min'])
            maxf = float(control_data['max'])
            control_data.update({'minf': minf, 'maxf': maxf})
            current_code += '        {iftag} (identifier == SourceIDs::{name}) {{ {name} = !normed ? juce::jlimit({minf}f, {maxf}f, value) : juce::jmap(value, {minf}f, {maxf}f); }}\n'.format(**control_data)
        else:
            # Don't know what to do with other types
            pass
    current_code += '        '
    code_dict['Source/SourceSamplerSound.cpp'] = {}
    code_dict['Source/SourceSamplerSound.cpp']['B'] = current_code

    # Generate SourceSamplerSound paramter attributes assignation for setParameterByNameInt
    current_code = ''
    for count, control_data in enumerate([control_data for control_data in controls_list if control_data['type'] == 'int']):
        iftag = 'else if' if count > 0 else 'if'
        control_data.update({'iftag': iftag})
        if control_data['type'] == 'int':
            mini = int(control_data['min'])
            maxi = int(control_data['max'])
            control_data.update({'mini': mini, 'maxi': maxi})
            current_code += '        {iftag} (identifier == SourceIDs::{name}) {{ {name} = juce::jlimit({mini}, {maxi}, value); }}\n'.format(**control_data)  
        else:
            # Don't know what to do with other types
            pass
    current_code += '        '
    code_dict['Source/SourceSamplerSound.cpp']['D'] = current_code

     # Generate SourceSamplerSound attributes for getParameterFloat i getParameterInt
    current_code_e = ''
    current_code_f = ''
    for count, control_data in enumerate([control_data for control_data in controls_list if control_data['type'] in ['float']]):
        iftag = 'else if' if count > 0 else 'if'
        minf = float(control_data['min'])
        maxf = float(control_data['max'])
        control_data.update({'minf': minf, 'maxf': maxf, 'iftag': iftag})
        current_code_f += '        {iftag} (identifier == SourceIDs::{name}) {{ return !normed ? {name}.get() : juce::jmap({name}.get(), {minf}f, {maxf}f, 0.0f, 1.0f); }}\n'.format(**control_data)
    for count, control_data in enumerate([control_data for control_data in controls_list if control_data['type'] in ['int']]):
        iftag = 'else if' if count > 0 else 'if'
        control_data.update({'iftag': iftag})
        current_code_e += '        {iftag} (identifier == SourceIDs::{name}) {{ return {name}.get(); }}\n'.format(**control_data)
        
    current_code_e += '        '
    current_code_f += '        '
    code_dict['Source/SourceSamplerSound.cpp']['E'] = current_code_e
    code_dict['Source/SourceSamplerSound.cpp']['F'] = current_code_f

    # Generate SourceSamplerSound.h code to define parameters
    current_code = ''
    for count, control_data in enumerate([control_data for control_data in controls_list]):
        if control_data['type'] == 'float':
            current_code += "    juce::CachedValue<float> {name};\n".format(**control_data)
        elif control_data['type'] == 'int':
            current_code += "    juce::CachedValue<int> {name};\n".format(**control_data)
        else:
            # Don't know what to do with other types
            pass
    current_code += '    '
    code_dict['Source/SourceSamplerSound.h'] = {}
    code_dict['Source/SourceSamplerSound.h']['A'] = current_code

    # Generate SourceSamplerSound.h code to bind state
    current_code = ''
    for count, control_data in enumerate([control_data for control_data in controls_list]):
        current_code += "    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::{name}, SourceDefaults::{name});\n    {name}.referTo(state, SourceIDs::{name}, nullptr, SourceDefaults::{name});\n".format(**control_data)
    current_code += '    '
    code_dict['Source/SourceSamplerSound.cpp']['C'] = current_code

    # Generate defines.h code to define parameter IDs and defaults
    current_code_a = ''
    current_code_b = ''
    for count, control_data in enumerate([control_data for control_data in controls_list]):
        current_code_b += "DECLARE_ID ({name})\n".format(**control_data)
        if control_data['type'] == 'float':
            defaultf = float(control_data['default'])
            control_data.update({'defaultf': defaultf})
            current_code_a += "inline float {name} = {defaultf}f;\n".format(**control_data)
        elif control_data['type'] == 'int':
            defaulti = int(control_data['default'])
            control_data.update({'defaulti': defaulti})
            current_code_a += "inline int {name} = {defaulti};\n".format(**control_data)
        else:
            # Don't know what to do with other types
            pass
    code_dict['Source/defines_source.h'] = {}
    code_dict['Source/defines_source.h']['A'] = current_code_a
    code_dict['Source/defines_source.h']['B'] = current_code_b

    # Generate helpers_source.h code to include all sound parameters when creating an empry sound
    current_code_a = ''
    for count, control_data in enumerate([control_data for control_data in controls_list]):
        if control_data['type'] == 'float':
            defaultf = float(control_data['default'])
            control_data.update({'defaultf': defaultf})
            current_code_a += "        sound.setProperty (SourceIDs::{name}, {defaultf}f, nullptr);\n".format(**control_data)
        elif control_data['type'] == 'int':
            defaulti = int(control_data['default'])
            control_data.update({'defaulti': defaulti})
            current_code_a += "        sound.setProperty (SourceIDs::{name}, {defaulti}, nullptr);\n".format(**control_data)
        else:
            # Don't know what to do with other types
            pass
    current_code_a += "        "
    code_dict['Source/helpers_source.h'] = {}
    code_dict['Source/helpers_source.h']['A'] = current_code_a

    # Generate HTML interface sound edit elements code
    current_code = ''
    for count, control_data in enumerate([control_data for control_data in controls_list]):
        if control_data['type'] == 'float':
            defaultf = float(control_data['default'])
            minf = float(control_data['min'])
            maxf = float(control_data['max'])
            control_data.update({'minf': minf, 'maxf': maxf, 'defaultf': defaultf, 'step': 1.0 if control_data['name'] == 'basePitch' else 0.01})
            current_code += """            html += '<input type="range" id="' + soundUUID + '_{name}" name="{name}" min="{minf}" max="{maxf}" value="{defaultf}" step="{step}" oninput="ss.setSoundParameter(\\'' + soundUUID + '\\', this)" > {name}: <span id="' + soundUUID + '_{name}Label"></span><br>'\n""".format(
                **control_data)
        elif control_data['type'] == 'int':
            defaulti = int(control_data['default'])
            mini = int(control_data['min'])
            maxi = int(control_data['max'])
            control_data.update({'mini': mini, 'maxi': maxi, 'defaulti': defaulti, 'step': 1})
            current_code += """            html += '<input type="range" id="' + soundUUID + '_{name}" name="{name}" min="{mini}" max="{maxi}" value="{defaulti}" step="{step}" oninput="ss.setSoundParameterInt(\\'' + soundUUID + '\\', this)" > {name}: <span id="' + soundUUID + '_{name}Label"></span><br>'\n""".format(
                **control_data)
        else:
            # Don't know what to do with other types
            pass
    current_code += '            '
    code_dict['Resources/ui_plugin_ws.html'] = {}
    code_dict['Resources/ui_plugin_ws.html']['A'] = current_code

    # Generate list of controls modifiable via MIDI cc in interface.hmtl
    current_code = '            parameterNames = ['
    midiCCenabledControls = [control_data for control_data in controls_list if control_data['midiCCenabled'] == True]
    for count, control_data in enumerate(midiCCenabledControls):
        if count != len(midiCCenabledControls) - 1:
            current_code += '"{name}", '.format(**control_data)
        else:
            current_code += '"{name}"]'.format(**control_data)
    current_code += '\n            '
    code_dict['Resources/ui_plugin_ws.html']['B'] = current_code

    # Generate list of all controls in interface.hmtl
    current_code = '            parameterNames = ['
    allControls = [control_data for control_data in controls_list]
    for count, control_data in enumerate(allControls):
        if count != len(allControls) - 1:
            current_code += '"{name}", '.format(**control_data)
        else:
            current_code += '"{name}"]'.format(**control_data)
    current_code += '\n            '
    code_dict['Resources/ui_plugin_ws.html']['C'] = current_code

    # Generate list of all control types in interface.hmtl
    current_code = '            parameterTypes = ['
    allControls = [control_data for control_data in controls_list]
    for count, control_data in enumerate(allControls):
        if count != len(allControls) - 1:
            current_code += '"{type}", '.format(**control_data)
        else:
            current_code += '"{type}"]'.format(**control_data)
    current_code += '\n            '
    code_dict['Resources/ui_plugin_ws.html']['D'] = current_code


    print('Code successfully generated for: %s' % str(list(code_dict.keys())))

    return code_dict

def replace_in_file(path, start_delimiter, end_delimiter, code):
    file_contents = open(path, 'r').read()
    new_file_contents = file_contents.split(start_delimiter)[0] + start_delimiter  + '\n'
    new_file_contents += code
    new_file_contents += end_delimiter + file_contents.split(end_delimiter)[1]
    open(path, 'w').write(new_file_contents)
   
def insert_code(code_dict):
    for key, sections_dict in code_dict.items():
        for section_key, code in sections_dict.items():
            replace_in_file(
                key, 
                START_DELIMITER_TEMPLATE.format(section_key), 
                END_DELIMITER_TEMPLATE.format(section_key),
                code)
            print('Done inserting {0} lines of code in {1} - {2}'.format(code.count('\n') + 1, key, section_key))

if __name__ == '__main__':
    parser = ArgumentParser(description="""Automatically generates code to handle manual work for sound parameters.""")
    parser.add_argument('-i', '--insert', help='insert code in existing source files (replace existing)', action='store_true')
    parser.add_argument('-f', '--filename', help='CSV file with controls metadata', default=CONTROLS_DATA_FILENAME)
    args = parser.parse_args()

    code_dict = generate_code(args.filename)
    if args.insert:
        insert_code(code_dict)
    else:
        for key, sections_dict in code_dict.items():
            for section_key, code in sections_dict.items():
                print(key + ' - ' + section_key)
                print('*' * len(key + ' - ' + section_key))
                print()
                print(code)
