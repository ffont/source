import json
import os
import random
import requests

from freesound_api_key import FREESOUND_API_KEY, FREESOUND_CLIENT_ID

# TODO: refactor this to use proper Freesound python API client (?)

fields_param = "id,previews,license,name,username,analysis"
descriptor_names = "rhythm.onset_times"

ac_descriptors_default_filter_ranges = {
    'low': [0, 33],
    'mid': [33, 66],
    'high': [66, 100],
}

ac_descriptors_filter_ranges = {
    'brightness': ac_descriptors_default_filter_ranges,
    # Here we can add custom ranges for the different descriptors, for now all us default
}

license_to_filter = {
    'All': '',  # Apply no filter
    'CC0': 'license:"Creative Commons 0"',
    'Exclude NC': 'license:("Creative Commons 0" OR "Attribution")'  # Exlude non-commercial and sampling+ as well
}

def prepare_ac_descriptors_filter(ac_descriptors_filters_dict):
    filter_texts = []
    for descriptor_name, text_value in ac_descriptors_filters_dict.items():
        range_min, range_max = ac_descriptors_filter_ranges.get(descriptor_name, ac_descriptors_default_filter_ranges)[text_value]
        filter_text = "ac_{}:[{}+TO+{}]".format(descriptor_name, range_min, range_max)
        filter_texts.append(filter_text)
    return ' '.join(filter_texts)


def find_sounds_by_query(query, n_sounds=15, min_length=0, max_length=300, page_size=50, license=None, ac_descriptors_filters={}):
    if n_sounds > 128:
        n_sounds = 128

    query_filter = "duration:[{}+TO+{}]".format(min_length, max_length)
    ac_descriptors_filter = prepare_ac_descriptors_filter(ac_descriptors_filters)
    if ac_descriptors_filter:
        query_filter += " {}".format(ac_descriptors_filter)
    if license is not None:
        query_filter += " {}".format(license_to_filter[license])

    url = 'https://freesound.org/apiv2/search/text/?query="{}"&filter={}&fields={}&page_size={}&descriptors={}&group_by_pack=1&token={}'.format(query, query_filter, fields_param, page_size, descriptor_names, FREESOUND_API_KEY)
    print(url)
    r = requests.get(url, timeout=30)
    response = r.json()
    if 'results' in response and len(response['results']) > 0:
        n_results = len(response['results'])
        return random.sample(response['results'], min(n_sounds, n_results)) 


def find_sound_by_query(query, min_length=0, max_length=300, page_size=50, license=None, ac_descriptors_filters={}):
    sound = find_sounds_by_query(query=query, n_sounds=1, min_length=min_length, max_length=max_length, page_size=page_size, license=license, ac_descriptors_filters=ac_descriptors_filters)
    if not sound:
        return None  # Make consistent interface with find_sound_by_similarity which only returns 1 element
    else:
        return sound[0]


def find_random_sounds(n_sounds=15, min_length=0, max_length=300, report_callback=None, license=None, ac_descriptors_filters={}):
    new_sounds = []

    query_filter = "duration:[{}+TO+{}]".format(min_length, max_length)
    ac_descriptors_filter = prepare_ac_descriptors_filter(ac_descriptors_filters)
    if ac_descriptors_filter:
        query_filter += " {}".format(ac_descriptors_filter)
    if license is not None:
        query_filter += " {}".format(license_to_filter[license])

    # First make query to know how many sounds there are in Freesound with min_length, max_length filter
    url = 'https://freesound.org/apiv2/search/text/?filter={}&fields=&page_size=1&group_by_pack=0&token={}'.format(query_filter, FREESOUND_API_KEY)
    print(url)
    r = requests.get(url, timeout=30)
    response = r.json()
    num_sounds_found = int(response['count'])

    # Now randomly select n_sounds from these sounds and make queries to retrieve them
    selected_sounds = random.sample(range(0, num_sounds_found), n_sounds)
    for count, sound_position in enumerate(selected_sounds):
        page = sound_position + 1
        url = 'https://freesound.org/apiv2/search/text/?filter={}&fields={}&page_size=1&descriptors={}&page={}&group_by_pack=0&token={}'.format(query_filter, fields_param, descriptor_names, page, FREESOUND_API_KEY)
        print(url)
        r = requests.get(url, timeout=30)
        response = r.json()
        if 'results' in response and len(response['results']) > 0:
            new_sounds.append(response['results'][0])  # Get the first (and only) result of the query

        if report_callback is not None:
            report_callback(count, len(selected_sounds))

    return new_sounds


def find_sound_by_similarity(sound_id):    
    url = 'https://freesound.org/apiv2/sounds/{0}/similar?&fields={1}&descriptors={2}&token={3}'.format(sound_id, fields_param, descriptor_names, FREESOUND_API_KEY)
    print(url)
    r = requests.get(url, timeout=30)
    response = r.json()
    if 'results' in response and len(response['results']) > 0:
        return random.choice(response['results'])
    else:
        return None


# OAuth2 stuff

access_token = None
refresh_token = None
stored_tokens_filename = 'tokens.json'
freesound_username = ""

def get_logged_in_user_information():
    global freesound_username

    url = 'https://freesound.org/apiv2/me/'
    print(url)
    r = requests.get(url, timeout=30, headers={'Authorization': 'Bearer {}'.format(access_token)})
    response = r.json()
    freesound_username = response['username']
    print('Freesound username set')


def is_logged_in():
    return freesound_username != ""


def get_crurrently_logged_in_user():
    return freesound_username


def refresh_access_token():
    global refresh_token
    global access_token

    url = 'https://freesound.org/apiv2/oauth2/access_token/'
    print(url)
    data = {
        'client_id': FREESOUND_CLIENT_ID,
        'client_secret': FREESOUND_API_KEY,
        'grant_type': 'refresh_token',
        'refresh_token': refresh_token
    }
    r = requests.post(url, data=data, timeout=30)
    response = r.json()
    access_token = response['access_token']
    refresh_token = response['refresh_token']
    json.dump(response, open(stored_tokens_filename, 'w'))
    print('Freesound access token refreshed correctly')
    get_logged_in_user_information()


def get_access_token_from_code(code):
    global refresh_token
    global access_token

    url = 'https://freesound.org/apiv2/oauth2/access_token/'
    print(url)
    data = {
        'client_id': FREESOUND_CLIENT_ID,
        'client_secret': FREESOUND_API_KEY,
        'grant_type': 'authorization_code',
        'code': code
    }
    r = requests.post(url, data=data, timeout=30)
    response = r.json()
    access_token = response['access_token']
    refresh_token = response['refresh_token']
    json.dump(response, open(stored_tokens_filename, 'w'))
    print('New Freesound access token saved correctly')
    get_logged_in_user_information()


def logout_from_freesound():
    global freesound_username
    freesound_username = ""


if os.path.exists(stored_tokens_filename):
    stored_tokens = json.load(open(stored_tokens_filename))
    access_token = stored_tokens['access_token']
    refresh_token = stored_tokens['refresh_token']
    refresh_access_token()  # Refresh access token at startup to make sure we have a valid one

