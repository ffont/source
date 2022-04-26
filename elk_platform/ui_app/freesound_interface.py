import json
import os
import random
import requests
import threading
import asyncio
import time

from freesound_api_key import FREESOUND_API_KEY, FREESOUND_CLIENT_ID

from helpers import DownloadFileThread


# OAuth2 stuff

access_token = None
refresh_token = None
access_token_expiration = None
stored_tokens_filename = 'tokens.json'
freesound_username = ""

# TODO: refactor this to use proper Freesound python API client (?)

fields_param = "id,previews,license,name,username,analysis,type,filesize"
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

    if query.isdigit():
        # If the query contains only a single number, we assume user is looking for a sound in particular and therefore
        # we remove the filters to make sure the sound is not filtered out in the query
        query_filter_param = ''
    else:
        query_filter_param = '&filter={}'.format(query_filter)

    url = 'https://freesound.org/apiv2/search/text/?query="{}"{}&fields={}&page_size={}&descriptors={}&group_by_pack=1&token={}'.format(query, query_filter_param, fields_param, page_size, descriptor_names, FREESOUND_API_KEY)
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


bookmarks_category_name = "SourceSampler"

def bookmark_sound(sound_id):
    url = 'https://freesound.org/apiv2/sounds/{}/bookmark/'.format(sound_id)
    print(url)
    data = {
        'category': bookmarks_category_name,
    }
    try:
        r = requests.post(url, data=data, timeout=30, headers={'Authorization': 'Bearer {}'.format(access_token)})
        response = r.json()
        return 'Successfully' in response.get('detail', '')
    except Exception as e:
        print(e)
        return False


def get_user_bookmarks():
    # 1) Find URL of bookmarks category for SourceSampler
    url_for_sounds = None
    url = 'https://freesound.org/apiv2/users/{}/bookmark_categories/'.format(freesound_username)
    print(url)
    r = requests.get(url, timeout=30, headers={'Authorization': 'Bearer {}'.format(access_token)})
    response = r.json()
    for category in response['results']:
        if category['name'] == bookmarks_category_name:
            url_for_sounds = category['sounds']

    # 2) Get the actual list of bookmarks
    if url_for_sounds is not None:
        print(url_for_sounds)
        r = requests.get(url_for_sounds + '?fields={}&descriptors={}'.format(fields_param, descriptor_names), timeout=30, headers={'Authorization': 'Bearer {}'.format(access_token)})
        response = r.json()

        # Trigger download of the lq ogg previews for previewing in menu
        for sound in response['results']:
            preview_url = sound['previews']['preview-lq-ogg']
            DownloadFileThread(preview_url, preview_url.split('/')[-1]).start()

        return response['results']
    else:
        return []
    

def get_logged_in_user_information():
    global freesound_username

    url = 'https://freesound.org/apiv2/me/'
    stored_access_token = get_stored_access_token()
    try:
        r = requests.get(url, timeout=10, headers={'Authorization': 'Bearer {}'.format(stored_access_token)})
        print(url)
        response = r.json()
        if 'username' in response:
            freesound_username = response['username']
            print('Freesound username set')
        else:
            print('ERROR getting logged user info: {}'.format(response))
    except requests.exceptions.ReadTimeout:
        print('ERROR request timed out')


def is_logged_in():
    return freesound_username != ""


def get_stored_access_token():
    if refresh_token is not None and (access_token_expiration is None or access_token_expiration < (time.time() - 60 * 5)):
        # If access token might have expired, refresh it first
        refresh_access_token()
    return access_token


def get_crurrently_logged_in_user():
    return freesound_username


def refresh_access_token():
    global refresh_token
    global access_token
    global access_token_expiration

    url = 'https://freesound.org/apiv2/oauth2/access_token/'
    data = {
        'client_id': FREESOUND_CLIENT_ID,
        'client_secret': FREESOUND_API_KEY,
        'grant_type': 'refresh_token',
        'refresh_token': refresh_token
    }
    print(url, data)
    try:
        r = requests.post(url, data=data, timeout=10)
        response = r.json()
        if 'access_token' in response:
            access_token = response['access_token']
            refresh_token = response['refresh_token']
            access_token_expiration = time.time() + float(response['expires_in'])
            json.dump(response, open(stored_tokens_filename, 'w'))
            print('Freesound access token refreshed correctly')
            get_logged_in_user_information()
            return True
        else:
            print('ERROR refreshing access token: {}'.format(response))
            return False
    except requests.exceptions.ReadTimeout:
        print('ERROR request timed out')
        return False


def get_access_token_from_code(code):
    global refresh_token
    global access_token
    global access_token_expiration

    url = 'https://freesound.org/apiv2/oauth2/access_token/'
    data = {
        'client_id': FREESOUND_CLIENT_ID,
        'client_secret': FREESOUND_API_KEY,
        'grant_type': 'authorization_code',
        'code': code
    }
    print(url, data)
    try:
        r = requests.post(url, data=data, timeout=10)
        response = r.json()
        if 'access_token' in response:
            access_token = response['access_token']
            refresh_token = response['refresh_token']
            access_token_expiration = time.time() + float(response['expires_in'])
            json.dump(response, open(stored_tokens_filename, 'w'))
            print('New Freesound access token saved correctly')
            get_logged_in_user_information()
            return True
        else:
            print('ERROR getting new access token: {}'.format(response))
            return False
    except requests.exceptions.ReadTimeout:
        print('ERROR request timed out')
        return False


def logout_from_freesound():
    global freesound_username
    os.remove(stored_tokens_filename)  # Remove stored tokens
    freesound_username = ""


class RefreshAccessTokenInThread(threading.Thread):
    def run(self):
        asyncio.set_event_loop(asyncio.new_event_loop())
        refresh_access_token()


if os.path.exists(stored_tokens_filename):
    stored_tokens = json.load(open(stored_tokens_filename))
    access_token = stored_tokens['access_token']
    refresh_token = stored_tokens['refresh_token']
    get_logged_in_user_information()
