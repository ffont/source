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
            'default': row.get('default', '')
        }
        controls_list.append(control_data)

    # Generate SourceSamplerSound paramter attributes assignation for setParameterByNameFloat and setParameterByNameFloatNorm
    current_code = ''
    for count, control_data in enumerate([control_data for control_data in controls_list if control_data['type'] in ['float', 'adsr']]):
        iftag = 'else if' if count > 0 else 'if'
        control_data.update({'iftag': iftag})
        if control_data['type'] == 'float':
            minf = float(control_data['min'])
            maxf = float(control_data['max'])
            control_data.update({'minf': minf, 'maxf': maxf})
            current_code += '    {iftag} (name == "{name}") {{ {name} = jlimit({minf}f, {maxf}f, value); }}\n'.format(**control_data)
        elif control_data['type'] == 'adsr':
            for adsr_phase in ['attack', 'decay', 'sustain', 'release']:  # Note that these names ust match ADSR::Parameters names
                control_data.update({'adsr_phase': adsr_phase})
                current_code += '    {iftag} (name == "{name}.{adsr_phase}") {{ {name}.{adsr_phase} = value; }}\n'.format(**control_data)
        else:
            # Don't know what to do with other types
            pass
    current_code += '    '
    code_dict['SourceSampler.cpp'] = {}
    code_dict['SourceSampler.cpp']['A'] = current_code

    current_code = ''
    for count, control_data in enumerate([control_data for control_data in controls_list if control_data['type'] in ['float', 'adsr']]):
        iftag = 'else if' if count > 0 else 'if'
        control_data.update({'iftag': iftag})
        if control_data['type'] == 'float':
            minf = float(control_data['min'])
            maxf = float(control_data['max'])
            control_data.update({'minf': minf, 'maxf': maxf})
            current_code += '    {iftag} (name == "{name}") {{ setParameterByNameFloat("{name}", jmap(value0to1, {minf}f, {maxf}f)); }}\n'.format(**control_data)
        elif control_data['type'] == 'adsr':
            for adsr_phase in ['attack', 'decay', 'sustain', 'release']:  # Note that these names ust match ADSR::Parameters names
                control_data.update({'adsr_phase': adsr_phase})
                current_code += '    {iftag} (name == "{name}.{adsr_phase}") {{ setParameterByNameFloat("{name}.{adsr_phase}", value0to1); }}\n'.format(**control_data)
        else:
            # Don't know what to do with other types
            pass
    current_code += '    '
    code_dict['SourceSampler.cpp']['D'] = current_code

    # Generate SourceSamplerSound paramter attributes assignation for setParameterByNameInt
    current_code = ''
    for count, control_data in enumerate([control_data for control_data in controls_list if control_data['type'] == 'int']):
        iftag = 'else if' if count > 0 else 'if'
        control_data.update({'iftag': iftag})
        if control_data['type'] == 'int':
            mini = int(control_data['min'])
            maxi = int(control_data['max'])
            control_data.update({'mini': mini, 'maxi': maxi})
            current_code += '    {iftag} (name == "{name}") {{ {name} = jlimit({mini}, {maxi}, value); }}\n'.format(**control_data)  
        else:
            # Don't know what to do with other types
            pass
    current_code += '    '
    code_dict['SourceSampler.cpp']['C'] = current_code

    # Generate SourceSamplerSound code to save state
    current_code = ''
    for count, control_data in enumerate([control_data for control_data in controls_list]):
        if control_data['type'] == 'float':
            current_code += """    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "{name}", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, {name}, nullptr),
                      nullptr);
""".format(**control_data)
        elif control_data['type'] == 'int':
            current_code += """    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "{name}", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, {name}, nullptr),
                      nullptr);
""".format(**control_data)
        elif control_data['type'] == 'adsr':
            for adsr_phase in ['attack', 'decay', 'sustain', 'release']:  # Note that these names ust match ADSR::Parameters names
                control_data.update({'adsr_phase': adsr_phase})
                current_code += """    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "{name}.{adsr_phase}", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, {name}.{adsr_phase}, nullptr),
                      nullptr);
""".format(**control_data)
        else:
            # Don't know what to do with other types
            pass
    current_code += '    '
    code_dict['SourceSampler.cpp']['B'] = current_code

    # Generate SourceSamplerSound code to define parameters
    current_code = ''
    for count, control_data in enumerate([control_data for control_data in controls_list]):
        if control_data['type'] == 'float':
            defaultf = float(control_data['default'])
            control_data.update({'defaultf': defaultf})
            current_code += "    float {name} = {defaultf}f;\n".format(**control_data)
        elif control_data['type'] == 'int':
            defaulti = int(control_data['default'])
            control_data.update({'defaulti': defaulti})
            current_code += "    int {name} = {defaulti};\n".format(**control_data)
        elif control_data['type'] == 'adsr':
            current_code += "    ADSR::Parameters {name} = {{{default}}};\n".format(**control_data)
        else:
            # Don't know what to do with other types
            pass
    current_code += '    '
    code_dict['SourceSampler.h'] = {}
    code_dict['SourceSampler.h']['A'] = current_code

    # Generate HTML interface sound edit elements code
    current_code = ''
    for count, control_data in enumerate([control_data for control_data in controls_list]):
        if control_data['type'] == 'float':
            defaultf = float(control_data['default'])
            minf = float(control_data['min'])
            maxf = float(control_data['max'])
            control_data.update({'minf': minf, 'maxf': maxf, 'defaultf': defaultf, 'step': 1.0 if control_data['name'] == 'basePitch' else 0.01})
            current_code += """            html += '<input type="range" id="' + soundIdx + '_{name}" name="{name}" min="{minf}" max="{maxf}" value="{defaultf}" step="{step}" oninput="setSoundParameter(' + soundIdx + ', this)" > {name}: <span id="' + soundIdx + '_{name}Label"></span><br>'\n""".format(
                **control_data)
        elif control_data['type'] == 'int':
            defaulti = int(control_data['default'])
            mini = int(control_data['min'])
            maxi = int(control_data['max'])
            control_data.update({'mini': mini, 'maxi': maxi, 'defaulti': defaulti, 'step': 1})
            current_code += """            html += '<input type="range" id="' + soundIdx + '_{name}" name="{name}" min="{mini}" max="{maxi}" value="{defaulti}" step="{step}" oninput="setSoundParameterInt(' + soundIdx + ', this)" > {name}: <span id="' + soundIdx + '_{name}Label"></span><br>'\n""".format(
                **control_data)
        elif control_data['type'] == 'adsr':
            for count, adsr_phase in enumerate(['attack', 'decay', 'sustain', 'release']):  # Note that these names ust match ADSR::Parameters names
                minf = [0.0, 0.0, 0.0, 0.0][count]
                maxf = [20.0, 20.0, 1.0, 20.0][count]
                defaultf = float(control_data['default'].replace('f', '').split(',')[count])
                control_data.update({'adsr_phase': adsr_phase, 'minf': minf, 'maxf': maxf, 'defaultf': defaultf})
                current_code += """            html += '<input type="range" id="' + soundIdx + '_{name}.{adsr_phase}" name="{name}.{adsr_phase}" min="{minf}" max="{maxf}" value="{defaultf}" step="0.01" oninput="setSoundParameter(' + soundIdx + ', this)" > {name}.{adsr_phase}: <span id="' + soundIdx + '_{name}.{adsr_phase}Label"></span><br>'\n""".format(
                    **control_data)
        else:
            # Don't know what to do with other types
            pass
    current_code += '            '
    code_dict['../Resources/index.html'] = {}
    code_dict['../Resources/index.html']['A'] = current_code


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
                os.path.join(SOURCE_CODE_FOLDER, key), 
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
