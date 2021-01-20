import random
import requests

from freesound_api_key import FREESOUND_API_KEY

# TODO: refactor this to use proper Freesound python API client (?)

fields_param = "id,previews,license,name,username,analysis"
descriptor_names = "rhythm.onset_times"


def find_sounds_by_query(query, n_sounds=15, min_length=0, max_length=300, page_size=50):
    if n_sounds > 128:
        n_sounds = 128
    url = 'https://freesound.org/apiv2/search/text/?query={0}&filter=duration:[{1}+TO+{2}]&fields={3}&page_size={4}&descriptors={5}&group_by_pack=1&token={6}'.format(query, min_length, max_length, fields_param, page_size, descriptor_names, FREESOUND_API_KEY)
    print(url)
    r = requests.get(url, timeout=30)
    response = r.json()
    if 'results' in response and len(response['results']) > 0:
        n_results = len(response['results'])
        return random.sample(response['results'], min(n_sounds, n_results)) 


def find_sound_by_query(query, min_length=0, max_length=300, page_size=50):
    sound = find_sounds_by_query(query=query, n_sounds=1, min_length=min_length, max_length=max_length, page_size=page_size)
    if not sound:
        return None  # Make consistent interface with find_sound_by_similarity which only returns 1 element
    else:
        return sound[0]


def find_sound_by_similarity(sound_id):    
    url = 'https://freesound.org/apiv2/sounds/{0}/similar?&fields={1}&descriptors={2}&token={3}'.format(sound_id, fields_param, descriptor_names, FREESOUND_API_KEY)
    print(url)
    r = requests.get(url, timeout=30)
    response = r.json()
    if 'results' in response and len(response['results']) > 0:
        return random.choice(response['results'])
    else:
        return None


def find_random_sounds(n_sounds=15, min_length=0, max_length=300, report_callback=None):
    new_sounds = []

    # First make query to know how many sounds there are in Freesound with min_length, max_length filter
    url = 'https://freesound.org/apiv2/search/text/?filter=duration:[{}+TO+{}]&fields=&page_size=1&group_by_pack=0&token={}'.format(min_length, max_length, FREESOUND_API_KEY)
    print(url)
    r = requests.get(url, timeout=30)
    response = r.json()
    num_sounds_found = int(response['count'])

    # Now randomly select n_sounds from these sounds and make queries to retrieve them
    selected_sounds = random.sample(range(0, num_sounds_found), n_sounds)
    for count, sound_position in enumerate(selected_sounds):
        page = sound_position + 1
        url = 'https://freesound.org/apiv2/search/text/?filter=duration:[{}+TO+{}]&fields={}&page_size=1&descriptors={}&page={}&group_by_pack=0&token={}'.format(min_length, max_length, fields_param, descriptor_names, page, FREESOUND_API_KEY)
        print(url)
        r = requests.get(url, timeout=30)
        response = r.json()
        if 'results' in response and len(response['results']) > 0:
            new_sounds.append(response['results'][0])  # Get the first (and only) result of the query

        if report_callback is not None:
            report_callback(count, len(selected_sounds))

    return new_sounds
